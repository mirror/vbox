#
# Makefile for the VirtualBox Linux Host Drivers.
#

#
#
# Copyright (C) 2008 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

KBUILD_VERBOSE =

all:
	@echo "*** Building 'vboxdrv' module ***"
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxdrv
	@cp vboxdrv/vboxdrv.ko .
	@echo
	@if [ -d vboxnetflt ]; then \
	    if [ -f vboxdrv/Module.symvers ]; then \
	        cp vboxdrv/Module.symvers vboxnetflt; \
	    fi; \
	    echo "*** Building 'vboxnetflt' module ***"; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxnetflt; \
	    cp vboxnetflt/vboxnetflt.ko .; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxdrv install
	@if [ -d vboxnetflt ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxnetflt install; \
	fi

clean:
	@$(MAKE) -C vboxdrv clean
	@if [ -d vboxnetflt ]; then \
	    $(MAKE) -C vboxnetflt clean; \
	fi
	rm -f vboxdrv.ko vboxnetflt.ko

load:
	@/sbin/rmmod vboxnetflt || true
	@/sbin/rmmod vboxdrv || true
	@/sbin/insmod vboxdrv.ko
	@if [ -f vboxnetflt.ko ]; then /sbin/insmod vboxnetflt.ko; fi
