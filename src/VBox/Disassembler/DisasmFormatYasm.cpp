/* $Id$ */
/** @file
 * VBox Disassembler - Yasm(/Nasm) Style Formatter.
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
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
#include <VBox/dis.h>
#include "DisasmInternal.h"
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szSpaces[] =
"                                                                               ";
static const char g_aszYasmRegGen8[20][5] =
{
    "al\0\0", "cl\0\0", "dl\0\0", "bl\0\0", "ah\0\0", "ch\0\0", "dh\0\0", "bh\0\0", "r8b\0",  "r9b\0",  "r10b",  "r11b",  "r12b",  "r13b",  "r14b",  "r15b",  "spl\0",  "bpl\0",  "sil\0",  "dil\0"
};
static const char g_aszYasmRegGen16[16][5] =
{
    "ax\0\0", "cx\0\0", "dx\0\0", "bx\0\0", "sp\0\0", "bp\0\0", "si\0\0", "di\0\0", "r8w\0",  "r9w\0",  "r10w",  "r11w",  "r12w",  "r13w",  "r14w",  "r15w"
};
static const char g_aszYasmRegGen1616[8][6] =
{
    "bx+si", "bx+di", "bp+si", "bp+di", "si\0\0\0", "di\0\0\0", "bp\0\0\0", "bx\0\0\0"
};
static const char g_aszYasmRegGen32[16][5] =
{
    "eax\0",  "ecx\0",  "edx\0",  "ebx\0",  "esp\0",  "ebp\0",  "esi\0",  "edi\0",  "r8d\0",  "r9d\0",  "r10d",  "r11d",  "r12d",  "r13d",  "r14d",  "r15d"
};
static const char g_aszYasmRegGen64[16][4] =
{
    "rax",    "rcx",    "rdx",    "rbx",    "rsp",    "rbp",    "rsi",    "rdi",    "r8\0",   "r9\0",   "r10",   "r11",   "r12",   "r13",   "r14",   "r15"
};
static const char g_aszYasmRegSeg[6][3] =
{
    "es",     "cs",     "ss",      "ds",    "fs",     "gs"
};
static const char g_aszYasmRegFP[8][4] =
{
    "st0",    "st1",    "st2",    "st3",    "st4",    "st5",    "st6",    "st7"
};
static const char g_aszYasmRegMMX[8][4] =
{
    "mm0",    "mm1",    "mm2",    "mm3",    "mm4",    "mm5",    "mm6",    "mm7"
};
static const char g_aszYasmRegXMM[16][6] =
{
    "xmm0\0", "xmm1\0", "xmm2\0", "xmm3\0", "xmm4\0", "xmm5\0", "xmm6\0", "xmm7\0", "xmm8\0", "xmm9\0", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
};
static const char g_aszYasmRegCRx[16][5] =
{
    "cr0\0",  "cr1\0",  "cr2\0",  "cr3\0",  "cr4\0",  "cr5\0",  "cr6\0",  "cr7\0",  "cr8\0",  "cr9\0",  "cr10",  "cr11",  "cr12",  "cr13",  "cr14",  "cr15"
};
static const char g_aszYasmRegDRx[16][5] =
{
    "dr0\0",  "dr1\0",  "dr2\0",  "dr3\0",  "dr4\0",  "dr5\0",  "dr6\0",  "dr7\0",  "dr8\0",  "dr9\0",  "dr10",  "dr11",  "dr12",  "dr13",  "dr14",  "dr15"
};
static const char g_aszYasmRegTRx[16][5] =
{
    "tr0\0",  "tr1\0",  "tr2\0",  "tr3\0",  "tr4\0",  "tr5\0",  "tr6\0",  "tr7\0",  "tr8\0",  "tr9\0",  "tr10",  "tr11",  "tr12",  "tr13",  "tr14",  "tr15"
};



/**
 * Gets the base register name for the given parameter.
 *
 * @returns Pointer to the register name.
 * @param   pCpu        The disassembler cpu state.
 * @param   pParam      The parameter.
 * @param   pcchReg     Where to store the length of the name.
 */
static const char *disasmFormatYasmBaseReg(PCDISCPUSTATE pCpu, PCDISOPPARAM pParam, size_t *pcchReg)
{
    switch (pParam->fUse & (  DISUSE_REG_GEN8 | DISUSE_REG_GEN16 | DISUSE_REG_GEN32 | DISUSE_REG_GEN64
                            | DISUSE_REG_FP   | DISUSE_REG_MMX   | DISUSE_REG_XMM   | DISUSE_REG_CR
                            | DISUSE_REG_DBG  | DISUSE_REG_SEG   | DISUSE_REG_TEST))

    {
        case DISUSE_REG_GEN8:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen8));
            const char *psz = g_aszYasmRegGen8[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISUSE_REG_GEN16:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen16));
            const char *psz = g_aszYasmRegGen16[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISUSE_REG_GEN32:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen32));
            const char *psz = g_aszYasmRegGen32[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISUSE_REG_GEN64:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen64));
            const char *psz = g_aszYasmRegGen64[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISUSE_REG_FP:
        {
            Assert(pParam->base.reg_fp < RT_ELEMENTS(g_aszYasmRegFP));
            const char *psz = g_aszYasmRegFP[pParam->base.reg_fp];
            *pcchReg = 3;
            return psz;
        }

        case DISUSE_REG_MMX:
        {
            Assert(pParam->base.reg_mmx < RT_ELEMENTS(g_aszYasmRegMMX));
            const char *psz = g_aszYasmRegMMX[pParam->base.reg_mmx];
            *pcchReg = 3;
            return psz;
        }

        case DISUSE_REG_XMM:
        {
            Assert(pParam->base.reg_xmm < RT_ELEMENTS(g_aszYasmRegXMM));
            const char *psz = g_aszYasmRegXMM[pParam->base.reg_mmx];
            *pcchReg = 4 + !!psz[4];
            return psz;
        }

        case DISUSE_REG_CR:
        {
            Assert(pParam->base.reg_ctrl < RT_ELEMENTS(g_aszYasmRegCRx));
            const char *psz = g_aszYasmRegCRx[pParam->base.reg_ctrl];
            *pcchReg = 3;
            return psz;
        }

        case DISUSE_REG_DBG:
        {
            Assert(pParam->base.reg_dbg < RT_ELEMENTS(g_aszYasmRegDRx));
            const char *psz = g_aszYasmRegDRx[pParam->base.reg_dbg];
            *pcchReg = 3;
            return psz;
        }

        case DISUSE_REG_SEG:
        {
            Assert(pParam->base.reg_seg < RT_ELEMENTS(g_aszYasmRegCRx));
            const char *psz = g_aszYasmRegSeg[pParam->base.reg_seg];
            *pcchReg = 2;
            return psz;
        }

        case DISUSE_REG_TEST:
        {
            Assert(pParam->base.reg_test < RT_ELEMENTS(g_aszYasmRegTRx));
            const char *psz = g_aszYasmRegTRx[pParam->base.reg_test];
            *pcchReg = 3;
            return psz;
        }

        default:
            AssertMsgFailed(("%#x\n", pParam->fUse));
            *pcchReg = 3;
            return "r??";
    }
}


