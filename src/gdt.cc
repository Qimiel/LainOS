#include <gdt.h>

using namespace os;
using namespace os::common;


GlobalDescriptorTable::GlobalDescriptorTable()
: nullSegmentSelector(0, 0, 0),
unusedSegmentSelector(0, 0, 0),
codeSegmentSelector(0, 0xFFFFFFFF, 0x9A),
dataSegmentSelector(0, 0xFFFFFFFF, 0x92) {


	struct GlobalDescriptorTablePointer {
		uint16_t size;
		uint32_t base;
	} __attribute__((packed));

	GlobalDescriptorTablePointer gdtPointer;
	gdtPointer.size = sizeof(GlobalDescriptorTable) - 1;
	gdtPointer.base = (uint32_t)this;

	asm volatile("lgdt %0" : : "m" (gdtPointer) : "memory");

	uint16_t codeSegment = CodeSegmentSelector();
	uint16_t dataSegment = DataSegmentSelector();
	asm volatile(
		"movw %%dx, %%ds\n"
		"movw %%dx, %%es\n"
		"movw %%dx, %%fs\n"
		"movw %%dx, %%gs\n"
		"movw %%dx, %%ss\n"
		"pushw %%cx\n"
		"pushl $1f\n"
		"lretl\n"
		"1:\n"
		:
		: "c" (codeSegment), "d" (dataSegment)
		: "memory");

}



GlobalDescriptorTable::~GlobalDescriptorTable() {
}



uint16_t GlobalDescriptorTable::DataSegmentSelector() {

	return (uint8_t*)&dataSegmentSelector - (uint8_t*)this;
}



uint16_t GlobalDescriptorTable::CodeSegmentSelector() {

	return (uint8_t*)&codeSegmentSelector - (uint8_t*)this;
}



GlobalDescriptorTable::SegmentDescriptor::SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t flags) {

	uint8_t* target = (uint8_t*)this;

	if (limit <= 65536)  {
	
		target[6] = 0x40;
	} else {

		if ((limit & 0xFFF) != 0xFFF) {

			limit = (limit >> 12) - 1;
		} else {

			limit = limit >> 12;
		}

		target[6] = 0xC0;
	}

	target[0] = limit & 0xFF;
	target[1] = (limit >> 8) & 0xFF;
	target[6] |= (limit >> 16) & 0xF;

	target[2] = base & 0xFF;
	target[3] = (base >> 8) & 0xFF;
	target[4] = (base >> 16) & 0xFF;
	target[7] = (base >> 24) & 0xFF;

	target[5] = flags;
}




uint32_t GlobalDescriptorTable::SegmentDescriptor::Base() {

	uint8_t* target = (uint8_t*)this;
	uint32_t result = target[7];
	
	result = (result << 8) + target[4];
	result = (result << 8) + target[3];
	result = (result << 8) + target[2];
	
	return result;
}



uint32_t GlobalDescriptorTable::SegmentDescriptor::Limit() {


	uint8_t* target = (uint8_t*)this;
	uint32_t result = target[6] & 0xF;

	result = (result << 8) + target[1];
	result = (result << 8) + target[0];

	if ((target[6] & 0xC0) == 0xC0) {
		result = (result << 12) | 0xFFF;
	}

	return result;
}

