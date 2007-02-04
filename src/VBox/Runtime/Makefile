# $Id$
## @file
# Makefile for the InnoTek Portable Runtime (IPRT).
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


DEPTH = ../../..
include $(PATH_KBUILD)/header.kmk

ifndef VBOX_ADDITIONS_LINUX_ONLY
 # Normal mode.
 BLDPROGS   = uniread
 LIBRARIES  = RuntimeR3 RuntimeR0 RuntimeGC RuntimeEFCPP RuntimeR3NoCRTGCC
 ifdef VBOX_WITH_VBOXDRV
  LIBRARIES += RuntimeR0Drv
 endif
 ifdef VBOX_WITH_WIN32_ADDITIONS
  LIBRARIES += RuntimeW32GuestR0   RuntimeW32GuestR3 RuntimeW32NT4GuestR0
 endif
 ifdef VBOX_WITH_LINUX_ADDITIONS
  LIBRARIES += RuntimeLnx32GuestR0 RuntimeLnx32GuestR3
 endif
 LIBRARIES.l4= RuntimeR3L4
 DLLS = VBoxRT
 OTHER_CLEAN = \
	$(PATH_TARGET)/errmsgdata.h \
	$(PATH_TARGET)/errmsgcomdata.h \
	$(PATH_TARGET)/Doxyfile \
	$(PATH_TARGET)/Doxyfile.dep \
	$(PATH_TARGET)/docs.iprt

 SUBDIRS_AFTER = testcase

else
 # Linux target guest runtime only mode.
 LIBRARIES  = RuntimeLnx32GuestR0 RuntimeLnx32GuestR3
endif # only linux guest additions


# global (for now at least)
INCS += include


#
# Unicode Specification reader used to regenerate unidata.cpp.
#
uniread_TEMPLATE = VBOXBLDPROG
uniread_SOURCES  = uniread.cpp

#
# Win64 assembly sources.
#
RuntimeWin64ASM_SOURCES = \
	win64/ASMAtomicBitClear.asm \
	win64/ASMAtomicBitTestAndToggle.asm \
	win64/ASMAtomicBitToggle.asm \
	win64/ASMAtomicReadU64.asm \
	win64/ASMAtomicXchgU16.asm \
	win64/ASMAtomicXchgU8.asm \
	win64/ASMBitFirstClear.asm \
	win64/ASMBitFirstSet.asm \
	win64/ASMGetCS.asm \
	win64/ASMGetDS.asm \
	win64/ASMGetES.asm \
	win64/ASMGetFlags.asm \
	win64/ASMGetFS.asm \
	win64/ASMGetGS.asm \
	win64/ASMGetIDTR.asm \
	win64/ASMGetSS.asm \
	win64/ASMProbeReadByte.asm \
	win64/ASMSetFlags.asm \
	win64/ASMGetDR0.asm \
	win64/ASMGetDR1.asm \
	win64/ASMGetDR2.asm \
	win64/ASMGetDR3.asm \
	win64/ASMGetDR6.asm \
	win64/ASMGetDR7.asm \

#
# RuntimeR3 - Static Runtime for Ring-3 executables.
#
RuntimeR3_TEMPLATE      = VBOXR3EXE
RuntimeR3_SDKS.win      = WINPSDK W2K3DDK
RuntimeR3_DEFS          = IN_RT_R3 IN_SUP_R3 LDR_WITH_NATIVE LDR_WITH_ELF32 LDR_WITH_PE RT_WITH_VBOX
ifdef IPRT_WITH_KLDR
 RuntimeR3_DEFS        += LDR_WITH_KLDR KLDR_USE_CRT
 RuntimeR3_DEFS.debug  += KLDRMOD_STRICT KLDRMODLX_STRICT KLDRMODPE_STRICT KLDRMODNATIVE_STRICT KLDRRDR_STRICT
endif
RuntimeR3_DEFS.l4       = L4
ifdef VBOX_MAIN_RELEASE_LOG ## @todo (dmik): temporary, until RTThreadSelf/RTThreadAdopt are properly updated
 RuntimeR3_DEFS         += RTCRITSECT_STRICT
endif
RuntimeR3_INCS          = \
	$(PATH_ROOT)/src/libs/liblzf-1.51
ifdef IPRT_WITH_KLDR
 RuntimeR3_INCS        += \
	$(IPRT_PATH_KLDR)
endif
RuntimeR3_INCS.l4       = \
	$(PATH_ROOT)/src/libs/libuuid \
	$(L4_INCDIR)
# for iconv.h
RuntimeR3_INCS.freebsd  = \
	/usr/local/include

