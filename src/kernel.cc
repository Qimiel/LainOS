#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <art.h>
#include <hardwarecommunication/interrupts.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/amd_am79c973.h>
#include <drivers/ata.h>
#include <drivers/speaker.h>
#include <drivers/pit.h>
#include <drivers/cmos.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <gui/button.h>
#include <gui/widget.h>
#include <gui/sim.h>
#include <gui/raycasting.h>
#include <gui/font.h>
#include <gui/pixelart.h>
#include <multitasking.h>
#include <net/network.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <filesys/ofs.h>
#include <cli.h>
#include <script.h>
#include <app.h>
#include <app/paint.h>
#include <app/file_edit.h>
#include <app/file_browse.h>
#include <mode/piano.h>
#include <mode/snake.h>
#include <mode/file_edit.h>
#include <mode/space.h>
#include <mode/bootscreen.h>
#include <math.h>


using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;
using namespace os::net;
using namespace os::filesystem;
using namespace os::gui;
using namespace os::math;


static bool bootFramebufferActive = false;
static uint8_t* bootFramebuffer = 0;
static uint32_t bootFramebufferPitch = 0;
static uint32_t bootFramebufferWidth = 0;
static uint32_t bootFramebufferHeight = 0;
static uint8_t bootFramebufferBpp = 0;
static uint8_t bootFramebufferType = 0;
static uint8_t bootRedFieldPosition = 16;
static uint8_t bootRedMaskSize = 8;
static uint8_t bootGreenFieldPosition = 8;
static uint8_t bootGreenMaskSize = 8;
static uint8_t bootBlueFieldPosition = 0;
static uint8_t bootBlueMaskSize = 8;
static uint8_t bootTextScale = 1;
static uint32_t bootTextOffsetX = 0;
static uint32_t bootTextOffsetY = 0;


static uint8_t BootEGAChannel(uint8_t color, uint8_t lowBit, uint8_t highBit) {

	bool low = (color & (1 << lowBit)) != 0;
	bool high = (color & (1 << highBit)) != 0;

	if (low && high) { return 0xff; }
	if (low) { return 0xaa; }
	if (high) { return 0x55; }
	return 0x00;
}


