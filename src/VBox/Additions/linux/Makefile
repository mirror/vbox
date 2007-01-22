#
# Makefile for the linux guest additions base directory.
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

DEPTH = ../../../..
include	$(PATH_KBUILD)/header.kmk

VBOX_VERSION = $(VBOX_VERSION_MAJOR).$(VBOX_VERSION_MINOR).$(VBOX_VERSION_BUILD)

# This can only be built on a real Linux system.
SUBDIRS =
ifeq ($(filter-out linux l4, $(BUILD_TARGET)),)
SUBDIRS += \
	module \
	daemon \
	xgraphics \
	xmouse
ifndef VBOX_OSE
SUBDIRS += \
	sharedfolders
endif
endif

PACKING      = $(PATH_BIN)/additions/VBoxLinuxAdditions.run
OTHER_CLEAN  = $(PACKING)


include	$(PATH_KBUILD)/footer.kmk


#
# Build the Linux Guest Additions self extracting installer.
#

# We need to depend on all source files for the additions and shared
# folders kernel modules.
$(PATH_BIN)/additions/VBoxLinuxAdditions.run: \
		$(INSTARGET_vboxmod-bin) \
		$(PATH_BIN)/additions/vboxadd-timesync \
		installer/vboxadd-timesync.sh \
		installer/vboxadd.sh \
		installer/install.sh \
		../../Installer/linux/runlevel.sh \
		installer/x11config.pl \
		installer/Makefile.test \
		installer/test.c \
		$(PATH_BIN)/additions/vboxmouse_drv.o \
		$(PATH_BIN)/additions/vboxmouse_drv_70.so \
		$(PATH_BIN)/additions/vboxmouse_drv_71.so \
		$(PATH_BIN)/additions/vboxvideo_drv.o \
		$(PATH_BIN)/additions/vboxvideo_drv_70.so \
		$(PATH_BIN)/additions/vboxvideo_drv_71.so \
		$(if VBOX_OSE,,installer/vboxvfs.sh $(PATH_BIN)/additions/mountvboxsf) \
		$(VBOX_VERSION_STAMP)
	$(call MSG_L1,Creating $@)
	$(QUIET)$(MKDIR) -p $(PATH_TARGET)/install
# Remove target directory first, otherwise the behaviour of cp will not be
# what we want if it already exists. See the cp manual page for more details.
	$(QUIET)rm -rf $(PATH_TARGET)/install/module
	$(QUIET)cp -af $(PATH_BIN)/additions/src $(PATH_TARGET)/install/module
	$(QUIET)$(MKDIR) -p $(PATH_TARGET)/install/module/test
	$(QUIET)$(INSTALL) -m 0644 installer/Makefile.test  $(PATH_TARGET)/install/module/test/Makefile
	$(QUIET)$(INSTALL) -m 0644 installer/test.c $(PATH_TARGET)/install/module/test/
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/vboxadd-timesync $(PATH_TARGET)/install/vboxadd-timesync
	$(QUIET)$(SED) "s;_VERSION_;$(VBOX_VERSION);g" installer/install.sh | $(SED) "s;_BUILD_;$(shell date);g" | $(SED) "s;_OSE_;$(VBOX_OSE);g" > $(PATH_TARGET)/install/install_.sh
	$(QUIET)$(INSTALL) -m 0755 $(PATH_TARGET)/install/install_.sh $(PATH_TARGET)/install/install.sh
	$(QUIET)$(RM) $(PATH_TARGET)/install/install_.sh
	$(QUIET)$(INSTALL) -m 0755 installer/vboxadd-timesync.sh $(PATH_TARGET)/install
	$(QUIET)$(INSTALL) -m 0755 installer/vboxadd.sh   $(PATH_TARGET)/install
	$(QUIET)$(INSTALL) -m 0755 ../../Installer/linux/runlevel.sh  $(PATH_TARGET)/install
	$(QUIET)$(INSTALL) -m 0755 installer/x11config.pl $(PATH_TARGET)/install
	$(QUIET)objcopy -g -R .comment $(PATH_BIN)/additions/vboxmouse_drv.o  $(PATH_TARGET)/install/vboxmouse_drv.o
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/vboxmouse_drv_70.so $(PATH_TARGET)/install/vboxmouse_drv_70.so
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/vboxmouse_drv_71.so $(PATH_TARGET)/install/vboxmouse_drv_71.so
	$(QUIET)objcopy -g -R .comment $(PATH_BIN)/additions/vboxvideo_drv.o  $(PATH_TARGET)/install/vboxvideo_drv.o
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/vboxvideo_drv_70.so $(PATH_TARGET)/install/vboxvideo_drv_70.so
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/vboxvideo_drv_71.so $(PATH_TARGET)/install/vboxvideo_drv_71.so
ifndef VBOX_OSE
	$(QUIET)$(INSTALL) -m 0755 installer/vboxvfs.sh $(PATH_TARGET)/install
	$(QUIET)objcopy -S -R .comment $(PATH_BIN)/additions/mountvboxsf $(PATH_TARGET)/install/mount.vboxsf
endif
	$(QUIET)$(VBOX_MAKESELF) $(PATH_TARGET)/install $@ \
	"VirtualBox $(VBOX_VERSION) Guest Additions for Linux installation" /bin/sh ./install.sh "> /dev/null"

