/* $Id$ */
/** @file
 * VBox disassembler - Internal header.
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

#ifndef VBOX_INCLUDED_SRC_DisasmInternal_armv8_h
#define VBOX_INCLUDED_SRC_DisasmInternal_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/log.h>

#include <iprt/param.h>
#include "DisasmInternal.h"


/** @addtogroup grp_dis_int Internals.
 * @ingroup grp_dis
 * @{
 */

/** @name Index into g_apfnFullDisasm.
 * @{ */
typedef enum DISPARMPARSEIDX
{
    kDisParmParseNop = 0,
    kDisParmParseImm,
    kDisParmParseImmRel,
    kDisParmParseImmAdr,
    kDisParmParseReg,
    kDisParmParseImmsImmrN,
    kDisParmParseHw,
    kDisParmParseCond,
    kDisParmParsePState,
    kDisParmParseCRnCRm,
    kDisParmParseSysReg,
    kDisParmParseMax
} DISPARMPARSEIDX;
/** @}  */


/**
 * Opcode structure.
 */
typedef struct DISARMV8OPCODE
{
    /** The mask defining the static bits of the opcode. */
    uint32_t            fMask;
    /** The value of masked bits of the isntruction. */
    uint32_t            fValue;
    /** The generic opcode structure. */
    DISOPCODE           Opc;
} DISARMV8OPCODE;
/** Pointer to a const opcode. */
typedef const DISARMV8OPCODE *PCDISARMV8OPCODE;


typedef struct DISARMV8INSNPARAM
{
    /** The parser to use for the parameter. */
    DISPARMPARSEIDX     idxParse;
    /** Bit index at which the field starts. */
    uint8_t             idxBitStart;
    /** Size of the bit field. */
    uint8_t             cBits;
} DISARMV8INSNPARAM;
typedef DISARMV8INSNPARAM *PDISARMV8INSNPARAM;
typedef const DISARMV8INSNPARAM *PCDISARMV8INSNPARAM;

#define DIS_ARMV8_INSN_PARAM_NONE { kDisParmParseNop, 0, 0 }
#define DIS_ARMV8_INSN_PARAM_CREATE(a_idxParse, a_idxBitStart, a_cBits) \
    { a_idxParse, a_idxBitStart, a_cBits }


/**
 * Opcode decode index.
 */
typedef enum DISARMV8OPCDECODE
{
    kDisArmV8OpcDecodeNop = 0,
    kDisArmV8OpcDecodeLookup,
    kDisArmV8OpcDecodeMax
} DISARMV8OPCDECODE;


/**
 * Decoder stage type.
 */
typedef enum kDisArmV8DecodeType
{
    kDisArmV8DecodeType_Invalid = 0,
    kDisArmV8DecodeType_Map,
    kDisArmV8DecodeType_Table,
    kDisArmV8DecodeType_InsnClass,
    kDisArmV8DecodeType_32Bit_Hack = 0x7fffffff
} kDisArmV8DecodeType;


/**
 * Decode header.
 */
typedef struct DISARMV8DECODEHDR
{
    /** Next stage decoding type. */
    kDisArmV8DecodeType         enmDecodeType;
    /** Number of entries in the next decoder stage or
     * opcodes in the instruction class. */
    uint32_t                    cDecode;
} DISARMV8DECODEHDR;
/** Pointer to a decode header. */
typedef DISARMV8DECODEHDR *PDISARMV8DECODEHDR;
/** Pointer to a const decode header. */
typedef const DISARMV8DECODEHDR *PCDISARMV8DECODEHDR;
typedef const PCDISARMV8DECODEHDR *PPCDISARMV8DECODEHDR;


/**
 * Instruction class descriptor.
 */
typedef struct DISARMV8INSNCLASS
{
    /** Decoder header. */
    DISARMV8DECODEHDR       Hdr;
    /** Pointer to the arry of opcodes. */
    PCDISARMV8OPCODE        paOpcodes;
    /** Some flags for this instruction class. */
    uint32_t                fClass;
    /** Opcode decoder function. */
    DISARMV8OPCDECODE       enmOpcDecode;
    /** The mask of the bits relevant for decoding. */
    uint32_t                fMask;
    /** Number of bits to shift to get an index. */
    uint32_t                cShift;
    /** The parameters. */
    DISARMV8INSNPARAM       aParms[4];
} DISARMV8INSNCLASS;
/** Pointer to a constant instruction class descriptor. */
typedef const DISARMV8INSNCLASS *PCDISARMV8INSNCLASS;

