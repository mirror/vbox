#
# Makefile for the Windows NT++ Guest Additions.
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
#

DEPTH = ../../../..
include $(PATH_KBUILD)/header.kmk

SUBDIRS = \
	Graphics \
	i8042prt \
	MouseFilter \
	VBoxGuest \
	VBoxService \
	VBoxGINA \
	VBoxControl
ifdef VBOX_WITH_WIN32_ADDITIONS_SHAREDFOLDERS
SUBDIRS += \
	SharedFolders
endif

ifneq ($(wildcard Installer),)
SUBDIRS += \
	Installer
endif

include $(PATH_KBUILD)/footer.kmk

