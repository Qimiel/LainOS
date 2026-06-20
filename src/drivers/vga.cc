#include <drivers/vga.h>


using namespace os;
using namespace os::gui;
using namespace os::common;
using namespace os::drivers;
using namespace os::math;


uint16_t strlen(char*);
uint8_t Web2EGA(uint32_t webColor);


static uint8_t EGAChannel(uint8_t color, uint8_t lowBit, uint8_t highBit) {

	bool low = (color & (1 << lowBit)) != 0;
	bool high = (color & (1 << highBit)) != 0;

	if (low && high) { return 0xff; }
	if (low) { return 0xaa; }
	if (high) { return 0x55; }
	return 0x00;
}


static uint32_t LegacyVGAToRGB(uint8_t color) {

	if (color == 0x40) { return 0x000000; }

	color &= 0x3f;

	uint8_t b = EGAChannel(color, 0, 3);
	uint8_t g = EGAChannel(color, 1, 4);
	uint8_t r = EGAChannel(color, 2, 5);

	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


static uint32_t PackField(uint8_t value, uint8_t position, uint8_t maskSize) {

	if (maskSize == 0) { return 0; }

	uint32_t max = (1u << maskSize) - 1;
	return ((uint32_t)value * max / 255) << position;
}


static uint32_t FieldMask(uint8_t position, uint8_t maskSize) {

	if (maskSize == 0) { return 0; }
	if (maskSize >= 32) { return 0xffffffff; }
	return ((1u << maskSize) - 1) << position;
}


static uint32_t PackFrameBufferPixel(uint8_t r, uint8_t g, uint8_t b,
				     uint8_t bpp,
				     uint8_t redFieldPosition,
				     uint8_t redMaskSize,
				     uint8_t greenFieldPosition,
				     uint8_t greenMaskSize,
				     uint8_t blueFieldPosition,
				     uint8_t blueMaskSize) {

	uint32_t redMask = FieldMask(redFieldPosition, redMaskSize);
	uint32_t greenMask = FieldMask(greenFieldPosition, greenMaskSize);
	uint32_t blueMask = FieldMask(blueFieldPosition, blueMaskSize);

	uint32_t packed =
		PackField(r, redFieldPosition, redMaskSize)
		| PackField(g, greenFieldPosition, greenMaskSize)
		| PackField(b, blueFieldPosition, blueMaskSize);

	if (bpp == 32) {

		packed |= ~(redMask | greenMask | blueMask);
	}

	return packed;
}


static bool ValidFrameBufferField(uint8_t position, uint8_t maskSize, uint8_t bpp) {

	return maskSize != 0 && maskSize <= 8 && position < bpp && position + maskSize <= bpp;
}


static void DefaultFrameBufferFields(uint8_t bpp,
				     uint8_t& redFieldPosition,
				     uint8_t& redMaskSize,
				     uint8_t& greenFieldPosition,
				     uint8_t& greenMaskSize,
				     uint8_t& blueFieldPosition,
				     uint8_t& blueMaskSize) {

	if (bpp == 15) {

		redFieldPosition = 10;
		redMaskSize = 5;
		greenFieldPosition = 5;
		greenMaskSize = 5;
		blueFieldPosition = 0;
		blueMaskSize = 5;
		return;
	}

	if (bpp == 16) {

		redFieldPosition = 11;
		redMaskSize = 5;
		greenFieldPosition = 5;
		greenMaskSize = 6;
		blueFieldPosition = 0;
		blueMaskSize = 5;
		return;
	}

	redFieldPosition = 16;
	redMaskSize = 8;
	greenFieldPosition = 8;
	greenMaskSize = 8;
	blueFieldPosition = 0;
	blueMaskSize = 8;
}


static uint16_t frameBufferScaleX[gui::GRAPHICS_LOGICAL_WIDTH + 1];
static uint16_t frameBufferScaleY[gui::GRAPHICS_LOGICAL_HEIGHT + 1];
static uint32_t frameBufferScaleWidth = 0;
static uint32_t frameBufferScaleHeight = 0;

static uint32_t frameBufferPalette[256];
static uint8_t frameBufferPaletteBpp = 0;
static uint8_t frameBufferPaletteRedFieldPosition = 0;
static uint8_t frameBufferPaletteRedMaskSize = 0;
static uint8_t frameBufferPaletteGreenFieldPosition = 0;
static uint8_t frameBufferPaletteGreenMaskSize = 0;
static uint8_t frameBufferPaletteBlueFieldPosition = 0;
static uint8_t frameBufferPaletteBlueMaskSize = 0;
static bool frameBufferPaletteValid = false;


static void EnsureFrameBufferScaleTables(uint32_t width, uint32_t height) {

	if (frameBufferScaleWidth == width && frameBufferScaleHeight == height) { return; }

	for (uint16_t x = 0; x <= gui::GRAPHICS_LOGICAL_WIDTH; x++) {

		frameBufferScaleX[x] = (x * width) / gui::GRAPHICS_LOGICAL_WIDTH;
	}

	for (uint16_t y = 0; y <= gui::GRAPHICS_LOGICAL_HEIGHT; y++) {

		frameBufferScaleY[y] = (y * height) / gui::GRAPHICS_LOGICAL_HEIGHT;
	}

	frameBufferScaleWidth = width;
	frameBufferScaleHeight = height;
}


static void EnsureFrameBufferPalette(uint8_t bpp,
				     uint8_t redFieldPosition,
				     uint8_t redMaskSize,
				     uint8_t greenFieldPosition,
				     uint8_t greenMaskSize,
				     uint8_t blueFieldPosition,
				     uint8_t blueMaskSize) {

	if (frameBufferPaletteValid
	 && frameBufferPaletteBpp == bpp
	 && frameBufferPaletteRedFieldPosition == redFieldPosition
	 && frameBufferPaletteRedMaskSize == redMaskSize
	 && frameBufferPaletteGreenFieldPosition == greenFieldPosition
	 && frameBufferPaletteGreenMaskSize == greenMaskSize
	 && frameBufferPaletteBlueFieldPosition == blueFieldPosition
	 && frameBufferPaletteBlueMaskSize == blueMaskSize) {

		return;
	}

	for (uint16_t color = 0; color < 256; color++) {

		uint32_t rgb = LegacyVGAToRGB(color);
		uint8_t r = (rgb >> 16) & 0xff;
		uint8_t g = (rgb >> 8) & 0xff;
		uint8_t b = rgb & 0xff;

		frameBufferPalette[color] =
			PackFrameBufferPixel(r, g, b, bpp,
					     redFieldPosition,
					     redMaskSize,
					     greenFieldPosition,
					     greenMaskSize,
					     blueFieldPosition,
					     blueMaskSize);
	}

	frameBufferPaletteBpp = bpp;
	frameBufferPaletteRedFieldPosition = redFieldPosition;
	frameBufferPaletteRedMaskSize = redMaskSize;
	frameBufferPaletteGreenFieldPosition = greenFieldPosition;
	frameBufferPaletteGreenMaskSize = greenMaskSize;
	frameBufferPaletteBlueFieldPosition = blueFieldPosition;
	frameBufferPaletteBlueMaskSize = blueMaskSize;
	frameBufferPaletteValid = true;
}


VideoGraphicsArray::VideoGraphicsArray() :

	miscPort(0x3c2),
	crtcIndexPort(0x3d4),
	crtcDataPort(0x3d5),
	sequencerIndexPort(0x3c4),
	sequencerDataPort(0x3c5),
	graphicsControllerIndexPort(0x3ce),
	graphicsControllerDataPort(0x3cf),
	attributeControllerIndexPort(0x3c0),
	attributeControllerReadPort(0x3c1),
	attributeControllerWritePort(0x3c0),
	attributeControllerResetPort(0x3da) {

	this->FrameBufferSegment = 0;
	this->frameBufferPitch = 0;
	this->frameBufferWidth = gui::GRAPHICS_LOGICAL_WIDTH;
	this->frameBufferHeight = gui::GRAPHICS_LOGICAL_HEIGHT;
	this->frameBufferBpp = 8;
	this->frameBufferType = 0;
	this->redFieldPosition = 16;
	this->redMaskSize = 8;
	this->greenFieldPosition = 8;
	this->greenMaskSize = 8;
	this->blueFieldPosition = 0;
	this->blueMaskSize = 8;
	this->linearFrameBuffer = false;
	this->highResolutionMode = false;

}


VideoGraphicsArray::~VideoGraphicsArray() {
}



void VideoGraphicsArray::WriteRegisters(uint8_t* registers) {

	//misc
	miscPort.Write(*(registers++));

	//sequencer
	for (uint8_t i = 0; i < 5; i++) {

		sequencerIndexPort.Write(i);
		sequencerDataPort.Write(*(registers++));
	}


	//cathode ray tube controller
	crtcIndexPort.Write(0x03);
	crtcDataPort.Write(crtcDataPort.Read() | 0x80);
	crtcIndexPort.Write(0x11);
	crtcDataPort.Write(crtcDataPort.Read() & ~0x80);


	registers[0x03] = registers[0x03] | 0x80;
	registers[0x11] = registers[0x11] & ~0x80;

	for (uint8_t i = 0; i < 25; i++) {

		crtcIndexPort.Write(i);
		crtcDataPort.Write(*(registers++));

	}

	//graphics controller
	for (uint8_t i = 0; i < 9; i++) {

		graphicsControllerIndexPort.Write(i);
		graphicsControllerDataPort.Write(*(registers++));

	}


	//attribute controller
	for (uint8_t i = 0; i < 21; i++) {

		attributeControllerResetPort.Read();
		attributeControllerIndexPort.Write(i);
		attributeControllerWritePort.Write(*(registers++));
	}


	attributeControllerResetPort.Read();
	attributeControllerIndexPort.Write(0x20);
}


bool VideoGraphicsArray::SupportsMode(uint32_t width, uint32_t height, uint32_t colordepth) {

	if (width == gui::GRAPHICS_LOGICAL_WIDTH
	 && height == gui::GRAPHICS_LOGICAL_HEIGHT
	 && colordepth == 8) {
		return true;
	}

	if (this->linearFrameBuffer
	 && width == gui::GRAPHICS_SCREEN_WIDTH
	 && height == gui::GRAPHICS_SCREEN_HEIGHT
	 && (colordepth == 8 || colordepth == 24 || colordepth == 32)) {
		return true;
	}

	return false;
}


bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth) {

	if (!SupportsMode(width, height, colordepth)) {

		return false;
	}

	if (this->linearFrameBuffer
	 && width == gui::GRAPHICS_SCREEN_WIDTH
	 && height == gui::GRAPHICS_SCREEN_HEIGHT) {

		this->highResolutionMode = true;
		return true;
	}

	unsigned char g_320x200x256[] = {

	/* misc */
		0x63,
	/* seq */
		0x03, 0x01, 0x0f, 0x00, 0x0e,
	/* crtc */
		0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f,
		0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x9c, 0x0e, 0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3,
		0xff,
	/* gc */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0f,
		0xff,
	/* ac */
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x41, 0x00, 0x0f, 0x00, 0x00
	};


	WriteRegisters(g_320x200x256);

	this->FrameBufferSegment = GetFrameBufferSegment();
	this->frameBufferPitch = gui::GRAPHICS_LOGICAL_WIDTH;
	this->frameBufferWidth = gui::GRAPHICS_LOGICAL_WIDTH;
	this->frameBufferHeight = gui::GRAPHICS_LOGICAL_HEIGHT;
	this->frameBufferBpp = 8;
	this->frameBufferType = 0;
	this->highResolutionMode = false;

	return true;
}


