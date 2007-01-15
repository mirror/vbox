#
# Makefile for the src directory.
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

DEPTH  = ..
include $(PATH_KBUILD)/header.kmk

# build Runtime first when IPC logging is on because it depends on Runtime
ifdef VBOX_IPC_RELEASE_LOG
 SUBDIRS = VBox/Runtime
else ifneq ($(BUILD_TYPE),release)
 SUBDIRS = VBox/Runtime
else
 SUBDIRS =
endif

SUBDIRS += bldprogs libs VBox
ifeq ($(filter-out darwin.x86 darwin.amd64 win.amd64 linux.amd64,$(BUILD_TARGET).$(BUILD_TARGET_ARCH)),)
 SUBDIRS += recompiler/new
else
 SUBDIRS += recompiler
endif
ifneq ($(wildcard apps),)
 SUBDIRS += apps
endif
ifdef VBOX_WITH_TESTSUITE
 SUBDIRS +=  tests
endif

include $(PATH_KBUILD)/footer.kmk

