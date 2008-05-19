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
#include <iprt/err.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef enum { kAsmStyle_Default, kAsmStyle_yasm, kAsmStyle_masm, kAsmStyle_invalid } ASMSTYLE;

typedef struct MYDISSTATE
{
    DISCPUSTATE     Cpu;
    uint64_t        uAddress;           /**< The current instruction address. */
    uint8_t        *pbInstr;            /**< The current instruction (pointer). */
    uint32_t        cbInstr;            /**< The size of the current instruction. */
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
    RTPrintf("yasm not implemented: %s", pState->szLine);
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
 * Callback for reading bytes.
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
 */
static int MyDisasmBlock(const char *argv0, DISCPUMODE enmCpuMode, uint64_t uAddress, uint8_t *pbFile, size_t cbFile,
                         ASMSTYLE enmStyle, bool fListing)
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
        if (RT_SUCCESS(rc))
        {
            pfnFormatter(&State);
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
             , argv0, argv0);
    return 1;
}


int main(int argc, char **argv)
{
    RTR3Init();

    /* options */
    uint64_t uAddress = 0;
    ASMSTYLE enmStyle = kAsmStyle_Default;
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
                    RTStrmPrintf(g_pStdErr, "%s: Invalid CPU mode value %RU32\n", argv[0], ValueUnion.u32);
                    return 1;
                }
                break;

            case 'h':
                return Usage(argv[0]);

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
                    RTStrmPrintf(g_pStdErr, "%s: masm style isn't implemented yet\n", argv[0]);
                    return 1;
                }
                else
                {
                    RTStrmPrintf(g_pStdErr, "%s: unknown assembly style: %s\n", argv[0], ValueUnion.psz);
                    return 1;
                }
                break;

            default:
                RTStrmPrintf(g_pStdErr, "%s: syntax error: %Rrc\n", argv[0], ch);
                return 1;
        }
    }
    if (iArg >= argc)
        return Usage(argv[0]);

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
            RTStrmPrintf(g_pStdErr, "%s: %s: %Rrc\n", argv[0], argv[iArg], rc);
            break;
        }

        /*
         * Disassemble it.
         */
        rc = MyDisasmBlock(argv[0], enmCpuMode, uAddress, (uint8_t *)pvFile, cbFile, enmStyle, fListing);
        if (RT_FAILURE(rc))
            break;
    }

    return RT_SUCCESS(rc) ? 0 : 1;
}

