#include <drivers/driver.h>


using namespace os::drivers;
using namespace os::common;



Driver::Driver() {
}

Driver::~Driver() {
}

void Driver::Activate() {
}

int Driver::Reset() {

	return 0;
}

void Driver::Deactive() {
}



DriverManager::DriverManager() {

	numDrivers = 0;

	for (int i = 0; i < 256; i++) {

		drivers[i] = 0;
	}
}


void DriverManager::AddDriver(Driver* drv) {

	if (drv == 0 || numDrivers >= 256) { return; }

	drivers[numDrivers] = drv;
	numDrivers++;
}


void DriverManager::ActivateAll() {

	for (int i = 0; i < numDrivers; i++) {

		if (drivers[i] != 0) { drivers[i]->Activate(); }
	}
}


void DriverManager::Replace(Driver* drv, int drvNum) {

	if (drv == 0 || drvNum < 0 || drvNum >= 256) { return; }

	drivers[drvNum] = drv;
	drivers[drvNum]->Activate();
}
