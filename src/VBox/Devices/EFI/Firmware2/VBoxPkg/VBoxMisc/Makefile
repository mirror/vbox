# $Id$
#* @file
# Makefile - assembling the iso image for experimenting with EFI.
#/
#
#
# Copyright (C) 2009-2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
#/

#
# Here we add modules to be burn on iso-image 
#
SOURCES += VBoxInterceptorDxe.efi
SOURCES += VBoxMimicryDxe.efi

ifeq ($(BUILD_ARCH), IA32)
ARCH_SUFFIX=
else
ARCH_SUFFIX=$(BUILD_ARCH)
endif

BUILD_ARCH ?= IA32
BUILD_DIR=$(WORKSPACE)/Build/VBoxPkg$(ARCH_SUFFIX)/DEBUG_UNIXGCC/$(BUILD_ARCH)

ISO_DIR=efi-app
FILES_TO_ISO=$(foreach file, $(SOURCES), $(addprefix $(ISO_DIR)/, $(file)))

DEST=efi-app.iso
all:${DEST}

$(DEST):$(FILES_TO_ISO)
	mkisofs -R -o $@ $(basename $@)

$(ISO_DIR)/%.efi:$(BUILD_DIR)/%.efi
	cp $< $@

clean:
	$(RM) -rf ${DEST} $(FILES_TO_ISO)