static uint32_t BootEGAToRGB(uint8_t color) {

	if (color == 0x40) { return 0x000000; }

	color &= 0x3f;

	uint8_t b = BootEGAChannel(color, 0, 3);
	uint8_t g = BootEGAChannel(color, 1, 4);
	uint8_t r = BootEGAChannel(color, 2, 5);

	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


static uint32_t BootPackField(uint8_t value, uint8_t position, uint8_t maskSize) {

	if (maskSize == 0) { return 0; }

	uint32_t max = (1u << maskSize) - 1;
	return ((uint32_t)value * max / 255) << position;
}


static uint32_t BootFieldMask(uint8_t position, uint8_t maskSize) {

	if (maskSize == 0) { return 0; }
	if (maskSize >= 32) { return 0xffffffff; }
	return ((1u << maskSize) - 1) << position;
}


static uint32_t BootPackFrameBufferPixel(uint8_t r, uint8_t g, uint8_t b) {

	uint32_t redMask = BootFieldMask(bootRedFieldPosition, bootRedMaskSize);
	uint32_t greenMask = BootFieldMask(bootGreenFieldPosition, bootGreenMaskSize);
	uint32_t blueMask = BootFieldMask(bootBlueFieldPosition, bootBlueMaskSize);

	uint32_t packed =
		BootPackField(r, bootRedFieldPosition, bootRedMaskSize)
		| BootPackField(g, bootGreenFieldPosition, bootGreenMaskSize)
		| BootPackField(b, bootBlueFieldPosition, bootBlueMaskSize);

	if (bootFramebufferBpp == 32) {

		packed |= ~(redMask | greenMask | blueMask);
	}

	return packed;
}


static bool BootValidFrameBufferField(uint8_t position, uint8_t maskSize, uint8_t bpp) {

	return maskSize != 0 && maskSize <= 8 && position < bpp && position + maskSize <= bpp;
}


static void BootDefaultFrameBufferFields(uint8_t bpp,
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


static void BootFramebufferPutPixel(uint32_t x, uint32_t y, uint8_t colorIndex) {

	if (!bootFramebufferActive) { return; }
	if (x >= bootFramebufferWidth || y >= bootFramebufferHeight) { return; }

	uint8_t bytesPerPixel = (bootFramebufferBpp + 7) / 8;
	uint8_t* pixelAddress = bootFramebuffer + (bootFramebufferPitch * y) + (bytesPerPixel * x);

	if (bootFramebufferType == 0 && bootFramebufferBpp == 8) {

		*pixelAddress = colorIndex;
		return;
	}

	uint32_t rgb = BootEGAToRGB(colorIndex);
	uint8_t r = (rgb >> 16) & 0xff;
	uint8_t g = (rgb >> 8) & 0xff;
	uint8_t b = rgb & 0xff;
	uint32_t packed = BootPackFrameBufferPixel(r, g, b);

	for (uint8_t byte = 0; byte < bytesPerPixel; byte++) {

		pixelAddress[byte] = (packed >> (8*byte)) & 0xff;
	}
}


static uint8_t* BootGlyph(uint8_t ch) {

	if (ch == 0xff) { return font_full; }
	if (ch < 32 || ch > 126) { return font_unknown; }
	return charset[ch - 32];
}


static void BootFramebufferFillRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t color) {

	for (uint32_t drawY = y; drawY < y + h && drawY < bootFramebufferHeight; drawY++) {
		for (uint32_t drawX = x; drawX < x + w && drawX < bootFramebufferWidth; drawX++) {

			BootFramebufferPutPixel(drawX, drawY, color);
		}
	}
}


static void BootFramebufferClearRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {

	BootFramebufferFillRectangle(x, y, w, h, 0x00);
}


static void BootFramebufferDrawCell(uint8_t ch, uint8_t forecolor, uint8_t backcolor, uint8_t x, uint8_t y) {

	if (!bootFramebufferActive) { return; }
	if (x >= 80 || y >= 25) { return; }

	uint32_t cellX = bootTextOffsetX + x * font_width * bootTextScale;
	uint32_t cellY = bootTextOffsetY + y * font_height * bootTextScale;
	BootFramebufferFillRectangle(cellX, cellY, font_width * bootTextScale,
				     font_height * bootTextScale, backcolor);

	if (ch == 0 || ch == 0xff) { return; }

	uint8_t* glyph = BootGlyph(ch);

	for (uint8_t glyphX = 0; glyphX < font_width - 1; glyphX++) {
		for (uint8_t glyphY = 0; glyphY < font_height; glyphY++) {

			if (glyph[glyphX] && ((glyph[glyphX] >> glyphY) & 1)) {
				for (uint8_t scaleY = 0; scaleY < bootTextScale; scaleY++) {
					for (uint8_t scaleX = 0; scaleX < bootTextScale; scaleX++) {

						BootFramebufferPutPixel(cellX + glyphX * bootTextScale + scaleX,
									 cellY + glyphY * bootTextScale + scaleY,
									 forecolor);
					}
				}
			}
		}
	}
}


static void BootFramebufferDrawChar(uint8_t ch, uint8_t color, uint8_t x, uint8_t y) {

	BootFramebufferDrawCell(ch, color, 0x00, x, y);
}


static void BootFramebufferClear() {

	if (!bootFramebufferActive) { return; }
	BootFramebufferClearRectangle(0, 0, bootFramebufferWidth, bootFramebufferHeight);
}


static void BootFramebufferRenderTextBuffer() {

	if (!bootFramebufferActive) { return; }

	BootFramebufferClear();
	volatile uint16_t* vidmem = (volatile uint16_t*)0xb8000;

	for (uint8_t y = 0; y < 25; y++) {
		for (uint8_t x = 0; x < 80; x++) {

			uint16_t entry = vidmem[80*y + x];
			uint8_t ch = entry & 0xff;
			uint8_t color = (entry >> 8) & 0x0f;
			if (ch != 0) { BootFramebufferDrawChar(ch, color, x, y); }
		}
	}
}


void ConfigureBootFramebuffer(void* multiboot_structure) {

	if (multiboot_structure == 0) { return; }

	uint32_t flags = *((uint32_t*)multiboot_structure);
	if ((flags & (1 << 12)) == 0) { return; }

	size_t base = (size_t)multiboot_structure;
	uint64_t framebufferAddress = *((uint64_t*)(base + 88));
	uint32_t framebufferPitch = *((uint32_t*)(base + 96));
	uint32_t framebufferWidth = *((uint32_t*)(base + 100));
	uint32_t framebufferHeight = *((uint32_t*)(base + 104));
	uint8_t framebufferBpp = *((uint8_t*)(base + 108));
	uint8_t framebufferType = *((uint8_t*)(base + 109));
	uint8_t redFieldPosition = 16;
	uint8_t redMaskSize = 8;
	uint8_t greenFieldPosition = 8;
	uint8_t greenMaskSize = 8;
	uint8_t blueFieldPosition = 0;
	uint8_t blueMaskSize = 8;

	if ((framebufferAddress >> 32) != 0) { return; }
	if (framebufferWidth == 0 || framebufferHeight == 0 || framebufferPitch == 0) { return; }
	if (framebufferBpp != 8 && framebufferBpp != 15
	 && framebufferBpp != 16 && framebufferBpp != 24 && framebufferBpp != 32) { return; }
	if (framebufferType != 0 && framebufferType != 1) { return; }
	if (framebufferType == 0 && framebufferBpp != 8) { return; }

	bootFramebuffer = (uint8_t*)((uint32_t)framebufferAddress);
	bootFramebufferPitch = framebufferPitch;
	bootFramebufferWidth = framebufferWidth;
	bootFramebufferHeight = framebufferHeight;
	bootFramebufferBpp = framebufferBpp;
	bootFramebufferType = framebufferType;

	if (framebufferType == 1) {

		redFieldPosition = *((uint8_t*)(base + 110));
		redMaskSize = *((uint8_t*)(base + 111));
		greenFieldPosition = *((uint8_t*)(base + 112));
		greenMaskSize = *((uint8_t*)(base + 113));
		blueFieldPosition = *((uint8_t*)(base + 114));
		blueMaskSize = *((uint8_t*)(base + 115));

		if (!BootValidFrameBufferField(redFieldPosition, redMaskSize, framebufferBpp)
		 || !BootValidFrameBufferField(greenFieldPosition, greenMaskSize, framebufferBpp)
		 || !BootValidFrameBufferField(blueFieldPosition, blueMaskSize, framebufferBpp)) {

			BootDefaultFrameBufferFields(framebufferBpp,
						     redFieldPosition,
						     redMaskSize,
						     greenFieldPosition,
						     greenMaskSize,
						     blueFieldPosition,
						     blueMaskSize);
		}
	}

	bootRedFieldPosition = redFieldPosition;
	bootRedMaskSize = redMaskSize;
	bootGreenFieldPosition = greenFieldPosition;
	bootGreenMaskSize = greenMaskSize;
	bootBlueFieldPosition = blueFieldPosition;
	bootBlueMaskSize = blueMaskSize;

	uint32_t scaleX = framebufferWidth / (80 * font_width);
	uint32_t scaleY = framebufferHeight / (25 * font_height);
	uint32_t scale = scaleX < scaleY ? scaleX : scaleY;
	if (scale < 1) { scale = 1; }
	if (scale > 4) { scale = 4; }

	bootTextScale = scale;
	bootTextOffsetX = (framebufferWidth - (80 * font_width * scale)) / 2;
	bootTextOffsetY = (framebufferHeight - (25 * font_height * scale)) / 2;
	bootFramebufferActive = true;
	BootFramebufferClear();
}



void putcharTUI(unsigned char ch, unsigned char forecolor,
		unsigned char backcolor, uint8_t x, uint8_t y) {

	uint16_t attrib = (backcolor << 4) | (forecolor & 0x0f);
	volatile uint16_t* vidmem;
	vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
	*vidmem = ch | (attrib << 8);
	BootFramebufferDrawCell(ch, forecolor, backcolor, x, y);
}


void TUI(uint8_t forecolor, uint8_t backcolor,
		uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
		bool shadow) {

	for (uint8_t y = 0; y < 25; y++) {

		for (uint8_t x = 0; x < 80; x++) {

			putcharTUI(0xff, 0x00, backcolor, x, y);
		}
	}

	uint8_t resetX = x1;

	while (y1 < y2) {

		while (x1 < x2) {

			putcharTUI(0xff, 0x00, forecolor, x1, y1);
			x1++;
		}
		y1++;

		//side shadow
		if (shadow) { putcharTUI(0xff, 0x00, 0x00, x1, y1); }
		x1 = resetX;
	}

	//bottom shadow
	if (shadow) {

		for (resetX++; resetX < (x2 + 1); resetX++) {

			putcharTUI(0xff, 0x00, 0x00, resetX, y1);
		}
	}
}


void printfTUI(char* str, uint8_t forecolor, uint8_t backcolor, uint8_t x, uint8_t y) {

	for (int i = 0; str[i] != '\0'; i++) {

		if (str[i] == '\n') {

			y++;
			x = 0;
		} else {
			putcharTUI(str[i], forecolor, backcolor, x, y);
			x++;
		}
		if (x >= 80) {

			y++;
			x = 0;
		}
		if (y >= 25) { y = 0; }
	}
}



//set color for printf
uint16_t setTextColor(bool set, uint16_t color = 0x07) {

	static uint16_t newColor = 0x07; //default gray on black text
	if (set) { newColor = color; }
	return newColor;
}


//the main print function for textmode
void printf(char* strChr) {

	static uint8_t x = 0, y = 0;
	static bool cliCursor = false;

	uint16_t attrib = setTextColor(false);
	volatile uint16_t* vidmem;


	uint8_t* str = (uint8_t*)strChr;


	for (int i = 0; str[i] != '\0'; i++) {

		vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);

		switch (str[i]) {

				case '\b':
					vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
					*vidmem = ' ' | (attrib << 8);
					BootFramebufferDrawChar(' ', attrib & 0x0f, x, y);
					if (x > 0) {
						vidmem--; *vidmem = '_' | (attrib << 8);
						BootFramebufferDrawChar('_', attrib & 0x0f, x-1, y);
						x--;
					}
					break;

				case '\n':
					*vidmem = ' ' | (attrib << 8);
					BootFramebufferDrawChar(' ', attrib & 0x0f, x, y);
					y++;
					x = 0;
					break;

			case '\t': //$: shell interface

				if (!i) {
					cliCursor = true;

					if (x < 3) { x = 3; }

						vidmem = (volatile uint16_t*)0xb8000 + (80*y);
						*vidmem = '$' | 0xa00;
						BootFramebufferDrawChar('$', 0x0a, 0, y);
						vidmem++; *vidmem = ':' | 0xf00;
						BootFramebufferDrawChar(':', 0x0f, 1, y);
						vidmem++; *vidmem = ' ';
						BootFramebufferDrawChar(' ', attrib & 0x0f, 2, y);
					} else {
						*vidmem = '_' | (attrib << 8);
						BootFramebufferDrawChar('_', attrib & 0x0f, x, y);
					}
					break;

			case '\v': //clear screen

				for (y = 0; y < 25; y++) {
					for (x = 0; x < 80; x++) {

						vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
						*vidmem = 0x00;
					}
					}
					x = 0;
					y = 0;
					BootFramebufferClear();
					break;
				default:
					*vidmem = str[i] | (attrib << 8);
					BootFramebufferDrawChar(str[i], attrib & 0x0f, x, y);
					x++;
					break;
			}

		if (x >= 80) {

			y++;
			x = 0;
		}


		//scrolling
		if (y >= 25) {

			uint16_t scroll_temp;

			for (y = 1; y < 25; y++) {
				for (x = 0; x < 80; x++) {

					vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
					scroll_temp = *vidmem;

					vidmem -= 80;
					*vidmem = scroll_temp;

					if (y == 24) {

						vidmem = (volatile uint16_t*)0xb8000 + (1920+x);
						*vidmem = ' ' | (attrib << 8);
					}
				}
				}
				x = 0;
				y = 24;
				BootFramebufferRenderTextBuffer();
			}
		}
	}