RuntimeR3_SOURCES       = \
	alloc.cpp \
	alloc/heapsimple.cpp \
	assert.cpp \
	table/avlhcphys.cpp \
	table/avlgcptr.cpp \
	table/avlogcphys.cpp \
	table/avlogcptr.cpp \
	table/avlohcphys.cpp \
	table/avloioport.cpp \
	table/avlpv.cpp \
	table/avlrgcptr.cpp \
	table/avlrogcphys.cpp \
	table/avlrogcptr.cpp \
	table/avlroogcptr.cpp \
	table/avlroioport.cpp \
	table/avlu32.cpp \
	table/avlul.cpp \
	table/table.cpp \
	crc32.cpp \
	crc64.cpp \
	dir.cpp \
	errmsg.cpp \
	fileio.cpp \
	fs.cpp \
	generic/critsect-generic.cpp \
	generic/RTFileCopy-generic.cpp \
	generic/RTLogWriteStdErr-generic.cpp \
	generic/RTLogWriteStdOut-generic.cpp \
	generic/RTLogWriteUser-generic.cpp \
	generic/semfastmutex-generic.cpp \
	generic/spinlock-generic.cpp \
	ldr.cpp \
	ldrFile.cpp \
	ldrNative.cpp \
	ldrELF.cpp \
	ldrEx.cpp \
	ldrPE.cpp \
	log.cpp \
	logcom.cpp \
	logformat.cpp \
	md5.cpp \
	path.cpp \
        req.cpp \
	r3/alloc-ef.cpp \
	r3/alloc.cpp \
	r3/init.cpp \
	r3/process.cpp \
	r3/stream.cpp \
	r3/tcp.cpp \
	RTErrConvertFromErrno.cpp \
	semspingpong.cpp \
	straprintf.cpp \
	strformat.cpp \
	strformatrt.cpp \
	string.cpp \
	strprintf.cpp \
	strspace.cpp \
	strstrip.cpp \
	strtonum.cpp \
	thread.cpp \
	time.cpp \
	timeprog.cpp \
	uni.cpp \
	unidata.cpp \
	utf-16.cpp \
	utf-8.cpp \
	zip.cpp \

ifdef IPRT_WITH_KLDR
 RuntimeR3_SOURCES += \
	ldrkLdr.cpp \
	$(IPRT_PATH_KLDR)/kLdrMod.c \
	$(IPRT_PATH_KLDR)/kLdrModLX.c \
	$(IPRT_PATH_KLDR)/kLdrModPE.c \
	$(IPRT_PATH_KLDR)/kLdrModNative.c \
	$(IPRT_PATH_KLDR)/kLdrMisc.c \
	$(IPRT_PATH_KLDR)/kLdrRdr.c \
	$(IPRT_PATH_KLDR)/kLdrHlpStr.c
# We Implement these using IPRT facilities.
#   $(IPRT_PATH_KLDR)/kLdrHlp.c \
#	$(IPRT_PATH_KLDR)/kLdrHlpHeap.c \
#	$(IPRT_PATH_KLDR)/kLdrHlpSem.c \
#	$(IPRT_PATH_KLDR)/kLdrHlpMem.c \
#	$(IPRT_PATH_KLDR)/kLdrHlpPath.c

endif

# VBox specific stuff.
RuntimeR3_SOURCES += \
	VBox/strformat-vbox.cpp \
	VBox/log-vbox.cpp

RuntimeR3_SOURCES.win = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/semsrw-generic.cpp \
	generic/semnoint-generic.cpp \
	nt/RTErrConvertFromNtStatus.cpp \
	win32/errmsgwin.cpp \
	r3/win32/alloc-win32.cpp \
	r3/win32/dir-win32.cpp \
	r3/win32/fileio-win32.cpp \
	r3/win32/fs-win32.cpp \
	r3/win32/ldrNative-win32.cpp \
	r3/win32/path-win32.cpp \
	r3/win32/process-win32.cpp \
	r3/win32/RTLogWriteDebugger-win32.cpp \
	r3/win32/sched-win32.cpp \
	r3/win32/sems-win32.cpp \
	r3/win32/system-win32.cpp \
	r3/win32/thread-win32.cpp \
	r3/win32/time-win32.cpp \
	r3/win32/timer-win32.cpp \
	r3/win32/utf16locale-win32.cpp \
	r3/win32/utf8-win32.cpp \
	r3/win32/uuid-win32.cpp \
	timesup.cpp \
	win32/RTErrConvertFromWin32.cpp

RuntimeR3_SOURCES.win.amd64 = \
	$(RuntimeWin64ASM_SOURCES)