/**
 * Gets the index register name for the given parameter.
 *
 * @returns The index register name.
 * @param   pCpu        The disassembler cpu state.
 * @param   pParam      The parameter.
 * @param   pcchReg     Where to store the length of the name.
 */
static const char *disasmFormatYasmIndexReg(PCDISCPUSTATE pCpu, PCDISOPPARAM pParam, size_t *pcchReg)
{
    switch (pCpu->uAddrMode)
    {
        case DISCPUMODE_16BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen16));
            const char *psz = g_aszYasmRegGen16[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISCPUMODE_32BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen32));
            const char *psz = g_aszYasmRegGen32[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case DISCPUMODE_64BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen64));
            const char *psz = g_aszYasmRegGen64[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        default:
            AssertMsgFailed(("%#x %#x\n", pParam->fUse, pCpu->uAddrMode));
            *pcchReg = 3;
            return "r??";
    }
}


/**
 * Formats the current instruction in Yasm (/ Nasm) style.
 *
 *
 * @returns The number of output characters. If this is >= cchBuf, then the content
 *          of pszBuf will be truncated.
 * @param   pCpu            Pointer to the disassembler CPU state.
 * @param   pszBuf          The output buffer.
 * @param   cchBuf          The size of the output buffer.
 * @param   fFlags          Format flags, see DIS_FORMAT_FLAGS_*.
 * @param   pfnGetSymbol    Get symbol name for a jmp or call target address. Optional.
 * @param   pvUser          User argument for pfnGetSymbol.
 */
