#
# Makefile for the storage device & driver testcases.
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

PROGRAMS        += vditool
vditool_TEMPLATE = VBOXR3EXE
vditool_SOURCES  = vditool.cpp
ifeq ($(BUILD_TARGET),win)
 vditool_LIBS    = $(PATH_LIB)/VBoxDDU$(VBOX_SUFF_LIB)
else
 vditool_LIBS    = $(PATH_BIN)/VBoxDDU$(VBOX_SUFF_DLL)
endif
vditool_LIBS    += $(LIB_RUNTIME)


ifdef VBOX_WITH_TESTCASES
 PROGRAMS       += tstVDI
 tstVDI_TEMPLATE = VBOXR3TSTEXE
 tstVDI_SOURCES  = tstVDI.cpp
 tstVDI_LIBS     = $(vditool_LIBS)
endif


include	$(PATH_KBUILD)/footer.kmk