void VideoGraphicsArray::UseLinearFrameBuffer(uint64_t address,
					      uint32_t pitch,
					      uint32_t width,
					      uint32_t height,
					      uint8_t bpp,
					      uint8_t type,
					      uint8_t redFieldPosition,
					      uint8_t redMaskSize,
					      uint8_t greenFieldPosition,
					      uint8_t greenMaskSize,
					      uint8_t blueFieldPosition,
					      uint8_t blueMaskSize) {

	if ((address >> 32) != 0) { return; }
	if (width < gui::GRAPHICS_SCREEN_WIDTH) { return; }
	if (height < gui::GRAPHICS_SCREEN_HEIGHT) { return; }
	if (bpp != 8 && bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) { return; }
	if (type != 0 && type != 1) { return; }
	if (type == 0 && bpp != 8) { return; }

	if (type == 1
	 && (!ValidFrameBufferField(redFieldPosition, redMaskSize, bpp)
	  || !ValidFrameBufferField(greenFieldPosition, greenMaskSize, bpp)
	  || !ValidFrameBufferField(blueFieldPosition, blueMaskSize, bpp))) {

		DefaultFrameBufferFields(bpp,
					 redFieldPosition,
					 redMaskSize,
					 greenFieldPosition,
					 greenMaskSize,
					 blueFieldPosition,
					 blueMaskSize);
	}

	this->FrameBufferSegment = (uint8_t*)((uint32_t)address);
	this->frameBufferPitch = pitch;
	this->frameBufferWidth = width;
	this->frameBufferHeight = height;
	this->frameBufferBpp = bpp;
	this->frameBufferType = type;
	this->redFieldPosition = redFieldPosition;
	this->redMaskSize = redMaskSize;
	this->greenFieldPosition = greenFieldPosition;
	this->greenMaskSize = greenMaskSize;
	this->blueFieldPosition = blueFieldPosition;
	this->blueMaskSize = blueMaskSize;
	this->linearFrameBuffer = true;
}