void ConfigureGraphicsFromMultiboot(VideoGraphicsArray* vga, void* multiboot_structure) {

	if (multiboot_structure == 0) { return; }

	uint32_t flags = *((uint32_t*)multiboot_structure);
	if ((flags & (1 << 12)) == 0) { return; }

	size_t base = (size_t)multiboot_structure;
	uint64_t framebufferAddress = *((uint64_t*)(base + 88));
	uint32_t framebufferPitch = *((uint32_t*)(base + 96));
	uint32_t framebufferWidth = *((uint32_t*)(base + 100));
	uint32_t framebufferHeight = *((uint32_t*)(base + 104));
	uint8_t framebufferBpp = *((uint8_t*)(base + 108));
	uint8_t framebufferType = *((uint8_t*)(base + 109));

	uint8_t redFieldPosition = 16;
	uint8_t redMaskSize = 8;
	uint8_t greenFieldPosition = 8;
	uint8_t greenMaskSize = 8;
	uint8_t blueFieldPosition = 0;
	uint8_t blueMaskSize = 8;

	if (framebufferType == 1) {

		redFieldPosition = *((uint8_t*)(base + 110));
		redMaskSize = *((uint8_t*)(base + 111));
		greenFieldPosition = *((uint8_t*)(base + 112));
		greenMaskSize = *((uint8_t*)(base + 113));
		blueFieldPosition = *((uint8_t*)(base + 114));
		blueMaskSize = *((uint8_t*)(base + 115));
	}

	vga->UseLinearFrameBuffer(framebufferAddress,
				  framebufferPitch,
				  framebufferWidth,
				  framebufferHeight,
				  framebufferBpp,
				  framebufferType,
				  redFieldPosition,
				  redMaskSize,
				  greenFieldPosition,
				  greenMaskSize,
				  blueFieldPosition,
				  blueMaskSize);
}


