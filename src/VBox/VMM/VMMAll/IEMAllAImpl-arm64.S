/* $Id$ */
/** @file
 * IEM - Instruction Implementation in Assembly, ARM64 variant.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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


#include <iprt/asmdefs-arm.h>


BEGINCODE
        .p2align        2
        .private_extern NAME(iemAImpl_placeholder)
        .globl          NAME(iemAImpl_placeholder)
NAME(iemAImpl_placeholder):
        brk #1
        ret

/* Some sketches.

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
        .p2align        2
        .private_extern NAME(iemAImpl_xchg_u8_locked)
        .globl          NAME(iemAImpl_xchg_u8_locked)
NAME(iemAImpl_xchg_u8_locked):
        ldrb    w2, [x1]
        swpalb  w2, w2, [x0]
        strb    w2, [x1]
        ret

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_locked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
        .p2align        2
        .private_extern NAME(iemAImpl_xchg_u16_locked)
        .globl          NAME(iemAImpl_xchg_u16_locked)
NAME(iemAImpl_xchg_u16_locked):
        ldrh    w2, [x1]
        swpalh  w2, w2, [x0]
        strh    w2, [x1]
        ret

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_locked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_locked,(uint64_t *pu64Mem, uint64_t *pu64Reg));

*/