uint8_t* VideoGraphicsArray::GetFrameBufferSegment() {

	graphicsControllerIndexPort.Write(0x06);
	uint8_t segmentNumber = graphicsControllerDataPort.Read() & (3 << 2);

	switch (segmentNumber) {

		default:
		case 0 << 2: return (uint8_t*)0x00000;
		case 1 << 2: return (uint8_t*)0xa0000;
		case 2 << 2: return (uint8_t*)0xb0000;
		case 3 << 2: return (uint8_t*)0xb8000;
	}
}




//place in backbuffer
void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t color) {

	if (x < 0 || gui::GRAPHICS_LOGICAL_WIDTH <= x
	 || y < 0 || gui::GRAPHICS_LOGICAL_HEIGHT <= y) { return; }

	pixels[gui::LogicalIndex(x, y)] = color;
}



//draw directly to vmem
void VideoGraphicsArray::PutPixelRaw(int32_t x, int32_t y, uint8_t colorIndex) {

	if (x < 0 || gui::GRAPHICS_LOGICAL_WIDTH <= x
	 || y < 0 || gui::GRAPHICS_LOGICAL_HEIGHT <= y) {
		return;
	}

	if (this->highResolutionMode) {

		pixels[gui::LogicalIndex(x, y)] = colorIndex;
		return;
	}

	uint8_t* pixelAddress = this->FrameBufferSegment + gui::LogicalIndex(x, y);
	*pixelAddress = colorIndex;
}