RuntimeR3_SOURCES.linux = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/RTFileMove-generic.cpp \
	generic/RTLogWriteDebugger-generic.cpp \
	generic/pathhost-generic.cpp \
	r3/linux/sched-linux.cpp \
	r3/linux/uuid-linux.cpp \
	r3/posix/alloc-posix.cpp \
	r3/posix/dir-posix.cpp \
	r3/posix/fileio-posix.cpp \
	r3/posix/fs-posix.cpp \
	r3/posix/ldrNative-posix.cpp \
	r3/posix/path-posix.cpp \
	r3/posix/process-posix.cpp \
	r3/posix/sems-posix.cpp \
	r3/posix/system-posix.cpp \
	r3/posix/thread-posix.cpp \
	r3/posix/time-posix.cpp \
	r3/posix/timer-posix.cpp \
	generic/utf16locale-generic.cpp \
	r3/posix/utf8-posix.cpp \
	timesup.cpp \

RuntimeR3_SOURCES.os2   = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/RTFileMove-generic.cpp \
	generic/RTLogWriteDebugger-generic.cpp \
	generic/pathhost-generic.cpp \
	generic/semsrw-generic.cpp \
	generic/semnoint-generic.cpp \
	generic/sched-generic.cpp \
	generic/uuid-generic.cpp \
	os2/RTErrConvertFromOS2.cpp \
	r3/os2/sems-os2.cpp \
	r3/os2/system-os2.cpp \
	r3/os2/thread-os2.cpp \
	r3/posix/alloc-posix.cpp \
	r3/posix/dir-posix.cpp \
	r3/posix/fileio-posix.cpp \
	r3/posix/fs-posix.cpp \
	r3/posix/ldrNative-posix.cpp \
	r3/posix/path-posix.cpp \
	r3/posix/process-posix.cpp \
	r3/posix/time-posix.cpp \
	r3/posix/timer-posix.cpp \
	generic/utf16locale-generic.cpp \
	r3/posix/utf8-posix.cpp \
	timesup.cpp \

RuntimeR3_SOURCES.darwin = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/RTFileMove-generic.cpp \
	generic/RTLogWriteDebugger-generic.cpp \
	generic/pathhost-generic.cpp \
	r3/darwin/alloc-darwin.cpp \
	r3/posix/dir-posix.cpp \
	r3/posix/fileio-posix.cpp \
	r3/posix/fs-posix.cpp \
	r3/posix/ldrNative-posix.cpp \
	r3/posix/path-posix.cpp \
	r3/posix/process-posix.cpp \
	generic/sched-generic.cpp \
	r3/posix/sems-posix.cpp \
	r3/posix/system-posix.cpp \
	r3/posix/thread-posix.cpp \
	r3/darwin/time-darwin.cpp \
	generic/timer-generic.cpp \
	generic/utf16locale-generic.cpp \
	r3/posix/utf8-posix.cpp \
	r3/linux/uuid-linux.cpp \
	timesup.cpp \

## @todo Make BSD sched.
RuntimeR3_SOURCES.freebsd = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/RTFileMove-generic.cpp \
	generic/RTLogWriteDebugger-generic.cpp \
	generic/pathhost-generic.cpp \
	generic/sched-generic.cpp \
	generic/uuid-generic.cpp \
	r3/freebsd/alloc-freebsd.cpp \
	r3/posix/dir-posix.cpp \
	r3/posix/fileio-posix.cpp \
	r3/posix/fs-posix.cpp \
	r3/posix/ldrNative-posix.cpp \
	r3/posix/path-posix.cpp \
	r3/posix/process-posix.cpp \
	r3/posix/sems-posix.cpp \
	r3/posix/system-posix.cpp \
	r3/posix/thread-posix.cpp \
	r3/posix/time-posix.cpp \
	r3/posix/timer-posix.cpp \
	generic/utf16locale-generic.cpp \
	r3/posix/utf8-posix.cpp \
	timesup.cpp \

## PORTME: Porters add their selection of platform specific files for Ring-3 here.


#
# L4 RuntimeR3 subtarget since L4 headers won't work with VBOXR3.
#
RuntimeR3L4_TEMPLATE      = VBOXR3NP
RuntimeR3L4_DEFS          = IN_RT_R3 IN_SUP_R3 LDR_WITH_NATIVE LDR_WITH_ELF LDR_WITH_PE
RuntimeR3L4_INCS          = $(L4_INCDIR) \
	$(PATH_ROOT)/src/libs/libuuid

