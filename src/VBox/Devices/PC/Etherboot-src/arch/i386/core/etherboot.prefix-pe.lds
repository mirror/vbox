OUTPUT_FORMAT("pe-i386", "pe-i386", "pe-i386")
OUTPUT_ARCH(i386)

ENTRY(__prefix_start)
SECTIONS {
	/* Prefix */
	.prefix : {
		__verbatim_start = . ;
		__prefix_start = . ;
		*(.prefix)
		. = ALIGN(16);
		__prefix_end = . ;
	} = 0x9090
	__prefix_size = __prefix_end - __prefix_start;

	.text.nocompress : {
		*(.prefix.udata)
	} = 0x9090

	_decompress_to = . ;
	.prefix.zdata : {
		__compressed = . ;
		*(.prefix.zdata)
		__compressed_end = . ;
	}
	__compressed_size = __compressed_end - __compressed;

	. = ALIGN(16);
	__verbatim_end = . ;


	/* Size of the core of etherboot in memory */
	__base_size = __end - __text;

	/* _prefix_size is the length of the non-core etherboot prefix */
	__prefix_size = __prefix_end - __prefix_start;

	/* _verbatim_size is the actual amount that has to be copied to base memory */
	__verbatim_size = __verbatim_end - __verbatim_start;

	/* _image_size is the amount of base memory needed to run */
	__image_size = __base_size +  __prefix_size;

	/* Standard sizes rounded up to paragraphs */
	__prefix_size_pgh   = (__prefix_size + 15) / 16;
	__verbatim_size_pgh = (__verbatim_size + 15) / 16;
	__image_size_pgh    = (__image_size + 15) / 16 ;
	
	/* Standard sizes in sectors */
	__prefix_size_sct   = (__prefix_size + 511) / 512;
	__verbatim_size_sct = (__verbatim_size + 511) / 512;
	__image_size_sct    = (__image_size + 511) / 512;

	/* Symbol offsets and sizes for the exe prefix */
	__exe_hdr_size   = 32;
	__exe_size       = __verbatim_size; /* Should this be - 32 to exclude the header? */
	__exe_size_tail  = (__exe_size) % 512;
	__exe_size_pages = ((__exe_size) + 511) / 512;
	__exe_bss_size   = ((__image_size - __verbatim_size) + 15) / 16;
	__exe_ss_offset  = (__stack_offset + __prefix_size - __exe_hdr_size + 15) / 16 ;

	/* This is where we copy the compressed image before decompression.
	 * Prepare to decompress in place.  The end mark is about 8.25 bytes long,
	 * and the worst case symbol is about 16.5 bytes long.  Therefore
	 * We need to reserve at least 25 bytes of slack here.  
	 * Currently I reserve 2048 bytes of just slack to be safe :)
	 * 2048 bytes easily falls within the BSS (the defualt stack is 4096 bytes)
	 * so we really are decompressing in place.
	 * 
	 * Hmm. I missed a trick.  In the very worst case (no compression)
	 * the encoded data is 9/8 the size as it started out so to be completely
	 * safe I need to be 1/8 of the uncompressed code size past the end.
	 * This will still fit compfortably into our bss in any conceivable scenario.
	 */
	__compressed_copy = __edata + __prefix_size  - __compressed_size +
		/* The amount to overflow _edata */
		MAX( ((__edata - __text + 7) / 8) , 2016 ) + 32; 
	__assert = ASSERT( ( __compressed_copy - __prefix_size ) < __ebss , "Cannot decompress in place" ) ; 

	_decompress = DEFINED(_decompress) ? _decompress : 0;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* Symbols used by the prefixes whose addresses are inconvinient 
	 * to compute, at runtime in the code.
	 */
	_image_basemem_size = DEFINED(_image_basemem_size)? _image_basemem_size : 65536;
	_image_basemem      = DEFINED(_image_basemem)?     _image_basemem : 65536;
	__prefix_real_to_prot          = __real_to_prot         + __prefix_size ;
	__prefix_prot_to_real          = __prot_to_real         + __prefix_size ;
	__prefix_image_basemem_size    = _image_basemem_size    + __prefix_size ;
	__prefix_image_basemem         = _image_basemem         + __prefix_size ;
	__prefix_rm_in_call            = __rm_in_call           + __prefix_size ;
	__prefix_in_call               = __in_call              + __prefix_size ;
	__prefix_rom                   = _rom                   + __prefix_size ;
	__prefix_rm_etherboot_location = _rm_etherboot_location + __prefix_size ;
	__prefix_stack_end             = __stack_end            + __prefix_size ;
}
