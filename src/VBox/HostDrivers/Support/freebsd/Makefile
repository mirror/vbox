# $Id$
## @file
# Makefile for the VirtualBox FreeBSD Host Driver.
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

KMOD = vboxdrv

CFLAGS += -DRT_OS_FREEBSD -DIN_RING0 -DIN_RT_R0 -DIN_SUP_R0 -DVBOX -DRT_WITH_VBOX -Iinclude -I. -Ir0drv -w -DVBOX_WITH_HARDENING -DVBOX_WITH_64_BITS_GUESTS

.if (${MACHINE_ARCH} == "i386")
 CFLAGS += -DRT_ARCH_X86
.elif (${MACHINE_ARCH} == "amd64")
 CFLAGS += -DRT_ARCH_AMD64
.endif

SRCS = \
	freebsd/SUPDrv-freebsd.c \
	SUPDrv.c \
	alloc/heapsimple.c \
	common/err/RTErrConvertFromErrno.c \
	common/err/RTErrConvertToErrno.c \
	common/log/log.c \
	common/log/logellipsis.c \
	common/log/logrel.c \
	common/log/logrelellipsis.c \
	common/log/logcom.c \
	common/log/logformat.c \
	common/misc/handletable.c \
	common/misc/handletablectx.c \
	common/misc/once.c \
	common/misc/thread.c \
	common/string/strformat.c \
	common/string/strformatrt.c \
	common/string/strformattype.c \
	common/string/strprintf.c \
	common/string/strtonum.c \
	common/string/memchr.c \
	common/string/string.c \
	common/rand/rand.c \
	common/rand/randadv.c \
	common/rand/randparkmiller.c \
	common/path/RTPathStripFilename.c \
	common/checksum/crc32.c \
	common/checksum/ipv4.c \
	common/table/avlpv.c \
	generic/uuid-generic.c \
	generic/RTAssertShouldPanic-generic.c \
	generic/RTLogWriteDebugger-generic.c \
	generic/RTLogWriteStdOut-stub-generic.c \
	generic/RTLogWriteStdErr-stub-generic.c \
	generic/RTLogWriteUser-generic.c \
	generic/RTRandAdvCreateSystemFaster-generic.c \
	generic/RTRandAdvCreateSystemTruer-generic.c \
	generic/RTTimerCreate-generic.c \
	generic/timer-generic.c \
	generic/mppresent-generic.c \
	r0drv/alloc-r0drv.c \
	r0drv/initterm-r0drv.c \
	r0drv/powernotification-r0drv.c \
	r0drv/freebsd/assert-r0drv-freebsd.c \
	r0drv/freebsd/alloc-r0drv-freebsd.c \
	r0drv/freebsd/initterm-r0drv-freebsd.c \
	r0drv/freebsd/memobj-r0drv-freebsd.c \
	r0drv/freebsd/memuserkernel-r0drv-freebsd.c \
	r0drv/freebsd/mp-r0drv-freebsd.c \
	r0drv/freebsd/process-r0drv-freebsd.c \
	r0drv/freebsd/semevent-r0drv-freebsd.c \
	r0drv/freebsd/semeventmulti-r0drv-freebsd.c \
	r0drv/freebsd/semfastmutex-r0drv-freebsd.c \
	r0drv/freebsd/spinlock-r0drv-freebsd.c \
	r0drv/freebsd/thread-r0drv-freebsd.c \
	r0drv/freebsd/thread2-r0drv-freebsd.c \
	r0drv/freebsd/time-r0drv-freebsd.c \
	r0drv/generic/semspinmutex-r0drv-generic.c \
	r0drv/generic/mpnotification-r0drv-generic.c \
	r0drv/generic/RTMpIsCpuWorkPending-r0drv-generic.c \
	r0drv/memobj-r0drv.c \
	VBox/log-vbox.c \
	VBox/strformat-vbox.c

SRCS += device_if.h bus_if.h

.include <bsd.kmod.mk>

# HACK ALERT: The generated object files are not stored in the same directories as the
#             C files but in the current directory. Linking will fail because
#             it searches in the given directories for object files but doesn't
#             find them. Redefine the varaiable used for linking with the correct paths
#             to the files. KEEP IN SYNC WITH ABOVE DEFINITION!!!
OBJS = \
	SUPDrv-freebsd.o \
	SUPDrv.o \
	heapsimple.o \
	RTErrConvertFromErrno.o \
	RTErrConvertToErrno.o \
	log.o \
	logellipsis.o \
	logrel.o \
	logrelellipsis.o \
	logcom.o \
	logformat.o \
	handletable.o \
	handletablectx.o \
	once.o \
	thread.o \
	strformat.o \
	strformatrt.o \
	strformattype.o \
	strprintf.o \
	strtonum.o \
	memchr.o \
	string.o \
	rand.o \
	randadv.o \
	randparkmiller.o \
	RTPathStripFilename.o \
	crc32.o \
	ipv4.o \
	avlpv.o \
	uuid-generic.o \
	RTAssertShouldPanic-generic.o \
	RTLogWriteDebugger-generic.o \
	RTLogWriteStdOut-stub-generic.o \
	RTLogWriteStdErr-stub-generic.o \
	RTLogWriteUser-generic.o \
	RTRandAdvCreateSystemFaster-generic.o \
	RTRandAdvCreateSystemTruer-generic.o \
	RTTimerCreate-generic.o \
	timer-generic.o \
	mppresent-generic.o \
	alloc-r0drv.o \
	initterm-r0drv.o \
	powernotification-r0drv.o \
	assert-r0drv-freebsd.o \
	alloc-r0drv-freebsd.o \
	initterm-r0drv-freebsd.o \
	memobj-r0drv-freebsd.o \
	memuserkernel-r0drv-freebsd.o \
	mp-r0drv-freebsd.o \
	process-r0drv-freebsd.o \
	semevent-r0drv-freebsd.o \
	semeventmulti-r0drv-freebsd.o \
	semfastmutex-r0drv-freebsd.o \
	spinlock-r0drv-freebsd.o \
	thread-r0drv-freebsd.o \
	thread2-r0drv-freebsd.o \
	time-r0drv-freebsd.o \
	semspinmutex-r0drv-generic.o \
	mpnotification-r0drv-generic.o \
	RTMpIsCpuWorkPending-r0drv-generic.o \
	memobj-r0drv.o \
	log-vbox.o \
	strformat-vbox.o

