/* $Id$ */
/** @file
 * VBox disassembler - Disassemble and optionally format.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DIS
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "DisasmInternal.h"
#include "DisasmTables.h"


/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pvInstr         Pointer to the instruction to disassemble.
 * @param   pcbInstr        Where to store the size of the instruction. NULL is
 *                          allowed.
 * @param   pszOutput       Storage for disassembled instruction
 * @param   cbOutput        Size of the output buffer.
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrToStr(void const *pvInstr, DISCPUMODE enmCpuMode, PDISCPUSTATE pCpu, uint32_t *pcbInstr,
                           char *pszOutput, size_t cbOutput)
{
    return DISInstrEx((uintptr_t)pvInstr, 0, enmCpuMode, NULL, NULL, OPTYPE_ALL,
                      pCpu, pcbInstr, pszOutput);
}

/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   uInstrAddr      Pointer to the instruction to disassemble.
 * @param   offRealAddr     Offset to add to instruction address to get the real
 *                          virtual address.
 * @param   pcbInstr        Where to store the size of the instruction. NULL is
 *                          allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrWithOff(PDISCPUSTATE pCpu, RTUINTPTR uInstrAddr, RTUINTPTR offRealAddr,
                             uint32_t *pcbInstr, char *pszOutput)
{
    return DISInstrEx(uInstrAddr, offRealAddr, pCpu->mode, pCpu->pfnReadBytes, pCpu->apvUserData[0], OPTYPE_ALL,
                      pCpu, pcbInstr, pszOutput);
}

/**
 * Disassembles one instruction with a byte fetcher caller.
 *
 * @returns VBox error code
 * @param   uInstrAddr      Pointer to the structure to disassemble.
 * @param   enmCpuMode      The CPU mode.
 * @param   pfnCallback     The byte fetcher callback.
 * @param   pvUser          The user argument (found in
 *                          DISCPUSTATE::apvUserData[0]).
 * @param   pCpu            Where to return the disassembled instruction.
 * @param   pcbInstr        Where to store the size of the instruction. NULL is
 *                          allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                PDISCPUSTATE pCpu, uint32_t *pcbInstr, char *pszOutput)

{
    return DISInstrEx(uInstrAddr, 0, enmCpuMode, pfnReadBytes, pvUser, OPTYPE_ALL,
                      pCpu, pcbInstr, pszOutput);
}

/**
 * Disassembles one instruction; only fully disassembly an instruction if it matches the filter criteria
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   uInstrAddr      Pointer to the structure to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbInstr        Where to store the size of the instruction. NULL is
 *                          allowed.
 * @param   pszOutput       Storage for disassembled instruction
 * @param   uFilter         Instruction type filter.
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrEx(RTUINTPTR uInstrAddr, RTUINTPTR offRealAddr, DISCPUMODE enmCpuMode,
                        PFNDISREADBYTES pfnReadBytes, void *pvUser, uint32_t uFilter,
                        PDISCPUSTATE pCpu, uint32_t *pcbInstr, char *pszOutput)
{
    int rc = DISCoreOneExEx(uInstrAddr, enmCpuMode, uFilter, pfnReadBytes, pvUser, pCpu, pcbInstr);
    if (RT_SUCCESS(rc) && pszOutput)
    {
        size_t      cbOutput = 128;
        size_t      cch;
#if 0
        RTUINTPTR   uRealAddr = uInstrAddr + offRealAddr;
        if (pCpu->mode == CPUMODE_64BIT || uRealAddr > UINT32_MAX)
            cch = RTStrPrintf(pszOutput, cbOutput, "%016RTptr:  ", uRealAddr);
        else
            cch = RTStrPrintf(pszOutput, cbOutput, "%08RX32:  ", (uint32_t)uRealAddr);
        rc = DISFormatYasmEx(pCpu, pszOutput + cch, cbOutput - cch,
                             DIS_FMT_FLAGS_BYTES_LEFT | DIS_FMT_FLAGS_BYTES_BRACKETS | DIS_FMT_FLAGS_BYTES_SPACED
                             | DIS_FMT_FLAGS_RELATIVE_BRANCH,
                             NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
#else
        pCpu->uInstrAddr += offRealAddr;
        rc = DISFormatYasmEx(pCpu, pszOutput, cbOutput,
                             DIS_FMT_FLAGS_BYTES_LEFT | DIS_FMT_FLAGS_BYTES_BRACKETS | DIS_FMT_FLAGS_BYTES_SPACED
                             | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_ADDR_LEFT,
                             NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
        pCpu->uInstrAddr = uInstrAddr;
#endif
        cch = strlen(pszOutput);
        if (cch < cbOutput)
        {
            pszOutput[cch++] = '\n';
            pszOutput[cch] = '\0';
        }
    }
    return rc;
}

