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

PROGRAMS = vditool
ifdef VBOX_WITH_TESTCASES
PROGRAMS += tstVDI
endif

TEMPLATE        = VBOXR3EXE
LIBS            = $(LIB_RUNTIME)
LIBS.darwin     = $(PATH_BIN)/VBoxDD.dylib
LIBS.linux      = $(PATH_BIN)/VBoxDD.so
LIBS.l4         = $(PATH_BIN)/VBoxDD.s.so
LIBS.os2        = $(PATH_BIN)/VBoxDD.dll
LIBS.win        = $(PATH_LIB)/VBoxDD.lib

tstVDI_SOURCES  = VHDDtest.cpp

vditool_SOURCES = vhddtool.cpp

include	$(PATH_KBUILD)/footer.kmk

