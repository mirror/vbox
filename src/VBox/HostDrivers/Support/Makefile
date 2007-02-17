#
# Sub-Makefile for the support library and the drivers/modules/kexts it uses.
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

DEPTH ?= ../../../..
SUB_PATH = ..
include	$(PATH_KBUILD)/subheader.kmk

#
# Targets
#
LIBRARIES += SUPR3
ifneq ($(filter-out darwin,$(BUILD_TARGET)),)
LIBRARIES += SUPR0
endif
ifdef VBOX_WITH_VBOXDRV
SYSMODS.darwin += VBoxDrv
SYSMODS.linux  += vboxdrv
SYSMODS.win    += VBoxDrv
endif


#
# Include sub-makefile(s).
#
include $(VBOX_PATH_SUPPORT)/testcase/Makefile


ifeq ($(BUILD_TARGET),linux)
## @todo change this to use a INSTALLS target.
EXPORT_FILE_PAIRS = \
	include/iprt/alloc.h=$(PATH_ROOT)/include/iprt/alloc.h \
	include/iprt/asm.h=$(PATH_ROOT)/include/iprt/asm.h \
	include/iprt/assert.h=$(PATH_ROOT)/include/iprt/assert.h \
	include/iprt/cdefs.h=$(PATH_ROOT)/include/iprt/cdefs.h \
	include/iprt/err.h=$(PATH_ROOT)/include/iprt/err.h \
	include/iprt/heap.h=$(PATH_ROOT)/include/iprt/heap.h \
	include/iprt/initterm.h=$(PATH_ROOT)/include/iprt/initterm.h \
	include/iprt/log.h=$(PATH_ROOT)/include/iprt/log.h \
	include/iprt/mem.h=$(PATH_ROOT)/include/iprt/mem.h \
	include/iprt/param.h=$(PATH_ROOT)/include/iprt/param.h \
	include/iprt/process.h=$(PATH_ROOT)/include/iprt/process.h \
	include/iprt/semaphore.h=$(PATH_ROOT)/include/iprt/semaphore.h \
	include/iprt/spinlock.h=$(PATH_ROOT)/include/iprt/spinlock.h \
	include/iprt/stdarg.h=$(PATH_ROOT)/include/iprt/stdarg.h \
	include/iprt/stdint.h=$(PATH_ROOT)/include/iprt/stdint.h \
	include/iprt/string.h=$(PATH_ROOT)/include/iprt/string.h \
	include/iprt/thread.h=$(PATH_ROOT)/include/iprt/thread.h \
	include/iprt/types.h=$(PATH_ROOT)/include/iprt/types.h \
	include/iprt/avl.h=$(PATH_ROOT)/include/iprt/avl.h \
	include/VBox/cdefs.h=$(PATH_ROOT)/include/VBox/cdefs.h \
	include/VBox/log.h=$(PATH_ROOT)/include/VBox/log.h \
	include/VBox/sup.h=$(PATH_ROOT)/include/VBox/sup.h \
	include/VBox/types.h=$(PATH_ROOT)/include/VBox/types.h \
	include/internal/initterm.h=$(PATH_ROOT)/src/VBox/Runtime/include/internal/initterm.h \
	include/internal/thread.h=$(PATH_ROOT)/src/VBox/Runtime/include/internal/thread.h \
	linux/SUPDrv-linux.c=$(VBOX_PATH_SUPPORT)/linux/SUPDrv-linux.c \
	Makefile=$(VBOX_PATH_SUPPORT)/linux/Makefile \
	alloc/heapsimple.c=$(PATH_ROOT)/src/VBox/Runtime/alloc/heapsimple.cpp \
	r0drv/alloc-r0drv.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/alloc-r0drv.cpp \
	r0drv/alloc-r0drv.h=$(PATH_ROOT)/src/VBox/Runtime/r0drv/alloc-r0drv.h \
	r0drv/initterm-r0drv.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/initterm-r0drv.cpp \
	r0drv/linux/alloc-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/alloc-r0drv-linux.c \
	r0drv/linux/initterm-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/initterm-r0drv-linux.c \
	r0drv/linux/process-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/process-r0drv-linux.c \
	r0drv/linux/semaphore-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/semaphore-r0drv-linux.c \
	r0drv/linux/spinlock-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/spinlock-r0drv-linux.c \
	r0drv/linux/string.h=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/string.h \
	r0drv/linux/thread-r0drv-linux.c=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/thread-r0drv-linux.c \
	r0drv/linux/the-linux-kernel.h=$(PATH_ROOT)/src/VBox/Runtime/r0drv/linux/the-linux-kernel.h \
	SUPDRV.h=$(VBOX_PATH_SUPPORT)/SUPDRV.h \
	SUPDRVIOC.h=$(VBOX_PATH_SUPPORT)/SUPDRVIOC.h \
	SUPDRVShared.c=$(VBOX_PATH_SUPPORT)/SUPDRVShared.c
