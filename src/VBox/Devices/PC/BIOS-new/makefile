# Compiler flags:
#  -q   no banner
#  -wx  max warnings
#  -0   generate 8086 code
#  -ms  force small model (default)
#  -s   remove runtime stack checks (required)
#  -os  optimize for size
#  -zu  assume that SS != DS (required)
#  -ecc default to __cdecl calling convention (bad idea)
#
!ifdef __UNIX__
QUOTE=\"
!else
QUOTE="
!endif
CFLAGS = -q -0 -wx -zu -s -oas -d1 -ms
DEFS   = -DVBOX -DVBOX_LANBOOT_SEG=0xE200 -DVBOX_VERSION_STRING=$(QUOTE)0.9$(QUOTE) &
 -DVBOX_WITH_SCSI

AFLAGS = -q -0 -wx

INCLS  = -I$(QUOTE)../../../../../include$(QUOTE)

.c.obj : .autodepend
	wcc -fo=.obj $(CFLAGS) $(DEFS) $<

.asm.obj : .autodepend
	wasm -fo=.obj $(AFLAGS) $(DEFS) $<

OBJS = 	bios.obj print.obj ata.obj floppy.obj eltorito.obj boot.obj &
	keyboard.obj disk.obj serial.obj system.obj timepci.obj &
	ps2mouse.obj parallel.obj logo.obj scsi.obj pcibio32.obj orgs.obj

vbxbios.rom : vbxbios.bin
	biossums $< $@

vbxbios.bin : $(OBJS) $(__MAKEFILES__)
	wlink name $@ system dos debug all option quiet &
	 option nofarcalls, map, verbose, statics, symfile &
	 output raw offset=0xF0000 order &
	 clname DATA segaddr=0xF000 segment _DATA &
	 clname CODE &
	 segment _TEXT segaddr=0xF000 offset=0x1400 &
	 segment BIOS32 segaddr=0xF000 offset=0xDE00 &
	 segment BIOSSEG segaddr=0xF000 offset=0xE000 &
	 file { $(OBJS) } &
	 library clibs.lib &
	 disable 1014, 1023, 2120

logo.obj : logo.c
	wcc -fo=.obj $(INCLS) $(CFLAGS) $(DEFS) -DVBOX_PC_BIOS $<

clean : .symbolic
	@rm -f *.obj *.err
	@rm -f vbxbios.bin vbxbios.rom vbxbios.map