//read from backbuffer
uint8_t VideoGraphicsArray::ReadPixel(int32_t x, int32_t y) {

	if (x < 0 || gui::GRAPHICS_LOGICAL_WIDTH <= x
	 || y < 0 || gui::GRAPHICS_LOGICAL_HEIGHT <= y) { return 0; }
	return pixels[gui::LogicalIndex(x, y)];
}



void VideoGraphicsArray::PutText(char* str, int32_t x, int32_t y, uint8_t color) {

	uint16_t length = strlen(str);

	if ((gui::GRAPHICS_LOGICAL_WIDTH - x) < (length * 5)) { return; }

	uint8_t* charArr = charset[0];

	for (int i = 0; str[i] != '\0'; i++) {

		charArr = charset[(uint8_t)(str[i])-32];

		for (uint8_t w = 0; w < font_width; w++) {
			for (uint8_t h = 0; h < font_height; h++) {

				if (charArr[w] && ((charArr[w] >> h) & 1)) {

					this->PutPixel(x+w, y+h, color);
				}
			}
		}
		x += font_width;
	}
}



//draw from buffer
void VideoGraphicsArray::FillBufferFull(int32_t x, int32_t y,
					int32_t w, int32_t h, uint8_t* buf) {

	for (int32_t Y = y; Y < y+h; Y++) {
		for (int32_t X = x; X < x+w; X++) {

			this->PutPixel(X, Y, buf[gui::GRAPHICS_LOGICAL_WIDTH*(Y-y)+(X-x)]);
		}
	}
}


void VideoGraphicsArray::FillBuffer(int16_t x, int16_t y,
				int16_t w, int16_t h, uint8_t* buf, bool mirror) {

	uint8_t pixelColor = 0;
	uint8_t scrollVert = 0;
	bool scroll = false;

	if (y < 0) {

		if (y+h < 0) { return; }
		scrollVert = (y * -1);
		h -= (y * -1);
		y = 0;
		scroll = true;
	}

	for (int16_t Y = y; Y < y+h; Y++) {
		for (int16_t X = x; X < x+w; X++) {

			if (mirror) { pixelColor = buf[w*(Y-y+scrollVert)+(x+w-X-1)];
			} else {      pixelColor = buf[w*(Y-y+scrollVert)+(X-x)]; }

			if (pixelColor) {

				this->PutPixel(X, Y, pixelColor);
			}
		}
	}
}





