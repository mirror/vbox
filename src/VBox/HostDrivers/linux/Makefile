#
# Makefile for the VirtualBox Linux Host Drivers.
#

#
#
# Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
	@if [ -d vboxnetadp ]; then \
	    echo "*** Building 'vboxnetadp' module ***"; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxnetadp; \
	    cp vboxnetadp/vboxnetadp.ko .; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxdrv install
	@if [ -d vboxnetflt ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxnetflt install; \
	fi
	@if [ -d vboxnetadp ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vboxnetadp install; \
	fi

clean:
	@$(MAKE) -C vboxdrv clean
	@if [ -d vboxnetflt ]; then \
	    $(MAKE) -C vboxnetflt clean; \
	fi
	@if [ -d vboxnetadp ]; then \
	    $(MAKE) -C vboxnetadp clean; \
	fi
	rm -f vboxdrv.ko vboxnetflt.ko vboxnetadp.ko

load:
	@for module in vboxnetadp vboxnetflt vboxdrv; do \
		if grep "^$$module " /proc/modules >/dev/null; then \
			echo "Removing previously installed $$module module"; \
			/sbin/rmmod $$module; \
		fi; \
	done
	@for module in vboxdrv vboxnetflt vboxnetadp; do \
		if test -f $$module.ko; then \
			echo "Installing $$module module"; \
			/sbin/insmod $$module.ko; \
		fi; \
	done
