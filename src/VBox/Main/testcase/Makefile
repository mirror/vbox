#
# Makefile for the VBox API testcases.
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
include $(PATH_KBUILD)/header.kmk

ifdef VBOX_WITH_TESTCASES
 PROGRAMS       = tstAPI
 PROGRAMS.linux = tstVBoxAPILinux
 PROGRAMS.win   = tstVBoxAPIWin
endif # VBOX_WITH_TESTCASES

INSTALLS = samples

#
# The samples
#
samples_INST = $(INST_SDK_SAMPLES)/API/
samples_MODE = a+r,u+w
ifeq ($(BUILD_TARGET),win)
 samples_SOURCES = tstVBoxAPIWin.cpp
else
 samples_SOURCES = tstVBoxAPILinux.cpp makefile.tstVBoxAPILinux
 ifdef VBOX_WITH_TESTCASES
 samples_SYMLINKS = tstVBoxAPILinux=>../../../tstVBoxAPILinux
 endif
endif

#
# tstAPI
#
tstAPI_TEMPLATE = VBOXMAINCLIENT
tstAPI_SOURCES  = tstAPI.cpp

#
# tstVBoxAPILinux
#
# We only build the testcase here to make sure it builds.
# It comes with a custom makefile which should be tested as well!
#
tstVBoxAPILinux_TEMPLATE = VBOXR3
tstVBoxAPILinux_SOURCES  = tstVBoxAPILinux.cpp
tstVBoxAPILinux_CXXFLAGS = -Wno-non-virtual-dtor
tstVBoxAPILinux_INCS     = \
	$(VBOX_XPCOM_INCS) \
	$(PATH_BIN)/sdk/include
tstVBoxAPILinux_LIBPATH  = $(LIBPATH_XPCOM)
tstVBoxAPILinux_LIBS     = \
	$(PATH_BIN)/VBoxXPCOM.so \
	$(PATH_LIB)/VBoxXPCOMGlue$(VBOX_SUFF_LIB)

#
# tstVBoxAPIWin
#
tstVBoxAPIWin_TEMPLATE = VBOXMAINCLIENT
tstVBoxAPIWin_SOURCES  = \
	tstVBoxAPIWin.cpp \
	$(PATH_BIN)/sdk/lib/VirtualBox_i.c

# generate rules.
include $(PATH_KBUILD)/footer.kmk