void VideoGraphicsArray::FillRectangle(int32_t x, int32_t y,
		int32_t w, int32_t h, uint8_t color) {

	for (int32_t Y = y; Y < y+h; Y++) {
		for (int32_t X = x; X < x+w; X++) {

			this->PutPixel(X, Y, color);
		}
	}
}



void VideoGraphicsArray::DrawRectangle(int32_t x, int32_t y,
		int32_t w, int32_t h, uint8_t color) {

	for (int32_t X = x; X < x+w; X++) {

		this->PutPixel(X, y,     color);
		this->PutPixel(X, y+h-1, color);
	}

	for (int32_t Y = y; Y < y+h; Y++) {

		this->PutPixel(x,     Y, color);
		this->PutPixel(x+w-1, Y, color);
	}
}

void VideoGraphicsArray::DrawLineFlat(int32_t x0, int32_t y0,
				int32_t x1, int32_t y1,
				uint8_t color,
				bool x) {
	if (x) {
		for (int32_t X = x0; X < x1; X++) {

			this->PutPixel(X, y0, color);
		}
	} else {
		for (int32_t Y = y0; Y < y1; Y++) {

			this->PutPixel(x0, Y, color);
		}
	}
}

void VideoGraphicsArray::DrawLineLow(int32_t x0, int32_t y0,
				int32_t x1, int32_t y1,
				uint8_t color) {

	int16_t dx = x1 - x0;
	int16_t dy = y1 - y0;
	int16_t yi = 1;

	if (dy < 0) {

		yi = -1;
		dy = -dy;
	}
	int16_t D = (2*dy) - dx;
	int16_t y = y0;


	for (int x = x0; x < x1; x++) {

		this->PutPixel(x, y, color);

		if (D > 0) {

			y += yi;
			D += (2*(dy-dx));
		} else {
			D += 2*dy;
		}
	}
}


void VideoGraphicsArray::DrawLineHigh(int32_t x0, int32_t y0,
				int32_t x1, int32_t y1,
				uint8_t color) {

	int16_t dx = x1 - x0;
	int16_t dy = y1 - y0;
	int16_t xi = 1;

	if (dx < 0) {

		xi = -1;
		dx = -dx;
	}
	int16_t D = (2*dx) - dy;
	int16_t x = x0;


	for (int y = y0; y < y1; y++) {

		this->PutPixel(x, y, color);

		if (D > 0) {

			x += xi;
			D += (2*(dx-dy));
		} else {
			D += 2*dx;
		}
	}
}


void VideoGraphicsArray::DrawLine(int32_t x0, int32_t y0,
				int32_t x1, int32_t y1,
				uint8_t color) {

	if (abs(y1 - y0) < abs(x1 - x0)) {

		if (x0 > x1) {  DrawLineLow(x1, y1, x0, y0, color);
		} else {	DrawLineLow(x0, y0, x1, y1, color); }
	} else {

		if (y0 > y1) {  DrawLineHigh(x1, y1, x0, y0, color);
		} else {	DrawLineHigh(x0, y0, x1, y1, color); }
	}
}


void VideoGraphicsArray::FillPolygon(uint16_t x[], uint16_t y[],
				     uint8_t edgeNum,
				     uint8_t color) {
	int16_t i, j, temp = 0;
	uint16_t xmin = gui::GRAPHICS_LOGICAL_WIDTH;
	uint16_t xmax = 0;

	for (i = 0; i < edgeNum; i++) {

		if (x[i] < xmin) {
			xmin = x[i];
		}
		if (x[i] > xmax) {
			xmax = x[i];
		}
	}

	for (i = xmin; i <= xmax; i++) {

		uint16_t interPoints[edgeNum];
		uint16_t count = 0;

		for (j = 0; j < edgeNum; j++) {

			uint16_t next = (j + 1) % edgeNum;

			if ((y[j] > i && y[next] <= i) || (y[next] > i && y[j] <= i)) {

				interPoints[count++] = x[j] + (i - y[j])
					* (x[next] - x[j]) / (y[next] - y[j]);
			}
		}

		for (j = 0; j < count-1; j++) {
			for (int k = 0; k < count-j-1; k++) {

				if (interPoints[k] > interPoints[k+1]) {

					temp = interPoints[k];
					interPoints[k] = interPoints[k+1];
					interPoints[k+1] = temp;
				}
			}
		}

		//draw line
		for (j = 0; j < count; j += 2) {

			this->DrawLine(interPoints[j], i, interPoints[j+1], i, color);
		}
	}
}


