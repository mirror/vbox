#
# Sub-Makefile for the Windows Network Driver.
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

DEPTH ?= ../../../..
SUB_DEPTH = ..
include	$(PATH_KBUILD)/subheader.kmk

SYSMODS.win += VBoxTAP
INSTALLS.win += VBoxTAP-inf


#
# VBoxTap
#
VBoxTAP_TEMPLATE    = VBOXR0DRV
VBoxTAP_SDKS        = W2K3DDK WINPSDKINCS
VBoxTAP_LDFLAGS     = -Entry:DriverEntry@8
VBoxTAP_SOURCES     = \
	$(VBOX_PATH_VBOXTAP)/tapdrvr.c \
	$(VBOX_PATH_VBOXTAP)/VBoxTAP.rc
VBoxTAP_LIBS        = \
	$(PATH_LIB)/RuntimeR0$(VBOX_SUFF_LIB) \
	$(PATH_SDK_W2K3DDK_LIB)/ntoskrnl.lib \
	$(PATH_SDK_W2K3DDK_LIB)/hal.lib \
	$(PATH_SDK_W2K3DDK_LIB)/ndis.lib \
	$(PATH_SDK_W2K3DDK_LIB)/ntstrsafe.lib \
	$(PATH_SDK_W2K3DDK_LIB)/BufferOverflowK.lib

#
# Install the .inf.
#
VBoxTAP-inf_INST = bin/
VBoxTAP-inf_MODE = a+r,u+w
VBoxTAP-inf_SOURCES = $(VBOX_PATH_VBOXTAP)/VBoxTAP.inf


# generate rules
include	$(PATH_KBUILD)/subfooter.kmk

