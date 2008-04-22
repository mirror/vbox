/** @file
 *
 * VBox disassembler:
 * Main
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
 * Disassembles a code block.
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pvCodeBlock     Pointer to the strunction to disassemble.
 * @param   cbMax           Maximum number of bytes to disassemble.
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 *
 *
 * @todo    Define output callback.
 * @todo    Using signed integers as sizes is a bit odd. There are still
 *          some GCC warnings about mixing signed and unsigend integers.
 * @todo    Need to extend this interface to include a code address so we
 *          can dissassemble GC code. Perhaps a new function is better...
 * @remark  cbMax isn't respected as a boundry. DISInstr() will read beyond cbMax.
 *          This means *pcbSize >= cbMax sometimes.
 */
DISDECL(int) DISBlock(PDISCPUSTATE pCpu, RTUINTPTR pvCodeBlock, unsigned cbMax, unsigned *pSize)
{
    unsigned i = 0;
    char szOutput[256];

    while (i < cbMax)
    {
        unsigned cbInstr;
        int rc = DISInstr(pCpu, pvCodeBlock + i, 0, &cbInstr, szOutput);
        if (VBOX_FAILURE(rc))
            return rc;

        i += cbInstr;
    }

    if (pSize)
        *pSize = i;
    return true;
}

/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pu8Instruction  Pointer to the strunction to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstr(PDISCPUSTATE pCpu, RTUINTPTR pu8Instruction, unsigned u32EipOffset, unsigned *pcbSize,
                       char *pszOutput)
{
    return DISInstrEx(pCpu, pu8Instruction, u32EipOffset, pcbSize, pszOutput, OPTYPE_ALL);
}

/**
 * Disassembles one instruction; only fully disassembly an instruction if it matches the filter criteria
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pu8Instruction  Pointer to the strunction to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 * @param   uFilter         Instruction type filter
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrEx(PDISCPUSTATE pCpu, RTUINTPTR pu8Instruction, unsigned u32EipOffset, unsigned *pcbSize,
                         char *pszOutput, unsigned uFilter)
{
    unsigned i = 0, prefixbytes;
    unsigned idx, inc;
#ifdef __L4ENV__
    jmp_buf jumpbuffer;
#endif

    //reset instruction settings
    pCpu->prefix      = PREFIX_NONE;
    pCpu->prefix_seg  = 0;
    pCpu->addrmode    = pCpu->mode;
    pCpu->opmode      = pCpu->mode;
    pCpu->ModRM       = 0;
    pCpu->SIB         = 0;
    pCpu->lastprefix  = 0;
    pCpu->param1.parval = 0;
    pCpu->param2.parval = 0;
    pCpu->param3.parval = 0;
    pCpu->param1.szParam[0] = 0;
    pCpu->param2.szParam[0] = 0;
    pCpu->param3.szParam[0] = 0;
    pCpu->param1.size  = 0;
    pCpu->param2.size  = 0;
    pCpu->param3.size  = 0;
    pCpu->param1.flags = 0;
    pCpu->param2.flags = 0;
    pCpu->param3.flags = 0;
    pCpu->uFilter      = uFilter;
    pCpu->pfnDisasmFnTable = pfnFullDisasm;

    if (pszOutput)
        *pszOutput = '\0';

    prefixbytes = 0;
#ifndef __L4ENV__  /* Unfortunately, we have no exception handling in l4env */
    try
#else
    pCpu->pJumpBuffer = &jumpbuffer;
    if (setjmp(jumpbuffer) == 0)
#endif
    {
        while(1)
        {
            uint8_t codebyte = DISReadByte(pCpu, pu8Instruction+i);
            uint8_t opcode   = g_aOneByteMapX86[codebyte].opcode;

            /* Hardcoded assumption about OP_* values!! */
            if (opcode <= OP_LOCK)
            {
                pCpu->lastprefix = opcode;
                switch(opcode)
                {
                case OP_INVALID:
#if 0 //defined (DEBUG_Sander)
                    AssertMsgFailed(("Invalid opcode!!\n"));
#endif
                    return VERR_DIS_INVALID_OPCODE;

                // segment override prefix byte
                case OP_SEG:
                    pCpu->prefix_seg = g_aOneByteMapX86[codebyte].param1 - OP_PARM_REG_SEG_START;
                    pCpu->prefix |= PREFIX_SEG;
                    i += sizeof(uint8_t);
                    prefixbytes++;
                    continue;   //fetch the next byte

                // lock prefix byte
                case OP_LOCK:
                    pCpu->prefix |= PREFIX_LOCK;
                    i += sizeof(uint8_t);
                    prefixbytes++;
                    continue;   //fetch the next byte

                // address size override prefix byte
                case OP_ADRSIZE:
                    pCpu->prefix |= PREFIX_ADDRSIZE;
                    if(pCpu->mode == CPUMODE_16BIT)
                         pCpu->addrmode = CPUMODE_32BIT;
                    else pCpu->addrmode = CPUMODE_16BIT;
                    i += sizeof(uint8_t);
                    prefixbytes++;
                    continue;   //fetch the next byte

                // operand size override prefix byte
                case OP_OPSIZE:
                    pCpu->prefix |= PREFIX_OPSIZE;
                    if(pCpu->mode == CPUMODE_16BIT)
                         pCpu->opmode = CPUMODE_32BIT;
                    else pCpu->opmode = CPUMODE_16BIT;
                    i += sizeof(uint8_t);
                    prefixbytes++;
                    continue;   //fetch the next byte

                // rep and repne are not really prefixes, but we'll treat them as such
                case OP_REPE:
                    pCpu->prefix |= PREFIX_REP;
                    i += sizeof(uint8_t);
                    prefixbytes += sizeof(uint8_t);
                    continue;   //fetch the next byte

                case OP_REPNE:
                    pCpu->prefix |= PREFIX_REPNE;
                    i += sizeof(uint8_t);
                    prefixbytes += sizeof(uint8_t);
                    continue;   //fetch the next byte

                case OP_REX:
                    Assert(pCpu->mode == CPUMODE_64BIT);
                    /* REX prefix byte */
                    pCpu->prefix    |= PREFIX_REX;
                    pCpu->prefix_rex = PREFIX_REX_OP_2_FLAGS(opcode);
                    break;
                }
            }

            idx = i;
            i += sizeof(uint8_t); //first opcode byte

            pCpu->opcode = codebyte;
            /* Prefix byte(s) is/are part of the instruction. */
            pCpu->opaddr = pu8Instruction + idx + u32EipOffset - prefixbytes;

            if (pCpu->mode == CPUMODE_64BIT)
                inc = ParseInstruction(pu8Instruction + i, &g_aOneByteMapX64[pCpu->opcode], pCpu);
            else
                inc = ParseInstruction(pu8Instruction + i, &g_aOneByteMapX86[pCpu->opcode], pCpu);

            pCpu->opsize = prefixbytes + inc + sizeof(uint8_t);

            if(pszOutput) {
                disasmSprintf(pszOutput, pu8Instruction+i-1-prefixbytes, pCpu, &pCpu->param1, &pCpu->param2, &pCpu->param3);
            }

            i += inc;
            prefixbytes = 0;
            break;
        }
    }
