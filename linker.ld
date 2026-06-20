ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

SECTIONS {

	. = 0x0100000;
	kernel_start = .;

	.text : {
		*(.multiboot)
		*(.text .text.*)
	}

	.rodata : {
		*(.rodata .rodata.*)
	}

	.data : {

		start_ctors = .;
		KEEP(*(.init_array));
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)));
		end_ctors = .;

		*(.data .data.*)
		*(.data.rel .data.rel.*)
		*(.got .got.*)
	}

	.bss : {
		*(COMMON)
		*(.bss .bss.*)
	}

	kernel_end = .;
	ASSERT(kernel_end < 0x00400000, "kernel image overlaps hardcoded heap at 4 MiB")


	/DISCARD/ : {
		*(.fini_array*)
		*(.eh_frame .eh_frame.*)
		*(.comment)
		*(.note.GNU-stack)
		*(.note.gnu.property)
	}
	}
