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

DEPTH = ../..
include $(PATH_KBUILD)/header.kmk

SUBDIRS_BLDPROGS = Devices

SUBDIRS = \
	Runtime \
	HostDrivers \
	VMM \
	Devices \
	Disassembler \
	Debugger 

ifneq ($(wildcard HostServices),)
SUBDIRS += HostServices
endif

ifdef VBOX_WITH_MAIN
SUBDIRS += Main
endif

ifdef VBOX_WITH_VRDP
SUBDIRS += RDP
endif

ifneq ($(VBOX_WITH_WIN32_ADDITIONS)$(VBOX_WITH_LINUX_ADDITIONS),)
SUBDIRS += Additions
endif

SUBDIRS += Frontends

# This must be the last one.
ifdef VBOX_WITH_INSTALLER
SUBDIRS += Installer
endif

include $(PATH_KBUILD)/footer.kmk

