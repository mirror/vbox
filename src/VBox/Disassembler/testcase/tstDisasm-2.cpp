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
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/runtime.h>
#include <VBox/err.h>
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


/*
 * Non-logging builds doesn't to full formatting so we must do it on our own.
 * This should probably be moved into the disassembler later as it's needed for
 * the vbox debugger as well.
 *
 * Comment in USE_MY_FORMATTER to enable it.
 */
#define USE_MY_FORMATTER

#ifdef USE_MY_FORMATTER
static const char g_aszYasmRegGen8x86[8][4] =
{
    "al\0",   "cl\0",   "dl\0",   "bl\0",   "ah\0",   "ch\0",   "dh\0",   "bh\0"
};
static const char g_aszYasmRegGen8Amd64[16][5] =
{
    "al\0\0", "cl\0\0", "dl\0\0", "bl\0\0", "spb\0",  "bpb\0",  "sib\0",  "dib\0",  "r8b\0",  "r9b\0",  "r10b",  "r11b",  "r12b",  "r13b",  "r14b",  "r15b"
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


DECLINLINE(const char *) MyDisasYasmFormatBaseReg(DISCPUSTATE const *pCpu, PCOP_PARAMETER pParam, size_t *pcchReg, bool fReg1616)
{
    switch (pParam->flags & (  USE_REG_GEN8 | USE_REG_GEN16 | USE_REG_GEN32 | USE_REG_GEN64
                             | USE_REG_FP   | USE_REG_MMX   | USE_REG_XMM   | USE_REG_CR
                             | USE_REG_DBG  | USE_REG_SEG   | USE_REG_TEST))

    {
        case USE_REG_GEN8:
            if (pCpu->opmode == CPUMODE_64BIT)
            {
                Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen8Amd64));
                const char *psz = g_aszYasmRegGen8Amd64[pParam->base.reg_gen];
                *pcchReg = 2 + !!psz[2] + !!psz[3];
                return psz;
            }
            *pcchReg = 2;
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen8x86));
            return g_aszYasmRegGen8x86[pParam->base.reg_gen];

        case USE_REG_GEN16:
        {
            if (fReg1616)
            {
                Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen1616));
                const char *psz = g_aszYasmRegGen1616[pParam->base.reg_gen];
                *pcchReg = psz[2] ? 5 : 2;
                return psz;
            }

            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen16));
            const char *psz = g_aszYasmRegGen16[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case USE_REG_GEN32:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen32));
            const char *psz = g_aszYasmRegGen32[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case USE_REG_GEN64:
        {
            Assert(pParam->base.reg_gen < RT_ELEMENTS(g_aszYasmRegGen64));
            const char *psz = g_aszYasmRegGen64[pParam->base.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case USE_REG_FP:
        {
            Assert(pParam->base.reg_fp < RT_ELEMENTS(g_aszYasmRegFP));
            const char *psz = g_aszYasmRegFP[pParam->base.reg_fp];
            *pcchReg = 3;
            return psz;
        }

        case USE_REG_MMX:
        {
            Assert(pParam->base.reg_mmx < RT_ELEMENTS(g_aszYasmRegMMX));
            const char *psz = g_aszYasmRegMMX[pParam->base.reg_mmx];
            *pcchReg = 3;
            return psz;
        }

        case USE_REG_XMM:
        {
            Assert(pParam->base.reg_xmm < RT_ELEMENTS(g_aszYasmRegXMM));
            const char *psz = g_aszYasmRegXMM[pParam->base.reg_mmx];
            *pcchReg = 4 + !!psz[4];
            return psz;
        }

        case USE_REG_CR:
        {
            Assert(pParam->base.reg_ctrl < RT_ELEMENTS(g_aszYasmRegCRx));
            const char *psz = g_aszYasmRegCRx[pParam->base.reg_ctrl];
            *pcchReg = 3;
            return psz;
        }

        case USE_REG_DBG:
        {
            Assert(pParam->base.reg_dbg < RT_ELEMENTS(g_aszYasmRegDRx));
            const char *psz = g_aszYasmRegDRx[pParam->base.reg_dbg];
            *pcchReg = 3;
            return psz;
        }

        case USE_REG_SEG:
        {
            Assert(pParam->base.reg_seg < RT_ELEMENTS(g_aszYasmRegCRx));
            const char *psz = g_aszYasmRegSeg[pParam->base.reg_seg];
            *pcchReg = 2;
            return psz;
        }

        case USE_REG_TEST:
        {
            Assert(pParam->base.reg_test < RT_ELEMENTS(g_aszYasmRegTRx));
            const char *psz = g_aszYasmRegTRx[pParam->base.reg_test];
            *pcchReg = 3;
            return psz;
        }

        default:
            AssertMsgFailed(("%#x\n", pParam->flags));
            *pcchReg = 3;
            return "r??";
    }
}

DECLINLINE(const char *) MyDisasYasmFormatIndexReg(DISCPUSTATE const *pCpu, PCOP_PARAMETER pParam, size_t *pcchReg)
{
    switch (pCpu->addrmode)
    {
        case CPUMODE_16BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen16));
            const char *psz = g_aszYasmRegGen16[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case CPUMODE_32BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen32));
            const char *psz = g_aszYasmRegGen32[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        case CPUMODE_64BIT:
        {
            Assert(pParam->index.reg_gen < RT_ELEMENTS(g_aszYasmRegGen64));
            const char *psz = g_aszYasmRegGen64[pParam->index.reg_gen];
            *pcchReg = 2 + !!psz[2] + !!psz[3];
            return psz;
        }

        default:
            AssertMsgFailed(("%#x %#x\n", pParam->flags, pCpu->addrmode));
            *pcchReg = 3;
            return "r??";
    }
}

