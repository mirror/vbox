/* $Id$ */
/** @file
 * IEM - Native Recompiler Assembly Helpers, ARM64 variant.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*       Header Files                                                                                                             *
*********************************************************************************************************************************/
#include <iprt/asmdefs-arm.h>
#include <iprt/x86.h>


BEGINCODE
/**
 * This does the epilogue of a TB, given the RBP for the frame and eax value to return.
 *
 * @param    pFrame  (x0)       The frame pointer.
 * @param    rc      (w1)       The return value.
 *
 * @note This doesn't really work for MSC since xmm6 thru xmm15 are non-volatile
 *       and since we don't save them in the TB prolog we'll potentially return
 *       with different values if any functions on the calling stack uses them
 *       as they're unlikely to restore them till they return.
 *
 *       For the GCC calling convention all xmm registers are volatile and the
 *       only worry would be someone fiddling the control bits of MXCSR or FCW
 *       without restoring them.  This is highly unlikely, unless we're doing
 *       it ourselves, I think.
 */
        .p2align        2
        .private_extern NAME(iemNativeTbLongJmp)
        .globl          NAME(iemNativeTbLongJmp)
NAME(iemNativeTbLongJmp):

        ;
        ; This must exactly match what iemNativeEmitEpilog does.
        ;
        sub     sp, x0, #0x50
        ldp     x19, x20, [sp, #0x00]
        ldp     x21, x22, [sp, #0x10]
        ldp     x23, x24, [sp, #0x20]
        ldp     x25, x26, [sp, #0x30]
        ldp     x27, x28, [sp, #0x40]
        ldp     x29, x30, [sp, #0x50]       /* the pFrame address points to this entry */
        add     sp, sp, #0x60
        mov     w0, w1                      /* The return value */
#ifdef RT_OS_DARWIN
        retab
#else
        ret
#endif
        brk #1

