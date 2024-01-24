/** @file
 * IPRT - Assembly Memory Functions, x86 32-bit Watcom C/C++ pragma aux.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_asm_mem_watcom_x86_32_h
#define IPRT_INCLUDED_asm_mem_watcom_x86_32_h
/* no pragma once */

#ifndef IPRT_INCLUDED_asm_mem_h
# error "Don't include this header directly."
#endif

#ifndef __FLAT__
# error "Only works with flat pointers! (-mf)"
#endif

/*
 * Note! The #undef that preceds the #pragma aux statements is for undoing
 *       the mangling, because the symbol in #pragma aux [symbol] statements
 *       doesn't get subjected to preprocessing.  This is also why we include
 *       the watcom header at both the top and the bottom of asm.h file.
 */

#undef      ASMMemZero32
#ifdef IPRT_ASM_WATCOM_X86_32_WITH_PRAGMAS
#pragma aux ASMMemZero32 = \
    "shr ecx, 2" \
    "xor eax, eax" \
    "rep stosd"  \
    parm [edi] [ecx] \
    modify exact [eax ecx edi];
#endif

#undef      ASMMemFill32
#ifdef IPRT_ASM_WATCOM_X86_32_WITH_PRAGMAS
#pragma aux ASMMemFill32 = \
    "shr ecx, 2" \
    "rep stosd"  \
    parm [edi] [ecx] [eax]\
    modify exact [ecx edi];
#endif

#undef      ASMProbeReadByte
#ifdef IPRT_ASM_WATCOM_X86_32_WITH_PRAGMAS
#pragma aux ASMProbeReadByte = \
    "mov al, [ecx]" \
    parm [ecx] \
    value [al] \
    modify exact [al];
#endif

#endif /* !IPRT_INCLUDED_asm_mem_watcom_x86_32_h */

