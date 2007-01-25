#
# VGA BIOS Makefile
#

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

DEPTH = ../../../../..
include	$(PATH_KBUILD)/header.kmk

BLDPROGS    = biossums
LIBRARIES   = VgaBiosBin
OTHERS      = $(PATH_TARGET)/vgabios.bin
OTHER_CLEAN = \
	$(PATH_TARGET)/_vgabios_.c \
	$(PATH_TARGET)/vgabios0.s \
	$(PATH_TARGET)/vgabios1.s \
	$(PATH_TARGET)/vgabios.tmp \
	$(PATH_TARGET)/vgabios.lst \
	$(PATH_TARGET)/vgabios.bin \
	$(PATH_TARGET)/VgaBiosBin.c \
	\
	$(PATH_TARGET)/_vgabios-cirrus_.c \
	$(PATH_TARGET)/vgabios-cirrus0.s \
	$(PATH_TARGET)/vgabios-cirrus1.s \
	$(PATH_TARGET)/vgabios-cirrus.tmp \
	$(PATH_TARGET)/vgabios-cirrus.lst \
	$(PATH_TARGET)/vgabios-cirrus.bin

biossums_TEMPLATE   = VBOXBLDPROG
biossums_SOURCES    = biossums.c

VgaBiosBin_TEMPLATE = VBOXR3
VgaBiosBin_DEFS     = IN_VBOXDD2
VgaBiosBin_INCS     = ../..
VgaBiosBin_SOURCES  = $(PATH_TARGET)/VgaBiosBin.c


include	$(PATH_KBUILD)/footer.kmk


#
# Rule for making the bios.
#
$(PATH_TARGET)/VgaBiosBin.c: $(PATH_TARGET)/vgabios.bin $(VBOX_BIN2C)
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) -min 32 -max 64 -mask 0x0fff -ascii -export VgaBiosBinary $< $@

#
# vgabios
#

# 1. precompile
$(PATH_TARGET)/_vgabios_.c: vgabios.c vgabios.h vbe.h vgatables.h vgafonts.h vbe.c clext.c
	$(call MSG_L1,Precompiling $<,=> $@)
	$(QUIET)$(TOOL_$(VBOX_GCC_TOOL)_CC) -DVBOX -DVBOX_VERSION=\"$(VBOX_VERSION)\" -E -P $< -DVBE -DVGABIOS_DATE="''" $(addprefix -D,$(DEFS) $(DEFS.$(BUILD_TYPE))) > $@

# 2. compile
$(PATH_TARGET)/vgabios0.s: $(PATH_TARGET)/_vgabios_.c
	$(call MSG_L1,Compiling $<,=> $@)
	$(QUIET)$(VBOX_BCC) -o $@ -C-c -D__i86__ -0 -S $^

# 3. preprocess intermediate assembly.
$(PATH_TARGET)/vgabios1.s: $(PATH_TARGET)/vgabios0.s
	$(call Adjusting BCC Assembly $<,=> $@)
	$(QUIET)$(SED) -e 's/^\.text//' -e 's/^\.data//' $^ > $@

# 4. assemble
$(PATH_TARGET)/vgabios.tmp $(PATH_TARGET)/vgabios.lst: $(PATH_TARGET)/vgabios1.s
	$(call MSG_L1,Compiling $<,=> $@)
	$(QUIET)$(VBOX_AS86) -b $(PATH_TARGET)/vgabios.tmp -u- -w- -g -0 -j -O -l $(PATH_TARGET)/vgabios.lst < $^

# 5. checksums
$(PATH_TARGET)/vgabios.bin: $(PATH_TARGET)/vgabios.tmp $(TARGET_biossums)
	$(call MSG_L1,biossums $@)
	$(QUIET)$(CP) -f $(PATH_TARGET)/vgabios.tmp $@
	$(QUIET)$(TARGET_biossums) $@

