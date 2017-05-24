# $Id$
## @file
# Makefile for the VirtualBox Linux Host Driver.
#

#
#
# Copyright (C) 2006-2015 Oracle Corporation
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

## @todo If you make non-trivial changes to this make file, please consider
#        rewriting it to use Makefile.include.header and
#        Makefile.include.footer.

#
# First, figure out which architecture we're targeting and the build type.
# (We have to support basic cross building (ARCH=i386|x86_64).)
# While at it, warn about BUILD_* vars found to help with user problems.
#
ifeq ($(filter-out x86_64 amd64 AMD64,$(shell uname -m)),)
 BUILD_TARGET_ARCH_DEF := amd64
else
 BUILD_TARGET_ARCH_DEF := x86
endif
ifneq ($(filter-out amd64 x86,$(BUILD_TARGET_ARCH)),)
 $(warning Ignoring unknown BUILD_TARGET_ARCH value '$(BUILD_TARGET_ARCH)'.)
 BUILD_TARGET_ARCH :=
endif
ifeq ($(BUILD_TARGET_ARCH),)
 ifeq ($(ARCH),x86_64)
  BUILD_TARGET_ARCH := amd64
 else
  ifeq ($(ARCH),i386)
   BUILD_TARGET_ARCH := x86
  else
   BUILD_TARGET_ARCH := $(BUILD_TARGET_ARCH_DEF)
  endif
 endif
else
 ifneq ($(BUILD_TARGET_ARCH),$(BUILD_TARGET_ARCH_DEF))
  $(warning Using BUILD_TARGET_ARCH='$(BUILD_TARGET_ARCH)' from the $(origin BUILD_TARGET_ARCH).)
 endif
endif

ifneq ($(filter-out release profile debug strict,$(BUILD_TYPE)),)
 $(warning Ignoring unknown BUILD_TYPE value '$(BUILD_TYPE)'.)
 BUILD_TYPE :=
endif
ifeq ($(BUILD_TYPE),)
 BUILD_TYPE := release
else
 ifneq ($(BUILD_TYPE),release)
  $(warning Using BUILD_TYPE='$(BUILD_TYPE)' from the $(origin BUILD_TYPE).)
 endif
endif

# override is required by the Debian guys
override MODULE = vboxdrv
OBJS   = \
	linux/SUPDrv-linux.o \
	SUPDrv.o \
	SUPDrvGip.o \
	SUPDrvSem.o \
	SUPDrvTracer.o \
	SUPLibAll.o \
	r0drv/alloc-r0drv.o \
	r0drv/initterm-r0drv.o \
	r0drv/memobj-r0drv.o \
	r0drv/mpnotification-r0drv.o \
	r0drv/powernotification-r0drv.o \
	r0drv/linux/assert-r0drv-linux.o \
	r0drv/linux/alloc-r0drv-linux.o \
	r0drv/linux/initterm-r0drv-linux.o \
	r0drv/linux/memobj-r0drv-linux.o \
	r0drv/linux/memuserkernel-r0drv-linux.o \
	r0drv/linux/mp-r0drv-linux.o \
	r0drv/linux/mpnotification-r0drv-linux.o \
	r0drv/linux/process-r0drv-linux.o \
	r0drv/linux/rtStrFormatKernelAddress-r0drv-linux.o \
	r0drv/linux/semevent-r0drv-linux.o \
	r0drv/linux/semeventmulti-r0drv-linux.o \
	r0drv/linux/semfastmutex-r0drv-linux.o \
	r0drv/linux/semmutex-r0drv-linux.o \
	r0drv/linux/spinlock-r0drv-linux.o \
	r0drv/linux/thread-r0drv-linux.o \
	r0drv/linux/thread2-r0drv-linux.o \
	r0drv/linux/threadctxhooks-r0drv-linux.o \
	r0drv/linux/time-r0drv-linux.o \
	r0drv/linux/timer-r0drv-linux.o \
	r0drv/generic/semspinmutex-r0drv-generic.o \
	common/alloc/alloc.o \
	common/checksum/crc32.o \
	common/checksum/ipv4.o \
	common/checksum/ipv6.o \
	common/err/RTErrConvertFromErrno.o \
	common/err/RTErrConvertToErrno.o \
	common/log/log.o \
	common/log/logellipsis.o \
	common/log/logrel.o \
	common/log/logrelellipsis.o \
	common/log/logcom.o \
	common/log/logformat.o \
	common/misc/RTAssertMsg1Weak.o \
	common/misc/RTAssertMsg2.o \
	common/misc/RTAssertMsg2Add.o \
	common/misc/RTAssertMsg2AddWeak.o \
	common/misc/RTAssertMsg2AddWeakV.o \
	common/misc/RTAssertMsg2Weak.o \
	common/misc/RTAssertMsg2WeakV.o \
	common/misc/assert.o \
	common/misc/handletable.o \
	common/misc/handletablectx.o \
	common/misc/thread.o \
	common/string/RTStrCopy.o \
	common/string/RTStrCopyEx.o \
	common/string/RTStrCopyP.o \
	common/string/RTStrNCmp.o \
	common/string/RTStrNLen.o \
	common/string/stringalloc.o \
	common/string/strformat.o \
	common/string/strformatrt.o \
	common/string/strformattype.o \
	common/string/strprintf.o \
	common/string/strtonum.o \
	common/table/avlpv.o \
	common/time/time.o \
	r0drv/linux/RTLogWriteDebugger-r0drv-linux.o \
	generic/RTAssertShouldPanic-generic.o \
	generic/RTLogWriteStdErr-stub-generic.o \
	generic/RTLogWriteStdOut-stub-generic.o \
	generic/RTLogWriteUser-generic.o \
	generic/RTMpGetArraySize-generic.o \
	generic/RTMpGetCoreCount-generic.o \
	generic/RTSemEventWait-2-ex-generic.o \
	generic/RTSemEventWaitNoResume-2-ex-generic.o \
	generic/RTSemEventMultiWait-2-ex-generic.o \
	generic/RTSemEventMultiWaitNoResume-2-ex-generic.o \
	generic/RTTimerCreate-generic.o \
	generic/errvars-generic.o \
	generic/mppresent-generic.o \
	generic/uuid-generic.o \
	VBox/log-vbox.o