/** The instruction class distinguishes between a 32-bit and 64-bit variant using the sf bit (bit 31). */
#define DISARMV8INSNCLASS_F_SF                          RT_BIT_32(0)
/** The N bit in an N:ImmR:ImmS bit vector must be 1 for 64-bit instruction variants. */
#define DISARMV8INSNCLASS_F_N_FORCED_1_ON_64BIT         RT_BIT_32(1)
/** The instruction class is using the 64-bit register encoding only. */
#define DISARMV8INSNCLASS_F_FORCED_64BIT                RT_BIT_32(2)


#define DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(a_Name) \
    static const DISARMV8OPCODE a_Name ## Opcodes[] = {
#define DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(a_Name, a_fClass, a_enmOpcDecode, a_fMask, a_cShift) \
    }; \
    static const DISARMV8INSNCLASS a_Name = { { kDisArmV8DecodeType_InsnClass, RT_ELEMENTS(a_Name ## Opcodes) }, &a_Name ## Opcodes[0],\
                                              a_fClass, a_enmOpcDecode, a_fMask, a_cShift, {
#define DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END } }

/**
 * Decoder lookup table entry.
 */
typedef struct DISARMV8DECODETBLENTRY
{
    /** The mask to apply to the instruction. */
    uint32_t                    fMask;
    /** The value the masked instruction must match for the entry to match. */
    uint32_t                    fValue;
    /** The next stage followed when there is a match. */
    PCDISARMV8DECODEHDR         pHdrNext;
} DISARMV8DECODETBLENTRY;
typedef struct DISARMV8DECODETBLENTRY *PDISARMV8DECODETBLENTRY;
typedef const DISARMV8DECODETBLENTRY *PCDISARMV8DECODETBLENTRY;


#define DIS_ARMV8_DECODE_TBL_ENTRY_INIT(a_fMask, a_fValue, a_pNext) \
    { a_fMask, a_fValue, &a_pNext.Hdr }


/**
 * Decoder lookup table using masks and values.
 */
typedef struct DISARMV8DECODETBL
{
    /** The header for the decoder lookup table. */
    DISARMV8DECODEHDR           Hdr;
    /** Pointer to the individual entries. */
    PCDISARMV8DECODETBLENTRY    paEntries;
} DISARMV8DECODETBL;
/** Pointer to a const decode table. */
typedef const struct DISARMV8DECODETBL *PCDISARMV8DECODETBL;


#define DIS_ARMV8_DECODE_TBL_DEFINE_BEGIN(a_Name) \
    static const DISARMV8DECODETBLENTRY a_Name ## TblEnt[] = {

#define DIS_ARMV8_DECODE_TBL_DEFINE_END(a_Name) \
    }; \
    static const DISARMV8DECODETBL a_Name = { { kDisArmV8DecodeType_Table, RT_ELEMENTS(a_Name ## TblEnt) }, &a_Name ## TblEnt[0] }


/**
 * Decoder map when direct indexing is possible.
 */
typedef struct DISARMV8DECODEMAP
{
    /** The header for the decoder map. */
    DISARMV8DECODEHDR           Hdr;
    /** The bitmask used to decide where to go next. */
    uint32_t                    fMask;
    /** Amount to shift to get at the index. */
    uint32_t                    cShift;
    /** Pointer to the array of pointers to the next stage to index into. */
    PPCDISARMV8DECODEHDR        papNext;
} DISARMV8DECODEMAP;
/** Pointer to a const decode map. */
typedef const struct DISARMV8DECODEMAP *PCDISARMV8DECODEMAP;

#define DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(a_Name) \
    static const PCDISARMV8DECODEHDR a_Name ## MapHdrs[] = {

#define DIS_ARMV8_DECODE_MAP_DEFINE_END(a_Name, a_fMask, a_cShift) \
    }; \
    static const DISARMV8DECODEMAP a_Name = { { kDisArmV8DecodeType_Map, RT_ELEMENTS(a_Name ## MapHdrs) }, a_fMask, a_cShift, &a_Name ## MapHdrs[0] }

#define DIS_ARMV8_DECODE_MAP_DEFINE_END_NON_STATIC(a_Name, a_fMask, a_cShift) \
    }; \
    DECL_HIDDEN_CONST(DISARMV8DECODEMAP) a_Name = { { kDisArmV8DecodeType_Map, RT_ELEMENTS(a_Name ## MapHdrs) }, a_fMask, a_cShift, &a_Name ## MapHdrs[0] }

#define DIS_ARMV8_DECODE_MAP_INVALID_ENTRY NULL
#define DIS_ARMV8_DECODE_MAP_ENTRY(a_Next) &a_Next.Hdr


/** @name Decoder maps.
 * @{ */
extern DECL_HIDDEN_DATA(DISOPCODE) g_ArmV8A64InvalidOpcode[1];

extern DECL_HIDDEN_DATA(DISARMV8DECODEMAP) g_ArmV8A64DecodeL0;
/** @} */


/** @} */
#endif /* !VBOX_INCLUDED_SRC_DisasmInternal_armv8_h */