DISDECL(size_t) DISFormatYasmEx(PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf, uint32_t fFlags,
                                PFNDISGETSYMBOL pfnGetSymbol, void *pvUser)
{
    /*
     * Input validation and massaging.
     */
    AssertPtr(pCpu);
    AssertPtrNull(pszBuf);
    Assert(pszBuf || !cchBuf);
    AssertPtrNull(pfnGetSymbol);
    AssertMsg(DIS_FMT_FLAGS_IS_VALID(fFlags), ("%#x\n", fFlags));
    if (fFlags & DIS_FMT_FLAGS_ADDR_COMMENT)
        fFlags = (fFlags & ~DIS_FMT_FLAGS_ADDR_LEFT) | DIS_FMT_FLAGS_ADDR_RIGHT;
    if (fFlags & DIS_FMT_FLAGS_BYTES_COMMENT)
        fFlags = (fFlags & ~DIS_FMT_FLAGS_BYTES_LEFT) | DIS_FMT_FLAGS_BYTES_RIGHT;

    PCDISOPCODE const pOp = pCpu->pCurInstr;

    /*
     * Output macros
     */
    char           *pszDst = pszBuf;
    size_t          cchDst = cchBuf;
    size_t          cchOutput = 0;
#define PUT_C(ch)       \
            do { \
                cchOutput++; \
                if (cchDst > 1) \
                { \
                    cchDst--; \
                    *pszDst++ = (ch); \
                } \
            } while (0)
#define PUT_STR(pszSrc, cchSrc) \
            do { \
                cchOutput += (cchSrc); \
                if (cchDst > (cchSrc)) \
                { \
                    memcpy(pszDst, (pszSrc), (cchSrc)); \
                    pszDst += (cchSrc); \
                    cchDst -= (cchSrc); \
                } \
                else if (cchDst > 1) \
                { \
                    memcpy(pszDst, (pszSrc), cchDst - 1); \
                    pszDst += cchDst - 1; \
                    cchDst = 1; \
                } \
            } while (0)
#define PUT_SZ(sz) \
            PUT_STR((sz), sizeof(sz) - 1)
#define PUT_SZ_STRICT(szStrict, szRelaxed) \
            do { if (fFlags & DIS_FMT_FLAGS_STRICT) PUT_SZ(szStrict); else PUT_SZ(szRelaxed); } while (0)
#define PUT_PSZ(psz) \
            do { const size_t cchTmp = strlen(psz); PUT_STR((psz), cchTmp); } while (0)
#define PUT_NUM(cch, fmt, num) \
            do { \
                 cchOutput += (cch); \
                 if (cchDst > 1) \
                 { \
                    const size_t cchTmp = RTStrPrintf(pszDst, cchDst, fmt, (num)); \
                    pszDst += cchTmp; \
                    cchDst -= cchTmp; \
                    Assert(cchTmp == (cch) || cchDst == 1); \
                 } \
            } while (0)
/** @todo add two flags for choosing between %X / %x and h / 0x. */
#define PUT_NUM_8(num)  PUT_NUM(4,  "0%02xh", (uint8_t)(num))
#define PUT_NUM_16(num) PUT_NUM(6,  "0%04xh", (uint16_t)(num))
#define PUT_NUM_32(num) PUT_NUM(10, "0%08xh", (uint32_t)(num))
#define PUT_NUM_64(num) PUT_NUM(18, "0%016RX64h", (uint64_t)(num))

#define PUT_NUM_SIGN(cch, fmt, num, stype, utype) \
            do { \
                if ((stype)(num) >= 0) \
                { \
                    PUT_C('+'); \
                    PUT_NUM(cch, fmt, (utype)(num)); \
                } \
                else \
                { \
                    PUT_C('-'); \
                    PUT_NUM(cch, fmt, (utype)-(stype)(num)); \
                } \
            } while (0)
#define PUT_NUM_S8(num)  PUT_NUM_SIGN(4,  "0%02xh", num, int8_t,  uint8_t)
#define PUT_NUM_S16(num) PUT_NUM_SIGN(6,  "0%04xh", num, int16_t, uint16_t)
#define PUT_NUM_S32(num) PUT_NUM_SIGN(10, "0%08xh", num, int32_t, uint32_t)
#define PUT_NUM_S64(num) PUT_NUM_SIGN(18, "0%016RX64h", num, int64_t, uint64_t)


    /*
     * The address?
     */
    if (fFlags & DIS_FMT_FLAGS_ADDR_LEFT)
    {
#if HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64
        if (pCpu->uInstrAddr >= _4G)
            PUT_NUM(9, "%08x`", (uint32_t)(pCpu->uInstrAddr >> 32));
#endif
        PUT_NUM(8, "%08x", (uint32_t)pCpu->uInstrAddr);
        PUT_C(' ');
    }

    /*
     * The opcode bytes?
     */
    if (fFlags & DIS_FMT_FLAGS_BYTES_LEFT)
    {
        size_t cchTmp = disFormatBytes(pCpu, pszDst, cchDst, fFlags);
        cchOutput += cchTmp;
        if (cchDst > 1)
        {
            if (cchTmp <= cchDst)
            {
                cchDst -= cchTmp;
                pszDst += cchTmp;
            }
            else
            {
                pszDst += cchDst - 1;
                cchDst = 1;
            }
        }

        /* Some padding to align the instruction. */
        size_t cchPadding = (7 * (2 + !!(fFlags & DIS_FMT_FLAGS_BYTES_SPACED)))
                          + !!(fFlags & DIS_FMT_FLAGS_BYTES_BRACKETS) * 2
                          + 2;
        cchPadding = cchTmp + 1 >= cchPadding ? 1 : cchPadding - cchTmp;
        PUT_STR(g_szSpaces, cchPadding);
    }


    /*
     * Filter out invalid opcodes first as they need special
     * treatment. UD2 is an exception and should be handled normally.
     */
    size_t const offInstruction = cchOutput;
    if (    pOp->uOpcode == OP_INVALID
        ||  (   pOp->uOpcode == OP_ILLUD2
             && (pCpu->fPrefix & DISPREFIX_LOCK)))
    {

    }
    else
    {
        /*
         * Prefixes
         */
        if (pCpu->fPrefix & DISPREFIX_LOCK)
            PUT_SZ("lock ");
        if(pCpu->fPrefix & DISPREFIX_REP)
            PUT_SZ("rep ");
        else if(pCpu->fPrefix & DISPREFIX_REPNE)
            PUT_SZ("repne ");

        /*
         * Adjust the format string to the correct mnemonic
         * or to avoid things the assembler cannot handle correctly.
         */
        char szTmpFmt[48];
        const char *pszFmt = pOp->pszOpcode;
        switch (pOp->uOpcode)
        {
            case OP_JECXZ:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "jcxz %Jb" : pCpu->uOpMode == DISCPUMODE_32BIT ? "jecxz %Jb"   : "jrcxz %Jb";
                break;
            case OP_PUSHF:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "pushfw"   : pCpu->uOpMode == DISCPUMODE_32BIT ? "pushfd"      : "pushfq";
                break;
            case OP_POPF:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "popfw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "popfd"       : "popfq";
                break;
            case OP_PUSHA:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "pushaw"   : "pushad";
                break;
            case OP_POPA:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "popaw"    : "popad";
                break;
            case OP_INSB:
                pszFmt = "insb";
                break;
            case OP_INSWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "insw"     : pCpu->uOpMode == DISCPUMODE_32BIT ? "insd"  : "insq";
                break;
            case OP_OUTSB:
                pszFmt = "outsb";
                break;
            case OP_OUTSWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "outsw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "outsd" : "outsq";
                break;
            case OP_MOVSB:
                pszFmt = "movsb";
                break;
            case OP_MOVSWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "movsw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "movsd" : "movsq";
                break;
            case OP_CMPSB:
                pszFmt = "cmpsb";
                break;
            case OP_CMPWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "cmpsw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "cmpsd" : "cmpsq";
                break;
            case OP_SCASB:
                pszFmt = "scasb";
                break;
            case OP_SCASWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "scasw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "scasd" : "scasq";
                break;
            case OP_LODSB:
                pszFmt = "lodsb";
                break;
            case OP_LODSWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "lodsw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "lodsd" : "lodsq";
                break;
            case OP_STOSB:
                pszFmt = "stosb";
                break;
            case OP_STOSWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "stosw"    : pCpu->uOpMode == DISCPUMODE_32BIT ? "stosd" : "stosq";
                break;
            case OP_CBW:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "cbw"      : pCpu->uOpMode == DISCPUMODE_32BIT ? "cwde"  : "cdqe";
                break;
            case OP_CWD:
                pszFmt = pCpu->uOpMode == DISCPUMODE_16BIT ? "cwd"      : pCpu->uOpMode == DISCPUMODE_32BIT ? "cdq"   : "cqo";
                break;
            case OP_SHL:
                Assert(pszFmt[3] == '/');
                pszFmt += 4;
                break;
            case OP_XLAT:
                pszFmt = "xlatb";
                break;
            case OP_INT3:
                pszFmt = "int3";
                break;

            /*
             * Don't know how to tell yasm to generate complicated nop stuff, so 'db' it.
             */
            case OP_NOP:
                if (pCpu->bOpCode == 0x90)
                    /* fine, fine */;
                else if (pszFmt[sizeof("nop %Ev") - 1] == '/' && pszFmt[sizeof("nop %Ev")] == 'p')
                    pszFmt = "prefetch %Eb";
                else if (pCpu->bOpCode == 0x1f)
                {
                    Assert(pCpu->cbInstr >= 3);
                    PUT_SZ("db 00fh, 01fh,");
                    PUT_NUM_8(pCpu->ModRM.u);
                    for (unsigned i = 3; i < pCpu->cbInstr; i++)
                    {
                        PUT_C(',');
                        PUT_NUM_8(0x90); ///@todo fixme.
                    }
                    pszFmt = "";
                }
                break;

            default:
                /* ST(X) -> stX  (floating point) */
                if (*pszFmt == 'f' && strchr(pszFmt, '('))
                {
                    char *pszFmtDst = szTmpFmt;
                    char ch;
                    do
                    {
                        ch = *pszFmt++;
                        if (ch == 'S' && pszFmt[0] == 'T' && pszFmt[1] == '(')
                        {
                            *pszFmtDst++ = 's';
                            *pszFmtDst++ = 't';
                            pszFmt += 2;
                            ch = *pszFmt;
                            Assert(pszFmt[1] == ')');
                            pszFmt += 2;
                            *pszFmtDst++ = ch;
                        }
                        else
                            *pszFmtDst++ = ch;
                    } while (ch != '\0');
                    pszFmt = szTmpFmt;
                }
                break;

            /*
             * Horrible hacks.
             */
            case OP_FLD:
                if (pCpu->bOpCode == 0xdb) /* m80fp workaround. */
                    *(int *)&pCpu->param1.param &= ~0x1f; /* make it pure OP_PARM_M */
                break;
            case OP_LAR: /* hack w -> v, probably not correct. */
                *(int *)&pCpu->param2.param &= ~0x1f;
                *(int *)&pCpu->param2.param |= OP_PARM_v;
                break;
        }

        /*
         * Formatting context and associated macros.
         */
        PCDISOPPARAM pParam = &pCpu->param1;
        int iParam = 1;

#define PUT_FAR() \
            do { \
                if (    OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p \
                    &&  pOp->uOpcode != OP_LDS /* table bugs? */ \
                    &&  pOp->uOpcode != OP_LES \
                    &&  pOp->uOpcode != OP_LFS \
                    &&  pOp->uOpcode != OP_LGS \
                    &&  pOp->uOpcode != OP_LSS ) \
                    PUT_SZ("far "); \
            } while (0)
        /** @todo mov ah,ch ends up with a byte 'override'... - check if this wasn't fixed. */
        /** @todo drop the work/dword/qword override when the src/dst is a register (except for movsx/movzx). */
#define PUT_SIZE_OVERRIDE() \
            do { \
                switch (OP_PARM_VSUBTYPE(pParam->param)) \
                { \
                    case OP_PARM_v: \
                        switch (pCpu->uOpMode) \
                        { \
                            case DISCPUMODE_16BIT: PUT_SZ("word "); break; \
                            case DISCPUMODE_32BIT: PUT_SZ("dword "); break; \
                            case DISCPUMODE_64BIT: PUT_SZ("qword "); break; \
                            default: break; \
                        } \
                        break; \
                    case OP_PARM_b: PUT_SZ("byte "); break; \
                    case OP_PARM_w: PUT_SZ("word "); break; \
                    case OP_PARM_d: PUT_SZ("dword "); break; \
                    case OP_PARM_q: PUT_SZ("qword "); break; \
                    case OP_PARM_dq: \
                        if (OP_PARM_VTYPE(pParam->param) != OP_PARM_W) /* these are 128 bit, pray they are all unambiguous.. */ \
                            PUT_SZ("qword "); \
                        break; \
                    case OP_PARM_p: break; /* see PUT_FAR */ \
                    case OP_PARM_s: if (pParam->fUse & DISUSE_REG_FP) PUT_SZ("tword "); break; /* ?? */ \
                    case OP_PARM_z: break; \
                    case OP_PARM_NONE: \
                        if (    OP_PARM_VTYPE(pParam->param) == OP_PARM_M \
                            &&  ((pParam->fUse & DISUSE_REG_FP) || pOp->uOpcode == OP_FLD)) \
                            PUT_SZ("tword "); \
                        break; \
                    default:        break; /*no pointer type specified/necessary*/ \
                } \
            } while (0)
        static const char s_szSegPrefix[6][4] = { "es:", "cs:", "ss:", "ds:", "fs:", "gs:" };
#define PUT_SEGMENT_OVERRIDE() \
        do { \
            if (pCpu->fPrefix & DISPREFIX_SEG) \
                PUT_STR(s_szSegPrefix[pCpu->idxSegPrefix], 3); \
        } while (0)


        /*
         * Segment prefixing for instructions that doesn't do memory access.
         */
        if (    (pCpu->fPrefix & DISPREFIX_SEG)
            &&  !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param1.fUse)
            &&  !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param2.fUse)
            &&  !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param3.fUse))
        {
            PUT_STR(s_szSegPrefix[pCpu->idxSegPrefix], 2);
            PUT_C(' ');
        }


        /*
         * The formatting loop.
         */
        RTINTPTR off;
        char szSymbol[128];
        char ch;
        while ((ch = *pszFmt++) != '\0')
        {
            if (ch == '%')
            {
                ch = *pszFmt++;
                switch (ch)
                {
                    /*
                     * ModRM - Register only.
                     */
                    case 'C': /* Control register (ParseModRM / UseModRM). */
                    case 'D': /* Debug register (ParseModRM / UseModRM). */
                    case 'G': /* ModRM selects general register (ParseModRM / UseModRM). */
                    case 'S': /* ModRM byte selects a segment register (ParseModRM / UseModRM). */
                    case 'T': /* ModRM byte selects a test register (ParseModRM / UseModRM). */
                    case 'V': /* ModRM byte selects an XMM/SSE register (ParseModRM / UseModRM). */
                    case 'P': /* ModRM byte selects MMX register (ParseModRM / UseModRM). */
                    {
                        pszFmt += RT_C_IS_ALPHA(pszFmt[0]) ? RT_C_IS_ALPHA(pszFmt[1]) ? 2 : 1 : 0;
                        Assert(!(pParam->fUse & (DISUSE_INDEX | DISUSE_SCALE) /* No SIB here... */));
                        Assert(!(pParam->fUse & (DISUSE_DISPLACEMENT8 | DISUSE_DISPLACEMENT16 | DISUSE_DISPLACEMENT32 | DISUSE_DISPLACEMENT64 | DISUSE_RIPDISPLACEMENT32)));

                        size_t cchReg;
                        const char *pszReg = disasmFormatYasmBaseReg(pCpu, pParam, &cchReg);
                        PUT_STR(pszReg, cchReg);
                        break;
                    }

                    /*
                     * ModRM - Register or memory.
                     */
                    case 'E': /* ModRM specifies parameter (ParseModRM / UseModRM / UseSIB). */
                    case 'Q': /* ModRM byte selects MMX register or memory address (ParseModRM / UseModRM). */
                    case 'R': /* ModRM byte may only refer to a general register (ParseModRM / UseModRM). */
                    case 'W': /* ModRM byte selects an XMM/SSE register or a memory address (ParseModRM / UseModRM). */
                    case 'M': /* ModRM may only refer to memory (ParseModRM / UseModRM). */
                    {
                        pszFmt += RT_C_IS_ALPHA(pszFmt[0]) ? RT_C_IS_ALPHA(pszFmt[1]) ? 2 : 1 : 0;

                        PUT_FAR();
                        uint32_t const fUse = pParam->fUse;
                        if (DISUSE_IS_EFFECTIVE_ADDR(fUse))
                        {
                            /* Work around mov seg,[mem16]  and mov [mem16],seg as these always make a 16-bit mem
                               while the register variants deals with 16, 32 & 64 in the normal fashion. */
                            if (    pParam->param != OP_PARM_Ev
                                ||  pOp->uOpcode != OP_MOV
                                ||  (   pOp->param1 != OP_PARM_Sw
                                     && pOp->param2 != OP_PARM_Sw))
                                PUT_SIZE_OVERRIDE();
                            PUT_C('[');
                        }
                        if (    (fFlags & DIS_FMT_FLAGS_STRICT)
                            &&  (fUse & (DISUSE_DISPLACEMENT8 | DISUSE_DISPLACEMENT16 | DISUSE_DISPLACEMENT32 | DISUSE_DISPLACEMENT64 | DISUSE_RIPDISPLACEMENT32)))
                        {
                            if (   (fUse & DISUSE_DISPLACEMENT8)
                                && !pParam->uDisp.i8)
                                PUT_SZ("byte ");
                            else if (   (fUse & DISUSE_DISPLACEMENT16)
                                     && (int8_t)pParam->uDisp.i16 == (int16_t)pParam->uDisp.i16)
                                PUT_SZ("word ");
                            else if (   (fUse & DISUSE_DISPLACEMENT32)
                                     && (int16_t)pParam->uDisp.i32 == (int32_t)pParam->uDisp.i32) //??
                                PUT_SZ("dword ");
                            else if (   (fUse & DISUSE_DISPLACEMENT64)
                                     && (pCpu->SIB.Bits.Base != 5 || pCpu->ModRM.Bits.Mod != 0)
                                     && (int32_t)pParam->uDisp.i64 == (int64_t)pParam->uDisp.i64) //??
                                PUT_SZ("qword ");
                        }
                        if (DISUSE_IS_EFFECTIVE_ADDR(fUse))
                            PUT_SEGMENT_OVERRIDE();

                        bool fBase =  (fUse & DISUSE_BASE) /* When exactly is DISUSE_BASE supposed to be set? disasmModRMReg doesn't set it. */
                                   || (   (fUse & (  DISUSE_REG_GEN8
                                                   | DISUSE_REG_GEN16
                                                   | DISUSE_REG_GEN32
                                                   | DISUSE_REG_GEN64
                                                   | DISUSE_REG_FP
                                                   | DISUSE_REG_MMX
                                                   | DISUSE_REG_XMM
                                                   | DISUSE_REG_CR
                                                   | DISUSE_REG_DBG
                                                   | DISUSE_REG_SEG
                                                   | DISUSE_REG_TEST ))
                                       && !DISUSE_IS_EFFECTIVE_ADDR(fUse));
                        if (fBase)
                        {
                            size_t cchReg;
                            const char *pszReg = disasmFormatYasmBaseReg(pCpu, pParam, &cchReg);
                            PUT_STR(pszReg, cchReg);
                        }

                        if (fUse & DISUSE_INDEX)
                        {
                            if (fBase)
                                PUT_C('+');

                            size_t cchReg;
                            const char *pszReg = disasmFormatYasmIndexReg(pCpu, pParam, &cchReg);
                            PUT_STR(pszReg, cchReg);

                            if (fUse & DISUSE_SCALE)
                            {
                                PUT_C('*');
                                PUT_C('0' + pParam->scale);
                            }
                        }
                        else
                            Assert(!(fUse & DISUSE_SCALE));

                        if (fUse & (DISUSE_DISPLACEMENT8 | DISUSE_DISPLACEMENT16 | DISUSE_DISPLACEMENT32 | DISUSE_DISPLACEMENT64 | DISUSE_RIPDISPLACEMENT32))
                        {
                            int64_t off2;
                            if (fUse & DISUSE_DISPLACEMENT8)
                                off2 = pParam->uDisp.i8;
                            else if (fUse & DISUSE_DISPLACEMENT16)
                                off2 = pParam->uDisp.i16;
                            else if (fUse & (DISUSE_DISPLACEMENT32 | DISUSE_RIPDISPLACEMENT32))
                                off2 = pParam->uDisp.i32;
                            else if (fUse & DISUSE_DISPLACEMENT64)
                                off2 = pParam->uDisp.i64;
                            else
                            {
                                AssertFailed();
                                off2 = 0;
                            }

                            if (fBase || (fUse & DISUSE_INDEX))
                            {
                                PUT_C(off2 >= 0 ? '+' : '-');
                                if (off2 < 0)
                                    off2 = -off2;
                            }
                            if (fUse & DISUSE_DISPLACEMENT8)
                                PUT_NUM_8( off2);
                            else if (fUse & DISUSE_DISPLACEMENT16)
                                PUT_NUM_16(off2);
                            else if (fUse & DISUSE_DISPLACEMENT32)
                                PUT_NUM_32(off2);
                            else if (fUse & DISUSE_DISPLACEMENT64)
                                PUT_NUM_64(off2);
                            else
                            {
                                PUT_NUM_32(off2);
                                PUT_SZ(" wrt rip"); //??
                            }
                        }

                        if (DISUSE_IS_EFFECTIVE_ADDR(fUse))
                            PUT_C(']');
                        break;
                    }

                    case 'F': /* Eflags register (0 - popf/pushf only, avoided in adjustments above). */
                        AssertFailed();
                        break;

                    case 'I': /* Immediate data (ParseImmByte, ParseImmByteSX, ParseImmV, ParseImmUshort, ParseImmZ). */
                        Assert(*pszFmt == 'b' || *pszFmt == 'v' || *pszFmt == 'w' || *pszFmt == 'z'); pszFmt++;
                        switch (pParam->fUse & (  DISUSE_IMMEDIATE8 | DISUSE_IMMEDIATE16 | DISUSE_IMMEDIATE32 | DISUSE_IMMEDIATE64
                                                | DISUSE_IMMEDIATE16_SX8 | DISUSE_IMMEDIATE32_SX8 | DISUSE_IMMEDIATE64_SX8))
                        {
                            case DISUSE_IMMEDIATE8:
                                if (    (fFlags & DIS_FMT_FLAGS_STRICT)
                                    &&  (   (pOp->param1 >= OP_PARM_REG_GEN8_START && pOp->param1 <= OP_PARM_REG_GEN8_END)
                                         || (pOp->param2 >= OP_PARM_REG_GEN8_START && pOp->param2 <= OP_PARM_REG_GEN8_END))
                                   )
                                    PUT_SZ("strict byte ");
                                PUT_NUM_8(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE16:
                                if (    pCpu->uCpuMode != pCpu->uOpMode
                                    ||  (   (fFlags & DIS_FMT_FLAGS_STRICT)
                                         && (   (int8_t)pParam->parval == (int16_t)pParam->parval
                                             || (pOp->param1 >= OP_PARM_REG_GEN16_START && pOp->param1 <= OP_PARM_REG_GEN16_END)
                                             || (pOp->param2 >= OP_PARM_REG_GEN16_START && pOp->param2 <= OP_PARM_REG_GEN16_END))
                                        )
                                   )
                                {
                                    if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_b)
                                        PUT_SZ_STRICT("strict byte ", "byte ");
                                    else if (   OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_v
                                             || OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_z)
                                        PUT_SZ_STRICT("strict word ", "word ");
                                }
                                PUT_NUM_16(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE16_SX8:
                                PUT_SZ_STRICT("strict byte ", "byte ");
                                PUT_NUM_16(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE32:
                                if (    pCpu->uOpMode != (pCpu->uCpuMode == DISCPUMODE_16BIT ? DISCPUMODE_16BIT : DISCPUMODE_32BIT) /* not perfect */
                                    ||  (   (fFlags & DIS_FMT_FLAGS_STRICT)
                                         && (   (int8_t)pParam->parval == (int32_t)pParam->parval
                                             || (pOp->param1 >= OP_PARM_REG_GEN32_START && pOp->param1 <= OP_PARM_REG_GEN32_END)
                                             || (pOp->param2 >= OP_PARM_REG_GEN32_START && pOp->param2 <= OP_PARM_REG_GEN32_END))
                                        )
                                    )
                                {
                                    if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_b)
                                        PUT_SZ_STRICT("strict byte ", "byte ");
                                    else if (   OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_v
                                             || OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_z)
                                        PUT_SZ_STRICT("strict dword ", "dword ");
                                }
                                PUT_NUM_32(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE32_SX8:
                                PUT_SZ_STRICT("strict byte ", "byte ");
                                PUT_NUM_32(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE64_SX8:
                                PUT_SZ_STRICT("strict byte ", "byte ");
                                PUT_NUM_64(pParam->parval);
                                break;

                            case DISUSE_IMMEDIATE64:
                                PUT_NUM_64(pParam->parval);
                                break;

                            default:
                                AssertFailed();
                                break;
                        }
                        break;

                    case 'J': /* Relative jump offset (ParseImmBRel + ParseImmVRel). */
                    {
                        int32_t offDisplacement;
                        Assert(iParam == 1);
                        bool fPrefix = (fFlags & DIS_FMT_FLAGS_STRICT)
                                    && pOp->uOpcode != OP_CALL
                                    && pOp->uOpcode != OP_LOOP
                                    && pOp->uOpcode != OP_LOOPE
                                    && pOp->uOpcode != OP_LOOPNE
                                    && pOp->uOpcode != OP_JECXZ;
                        if (pOp->uOpcode == OP_CALL)
                            fFlags &= ~DIS_FMT_FLAGS_RELATIVE_BRANCH;

                        if (pParam->fUse & DISUSE_IMMEDIATE8_REL)
                        {
                            if (fPrefix)
                                PUT_SZ("short ");
                            offDisplacement = (int8_t)pParam->parval;
                            Assert(*pszFmt == 'b'); pszFmt++;

                            if (fFlags & DIS_FMT_FLAGS_RELATIVE_BRANCH)
                                PUT_NUM_S8(offDisplacement);
                        }
                        else if (pParam->fUse & DISUSE_IMMEDIATE16_REL)
                        {
                            if (fPrefix)
                                PUT_SZ("near ");
                            offDisplacement = (int16_t)pParam->parval;
                            Assert(*pszFmt == 'v'); pszFmt++;

                            if (fFlags & DIS_FMT_FLAGS_RELATIVE_BRANCH)
                                PUT_NUM_S16(offDisplacement);
                        }
                        else
                        {
                            if (fPrefix)
                                PUT_SZ("near ");
                            offDisplacement = (int32_t)pParam->parval;
                            Assert(pParam->fUse & (DISUSE_IMMEDIATE32_REL|DISUSE_IMMEDIATE64_REL));
                            Assert(*pszFmt == 'v'); pszFmt++;

                            if (fFlags & DIS_FMT_FLAGS_RELATIVE_BRANCH)
                                PUT_NUM_S32(offDisplacement);
                        }
                        if (fFlags & DIS_FMT_FLAGS_RELATIVE_BRANCH)
                            PUT_SZ(" (");

                        RTUINTPTR uTrgAddr = pCpu->uInstrAddr + pCpu->cbInstr + offDisplacement;
                        if (pCpu->uCpuMode == DISCPUMODE_16BIT)
                            PUT_NUM_16(uTrgAddr);
                        else if (pCpu->uCpuMode == DISCPUMODE_32BIT)
                            PUT_NUM_32(uTrgAddr);
                        else
                            PUT_NUM_64(uTrgAddr);

                        if (pfnGetSymbol)
                        {
                            int rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), uTrgAddr, szSymbol, sizeof(szSymbol), &off, pvUser);
                            if (RT_SUCCESS(rc))
                            {
                                PUT_SZ(" [");
                                PUT_PSZ(szSymbol);
                                if (off != 0)
                                {
                                    if ((int8_t)off == off)
                                        PUT_NUM_S8(off);
                                    else if ((int16_t)off == off)
                                        PUT_NUM_S16(off);
                                    else if ((int32_t)off == off)
                                        PUT_NUM_S32(off);
                                    else
                                        PUT_NUM_S64(off);
                                }
                                PUT_C(']');
                            }
                        }

                        if (fFlags & DIS_FMT_FLAGS_RELATIVE_BRANCH)
                            PUT_C(')');
                        break;
                    }

                    case 'A': /* Direct (jump/call) address (ParseImmAddr). */
                    {
                        Assert(*pszFmt == 'p'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_SEGMENT_OVERRIDE();
                        int rc = VERR_SYMBOL_NOT_FOUND;
                        switch (pParam->fUse & (DISUSE_IMMEDIATE_ADDR_16_16 | DISUSE_IMMEDIATE_ADDR_16_32 | DISUSE_DISPLACEMENT64 | DISUSE_DISPLACEMENT32 | DISUSE_DISPLACEMENT16))
                        {
                            case DISUSE_IMMEDIATE_ADDR_16_16:
                                PUT_NUM_16(pParam->parval >> 16);
                                PUT_C(':');
                                PUT_NUM_16(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_VALUE(pParam->parval >> 16), (uint16_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_IMMEDIATE_ADDR_16_32:
                                PUT_NUM_16(pParam->parval >> 32);
                                PUT_C(':');
                                PUT_NUM_32(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_VALUE(pParam->parval >> 16), (uint32_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT16:
                                PUT_NUM_16(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), (uint16_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT32:
                                PUT_NUM_32(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), (uint32_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT64:
                                PUT_NUM_64(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), (uint64_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            default:
                                AssertFailed();
                                break;
                        }

                        if (RT_SUCCESS(rc))
                        {
                            PUT_SZ(" [");
                            PUT_PSZ(szSymbol);
                            if (off != 0)
                            {
                                if ((int8_t)off == off)
                                    PUT_NUM_S8(off);
                                else if ((int16_t)off == off)
                                    PUT_NUM_S16(off);
                                else if ((int32_t)off == off)
                                    PUT_NUM_S32(off);
                                else
                                    PUT_NUM_S64(off);
                            }
                            PUT_C(']');
                        }
                        break;
                    }

                    case 'O': /* No ModRM byte (ParseImmAddr). */
                    {
                        Assert(*pszFmt == 'b' || *pszFmt == 'v'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_C('[');
                        PUT_SEGMENT_OVERRIDE();
                        int rc = VERR_SYMBOL_NOT_FOUND;
                        switch (pParam->fUse & (DISUSE_IMMEDIATE_ADDR_16_16 | DISUSE_IMMEDIATE_ADDR_16_32 | DISUSE_DISPLACEMENT64 | DISUSE_DISPLACEMENT32 | DISUSE_DISPLACEMENT16))
                        {
                            case DISUSE_IMMEDIATE_ADDR_16_16:
                                PUT_NUM_16(pParam->parval >> 16);
                                PUT_C(':');
                                PUT_NUM_16(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_VALUE(pParam->parval >> 16), (uint16_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_IMMEDIATE_ADDR_16_32:
                                PUT_NUM_16(pParam->parval >> 32);
                                PUT_C(':');
                                PUT_NUM_32(pParam->parval);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_VALUE(pParam->parval >> 16), (uint32_t)pParam->parval, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT16:
                                PUT_NUM_16(pParam->uDisp.i16);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), pParam->uDisp.u16, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT32:
                                PUT_NUM_32(pParam->uDisp.i32);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), pParam->uDisp.u32, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            case DISUSE_DISPLACEMENT64:
                                PUT_NUM_64(pParam->uDisp.i64);
                                if (pfnGetSymbol)
                                    rc = pfnGetSymbol(pCpu, DIS_FMT_SEL_FROM_REG(DISSELREG_CS), pParam->uDisp.u64, szSymbol, sizeof(szSymbol), &off, pvUser);
                                break;
                            default:
                                AssertFailed();
                                break;
                        }
                        PUT_C(']');

                        if (RT_SUCCESS(rc))
                        {
                            PUT_SZ(" (");
                            PUT_PSZ(szSymbol);
                            if (off != 0)
                            {
                                if ((int8_t)off == off)
                                    PUT_NUM_S8(off);
                                else if ((int16_t)off == off)
                                    PUT_NUM_S16(off);
                                else if ((int32_t)off == off)
                                    PUT_NUM_S32(off);
                                else
                                    PUT_NUM_S64(off);
                            }
                            PUT_C(')');
                        }
                        break;
                    }

                    case 'X': /* DS:SI (ParseXb, ParseXv). */
                    case 'Y': /* ES:DI (ParseYb, ParseYv). */
                    {
                        Assert(*pszFmt == 'b' || *pszFmt == 'v'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_C('[');
                        if (pParam->fUse & DISUSE_POINTER_DS_BASED)
                            PUT_SZ("ds:");
                        else
                            PUT_SZ("es:");

                        size_t cchReg;
                        const char *pszReg = disasmFormatYasmBaseReg(pCpu, pParam, &cchReg);
                        PUT_STR(pszReg, cchReg);
                        PUT_C(']');
                        break;
                    }

                    case 'e': /* Register based on operand size (e.g. %eAX) (ParseFixedReg). */
                    {
                        Assert(RT_C_IS_ALPHA(pszFmt[0]) && RT_C_IS_ALPHA(pszFmt[1]) && !RT_C_IS_ALPHA(pszFmt[2])); pszFmt += 2;
                        size_t cchReg;
                        const char *pszReg = disasmFormatYasmBaseReg(pCpu, pParam, &cchReg);
                        PUT_STR(pszReg, cchReg);
                        break;
                    }

                    default:
                        AssertMsgFailed(("%c%s!\n", ch, pszFmt));
                        break;
                }
                AssertMsg(*pszFmt == ',' || *pszFmt == '\0', ("%c%s\n", ch, pszFmt));
            }
            else
            {
                PUT_C(ch);
                if (ch == ',')
                {
                    Assert(*pszFmt != ' ');
                    PUT_C(' ');
                    switch (++iParam)
                    {
                        case 2: pParam = &pCpu->param2; break;
                        case 3: pParam = &pCpu->param3; break;
                        default: pParam = NULL; break;
                    }
                }
            }
        } /* while more to format */
    }

    /*
     * Any additional output to the right of the instruction?
     */
    if (fFlags & (DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_ADDR_RIGHT))
    {
        /* some up front padding. */
        size_t cchPadding = cchOutput - offInstruction;
        cchPadding = cchPadding + 1 >= 42 ? 1 : 42 - cchPadding;
        PUT_STR(g_szSpaces, cchPadding);

        /* comment? */
        if (fFlags & (DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_ADDR_RIGHT))
            PUT_SZ(";");

        /*
         * The address?
         */
        if (fFlags & DIS_FMT_FLAGS_ADDR_RIGHT)
        {
            PUT_C(' ');
#if HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64
            if (pCpu->uInstrAddr >= _4G)
                PUT_NUM(9, "%08x`", (uint32_t)(pCpu->uInstrAddr >> 32));
#endif
            PUT_NUM(8, "%08x", (uint32_t)pCpu->uInstrAddr);
        }

        /*
         * Opcode bytes?
         */
        if (fFlags & DIS_FMT_FLAGS_BYTES_RIGHT)
        {
            PUT_C(' ');
            size_t cchTmp = disFormatBytes(pCpu, pszDst, cchDst, fFlags);
            cchOutput += cchTmp;
            if (cchTmp >= cchDst)
                cchTmp = cchDst - (cchDst != 0);
            cchDst -= cchTmp;
            pszDst += cchTmp;
        }
    }

    /*
     * Terminate it - on overflow we'll have reserved one byte for this.
     */
    if (cchDst > 0)
        *pszDst = '\0';
    else
        Assert(!cchBuf);

    /* clean up macros */
#undef PUT_PSZ
#undef PUT_SZ
#undef PUT_STR
#undef PUT_C
    return cchOutput;
}


/**
 * Formats the current instruction in Yasm (/ Nasm) style.
 *
 * This is a simplified version of DISFormatYasmEx() provided for your convenience.
 *
 *
 * @returns The number of output characters. If this is >= cchBuf, then the content
 *          of pszBuf will be truncated.
 * @param   pCpu    Pointer to the disassembler CPU state.
 * @param   pszBuf  The output buffer.
 * @param   cchBuf  The size of the output buffer.
 */
DISDECL(size_t) DISFormatYasm(PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf)
{
    return DISFormatYasmEx(pCpu, pszBuf, cchBuf, 0 /* fFlags */, NULL /* pfnGetSymbol */, NULL /* pvUser */);
}


/**
 * Checks if the encoding of the given disassembled instruction is something we
 * can never get YASM to produce.
 *
 * @returns true if it's odd, false if it isn't.
 * @param   pCpu        The disassembler output.  The byte fetcher callback will
 *                      be used if present as we might need to fetch opcode
 *                      bytes.
 */
DISDECL(bool) DISFormatYasmIsOddEncoding(PDISCPUSTATE pCpu)
{
    /*
     * Mod rm + SIB: Check for duplicate EBP encodings that yasm won't use for very good reasons.
     */
    if (    pCpu->uAddrMode != DISCPUMODE_16BIT ///@todo correct?
        &&  pCpu->ModRM.Bits.Rm == 4
        &&  pCpu->ModRM.Bits.Mod != 3)
    {
        /* No scaled index SIB (index=4), except for ESP. */
        if (    pCpu->SIB.Bits.Index == 4
            &&  pCpu->SIB.Bits.Base != 4)
            return true;

        /* EBP + displacement */
        if (    pCpu->ModRM.Bits.Mod != 0
             && pCpu->SIB.Bits.Base == 5
             && pCpu->SIB.Bits.Scale == 0)
            return true;
    }

    /*
     * Seems to be an instruction alias here, but I cannot find any docs on it... hrmpf!
     */
    if (    pCpu->pCurInstr->uOpcode == OP_SHL
        &&  pCpu->ModRM.Bits.Reg == 6)
        return true;

    /*
     * Check for multiple prefixes of the same kind.
     */
    uint32_t fPrefixes = 0;
    for (uint32_t offOpcode = 0; offOpcode < RT_ELEMENTS(pCpu->abInstr); offOpcode++)
    {
        uint32_t f;
        switch (pCpu->abInstr[offOpcode])
        {
            case 0xf0:
                f = DISPREFIX_LOCK;
                break;

            case 0xf2:
            case 0xf3:
                f = DISPREFIX_REP; /* yes, both */
                break;

            case 0x2e:
            case 0x3e:
            case 0x26:
            case 0x36:
            case 0x64:
            case 0x65:
                f = DISPREFIX_SEG;
                break;

            case 0x66:
                f = DISPREFIX_OPSIZE;
                break;

            case 0x67:
                f = DISPREFIX_ADDRSIZE;
                break;

            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                f = pCpu->uCpuMode == DISCPUMODE_64BIT ? DISPREFIX_REX : 0;
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
    if (fPrefixes & DISPREFIX_SEG)
    {
        /* no effective address which it may apply to. */
        Assert((pCpu->fPrefix & DISPREFIX_SEG) || pCpu->uCpuMode == DISCPUMODE_64BIT);
        if (    !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param1.fUse)
            &&  !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param2.fUse)
            &&  !DISUSE_IS_EFFECTIVE_ADDR(pCpu->param3.fUse))
            return true;
    }

    /* fixed register + addr override doesn't go down all that well. */
    if (fPrefixes & DISPREFIX_ADDRSIZE)
    {
        Assert(pCpu->fPrefix & DISPREFIX_ADDRSIZE);
        if (    pCpu->pCurInstr->param3 == OP_PARM_NONE
            &&  pCpu->pCurInstr->param2 == OP_PARM_NONE
            &&  (   pCpu->pCurInstr->param1 >= OP_PARM_REG_GEN32_START
                 && pCpu->pCurInstr->param1 <= OP_PARM_REG_GEN32_END))
            return true;
    }

    /* Almost all prefixes are bad. */
    if (fPrefixes)
    {
        switch (pCpu->pCurInstr->uOpcode)
        {
            /* nop w/ prefix(es). */
            case OP_NOP:
                return true;

            case OP_JMP:
                if (    pCpu->pCurInstr->param1 != OP_PARM_Jb
                    &&  pCpu->pCurInstr->param1 != OP_PARM_Jv)
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
    if (fPrefixes & ~DISPREFIX_SEG)
    {
        switch (pCpu->pCurInstr->uOpcode)
        {
            case OP_POP:
            case OP_PUSH:
                if (    pCpu->pCurInstr->param1 >= OP_PARM_REG_SEG_START
                    &&  pCpu->pCurInstr->param1 <= OP_PARM_REG_SEG_END)
                    return true;
                if (    (fPrefixes & ~DISPREFIX_OPSIZE)
                    &&  pCpu->pCurInstr->param1 >= OP_PARM_REG_GEN32_START
                    &&  pCpu->pCurInstr->param1 <= OP_PARM_REG_GEN32_END)
                    return true;
                break;

            case OP_POPA:
            case OP_POPF:
            case OP_PUSHA:
            case OP_PUSHF:
                if (fPrefixes & ~DISPREFIX_OPSIZE)
                    return true;
                break;
        }
    }

    /* Implicit 8-bit register instructions doesn't mix with operand size. */
    if (    (fPrefixes & DISPREFIX_OPSIZE)
        &&  (   (   pCpu->pCurInstr->param1 == OP_PARM_Gb /* r8 */
                 && pCpu->pCurInstr->param2 == OP_PARM_Eb /* r8/mem8 */)
             || (   pCpu->pCurInstr->param2 == OP_PARM_Gb /* r8 */
                 && pCpu->pCurInstr->param1 == OP_PARM_Eb /* r8/mem8 */))
       )
    {
        switch (pCpu->pCurInstr->uOpcode)
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
    if (pCpu->ModRM.Bits.Mod == 3 /* reg,reg */)
    {
        switch (pCpu->pCurInstr->uOpcode)
        {
            case OP_ADD:
            case OP_OR:
            case OP_ADC:
            case OP_SBB:
            case OP_AND:
            case OP_SUB:
            case OP_XOR:
            case OP_CMP:
                if (    (    pCpu->pCurInstr->param1 == OP_PARM_Gb /* r8 */
                         && pCpu->pCurInstr->param2 == OP_PARM_Eb /* r8/mem8 */)
                    ||  (    pCpu->pCurInstr->param1 == OP_PARM_Gv /* rX */
                         && pCpu->pCurInstr->param2 == OP_PARM_Ev /* rX/memX */))
                    return true;

                /* 82 (see table A-6). */
                if (pCpu->bOpCode == 0x82)
                    return true;
                break;

            /* ff /0, fe /0, ff /1, fe /0 */
            case OP_DEC:
            case OP_INC:
                return true;

            case OP_POP:
            case OP_PUSH:
                Assert(pCpu->bOpCode == 0x8f);
                return true;

            case OP_MOV:
                if (   pCpu->bOpCode == 0x8a
                    || pCpu->bOpCode == 0x8b)
                    return true;
                break;

            default:
                break;
        }
    }

    /* shl eax,1 will be assembled to the form without the immediate byte. */
    if (    pCpu->pCurInstr->param2 == OP_PARM_Ib
        &&  (uint8_t)pCpu->param2.parval == 1)
    {
        switch (pCpu->pCurInstr->uOpcode)
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
    if (pCpu->bOpCode == 0x82)
    {
        switch (pCpu->pCurInstr->uOpcode)
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
    switch (pCpu->pCurInstr->uOpcode)
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
            AssertMsg(pCpu->bOpCode >= 0x90 && pCpu->bOpCode <= 0x9f, ("%#x\n", pCpu->bOpCode));
            if (pCpu->ModRM.Bits.Reg != 2)
                return true;
            break;
    }

    /*
     * The MOVZX reg32,mem16 instruction without an operand size prefix
     * doesn't quite make sense...
     */
    if (    pCpu->pCurInstr->uOpcode == OP_MOVZX
        &&  pCpu->bOpCode == 0xB7
        &&  (pCpu->uCpuMode == DISCPUMODE_16BIT) != !!(fPrefixes & DISPREFIX_OPSIZE))
        return true;

    return false;
}

