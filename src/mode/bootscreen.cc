#include <mode/bootscreen.h>


void printLain(os::common::uint8_t num, bool cube);
void sleep(os::common::uint32_t);


void bootInit() {
	
	
	
	printLain(4, false);
}

void bootScreen(bool pressed, char ch) {

	static os::common::uint16_t cubeCount = 0;

	
	if (cubeCount >= 256) {
	
		cubeCount = 0;
	}
	

	printLain(cubeCount, true);
	cubeCount++;
	sleep(10);
}
