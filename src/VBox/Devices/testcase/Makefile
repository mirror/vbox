#
# Makefile for the Devices testcases.
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

DEPTH = ../../../..
include	$(PATH_KBUILD)/header.kmk

#
# Target lists.
#
PROGRAMS = tstDeviceStructSize tstDeviceStructSizeGC

#
# We setup one 'other' targets for executing the structure & alignment
# validation testcases. Perhaps a bit hackish, but extremely useful.
#
ifeq ($(BUILD_TARGET),$(BUILD_PLATFORM))
 ifeq ($(filter-out x86.x86 amd64.amd64 x86.amd64, $(BUILD_TARGET_ARCH).$(BUILD_PLATFORM_ARCH)),)
  OTHERS          += \
	$(PATH_TARGET)/tstDeviceStructSize.run
 endif
endif
OTHER_CLEAN     += \
	$(PATH_TARGET)/tstDeviceStructSizeGC.h \
	$(PATH_TARGET)/tstDeviceStructSize.run \

#
# The testcase generator.
#
tstDeviceStructSizeGC_TEMPLATE  = VBOXGCEXE
tstDeviceStructSizeGC_DEFS      =
ifdef VBOX_WITH_USB
tstDeviceStructSizeGC_DEFS     += VBOX_WITH_USB IN_USB_GC
endif
tstDeviceStructSizeGC_SOURCES   = tstDeviceStructSizeGC.cpp
tstDeviceStructSizeGC_INCS      = .. ../Bus

#
# The testcase it self.
#
tstDeviceStructSize_TEMPLATE = VBOXR3EXE
tstDeviceStructSize_DEFS     =
ifdef VBOX_WITH_USB
tstDeviceStructSize_DEFS    += VBOX_WITH_USB IN_USB_GC
endif
tstDeviceStructSize_INCS     = .. ../Bus $(PATH_TARGET)
tstDeviceStructSize_SOURCES  = tstDeviceStructSize.cpp
tstDeviceStructSize_LIBS.l4  = $(LIB_RUNTIME)
tstDeviceStructSize.cpp_DEPS = $(PATH_TARGET)/tstDeviceStructSizeGC.h


include	$(PATH_KBUILD)/footer.kmk


#
# Run rule for tstDeviceStructSize.
#

# 1. Dump selected structure in the VMMGC.gc debug info.
# 2. Generate a testcase from the dump
## future

# 1&2. Manually dump selected structures and members.
$(PATH_TARGET)/tstDeviceStructSizeGC.h: $(INSTARGET_tstDeviceStructSizeGC)
	$(call MSG_L1,Generating $<)
	$(QUIET)$(INSTARGET_tstDeviceStructSizeGC) > $@

# 3. run it.
$(PATH_TARGET)/tstDeviceStructSize.run: $(INSTARGET_tstDeviceStructSize)
	$(QUIET)$(RM) -f $@
	$^
	$(QUIET)$(APPEND) "$@" "done"

# alias for the struct test.
run-struct-tests: $(PATH_TARGET)/tstDeviceStructSize.run