void VideoGraphicsArray::MakeDark(uint8_t darkness) {

	if (darkness > 0) {

		for (uint16_t y = 0; y < gui::GRAPHICS_LOGICAL_HEIGHT; y++) {
			for (uint16_t x = 0; x < gui::GRAPHICS_LOGICAL_WIDTH; x++) {

				//make pixel darker
				for (uint8_t i = 0; i < darkness; i++) {

					pixels[gui::LogicalIndex(x, y)] =
						light2dark[pixels[gui::LogicalIndex(x, y)]];
				}
			}
		}
	}
}

void VideoGraphicsArray::MakeWave(uint8_t waveLength) {

	if (!waveLength) { return; }

	uint16_t offset = waveLength;
	bool incOrDec = true;

	for (uint16_t y = 0; y < gui::GRAPHICS_LOGICAL_HEIGHT; y++) {
		for (uint16_t x = offset; x < gui::GRAPHICS_LOGICAL_WIDTH; x++) {

			pixels[gui::LogicalIndex(x-offset, y)] = pixels[gui::LogicalIndex(x, y)];
		}

		if (offset > waveLength) { incOrDec = false; }
		if (offset == 0) { incOrDec = true; }

		if (incOrDec) { offset++;
		} else { offset--; }
	}
}

void VideoGraphicsArray::DrawToScreen() {

	if (this->highResolutionMode) {

		uint8_t bytesPerPixel = (this->frameBufferBpp + 7) / 8;

		EnsureFrameBufferScaleTables(gui::GRAPHICS_SCREEN_WIDTH,
					     gui::GRAPHICS_SCREEN_HEIGHT);

		if (this->frameBufferType != 0 || this->frameBufferBpp != 8) {

			EnsureFrameBufferPalette(this->frameBufferBpp,
						 this->redFieldPosition,
						 this->redMaskSize,
						 this->greenFieldPosition,
						 this->greenMaskSize,
						 this->blueFieldPosition,
						 this->blueMaskSize);
		}

		for (uint16_t logicalY = 0; logicalY < gui::GRAPHICS_LOGICAL_HEIGHT; logicalY++) {

			uint16_t screenYStart = frameBufferScaleY[logicalY];
			uint16_t screenYEnd = frameBufferScaleY[logicalY + 1];

			for (uint16_t logicalX = 0; logicalX < gui::GRAPHICS_LOGICAL_WIDTH; logicalX++) {

				uint16_t screenXStart = frameBufferScaleX[logicalX];
				uint16_t screenXEnd = frameBufferScaleX[logicalX + 1];
				uint8_t colorIndex = pixels[gui::LogicalIndex(logicalX, logicalY)];
				uint32_t packed = frameBufferPalette[colorIndex];

				for (uint16_t screenY = screenYStart; screenY < screenYEnd; screenY++) {

					uint8_t* pixelAddress = this->FrameBufferSegment
						+ (this->frameBufferPitch * screenY)
						+ (bytesPerPixel * screenXStart);

					if (this->frameBufferType == 0 && this->frameBufferBpp == 8) {

						for (uint16_t screenX = screenXStart; screenX < screenXEnd; screenX++) {

							*(pixelAddress++) = colorIndex;
						}
					} else if (this->frameBufferBpp == 32) {

						uint32_t* pixel = (uint32_t*)pixelAddress;
						for (uint16_t screenX = screenXStart; screenX < screenXEnd; screenX++) {

							*(pixel++) = packed;
						}
					} else if (this->frameBufferBpp == 15 || this->frameBufferBpp == 16) {

						uint16_t packed16 = packed & 0xffff;
						uint16_t* pixel = (uint16_t*)pixelAddress;
						for (uint16_t screenX = screenXStart; screenX < screenXEnd; screenX++) {

							*(pixel++) = packed16;
						}
					} else {

						for (uint16_t screenX = screenXStart; screenX < screenXEnd; screenX++) {

							for (uint8_t byte = 0; byte < bytesPerPixel; byte++) {

								pixelAddress[byte] = (packed >> (8*byte)) & 0xff;
							}
							pixelAddress += bytesPerPixel;
						}
					}
				}
			}
		}
		return;
	}

	for (uint16_t y = 0; y < gui::GRAPHICS_LOGICAL_HEIGHT; y++) {
		for (uint16_t x = 0; x < gui::GRAPHICS_LOGICAL_WIDTH; x++) {

			uint8_t* pixelAddress = this->FrameBufferSegment + gui::LogicalIndex(x, y);
			*pixelAddress = pixels[gui::LogicalIndex(x, y)];
		}
	}
}