void printfLine(const char* str, uint8_t line) {

	for (uint16_t i = 0; str[i] != '\0'; i++) {

		volatile uint16_t* vidmem = (volatile uint16_t*)0xb8000 + (80*line+i);
		*vidmem = str[i] | 0x700;
		if (i < 80 && line < 25) { BootFramebufferDrawChar(str[i], 0x07, i, line); }
	}
}



void printfHex(uint8_t key) {

	char* foo = "00 ";
	char* hex = "0123456789ABCDEF";

	foo[0] = hex[(key >> 4) & 0x0F];
	foo[1] = hex[key & 0x0F];
	printf(foo);
}



uint16_t strlen(char* args) {

	uint16_t length = 0;
	for (length = 0; args[length] != '\0'; length++) {}
	return length;
}



bool strcmp(char* one, char* two) {

	uint16_t i = 0;

	for (i; one[i] != '\0'; i++) {

		if (one[i] != two[i]) {

			return false;
		}
	}
	return true;
}



uint32_t str2int(char* args) {

	uint32_t number = 0;
	uint16_t i = 0;
	bool gotNum = false;

	for (uint16_t i = 0; args[i] != '\0'; i++) {

		if ((args[i] >= 58 || args[i] <= 47) && args[i] != ' ') {

			return 0;
		}

		if (args[i] != ' ') {

			number *= 10;
			number += ((uint32_t)args[i] - 48);
			gotNum = true;
			args[i] = ' ';
                } else {
                        if (gotNum) { return number; }
		}
	}
	return number;
}