RuntimeR3L4_SOURCES = \
	generic/RTDirQueryInfo-generic.cpp \
	generic/RTDirSetTimes-generic.cpp \
	generic/RTFileMove-generic.cpp \
	generic/RTLogWriteDebugger-generic.cpp \
	generic/fs-stubs-generic.cpp \
	generic/pathhost-generic.cpp \
	generic/sched-generic.cpp \
	generic/semsrw-generic.cpp \
	generic/semnoint-generic.cpp \
	l4/l4-errno.cpp \
	l4/process-l4env.cpp \
	l4/sems-l4env.cpp \
	l4/system-l4env.cpp \
	l4/thread-l4env.cpp \
	l4/timer-l4env.cpp \
	generic/utf16locale-generic.cpp \
	l4/utf8-l4env.cpp \
	r3/linux/uuid-linux.cpp \
	r3/posix/alloc-posix.cpp \
	r3/posix/dir-posix.cpp \
	r3/posix/fileio-posix.cpp \
	r3/posix/ldrNative-posix.cpp \
	r3/posix/path-posix.cpp \
	r3/posix/time-posix.cpp \
	timesup.cpp

#
# RuntimeW32GuestR3 - Win32 guest program runtime.
#
RuntimeW32GuestR3_TEMPLATE     = VBOXW32GUESTR3LIB
RuntimeW32GuestR3_SDKS         = $(RuntimeR3_SDKS.win) $(RuntimeR3_SDKS.win.x86) $(RuntimeR3_SDKS)
RuntimeW32GuestR3_DEFS         = $(RuntimeR3_DEFS)
RuntimeW32GuestR3_DEFS.$(BUILD_TYPE) = $(RuntimeR3_DEFS).$(BUILD_TYPE)
RuntimeW32GuestR3_SOURCES      = $(RuntimeR3_SOURCES.win) $(RuntimeR3_SOURCES)
RuntimeW32GuestR3_INCS         = $(RuntimeR3_INCS.win)    $(RuntimeR3_INCS)


#
# RuntimeLnx32GuestR3 - 32-bit Linux guest program runtime.
#
RuntimeLnx32GuestR3_TEMPLATE   = VBOXLNX32GUESTR3LIB
RuntimeLnx32GuestR3_DEFS       = $(RuntimeR3_DEFS.linux)    $(RuntimeR3_DEFS)    IN_GUEST IN_GUEST_R3
RuntimeLnx32GuestR3_DEFS.$(BUILD_TYPE) = $(RuntimeR3_DEFS.$(BUILD_TYPE))
RuntimeLnx32GuestR3_SOURCES    = $(RuntimeR3_SOURCES.linux) $(RuntimeR3_SOURCES)
RuntimeLnx32GuestR3_INCS       = $(RuntimeR3_INCS.linux)    $(RuntimeR3_INCS)


#
# VBoxRT - Shared Object / DLL version.
#
VBoxRT_TEMPLATE                = VBOXR3
VBoxRT_SDKS.win                = WINPSDK W2K3DDK VBOX_NTDLL
VBoxRT_DEFS                    = $(RuntimeR3_DEFS) IN_SUP_R3 IN_SUP_R3
VBoxRT_DEFS.$(BUILD_TYPE)      = $(RuntimeR3_DEFS.$(BUILD_TYPE))
VBoxRT_SOURCES                 = \
	VBox/VBoxRTDeps.cpp \
	$(RuntimeR3_SOURCES)
VBoxRT_SOURCES.$(BUILD_TARGET) = $(RuntimeR3_SOURCES.$(BUILD_TARGET))
VBoxRT_SOURCES.$(BUILD_TARGET).$(BUILD_TARGET_ARCH) = $(RuntimeR3_SOURCES.$(BUILD_TARGET).$(BUILD_TARGET_ARCH))
VBoxRT_SOURCES.win            += r3/win32/dllmain-win32.cpp
VBoxRT_SOURCES.win.amd64      += r3/win64/VBoxRT-win64.def
VBoxRT_INCS                    = $(RuntimeR3_INCS)
VBoxRT_INCS.$(BUILD_TARGET)    = $(RuntimeR3_INCS.$(BUILD_TARGET))
VBoxRT_INCS.$(BUILD_TARGET).$(BUILD_TARGET_ARCH) = $(RuntimeR3_INCS.$(BUILD_TARGET).$(BUILD_TARGET_ARCH))
VBoxRT_LIBS                    = \
	$(PATH_LIB)/SUPR3$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/VBox-liblzf$(VBOX_SUFF_LIB)
VBoxRT_LIBS.darwin             = \
	iconv
VBoxRT_LIBS.freebsd            = \
	iconv
VBoxRT_LIBPATH.freebsd         = \
	/usr/local/lib	
VBoxRT_LDFLAGS.darwin          = -framework IOKit
ifdef VBOX_USE_VCC80
VBoxRT_LDFLAGS.win             = /MANIFEST
endif
VBoxRT_LDFLAGS.l4              = \
	-Wl,-whole-archive \
	$(PATH_LIB)/RuntimeR3L4$(VBOX_SUFF_LIB) \
	-Wl,-no-whole-archive
