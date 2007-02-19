#
# Makefile for the VirtualBox Linux Guest Additions Kernel Module.
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

INSTALLS = vboxmod-bin vboxmod-sh
ifdef VBOX_WITH_LINUX_ADDITIONS_32BIT_R0
SYSMODS = vboxadd
endif

vboxmod-bin_INST    = bin/additions/src/vboxadd/
vboxmod-bin_MODE    = a+r,u+w
vboxmod-bin_SOURCES = \
	$(PATH_ROOT)/include/iprt/alloc.h=>include/iprt/alloc.h \
	$(PATH_ROOT)/include/iprt/asm.h=>include/iprt/asm.h \
	$(PATH_ROOT)/include/iprt/assert.h=>include/iprt/assert.h \
	$(PATH_ROOT)/include/iprt/cdefs.h=>include/iprt/cdefs.h \
	$(PATH_ROOT)/include/iprt/err.h=>include/iprt/err.h \
	$(PATH_ROOT)/include/iprt/log.h=>include/iprt/log.h \
	$(PATH_ROOT)/include/iprt/mem.h=>include/iprt/mem.h \
	$(PATH_ROOT)/include/iprt/param.h=>include/iprt/param.h \
	$(PATH_ROOT)/include/iprt/semaphore.h=>include/iprt/semaphore.h \
	$(PATH_ROOT)/include/iprt/stdarg.h=>include/iprt/stdarg.h \
	$(PATH_ROOT)/include/iprt/stdint.h=>include/iprt/stdint.h \
	$(PATH_ROOT)/include/iprt/string.h=>include/iprt/string.h \
	$(PATH_ROOT)/include/iprt/types.h=>include/iprt/types.h \
	$(PATH_ROOT)/include/VBox/cdefs.h=>include/VBox/cdefs.h \
	$(PATH_ROOT)/include/VBox/err.h=>include/VBox/err.h \
	$(PATH_ROOT)/include/VBox/log.h=>include/VBox/log.h \
	$(PATH_ROOT)/include/VBox/types.h=>include/VBox/types.h \
	$(PATH_ROOT)/include/VBox/ostypes.h=>include/VBox/ostypes.h \
	$(PATH_ROOT)/include/VBox/VBoxGuest.h=>include/VBox/VBoxGuest.h \
	$(PATH_ROOT)/include/VBox/VBoxGuestLib.h=>include/VBox/VBoxGuestLib.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/GenericRequest.cpp=>GenericRequest.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/HGCM.cpp=>HGCM.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/HGCMInternal.cpp=>HGCMInternal.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/Init.cpp=>Init.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/PhysHeap.cpp=>PhysHeap.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/PhysHeap.h=>PhysHeap.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/SysHlp.cpp=>SysHlp.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/SysHlp.h=>SysHlp.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/VBGLInternal.h=>VBGLInternal.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/VBoxCalls.c=>VBoxCalls.c \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/VBoxCalls.h=>VBoxCalls.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/VBoxGuestLog.h=>VBoxGuestLog.h \
	$(PATH_ROOT)/src/VBox/Additions/common/VBoxGuestLib/VMMDev.cpp=>VMMDev.c \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/alloc-r0drv.cpp=>r0drv/alloc-r0drv.c \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/alloc-r0drv.h=>r0drv/alloc-r0drv.h \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/alloc-r0drv-linux.c=>r0drv/linux/alloc-r0drv-linux.c \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/semaphore-r0drv-linux.c=>r0drv/linux/semaphore-r0drv-linux.c \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/string.h=>r0drv/linux/string.h \
	$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/the-linux-kernel.h=>r0drv/linux/the-linux-kernel.h \
	vboxmod.c=>vboxmod.c \
	cmc.c=>cmc.c \
	vboxmod.h=>vboxmod.h \
	waitcompat.h=>waitcompat.h \
	Makefile.module=>Makefile

vboxmod-sh_INST    = bin/additions/src/vboxadd/
vboxmod-sh_MODE    = a+rx,u+w
vboxmod-sh_SOURCES = \
	$(PATH_ROOT)/src/VBox/Additions/linux/installer/build_in_tmp=>build_in_tmp

#
# The module.
#
vboxadd_TEMPLATE = VBOXLNX32GUESTR0
vboxadd_DEFS     = KBUILD_MODNAME=KBUILD_STR\(vboxadd\) KBUILD_BASENAME=KBUILD_STR\(vboxadd\) MODULE IN_RING0 IN_RT_R0 VBGL_VBOXGUEST EXPORT_SYMTAB VBGL_HGCM VBOX_HGCM
vboxadd_LIBS     = $(PATH_LIB)/RuntimeLnx32GuestR0.a
vboxadd_SOURCES  = vboxmod.c cmc.c
vboxadd_INCS     = $(PATH_ROOT)/src/VBox/Runtime/r0drv/linux
vboxadd_NOINST   = 1

include	$(PATH_KBUILD)/footer.kmk

