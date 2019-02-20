# $Id$
## @file
# Makefile for the VirtualBox Linux Host PCI Driver.
#

#
# Copyright (C) 2011-2019 Oracle Corporation
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
VBOXPCI_DIR := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

# Linux kbuild sets this to our source directory if we are called from there
obj ?= $(CURDIR)
include $(obj)/Makefile-header.gmk

# override is required by the Debian guys
MOD_NAME = vboxpci
MOD_OBJS = \
	linux/VBoxPci-linux.o \
	VBoxPci.o \
	SUPR0IdcClient.o \
	SUPR0IdcClientComponent.o \
	linux/SUPR0IdcClient-linux.o
ifeq ($(BUILD_TARGET_ARCH),x86)
MOD_OBJS += \
	math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
	math/gcc/udivmoddi4.o \
	math/gcc/divdi3.o \
	math/gcc/umoddi3.o
endif
MOD_INCL = \
	-I$(VBOXPCI_DIR) \
	-I$(VBOXPCI_DIR)include \
	-I$(VBOXPCI_DIR)r0drv/linux
MOD_DEFS = \
	-DRT_OS_LINUX \
	-DIN_RING0 \
	-DIN_RT_R0 \
	-DIN_SUP_R0 \
	-DVBOX \
	-DRT_WITH_VBOX \
	-DVBOX_WITH_HARDENING
ifeq ($(BUILD_TARGET_ARCH),amd64)
MOD_DEFS += -DRT_ARCH_AMD64
else
MOD_DEFS += -DRT_ARCH_X86
endif
MOD_CFLAGS = -include $(VBOXPCI_DIR)include/VBox/SUPDrvMangling.h -fno-pie

include $(obj)/Makefile-footer.gmk
