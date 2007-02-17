#
# Makefile for VBoxManage (the cli frontend).
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

PROGRAMS = VBoxManage

VBoxManage_TEMPLATE   = VBOXMAINCLIENT
VBoxManage_SOURCES    = \
	VBoxManage.cpp \
	VBoxInternalManage.cpp
ifdef VBOX_WITH_VRDP
VBoxManage_DEFS      += VBOX_VRDP
endif
ifdef VBOX_WITH_ALSA
VBoxManage_DEFS      += VBOX_WITH_ALSA
endif
VBoxManage_LIBS.win = \
	$(PATH_LIB)/VBoxDD$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/VBoxXML$(VBOX_SUFF_LIB)
VBoxManage_LIBS.linux = \
	$(PATH_BIN)/VBoxDD.so \
	$(PATH_BIN)/VBoxXML.so
VBoxManage_LIBS.darwin = \
	$(PATH_BIN)/VBoxDD$(VBOX_SUFF_DLL) \
	$(PATH_BIN)/VBoxXML$(VBOX_SUFF_DLL) \
	$(LIB_VMM) \
	$(LIB_REM)


include $(PATH_KBUILD)/footer.kmk