OTHERS       := $(foreach pair,$(EXPORT_FILE_PAIRS),$(PATH_BIN)/src/$(firstword $(subst =, ,$(pair))))
OTHER_CLEAN   = $(OTHERS)
endif # real linux



#
# The Ring-3 Support Library (this is linked into the IPRT dll, VBoxRT).
#
ifneq ($(filter l4%,$(BUILD_TARGET) $(BUILD_TARGET_SUB)),)
# L4 has trouble with -pedantic. It also make trouble when inlining is not enabled.
SUPR3_TEMPLATE      = VBOXR3NP
else
SUPR3_TEMPLATE      = VBOXR3
endif
SUPR3_DEFS          = IN_SUP_R3 IN_RT_R3
SUPR3_INCS          = $(VBOX_PATH_SUPPORT)
SUPR3_INCS.l4       = $(L4_INCDIR)
ifneq ($(BUILD_TARGET),win) ## @todo rename win32 -> win!
SUPR3_SOURCES       = \
	$(VBOX_PATH_SUPPORT)/SUPLib.cpp \
	$(VBOX_PATH_SUPPORT)/$(BUILD_TARGET)/SUPLib-$(BUILD_TARGET).cpp
else
SUPR3_SOURCES       = \
	$(VBOX_PATH_SUPPORT)/SUPLib.cpp \
	$(VBOX_PATH_SUPPORT)/win32/SUPLib-win32.cpp
endif

#
# SUPR0 - The Ring-0 Import / Thunk library.
#
SUPR0_TEMPLATE      = VBOXR0
ifeq ($(VBOX_LDR_FMT),pe)
SUPR0_SOURCES       += $(VBOX_PATH_SUPPORT)/SUPR0.def
endif
ifeq ($(VBOX_LDR_FMT),lx)
SUPR0_SOURCES       += $(PATH_TARGET)/SUPR0.def
$(PATH_TARGET)/SUPR0.def: $(VBOX_PATH_SUPPORT)/SUPR0.def | $(call DIRDEP,$(PATH_TARGET))
	$(SED) -e 's/^[ \t][ \t]*\([gA-Z]\)/    _\1/' -e 's/[ \t]DATA[ \t]*/ /' $< > $@.tmp
	$(MV) -f $@.tmp $@
endif


#
# VBoxDrv.sys - The Windows driver.
#
ifeq ($(BUILD_TARGET),win)
VBoxDrv_TEMPLATE      = VBOXR0DRV
VBoxDrv_DEFS          = IN_RT_R0 IN_SUP_R0
VBoxDrv_SDKS          = W2K3DDK WINPSDKINCS
VBoxDrv_INCS          = $(VBOX_PATH_SUPPORT)
VBoxDrv_SOURCES       = \
	$(VBOX_PATH_SUPPORT)/SUPDRVShared.c
VBoxDrv_SOURCES.x86   = \
	$(VBOX_PATH_SUPPORT)/win32/SUPDrv-win32.cpp \
	$(VBOX_PATH_SUPPORT)/win32/SUPDrvA-win32.asm
VBoxDrv_SOURCES.amd64 = \
	$(VBOX_PATH_SUPPORT)/win64/SUPDrv-win64.cpp \
	$(VBOX_PATH_SUPPORT)/win64/SUPDrvA-win64.asm
VBoxDrv_LDFLAGS.x86   = -Entry:DriverEntry@8
VBoxDrv_LDFLAGS.amd64 = -Entry:DriverEntry
VBoxDrv_LIBS          = \
	$(PATH_SDK_W2K3DDK_LIB)/ntoskrnl.lib \
	$(PATH_SDK_W2K3DDK_LIB)/hal.lib \
	$(PATH_LIB)/RuntimeR0Drv$(VBOX_SUFF_LIB)
endif


