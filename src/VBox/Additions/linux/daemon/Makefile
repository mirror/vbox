#
# Makefile for the VirtualBox Linux Guest Addition Linux Daemon.
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
#

DEPTH = ../../../../..
include	$(PATH_KBUILD)/header.kmk


PROGRAMS = vboxadd-timesync

vboxadd-timesync_TEMPLATE = VBOXLNX32GUESTR3EXE
vboxadd-timesync_SOURCES  = \
    vboxadd_timesync.c supc++.c
vboxadd-timesync_LIBS     = \
    $(PATH_LIB)/VBoxGuestLibLinux.a \
    $(PATH_LIB)/RuntimeLnx32GuestR3.a
vboxadd-timesync_DEFS     = _GNU_SOURCE

# use gcc not g++ as linker to not depend on libstdc++
TOOL_GCC3_LD              = $(TOOL_$(VBOX_GCC32_TOOL)_CC)

# prevent generation of cross reference table (broken with some binutils)
## @todo r=bird: Why is this only a problem here and not for the 50 other binaries?
TOOL_GCC3_LD_MAP          = -Wl,-Map -Wl,$(1)

include	$(PATH_KBUILD)/footer.kmk