char* int2str(uint32_t num) {

	uint32_t numChar = 1;
	uint8_t i = 1;

	if (num % 10 != num) {

		while ((num / (numChar)) >= 10) {

			numChar *= 10;
			i++;
		}
		char* str = "4294967296";
		uint8_t strIndex = 0;

		while (i) {

			str[strIndex] = (char)(((num / (numChar)) % 10) + 48);

			if (numChar >= 10) { numChar /= 10; }
			strIndex++;
			i--;
		}
		str[strIndex] = '\0';
		return str;
	}
	char* str = " ";
	str[0] = (num + 48);

	return str;
}


char* argparse(char* args, uint8_t num) {

	char buffer[256];

	bool valid = false;
	uint8_t argIndex = 0;
	uint8_t bufferIndex = 0;


	for (int i = 0; i < (strlen(args) + 1); i++) {

		if (args[i] == ' ' || args[i] == '\0') {

			if (valid) {

				if (argIndex == num) {

					buffer[bufferIndex] = '\0';
					char* arg = buffer;
					return arg;
				}
				argIndex++;
			}
			valid = false;
		} else {
			if (argIndex == num) {

				buffer[bufferIndex] = args[i];
				bufferIndex++;
			}
			valid = true;
		}
	}
	//       |
	//this   v
	return "wtf";
}

uint8_t argcount(char* args) {

	uint8_t i = 0;
	char* foo = argparse(args, i);


	while (foo != "wtf") {

		foo = argparse(args, i);
		i++;
	}
	return i-1;
}


void altCode(uint8_t c, uint8_t &numCode) {

	static uint8_t count = 0;
	bool bitShift = (count % 2 == 0);
	count++;

	if (c <= '9' && c >= '0') { numCode += (c - '0'); }
	if (c <= 'f' && c >= 'a') { numCode += (c - 'a') + 10; }
	numCode <<= (4 * bitShift);
}




//class that connects the keyboard to the rest of the command line
class CLIKeyboardEventHandler : public KeyboardEventHandler, public CommandLine {

	public:
		bool pressed;
	public:
		CLIKeyboardEventHandler(GlobalDescriptorTable* gdt,
					TaskManager* tm,
					MemoryManager* mm,
					FileSystem* filesystem,
					CMOS* cmos,
					DriverManager* drvManager)
		: CommandLine(gdt, tm, mm, filesystem, cmos, drvManager) {

			this->cli = true;
		}

		void modeSelect(uint16_t mode, bool pressed, unsigned char ch,
				bool ctrl, bool type, FileSystem* filesystem) {

			switch (this->cliMode) {

				case 2:
					piano(pressed, ch);
					break;
				case 3:
					snake(ch);
					break;
				case 4:
					if (type) { fileMain(pressed, ch, ctrl, filesystem); }
					break;
				case 5:
					space(pressed, ch);
					break;
				case 6:
					bootScreen(pressed, ch);
					break;
				default:
					break;
			}
		}

		void OnKeyDown(char c) {

			this->pressed = true;
			keyChar = c;

			//num code
			if (this->alt) {

				altCode(c, numCode);
				return;
			}

			if (this->alt == false && this->numCode != 0) {

				c = this->numCode;
				keyChar = this->numCode;
			}
			this->numCode = 0;

			//mode shortcut
			if (this->ctrl) {

				switch (c) {
					case 'c':
						this->resetMode();
						return; break;
					case 'p':
						this->cliMode = 2;
						modeSet(this->cliMode);
						break;
					case 's':
						this->cliMode = 3;
						modeSet(this->cliMode);
						break;
					case 'e':
						this->cliMode = 4;
						modeSet(this->cliMode);
						break;
					case 'i':
						this->cliMode = 5;
						modeSet(this->cliMode);
						break;
					case 'b':
						this->cliMode = 6;
						modeSet(this->cliMode);
						break;
					default:
						break;
				}
			}

			//normal cli
			if (this->cliMode == 0) {

				char* foo = " \t";
				foo[0] = c;

				switch (c) {

					//scroll command history
					case '\xff':
						break;
					case '\xfc':
						break;
					//up
					case '\xfd':

						if (index > 0) {

							for (int i = 0; i < index; i++) {

								input[index] = 0x00;
								printf("\b");
							}
						}

						for (index = 0; lastCmd[index] != '\0'; index++) {

							input[index] = lastCmd[index];
						}
						input[index] = '\0';
						printf(input);
						break;
					//down
					case '\xfe':

						for (int i = 0; i < index; i++) {

							input[index] = 0x00;
							printf("\b");
						}
						index = 0;
						break;
					//backspace
					case '\b':
						if (index > 0) {

							printf(foo);
							index--;
							input[index] = 0;
						}
						break;
					//enter
					case '\n':
						printf(foo);
						input[index] = '\0';

						//execute command input
						if (index > 0 && input[0] != ' ') {

							for (int i = 0; input[i] != '\0'; i++) {
								lastCmd[i] = input[i];
							}
							lastCmd[index] = '\0';

							command(input, index);
						}

						input[0] = 0x00;
						index = 0;
						break;
					//type
					default:
						printf(foo);
						input[index] = keyChar;
						index++;
						break;
				}
			} else {
				//modes
				this->modeSelect(this->cliMode, this->pressed,
						this->keyChar, this->ctrl, 1, this->filesystem);

				//lol
				index = 0;
			}
		}


