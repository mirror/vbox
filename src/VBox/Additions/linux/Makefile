#
# Makefile for the VirtualBox Linux Guest Drivers.
#

#
# Copyright (C) 2009-2019 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

ifneq ($(KBUILD_EXTMOD),)

# Building from kBuild (make -C <kernel_directory> M=`pwd`).
# KBUILD_EXTMOD is set to $(M) in this case.

obj-m = vboxguest/ vboxsf/ vboxvideo/

else # ! KBUILD_EXTMOD

KBUILD_VERBOSE =
 ifeq ($(KBUILD_VERBOSE),)
VBOX_QUIET    := @
VBOX_QUIET_SH := @
 else
VBOX_QUIET    :=
VBOX_QUIET_SH := set -x;
 endif

all: vboxguest vboxsf vboxvideo

vboxguest:
	@echo "=== Building 'vboxguest' module ==="
	$(VBOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest
	$(VBOX_QUIET_SH)if [ -f vboxguest/vboxguest.ko ]; then \
	    cp vboxguest/vboxguest.ko .; \
	 else \
	    cp vboxguest/vboxguest.o .; \
	 fi
	@echo

vboxsf: vboxguest
	$(VBOX_QUIET_SH)if [ -d vboxsf ]; then \
	    if [ -f vboxguest/Module.symvers ]; then \
	        cp vboxguest/Module.symvers vboxsf; \
	    fi; \
	    echo "=== Building 'vboxsf' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxsf || exit 1; \
	    if [ -f vboxsf/vboxsf.ko ]; then \
	        cp vboxsf/vboxsf.ko .; \
	    else \
	        cp vboxsf/vboxsf.o .; \
	    fi; \
	    echo; \
	fi

vboxvideo:
	$(VBOX_QUIET_SH)if [ -d vboxvideo ]; then \
	    if [ -f vboxguest/Module.symvers ]; then \
	        cp vboxguest/Module.symvers vboxvideo; \
	    fi; \
	    echo "=== Building 'vboxvideo' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxvideo || exit 1; \
	    if [ -f vboxvideo/vboxvideo.ko ]; then \
	        cp vboxvideo/vboxvideo.ko .; \
	    elif [ -f vboxvideo/vboxvideo.o ]; then \
	        cp vboxvideo/vboxvideo.o .; \
	    fi; \
	    echo; \
	fi

install:
	$(VBOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest install
	$(VBOX_QUIET_SH)if [ -d vboxsf ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxsf install; \
	fi
	$(VBOX_QUIET_SH)if [ -d vboxvideo ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxvideo install; \
	fi

clean:
	$(VBOX_QUIET)$(MAKE) -C vboxguest clean
	$(VBOX_QUIET_SH)if [ -d vboxsf ]; then \
	    $(MAKE) -C vboxsf clean; \
	fi
	$(VBOX_QUIET_SH)if [ -d vboxvideo ]; then \
	    $(MAKE) -C vboxvideo clean; \
	fi
	rm -f vboxguest.*o vboxsf.*o vboxvideo.*o

check:
	$(VBOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest check

load:
	$(VBOX_QUIET)/sbin/rmmod vboxvideo || true
	$(VBOX_QUIET)/sbin/rmmod vboxvfs || true
	$(VBOX_QUIET)/sbin/rmmod vboxsf || true
	$(VBOX_QUIET)/sbin/rmmod vboxguest || true
	$(VBOX_QUIET)/sbin/insmod vboxguest.ko
	$(VBOX_QUIET)if [ -f vboxsf.ko ]; then /sbin/insmod vboxsf.ko; fi
	$(VBOX_QUIET)if [ -f vboxvideo.ko ]; then /sbin/insmod vboxvideo.ko; fi

.PHONY: vboxguest vboxsf vboxvideo all install clean check load

endif # ! KBUILD_EXTMOD