ifeq ($(BUILD_TARGET),l4)
VBoxRT_LIBS                   += \
	$(L4_LIBDIR)/libl4sys.a \
	$(L4_LIBDIR)/libl4sys.p.a
endif
VBoxRT_LIBS.l4                 = \
	$(PATH_LIB)/VBox-uuid$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/RuntimeR3L4$(VBOX_SUFF_LIB)

#
# RuntimeR3NoCRTGCC - CRT replacement lib for VBoxREM.
# This is used together with VBoxRT.
#
RuntimeR3NoCRTGCC_TEMPLATE = VBOXNOCRTGCC
RuntimeR3NoCRTGCC_SOURCES  = \
	\
	math/ceill.asm \
	math/cosl.asm \
	math/fabs.asm \
	math/fabsf.asm \
	math/fabsl.asm \
	math/floor.asm \
	math/floorf.asm \
	math/floorl.asm \
	math/ldexpl.asm \
	math/llrint.asm \
	math/llrintf.asm \
	math/llrintl.asm \
	math/logl.asm \
	math/lrint.asm \
	math/lrintf.asm \
	math/lrintl.asm \
	math/remainder.asm \
	math/remainderf.asm \
	math/remainderl.asm \
	math/sinl.asm \
	math/tanl.asm \
	math/trunc.asm \
	math/truncf.asm \
	math/truncl.asm \
	\
	misc/setjmp.asm \
	\
	string/memchr.asm \
	string/memchr_alias.c \
	string/memcpy.asm \
	string/memcpy_alias.c \
	string/mempcpy.asm \
	string/memset.asm \
	string/memset_alias.c

RuntimeR3NoCRTGCC_SOURCES.x86 = \
	math/x86/fenv-x86.c


## @todo stop using the old memcpy.c and memset.c code.

#
# RuntimeR0 - Ring0 library for VMMR0.
#
RuntimeR0_TEMPLATE      = VBOXR0
RuntimeR0_DEFS          = IN_RT_R0 RT_WITH_VBOX
RuntimeR0_SOURCES       = \
	assert.cpp \
	table/avlhcphys.cpp \
	table/avlgcptr.cpp \
	table/avlogcphys.cpp \
	table/avlohcphys.cpp \
	table/avlpv.cpp \
	table/avlrogcphys.cpp \
	table/avlogcptr.cpp \
	table/avlrogcptr.cpp \
	table/avlroogcptr.cpp \
	table/avloioport.cpp \
	table/avlroioport.cpp \
	logcom.cpp \
	logformat.cpp \
	strformat.cpp \
	strformatrt.cpp \
	strprintf.cpp \
	string/memchr.cpp \
	string/memcmp.cpp \
	string/memcpy.cpp \
	string/memset.cpp \
	string/strcpy.cpp \
	string/strlen.cpp \
	string/strncmp.cpp \
	string/strpbrk.cpp \
	VBox/strformat-vbox.cpp \
	timesup.cpp

RuntimeR0_SOURCES.win.amd64 = $(RuntimeWin64ASM_SOURCES)

RuntimeR0_SOURCES.os2 = \
	os2/RTErrConvertFromOS2.cpp \
	os2/sys0.asm

string/memchr.cpp_CXXFLAGS.win = -Oi-
string/memcmp.cpp_CXXFLAGS.win = -Oi-
string/memcpy.cpp_CXXFLAGS.win = -Oi-
string/memset.cpp_CXXFLAGS.win = -Oi-
string/strcpy.cpp_CXXFLAGS.win = -Oi-
string/strlen.cpp_CXXFLAGS.win = -Oi-

ifeq ($(BUILD_TARGET),l4)
RuntimeR0Drv_BLD_TRG      = linux
RuntimeR0Drv_BLD_TRG_ARCH = x86
RuntimeR0Drv_BLD_TRG_CPU  = i386
endif

#
# RuntimeR0Drv - Ring0 library for host drivers.
#
RuntimeR0Drv_TEMPLATE   = VBOXR0DRV
RuntimeR0Drv_SDKS.win   = W2K3DDK WINPSDKINCS
RuntimeR0Drv_DEFS       = IN_RT_R0 RT_WITH_VBOX RT_WITHOUT_NOCRT_WRAPPERS
RuntimeR0Drv_DEFS.win   = IN_SUP_R0
RuntimeR0Drv_DEFS.linux = MODULE KBUILD_MODNAME=KBUILD_STR\(vboxdrv\) KBUILD_BASENAME=KBUILD_STR\(vboxdrv\) IN_SUP_R0 # why is IN_SUP_R0 here? because!

RuntimeR0Drv_INCS.linux = \
	r0drv/linux