		void OnKeyUp() { this->pressed = false; }


		void resetCmd() {

			for (uint16_t i = 0; i < 256; i++) {

				input[i] = 0x00;
			}
		}

		//print the ui for mode here
		void modeSet(uint8_t mode) {

			//reset mode before entering next one
			this->resetMode();
			this->cliMode = mode;

			switch (this->cliMode) {

				case 2:
					//piano
					pianoTUI();
					break;
				case 3:
					//snake
					snake('r');
					snakeTUI();
					snakeInit();
					break;
				case 4:
					//file editor
					fileTUI();
					fileMain(0, 'c', 1, nullptr);
					break;
				case 5:
					//space game
					space(true, 'r');
					spaceTUI();
					break;
				case 6:
					//initial mode for cube
					bootInit();
					bootScreen(false, 'b');
					break;
				default:
					printf("Mode not found.\n");
					break;
			}
		}
		//print error messages for modes
		//and other things you want
		void resetMode() {

			printf("\v");

			switch (this->cliMode) {

				case 2:
					printf("\nExiting piano mode...\n\n");
					break;
				case 3:
					printf("\nExiting snake mode...\n\n");
					break;
				case 4:
					fileMain(0, 'c', 1, nullptr);
					printf("\nExiting file edit mode...\n\n");
					break;
				case 5:
					printf("\nExiting space mode...\n\n");
					break;
				case 6:
					break;
				default:
					break;
			}
			this->cliMode = 0;
		}
};


void sleep(uint32_t ms) {

	//sleep 1 = wait 1 ms
	PIT pit;

	for (uint32_t i = 0; i < ms; i++) {

		pit.setCount(1193182/1000);
		uint32_t start = pit.readCount();

		while ((start - pit.readCount()) < 1000) {}
	}
}


double getTicks() {

	PIT pit;
	pit.setCount(1193182/1000);
	return (double)(pit.readCount());
}



void memWrite(uint32_t memory, uint32_t inputVal) {

	volatile uint32_t* value;
	value = (volatile uint32_t*)memory;
	*value = inputVal;
}

uint32_t memRead(uint32_t memory) {

	volatile uint32_t* value;
	value = (volatile uint32_t*)memory;

	return *value;
}


void printLain(uint8_t num, bool cube) {

	Funny Lain;

	if (cube) {
		Lain.cubeAscii(num);
		return;
	} else {
		printf("\v");
	}


	switch (num) {

		case 0:
			Lain.LainFace();
			break;
		case 1:
			Lain.LainSpiral();
			break;
		case 2:
			Lain.god();
			break;
		case 3:
			Lain.LainNavi();
			break;
		case 4:
			Lain.LainAscii();
			break;
		default:
			Lain.LainFace();
			break;
	}
}



void makeBeep(uint32_t freq) {

	Speaker speaker;
	speaker.Speak(freq);
}


uint16_t prng() {

	PIT pit;
	uint16_t seed = (uint16_t)pit.readCount();
	uint16_t lfsr = seed;
	uint16_t period = 0;

	do {
		uint16_t lsb = lfsr & 1u;
		lfsr >>= 1;
		lfsr ^= (-lsb) & 0xb400u;

		period++;

	} while (period < seed);


	return lfsr;
}


void reboot() {

	asm volatile ("cli");

	uint8_t read = 0x02;
	Port8Bit resetPort(0x64);

	while (read & 0x02) {

		read = resetPort.Read();
	}

	resetPort.Write(0xfe);
	asm volatile ("hlt");
}