static size_t MyDisasYasmFormat(DISCPUSTATE const *pCpu, char *pszBuf, size_t cchBuf)
{
    PCOPCODE const  pOp = pCpu->pCurInstr;
    size_t          cchOutput = 0;
    char           *pszDst = pszBuf;
    size_t          cchDst = cchBuf;

    /* output macros */
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
#define PUT_PSZ(psz) \
            do { const size_t cchTmp = strlen(psz); PUT_STR((psz), cchTmp); } while (0)
#define PUT_NUM(cch, fmt, num) \
            do { \
                 cchOutput += (cch); \
                 if (cchBuf > 1) \
                 { \
                    const size_t cchTmp = RTStrPrintf(pszDst, cchBuf, fmt, (num)); \
                    pszDst += cchTmp; \
                    cchBuf -= cchTmp; \
                    Assert(cchTmp == (cch) || cchBuf == 1); \
                 } \
            } while (0)
#define PUT_NUM_8(num)  PUT_NUM(4,  "0%02xh", (uint8_t)(num))
#define PUT_NUM_16(num) PUT_NUM(6,  "0%04xh", (uint16_t)(num))
#define PUT_NUM_32(num) PUT_NUM(10, "0%08xh", (uint32_t)(num))
#define PUT_NUM_64(num) PUT_NUM(18, "0%08xh", (uint64_t)(num))

    /*
     * Filter out invalid opcodes first as they need special
     * treatment. UD2 is an exception and should be handled normally.
     */
    if (    pOp->opcode == OP_INVALID
        ||  (   pOp->opcode == OP_ILLUD2
             && (pCpu->prefix & PREFIX_LOCK)))
    {

    }
    else
    {
        /*
         * Prefixes
         */
        if (pCpu->prefix & PREFIX_LOCK)
            PUT_SZ("lock ");
        if(pCpu->prefix & PREFIX_REP)
            PUT_SZ("rep ");
        else if(pCpu->prefix & PREFIX_REPNE)
            PUT_SZ("repne ");

        /*
         * Adjust the format string to avoid stuff the assembler cannot handle.
         */
        char szTmpFmt[48];
        const char *pszFmt = pOp->pszOpcode;
        switch (pOp->opcode)
        {
            case OP_JECXZ:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "jcxz %Jb" : pCpu->opmode == CPUMODE_32BIT ? "jecxz %Jb"   : "jrcxz %Jb";
                break;
            case OP_PUSHF:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "pushfw"   : pCpu->opmode == CPUMODE_32BIT ? "pushfd"      : "pushfq";
                break;
            case OP_POPF:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "popfw"    : pCpu->opmode == CPUMODE_32BIT ? "popfd"       : "popfq";
                break;
            case OP_PUSHA:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "pushaw"   : "pushad";
                break;
            case OP_POPA:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "popaw"    : "popad";
                break;
            case OP_INSB:
                pszFmt = "insb";
                break;
            case OP_INSWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "insw"     : pCpu->opmode == CPUMODE_32BIT ? "insd"  : "insq";
                break;
            case OP_OUTSB:
                pszFmt = "outsb";
                break;
            case OP_OUTSWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "outsw"    : pCpu->opmode == CPUMODE_32BIT ? "outsd" : "outsq";
                break;
            case OP_MOVSB:
                pszFmt = "movsb";
                break;
            case OP_MOVSWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "movsw"    : pCpu->opmode == CPUMODE_32BIT ? "movsd" : "movsq";
                break;
            case OP_CMPSB:
                pszFmt = "cmpsb";
                break;
            case OP_CMPWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "cmpsw"    : pCpu->opmode == CPUMODE_32BIT ? "cmpsd" : "cmpsq";
                break;
            case OP_SCASB:
                pszFmt = "scasb";
                break;
            case OP_SCASWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "scasw"    : pCpu->opmode == CPUMODE_32BIT ? "scasd" : "scasq";
                break;
            case OP_LODSB:
                pszFmt = "lodsb";
                break;
            case OP_LODSWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "lodsw"    : pCpu->opmode == CPUMODE_32BIT ? "lodsd" : "lodsq";
                break;
            case OP_STOSB:
                pszFmt = "stosb";
                break;
            case OP_STOSWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "stosw"    : pCpu->opmode == CPUMODE_32BIT ? "stosd" : "stosq";
                break;
            case OP_CBW:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "cbw"      : pCpu->opmode == CPUMODE_32BIT ? "cwde"  : "cdqe";
                break;
            case OP_CWD:
                pszFmt = pCpu->opmode == CPUMODE_16BIT ? "cwd"      : pCpu->opmode == CPUMODE_32BIT ? "cdq"   : "cqo";
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
                if (pCpu->opcode == 0x90)
                    /* fine, fine */;
                else if (pszFmt[sizeof("nop %Ev")] == '/' && pszFmt[sizeof("nop %Ev") + 1] == 'p')
                    pszFmt = "prefetch %Eb";
                else if (pCpu->opcode == 0x1f)
                {
                    Assert(pCpu->opsize >= 3);
                    PUT_SZ("db 00fh, 01fh,");
                    PUT_NUM_8(pCpu->ModRM.u);
                    for (unsigned i = 3; i < pCpu->opsize; i++)
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
                if (pCpu->opcode == 0xdb) /* m80fp workaround. */
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
        PCOP_PARAMETER pParam = &pCpu->param1;
        int iParam = 1;

#define PUT_FAR() \
            do { \
                if (    OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p \
                    &&  pOp->opcode != OP_LDS /* table bugs? */ \
                    &&  pOp->opcode != OP_LES \
                    &&  pOp->opcode != OP_LFS \
                    &&  pOp->opcode != OP_LGS \
                    &&  pOp->opcode != OP_LSS ) \
                    PUT_SZ("far "); \
            } while (0)
        /** @todo mov ah,ch ends up with a byte 'override'... */
#define PUT_SIZE_OVERRIDE() \
            do { \
                switch (OP_PARM_VSUBTYPE(pParam->param)) \
                { \
                    case OP_PARM_v: \
                        switch (pCpu->opmode) \
                        { \
                            case CPUMODE_16BIT: PUT_SZ("word "); break; \
                            case CPUMODE_32BIT: PUT_SZ("dword "); break; \
                            case CPUMODE_64BIT: PUT_SZ("qword "); break; \
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
                    case OP_PARM_s: if (pParam->flags & USE_REG_FP) PUT_SZ("tword "); break; /* ?? */ \
                    case OP_PARM_z: break; \
                    case OP_PARM_NONE: \
                        if (    OP_PARM_VTYPE(pParam->param) == OP_PARM_M \
                            &&  ((pParam->flags & USE_REG_FP) || pOp->opcode == OP_FLD)) \
                            PUT_SZ("tword "); \
                        break; \
                    default:        break; /*no pointer type specified/necessary*/ \
                } \
            } while (0)
        static const char s_szSegPrefix[6][4] = { "es:", "cs:", "ss:", "ds:", "fs:", "gs:" };
#define PUT_SEGMENT_OVERRIDE() \
        do { \
            if (pCpu->prefix & PREFIX_SEG) \
                PUT_STR(s_szSegPrefix[pCpu->prefix_seg], 3); \
        } while (0)


        /*
         * Segment prefixing for instructions that doesn't do memory access.
         */
        if (    (pCpu->prefix & PREFIX_SEG)
            &&  !(pCpu->param1.flags & USE_EFFICIENT_ADDRESS)
            &&  !(pCpu->param2.flags & USE_EFFICIENT_ADDRESS)
            &&  !(pCpu->param3.flags & USE_EFFICIENT_ADDRESS))
        {
            PUT_STR(s_szSegPrefix[pCpu->prefix_seg], 2);
            PUT_C(' ');
        }


        /*
         * The formatting loop.
         */
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
                        Assert(!(pParam->flags & (USE_INDEX | USE_SCALE) /* No SIB here... */));
                        Assert(!(pParam->flags & (USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_DISPLACEMENT64 | USE_RIPDISPLACEMENT32)));

                        size_t cchReg;
                        const char *pszReg = MyDisasYasmFormatBaseReg(pCpu, pParam, &cchReg, 0 /* pCpu->addrmode == CPUMODE_16BIT */);
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
                        if (pParam->flags & USE_EFFICIENT_ADDRESS)
                        {
                            /* Work around mov seg,[mem16]  and mov [mem16],seg as these always make a 16-bit mem
                               while the register variants deals with 16, 32 & 64 in the normal fashion. */
                            if (    pParam->param != OP_PARM_Ev
                                ||  pOp->opcode != OP_MOV
                                ||  (   pOp->param1 != OP_PARM_Sw
                                     && pOp->param2 != OP_PARM_Sw))
                                PUT_SIZE_OVERRIDE();
                            PUT_C('[');
                        }
                        if (pParam->flags & (USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_DISPLACEMENT64 | USE_RIPDISPLACEMENT32))
                        {
                            if (   (pParam->flags & USE_DISPLACEMENT8)
                                && !pParam->disp8)
                                PUT_SZ("byte ");
                            else if (   (pParam->flags & USE_DISPLACEMENT16)
                                     && (int8_t)pParam->disp16 == (int16_t)pParam->disp16)
                                PUT_SZ("word ");
                            else if (   (pParam->flags & USE_DISPLACEMENT32)
                                     && (int8_t)pParam->disp32 == (int32_t)pParam->disp32)
                                PUT_SZ("dword ");
                        }
                        if (pParam->flags & USE_EFFICIENT_ADDRESS)
                            PUT_SEGMENT_OVERRIDE();

                        bool fBase =  (pParam->flags & USE_BASE) /* When exactly is USE_BASE supposed to be set? disasmModRMReg doesn't set it. */
                                   || (   (pParam->flags & (USE_REG_GEN8 | USE_REG_GEN16 | USE_REG_GEN32 | USE_REG_GEN64))
                                       && !(pParam->flags & USE_EFFICIENT_ADDRESS));
                        if (fBase)
                        {
                            size_t cchReg;
                            const char *pszReg = MyDisasYasmFormatBaseReg(pCpu, pParam, &cchReg, 0 /*pCpu->addrmode == CPUMODE_16BIT*/);
                            PUT_STR(pszReg, cchReg);
                        }

                        if (pParam->flags & USE_INDEX)
                        {
                            if (fBase)
                                PUT_C('+');

                            size_t cchReg;
                            const char *pszReg = MyDisasYasmFormatIndexReg(pCpu, pParam, &cchReg);
                            PUT_STR(pszReg, cchReg);

                            if (pParam->flags & USE_SCALE)
                            {
                                PUT_C('*');
                                PUT_C('0' + pParam->scale);
                            }
                        }
                        else
                            Assert(!(pParam->flags & USE_SCALE));

                        if (pParam->flags & (USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_DISPLACEMENT64 | USE_RIPDISPLACEMENT32))
                        {
                            Assert(!(pParam->flags & USE_DISPLACEMENT64));
                            int32_t off;
                            if (pParam->flags & USE_DISPLACEMENT8)
                                off = pParam->disp8;
                            else if (pParam->flags & USE_DISPLACEMENT16)
                                off = pParam->disp16;
                            else if (pParam->flags & (USE_DISPLACEMENT32 | USE_RIPDISPLACEMENT32))
                                off = pParam->disp32;

                            if (fBase || (pParam->flags & USE_INDEX))
                                PUT_C(off >= 0 ? '+' : '-');

                            if (off < 0)
                                off = -off;
                            if (pParam->flags & USE_DISPLACEMENT8)
                                PUT_NUM_8( off);
                            else if (pParam->flags & USE_DISPLACEMENT16)
                                PUT_NUM_16(off);
                            else if (pParam->flags & USE_DISPLACEMENT32)
                                PUT_NUM_32(off);
                            else
                            {
                                PUT_NUM_32(off);
                                PUT_SZ(" wrt rip"); //??
                            }
                        }

                        if (pParam->flags & USE_EFFICIENT_ADDRESS)
                            PUT_C(']');
                        break;
                    }

                    case 'F': /* Eflags register (0 - popf/pushf only, avoided in adjustments above). */
                        AssertFailed();
                        break;

                    case 'I': /* Immediate data (ParseImmByte, ParseImmByteSX, ParseImmV, ParseImmUshort, ParseImmZ). */
                        Assert(*pszFmt == 'b' || *pszFmt == 'v' || *pszFmt == 'w' || *pszFmt == 'z'); pszFmt++;
                        switch (pParam->flags & (  USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE64
                                                 | USE_IMMEDIATE16_SX8 | USE_IMMEDIATE32_SX8))
                        {
                            case USE_IMMEDIATE8:
                                if (    (pOp->param1 >= OP_PARM_REG_GEN8_START && pOp->param1 <= OP_PARM_REG_GEN8_END)
                                    ||  (pOp->param2 >= OP_PARM_REG_GEN8_START && pOp->param2 <= OP_PARM_REG_GEN8_END)
                                    )
                                    PUT_SZ("strict byte ");
                                PUT_NUM_8(pParam->parval);
                                break;

                            case USE_IMMEDIATE16:
                                if (    (int8_t)pParam->parval == (int16_t)pParam->parval
                                    ||  (pOp->param1 >= OP_PARM_REG_GEN16_START && pOp->param1 <= OP_PARM_REG_GEN16_END)
                                    ||  (pOp->param2 >= OP_PARM_REG_GEN16_START && pOp->param2 <= OP_PARM_REG_GEN16_END)
                                    ||  pCpu->mode != pCpu->opmode
                                    )
                                {
                                    if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_b)
                                        PUT_SZ("strict byte ");
                                    else if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_v)
                                        PUT_SZ("strict word ");
                                }
                                PUT_NUM_16(pParam->parval);
                                break;

                            case USE_IMMEDIATE16_SX8:
                                PUT_SZ("strict byte ");
                                PUT_NUM_16(pParam->parval);
                                break;

                            case USE_IMMEDIATE32:
                                if (    (int8_t)pParam->parval == (int32_t)pParam->parval
                                    ||  (pOp->param1 >= OP_PARM_REG_GEN32_START && pOp->param1 <= OP_PARM_REG_GEN32_END)
                                    ||  (pOp->param2 >= OP_PARM_REG_GEN32_START && pOp->param2 <= OP_PARM_REG_GEN32_END)
                                    ||  pCpu->opmode != (pCpu->mode == CPUMODE_16BIT ? CPUMODE_16BIT : CPUMODE_32BIT) /* not perfect */
                                    )
                                {
                                    if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_b)
                                        PUT_SZ("strict byte ");
                                    else if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_v)
                                        PUT_SZ("strict dword ");
                                }
                                PUT_NUM_32(pParam->parval);
                                break;

                            case USE_IMMEDIATE32_SX8:
                                PUT_SZ("strict byte ");
                                PUT_NUM_32(pParam->parval);
                                break;

                            case USE_IMMEDIATE64:
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
                        bool fPrefix = pOp->opcode != OP_CALL
                                    && pOp->opcode != OP_LOOP
                                    && pOp->opcode != OP_LOOPE
                                    && pOp->opcode != OP_LOOPNE
                                    && pOp->opcode != OP_JECXZ;

                        if (pParam->flags & USE_IMMEDIATE8_REL)
                        {
                            if (fPrefix)
                                PUT_SZ("short ");
                            offDisplacement = (int8_t)pParam->parval;
                            Assert(*pszFmt == 'b'); pszFmt++;
                        }
                        else if (pParam->flags & USE_IMMEDIATE16_REL)
                        {
                            if (fPrefix)
                                PUT_SZ("near ");
                            offDisplacement = (int16_t)pParam->parval;
                            Assert(*pszFmt == 'v'); pszFmt++;
                        }
                        else
                        {
                            if (fPrefix)
                                PUT_SZ("near ");
                            offDisplacement = (int32_t)pParam->parval;
                            Assert(pParam->flags & USE_IMMEDIATE32_REL);
                            Assert(*pszFmt == 'v'); pszFmt++;
                        }

                        RTUINTPTR uTrgAddr = pCpu->opaddr + pCpu->opsize + offDisplacement;
                        if (pCpu->mode == CPUMODE_16BIT)
                            PUT_NUM_16(uTrgAddr);
                        else if (pCpu->mode == CPUMODE_32BIT)
                            PUT_NUM_32(uTrgAddr);
                        else
                            PUT_NUM_64(uTrgAddr);
                        break;
                    }

                    case 'A': /* Direct (jump/call) address (ParseImmAddr). */
                        Assert(*pszFmt == 'p'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_SEGMENT_OVERRIDE();
                        switch (pParam->flags & (USE_IMMEDIATE_ADDR_16_16 | USE_IMMEDIATE_ADDR_16_32 | USE_DISPLACEMENT64 | USE_DISPLACEMENT32 | USE_DISPLACEMENT16))
                        {
                            case USE_IMMEDIATE_ADDR_16_16:
                                PUT_NUM_16(pParam->parval >> 16);
                                PUT_C(':');
                                PUT_NUM_16(pParam->parval);
                                break;
                            case USE_IMMEDIATE_ADDR_16_32:
                                PUT_NUM_16(pParam->parval >> 32);
                                PUT_C(':');
                                PUT_NUM_32(pParam->parval);
                                break;
                            case USE_DISPLACEMENT16:
                                PUT_NUM_16(pParam->parval);
                                break;
                            case USE_DISPLACEMENT32:
                                PUT_NUM_32(pParam->parval);
                                break;
                            case USE_DISPLACEMENT64:
                                PUT_NUM_64(pParam->parval);
                                break;
                            default:
                                AssertFailed();
                                break;
                        }
                        break;

                    case 'O': /* No ModRM byte (ParseImmAddr). */
                        Assert(*pszFmt == 'b' || *pszFmt == 'v'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_C('[');
                        PUT_SEGMENT_OVERRIDE();
                        switch (pParam->flags & (USE_IMMEDIATE_ADDR_16_16 | USE_IMMEDIATE_ADDR_16_32 | USE_DISPLACEMENT64 | USE_DISPLACEMENT32 | USE_DISPLACEMENT16))
                        {
                            case USE_IMMEDIATE_ADDR_16_16:
                                PUT_NUM_16(pParam->parval >> 16);
                                PUT_C(':');
                                PUT_NUM_16(pParam->parval);
                                break;
                            case USE_IMMEDIATE_ADDR_16_32:
                                PUT_NUM_16(pParam->parval >> 32);
                                PUT_C(':');
                                PUT_NUM_32(pParam->parval);
                                break;
                            case USE_DISPLACEMENT16:
                                PUT_NUM_16(pParam->disp16);
                                break;
                            case USE_DISPLACEMENT32:
                                PUT_NUM_32(pParam->disp32);
                                break;
                            case USE_DISPLACEMENT64:
                                PUT_NUM_64(pParam->disp64);
                                break;
                            default:
                                AssertFailed();
                                break;
                        }
                        PUT_C(']');
                        break;

                    case 'X': /* DS:SI (ParseXb, ParseXv). */
                    case 'Y': /* ES:DI (ParseYb, ParseYv). */
                    {
                        Assert(*pszFmt == 'b' || *pszFmt == 'v'); pszFmt++;
                        PUT_FAR();
                        PUT_SIZE_OVERRIDE();
                        PUT_C('[');
                        if (pParam->flags & USE_POINTER_DS_BASED)
                            PUT_SZ("ds:");
                        else
                            PUT_SZ("es:");

                        size_t cchReg;
                        const char *pszReg = MyDisasYasmFormatBaseReg(pCpu, pParam, &cchReg, 0);
                        PUT_STR(pszReg, cchReg);
                        PUT_C(']');
                        break;
                    }

                    case 'e': /* Register based on operand size (e.g. %eAX) (ParseFixedReg). */
                    {
                        Assert(RT_C_IS_ALPHA(pszFmt[0]) && RT_C_IS_ALPHA(pszFmt[1]) && !RT_C_IS_ALPHA(pszFmt[2])); pszFmt += 2;
                        size_t cchReg;
                        const char *pszReg = MyDisasYasmFormatBaseReg(pCpu, pParam, &cchReg, 0);
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


    /* Terminate it - on overflow we'll have reserved one byte for this. */
    if (cchDst > 0)
        *pszDst = '\0';

    /* clean up macros */
#undef PUT_PSZ
#undef PUT_SZ
#undef PUT_STR
#undef PUT_C
    return cchOutput;
}
#endif


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
#ifndef USE_MY_FORMATTER
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

#else  /* USE_MY_FORMATTER */
    size_t cch = MyDisasYasmFormat(&pState->Cpu, szTmp, sizeof(szTmp));
    Assert(cch < sizeof(szTmp));
    while (cch < 71)
        szTmp[cch++] = ' ';
    szTmp[cch] = '\0';
#endif /* USE_MY_FORMATTER */

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
        /* no efficient address which it may apply to. */
        Assert((pState->Cpu.prefix & PREFIX_SEG) || pState->Cpu.mode == CPUMODE_64BIT);
        if (    !(pState->Cpu.param1.flags & USE_EFFICIENT_ADDRESS)
            &&  !(pState->Cpu.param2.flags & USE_EFFICIENT_ADDRESS)
            &&  !(pState->Cpu.param3.flags & USE_EFFICIENT_ADDRESS))
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
                    cbRead -= pState->cbLeft;
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
 * Prints usage info.
 *
 * @returns 1.
 * @param   argv0       The program name.
 */
static int Usage(const char *argv0)
{
    RTStrmPrintf(g_pStdErr,
"usage: %s [options] <file1> [file2..fileN]\n"
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

    /*
     * Parse arguments.
     */
    static const RTOPTIONDEF g_aOptions[] =
    {
        { "--address",      'a', RTGETOPT_REQ_UINT64 },
        { "--cpumode",      'c', RTGETOPT_REQ_UINT32 },
        { "--help",         'h', 0 },
        { "--bytes",        'b', RTGETOPT_REQ_INT64 },
        { "--listing",      'l', 0 },
        { "--no-listing",   'L', 0 },
        { "--offset",       'o', RTGETOPT_REQ_INT64 },
        { "--style",        's', RTGETOPT_REQ_STRING },
        { "--undef-op",     'u', RTGETOPT_REQ_STRING },
    };

    int ch;
    int iArg = 1;
    RTOPTIONUNION ValueUnion;
    while ((ch = RTGetOpt(argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), &iArg, &ValueUnion)))
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

            default:
                RTStrmPrintf(g_pStdErr, "%s: syntax error: %Rrc\n", argv0, ch);
                return 1;
        }
    }
    if (iArg >= argc)
        return Usage(argv0);

    /*
     * Process the files.
     */
    int rc = VINF_SUCCESS;
    for ( ; iArg < argc; iArg++)
    {
        /*
         * Read the file into memory.
         */
        void   *pvFile;
        size_t  cbFile;
        rc = RTFileReadAllEx(argv[iArg], off, cbMax, 0, &pvFile, &cbFile);
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

    return RT_SUCCESS(rc) ? 0 : 1;
}

