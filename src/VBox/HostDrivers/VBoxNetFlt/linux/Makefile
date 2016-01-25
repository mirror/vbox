#
# Makefile for the VirtualBox Linux Host Network Filter Driver.
# (For 2.6.x this file must be called 'Makefile'!)
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
override MODULE = vboxnetflt
OBJS   = \
	linux/VBoxNetFlt-linux.o \
	VBoxNetFlt.o \
	SUPR0IdcClient.o \
	SUPR0IdcClientComponent.o \
	linux/SUPR0IdcClient-linux.o

ifeq ($(BUILD_TARGET_ARCH),x86)
OBJS += math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
        math/gcc/divdi3.o \
	math/gcc/umoddi3.o
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
 INCL    += $(addprefix -I$(KBUILD_EXTMOD)/vboxnetflt,/ /include /r0drv/linux)
 export INCL
endif
ifneq ($(wildcard $(KBUILD_EXTMOD)/vboxnetflt),)
 MANGLING := $(KBUILD_EXTMOD)/vboxnetflt/include/VBox/SUPDrvMangling.h
else
 MANGLING := $(KBUILD_EXTMOD)/include/VBox/SUPDrvMangling.h
endif
KFLAGS   := -D__KERNEL__ -DMODULE -DRT_OS_LINUX -DIN_RING0 -DIN_RT_R0 \
	    -DIN_SUP_R0 -DVBOX -DRT_WITH_VBOX -DVBOX_WITH_HARDENING \
	    -Wno-declaration-after-statement
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
 #KFLAGS  += -DIPRT_DEBUG_SEMS
endif

# By default we use remap_pfn_range() kernel API to make kernel pages
# visible for userland. Unfortunately, it leads to situation that
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
EXTRA_CFLAGS  += -include $(MANGLING) $(INCL) $(KFLAGS) $(KDEBUG)

all: $(MODULE)

obj-m += $(MODULE).o

# OL/UEK: disable module signing for external modules -- we don't have any private key
$(MODULE):
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) CONFIG_MODULE_SIG= -C $(KERN_DIR) modules

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
