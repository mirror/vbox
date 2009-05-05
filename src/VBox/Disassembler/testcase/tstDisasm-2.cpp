/* $Id$ */
/** @file
 * Testcase - Generic Disassembler Tool.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <VBox/dis.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef enum { kAsmStyle_Default, kAsmStyle_yasm, kAsmStyle_masm, kAsmStyle_gas, kAsmStyle_invalid } ASMSTYLE;
typedef enum { kUndefOp_Fail, kUndefOp_All, kUndefOp_DefineByte, kUndefOp_End } UNDEFOPHANDLING;

typedef struct MYDISSTATE
{
    DISCPUSTATE     Cpu;
    uint64_t        uAddress;           /**< The current instruction address. */
    uint8_t        *pbInstr;            /**< The current instruction (pointer). */
    uint32_t        cbInstr;            /**< The size of the current instruction. */
    bool            fUndefOp;           /**< Whether the current instruction is really an undefined opcode.*/
    UNDEFOPHANDLING enmUndefOp;         /**< How to treat undefined opcodes. */
    int             rc;                 /**< Set if we hit EOF. */
    size_t          cbLeft;             /**< The number of bytes left. (read) */
    uint8_t        *pbNext;             /**< The next byte. (read) */
    uint64_t        uNextAddr;          /**< The address of the next byte. (read) */
    char            szLine[256];        /**< The disassembler text output. */
} MYDISSTATE;
typedef MYDISSTATE *PMYDISSTATE;



/**
 * Default style.
 *
 * @param   pState      The disassembler state.
 */
static void MyDisasDefaultFormatter(PMYDISSTATE pState)
{
    RTPrintf("%s", pState->szLine);
}


/**
 * Yasm style.
 *
 * @param   pState      The disassembler state.
 */
static void MyDisasYasmFormatter(PMYDISSTATE pState)
{
    char szTmp[256];
#if 0
    /* a very quick hack. */
    strcpy(szTmp, RTStrStripL(strchr(pState->szLine, ':') + 1));

    char *psz = strrchr(szTmp, '[');
    *psz = '\0';
    RTStrStripR(szTmp);

    psz = strstr(szTmp, " ptr ");
    if (psz)
        memset(psz, ' ', 5);

    char *pszEnd = strchr(szTmp, '\0');
    while (pszEnd - &szTmp[0] < 71)
        *pszEnd++ = ' ';
    *pszEnd = '\0';

#else
    size_t cch = DISFormatYasmEx(&pState->Cpu, szTmp, sizeof(szTmp),
                                 DIS_FMT_FLAGS_STRICT | DIS_FMT_FLAGS_ADDR_RIGHT | DIS_FMT_FLAGS_ADDR_COMMENT
                                 | DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_BYTES_COMMENT | DIS_FMT_FLAGS_BYTES_SPACED,
                                 NULL, NULL);
    Assert(cch < sizeof(szTmp));
    while (cch < 71)
        szTmp[cch++] = ' ';
    szTmp[cch] = '\0';
#endif

    RTPrintf("    %s ; %08llu %s", szTmp, pState->uAddress, pState->szLine);
}


/**
 * Checks if the encoding of the current instruction is something
 * we can never get the assembler to produce.
 *
 * @returns true if it's odd, false if it isn't.
 * @param   pCpu        The disassembler output.
 */
