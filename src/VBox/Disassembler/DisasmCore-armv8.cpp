/* $Id$ */
/** @file
 * VBox Disassembler - Core Components.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DIS
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include "DisasmInternal-armv8.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/** @name Parsers
 * @{ */
static FNDISPARSEARMV8 disArmV8ParseIllegal;
/** @}  */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Parser opcode table for full disassembly. */
static PFNDISPARSEARMV8 const g_apfnDisasm[IDX_ParseMax] =
{
    disArmV8ParseIllegal,
};

static size_t disArmV8ParseIllegal(size_t offInstr, PCDISOPCODE pOp, PDISSTATE pDis, PDISOPPARAM pParam, uint8_t cBitStart, uint8_t cBits)
{
    RT_NOREF_PV(pOp); RT_NOREF_PV(pParam); RT_NOREF_PV(pDis);
    AssertFailed();
    return offInstr;
}


/**
 * Internal worker for DISInstrEx and DISInstrWithPrefetchedBytes.
 *
 * @returns VBox status code.
 * @param   pDis            Initialized disassembler state.
 * @param   paOneByteMap    The one byte opcode map to use.
 * @param   pcbInstr        Where to store the instruction size. Can be NULL.
 */
DECLHIDDEN(int) disInstrWorkerArmV8(PDISSTATE pDis, PCDISOPCODE paOneByteMap, uint32_t *pcbInstr)
{
    RT_NOREF(pDis, paOneByteMap, pcbInstr);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Inlined worker that initializes the disassembler state.
 *
 * @returns The primary opcode map to use.
 * @param   pDis            The disassembler state.
 * @param   uInstrAddr      The instruction address.
 * @param   enmCpuMode      The CPU mode.
 * @param   fFilter         The instruction filter settings.
 * @param   pfnReadBytes    The byte reader, can be NULL.
 * @param   pvUser          The user data for the reader.
 */
DECLHIDDEN(PCDISOPCODE) disInitializeStateArmV8(PDISSTATE pDis, DISCPUMODE enmCpuMode, uint32_t fFilter)
{
    RT_NOREF(pDis, enmCpuMode, fFilter);
    return NULL;
}