uint32_t cmosDetectMemory() {

	uint32_t total = 0;
	uint8_t lowmem, highmem = 0;

	Port8Bit WriteCMOS(0x70);
	Port8Bit ReadCMOS(0x71);

	//size of low memory
	WriteCMOS.Write(0x15);
	sleep(10);
	lowmem = ReadCMOS.Read();

	WriteCMOS.Write(0x16);
	sleep(10);
	highmem = ReadCMOS.Read();
	total += ((lowmem | highmem) << 10);

	//total memory between 1M and 16M
	WriteCMOS.Write(0x17);
	sleep(10);
	lowmem = ReadCMOS.Read();

	WriteCMOS.Write(0x18);
	sleep(10);
	highmem = ReadCMOS.Read();
	total += ((lowmem | highmem) << 10);

	//total memory between 16M and 4G
	WriteCMOS.Write(0x30);
	sleep(10);
	lowmem = ReadCMOS.Read();

	WriteCMOS.Write(0x31);
	sleep(10);
	highmem = ReadCMOS.Read();
	total += ((lowmem | highmem) << 16);

	return total;
}



uint8_t Web2EGA(uint32_t color) {

	uint8_t bytes[3];
	bytes[2] = color >> 16;
	bytes[1] = (color >> 8) & 0xff;
	bytes[0] = color & 0xff;

	uint8_t result = 0;

	for (int i = 0; i < 3; i++) {

		if        (bytes[i] < 0x2b) { bytes[i] = 0x00;
		} else if (bytes[i] < 0x80) { bytes[i] = 0x55;
		} else if (bytes[i] < 0xd5) { bytes[i] = 0xaa;
		} else {		      bytes[i] = 0xff; }
	}

	for (int i = 0; i < 3; i++) {

		switch (bytes[i]) {

			//both sets of
			//3 bits
			case 0xff:
				result |= (1 << (i+3));
				result |= (1 << i);
				break;
			//most sig 3 bits
			case 0x55: result |= (1 << (i+3)); break;
			//least sig 3 bits
			case 0xaa: result |= (1 << i); break;
			default: break;
		}
	}
	return result;
}




CommandLine* LoadScriptForTask(bool set, CommandLine* cli = 0) {

	static CommandLine* retCli = 0;
	if (set) { retCli = cli; }
	return retCli;
}


Desktop* LoadDesktopForTask(bool set, Desktop* desktop = 0) {

	static Desktop* retDesktop = 0;
	if (set) { retDesktop = desktop; }
	return retDesktop;
}

void DrawDesktopTask() {

	Desktop* desktop = LoadDesktopForTask(false);
	if (!desktop->gc->SetMode(GRAPHICS_SCREEN_WIDTH, GRAPHICS_SCREEN_HEIGHT, 32)) {

		desktop->gc->SetMode(GRAPHICS_LOGICAL_WIDTH, GRAPHICS_LOGICAL_HEIGHT, 8);
	}
    //finna add some stuff
    PIT pit;
    const uint32_t frameTicks = 1193182 / 60;


	while (1) {
        pit.setCount(frameTicks);
       // uint32_t start = pit.readCount();

        desktop->Draw(desktop->gc);
        asm volatile("hlt");


    }
}


typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;



extern "C" void callConstructors() {

	for (constructor* i = &start_ctors; i != &end_ctors; i++) {
		(*i)();
	}
}



