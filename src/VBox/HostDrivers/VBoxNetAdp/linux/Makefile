#
# Makefile for the VirtualBox Linux Host Virtual Network Adapter Driver.
# (For 2.6.x this file must be called 'Makefile'!)
#

#
#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
override MODULE = vboxnetadp
OBJS   = \
	linux/VBoxNetAdp-linux.o \
	VBoxNetAdp.o \
	r0drv/alloc-r0drv.o \
	r0drv/initterm-r0drv.o \
	r0drv/memobj-r0drv.o \
	r0drv/mpnotification-r0drv.o \
	r0drv/powernotification-r0drv.o \
	r0drv/linux/assert-r0drv-linux.o \
	r0drv/linux/alloc-r0drv-linux.o \
	r0drv/linux/initterm-r0drv-linux.o \
	r0drv/linux/memobj-r0drv-linux.o \
	r0drv/linux/mp-r0drv-linux.o \
	r0drv/linux/mpnotification-r0drv-linux.o \
	r0drv/linux/process-r0drv-linux.o \
	r0drv/linux/semevent-r0drv-linux.o \
	r0drv/linux/semeventmulti-r0drv-linux.o \
	r0drv/linux/semfastmutex-r0drv-linux.o \
	r0drv/linux/spinlock-r0drv-linux.o \
	r0drv/linux/thread-r0drv-linux.o \
	r0drv/linux/thread2-r0drv-linux.o \
	r0drv/linux/time-r0drv-linux.o \
	common/err/RTErrConvertFromErrno.o \
	common/err/RTErrConvertToErrno.o \
	common/log/log.o \
	common/log/logellipsis.o \
	common/log/logrel.o \
	common/log/logrelellipsis.o \
	common/log/logcom.o \
	common/log/logformat.o \
	common/string/strformat.o \
	common/string/strformatrt.o \
	common/string/strformattype.o \
	common/string/strprintf.o \
	common/string/strtonum.o \
	r0drv/linux/RTLogWriteDebugger-r0drv-linux.o \
	generic/RTAssertShouldPanic-generic.o \
	generic/RTLogWriteStdErr-stub-generic.o \
	generic/RTLogWriteStdOut-stub-generic.o \
	generic/RTLogWriteUser-generic.o \
	generic/uuid-generic.o \
	VBox/log-vbox.o \
	VBox/strformat-vbox.o
ifeq ($(BUILD_TARGET_ARCH),x86)
OBJS += math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
        math/gcc/divdi3.o \
	math/gcc/umoddi3.o
endif
ifeq ($(BUILD_TARGET_ARCH),amd64)
OBJS += alloc/heapsimple.o
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
  # check if versions match -- works only for later 2.6 kernels
  VBOX_KERN_VER := $(shell $(MAKE) -sC $(KERN_DIR) kernelrelease 2> /dev/null || true)
  ifneq ($(VBOX_KERN_VER),)
   ifneq ($(VBOX_KERN_VER),$(shell uname -r))
    $(error Error: /usr/src/linux (version $(VBOX_KERN_VER)) does not match \
                   the current kernel (version $(shell uname -r)))
   endif
  endif
 else
  # build for a dedicated kernel, no version check
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
 export INCL
endif
KFLAGS   := -D__KERNEL__ -DMODULE -DRT_OS_LINUX -DIN_RING0 -DIN_RT_R0 -DIN_SUP_R0 -DVBOX -DRT_WITH_VBOX -DVBOX_WITH_HARDENING
ifdef VBOX_REDHAT_KABI
 KFLAGS  += -DVBOX_REDHAT_KABI
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
 KFLAGS  += -DIPRT_DEBUG_SEMS
endif

# By default we use remap_pfn_range() kernel API to make kernel pages
# visible for userland. Unfortuately, it leads to situation that
# during debug session all structures on that page (such as PVM pointer)
# are not accessible to the debugger (see #3214).
# This code enables experimental support
# for vm_insert_page() kernel API, allowing to export kernel pages
# to the userland in more debugger-friendly way. Due to stability
# concerns, not enabled by default yet.
ifdef VBOX_USE_INSERT_PAGE
 KFLAGS  += -DVBOX_USE_INSERT_PAGE
endif

MODULE_EXT    := ko
$(MODULE)-y   := $(OBJS)

# build defs
EXTRA_CFLAGS  += $(INCL) $(KFLAGS) $(KDEBUG)

all: $(MODULE)

obj-m += $(MODULE).o

$(MODULE):
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C $(KERN_DIR) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) modules

install: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0664 -o root -g root $(MODULE).$(MODULE_EXT) $(MODULE_DIR); \
	PATH="$(PATH):/bin:/sbin" depmod -ae; \
	rm -f /etc/vbox/module_not_compiled

install_rpm: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0664 $(MODULE).$(MODULE_EXT) $(MODULE_DIR)

endif # eq($(MAKECMDGOALS),clean)

# important: Don't remove Module.symvers! DKMS does 'make clean' before building ...
clean:
	for f in . linux r0drv r0drv/linux VBox common/err common/string common/log generic math/gcc; \
	    do rm -f $$f/*.o $$f/.*.cmd $$f/.*.flags; done
	rm -rf .vboxnetadp* .tmp_ver* vboxnetadp.* Modules.symvers modules.order
