#
# Makefile for the VirtualBox Linux Host Driver.
# (For 2.6.x this file must be called 'Makefile'!)
#

#
#
#  Copyright (C) 2006-2007 innotek GmbH
#
#  This file is part of VirtualBox Open Source Edition (OSE), as
#  available from http://www.virtualbox.org. This file is free software;
#  you can redistribute it and/or modify it under the terms of the GNU
#  General Public License as published by the Free Software Foundation,
#  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
#  distribution. VirtualBox OSE is distributed in the hope that it will
#  be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# First, figure out which architecture we're targeting.
# (We have to support basic cross building (ARCH=i386|x86_64).)
#
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
   ifeq ($(filter-out x86_64 amd64 AMD64,$(shell uname -m)),)
    BUILD_TARGET_ARCH := amd64
   else
    BUILD_TARGET_ARCH := x86
   endif
  endif
 endif
endif


MODULE = vboxdrv
OBJS   = \
	linux/SUPDrv-linux.o \
	SUPDRVShared.o \
	r0drv/alloc-r0drv.o \
	r0drv/initterm-r0drv.o \
	r0drv/linux/alloc-r0drv-linux.o \
	r0drv/linux/initterm-r0drv-linux.o \
	r0drv/linux/process-r0drv-linux.o \
	r0drv/linux/semevent-r0drv-linux.o \
	r0drv/linux/semeventmulti-r0drv-linux.o \
	r0drv/linux/semfastmutex-r0drv-linux.o \
	r0drv/linux/spinlock-r0drv-linux.o \
	r0drv/linux/thread-r0drv-linux.o
ifeq ($(BUILD_TARGET_ARCH),amd64)
OBJS  += alloc/heapsimple.o
endif

ifneq ($(MAKECMDGOALS),clean)

# kernel base directory
ifndef KERN_DIR
 KERN_DIR := /lib/modules/$(shell uname -r)/build
 ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
  KERN_DIR := /usr/src/linux
  ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
   $(error Error: unable to find the sources of your current Linux kernel. Specify KERN_DIR=<directory> and run Make again.)
  endif
  $(warning Warning: using /usr/src/linux as the source directory of your Linux kernel. If this is not correct, specify KERN_DIR=<directory> and run Make again.)
 endif
else
 ifneq ($(shell if test -d $(KERN_DIR); then echo yes; fi),yes)
  $(error Error: KERN_DIR does not point to a directory.)
 endif
endif

# includes
ifndef KERN_INCL
 KERN_INCL = $(KERN_DIR)/include
endif
ifneq ($(shell if test -d $(KERN_INCL); then echo yes; fi),yes)
 $(error Error: unable to find the include directory for your current Linux kernel. Specify KERN_INCL=<directory> and run Make again.)
endif

# module install dir.
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

# guess kernel version (24 or 26)
KERN_VERSION := $(if $(wildcard $(KERN_DIR)/Rules.make),24,26)

# debug - show guesses.
ifdef DEBUG
$(warning dbg: KERN_DIR     = $(KERN_DIR))
$(warning dbg: KERN_INCL    = $(KERN_INCL))
$(warning dbg: MODULE_DIR   = $(MODULE_DIR))
$(warning dbg: KERN_VERSION = $(KERN_VERSION))
endif

#
# Compiler options
#
ifndef INCL
 INCL    := -I$(KERN_INCL) $(addprefix -I, $(EXTRA_INCL))
 ifndef KBUILD_EXTMOD
  KBUILD_EXTMOD := $(shell pwd)
 endif
 INCL    += $(addprefix -I$(KBUILD_EXTMOD),/ /include /r0drv/linux)
 export INCL
endif
KFLAGS   := -D__KERNEL__ -DMODULE -DRT_OS_LINUX -DIN_RING0 -DIN_RT_R0 -DIN_SUP_R0
ifndef CONFIG_VBOXDRV_FIXEDMAJOR
 KFLAGS  += -DCONFIG_VBOXDRV_AS_MISC
endif
ifeq ($(BUILD_TARGET_ARCH),amd64)
 KFLAGS  += -DRT_ARCH_AMD64
else
 KFLAGS  += -DRT_ARCH_X86
endif
#ifeq ($(BUILD_TYPE),debug) - you'll have to enable this manually to get debug stuff.
#KFLAGS   += -DDEBUG
#endif

#
# Use the RTR0MemObj API - testing.
# If this makes testboxes crash/burn/leak disable it and add a comment to defect #2116.
#
KFLAGS += -DUSE_NEW_OS_INTERFACE_FOR_MM
OBJS += r0drv/memobj-r0drv.o \
	r0drv/linux/memobj-r0drv-linux.o
ifeq ($(BUILD_TARGET_ARCH),x86) # Some gcc versions ends up needing __divdi3.
OBJS += math/gcc/qdivrem.o \
	math/gcc/divdi3.o
endif

ifeq ($(KERN_VERSION), 24)
# 2.4
TOPDIR         = $(KERN_DIR)
MODULE_EXT    := o
EXTRA_CFLAGS  := -DVBOX_LINUX_2_4
$(MODULE)-objs = $(OBJS)
else
# 2.6 and later
MODULE_EXT    := ko
$(MODULE)-y   := $(OBJS)
endif

# build defs
EXTRA_CFLAGS  += $(INCL) $(KFLAGS) $(KDEBUG)

all: $(MODULE)

obj-m += $(MODULE).o

$(MODULE):
	$(MAKE) KBUILD_VERBOSE=1 -C $(KERN_DIR) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) modules

ifeq ($(KERN_VERSION), 24)
#
# 2.4 Module linking
#
$(MODULE).o: $(OBJS)
	$(LD) -o $@ -r $(OBJS)

include $(KERN_DIR)/Rules.make

endif

install: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0664 -o root -g root $(MODULE).$(MODULE_EXT) $(MODULE_DIR); \
	PATH="$(PATH):/bin:/sbin" depmod -ae; \
	rm -f /etc/vbox/module_not_compiled

install_rpm: $(MODULE)
	@mkdir -p $(MODULE_DIR); \
	install -m 0664 $(MODULE).$(MODULE_EXT) $(MODULE_DIR)

endif # eq($(MAKECMDGOALS),clean)

clean:
	for f in . linux r0drv r0drv/linux; do rm -f $$f/*.o $$f/.*.cmd $$f/.*.flags; done
	rm -rf .vboxdrv* .tmp_ver* vboxdrv.* Module.symvers Modules.symvers
