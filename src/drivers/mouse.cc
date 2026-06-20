#include <drivers/mouse.h>

using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;


static bool WaitPS2InputClear(Port8Bit& commandport) {

	for (uint32_t timeout = 0; timeout < 100000; timeout++) {

		if ((commandport.Read() & 0x02) == 0) { return true; }
	}
	return false;
}


static bool WaitPS2OutputFull(Port8Bit& commandport) {

	for (uint32_t timeout = 0; timeout < 100000; timeout++) {

		if ((commandport.Read() & 0x01) != 0) { return true; }
	}
	return false;
}


static void DrainPS2Output(Port8Bit& commandport, Port8Bit& dataport) {

	for (uint32_t timeout = 0; timeout < 256 && (commandport.Read() & 0x01) != 0; timeout++) {

		dataport.Read();
	}
}



MouseEventHandler::MouseEventHandler() {
}

void MouseEventHandler::OnActivate() {
}

void MouseEventHandler::OnMouseDown(uint8_t button) {
}

void MouseEventHandler::OnMouseUp(uint8_t button) {
}

void MouseEventHandler::OnMouseMove(int x, int y) {
}



MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
: InterruptHandler(0x2C, manager),
dataport(0x60),
commandport(0x64) {

	this->handler = handler;
}


MouseDriver::~MouseDriver() {
}



void MouseDriver::Activate() {

	offset = 0;
	buttons = 0;

	DrainPS2Output(commandport, dataport);

	if (!WaitPS2InputClear(commandport)) { return; }
	commandport.Write(0xA8); // activate auxiliary device

	if (!WaitPS2InputClear(commandport)) { return; }
	commandport.Write(0x20); // get current state

	if (!WaitPS2OutputFull(commandport)) { return; }
	uint8_t status = dataport.Read() | 2;

	if (!WaitPS2InputClear(commandport)) { return; }
	commandport.Write(0x60); // set state

	if (!WaitPS2InputClear(commandport)) { return; }
	dataport.Write(status);

	if (!WaitPS2InputClear(commandport)) { return; }
	commandport.Write(0xD4);

	if (!WaitPS2InputClear(commandport)) { return; }
	dataport.Write(0xF4);

	if (WaitPS2OutputFull(commandport)) { dataport.Read(); }
	if (handler != 0) { handler->OnActivate(); }
}






uint32_t MouseDriver::HandleInterrupt(uint32_t esp) {

	uint8_t status = commandport.Read();

	if (!(status & 0x20)) {
		
		return esp;
	}

	buffer[offset] = dataport.Read();
	
	if (handler == 0) {
		
		return esp;
	}
	
	offset = (offset + 1) % 3;

	if (offset == 0) {

		handler->OnMouseMove((int8_t)buffer[1], -((int8_t)buffer[2]));
		
		for (uint8_t i = 0; i < 3; i++) {
			
			if ((buffer[0] & (0x1 << i)) != (buttons & (0x1 << i))) {
				
				if (buttons & (0x1<<i)) {

					handler->OnMouseUp(i+1);
					this->pressed = false;
				} else {

					handler->OnMouseDown(i+1);
					this->pressed = true;
				}
			}
		}

		buttons = buffer[0];
	}

	return esp;
}
