#
# Makefile for the VBox Host drivers base directory.
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

DEPTH = ../../..
include $(PATH_KBUILD)/subheader.kmk

# Include sub-makefile.
include $(VBOX_PATH_HOSTDRIVERS)/Support/Makefile
ifeq ($(BUILD_TARGET),win)
 include $(VBOX_PATH_HOSTDRIVERS)/VBoxTAP/Makefile
 ifdef VBOX_WITH_USB
  include $(VBOX_PATH_HOSTDRIVERS)/VBoxUSB/Makefile.kmk
 endif
endif

# Let kBuild generate the rules.
include $(PATH_KBUILD)/subfooter.kmk

