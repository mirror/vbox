OUTPUT_FORMAT("pe-i386", "pe-i386", "pe-i386")
OUTPUT_ARCH(i386)

SECTIONS {
	 .prefix.udata : {
		      *(*)
	 }
}
