#
# Makefile for the VirtualBox Linux Guest Drivers.
#

#
#
# Copyright (C) 2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

KBUILD_VERBOSE =

all:
	@echo "*** Building 'vboxguest' module ***"
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest
	@cp vboxguest/vboxguest.*o .
	@echo
	@if [ -d vboxvfs ]; then \
	    if [ -f vboxguest/Module.symvers ]; then \
	        cp vboxguest/Module.symvers vboxvfs; \
	    fi; \
	    echo "*** Building 'vboxvfs' module ***"; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxvfs; \
	    if [ -f vboxvfs/vboxvfs.o ]; then \
	      cp vboxvfs/vboxvfs.o .; \
	    else \
	      cp vboxvfs/vboxvfs.ko .; \
	    fi; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest install
	@if [ -d vboxvfs ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxvfs install; \
	fi

clean:
	@$(MAKE) -C vboxguest clean
	@if [ -d vboxvfs ]; then \
	    $(MAKE) -C vboxvfs clean; \
	fi
	rm -f vboxguest.*o vboxvfs.*o

load:
	@/sbin/rmmod vboxvfs || true
	@/sbin/rmmod vboxguest || true
	@/sbin/insmod vboxguest.ko
	@if [ -f vboxvfs.ko ]; then /sbin/insmod vboxvfs.ko; fi