void VideoGraphicsArray::FSdither(uint32_t* buf, uint16_t w, uint16_t h) {

	uint8_t oldPixel = 0;
	uint8_t newPixel = 0;
	uint8_t quantError = 0;

	for (uint16_t y = 0; y < h; y++) {
		for (uint16_t x = 0; x < w; x++) {

			oldPixel = buf[w*y+x];
			newPixel = Web2EGA(oldPixel);
			buf[w*y+x] = newPixel;
			quantError = oldPixel - newPixel;
			buf[w*(y)+(x+1)]   += quantError*7 / 16;
			buf[w*(y+1)+(x-1)] += quantError*3 / 16;
			buf[w*(y+1)+(x)]   += quantError*5 / 16;
			buf[w*(y+1)+(x+1)] += quantError / 16;
		}
	}
}

void VideoGraphicsArray::ErrorScreen() {

	this->FillRectangle(0, 0, gui::GRAPHICS_LOGICAL_WIDTH, gui::GRAPHICS_LOGICAL_HEIGHT, 0x09);
	this->FillRectangle(0, 29, 300, 101, 0x3f);

	this->PutText("Sorry, we experienced a critical error. :(", 1, 11, 0x40);
	this->PutText("Sorry, we experienced a critical error. :(", 0, 10, 0x3f);

	this->PutText("ERROR CODE: ", 0, 30, 0x09);
	this->PutText("0x61 0x72 0x65 0x20 0x79 0x6F 0x75 0x20 0x66 0x75 0x63 0x6B 0x69", 0, 40, 0x09);
	this->PutText("0x6E 0x67 0x20 0x72 0x65 0x74 0x61 0x72 0x74 0x65 0x64 0x3F 0x20", 0, 50, 0x09);
	this->PutText("0x64 0x6F 0x20 0x79 0x6F 0x75 0x20 0x73 0x65 0x72 0x69 0x6F 0x75", 0, 60, 0x09);
	this->PutText("0x73 0x6C 0x79 0x20 0x74 0x68 0x69 0x6E 0x6B 0x20 0x64 0x6F 0x69", 0, 70, 0x09);
	this->PutText("0x6E 0x67 0x20 0x74 0x68 0x61 0x74 0x20 0x73 0x68 0x69 0x74 0x20", 0, 80, 0x09);
	this->PutText("0x74 0x6F 0x20 0x79 0x6F 0x75 0x72 0x20 0x63 0x6F 0x6D 0x70 0x75", 0, 90, 0x09);
	this->PutText("0x74 0x65 0x72 0x20 0x77 0x61 0x73 0x20 0x61 0x20 0x67 0x6F 0x6F", 0, 100, 0x09);
	this->PutText("0x64 0x20 0x69 0x64 0x65 0x61 0x3F 0x20 0x6B 0x69 0x6C 0x6C 0x20", 0, 110, 0x09);
	this->PutText("0x79 0x6F 0x75 0x72 0x73 0x65 0x6C 0x66", 0, 120, 0x09);

	this->PutText("Sending all your data to our remote servers...", 1, 151, 0x40);
	this->PutText("Sending all your data to our remote servers...", 0, 150, 0x3f);
	this->PutText("Don't turn off your kitchen lights.", 1, 161, 0x40);
	this->PutText("Don't turn off your kitchen lights.", 0, 160, 0x3f);

	for (int i = 20; i < 180; i += 20) {

		this->FillBuffer(304, i, 13, 20, cursorClickLeft, false);
	}
}