RuntimeR0Drv_SOURCES    = \
	assert.cpp \
	log.cpp \
	logcom.cpp \
	logformat.cpp \
	string/strpbrk.cpp \
	strformat.cpp \
	strformatrt.cpp \
	strtonum.cpp \
	strprintf.cpp \
	VBox/strformat-vbox.cpp \
	r0drv/alloc-r0drv.cpp \
	r0drv/initterm-r0drv.cpp \
	generic/RTLogWriteStdErr-stub-generic.cpp \
	generic/RTLogWriteStdOut-stub-generic.cpp \
	generic/RTLogWriteUser-generic.cpp \
	VBox/log-vbox.cpp \
	table/avlpv.cpp \
	crc32.cpp \
	crc64.cpp

RuntimeR0Drv_SOURCES.linux = \
	alloc/heapsimple.cpp \
	r0drv/linux/alloc-r0drv-linux.c \
	r0drv/linux/initterm-r0drv-linux.c \
	r0drv/linux/process-r0drv-linux.c \
	r0drv/linux/RTLogWriteDebugger-r0drv-linux.c \
	r0drv/linux/semaphore-r0drv-linux.c \
	r0drv/linux/spinlock-r0drv-linux.c \
	r0drv/linux/thread-r0drv-linux.c

RuntimeR0Drv_SOURCES.win = \
	nt/RTErrConvertFromNtStatus.cpp \
	r0drv/nt/alloc-r0drv-nt.cpp \
	r0drv/nt/initterm-r0drv-nt.cpp \
	r0drv/nt/process-r0drv-nt.cpp \
	r0drv/nt/RTLogWriteDebugger-r0drv-nt.cpp \
	r0drv/nt/semaphore-r0drv-nt.cpp \
	r0drv/nt/spinlock-r0drv-nt.cpp \
	r0drv/nt/thread-r0drv-nt.cpp \
	string/strncmp.cpp

RuntimeR0Drv_SOURCES.win.amd64 = \
	$(RuntimeWin64ASM_SOURCES)

RuntimeR0Drv_SOURCES.darwin = \
	RTErrConvertFromErrno.cpp \
	string/memchr.asm \
	r0drv/memobj-r0drv.cpp \
	r0drv/darwin/alloc-r0drv-darwin.cpp \
	r0drv/darwin/memobj-r0drv-darwin.cpp \
	r0drv/darwin/initterm-r0drv-darwin.cpp \
	r0drv/darwin/process-r0drv-darwin.cpp \
	r0drv/darwin/RTLogWriteDebugger-r0drv-darwin.cpp \
	r0drv/darwin/semaphore-r0drv-darwin.cpp \
	r0drv/darwin/spinlock-r0drv-darwin.cpp \
	r0drv/darwin/thread-r0drv-darwin.cpp \
	r0drv/darwin/thread2-r0drv-darwin.cpp \
	r0drv/darwin/time-r0drv-darwin.cpp \
	thread.cpp \
	generic/timer-generic.cpp \

RuntimeR0Drv_SOURCES.os2 = \
	os2/RTErrConvertFromOS2.cpp \
	os2/sys0.asm

## PORTME: Porters create and add their selection of platform specific Ring-0 Driver files here.

#
# RuntimeW32GuestR0 - Win32 guest driver runtime.
#
RuntimeW32GuestR0_TEMPLATE      = VBOXW32GUESTR0LIB
RuntimeW32GuestR0_SDKS          = W2K3DDKX86 WINPSDKINCS
RuntimeW32GuestR0_DEFS          = $(RuntimeR0Drv_DEFS.win) $(RuntimeR0Drv_DEFS.win.x86) $(RuntimeR0Drv_DEFS)
RuntimeW32GuestR0_INCS          = $(RuntimeR0Drv_INCS.win) $(RuntimeR0Drv_INCS.win.x86) $(RuntimeR0Drv_INCS)
RuntimeW32GuestR0_SOURCES       = \
	$(RuntimeR0Drv_SOURCES) \
	$(RuntimeR0Drv_SOURCES.win) \
	VBox/logbackdoor.cpp \
	logR0.cpp
ifdef VBOX_USE_VCC80
RuntimeW32GuestR0_SOURCES += \
	string/memchr.cpp \
	string/memcmp.cpp \
	string/memcpy.cpp \
	string/memset.cpp \
	string/strlen.cpp
endif


#
# RuntimeW32NT4GuestR0 - Win32 NT4 guest driver runtime.
#
RuntimeW32NT4GuestR0_TEMPLATE      = VBOXW32GUESTR0LIB
RuntimeW32NT4GuestR0_SDKS          = $(RuntimeW32GuestR0_SDKS)
RuntimeW32NT4GuestR0_DEFS          = $(RuntimeW32GuestR0_DEFS) IPRT_TARGET_NT4
RuntimeW32NT4GuestR0_INCS          = $(RuntimeW32GuestR0_INCS)
RuntimeW32NT4GuestR0_SOURCES       = $(RuntimeW32GuestR0_SOURCES)


