#
# Makefile for the VBox Linux Additions X.org graphics driver.
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

LIBRARIES   = vboxvideo vboxvideo_so_70 vboxvideo_so_71
OTHERS      = \
	$(PATH_BIN)/additions/vboxvideo_drv.o \
	$(PATH_BIN)/additions/vboxvideo_drv_70.so \
	$(PATH_BIN)/additions/vboxvideo_drv_71.so
OTHER_CLEAN = $(OTHERS)

vboxvideo_NOINST   = 1
vboxvideo_TEMPLATE = VBOXLNX32GUESTR3
vboxvideo_SOURCES  = \
	vboxvideo.c \
	vboxutils.c

vboxvideo_so_70_NOINST   = 1
vboxvideo_so_70_TEMPLATE = VBOXLNX32GUESTR3
vboxvideo_so_70_SOURCES  = \
	$(vboxvideo_SOURCES)

vboxvideo_so_71_NOINST   = 1
vboxvideo_so_71_TEMPLATE = VBOXLNX32GUESTR3
vboxvideo_so_71_SOURCES  = \
	$(vboxvideo_SOURCES)

ARGB_CURSORS=1
ifdef ARGB_CURSORS
vboxvideo_INCS = \
	../x11include/4.3/include \
	../x11include/4.3/include/extensions \
	../x11include/4.3/include/fonts \
	../x11include/4.3/programs/Xserver/afb \
	../x11include/4.3/programs/Xserver/include \
	../x11include/4.3/programs/Xserver/fb \
	../x11include/4.3/programs/Xserver/hw/xfree86 \
	../x11include/4.3/programs/Xserver/hw/xfree86/common \
	../x11include/4.3/programs/Xserver/hw/xfree86/ddc \
	../x11include/4.3/programs/Xserver/hw/xfree86/int10 \
	../x11include/4.3/programs/Xserver/hw/xfree86/i2c \
	../x11include/4.3/programs/Xserver/hw/xfree86/os-support \
	../x11include/4.3/programs/Xserver/hw/xfree86/vbe \
	../x11include/4.3/programs/Xserver/hw/xfree86/os-support/bus \
	../x11include/4.3/programs/Xserver/hw/xfree86/rac \
	../x11include/4.3/programs/Xserver/hw/xfree86/ramdac \
	../x11include/4.3/programs/Xserver/hw/xfree86/shadowfb \
	../x11include/4.3/programs/Xserver/hw/xfree86/vgahw \
	../x11include/4.3/programs/Xserver/hw/xfree86/xf1bpp \
	../x11include/4.3/programs/Xserver/hw/xfree86/xf24_32bpp \
	../x11include/4.3/programs/Xserver/hw/xfree86/xf4bpp \
	../x11include/4.3/programs/Xserver/mfb \
	../x11include/4.3/programs/Xserver/mi \
	../x11include/4.3/programs/Xserver/miext/shadow \
	../x11include/4.3/programs/Xserver/render \
	../x11include/4.3/programs/Xserver/Xext
else
vboxvideo_INCS = \
	../x11include/4.2/exports/include/X11 \
	../x11include/4.2/include \
	../x11include/4.2/include/extensions \
	../x11include/4.2/include/fonts \
	../x11include/4.2/programs/Xserver/afb \
	../x11include/4.2/programs/Xserver/include \
	../x11include/4.2/programs/Xserver/fb \
	../x11include/4.2/programs/Xserver/hw/xfree86/common \
	../x11include/4.2/programs/Xserver/hw/xfree86/ddc \
	../x11include/4.2/programs/Xserver/hw/xfree86/int10 \
	../x11include/4.2/programs/Xserver/hw/xfree86/i2c \
	../x11include/4.2/programs/Xserver/hw/xfree86/os-support \
	../x11include/4.2/programs/Xserver/hw/xfree86/os-support/bus \
	../x11include/4.2/programs/Xserver/hw/xfree86/os-support/vbe \
	../x11include/4.2/programs/Xserver/hw/xfree86/rac \
	../x11include/4.2/programs/Xserver/hw/xfree86/ramdac \
	../x11include/4.2/programs/Xserver/hw/xfree86/shadowfb \
	../x11include/4.2/programs/Xserver/hw/xfree86/vgahw \
	../x11include/4.2/programs/Xserver/hw/xfree86/xf1bpp \
	../x11include/4.2/programs/Xserver/hw/xfree86/xf24_32bpp \
	../x11include/4.2/programs/Xserver/hw/xfree86/xf4bpp \
	../x11include/4.2/programs/Xserver/mfb \
	../x11include/4.2/programs/Xserver/mi \
	../x11include/4.2/programs/Xserver/miext/shadow \
	../x11include/4.2/programs/Xserver/render \
	../x11include/4.2/programs/Xserver/Xext
