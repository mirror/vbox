/** @file
 *
 * VBox disassembler:
 * Main
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#ifdef USING_VISUAL_STUDIO
# include <stdafx.h>
#endif
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
 * @param   uInstrAddr      Pointer to the structure to disassemble.
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstr(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PDISCPUSTATE pCpu, uint32_t *pcbSize, char *pszOutput)
{
    return DISInstrEx(uInstrAddr, 0, enmCpuMode, NULL, NULL, OPTYPE_ALL,
                      pCpu, pcbSize, pszOutput);
}

/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   uInstrAddr      Pointer to the structure to disassemble.
 * @param   offRealAddr     Offset to add to instruction address to get the real
 *                          virtual address.
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrWithOff(PDISCPUSTATE pCpu, RTUINTPTR uInstrAddr, RTUINTPTR offRealAddr,
                             unsigned *pcbSize, char *pszOutput)
{
    return DISInstrEx(uInstrAddr, offRealAddr, pCpu->mode, pCpu->pfnReadBytes, pCpu->apvUserData[0], OPTYPE_ALL,
                      pCpu, pcbSize, pszOutput);
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
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                PDISCPUSTATE pCpu, uint32_t *pcbSize, char *pszOutput)

{
    return DISInstrEx(uInstrAddr, 0, enmCpuMode, pfnReadBytes, pvUser, OPTYPE_ALL,
                      pCpu, pcbSize, pszOutput);
}

/**
 * Disassembles one instruction; only fully disassembly an instruction if it matches the filter criteria
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   uInstrAddr      Pointer to the structure to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 * @param   uFilter         Instruction type filter
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrEx(RTUINTPTR uInstrAddr, RTUINTPTR offRealAddr, DISCPUMODE enmCpuMode,
                        PFNDISREADBYTES pfnReadBytes, void *pvUser, uint32_t uFilter,
                        PDISCPUSTATE pCpu, uint32_t *pcbSize, char *pszOutput)
{
    const OPCODE   *paOneByteMap;

    /*
     * Initialize the CPU state.
     * Note! The RT_BZERO make ASSUMPTIONS about the placement of apvUserData.
     */
    RT_BZERO(pCpu, RT_OFFSETOF(DISCPUSTATE, apvUserData));

    pCpu->mode              = enmCpuMode;
    if (enmCpuMode == CPUMODE_64BIT)
    {
        paOneByteMap        = g_aOneByteMapX64;
        pCpu->addrmode      = CPUMODE_64BIT;
        pCpu->opmode        = CPUMODE_32BIT;
    }
    else
    {
        paOneByteMap        = g_aOneByteMapX86;
        pCpu->addrmode      = enmCpuMode;
        pCpu->opmode        = enmCpuMode;
    }
    pCpu->prefix            = PREFIX_NONE;
    pCpu->enmPrefixSeg      = DIS_SELREG_DS;
    pCpu->uInstrAddr        = uInstrAddr;
    pCpu->opaddr            = uInstrAddr + offRealAddr;
    pCpu->pfnDisasmFnTable  = pfnFullDisasm;
    pCpu->uFilter           = uFilter;
    pCpu->rc                = VINF_SUCCESS;
    pCpu->pfnReadBytes      = pfnReadBytes ? pfnReadBytes : disReadBytesDefault;
    pCpu->apvUserData[0]    = pvUser;

    /*
     * Parse the instruction byte by byte.
     */
    unsigned i        = 0;
    unsigned cbPrefix = 0; /** @todo this isn't really needed, is it? Seems to be a bit too many variables tracking the same stuff here. cbInc, i, cbPrefix, idx... */
    for (;;)
    {
        uint8_t codebyte = DISReadByte(pCpu, uInstrAddr+i);
        uint8_t opcode   = paOneByteMap[codebyte].opcode;

        /* Hardcoded assumption about OP_* values!! */
        if (opcode <= OP_LAST_PREFIX)
        {
            /* The REX prefix must precede the opcode byte(s). Any other placement is ignored. */
            if (opcode != OP_REX)
            {
                pCpu->lastprefix = opcode;
                pCpu->prefix &= ~PREFIX_REX;
            }

            switch (opcode)
            {
            case OP_INVALID:
                if (pcbSize)
                    *pcbSize = pCpu->opsize;
                return pCpu->rc = VERR_DIS_INVALID_OPCODE;

            // segment override prefix byte
            case OP_SEG:
                pCpu->enmPrefixSeg = (DIS_SELREG)(paOneByteMap[codebyte].param1 - OP_PARM_REG_SEG_START);
                /* Segment prefixes for CS, DS, ES and SS are ignored in long mode. */
                if (   pCpu->mode != CPUMODE_64BIT
                    || pCpu->enmPrefixSeg >= DIS_SELREG_FS)
                    pCpu->prefix |= PREFIX_SEG;
                i += sizeof(uint8_t);
                cbPrefix++;
                continue;   //fetch the next byte

            // lock prefix byte
            case OP_LOCK:
                pCpu->prefix |= PREFIX_LOCK;
                i += sizeof(uint8_t);
                cbPrefix++;
                continue;   //fetch the next byte

            // address size override prefix byte
            case OP_ADDRSIZE:
                pCpu->prefix |= PREFIX_ADDRSIZE;
                if (pCpu->mode == CPUMODE_16BIT)
                    pCpu->addrmode = CPUMODE_32BIT;
                else
                if (pCpu->mode == CPUMODE_32BIT)
                    pCpu->addrmode = CPUMODE_16BIT;
                else
                    pCpu->addrmode = CPUMODE_32BIT;     /* 64 bits */

                i += sizeof(uint8_t);
                cbPrefix++;
                continue;   //fetch the next byte

            // operand size override prefix byte
            case OP_OPSIZE:
                pCpu->prefix |= PREFIX_OPSIZE;
                if (pCpu->mode == CPUMODE_16BIT)
                    pCpu->opmode = CPUMODE_32BIT;
                else
                    pCpu->opmode = CPUMODE_16BIT;  /* for 32 and 64 bits mode (there is no 32 bits operand size override prefix) */

                i += sizeof(uint8_t);
                cbPrefix++;
                continue;   //fetch the next byte

            // rep and repne are not really prefixes, but we'll treat them as such
            case OP_REPE:
                pCpu->prefix |= PREFIX_REP;
                i += sizeof(uint8_t);
                cbPrefix += sizeof(uint8_t);
                continue;   //fetch the next byte

            case OP_REPNE:
                pCpu->prefix |= PREFIX_REPNE;
                i += sizeof(uint8_t);
                cbPrefix += sizeof(uint8_t);
                continue;   //fetch the next byte

            case OP_REX:
                Assert(pCpu->mode == CPUMODE_64BIT);
                /* REX prefix byte */
                pCpu->prefix    |= PREFIX_REX;
                pCpu->prefix_rex = PREFIX_REX_OP_2_FLAGS(paOneByteMap[codebyte].param1);
                i += sizeof(uint8_t);
                cbPrefix += sizeof(uint8_t);

                if (pCpu->prefix_rex & PREFIX_REX_FLAGS_W)
                    pCpu->opmode = CPUMODE_64BIT;  /* overrides size prefix byte */
                continue;   //fetch the next byte
            }
        }

        pCpu->cbPrefix = i; Assert(cbPrefix == i);
        pCpu->opcode   = codebyte;

        unsigned idx = i;
        i += sizeof(uint8_t); //first opcode byte

        unsigned cbInc = ParseInstruction(uInstrAddr + i, &paOneByteMap[pCpu->opcode], pCpu);

        AssertMsg(pCpu->opsize == cbPrefix + cbInc + sizeof(uint8_t),
                  ("%u %u\n", pCpu->opsize, cbPrefix + cbInc + sizeof(uint8_t)));
        pCpu->opsize = cbPrefix + cbInc + sizeof(uint8_t);

        if (pszOutput)
            disasmSprintf(pszOutput, uInstrAddr+i-1-cbPrefix, pCpu, &pCpu->param1, &pCpu->param2, &pCpu->param3);

        i += cbInc;
        cbPrefix = 0;
        break;
    }

    if (pcbSize)
        *pcbSize = i;

    if (pCpu->prefix & PREFIX_LOCK)
        disValidateLockSequence(pCpu);

    return pCpu->rc;
}
//*****************************************************************************
//*****************************************************************************
static size_t DbgBytesToString(PDISCPUSTATE pCpu, char *pszOutput, size_t offStart)
{
    unsigned off = offStart;

    while (off < 40)
        pszOutput[off++] = ' ';
    pszOutput[off++] = ' ';
    pszOutput[off++] = '[';

    for (unsigned i = 0; i < pCpu->opsize; i++)
        off += RTStrPrintf(&pszOutput[off], 64, "%02X ", pCpu->abInstr[i]);

    pszOutput[off - 1] = ']';  // replaces space
    return off - offStart;
}
//*****************************************************************************
//*****************************************************************************
void disasmSprintf(char *pszOutput, RTUINTPTR uInstrAddr, PDISCPUSTATE pCpu,
                   OP_PARAMETER *pParam1, OP_PARAMETER *pParam2, OP_PARAMETER *pParam3)
{
    const char *pszFormat = pCpu->pszOpcode;
    int   param = 1;


    size_t off = RTStrPrintf(pszOutput, 64, "%08RTptr:  ", pCpu->opaddr);
    if (pCpu->prefix & PREFIX_LOCK)
        off += RTStrPrintf(&pszOutput[off], 64, "lock ");
    if (pCpu->prefix & PREFIX_REP)
        off += RTStrPrintf(&pszOutput[off], 64, "rep(e) ");
    else if(pCpu->prefix & PREFIX_REPNE)
        off += RTStrPrintf(&pszOutput[off], 64, "repne ");

    if (!strcmp("Invalid Opcode", pszFormat))
    {
        if (pCpu->opsize >= 2)
            off += RTStrPrintf(&pszOutput[off], 64, "Invalid Opcode [%02X][%02X]", pCpu->abInstr[0], pCpu->abInstr[1]);
        else
            off += RTStrPrintf(&pszOutput[off], 64, "Invalid Opcode [%02X]", pCpu->abInstr[0]);
    }
    else
        while (*pszFormat)
        {
            switch (*pszFormat)
            {
            case '%':
                switch (pszFormat[1])
                {
                case 'J': //Relative jump offset
                {
                    int32_t disp;

                    AssertMsg(param == 1, ("Invalid branch parameter nr"));
                    if (pParam1->flags & USE_IMMEDIATE8_REL)
                        disp = (int32_t)(char)pParam1->parval;
                    else if (pParam1->flags & USE_IMMEDIATE16_REL)
                        disp = (int32_t)(uint16_t)pParam1->parval;
                    else if (pParam1->flags & USE_IMMEDIATE32_REL)
                        disp = (int32_t)pParam1->parval;
                    else if(pParam1->flags & USE_IMMEDIATE64_REL)
                        /** @todo: is this correct? */
                        disp = (int32_t)pParam1->parval;
                    else
                    {
                        AssertMsgFailed(("Oops!\n"));
                        return;
                    }
                    uint32_t addr = (uint32_t)(pCpu->opaddr + pCpu->opsize) + disp;
                    off += RTStrPrintf(&pszOutput[off], 64, "[%08X]", addr);
                }

                    //no break;

                case 'A': //direct address
                case 'C': //control register
                case 'D': //debug register
                case 'E': //ModRM specifies parameter
                case 'F': //Eflags register
                case 'G': //ModRM selects general register
                case 'I': //Immediate data
                case 'M': //ModRM may only refer to memory
                case 'O': //No ModRM byte
                case 'P': //ModRM byte selects MMX register
                case 'Q': //ModRM byte selects MMX register or memory address
                case 'R': //ModRM byte may only refer to a general register
                case 'S': //ModRM byte selects a segment register
                case 'T': //ModRM byte selects a test register
                case 'V': //ModRM byte selects an XMM/SSE register
                case 'W': //ModRM byte selects an XMM/SSE register or a memory address
                case 'X': //DS:SI
                case 'Y': //ES:DI
                    switch(param)
                    {
                    case 1:
                        off += RTStrPrintf(&pszOutput[off], 64, pParam1->szParam);
                        break;
                    case 2:
                        off += RTStrPrintf(&pszOutput[off], 64, pParam2->szParam);
                        break;
                    case 3:
                        off += RTStrPrintf(&pszOutput[off], 64, pParam3->szParam);
                        break;
                    }
                    break;

                case 'e': //register based on operand size (e.g. %eAX)
                    if(pCpu->opmode == CPUMODE_32BIT)
                        off += RTStrPrintf(&pszOutput[off], 64, "E");
                    if(pCpu->opmode == CPUMODE_64BIT)
                        off += RTStrPrintf(&pszOutput[off], 64, "R");

                    off += RTStrPrintf(&pszOutput[off], 64, "%c%c", pszFormat[2], pszFormat[3]);
                    break;

                default:
                    AssertMsgFailed(("Oops!\n"));
                    break;
                }

                //Go to the next parameter in the format string
                while (*pszFormat && *pszFormat != ',')
                    pszFormat++;
                if (*pszFormat == ',')
                    pszFormat--;

                break;

            case ',':
                param++;
                //no break

            default:
                off += RTStrPrintf(&pszOutput[off], 64, "%c", *pszFormat);
                break;
            }

            if (*pszFormat)
                pszFormat++;
        }

    off += DbgBytesToString(pCpu, pszOutput, off);
    off += RTStrPrintf(&pszOutput[off], 64, "\n");
}
//*****************************************************************************
//*****************************************************************************