#
# RuntimeLnx32GuestR0 - 32-bit Linux guest driver runtime.
#
RuntimeLnx32GuestR0_TEMPLATE      = VBOXLNX32GUESTR0
RuntimeLnx32GuestR0_DEFS          = $(RuntimeR0Drv_DEFS.linux) $(RuntimeR0Drv_DEFS) IN_GUEST IN_GUEST_R0
RuntimeLnx32GuestR0_INCS          = $(RuntimeR0Drv_INCS.linux) $(RuntimeR0Drv_INCS)
RuntimeLnx32GuestR0_SOURCES       = \
	$(RuntimeR0Drv_SOURCES) \
	$(RuntimeR0Drv_SOURCES.linux) \
	VBox/logbackdoor.cpp

#
# RuntimeGC - Guest context library.
#
RuntimeGC_TEMPLATE      = VBOXGC
RuntimeGC_DEFS          = IN_RT_GC RT_WITH_VBOX
RuntimeGC_SOURCES       = \
	table/avlogcphys.cpp \
	table/avlrogcphys.cpp \
	table/avlogcptr.cpp \
	table/avlohcphys.cpp \
	table/avlrogcptr.cpp \
	table/avlroogcptr.cpp \
	table/avloioport.cpp \
	table/avlroioport.cpp \
	assert.cpp \
	log.cpp \
	logcom.cpp \
	logformat.cpp \
	string/strpbrk.cpp \
	string/strncmp.cpp \
	strformat.cpp \
	strformatrt.cpp \
	strprintf.cpp \
	VBox/strformat-vbox.cpp \
	timesup.cpp \
	string/memchr.cpp \
	string/memcmp.cpp \
	string/memcpy.cpp \
	string/memset.cpp \
	string/strcpy.cpp \
	string/strlen.cpp

ifeq ($(VBOX_LDR_FMT32),lx)
RuntimeGC_SOURCES += os2/sys0.asm
endif


#
# Static library for new & delete for the electric fence.
#
RuntimeEFCPP_TEMPLATE   = $(RuntimeR3_TEMPLATE)
RuntimeEFCPP_SDKS       = $(RuntimeR3_SDKS)
RuntimeEFCPP_SDKS.$(BUILD_TARGET) = $(RuntimeR3_SDKS.$(BUILD_TARGET))
RuntimeEFCPP_DEFS       = $(RuntimeR3_DEFS)
RuntimeEFCPP_DEFS.$(BUILD_TARGET) = $(RuntimeR3_DEFS.$(BUILD_TARGET))
RuntimeEFCPP_INCS       = $(RuntimeR3_INCS)
RuntimeEFCPP_INCS.$(BUILD_TARGET) = $(RuntimeR3_INCS.$(BUILD_TARGET))
RuntimeEFCPP_SOURCES    = r3/alloc-ef-cpp.cpp



#
# errmsg.cpp depends on a generated header.
#
errmsg.cpp_DEPS = $(PATH_TARGET)/errmsgdata.h
errmsg.cpp_INCS = $(PATH_TARGET)

win32/errmsgwin.cpp_DEPS = $(PATH_TARGET)/errmsgcomdata.h
win32/errmsgwin.cpp_INCS = $(PATH_TARGET)

linux/errmsglinux.cpp_DEPS = $(PATH_TARGET)/errmsgxpcomdata.h
linux/errmsglinux.cpp_INCS = $(PATH_TARGET)


# Generate the rules
include $(PATH_KBUILD)/footer.kmk


#
# Generate the status code data.
#
$(PATH_TARGET)/errmsgdata.h: errmsg.sed $(DEPTH)/include/iprt/err.h $(DEPTH)/include/VBox/err.h
	$(call MSG_L1,Generating $@)
	$(QUIET)$(MKDIR) -p $(PATH_TARGET)
	$(QUIET)$(SED) -f $< $(filter %.h,$^) > $@
## @todo r=bird: rename this to indicate that it's not only COM errors, but all win32/64 errors.
$(PATH_TARGET)/errmsgcomdata.h: errmsgcom.sed $(PATH_SDK_WINPSDK_INC)/WinError.h
	$(call MSG_L1,Generating $@)
	$(QUIET)$(MKDIR) -p $(PATH_TARGET)
	$(QUIET)$(SED) -f $< $(filter %.h,$^) > $@
$(PATH_TARGET)/errmsgxpcomdata.h: errmsgxpcom.sed $(DEPTH)/src/libs/xpcom18a4/xpcom/base/nsError.h
	$(call MSG_L1,Generating $@)
	$(QUIET)$(MKDIR) -p $(PATH_TARGET)
	$(QUIET)$(SED) -f $< $(filter %.h,$^) > $@