#ifndef __L4ENV__
    catch(...)
#else
    else  /* setjmp has returned a non-zero value: an exception occured */
#endif
    {
        if (pcbSize)
            *pcbSize = 0;
        return VERR_DIS_GEN_FAILURE;
    }

    if (pcbSize)
        *pcbSize = i;

    return VINF_SUCCESS;
}
//*****************************************************************************
//*****************************************************************************
char *DbgBytesToString(PDISCPUSTATE pCpu, RTUINTPTR pBytes, int size, char *pszOutput)
{
    char szByte[4];
    int len = strlen(pszOutput);
    int i;

    for(i = len; i < 40; i++)
    {
        strcat(pszOutput, " ");
    }
    strcat(pszOutput, " [");
    for(i = 0; i < size; i++)
    {
        RTStrPrintf(szByte, sizeof(szByte), "%02X ", DISReadByte(pCpu, pBytes+i));
        RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, szByte);
    }
    len = strlen(pszOutput);
    pszOutput[len - 1] = 0;  //cut off last space

    strcat(pszOutput, "]");
    return pszOutput;
}
//*****************************************************************************
//*****************************************************************************
void disasmSprintf(char *pszOutput, RTUINTPTR pu8Instruction, PDISCPUSTATE pCpu, OP_PARAMETER *pParam1, OP_PARAMETER *pParam2, OP_PARAMETER *pParam3)
{
    const char *lpszFormat = pCpu->pszOpcode;
    int   param = 1;

    RTStrPrintf(pszOutput, 64, "%08X:  ", (unsigned)pCpu->opaddr);
    if(pCpu->prefix & PREFIX_LOCK)
    {
        RTStrPrintf(&pszOutput[strlen(pszOutput)], 64,  "lock ");
    }
    if(pCpu->prefix & PREFIX_REP)
    {
        RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "rep(e) ");
    }
    else
    if(pCpu->prefix & PREFIX_REPNE)
    {
        RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "repne ");
    }

    if(!strcmp("Invalid Opcode", lpszFormat))
    {
        RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "Invalid Opcode [%02X][%02X]", DISReadByte(pCpu, pu8Instruction), DISReadByte(pCpu, pu8Instruction+1) );
    }
    else
    while(*lpszFormat)
    {
        switch(*lpszFormat)
        {
        case '%':
            switch(*(lpszFormat+1))
            {
            case 'J': //Relative jump offset
            {
                int32_t disp;

                AssertMsg(param == 1, ("Invalid branch parameter nr"));
                if(pParam1->flags & USE_IMMEDIATE8_REL)
                {
                    disp = (int32_t)(char)pParam1->parval;
                }
                else
                if(pParam1->flags & USE_IMMEDIATE16_REL)
                {
                    disp = (int32_t)(uint16_t)pParam1->parval;
                }
                else
                if(pParam1->flags & USE_IMMEDIATE32_REL)
                {
                    disp = (int32_t)pParam1->parval;
                }
                else
                {
                    AssertMsgFailed(("Oops!\n"));
                    return;
                }
                uint32_t addr = (uint32_t)(pCpu->opaddr + pCpu->opsize) + disp;
                RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "[%08X]", addr);
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
                    RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, pParam1->szParam);
                    break;
                case 2:
                    RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, pParam2->szParam);
                    break;
                case 3:
                    RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, pParam3->szParam);
                    break;
                }
                break;

            case 'e': //register based on operand size (e.g. %eAX)
                if(pCpu->opmode == CPUMODE_32BIT)
                {
                    RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "E");
                }
                RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "%c%c", lpszFormat[2], lpszFormat[3]);
                break;

            default:
                AssertMsgFailed(("Oops!\n"));
                break;
            }

            //Go to the next parameter in the format string
            while(*lpszFormat && *lpszFormat != ',') lpszFormat++;
            if(*lpszFormat == ',') lpszFormat--;

            break;

        case ',':
            param++;
            //no break

        default:
            RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "%c", *lpszFormat);
            break;
        }

        if(*lpszFormat) lpszFormat++;
    }
    DbgBytesToString(pCpu, pu8Instruction, pCpu->opsize, pszOutput);
    RTStrPrintf(&pszOutput[strlen(pszOutput)], 64, "\n");
}
//*****************************************************************************
//*****************************************************************************