endif

vboxvideo_so_70_INCS = \
	../x11include/7.0 \
	../x11include/7.0/X11 \
	../x11include/7.0/xorg

vboxvideo_so_71_INCS = \
	../x11include/7.1 \
	../x11include/7.1/X11 \
	../x11include/7.1/xorg

vboxvideo_DEFS = \
	linux __i386__ _POSIX_C_SOURCE=199309L _POSIX_SOURCE _XOPEN_SOURCE \
	_BSD_SOURCE _SVID_SOURCE _GNU_SOURCE SHAPE XINPUT XKB LBX XAPPGROUP \
	XCSECURITY TOGCUP XF86BIGFONT DPMSExtension PIXPRIV PANORAMIX RENDER \
	GCCUSESGAS AVOID_GLYPHBLT PIXPRIV SINGLEDEPTH XFreeXDGA XvExtension \
	XFree86LOADER XFree86Server XF86VIDMODE XvMCExtension SMART_SCHEDULE \
	BUILDDEBUG X_BYTE_ORDER=X_LITTLE_ENDIAN DNDEBUG FUNCPROTO=15 NARROWPROTO \
	IN_MODULE XFree86Module \
	IN_RING3 VBOX

vboxvideo_CFLAGS.linux := \
	-fno-merge-constants -Wno-conversion -Wno-unused-parameter \
	$(VBOX_GCC_Wno-variadic-macros)

vboxvideo_so_70_DEFS := \
	XFree86Server IN_MODULE XFree86Module XFree86LOADER \
	IN_RING3 VBOX XORG_7X PIC RENDER=1
vboxvideo_so_71_DEFS := $(vboxvideo_so_70_DEFS)

vboxvideo_so_70_CFLAGS.linux := \
	-fPIC -Wno-conversion -Wno-unused-parameter \
	$(VBOX_GCC_Wno-variadic-macros)
vboxvideo_so_71_CFLAGS.linux := $(vboxvideo_so_70_CFLAGS.linux)

include	$(PATH_KBUILD)/footer.kmk

$(PATH_BIN)/additions/vboxvideo_drv.o: vboxvideo.c vboxutils.c
	$(call MSG_L1,Linking $@)
	$(QUIET)$(MKDIR) -p $(PATH_BIN)/additions
	$(QUIET)ld -r -o $@ $(PATH_vboxvideo)/vboxvideo.o $(PATH_vboxvideo)/vboxutils.o

$(PATH_BIN)/additions/vboxvideo_drv_70.so: vboxvideo.c vboxutils.c
	$(call MSG_L1,Linking $@)
	$(QUIET)$(MKDIR) -p $(PATH_BIN)/additions
	$(QUIET)$(TOOL_$(VBOX_GCC_TOOL)_CC) -shared -s -o $@ $(PATH_vboxvideo_so_70)/vboxvideo.o $(PATH_vboxvideo_so_70)/vboxutils.o

$(PATH_BIN)/additions/vboxvideo_drv_71.so: vboxvideo.c vboxutils.c
	$(call MSG_L1,Linking $@)
	$(QUIET)$(MKDIR) -p $(PATH_BIN)/additions
	$(QUIET)$(TOOL_$(VBOX_GCC_TOOL)_CC) -shared -s -o $@ $(PATH_vboxvideo_so_71)/vboxvideo.o $(PATH_vboxvideo_so_71)/vboxutils.o

