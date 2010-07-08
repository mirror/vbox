/* $Id$ */
/** @file
 * VMMDev - Testing Extensions.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ___VBox_VMMDevTesting_h
#define ___VBox_VMMDevTesting_h

#include <VBox/types.h>

/** The base address of the MMIO range used for testing.
 * This is intentionally put at the 2nd page above 1M so that it can be
 * accessed from both real (!A20) and protected mode. */
#define VMMDEV_TESTING_MMIO_BASE        UINT32_C(0x00101000)
/** The size of the MMIO range used for testing.  */
#define VMMDEV_TESTING_MMIO_SIZE        UINT32_C(0x00001000)
/** The NOP MMIO register - 124 RW. */
#define VMMDEV_TESTING_MMIO_NOP         (VMMDEV_TESTING_MMIO_BASE + 0x000)
/** The XXX MMIO register - 124 RW. */
#define VMMDEV_TESTING_MMIO_TODO        (VMMDEV_TESTING_MMIO_BASE + 0x004)
/** The real mode selector to use.
 * @remarks Requires that the A20 gate is enabled. */
#define VMMDEV_TESTING_MMIO_RM_SEL       0xffff
/** Calculate the real mode offset of a MMIO register. */
#define VMMDEV_TESTING_MMIO_RM_OFF(val)  ((val) - 0xffff0)

/** The base port of the I/O range used for testing. */
#define VMMDEV_TESTING_IOPORT_BASE      0x0510
/** The number of I/O ports reserved for testing. */
#define VMMDEV_TESTING_IOPORT_COUNT     0x0010
/** The NOP I/O port - 124 RW. */
#define VMMDEV_TESTING_IOPORT_NOP       (VMMDEV_TESTING_IOPORT_BASE + 0)
/** The low nanosecond timestamp - 4 RO.  */
#define VMMDEV_TESTING_IOPORT_TS_LOW    (VMMDEV_TESTING_IOPORT_BASE + 1)
/** The high nanosecond timestamp - 4 RO.  Read this after the low one!  */
#define VMMDEV_TESTING_IOPORT_TS_HIGH   (VMMDEV_TESTING_IOPORT_BASE + 2)

/** What the NOP accesses returns. */
#define VMMDEV_TESTING_NOP_RET          UINT32_C(0x64726962) /* bird */

#endif