#
# vboxdrv.ko - The Linux Kernel Module.
#
ifeq ($(BUILD_TARGET),linux)
vboxdrv_TEMPLATE      = VBOXR0DRV
vboxdrv_DEFS          = KBUILD_MODNAME=KBUILD_STR\(vboxdrv\) KBUILD_BASENAME=KBUILD_STR\(vboxdrv\) MODULE IN_RT_R0 IN_SUP_R0 CONFIG_VBOXDRV_AS_MISC
vboxdrv_INCS          = $(VBOX_PATH_SUPPORT)
vboxdrv_LIBS          = $(PATH_LIB)/RuntimeR0Drv$(VBOX_SUFF_LIB)
vboxdrv_LIBS.debug    = $(vboxdrv_LIBS) $(VBOX_GCC_LIBGCC)
vboxdrv_SOURCES       = \
	$(VBOX_PATH_SUPPORT)/$(BUILD_TARGET)/SUPDrv-$(BUILD_TARGET).c \
	$(VBOX_PATH_SUPPORT)/SUPDRVShared.c
ifndef VBOX_LINUX_VERSION_2_4
vboxdrv_SOURCES      += \
	$(VBOX_PATH_SUPPORT)/$(BUILD_TARGET)/SUPDrv-$(BUILD_TARGET).mod.c
endif
endif # real linux


#
# VBoxDrv.kext - The Darwin Kernel Extension.
#
ifeq ($(BUILD_TARGET),darwin)
VBoxDrv_TEMPLATE      = VBOXR0DRV
VBoxDrv_DEFS          = IN_RT_R0 IN_SUP_R0 USE_NEW_OS_INTERFACE
VBoxDrv_INCS          = $(VBOX_PATH_SUPPORT)
VBoxDrv_LIBS          = $(PATH_LIB)/RuntimeR0Drv$(VBOX_SUFF_LIB)
VBoxDrv_LDFLAGS       = -v -Wl,-whyload -Wl,-v -Wl,-whatsloaded
VBoxDrv_INST          = $(INST_VBOXDRV)Contents/MacOS/
VBoxDrv_SOURCES       = \
	$(VBOX_PATH_SUPPORT)/SUPDRVShared.c \
	$(VBOX_PATH_SUPPORT)/$(BUILD_TARGET)/SUPDrv-$(BUILD_TARGET).cpp

INSTALLS += VBoxDrv.kext
VBoxDrv.kext_INST     = $(INST_VBOXDRV)Contents/
VBoxDrv.kext_SOURCES  = \
	$(PATH_TARGET)/Info.plist

$(PATH_TARGET)/Info.plist: $(VBOX_PATH_SUPPORT)/darwin/Info.plist $(VBOX_VERSION_MK) | $(call DIRDEP,$(PATH_TARGET))
	$(call MSG_L1,Generating $@)
	$(xQUIET)$(RM) -f $@
	$(xQUIET)$(SED) \
		-e 's/@VBOX_VERSION_MAJOR@/$(VBOX_VERSION_MAJOR)/g' \
		-e 's/@VBOX_VERSION_MINOR@/$(VBOX_VERSION_MINOR)/g' \
		-e 's/@VBOX_VERSION_BUILD@/$(VBOX_VERSION_BUILD)/g' \
		$< > $@

INSTALLS += Scripts
Scripts_INST     = $(INST_DIST)
Scripts_SOURCES  = \
	$(VBOX_PATH_SUPPORT)/darwin/load.sh

endif


#
# Generate rules for exporting the linux sources.
#
define def_export_rule
$(eval dst := $(word 1,$(subst =, ,$(pair))))
$(eval src := $(word 2,$(subst =, ,$(pair))))
$(PATH_BIN)/src/$(dst): $(src)
	$$(call MSG_L1,Installing $$@)
	$$(QUIET)$$(MKDIR) -p $$(@D)
	$$(QUIET)$$(CP) $$< $$@

symlinked/$(dst):
	$$(call MSG_L1,Symlink $$@)
	$$(QUIET)$$(MKDIR) -p $$(@D)
	$$(QUIET)ln -sfn $(abspath $(src)) $$@

endef
# generate rules
$(foreach pair,$(EXPORT_FILE_PAIRS),$(eval $(def_export_rule)))


#
# Rule for creating a symlinked setup.
#
symlinked: $(subst $(PATH_BIN)/src/,symlinked/, $(OTHERS))
	$(ECHO) done


include	$(PATH_KBUILD)/subfooter.kmk