#
# Aliases for .cpp.h files so we can more easily do syntax checking from the editor.
#
ldrELFRelocatable.cpp.o: ldrELF.o
ldrELFRelocatable.cpp.obj: ldrELF.obj


#
# Doxygen documentation.
#
DOXYGEN_INPUT_DIRS = \
	$(PATH_ROOT)/include/iprt \
	$(PATH_ROOT)/include/iprt/nocrt \
	$(PATH_ROOT)/include/iprt/nocrt/x86 \
	$(PATH_ROOT)/include/iprt/nocrt/amd64 \
	include/internal \
	string \
	table \
	$(foreach dir, . r3 r0drv,\
		$(dir) \
		$(dir)/darwin \
		$(dir)/l4 \
		$(dir)/linux \
		$(dir)/nt \
		$(dir)/os2 \
		$(dir)/win \
		$(dir)/win32 \
		$(dir)/win64 \
		$(dir)/generic \
		)

# These must come first in order to make things look nice.
DOXYGEN_INPUT_FIRST =\
	$(PATH_ROOT)/include/iprt/cdefs.h \
	$(PATH_ROOT)/include/iprt/types.h \
	$(PATH_ROOT)/include/iprt/runtime.h \
	$(PATH_ROOT)/include/iprt/param.h \
	$(PATH_ROOT)/include/iprt/assert.h \
	$(PATH_ROOT)/include/iprt/asm.h \

DOXYGEN_INPUT := \
	$(filter-out %.cpp.h, $(sort $(wildcard $(addsuffix /*.h, $(DOXYGEN_INPUT_DIRS)))) ) \
	$(foreach dir, $(DOXYGEN_INPUT_DIRS), $(wildcard $(dir)/*.cpp $(dir)/.c $(dir)/.asm))
DOXYGEN_INPUT := \
	$(DOXYGEN_INPUT_FIRST) \
	$(filter-out $(DOXYGEN_INPUT_FIRST), $(DOXYGEN_INPUT))


DOXYGEN_OUTPUT = $(PATH_OUT)/docs/iprt

-include $(PATH_TARGET)/Doxyfile.dep

# Generate the Doxyfile
$(PATH_TARGET)/Doxyfile: Doxyfile \
		$(comp-vars DOXYGEN_INPUT,DOXYGEN_INPUT_PREV,FORCE) \
		$(comp-vars DOXYGEN_OUTPUT,DOXYGEN_OUTPUT_PREV,FORCE) \
		| $(call DIRDEP, $(PATH_TARGET))
	$(RM) -f $@ $@.tmp $(PATH_TARGET)/Doxyfile.dep
	$(CP) -f Doxyfile $@.tmp
	$(APPEND) $@.tmp
	$(APPEND) $@.tmp "OUTPUT_DIRECTORY = $(DOXYGEN_OUTPUT)"
	$(APPEND) $@.tmp "WARN_LOGFILE = $(DOXYGEN_OUTPUT)/errors"
	$(APPEND) $@.tmp "INCLUDE_PATH = $(PATH_ROOT)/include include . table"
	$(APPEND) $@.tmp "INCLUDE_FILE_PATTERNS = *.cpp.h"
	$(APPEND) $@.tmp
	$(APPEND) $@.tmp "INPUT = $(DOXYGEN_INPUT)"
	$(APPEND) $@.tmp
	$(MV) -f $@.tmp $@
	@$(APPEND) $(PATH_TARGET)/Doxyfile.dep "DOXYGEN_OUTPUT_PREV = $(DOXYGEN_OUTPUT)"
	@$(APPEND) $(PATH_TARGET)/Doxyfile.dep "DOXYGEN_INPUT_PREV = $(DOXYGEN_INPUT)"

# Create the output directory.
$(call DIRDEP, $(DOXYGEN_OUTPUT)):
	$(MKDIR) -p $@

# Do the actual job.
$(PATH_TARGET)/docs.iprt: $(PATH_TARGET)/Doxyfile $(DOXYGEN_INPUT) | $(call DIRDEP, $(DOXYGEN_OUTPUT))
	$(RM) -f $(wildcard $(DOXYGEN_OUTPUT)/html/*) $(PATH_TARGET)/docs.iprt
	doxygen $(PATH_TARGET)/Doxyfile
	$(APPEND) $(PATH_TARGET)/docs.iprt

# alias
docs: $(PATH_TARGET)/docs.iprt

test:
	@echo test-$(comp-vars DOXYGEN_OUTPUT,DOXYGEN_OUTPUT_PREV,FORCE)
	@echo $(DOXYGEN_OUTPUT)
	@echo $(DOXYGEN_OUTPUT_PREV)

