#
# Makefile for VBoxDisp.dll.
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

DEPTH = ../../../../../..
include	$(PATH_KBUILD)/header.kmk

SYSMODS = VBoxDisp

VBoxDisp_TEMPLATE    = VBOXW32GUESTR0
VBoxDisp_SDKS        = W2K3DDKX86 WINPSDKINCS
VBoxDisp_SYSSUFF     = .dll
VBoxDisp_DEFS        = _WIN32_WINNT=0x0501 LOG_TO_BACKDOOR
#VBoxDisp_DEFS        += LOG_ENABLED
#VBoxDisp_DEFS        += STAT_sunlover
VBoxDisp_SOURCES     = \
	color.c \
	debug.c \
	enable.c \
	palette.c \
	pointer.c \
	screen.c \
	drv.c \
	vbva.c \
	vrdp.c \
	vrdpbmp.c \
	vbox.c \
	VBoxDisp.def \
	VBoxDisp.rc
VBoxDisp_LDFLAGS     = /Entry:DrvEnableDriver@12
VBoxDisp_LIBS        = \
	$(PATH_SDK_W2K3DDKX86_LIB)/int64.lib \
	$(PATH_SDK_W2K3DDKX86_LIB)/win32k.lib \
	$(PATH_LIB)/additions/RuntimeW32GuestR0$(VBOXW32GUEST_SUFF_LIB)
ifndef VBOX_USE_VCC80
VBoxDisp_LIBS       += \
	$(PATH_TOOL_$(VBOX_VCC_TOOL)_LIB)/libcmt.lib 
endif


include	$(PATH_KBUILD)/footer.kmk