extern "C" void kernelMain(void* multiboot_structure, uint32_t magicnumber) {

	ConfigureBootFramebuffer(multiboot_structure);
	printf("Hello!!!!!!!!!!!\n");


	GlobalDescriptorTable gdt;
	TaskManager taskManager(&gdt);

	//heap manager
	uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
	size_t heap = 4*1024*1024;
	MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);


	InterruptManager interrupts(0x20, &gdt, &taskManager);
	printf("Initializing Hardware, Stage 1\n");

	DriverManager drvManager;

	//drivers and command line
	CMOS cmos;
	AdvancedTechnologyAttachment ata0m(0x1F0, true);
	FileSystem LainFileSystem(&ata0m);

	CLIKeyboardEventHandler* kbhandler = (CLIKeyboardEventHandler*)memoryManager.malloc(sizeof(CLIKeyboardEventHandler));
	new (kbhandler) CLIKeyboardEventHandler(&gdt, &taskManager, &memoryManager, &LainFileSystem, &cmos, &drvManager);
	kbhandler->hash_cli_init(); //init command line

	KeyboardDriver keyboard(&interrupts, kbhandler);
	kbhandler->caps = false;
	kbhandler->shift = false;
	kbhandler->ctrl = false;


	//gui driver stuff
	VideoGraphicsArray vga;
	ConfigureGraphicsFromMultiboot(&vga, multiboot_structure);
	Simulator Lain(&cmos);
	Button buttons;
	Desktop desktop(GRAPHICS_LOGICAL_WIDTH, GRAPHICS_LOGICAL_HEIGHT, 0x01, &vga, &gdt, &taskManager,
			&memoryManager, &LainFileSystem, &cmos,
			&drvManager, &buttons, &Lain);
	MouseDriver mouse(&interrupts, &desktop);


	drvManager.AddDriver(&keyboard);
	drvManager.AddDriver(&mouse);
	drvManager.AddDriver(&ata0m);


	//pci and init
	PeripheralComponentInterconnectController PCIController(&memoryManager);
	PCIController.SelectDrivers(&drvManager, &interrupts);


	//network
	amd_am79c973* eth0 = 0;
	if (drvManager.numDrivers > 3) { eth0 = (amd_am79c973*)(drvManager.drivers[3]); }
	//amd_am79c973 eth0(PCIController.PCIdev, &interrupts);
	//drvManager.drivers[3] = &eth0;


	printf("\nInitializing Hardware, Stage 2\n");
	drvManager.ActivateAll();

	//network init
	//IP Address
    printf("\nnetwork init\n");
	uint8_t ip1 = 10, ip2 = 0, ip3 = 2, ip4 = 15;
	uint32_t ip_be = ((uint32_t)ip4 << 24) | ((uint32_t)ip3 << 16)
			| ((uint32_t)ip2 << 8) | (uint32_t)ip1;

	// IP Address of the default gateway
	uint8_t gip1 = 10, gip2 = 0, gip3 = 2, gip4 = 2;
	uint32_t gip_be = ((uint32_t)gip4 << 24) | ((uint32_t)gip3 << 16)
			| ((uint32_t)gip2 << 8) | (uint32_t)gip1;

	uint8_t subnet1 = 255, subnet2 = 255, subnet3 = 255, subnet4 = 0;
	uint32_t subnet_be = ((uint32_t)subnet4 << 24) | ((uint32_t)subnet3 << 16)
			| ((uint32_t)subnet2 << 8) | (uint32_t)subnet1;

	kbhandler->network = 0;
	if (eth0 != 0) {

		eth0->SetIPAddress(ip_be);

		EtherFrameProvider* etherframe =
				(EtherFrameProvider*)memoryManager.malloc(sizeof(EtherFrameProvider));
		AddressResolutionProtocol* arp =
				(AddressResolutionProtocol*)memoryManager.malloc(sizeof(AddressResolutionProtocol));
		InternetProtocolProvider* ipv4 =
				(InternetProtocolProvider*)memoryManager.malloc(sizeof(InternetProtocolProvider));
		InternetControlMessageProtocol* icmp =
				(InternetControlMessageProtocol*)memoryManager.malloc(sizeof(InternetControlMessageProtocol));
		Network* network =
				(Network*)memoryManager.malloc(sizeof(Network));

		if (etherframe != 0 && arp != 0 && ipv4 != 0 && icmp != 0 && network != 0) {

			new (etherframe) EtherFrameProvider(eth0);
			new (arp) AddressResolutionProtocol(etherframe);
			new (ipv4) InternetProtocolProvider(etherframe, arp, gip_be, subnet_be);
			new (icmp) InternetControlMessageProtocol(ipv4);
			new (network) Network(eth0, arp, ipv4, icmp, gip_be, subnet_be);
			kbhandler->network = network;
		}
	}


	printf("Initializing Hardware, Stage 3\n");
	interrupts.Activate();

	printf("\n\nEverything seems fine.\n");


	interrupts.boot = true;
	makeBeep(600);

	//while (1) {}


	//the boot screen and very important cube
	uint8_t cubeNum = 0;
	printLain(4, false);
	kbhandler->pressed = false;
	while (kbhandler->pressed == false) {

		printLain(cubeNum, true);
		cubeNum++;
		sleep(10);
	}


	//initialize command line hash table
	kbhandler->gui = false;
	kbhandler->cli = true;
	kbhandler->OnKeyDown('\b');
	printf("\v");


	//this is the command line :DDDDDDDDDDDDDDDDDDDDDDDDDD
	while (keyboard.handler->keyValue != 0x5b) { //0x5b = command/windows key

		kbhandler->cli = true;

		while (kbhandler->cliMode) {

			kbhandler->cli = false;
			kbhandler->modeSelect(kbhandler->cliMode, kbhandler->pressed,
					kbhandler->keyChar, kbhandler->ctrl, 0,
					&LainFileSystem);
		}
	}
	kbhandler->cli = true;
	kbhandler->gui = true;


	//initialize desktop
	KeyboardDriver keyboardDesktop(&interrupts, &desktop);
	drvManager.Replace(&keyboardDesktop, 0);

	desktop.CreateChild(1, "Terminal", kbhandler);


	//add task for drawing desktop
	LoadDesktopForTask(true, &desktop);
	Task guiTask(&gdt, DrawDesktopTask, "Lain GUI");
	taskManager.AddTask(&guiTask);


	//gui
	while (1) {

		/*this is the loop where the kernel
		  exists in,     o7
					    /|
					    / \ */

	}
}