ifeq ($(BUILD_TARGET_ARCH),x86)
OBJS += math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
	math/gcc/divdi3.o \
	math/gcc/umoddi3.o
endif
ifeq ($(BUILD_TARGET_ARCH),amd64)
OBJS += common/alloc/heapsimple.o
endif
ifdef VBOX_WITH_NATIVE_DTRACE
OBJS += SUPDrvDTrace.o
endif

ifneq ($(MAKECMDGOALS),clean)

ifeq ($(KERNELRELEASE),)

 #
 # building from this directory
 #

 # kernel base directory
 ifndef KERN_DIR
  # build for the current kernel, version check
  KERN_DIR := /lib/modules/$(shell uname -r)/build
  ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
   KERN_DIR := /usr/src/linux
   ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
    $(error Error: unable to find the sources of your current Linux kernel. \
	           Specify KERN_DIR=<directory> and run Make again)
   endif
   $(warning Warning: using /usr/src/linux as the source directory of your \
                      Linux kernel. If this is not correct, specify \
		      KERN_DIR=<directory> and run Make again.)
  endif
 else
  ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
   $(error Error: KERN_DIR does not point to a directory)
  endif
 endif

 # includes
 ifndef KERN_INCL
  KERN_INCL = $(KERN_DIR)/include
 endif
 ifneq ($(shell if test -d $(KERN_INCL); then echo yes; fi),yes)
  $(error Error: unable to find the include directory for your current Linux \
                 kernel. Specify KERN_INCL=<directory> and run Make again)
 endif

 # module install dir, only for current kernel
 ifneq ($(filter install install_rpm,$(MAKECMDGOALS)),)
  ifndef MODULE_DIR
   MODULE_DIR_TST := /lib/modules/$(shell uname -r)
   ifeq ($(shell if test -d $(MODULE_DIR_TST); then echo yes; fi),yes)
    MODULE_DIR := $(MODULE_DIR_TST)/misc
   else
    $(error Unable to find the folder to install the support driver to)
   endif
  endif # MODULE_DIR unspecified
 endif

else # neq($(KERNELRELEASE),)

 #
 # building from kbuild (make -C <kernel_directory> M=`pwd`)
 #

endif # neq($(KERNELRELEASE),)

# debug - show guesses.
ifdef DEBUG
$(warning dbg: KERN_DIR     = $(KERN_DIR))
$(warning dbg: KERN_INCL    = $(KERN_INCL))
$(warning dbg: MODULE_DIR   = $(MODULE_DIR))
endif

KBUILD_VERBOSE ?= 1

#
# Compiler options
#
ifndef INCL
 INCL    := $(addprefix -I,$(KERN_INCL) $(EXTRA_INCL))
 ifndef KBUILD_EXTMOD
  KBUILD_EXTMOD := $(shell pwd)
 endif
 INCL    += $(addprefix -I$(KBUILD_EXTMOD),/ /include /r0drv/linux)
 INCL    += $(addprefix -I$(KBUILD_EXTMOD)/vboxdrv,/ /include /r0drv/linux)
 ifdef VBOX_WITH_NATIVE_DTRACE
  INCL   += -I/usr/include/linux -I/usr/include
 endif
 export INCL
