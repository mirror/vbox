OUTPUT_FORMAT("pe-i386", "pe-i386", "pe-i386")
OUTPUT_ARCH(i386)

ENTRY(__text)
SECTIONS {
	. = ALIGN(16);
	/* Start address of Etherboot in the virtual address space */
	__virt_start = 0;
	__text = . ;
	.text.nocompress : {
		*(.text*.nocompress)
		. = ALIGN(16);
	} = 0x9090

	.text16 : {
		__text16 = .;
		*(.text16)
		*(.text16.*)
		__etext16 = . ;
	}
	.text.compress : {
		*(.text)
		*(.text.*)
	} = 0x9090
	.rodata : {
		. = ALIGN(4);
		*(.rodata)
		*(.rodata.*)
	}
	. = ALIGN(4);
	.drivers.pci : {
		_pci_drivers = . ;
		*(.drivers.pci);
		_pci_drivers_end = . ;
	}
	. = ALIGN(4);
	.drivers.isa : {
		_isa_drivers = . ;
		*(.drivers.isa);
		_isa_drivers_end = .;
	}
	__etext = . ;
	__data = . ;
	.data : {
		*(.data)
		*(.data.*)
	}
	__edata = . ;
	__uncompressed_verbatim_end = . ;
	. = ALIGN(16);
	.bss.preserve : {
		*(.bss.preserve)
		*(.bss.preserve.*)
	}
	__bss = . ;
	.bss : {
		*(.bss)
		*(.bss.*)
	}
	. = ALIGN(16);
	__ebss = .;
	__stack = . ;
	.stack : {
		__stack_start = . ;
		. = ALIGN(8);
		__stack = . ;
		. = . + 4096;
		__estack = . ;
		__stack_end = . ;
	}
	__bss_size = __ebss - __bss;
	__stack_offset = __stack - __text ;
	__stack_offset_pgh = __stack_offset / 16 ;
	__stack_size = __stack_end - __stack_start ;
	. = ALIGN(16);
	__end = . ;

	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* PXE-specific symbol calculations.  The results of these are
	 * needed in romprefix.S, which is why they must be calculated
	 * here.
	 */
	__pxe_stack_size = __pxe_stack_t_size
		+ __pxe_callback_interface_size
		+ __rm_callback_interface_size
		+ __e820mangler_size + 15 ;

}
