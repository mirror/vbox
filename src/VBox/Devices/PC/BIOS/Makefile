#
# PC BIOS Makefile
#

# Copyright (C) 2006 InnoTek Systemberatung GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#
# If you received this file as part of a commercial VirtualBox
# distribution, then only the terms of your commercial VirtualBox
# license agreement apply instead of the previous paragraph.
#

DEPTH = ../../../../../
include	$(PATH_KBUILD)/header.kmk

BLDPROGS    = biossums
LIBRARIES   = PcBiosBin PcDefBiosLogo

OTHERS      = $(PATH_TARGET)/bios.bin

OTHER_CLEAN = \
	$(PATH_TARGET)/_rombios_.c \
	$(PATH_TARGET)/rombios0.s \
	$(PATH_TARGET)/rombios1.s \
	$(PATH_TARGET)/bios.lst \
	$(PATH_TARGET)/bios.tmp \
	$(PATH_TARGET)/bios.bin \
	$(PATH_TARGET)/PcBiosBin.c \
	$(PATH_TARGET)/PcDefBiosLogo.c \
	$(PATH_TARGET)/bios-syms.h

TEMPLATE = VBOXBLDPROG
biossums_SOURCES    = biossums.c

PcBiosBin_TEMPLATE  = VBOXR3
PcBiosBin_DEFS      = IN_VBOXDD2
PcBiosBin_INCS      = ../..
PcBiosBin_SOURCES   = $(PATH_TARGET)/PcBiosBin.c
$(PATH_TARGET)/PcBiosBin.c_DEPS = $(PATH_TARGET)/bios-syms.h

PcDefBiosLogo_TEMPLATE  = VBOXR3
PcDefBiosLogo_INCS      = ../..
PcDefBiosLogo_SOURCES   = $(PATH_TARGET)/PcDefBiosLogo.c


include	$(PATH_KBUILD)/footer.kmk


#
# Rule for making the bios.
#
$(PATH_TARGET)/PcBiosBin.c: $(PATH_TARGET)/bios.bin $(VBOX_BIN2C)
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) -min 64 -max 256 -mask 0xffff -ascii -export PcBiosBinary $< $@

#
# Rule for making the bios logo.
#
$(PATH_TARGET)/PcDefBiosLogo.c: innotek_logo.bmp $(VBOX_BIN2C)
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) -max 64 PcDefBiosLogo $< $@

#
# Six steps to bios.bin and bios-syms.h
#

# 1. precompile rombios.c
$(PATH_TARGET)/_rombios_.c: rombios.c logo.c apmbios.S
	$(call MSG_L1,Precompiling $<,=> $@)
	$(QUIET)$(TOOL_$(VBOX_GCC_TOOL)_CC) -E -I$(PATH_TARGET) -I$(PATH_CURRENT)/.. -DBX_SMP_PROCESSORS=1 -DVBOX $(addprefix -D,$(DEFS) $(DEFS.$(BUILD_TYPE))) -P $< > $@

# 2. compile to intermediate asm file.
$(PATH_TARGET)/rombios0.s: $(PATH_TARGET)/_rombios_.c
	$(call MSG_L1,Compiling $<,=> $@)
	$(QUIET)$(VBOX_BCC) -o $@ -C-c -D__i86__ -0 -S $^

# 3. post process intermediate asm file.
$(PATH_TARGET)/rombios1.s: $(PATH_TARGET)/rombios0.s
	$(call MSG_L1,Adjusting BCC Assembly $<,=> $@)
	$(QUIET)$(SED) -e 's/^\.text//' -e 's/^\.data//' $^ > $@

# 4. assemble the intermediate asm file. (also creates a listing rombios.lst)
$(PATH_TARGET)/bios.tmp $(PATH_TARGET)/bios.lst: $(PATH_TARGET)/rombios1.s
	$(call MSG_L1,Compiling $<,=> $@)
	$(QUIET)$(VBOX_AS86) -b $(PATH_TARGET)/bios.tmp -u- -w- -g -0 -j -O -l $(PATH_TARGET)/bios.lst - < $^

# 5. calculate checksums for the final BIOS image.
$(PATH_TARGET)/bios.bin: $(PATH_TARGET)/bios.tmp $(TARGET_biossums)
	$(call MSG_L1,biossums $@)
	$(QUIET)$(CP) -f $(PATH_TARGET)/bios.tmp $(PATH_TARGET)/bios.bin
	$(QUIET)$(TARGET_biossums) $(PATH_TARGET)/bios.bin

# 6. create the symbol table.
$(PATH_TARGET)/bios-syms.h: $(PATH_TARGET)/bios.lst Makefile
	$(call MSG_L1,symtable $<,=> $@)
	$(QUIET)$(SED) \
		-e  '/^[0-9][0-9][0-9][0-9][0-9]                       000[0-9A-F]*            [[:space:]]*[a-zA-Z0-9_]*:/!d' \
		-e 's/^[0-9][0-9][0-9][0-9][0-9]                       \(000[0-9A-F]*\)            [[:space:]]*\([a-zA-Z0-9_]*\):.*$$/    DEFSYM(0x\1, "\2"),/' \
		$(PATH_TARGET)/bios.lst > $@

