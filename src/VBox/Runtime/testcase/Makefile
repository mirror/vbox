#
# Makefile for the IPRT testcases.
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

DEPTH = ../../../..
include	$(PATH_KBUILD)/header.kmk

ifdef VBOX_WITH_TESTCASES
PROGRAMS = \
	tstCritSect \
	tstPrfRT \
	tstLdr \
	tstLdr-2 \
	tstLdr-3 \
	tstLdrLoad \
	tstAvl \
	tstTimer \
	tstTime \
	tstTime-2 \
	tstTime-3 \
	tstTimeSpec \
	tstStrFormat \
	tstPath \
	tstLog \
	tstFile \
	tstFileLock \
	tstBitOperations \
	tstInlineAsm \
	tstStrToNum \
	tstDir \
	tstDir-2 \
	tstRTFsQueries \
	tstMove \
	tstUtf8 \
	tstDeadlock \
	tstErrUnique
PROGRAMS.win = \
	tstRTProcWait \
	tstCritSectW32 \
	ntGetTimerResolution
PROGRAMS.linux = \
	tstRTProcWait \
	tstBitOperationsPIC3 \
	tstInlineAsmPIC \
	tstInlineAsmPIC3
PROGRAMS.l4 = \
	tstIoCtl
ifneq ($(BUILD_TARGET),darwin) #until we've figured out how to link R0/GC modules on Mac OS X.
SYSMODS = tstLdrObj
endif
# tstSems
endif # VBOX_WITH_TESTCASES


TEMPLATE = VBOXR3TSTEXE


tstTimer_SOURCES = tstTimer.cpp

tstTime_SOURCES = tstTime.cpp

tstTime-2_SOURCES = tstTime-2.cpp

tstTime-3_SOURCES = tstTime-3.cpp

tstTimeSpec_SOURCES = tstTimeSpec.cpp

tstStrFormat_SOURCES = tstStrFormat.cpp

tstLog_SOURCES  = tstLog.cpp

tstPath_SOURCES = tstPath.cpp

tstSems_SOURCES = tstSems.cpp

tstAvl_SOURCES  = tstAvl.cpp

tstFile_SOURCES = tstFile.cpp

tstFileLock_SOURCES = tstFileLock.cpp

tstBitOperations_SOURCES = tstBitOperations.cpp

tstBitOperationsPIC3_SOURCES  = tstBitOperations.cpp
tstBitOperationsPIC3_CXXFLAGS = -fPIC -fomit-frame-pointer -O3
tstBitOperationsPIC3_DEFS     = PIC

tstInlineAsm_SOURCES     = tstInlineAsm.cpp

tstInlineAsmPIC_SOURCES  = tstInlineAsm.cpp
tstInlineAsmPIC_CXXFLAGS = -fPIC
tstInlineAsmPIC_DEFS     = PIC

tstInlineAsmPIC3_SOURCES  = tstInlineAsm.cpp
tstInlineAsmPIC3_CXXFLAGS = -fPIC -fomit-frame-pointer -O3
tstInlineAsmPIC3_DEFS     = PIC

tstCritSect_SOURCES     = tstCritSect.cpp

tstCritSectW32_SOURCES  = tstCritSect.cpp
tstCritSectW32_DEFS     = TRY_WIN32_CRIT

tstLdr_SOURCES = tstLdr.cpp

tstLdr-2_SOURCES = tstLdr-2.cpp
tstLdr-2_DEFS    = IN_DIS_R3
tstLdr-2_LIBS    = \
	$(PATH_LIB)/DisasmR3$(VBOX_SUFF_LIB)

tstLdrObj_TEMPLATE = VBOXGC
tstLdrObj_INST     = $(INST_TESTCASE)
tstLdrObj_SYSSUFF  = .gc
tstLdrObj_SOURCES  = tstLdrObj.cpp
tstLdrObj_DEFS     = IN_DIS_GC IN_RT_GC
ifeq ($(VBOX_LDR_FMT32),elf)
tstLdrObj_DEFS    += VBOX_SOME_IMPORT_FUNCTION
endif
tstLdrObj_LIBS     = \
	$(PATH_LIB)/DisasmGC$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/RuntimeGC$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/VMMGCBuiltin$(VBOX_SUFF_LIB)
ifeq ($(VBOX_LDR_FMT32),pe)
tstLdrObj_LDFLAGS  = -Entry:Entrypoint
endif # PE
ifeq ($(VBOX_LDR_FMT32),elf)
tstLdrObj_LDFLAGS  = -entry=Entrypoint
endif

tstLdr-3_SOURCES = tstLdr-3.cpp
tstLdr-3_DEFS    = IN_DIS_R3
tstLdr-3_LIBS    = \
	$(PATH_LIB)/DisasmR3$(VBOX_SUFF_LIB)

tstLdrLoad_SOURCES = tstLdrLoad.cpp

tstPrfRT_SOURCES = tstPrfRT.cpp

tstRTProcWait_SOURCES = tstRTProcWait.cpp

tstStrToNum_SOURCES = tstStrToNum.cpp

tstUtf8_SOURCES     = tstUtf8.cpp

tstDir_SOURCES      = tstDir.cpp

tstDir-2_SOURCES    = tstDir-2.cpp

tstRTFsQueries_SOURCES = tstRTFsQueries.cpp

tstMove_SOURCES     = tstMove.cpp

ntGetTimerResolution_SOURCES = ntGetTimerResolution.cpp
ntGetTimerResolution_SDKS.win = WINPSDK W2K3DDK
ntGetTimerResolution_LIBS.win = \
	$(PATH_SDK_W2K3DDK_LIB)/ntdll.lib

tstIoCtl_SOURCES = tstIoCtl.cpp

tstDeadlock_SOURCES = tstDeadlock.cpp

tstErrUnique_SOURCES = tstErrUnique.cpp
tstErrUnique_INCS    = $(PATH_TARGET)/..
## tstErrUnique.cpp depends on a generated header.
tstErrUnique.cpp_DEPS = $(PATH_TARGET)/../errmsgdata.h


include	$(PATH_KBUILD)/footer.kmk

