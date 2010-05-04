#
# Makefile for the VirtualBox Linux Guest Drivers.
#

#
#
# Copyright (C) 2009 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

KBUILD_VERBOSE =

all:
	@echo "*** Building 'vboxguest' module ***"
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest
	@if [ -f vboxguest/vboxguest.ko ]; then \
	    cp vboxguest/vboxguest.ko .; \
	 else \
	    cp vboxguest/vboxguest.o .; \
	 fi
	@echo
	@if [ -d vboxsf ]; then \
	    if [ -f vboxguest/Module.symvers ]; then \
	        cp vboxguest/Module.symvers vboxsf; \
	    fi; \
	    echo "*** Building 'vboxsf' module ***"; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxsf; \
	    if [ -f vboxsf/vboxsf.ko ]; then \
	        cp vboxsf/vboxsf.ko .; \
	    else \
	        cp vboxsf/vboxsf.o .; \
	    fi; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxguest install
	@if [ -d vboxsf ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxsf install; \
	fi

clean:
	@$(MAKE) -C vboxguest clean
	@if [ -d vboxsf ]; then \
	    $(MAKE) -C vboxsf clean; \
	fi
	rm -f vboxguest.*o vboxsf.*o

load:
	@/sbin/rmmod vboxvfs || true
	@/sbin/rmmod vboxsf || true
	@/sbin/rmmod vboxguest || true
	@/sbin/insmod vboxguest.ko
	@if [ -f vboxsf.ko ]; then /sbin/insmod vboxsf.ko; fi
