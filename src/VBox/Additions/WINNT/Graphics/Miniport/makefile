#
# Makefile for VBoxVideo.
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

SYSMODS = VBoxVideo

# This has to run on NT4!
VBoxVideo_TEMPLATE    = VBOXW32GUESTR0
VBoxVideo_SDKS        = W2K3DDKX86 WINPSDKINCS
VBoxVideo_DEFS        = LOG_TO_BACKDOOR
#VBoxVideo_DEFS        += LOG_ENABLED
VBoxVideo_LDFLAGS     = /Entry:DriverEntry@8
VBoxVideo_SOURCES     = \
	VBoxVideo.cpp \
	Helper.cpp \
	VBoxVideo.def \
	VBoxVideo.rc
VBoxVideo_LIBS        = \
	$(PATH_SDK_W2K3DDKX86_LIB)/videoprt.lib \
	$(PATH_SDK_W2K3DDKX86_LIB)/int64.lib \
	$(PATH_SDK_W2K3DDKX86_LIB)/ntoskrnl.lib \
	$(PATH_SDK_W2K3DDKX86_LIB)/hal.lib \
	$(PATH_LIB)/additions/VBoxGuestLib$(VBOXW32GUEST_SUFF_LIB) \
	$(PATH_LIB)/additions/RuntimeW32NT4GuestR0$(VBOXW32GUEST_SUFF_LIB)

include	$(PATH_KBUILD)/footer.kmk