static bool MyDisasYasmFormatterIsOddEncoding(PMYDISSTATE pState)
{
    /*
     * Mod rm + SIB: Check for duplicate EBP encodings that yasm won't use for very good reasons.
     */
    if (    pState->Cpu.addrmode != CPUMODE_16BIT ///@todo correct?
        &&  pState->Cpu.ModRM.Bits.Rm == 4
        &&  pState->Cpu.ModRM.Bits.Mod != 3)
    {
        /* No scaled index SIB (index=4), except for ESP. */
        if (    pState->Cpu.SIB.Bits.Index == 4
            &&  pState->Cpu.SIB.Bits.Base != 4)
            return true;

        /* EBP + displacement */
        if (    pState->Cpu.ModRM.Bits.Mod != 0
             && pState->Cpu.SIB.Bits.Base == 5
             && pState->Cpu.SIB.Bits.Scale == 0)
            return true;
    }

    /*
     * Seems to be an instruction alias here, but I cannot find any docs on it... hrmpf!
     */
    if (    pState->Cpu.pCurInstr->opcode == OP_SHL
        &&  pState->Cpu.ModRM.Bits.Reg == 6)
        return true;

    /*
     * Check for multiple prefixes of the same kind.
     */
    uint32_t fPrefixes = 0;
    for (uint8_t const *pu8 = pState->pbInstr;; pu8++)
    {
        uint32_t f;
        switch (*pu8)
        {
            case 0xf0:
                f = PREFIX_LOCK;
                break;

            case 0xf2:
            case 0xf3:
                f = PREFIX_REP; /* yes, both */
                break;

            case 0x2e:
            case 0x3e:
            case 0x26:
            case 0x36:
            case 0x64:
            case 0x65:
                f = PREFIX_SEG;
                break;

            case 0x66:
                f = PREFIX_OPSIZE;
                break;

            case 0x67:
                f = PREFIX_ADDRSIZE;
                break;

            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                f = pState->Cpu.mode == CPUMODE_64BIT ? PREFIX_REX : 0;
                break;

            default:
                f = 0;
                break;
        }
        if (!f)
            break; /* done */
        if (fPrefixes & f)
            return true;
        fPrefixes |= f;
    }

    /* segment overrides are fun */
    if (fPrefixes & PREFIX_SEG)
    {
        /* no effective address which it may apply to. */
        Assert((pState->Cpu.prefix & PREFIX_SEG) || pState->Cpu.mode == CPUMODE_64BIT);
        if (    !DIS_IS_EFFECTIVE_ADDR(pState->Cpu.param1.flags)
            &&  !DIS_IS_EFFECTIVE_ADDR(pState->Cpu.param2.flags)
            &&  !DIS_IS_EFFECTIVE_ADDR(pState->Cpu.param3.flags))
            return true;
    }

    /* fixed register + addr override doesn't go down all that well. */
    if (fPrefixes & PREFIX_ADDRSIZE)
    {
        Assert(pState->Cpu.prefix & PREFIX_ADDRSIZE);
        if (    pState->Cpu.pCurInstr->param3 == OP_PARM_NONE
            &&  pState->Cpu.pCurInstr->param2 == OP_PARM_NONE
            &&  (   pState->Cpu.pCurInstr->param1 >= OP_PARM_REG_GEN32_START
                 && pState->Cpu.pCurInstr->param1 <= OP_PARM_REG_GEN32_END))
            return true;
    }

    /* Almost all prefixes are bad. */
    if (fPrefixes)
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            /* nop w/ prefix(es). */
            case OP_NOP:
                return true;

            case OP_JMP:
                if (    pState->Cpu.pCurInstr->param1 != OP_PARM_Jb
                    &&  pState->Cpu.pCurInstr->param1 != OP_PARM_Jv)
                    break;
                /* fall thru */
            case OP_JO:
            case OP_JNO:
            case OP_JC:
            case OP_JNC:
            case OP_JE:
            case OP_JNE:
            case OP_JBE:
            case OP_JNBE:
            case OP_JS:
            case OP_JNS:
            case OP_JP:
            case OP_JNP:
            case OP_JL:
            case OP_JNL:
            case OP_JLE:
            case OP_JNLE:
                /** @todo branch hinting 0x2e/0x3e... */
                return true;
        }

    }

    /* All but the segment prefix is bad news. */
    if (fPrefixes & ~PREFIX_SEG)
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            case OP_POP:
            case OP_PUSH:
                if (    pState->Cpu.pCurInstr->param1 >= OP_PARM_REG_SEG_START
                    &&  pState->Cpu.pCurInstr->param1 <= OP_PARM_REG_SEG_END)
                    return true;
                if (    (fPrefixes & ~PREFIX_OPSIZE)
                    &&  pState->Cpu.pCurInstr->param1 >= OP_PARM_REG_GEN32_START
                    &&  pState->Cpu.pCurInstr->param1 <= OP_PARM_REG_GEN32_END)
                    return true;
                break;

            case OP_POPA:
            case OP_POPF:
            case OP_PUSHA:
            case OP_PUSHF:
                if (fPrefixes & ~PREFIX_OPSIZE)
                    return true;
                break;
        }
    }

    /* Implicit 8-bit register instructions doesn't mix with operand size. */
    if (    (fPrefixes & PREFIX_OPSIZE)
        &&  (   (   pState->Cpu.pCurInstr->param1 == OP_PARM_Gb /* r8 */
                 && pState->Cpu.pCurInstr->param2 == OP_PARM_Eb /* r8/mem8 */)
             || (   pState->Cpu.pCurInstr->param2 == OP_PARM_Gb /* r8 */
                 && pState->Cpu.pCurInstr->param1 == OP_PARM_Eb /* r8/mem8 */))
       )
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            case OP_ADD:
            case OP_OR:
            case OP_ADC:
            case OP_SBB:
            case OP_AND:
            case OP_SUB:
            case OP_XOR:
            case OP_CMP:
                return true;
            default:
                break;
        }
    }


    /*
     * Check for the version of xyz reg,reg instruction that the assembler doesn't use.
     *
     * For example:
     *    expected: 1aee   sbb ch, dh     ; SBB r8, r/m8
     *        yasm: 18F5   sbb ch, dh     ; SBB r/m8, r8
     */
    if (pState->Cpu.ModRM.Bits.Mod == 3 /* reg,reg */)
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            case OP_ADD:
            case OP_OR:
            case OP_ADC:
            case OP_SBB:
            case OP_AND:
            case OP_SUB:
            case OP_XOR:
            case OP_CMP:
                if (    (    pState->Cpu.pCurInstr->param1 == OP_PARM_Gb /* r8 */
                         && pState->Cpu.pCurInstr->param2 == OP_PARM_Eb /* r8/mem8 */)
                    ||  (    pState->Cpu.pCurInstr->param1 == OP_PARM_Gv /* rX */
                         && pState->Cpu.pCurInstr->param2 == OP_PARM_Ev /* rX/memX */))
                    return true;

                /* 82 (see table A-6). */
                if (pState->Cpu.opcode == 0x82)
                    return true;
                break;

            /* ff /0, fe /0, ff /1, fe /0 */
            case OP_DEC:
            case OP_INC:
                return true;

            case OP_POP:
            case OP_PUSH:
                Assert(pState->Cpu.opcode == 0x8f);
                return true;

            default:
                break;
        }
    }

    /* shl eax,1 will be assembled to the form without the immediate byte. */
    if (    pState->Cpu.pCurInstr->param2 == OP_PARM_Ib
        &&  (uint8_t)pState->Cpu.param2.parval == 1)
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            case OP_SHL:
            case OP_SHR:
            case OP_SAR:
            case OP_RCL:
            case OP_RCR:
            case OP_ROL:
            case OP_ROR:
                return true;
        }
    }

    /* And some more - see table A-6. */
    if (pState->Cpu.opcode == 0x82)
    {
        switch (pState->Cpu.pCurInstr->opcode)
        {
            case OP_ADD:
            case OP_OR:
            case OP_ADC:
            case OP_SBB:
            case OP_AND:
            case OP_SUB:
            case OP_XOR:
            case OP_CMP:
                return true;
                break;
        }
    }


    /* check for REX.X = 1 without SIB. */

    /* Yasm encodes setnbe al with /2 instead of /0 like the AMD manual
       says (intel doesn't appear to care). */
    switch (pState->Cpu.pCurInstr->opcode)
    {
        case OP_SETO:
        case OP_SETNO:
        case OP_SETC:
        case OP_SETNC:
        case OP_SETE:
        case OP_SETNE:
        case OP_SETBE:
        case OP_SETNBE:
        case OP_SETS:
        case OP_SETNS:
        case OP_SETP:
        case OP_SETNP:
        case OP_SETL:
        case OP_SETNL:
        case OP_SETLE:
        case OP_SETNLE:
            AssertMsg(pState->Cpu.opcode >= 0x90 && pState->Cpu.opcode <= 0x9f, ("%#x\n", pState->Cpu.opcode));
            if (pState->Cpu.ModRM.Bits.Reg != 2)
                return true;
            break;
    }

    /*
     * The MOVZX reg32,mem16 instruction without an operand size prefix
     * doesn't quite make sense...
     */
    if (    pState->Cpu.pCurInstr->opcode == OP_MOVZX
        &&  pState->Cpu.opcode == 0xB7
        &&  (pState->Cpu.mode == CPUMODE_16BIT) != !!(fPrefixes & PREFIX_OPSIZE))
        return true;

    return false;
}


