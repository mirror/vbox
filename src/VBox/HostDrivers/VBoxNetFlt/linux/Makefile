# $Id$
## @file
# Makefile for the VirtualBox Linux Host Network Filter Driver.
#

#
# Copyright (C) 2006-2019 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

# Find the directory of this makefile.
VBOXNETFLT_DIR := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

# Linux kbuild sets this to our source directory if we are called from there
obj ?= $(CURDIR)
include $(obj)/Makefile-header.gmk

VBOXMOD_NAME = vboxnetflt
VBOXMOD_OBJS = \
	linux/VBoxNetFlt-linux.o \
	VBoxNetFlt.o \
	SUPR0IdcClient.o \
	SUPR0IdcClientComponent.o \
	linux/SUPR0IdcClient-linux.o
ifeq ($(BUILD_TARGET_ARCH),x86)
VBOXMOD_OBJS += \
	math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
	math/gcc/udivmoddi4.o \
	math/gcc/divdi3.o \
	math/gcc/umoddi3.o
endif
VBOXMOD_INCL = \
	-I$(VBOXNETFLT_DIR) \
	-I$(VBOXNETFLT_DIR)include \
	-I$(VBOXNETFLT_DIR)r0drv/linux
VBOXMOD_DEFS = \
	RT_OS_LINUX \
	IN_RING0 \
	IN_RT_R0 \
	IN_SUP_R0 \
	VBOX \
	RT_WITH_VBOX \
	VBOX_WITH_HARDENING \
	VBOX_WITH_64_BITS_GUESTS # <-- must be consistent with Config.kmk!
VBOXMOD_CFLAGS = -include $(VBOXNETFLT_DIR)include/VBox/SUPDrvMangling.h -fno-pie -Wno-declaration-after-statement

include $(obj)/Makefile-footer.gmk

