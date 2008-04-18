/** @file
 *
 * VBox disassembler:
 * Tables.header
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __disasmtable_h__
#define __disasmtable_h__

#include <VBox/dis.h>

extern const OPCODE g_aOneByteMapX86[256];
extern const OPCODE g_aTwoByteMapX86[256];

/** Two byte opcode map with prefix 0x66 */
extern const OPCODE g_aTwoByteMapX86_PF66[256];

/** Two byte opcode map with prefix 0xF2 */
extern const OPCODE g_aTwoByteMapX86_PFF2[256];

/** Two byte opcode map with prefix 0xF3 */
extern const OPCODE g_aTwoByteMapX86_PFF3[256];

/** Opcode extensions (Group tables)
 * @{ */
extern const OPCODE g_aMapX86_Group1[8*4];
extern const OPCODE g_aMapX86_Group2[8*6];
extern const OPCODE g_aMapX86_Group3[8*2];
extern const OPCODE g_aMapX86_Group4[8];
extern const OPCODE g_aMapX86_Group5[8];
extern const OPCODE g_aMapX86_Group6[8];
extern const OPCODE g_aMapX86_Group7_mem[8];
extern const OPCODE g_aMapX86_Group7_mod11_rm000[8];
extern const OPCODE g_aMapX86_Group7_mod11_rm001[8];
extern const OPCODE g_aMapX86_Group8[8];
extern const OPCODE g_aMapX86_Group9[8];
extern const OPCODE g_aMapX86_Group10[8];
extern const OPCODE g_aMapX86_Group11[8*2];
extern const OPCODE g_aMapX86_Group12[8*2];
extern const OPCODE g_aMapX86_Group13[8*2];
extern const OPCODE g_aMapX86_Group14[8*2];
extern const OPCODE g_aMapX86_Group15_mem[8];
extern const OPCODE g_aMapX86_Group15_mod11_rm000[8];
extern const OPCODE g_aMapX86_Group16[8];
extern const OPCODE g_aMapX86_NopPause[2];
/** @} */

/** 3DNow! map (0x0F 0x0F prefix) */
extern const OPCODE g_aTwoByteMapX86_3DNow[256];

/** Floating point opcodes starting with escape byte 0xDF
 * @{ */
extern const OPCODE g_aMapX86_EscF0_Low[8];
extern const OPCODE g_aMapX86_EscF0_High[16*4];
extern const OPCODE g_aMapX86_EscF1_Low[8];
extern const OPCODE g_aMapX86_EscF1_High[16*4];
extern const OPCODE g_aMapX86_EscF2_Low[8];
extern const OPCODE g_aMapX86_EscF2_High[16*4];
extern const OPCODE g_aMapX86_EscF3_Low[8];
extern const OPCODE g_aMapX86_EscF3_High[16*4];
extern const OPCODE g_aMapX86_EscF4_Low[8];
extern const OPCODE g_aMapX86_EscF4_High[16*4];
extern const OPCODE g_aMapX86_EscF5_Low[8];
extern const OPCODE g_aMapX86_EscF5_High[16*4];
extern const OPCODE g_aMapX86_EscF6_Low[8];
extern const OPCODE g_aMapX86_EscF6_High[16*4];
extern const OPCODE g_aMapX86_EscF7_Low[8];
extern const OPCODE g_aMapX86_EscF7_High[16*4];

extern PCOPCODE     g_paMapX86_FP_Low[8];
extern PCOPCODE     g_paMapX86_FP_High[8];
/** @} */

#endif /* !__disasmtable_h__ */