/**
 * Masm style.
 *
 * @param   pState      The disassembler state.
 */
static void MyDisasMasmFormatter(PMYDISSTATE pState)
{
    RTPrintf("masm not implemented: %s", pState->szLine);
}


/**
 * This is a temporary workaround for catching a few illegal opcodes
 * that the disassembler is currently letting thru, just enough to make
 * the assemblers happy.
 *
 * We're too close to a release to dare mess with these things now as
 * they may consequences for performance and let alone introduce bugs.
 *
 * @returns true if it's valid. false if it isn't.
 *
 * @param   pCpu        The disassembler output.
 */
static bool MyDisasIsValidInstruction(DISCPUSTATE const *pCpu)
{
    switch (pCpu->pCurInstr->opcode)
    {
        /* These doesn't take memory operands. */
        case OP_MOV_CR:
        case OP_MOV_DR:
        case OP_MOV_TR:
            if (pCpu->ModRM.Bits.Mod != 3)
                return false;
            break;

         /* The 0x8f /0 variant of this instruction doesn't get its /r value verified. */
        case OP_POP:
            if (    pCpu->opcode == 0x8f
                &&  pCpu->ModRM.Bits.Reg != 0)
                return false;
            break;

        /* The 0xc6 /0 and 0xc7 /0 variants of this instruction don't get their /r values verified. */
        case OP_MOV:
            if (    (   pCpu->opcode == 0xc6
                     || pCpu->opcode == 0xc7)
                &&  pCpu->ModRM.Bits.Reg != 0)
                return false;
            break;

        default:
            break;
    }

    return true;
}