endif
ifneq ($(wildcard $(KBUILD_EXTMOD)/vboxdrv),)
 MANGLING := $(KBUILD_EXTMOD)/vboxdrv/include/VBox/SUPDrvMangling.h
else
 MANGLING := $(KBUILD_EXTMOD)/include/VBox/SUPDrvMangling.h
endif
KFLAGS   := -D__KERNEL__ -DMODULE -DRT_OS_LINUX -DIN_RING0 -DIN_RT_R0 \
	    -DIN_SUP_R0 -DVBOX -DRT_WITH_VBOX -DVBOX_WITH_HARDENING \
	    -DSUPDRV_WITH_RELEASE_LOGGER -DVBOX_WITH_EFLAGS_AC_SET_IN_VBOXDRV -DIPRT_WITH_EFLAGS_AC_PRESERVING \
	    -Wno-declaration-after-statement
ifdef VBOX_REDHAT_KABI
 KFLAGS  += -DVBOX_REDHAT_KABI
endif
ifndef CONFIG_VBOXDRV_FIXEDMAJOR
 KFLAGS  += -DCONFIG_VBOXDRV_AS_MISC
endif
ifdef VBOX_WITH_NATIVE_DTRACE
 KFLAGS  += -DVBOX_WITH_NATIVE_DTRACE
endif
ifeq ($(BUILD_TARGET_ARCH),amd64)
 KFLAGS  += -DRT_ARCH_AMD64
else
 KFLAGS  += -DRT_ARCH_X86
endif
# must be consistent with Config.kmk!
KFLAGS  += -DVBOX_WITH_64_BITS_GUESTS
ifeq ($(BUILD_TYPE),debug)
 KFLAGS  += -DDEBUG -DDEBUG_$(USER) -g
 # IPRT_DEBUG_SEMS indicates thread wrt sems state via the comm field.
 #KFLAGS  += -DIPRT_DEBUG_SEMS
endif
ifdef VBOX_WITH_TEXT_MODMEM_HACK
 KFLAGS += -DRTMEMALLOC_EXEC_HEAP -DVBOX_WITH_TEXT_MODMEM_HACK
endif

# 2.6 and later
MODULE_EXT    := ko
$(MODULE)-y   := $(OBJS)

# build defs
EXTRA_CFLAGS  += -include $(MANGLING) $(INCL) $(KFLAGS) $(KDEBUG) -fno-omit-frame-pointer -fno-pie

all: $(MODULE)

obj-m += $(MODULE).o

JOBS := $(shell (getconf _NPROCESSORS_ONLN || grep -Ec '^processor|^CPU[0-9]' /proc/cpuinfo) 2>/dev/null)
ifeq ($(JOBS),0)
  JOBS := 1
endif

# OL/UEK: disable module signing for external modules -- we don't have any private key
$(MODULE):
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) CONFIG_MODULE_SIG= -C $(KERN_DIR) -j$(JOBS) modules

install: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0644 -o root -g root $(MODULE).$(MODULE_EXT) $(MODULE_DIR); \
	PATH="$(PATH):/bin:/sbin" depmod -a; \
	rm -f /etc/vbox/module_not_compiled

install_rpm: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0644 $(MODULE).$(MODULE_EXT) $(MODULE_DIR)

else # eq ($(MAKECMDGOALS),clean)

 ifndef KERN_DIR
  KERN_DIR := /lib/modules/$(shell uname -r)/build
  ifeq ($(wildcard $(KERN_DIR)/Makefile),)
   KERN_DIR := /usr/src/linux
  endif
 endif
 ifeq ($(wildcard $(KERN_DIR)/Makefile),)

clean:
	find . \( -name \*.o -o -name \*.cmd \) -print0 | xargs -0 rm -f
	rm -rf .tmp_ver* $(MODULE).* Module.symvers Modules.symvers modules.order

 else

clean:
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) -C $(KERN_DIR) clean

 endif

endif # eq($(MAKECMDGOALS),clean)

check: $(MODULE)
	@if ! readelf -p __ksymtab_strings vboxdrv.ko | grep -E "\[.*\]  *(RT|g_..*RT.*)"; then \
	    echo "All exported IPRT symbols are properly renamed!"; \
	 else \
	    echo "error: Some exported IPRT symbols was not properly renamed! See above." >&2; \
	    false; \
	 fi