/**
 * Callback for reading bytes.
 *
 * @todo This should check that the disassembler doesn't do unnecessary reads,
 *       however the current doesn't do this and is just complicated...
 */
static DECLCALLBACK(int) MyDisasInstrRead(RTUINTPTR uSrcAddr, uint8_t *pbDst, uint32_t cbRead, void *pvDisCpu)
{
    PMYDISSTATE pState = (PMYDISSTATE)pvDisCpu;
    if (RT_LIKELY(   pState->uNextAddr == uSrcAddr
                  && pState->cbLeft >= cbRead))
    {
        /*
         * Straight forward reading.
         */
        if (cbRead == 1)
        {
            pState->cbLeft--;
            *pbDst = *pState->pbNext++;
            pState->uNextAddr++;
        }
        else
        {
            memcpy(pbDst, pState->pbNext, cbRead);
            pState->pbNext += cbRead;
            pState->cbLeft -= cbRead;
            pState->uNextAddr += cbRead;
        }
    }
    else
    {
        /*
         * Jumping up the stream.
         * This occures when the byte sequence is added to the output string.
         */
        uint64_t offReq64 = uSrcAddr - pState->uAddress;
        if (offReq64 < 32)
        {
            uint32_t offReq = offReq64;
            uintptr_t off = pState->pbNext - pState->pbInstr;
            if (off + pState->cbLeft <= offReq)
            {
                pState->pbNext += pState->cbLeft;
                pState->uNextAddr += pState->cbLeft;
                pState->cbLeft = 0;

                memset(pbDst, 0xcc, cbRead);
                pState->rc = VERR_EOF;
                return VERR_EOF;
            }

            /* reset the stream. */
            pState->cbLeft += off;
            pState->pbNext = pState->pbInstr;
            pState->uNextAddr = pState->uAddress;

            /* skip ahead. */
            pState->cbLeft -= offReq;
            pState->pbNext += offReq;
            pState->uNextAddr += offReq;

            /* do the reading. */
            if (pState->cbLeft >= cbRead)
            {
                memcpy(pbDst, pState->pbNext, cbRead);
                pState->cbLeft -= cbRead;
                pState->pbNext += cbRead;
                pState->uNextAddr += cbRead;
            }
            else
            {
                if (pState->cbLeft > 0)
                {
                    memcpy(pbDst, pState->pbNext, pState->cbLeft);
                    pbDst += pState->cbLeft;
                    cbRead -= (uint32_t)pState->cbLeft;
                    pState->pbNext += pState->cbLeft;
                    pState->uNextAddr += pState->cbLeft;
                    pState->cbLeft = 0;
                }
                memset(pbDst, 0xcc, cbRead);
                pState->rc = VERR_EOF;
                return VERR_EOF;
            }
        }
        else
        {
            RTStrmPrintf(g_pStdErr, "Reading before current instruction!\n");
            memset(pbDst, 0x90, cbRead);
            pState->rc = VERR_INTERNAL_ERROR;
            return VERR_INTERNAL_ERROR;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Disassembles a block of memory.
 *
 * @returns VBox status code.
 * @param   argv0       Program name (for errors and warnings).
 * @param   enmCpuMode  The cpu mode to disassemble in.
 * @param   uAddress    The address we're starting to disassemble at.
 * @param   pbFile      Where to start disassemble.
 * @param   cbFile      How much to disassemble.
 * @param   enmStyle    The assembly output style.
 * @param   fListing    Whether to print in a listing like mode.
 * @param   enmUndefOp  How to deal with undefined opcodes.
 */
static int MyDisasmBlock(const char *argv0, DISCPUMODE enmCpuMode, uint64_t uAddress, uint8_t *pbFile, size_t cbFile,
                         ASMSTYLE enmStyle, bool fListing, UNDEFOPHANDLING enmUndefOp)
{
    /*
     * Initialize the CPU context.
     */
    MYDISSTATE State;
    State.Cpu.mode = enmCpuMode;
    State.Cpu.pfnReadBytes = MyDisasInstrRead;
    State.uAddress = uAddress;
    State.pbInstr = pbFile;
    State.cbInstr = 0;
    State.enmUndefOp = enmUndefOp;
    State.rc = VINF_SUCCESS;
    State.cbLeft = cbFile;
    State.pbNext = pbFile;
    State.uNextAddr = uAddress;

    void (*pfnFormatter)(PMYDISSTATE pState);
    switch (enmStyle)
    {
        case kAsmStyle_Default:
            pfnFormatter = MyDisasDefaultFormatter;
            break;

        case kAsmStyle_yasm:
            RTPrintf("    BITS %d\n", enmCpuMode == CPUMODE_16BIT ? 16 : enmCpuMode == CPUMODE_32BIT ? 32 : 64);
            pfnFormatter = MyDisasYasmFormatter;
            break;

        case kAsmStyle_masm:
            pfnFormatter = MyDisasMasmFormatter;
            break;

        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }

    /*
     * The loop.
     */
    int rcRet = VINF_SUCCESS;
    while (State.cbLeft > 0)
    {
        /*
         * Disassemble it.
         */
        State.cbInstr = 0;
        State.cbLeft += State.pbNext - State.pbInstr;
        State.uNextAddr = State.uAddress;
        State.pbNext = State.pbInstr;

        int rc = DISInstr(&State.Cpu, State.uAddress, 0, &State.cbInstr, State.szLine);
        if (    RT_SUCCESS(rc)
            ||  (   (   rc == VERR_DIS_INVALID_OPCODE
                     || rc == VERR_DIS_GEN_FAILURE)
                 &&  State.enmUndefOp == kUndefOp_DefineByte))
        {
            State.fUndefOp = rc == VERR_DIS_INVALID_OPCODE
                          || rc == VERR_DIS_GEN_FAILURE
                          || State.Cpu.pCurInstr->opcode == OP_INVALID
                          || State.Cpu.pCurInstr->opcode == OP_ILLUD2
                          || (    State.enmUndefOp == kUndefOp_DefineByte
                              && !MyDisasIsValidInstruction(&State.Cpu));
            if (State.fUndefOp && State.enmUndefOp == kUndefOp_DefineByte)
            {
                RTPrintf("    db");
                if (!State.cbInstr)
                    State.cbInstr = 1;
                for (unsigned off = 0; off < State.cbInstr; off++)
                {
                    uint8_t b;
                    State.Cpu.pfnReadBytes(State.uAddress + off, &b, 1, &State.Cpu);
                    RTPrintf(off ? ", %03xh" : " %03xh", b);
                }
                RTPrintf("    ; %s\n", State.szLine);
            }
            else if (!State.fUndefOp && State.enmUndefOp == kUndefOp_All)
            {
                RTPrintf("%s: error at %#RX64: unexpected valid instruction (op=%d)\n", argv0, State.uAddress, State.Cpu.pCurInstr->opcode);
                pfnFormatter(&State);
                rcRet = VERR_GENERAL_FAILURE;
            }
            else if (State.fUndefOp && State.enmUndefOp == kUndefOp_Fail)
            {
                RTPrintf("%s: error at %#RX64: undefined opcode (op=%d)\n", argv0, State.uAddress, State.Cpu.pCurInstr->opcode);
                pfnFormatter(&State);
                rcRet = VERR_GENERAL_FAILURE;
            }
            else
            {
                /* Use db for odd encodings that we can't make the assembler use. */
                if (    State.enmUndefOp == kUndefOp_DefineByte
                    &&  MyDisasYasmFormatterIsOddEncoding(&State))
                {
                    RTPrintf("    db");
                    for (unsigned off = 0; off < State.cbInstr; off++)
                    {
                        uint8_t b;
                        State.Cpu.pfnReadBytes(State.uAddress + off, &b, 1, &State.Cpu);
                        RTPrintf(off ? ", %03xh" : " %03xh", b);
                    }
                    RTPrintf(" ; ");
                }

                pfnFormatter(&State);
            }
        }
        else
        {
            State.cbInstr = State.pbNext - State.pbInstr;
            if (!State.cbLeft)
                RTPrintf("%s: error at %#RX64: read beyond the end (%Rrc)\n", argv0, State.uAddress, rc);
            else if (State.cbInstr)
                RTPrintf("%s: error at %#RX64: %Rrc cbInstr=%d\n", argv0, State.uAddress, rc, State.cbInstr);
            else
            {
                RTPrintf("%s: error at %#RX64: %Rrc cbInstr=%d!\n", argv0, State.uAddress, rc, State.cbInstr);
                if (rcRet == VINF_SUCCESS)
                    rcRet = rc;
                break;
            }
        }


        /* next */
        State.uAddress += State.cbInstr;
        State.pbInstr += State.cbInstr;
    }

    return rcRet;
}

/**
 * Converts a hex char to a number.
 *
 * @returns 0..15 on success, -1 on failure.
 * @param   ch      The character.
 */
static int HexDigitToNum(char ch)
{
    switch (ch)
    {
        case '0': return 0;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        case 'A':
        case 'a': return 0xa;
        case 'B':
        case 'b': return 0xb;
        case 'C':
        case 'c': return 0xc;
        case 'D':
        case 'd': return 0xd;
        case 'E':
        case 'e': return 0xe;
        case 'F':
        case 'f': return 0xf;
        default:
            RTPrintf("error: Invalid hex digig '%c'\n", ch);
            return -1;
    }
}

/**
 * Prints usage info.
 *
 * @returns 1.
 * @param   argv0       The program name.
 */
static int Usage(const char *argv0)
{
    RTStrmPrintf(g_pStdErr,
"usage: %s [options] <file1> [file2..fileN]\n"
"   or: %s [options] <-x|--hex-bytes> <hex byte> [more hex..]\n"
"   or: %s <--help|-h>\n"
"\n"
"Options:\n"
"  --address|-a <address>\n"
"    The base address. Default: 0\n"
"  --max-bytes|-b <bytes>\n"
"    The maximum number of bytes to disassemble. Default: 1GB\n"
"  --cpumode|-c <16|32|64>\n"
"    The cpu mode. Default: 32\n"
"  --listing|-l, --no-listing|-L\n"
"    Enables or disables listing mode. Default: --no-listing\n"
"  --offset|-o <offset>\n"
"    The file offset at which to start disassembling. Default: 0\n"
"  --style|-s <default|yasm|masm>\n"
"    The assembly output style. Default: default\n"
"  --undef-op|-u <fail|all|db>\n"
"    How to treat undefined opcodes. Default: fail\n"
             , argv0, argv0);
    return 1;
}


int main(int argc, char **argv)
{
    RTR3Init();
    const char * const argv0 = RTPathFilename(argv[0]);

    /* options */
    uint64_t uAddress = 0;
    ASMSTYLE enmStyle = kAsmStyle_Default;
    UNDEFOPHANDLING enmUndefOp = kUndefOp_Fail;
    bool fListing = true;
    DISCPUMODE enmCpuMode = CPUMODE_32BIT;
    RTFOFF off = 0;
    RTFOFF cbMax = _1G;
    bool fHexBytes = false;

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF g_aOptions[] =
    {
        { "--address",      'a', RTGETOPT_REQ_UINT64 },
        { "--cpumode",      'c', RTGETOPT_REQ_UINT32 },
        { "--help",         'h', RTGETOPT_REQ_NOTHING },
        { "--bytes",        'b', RTGETOPT_REQ_INT64 },
        { "--listing",      'l', RTGETOPT_REQ_NOTHING },
        { "--no-listing",   'L', RTGETOPT_REQ_NOTHING },
        { "--offset",       'o', RTGETOPT_REQ_INT64 },
        { "--style",        's', RTGETOPT_REQ_STRING },
        { "--undef-op",     'u', RTGETOPT_REQ_STRING },
        { "--hex-bytes",    'x', RTGETOPT_REQ_NOTHING },
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'a':
                uAddress = ValueUnion.u64;
                break;

            case 'b':
                cbMax = ValueUnion.i;
                break;

            case 'c':
                if (ValueUnion.u32 == 16)
                    enmCpuMode = CPUMODE_16BIT;
                else if (ValueUnion.u32 == 32)
                    enmCpuMode = CPUMODE_32BIT;
                else if (ValueUnion.u32 == 64)
                    enmCpuMode = CPUMODE_64BIT;
                else
                {
                    RTStrmPrintf(g_pStdErr, "%s: Invalid CPU mode value %RU32\n", argv0, ValueUnion.u32);
                    return 1;
                }
                break;

            case 'h':
                return Usage(argv0);

            case 'l':
                fListing = true;
                break;

            case 'L':
                fListing = false;
                break;

            case 'o':
                off = ValueUnion.i;
                break;

            case 's':
                if (!strcmp(ValueUnion.psz, "default"))
                    enmStyle = kAsmStyle_Default;
                else if (!strcmp(ValueUnion.psz, "yasm"))
                    enmStyle = kAsmStyle_yasm;
                else if (!strcmp(ValueUnion.psz, "masm"))
                {
                    enmStyle = kAsmStyle_masm;
                    RTStrmPrintf(g_pStdErr, "%s: masm style isn't implemented yet\n", argv0);
                    return 1;
                }
                else
                {
                    RTStrmPrintf(g_pStdErr, "%s: unknown assembly style: %s\n", argv0, ValueUnion.psz);
                    return 1;
                }
                break;

            case 'u':
                if (!strcmp(ValueUnion.psz, "fail"))
                    enmUndefOp = kUndefOp_Fail;
                else if (!strcmp(ValueUnion.psz, "all"))
                    enmUndefOp = kUndefOp_All;
                else if (!strcmp(ValueUnion.psz, "db"))
                    enmUndefOp = kUndefOp_DefineByte;
                else
                {
                    RTStrmPrintf(g_pStdErr, "%s: unknown undefined opcode handling method: %s\n", argv0, ValueUnion.psz);
                    return 1;
                }
                break;

            case 'x':
                fHexBytes = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                break;

            default:
                RTStrmPrintf(g_pStdErr, "%s: syntax error: %Rrc\n", argv0, ch);
                return 1;
        }
    }
    int iArg = GetState.iNext - 1; /** @todo Not pretty, add RTGetOptInit flag for this. */
    if (iArg >= argc)
        return Usage(argv0);

    int rc = VINF_SUCCESS;
    if (fHexBytes)
    {
        /*
         * Convert the remaining arguments from a hex byte string into
         * a buffer that we disassemble.
         */
        size_t      cb = 0;
        uint8_t    *pb = NULL;
        for ( ; iArg < argc; iArg++)
        {
            const char *psz = argv[iArg];
            while (*psz)
            {
                /** @todo this stuff belongs in IPRT, same stuff as mac address reading. Could be reused for IPv6 with a different item size.*/
                /* skip white space */
                while (isspace(*psz))
                    psz++;
                if (!*psz)
                    break;

                /* one digit followed by a space or EOS, or two digits. */
                int iNum = HexDigitToNum(*psz++);
                if (iNum == -1)
                    return 1;
                if (!isspace(*psz) && *psz)
                {
                    int iDigit = HexDigitToNum(*psz++);
                    if (iDigit == -1)
                        return 1;
                    iNum = iNum * 16 + iDigit;
                }

                /* add the byte */
                if (!(cb % 4 /*64*/))
                {
                    pb = (uint8_t *)RTMemRealloc(pb, cb + 64);
                    if (!pb)
                    {
                        RTPrintf("%s: error: RTMemRealloc failed\n", argv[0]);
                        return 1;
                    }
                }
                pb[cb++] = (uint8_t)iNum;
            }
        }

        /*
         * Disassemble it.
         */
        rc = MyDisasmBlock(argv0, enmCpuMode, uAddress, pb, cb, enmStyle, fListing, enmUndefOp);
    }
    else
    {
        /*
         * Process the files.
         */
        for ( ; iArg < argc; iArg++)
        {
            /*
             * Read the file into memory.
             */
            void   *pvFile;
            size_t  cbFile;
            rc = RTFileReadAllEx(argv[iArg], off, cbMax, RTFILE_RDALL_O_DENY_NONE, &pvFile, &cbFile);
            if (RT_FAILURE(rc))
            {
                RTStrmPrintf(g_pStdErr, "%s: %s: %Rrc\n", argv0, argv[iArg], rc);
                break;
            }

            /*
             * Disassemble it.
             */
            rc = MyDisasmBlock(argv0, enmCpuMode, uAddress, (uint8_t *)pvFile, cbFile, enmStyle, fListing, enmUndefOp);
            if (RT_FAILURE(rc))
                break;
        }
    }

    return RT_SUCCESS(rc) ? 0 : 1;
}

