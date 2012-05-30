/* $Id$ */
/** @file
 * MakeDebianBiosAssembly - Generate Assembly Source for Debian-minded Distros.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dbg.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#include <VBox/dis.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The flat ROM base address. */
#define VBOX_BIOS_BASE      UINT32_C(0xf0000)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * A BIOS segment.
 */
typedef struct BIOSSEG
{
    char        szName[32];
    char        szClass[32];
    char        szGroup[32];
    RTFAR16     Address;
    uint32_t    uFlatAddr;
    uint32_t    cb;
} BIOSSEG;
/** Pointer to a BIOS segment. */
typedef BIOSSEG *PBIOSSEG;


/**
 * Pointer to a BIOS map parser handle.
 */
typedef struct BIOSMAP
{
    /** The stream pointer. */
    PRTSTREAM   hStrm;
    /** The file name. */
    const char *pszMapFile;
    /** Set when EOF has been reached. */
    bool        fEof;
    /** The current line number (0 based).*/
    uint32_t    iLine;
    /** The length of the current line. */
    uint32_t    cch;
    /** The offset of the first non-white character on the line. */
    uint32_t    offNW;
    /** The line buffer. */
    char        szLine[16384];
} BIOSMAP;
/** Pointer to a BIOS map parser handle. */
typedef BIOSMAP *PBIOSMAP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned         g_cVerbose = 1 /*0*/;
/** Pointer to the BIOS image. */
static uint8_t const   *g_pbImg;
/** The size of the BIOS image. */
static size_t           g_cbImg;

/** Debug module for the map file.  */
static RTDBGMOD         g_hMapMod = NIL_RTDBGMOD;
/** The number of BIOS segments found in the map file. */
static uint32_t         g_cSegs = 0;
/** Array of BIOS segments from the map file. */
static BIOSSEG          g_aSegs[32];

/** The output stream. */
static PRTSTREAM        g_hStrmOutput = NULL;


static bool outputPrintfV(const char *pszFormat, va_list va)
{
    int rc = RTStrmPrintfV(g_hStrmOutput, pszFormat, va);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Output error: %Rrc\n", rc);
        return false;
    }
    return true;
}


static bool outputPrintf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    bool fRc = outputPrintfV(pszFormat, va);
    va_end(va);
    return fRc;
}


/**
 * Opens the output file for writing.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszOutput           Path to the output file.
 */
static RTEXITCODE OpenOutputFile(const char *pszOutput)
{
    if (!pszOutput)
        g_hStrmOutput = g_pStdOut;
    else
    {
        int rc = RTStrmOpen(pszOutput, "w", &g_hStrmOutput);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to open output file '%s': %Rrc", pszOutput, rc);
    }
    return RTEXITCODE_SUCCESS;
}


/**
 * Displays a disassembly error and returns @c false.
 *
 * @returns @c false.
 * @param   pszFormat           The error format string.
 * @param   ...                 Format argument.
 */
static bool disError(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTMsgErrorV(pszFormat, va);
    va_end(va);
    return false;
}


/**
 * Output the disassembly file header.
 *
 * @returns @c true on success,
 */
static bool disFileHeader(void)
{
    return outputPrintf("; $Id$ \n"
                        ";; @file\n"
                        "; Auto Generated source file. Do not edit.\n"
                        ";\n"
                        "\n"
                        "org 0xf000\n"
                        "\n"
                        );
}


/**
 * Checks if a byte sequence could be a string litteral.
 *
 * @returns @c true if it is, @c false if it isn't.
 * @param   uFlatAddr           The address of the byte sequence.
 * @param   cb                  The length of the sequence.
 */
static bool disIsString(uint32_t uFlatAddr, uint32_t cb)
{
    if (cb < 6)
        return false;

    uint8_t const *pb = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    while (cb > 0)
    {
        if (   !RT_C_IS_PRINT(*pb)
            && *pb != '\r'
            && *pb != '\n'
            && *pb != '\t')
        {
            if (*pb == '\0')
            {
                do
                {
                    pb++;
                    cb--;
                } while (cb > 0 && *pb == '\0');
                return cb == 0;
            }
            return false;
        }
        pb++;
        cb--;
    }

    return true;
}


/**
 * Checks if a dword could be a far 16:16 BIOS address.
 *
 * @returns @c true if it is, @c false if it isn't.
 * @param   uFlatAddr           The address of the dword.
 */
static bool disIsFarBiosAddr(uint32_t uFlatAddr)
{
    uint16_t const *pu16 = (uint16_t const *)&g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    if (pu16[1] < 0xf000)
        return false;
    if (pu16[1] > 0xfff0)
        return false;
    uint32_t uFlatAddr2 = (uint32_t)(pu16[1] << 4) | pu16[0];
    if (uFlatAddr2 >= VBOX_BIOS_BASE + _64K)
        return false;
    return true;
}


static bool disByteData(uint32_t uFlatAddr, uint32_t cb)
{
    uint8_t const  *pb = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    size_t          cbOnLine = 0;
    while (cb-- > 0)
    {
        bool fRc;
        if (cbOnLine >= 16)
        {
            fRc = outputPrintf("\n"
                               "    db  0%02xh", *pb);
            cbOnLine = 1;
        }
        else if (!cbOnLine)
        {
            fRc = outputPrintf("    db  0%02xh", *pb);
            cbOnLine = 1;
        }
        else
        {
            fRc = outputPrintf(", 0%02xh", *pb);
            cbOnLine++;
        }
        if (!fRc)
            return false;
        pb++;
    }
    return outputPrintf("\n");
}


static bool disWordData(uint32_t uFlatAddr, uint32_t cb)
{
    if (cb & 1)
        return disError("disWordData expects word aligned size: cb=%#x uFlatAddr=%#x", uFlatAddr, cb);

    uint16_t const *pu16 = (uint16_t const *)&g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    size_t          cbOnLine = 0;
    while (cb > 0)
    {
        bool fRc;
        if (cbOnLine >= 16)
        {
            fRc = outputPrintf("\n"
                               "    dw  0%04xh", *pu16);
            cbOnLine = 2;
        }
        else if (!cbOnLine)
        {
            fRc = outputPrintf("    dw  0%04xh", *pu16);
            cbOnLine = 2;
        }
        else
        {
            fRc = outputPrintf(", 0%04xh", *pu16);
            cbOnLine += 2;
        }
        if (!fRc)
            return false;
        pu16++;
        cb -= 2;
    }
    return outputPrintf("\n");
}


static bool disDWordData(uint32_t uFlatAddr, uint32_t cb)
{
    if (cb & 3)
        return disError("disWordData expects dword aligned size: cb=%#x uFlatAddr=%#x", uFlatAddr, cb);

    uint32_t const *pu32 = (uint32_t const *)&g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    size_t          cbOnLine = 0;
    while (cb > 0)
    {
        bool fRc;
        if (cbOnLine >= 16)
        {
            fRc = outputPrintf("\n"
                               "    dd  0%08xh", *pu32);
            cbOnLine = 4;
        }
        else if (!cbOnLine)
        {
            fRc = outputPrintf("    dd  0%08xh", *pu32);
            cbOnLine = 4;
        }
        else
        {
            fRc = outputPrintf(", 0%08xh", *pu32);
            cbOnLine += 4;
        }
        if (!fRc)
            return false;
        pu32++;
        cb -= 4;
    }
    return outputPrintf("\n");
}


static bool disStringData(uint32_t uFlatAddr, uint32_t cb)
{
    uint8_t const  *pb        = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    uint32_t        cchOnLine = 0;
    while (cb > 0)
    {
        /* Line endings and beginnings. */
        if (cchOnLine >= 72)
        {
            if (!outputPrintf("\n"))
                return false;
            cchOnLine = 0;
        }
        if (   !cchOnLine
            && !outputPrintf("    db  "))
            return false;

        /* See how many printable character we've got. */
        uint32_t cchPrintable = 0;
        while (   cchPrintable < cb
               && RT_C_IS_PRINT(pb[cchPrintable])
               && pb[cchPrintable] != '\'')
            cchPrintable++;

        bool fRc = true;
        if (cchPrintable)
        {
            if (cchPrintable + cchOnLine > 72)
                cchPrintable = 72 - cchOnLine;
            if (cchOnLine)
            {
                fRc = outputPrintf(", '%.*s'", cchPrintable, pb);
                cchOnLine += 4 + cchPrintable;
            }
            else
            {
                fRc = outputPrintf("'%.*s'", cchPrintable, pb);
                cchOnLine += 2 + cchPrintable;
            }
            pb += cchPrintable;
            cb -= cchPrintable;
        }
        else
        {
            if (cchOnLine)
            {
                fRc = outputPrintf(", 0%02xh", *pb);
                cchOnLine += 6;
            }
            else
            {
                fRc = outputPrintf("0%02xh", *pb);
                cchOnLine += 4;
            }
            pb++;
            cb--;
        }
        if (!fRc)
            return false;
    }
    return outputPrintf("\n");
}


/**
 * For dumping a portion of a string table.
 *
 * @returns @c true on success, @c false on failure.
 * @param   uFlatAddr           The start address.
 * @param   cb                  The size of the string table.
 */
static bool disStringsData(uint32_t uFlatAddr, uint32_t cb)
{
    uint8_t const  *pb        = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    uint32_t        cchOnLine = 0;
    uint8_t         bPrev     = 255;
    while (cb > 0)
    {
        /* Line endings and beginnings. */
        if (   cchOnLine >= 72
            || (bPrev == '\0' && *pb != '\0'))
        {
            if (!outputPrintf("\n"))
                return false;
            cchOnLine = 0;
        }
        if (   !cchOnLine
            && !outputPrintf("    db   "))
            return false;

        /* See how many printable character we've got. */
        uint32_t cchPrintable = 0;
        while (   cchPrintable < cb
               && RT_C_IS_PRINT(pb[cchPrintable])
               && pb[cchPrintable] != '\'')
            cchPrintable++;

        bool fRc = true;
        if (cchPrintable)
        {
            if (cchPrintable + cchOnLine > 72)
                cchPrintable = 72 - cchOnLine;
            if (cchOnLine)
            {
                fRc = outputPrintf(", '%.*s'", cchPrintable, pb);
                cchOnLine += 4 + cchPrintable;
            }
            else
            {
                fRc = outputPrintf("'%.*s'", cchPrintable, pb);
                cchOnLine += 2 + cchPrintable;
            }
            pb += cchPrintable;
            cb -= cchPrintable;
        }
        else
        {
            if (cchOnLine)
            {
                fRc = outputPrintf(", 0%02xh", *pb);
                cchOnLine += 6;
            }
            else
            {
                fRc = outputPrintf("0%02xh", *pb);
                cchOnLine += 4;
            }
            pb++;
            cb--;
        }
        if (!fRc)
            return false;
        bPrev = pb[-1];
    }
    return outputPrintf("\n");
}


/**
 * Minds the gap between two segments.
 *
 * Gaps should generally be zero filled.
 *
 * @returns @c true on success, @c false on failure.
 * @param   uFlatAddr           The address of the gap.
 * @param   cbPadding           The size of the gap.
 */
static bool disCopySegmentGap(uint32_t uFlatAddr, uint32_t cbPadding)
{
    if (g_cVerbose > 0)
        outputPrintf("\n"
                     "  ; Padding %#x bytes at %#x\n", cbPadding, uFlatAddr);
    uint8_t const  *pb = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    if (!ASMMemIsAll8(pb, cbPadding, 0))
        return outputPrintf("  times %u db 0\n", cbPadding);

    return disByteData(uFlatAddr, cbPadding);
}


/**
 * Worker for disGetNextSymbol that only does the looking up, no RTDBSYMBOL::cb
 * calc.
 *
 * @param   uFlatAddr           The address to start searching at.
 * @param   cbMax               The size of the search range.
 * @param   poff                Where to return the offset between the symbol
 *                              and @a uFlatAddr.
 * @param   pSym                Where to return the symbol data.
 */
static void disGetNextSymbolWorker(uint32_t uFlatAddr, uint32_t cbMax, uint32_t *poff, PRTDBGSYMBOL pSym)
{
    RTINTPTR off = 0;
    int rc = RTDbgModSymbolByAddr(g_hMapMod, RTDBGSEGIDX_RVA, uFlatAddr, RTDBGSYMADDR_FLAGS_GREATER_OR_EQUAL, &off, pSym);
    if (RT_SUCCESS(rc))
    {
        /* negative offset, indicates beyond. */
        if (off <= 0)
        {
            *poff = (uint32_t)-off;
            return;
        }

        outputPrintf("  ; !! RTDbgModSymbolByAddr(,,%#x,,) -> off=%RTptr cb=%RTptr uValue=%RTptr '%s'\n",
                     uFlatAddr, off, pSym->cb, pSym->Value, pSym->szName);
    }
    else if (rc != VERR_SYMBOL_NOT_FOUND)
        outputPrintf("  ; !! RTDbgModSymbolByAddr(,,%#x,,) -> %Rrc\n", uFlatAddr, rc);

    RTStrPrintf(pSym->szName, sizeof(pSym->szName), "_dummy_addr_%#x", uFlatAddr + cbMax);
    pSym->Value     = uFlatAddr + cbMax;
    pSym->cb        = 0;
    pSym->offSeg    = uFlatAddr + cbMax;
    pSym->iSeg      = RTDBGSEGIDX_RVA;
    pSym->iOrdinal  = 0;
    pSym->fFlags    = 0;
    *poff = cbMax;
}


/**
 * Gets the symbol at or after the given address.
 *
 * If there are no symbols in the specified range, @a pSym and @a poff will be
 * set up to indicate a symbol at the first byte after the range.
 *
 * @param   uFlatAddr           The address to start searching at.
 * @param   cbMax               The size of the search range.
 * @param   poff                Where to return the offset between the symbol
 *                              and @a uFlatAddr.
 * @param   pSym                Where to return the symbol data.
 */
static void disGetNextSymbol(uint32_t uFlatAddr, uint32_t cbMax, uint32_t *poff, PRTDBGSYMBOL pSym)
{
    disGetNextSymbolWorker(uFlatAddr, cbMax, poff, pSym);
    if (   *poff < cbMax
        && pSym->cb == 0)
    {
        if (*poff + 1 < cbMax)
        {
            uint32_t    off2;
            RTDBGSYMBOL Sym2;
            disGetNextSymbolWorker(uFlatAddr + *poff + 1, cbMax - *poff - 1, &off2, &Sym2);
            pSym->cb = off2 + 1;
        }
        else
            pSym->cb = 1;
    }
    if (pSym->cb > cbMax - *poff)
        pSym->cb = cbMax - *poff;

    if (g_cVerbose > 1)
        outputPrintf("  ; disGetNextSymbol %#x LB %#x -> off=%#x cb=%RTptr uValue=%RTptr '%s'\n",
                     uFlatAddr, cbMax, *poff, pSym->cb, pSym->Value, pSym->szName);

}


/**
 * For dealing with the const segment (string constants).
 *
 * @returns @c true on success, @c false on failure.
 * @param   iSeg                The segment.
 */
static bool disConstSegment(uint32_t iSeg)
{
    uint32_t    uFlatAddr = g_aSegs[iSeg].uFlatAddr;
    uint32_t    cb        = g_aSegs[iSeg].cb;

    while (cb > 0)
    {
        uint32_t    off;
        RTDBGSYMBOL Sym;
        disGetNextSymbol(uFlatAddr, cb, &off, &Sym);

        if (off > 0)
        {
            if (!disStringsData(uFlatAddr, off))
                return false;
            cb        -= off;
            uFlatAddr += off;
            off        = 0;
            if (!cb)
                break;
        }

        bool fRc;
        if (off == 0)
        {
            size_t cchName = strlen(Sym.szName);
            fRc = outputPrintf("%s: %*s; %#x LB %#x\n", Sym.szName, cchName < 41 - 2 ? cchName - 41 - 2 : 0, "", uFlatAddr, Sym.cb);
            if (!fRc)
                return false;
            fRc = disStringsData(uFlatAddr, Sym.cb);
            uFlatAddr += Sym.cb;
            cb        -= Sym.cb;
        }
        else
        {
            fRc = disStringsData(uFlatAddr, Sym.cb);
            uFlatAddr += cb;
            cb = 0;
        }
        if (!fRc)
            return false;
    }

    return true;
}



static bool disDataSegment(uint32_t iSeg)
{
    uint32_t    uFlatAddr = g_aSegs[iSeg].uFlatAddr;
    uint32_t    cb        = g_aSegs[iSeg].cb;

    while (cb > 0)
    {
        uint32_t    off;
        RTDBGSYMBOL Sym;
        disGetNextSymbol(uFlatAddr, cb, &off, &Sym);

        if (off > 0)
        {
            if (!disByteData(uFlatAddr, off))
                return false;
            cb        -= off;
            uFlatAddr += off;
            off        = 0;
            if (!cb)
                break;
        }

        bool fRc;
        if (off == 0)
        {
            size_t cchName = strlen(Sym.szName);
            fRc = outputPrintf("%s: %*s; %#x LB %#x\n", Sym.szName, cchName < 41 - 2 ? cchName - 41 - 2 : 0, "", uFlatAddr, Sym.cb);
            if (!fRc)
                return false;

            if (Sym.cb == 2)
                fRc = disWordData(uFlatAddr, 2);
            //else if (Sym.cb == 4 && disIsFarBiosAddr(uFlatAddr))
            //    fRc = disDWordData(uFlatAddr, 4);
            else if (Sym.cb == 4)
                fRc = disDWordData(uFlatAddr, 4);
            else if (disIsString(uFlatAddr, Sym.cb))
                fRc = disStringData(uFlatAddr, Sym.cb);
            else
                fRc = disByteData(uFlatAddr, Sym.cb);

            uFlatAddr += Sym.cb;
            cb        -= Sym.cb;
        }
        else
        {
            fRc = disByteData(uFlatAddr, cb);
            uFlatAddr += cb;
            cb = 0;
        }
        if (!fRc)
            return false;
    }

    return true;
}


static bool disIsCodeAndAdjustSize(uint32_t uFlatAddr, PRTDBGSYMBOL pSym, PBIOSSEG pSeg)
{
    if (!strcmp(pSeg->szName, "BIOSSEG"))
    {
        if (   !strcmp(pSym->szName, "rom_fdpt")
            || !strcmp(pSym->szName, "pmbios_gdt")
            || !strcmp(pSym->szName, "pmbios_gdt_desc")
            || !strcmp(pSym->szName, "_pmode_IDT")
            || !strcmp(pSym->szName, "_rmode_IDT")
            || !strncmp(pSym->szName, RT_STR_TUPLE("font"))
            || !strcmp(pSym->szName, "bios_string")
            || !strcmp(pSym->szName, "vector_table")
            || !strcmp(pSym->szName, "pci_routing_table_structure")
            )
            return false;
    }

    if (!strcmp(pSym->szName, "cpu_reset"))
        pSym->cb = RT_MIN(pSym->cb, 5);
    else if (!strcmp(pSym->szName, "pci_init_end"))
        pSym->cb = RT_MIN(pSym->cb, 3);

    return true;
}


static bool disIs16BitCode(const char *pszSymbol)
{
    return true;
}

/*
< 00006590  67 aa 67 e7 67 1b 68 56  55 89 e5 83 ec 08 8a 46  |g.g.g.hVU......F|
< 000065a0  23 30 e4 3d e8 00 74 3f  3d 86 00 75 49 fb 8b 46  |#0.=..t?=..uI..F|

> 00006590  67 aa e7 67 1b 68 56 55  89 e5 83 ec 08 8a 46 23  |g..g.hVU......F#|
> 000065a0  30 e4 3d e8 00 74 40 3d  86 00 75 4a fb 8b 46 1e  |0.=..t@=..uJ..F.|
*/

/**
 * Deals with instructions that YASM will assemble differently than WASM/WCC.
 */
static size_t disHandleYasmDifferences(PDISCPUSTATE pCpuState, uint32_t uFlatAddr, uint32_t cbInstr,
                                       char *pszBuf, size_t cbBuf, size_t cchUsed)
{
    bool fDifferent = DISFormatYasmIsOddEncoding(pCpuState);
    uint8_t const  *pb = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];
    if (   pb[0] == 0x2a
        && pb[1] == 0xe4)
        fDifferent = true; /* sub ah, ah    - alternative form 0x28 0x?? */
    else if (   pb[0] == 0x2b
             && pb[1] == 0xc2)
        fDifferent = true; /* sub ax, dx    - alternative form 0x29 0xd0. */
    else if (   pb[0] == 0x1b
             && pb[1] == 0xff)
        fDifferent = true; /* sbb di, di    - alternative form 0x19 0xff. */
    else if (   pb[0] == 0x33
             && (   pb[1] == 0xdb /* xor bx, bx */
                 || pb[1] == 0xf6 /* xor si, si */
                 || pb[1] == 0xff /* xor di, di */
                 || pb[1] == 0xc0 /* xor ax, ax */
                ))
        fDifferent = true; /* xor x, x      - alternative form 0x31 xxxx. */
    else if (   pb[0] == 0x66
             && pb[1] == 0x33
             && pb[2] == 0xc0)
        fDifferent = true; /* xor eax, eax  - alternative form 0x66 0x31 0xc0. */
    else if (   pb[0] == 0xf3
             && pb[1] == 0x66
             && pb[2] == 0x6d)
        fDifferent = true; /* rep insd      - prefix switched. */
    else if (   pb[0] == 0xf3
             && pb[1] == 0x66
             && pb[2] == 0x26
             && pb[3] == 0x6f)
        fDifferent = true; /* rep es outsd  - prefix switched. */
    else if (   pb[0] == 0xc6
             && pb[1] == 0xc5
             && pb[2] == 0xba)
        fDifferent = true; /* mov ch, 0bah  - yasm uses a short sequence: 0xb5 0xba. */
    else if (   pb[0] == 0x8b
             && pb[1] == 0xe0)
        fDifferent = true; /* mov sp, ax    - alternative form 0x89 c4. */
    /*
     * Switch table fun (.sym may help):
     */
    else if (   pb[0] == 0x64
             && pb[1] == 0x65
             && pb[2] == 0x05
             /*&& pb[3] == 0x61
             && pb[4] == 0x19*/)
        fDifferent = true; /* gs add ax, 01961h - both fs and gs prefix. Probably some switch table. */
    else if (   pb[0] == 0x65
             && pb[1] == 0x36
             && pb[2] == 0x65
             && pb[3] == 0xae)
        fDifferent = true; /* gs scasb - switch table or smth. */
    else if (   pb[0] == 0x67
             && pb[1] == 0xe7
             /*&& pb[2] == 0x67*/)
        fDifferent = true; /* out 067h, ax - switch table or smth. */
    /*
     * Disassembler / formatter bugs:
     */
    else if (pb[0] == 0x6c && RT_C_IS_SPACE(*pszBuf))
        fDifferent = true;


    /*
     * Handle different stuff.
     */
    if (fDifferent)
    {
        disByteData(uFlatAddr, cbInstr); /* lazy bird. */

        if (cchUsed + 2 < cbBuf)
        {
            memmove(pszBuf + 2, pszBuf, cchUsed + 2);
            cchUsed += 2;
        }

        pszBuf[0] = ';';
        pszBuf[1] = ' ';
    }

    return cchUsed;
}


/**
 * Disassembler callback for reading opcode bytes.
 *
 * @returns VINF_SUCCESS.
 * @param   uFlatAddr           The address to read at.
 * @param   pbDst               Where to store them.
 * @param   cbToRead            How many to read.
 * @param   pvUser              Unused.
 */
static DECLCALLBACK(int) disReadOpcodeBytes(RTUINTPTR uFlatAddr, uint8_t *pbDst, unsigned cbToRead, void *pvUser)
{
    if (uFlatAddr + cbToRead >= VBOX_BIOS_BASE + _64K)
    {
        RT_BZERO(pbDst, cbToRead);
        if (uFlatAddr >= VBOX_BIOS_BASE + _64K)
            cbToRead = 0;
        else
            cbToRead = VBOX_BIOS_BASE + _64K - uFlatAddr;
    }
    memcpy(pbDst, &g_pbImg[uFlatAddr - VBOX_BIOS_BASE], cbToRead);
    NOREF(pvUser);
    return VINF_SUCCESS;
}


/**
 * Disassembles code.
 *
 * @returns @c true on success, @c false on failure.
 * @param   uFlatAddr           The address where the code starts.
 * @param   cb                  The amount of code to disassemble.
 * @param   fIs16Bit            Is is 16-bit (@c true) or 32-bit (@c false).
 */
static bool disCode(uint32_t uFlatAddr, uint32_t cb, bool fIs16Bit)
{
    uint8_t const  *pb = &g_pbImg[uFlatAddr - VBOX_BIOS_BASE];

    while (cb > 0)
    {
        /* Trailing zero padding detection. */
        if (   *pb == '\0'
            && ASMMemIsAll8(pb, RT_MIN(cb, 8), 0) == NULL)
        {
            void    *pv      = ASMMemIsAll8(pb, cb, 0);
            uint32_t cbZeros = pv ? (uint32_t)((uint8_t const *)pv - pb) : cb;
            if (!outputPrintf("    times %#x db 0\n", cbZeros))
                return false;
            cb -= cbZeros;
            pb += cbZeros;
            uFlatAddr += cbZeros;
            if (   cb == 2
                && pb[0] == 'X'
                && pb[1] == 'M')
                return disStringData(uFlatAddr, cb);
        }
        /* Work arounds for switch tables and such (disas assertions). */
        else if (   (   pb[0] == 0x11   /* int13_cdemu switch */
                     && pb[1] == 0xda
                     && pb[2] == 0x05
                     && pb[3] == 0xff
                     && pb[4] == 0xff
                    )
                 || 0
                 )
            return disByteData(uFlatAddr, cb);
        else
        {
            unsigned    cbInstr;
            DISCPUSTATE CpuState;
            int rc = DISCoreOneEx(uFlatAddr, fIs16Bit ? CPUMODE_16BIT : CPUMODE_32BIT,
                                  disReadOpcodeBytes, NULL, &CpuState, &cbInstr);
            if (   RT_SUCCESS(rc)
                && cbInstr <= cb)
            {
                char szTmp[4096];
                size_t cch = DISFormatYasmEx(&CpuState, szTmp, sizeof(szTmp),
                                             DIS_FMT_FLAGS_STRICT
                                             | DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_BYTES_COMMENT | DIS_FMT_FLAGS_BYTES_SPACED,
                                             NULL, NULL);
                cch = disHandleYasmDifferences(&CpuState, uFlatAddr, cbInstr, szTmp, sizeof(szTmp), cch);
                Assert(cch < sizeof(szTmp));

                if (g_cVerbose > 1)
                {
                    while (cch < 72)
                        szTmp[cch++] = ' ';
                    RTStrPrintf(&szTmp[cch], sizeof(szTmp) - cch, "; %#x", uFlatAddr);
                }

                if (!outputPrintf("    %s\n", szTmp))
                    return false;
                cb -= cbInstr;
                pb += cbInstr;
                uFlatAddr += cbInstr;
            }
            else
            {
                if (!disByteData(uFlatAddr, 1))
                    return false;
                cb--;
                pb++;
                uFlatAddr++;
            }
        }
    }
    return true;
}


static bool disCodeSegment(uint32_t iSeg)
{
    uint32_t    uFlatAddr = g_aSegs[iSeg].uFlatAddr;
    uint32_t    cb        = g_aSegs[iSeg].cb;

    while (cb > 0)
    {
        uint32_t    off;
        RTDBGSYMBOL Sym;
        disGetNextSymbol(uFlatAddr, cb, &off, &Sym);

        if (off > 0)
        {
            if (!disByteData(uFlatAddr, off))
                return false;
            cb        -= off;
            uFlatAddr += off;
            off        = 0;
            if (!cb)
                break;
        }

        bool fRc;
        if (off == 0)
        {
            size_t cchName = strlen(Sym.szName);
            fRc = outputPrintf("%s: %*s; %#x LB %#x\n", Sym.szName, cchName < 41 - 2 ? cchName - 41 - 2 : 0, "", uFlatAddr, Sym.cb);
            if (!fRc)
                return false;

            if (disIsCodeAndAdjustSize(uFlatAddr, &Sym, &g_aSegs[iSeg]))
                fRc = disCode(uFlatAddr, Sym.cb, disIs16BitCode(Sym.szName));
            else
                fRc = disByteData(uFlatAddr, Sym.cb);

            uFlatAddr += Sym.cb;
            cb        -= Sym.cb;
        }
        else
        {
            fRc = disByteData(uFlatAddr, cb);
            uFlatAddr += cb;
            cb = 0;
        }
        if (!fRc)
            return false;
    }

    return true;
}


static RTEXITCODE DisassembleBiosImage(void)
{
    if (!outputPrintf(""))
        return RTEXITCODE_FAILURE;

    /*
     * Work the image segment by segment.
     */
    bool        fRc       = true;
    uint32_t    uFlatAddr = VBOX_BIOS_BASE;
    for (uint32_t iSeg = 0; iSeg < g_cSegs && fRc; iSeg++)
    {
        /* Is there a gap between the segments? */
        if (uFlatAddr < g_aSegs[iSeg].uFlatAddr)
        {
            fRc = disCopySegmentGap(uFlatAddr, g_aSegs[iSeg].uFlatAddr - uFlatAddr);
            if (!fRc)
                break;
            uFlatAddr = g_aSegs[iSeg].uFlatAddr;
        }
        else if (uFlatAddr > g_aSegs[iSeg].uFlatAddr)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Overlapping segments: %u and %u; uFlatAddr=%#x\n", iSeg - 1, iSeg, uFlatAddr);

        /* Disassemble the segment. */
        fRc = outputPrintf("\n"
                           "section %s progbits vstart=%#x align=1 ; size=%#x class=%s group=%s\n",
                           g_aSegs[iSeg].szName, g_aSegs[iSeg].uFlatAddr - VBOX_BIOS_BASE,
                           g_aSegs[iSeg].cb, g_aSegs[iSeg].szClass, g_aSegs[iSeg].szGroup);
        if (!fRc)
            return RTEXITCODE_FAILURE;
        if (!strcmp(g_aSegs[iSeg].szName, "CONST"))
            fRc = disConstSegment(iSeg);
        else if (!strcmp(g_aSegs[iSeg].szClass, "DATA"))
            fRc = disDataSegment(iSeg);
        else
            fRc = disCodeSegment(iSeg);

        /* Advance. */
        uFlatAddr += g_aSegs[iSeg].cb;
    }

    /* Final gap. */
    if (uFlatAddr < VBOX_BIOS_BASE + _64K)
        fRc = disCopySegmentGap(uFlatAddr, VBOX_BIOS_BASE + _64K - uFlatAddr);
    else if (uFlatAddr > VBOX_BIOS_BASE + _64K)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Last segment spills beyond 1MB; uFlatAddr=%#x\n", uFlatAddr);

    if (!fRc)
        return RTEXITCODE_FAILURE;
    return RTEXITCODE_SUCCESS;
}



/**
 * Parses the symbol file for the BIOS.
 *
 * This is in ELF/DWARF format.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosSym          Path to the sym file.
 */
static RTEXITCODE ParseSymFile(const char *pszBiosSym)
{
    /** @todo use RTDbg* later. (Just checking for existance currently.) */
    PRTSTREAM hStrm;
    int rc = RTStrmOpen(pszBiosSym, "rb", &hStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening '%s': %Rrc", pszBiosSym, rc);
    RTStrmClose(hStrm);
    return RTEXITCODE_SUCCESS;
}


/**
 * Reads a line from the file.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapReadLine(PBIOSMAP pMap)
{
    int rc = RTStrmGetLine(pMap->hStrm, pMap->szLine, sizeof(pMap->szLine));
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_EOF)
        {
            pMap->fEof      = true;
            pMap->cch       = 0;
            pMap->offNW     = 0;
            pMap->szLine[0] = '\0';
        }
        else
            RTMsgError("%s:%d: Read error %Rrc", pMap->pszMapFile, pMap->iLine + 1, rc);
        return false;
    }
    pMap->iLine++;
    pMap->cch   = (uint32_t)strlen(pMap->szLine);

    /* Check out leading white space. */
    if (!RT_C_IS_SPACE(pMap->szLine[0]))
        pMap->offNW = 0;
    else
    {
        uint32_t off = 1;
        while (RT_C_IS_SPACE(pMap->szLine[off]))
            off++;
        pMap->offNW = off;
    }

    return true;
}


/**
 * Checks if it is an empty line.
 * @returns @c true if empty, @c false if not.
 * @param   pMap                The map file handle.
 */
static bool mapIsEmptyLine(PBIOSMAP pMap)
{
    Assert(pMap->offNW <= pMap->cch);
    return pMap->offNW == pMap->cch;
}


/**
 * Reads ahead in the map file until a non-empty line or EOF is encountered.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapSkipEmptyLines(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLine(pMap))
            return false;
        if (pMap->offNW < pMap->cch)
            return true;
    }
}


/**
 * Reads ahead in the map file until an empty line or EOF is encountered.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapSkipNonEmptyLines(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLine(pMap))
            return false;
        if (pMap->offNW == pMap->cch)
            return true;
    }
}


/**
 * Strips the current line.
 *
 * The string length may change.
 *
 * @returns Pointer to the first non-space character.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapStripCurrentLine(PBIOSMAP pMap, size_t *pcch)
{
    char *psz    = &pMap->szLine[pMap->offNW];
    char *pszEnd = &pMap->szLine[pMap->cch];
    while (   (uintptr_t)pszEnd > (uintptr_t)psz
           && RT_C_IS_SPACE(pszEnd[-1]))
    {
        *--pszEnd = '\0';
        pMap->cch--;
    }
    if (pcch)
        *pcch = pszEnd - psz;
    return psz;
}


/**
 * Reads a line from the file and right strips it.
 *
 * @returns Pointer to szLine on success, @c NULL + msg on failure, @c NULL on
 *          EOF.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapReadLineStripRight(PBIOSMAP pMap, size_t *pcch)
{
    if (!mapReadLine(pMap))
        return NULL;
    mapStripCurrentLine(pMap, NULL);
    if (pcch)
        *pcch = pMap->cch;
    return pMap->szLine;
}


/**
 * mapReadLine() + mapStripCurrentLine().
 *
 * @returns Pointer to the first non-space character in the new line. NULL on
 *          read error (bitched already) or end of file.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapReadLineStrip(PBIOSMAP pMap, size_t *pcch)
{
    if (!mapReadLine(pMap))
        return NULL;
    return mapStripCurrentLine(pMap, pcch);
}


/**
 * Parses a word, copying it into the supplied buffer, and skipping any spaces
 * following it.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pszBuf              The output buffer.
 * @param   cbBuf               The size of the output buffer.
 */
static bool mapParseWord(char **ppszCursor, char *pszBuf, size_t cbBuf)
{
    /* Check that we start on a non-blank. */
    char *pszStart  = *ppszCursor;
    if (!*pszStart || RT_C_IS_SPACE(*pszStart))
        return false;

    /* Find the end of the word. */
    char *psz = pszStart + 1;
    while (*psz && !RT_C_IS_SPACE(*psz))
        psz++;

    /* Copy it. */
    size_t cchWord = (uintptr_t)psz - (uintptr_t)pszStart;
    if (cchWord >= cbBuf)
        return false;
    memcpy(pszBuf, pszStart, cchWord);
    pszBuf[cchWord] = '\0';

    /* Skip blanks following it. */
    while (RT_C_IS_SPACE(*psz))
        psz++;
    *ppszCursor = psz;
    return true;
}


/**
 * Parses an 16:16 address.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pAddr               Where to return the address.
 */
static bool mapParseAddress(char **ppszCursor, PRTFAR16 pAddr)
{
    char szWord[32];
    if (!mapParseWord(ppszCursor, szWord, sizeof(szWord)))
        return false;
    size_t cchWord = strlen(szWord);

    /* An address is at least 16:16 format. It may be 16:32. It may also be flagged. */
    size_t cchAddr = 4 + 1 + 4;
    if (cchWord < cchAddr)
        return false;
    if (   !RT_C_IS_XDIGIT(szWord[0])
        || !RT_C_IS_XDIGIT(szWord[1])
        || !RT_C_IS_XDIGIT(szWord[2])
        || !RT_C_IS_XDIGIT(szWord[3])
        || szWord[4] != ':'
        || !RT_C_IS_XDIGIT(szWord[5])
        || !RT_C_IS_XDIGIT(szWord[6])
        || !RT_C_IS_XDIGIT(szWord[7])
        || !RT_C_IS_XDIGIT(szWord[8])
       )
        return false;
    if (   cchWord > cchAddr
        && RT_C_IS_XDIGIT(szWord[9])
        && RT_C_IS_XDIGIT(szWord[10])
        && RT_C_IS_XDIGIT(szWord[11])
        && RT_C_IS_XDIGIT(szWord[12]))
        cchAddr += 4;

    /* Drop flag if present. */
    if (cchWord > cchAddr)
    {
        if (RT_C_IS_XDIGIT(szWord[4+1+4]))
            return false;
        szWord[cchAddr] = '\0';
        cchWord = cchAddr;
    }

    /* Convert it. */
    szWord[4] = '\0';
    int rc1 = RTStrToUInt16Full(szWord,     16, &pAddr->sel);
    if (rc1 != VINF_SUCCESS)
        return false;

    int rc2 = RTStrToUInt16Full(szWord + 5, 16, &pAddr->off);
    if (rc2 != VINF_SUCCESS)
        return false;
    return true;
}


/**
 * Parses a size.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pcb                 Where to return the size.
 */
static bool mapParseSize(char **ppszCursor, uint32_t *pcb)
{
    char szWord[32];
    if (!mapParseWord(ppszCursor, szWord, sizeof(szWord)))
        return false;
    size_t cchWord = strlen(szWord);
    if (cchWord != 8)
        return false;

    int rc = RTStrToUInt32Full(szWord, 16, pcb);
    if (rc != VINF_SUCCESS)
        return false;
    return true;
}


/**
 * Parses a section box and the following column header.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap            Map file handle.
 * @param   pszSectionNm    The expected section name.
 * @param   cColumns        The number of columns.
 * @param   ...             The column names.
 */
static bool mapSkipThruColumnHeadings(PBIOSMAP pMap, const char *pszSectionNm, uint32_t cColumns, ...)
{
    if (   mapIsEmptyLine(pMap)
        && !mapSkipEmptyLines(pMap))
        return false;

    /* +------------+ */
    size_t cch;
    char  *psz = mapStripCurrentLine(pMap, &cch);
    if (!psz)
        return false;

    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
    {
        RTMsgError("%s:%d: Expected section box: +-----...", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    /* |   pszSectionNm   | */
    psz = mapReadLineStrip(pMap, &cch);
    if (!psz)
        return false;

    size_t cchSectionNm = strlen(pszSectionNm);
    if (   psz[0] != '|'
        || psz[1] != ' '
        || psz[2] != ' '
        || psz[3] != ' '
        || psz[cch - 4] != ' '
        || psz[cch - 3] != ' '
        || psz[cch - 2] != ' '
        || psz[cch - 1] != '|'
        || cch != 1 + 3 + cchSectionNm + 3 + 1
        || strncmp(&psz[4], pszSectionNm, cchSectionNm)
        )
    {
        RTMsgError("%s:%d: Expected section box: |   %s   |", pMap->pszMapFile, pMap->iLine, pszSectionNm);
        return false;
    }

    /* +------------+ */
    psz = mapReadLineStrip(pMap, &cch);
    if (!psz)
        return false;
    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
    {
        RTMsgError("%s:%d: Expected section box: +-----...", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    /* There may be a few lines describing the table notation now, surrounded by blank lines. */
    do
    {
        psz = mapReadLineStripRight(pMap, &cch);
        if (!psz)
            return false;
    } while (   *psz == '\0'
             || (   !RT_C_IS_SPACE(psz[0])
                 && RT_C_IS_SPACE(psz[1])
                 && psz[2] == '='
                 && RT_C_IS_SPACE(psz[3]))
            );

    /* Should have the column heading now. */
    va_list va;
    va_start(va, cColumns);
    for (uint32_t i = 0; i < cColumns; i++)
    {
        const char *pszColumn = va_arg(va, const char *);
        size_t      cchColumn = strlen(pszColumn);
        if (   strncmp(psz, pszColumn, cchColumn)
            || (    psz[cchColumn] != '\0'
                && !RT_C_IS_SPACE(psz[cchColumn])))
        {
            va_end(va);
            RTMsgError("%s:%d: Expected column '%s' found '%s'", pMap->pszMapFile, pMap->iLine, pszColumn, psz);
            return false;
        }
        psz += cchColumn;
        while (RT_C_IS_SPACE(*psz))
            psz++;
    }
    va_end(va);

    /* The next line is the underlining. */
    psz = mapReadLineStripRight(pMap, &cch);
    if (!psz)
        return false;
    if (*psz != '=' || psz[cch - 1] != '=')
    {
        RTMsgError("%s:%d: Expected column header underlining", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    /* Skip one blank line. */
    psz = mapReadLineStripRight(pMap, &cch);
    if (!psz)
        return false;
    if (*psz)
    {
        RTMsgError("%s:%d: Expected blank line beneath the column headers", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    return true;
}


/**
 * Parses a segment list.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapParseSegments(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLineStripRight(pMap, NULL))
            return false;

        /* The end? The line should be empty. Expectes segment name to not
           start with a space. */
        if (!pMap->szLine[0] || RT_C_IS_SPACE(pMap->szLine[0]))
        {
            if (!pMap->szLine[0])
                return true;
            RTMsgError("%s:%u: Malformed segment line", pMap->pszMapFile, pMap->iLine);
            return false;
        }

        /* Parse the segment line. */
        uint32_t iSeg = g_cSegs;
        if (iSeg >= RT_ELEMENTS(g_aSegs))
        {
            RTMsgError("%s:%u: Too many segments", pMap->pszMapFile, pMap->iLine);
            return false;
        }

        char *psz = pMap->szLine;
        if (!mapParseWord(&psz, g_aSegs[iSeg].szName, sizeof(g_aSegs[iSeg].szName)))
            RTMsgError("%s:%u: Segment name parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, g_aSegs[iSeg].szClass, sizeof(g_aSegs[iSeg].szClass)))
            RTMsgError("%s:%u: Segment class parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, g_aSegs[iSeg].szGroup, sizeof(g_aSegs[iSeg].szGroup)))
            RTMsgError("%s:%u: Segment group parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseAddress(&psz, &g_aSegs[iSeg].Address))
            RTMsgError("%s:%u: Segment address parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseSize(&psz, &g_aSegs[iSeg].cb))
            RTMsgError("%s:%u: Segment size parser error", pMap->pszMapFile, pMap->iLine);
        else
        {
            g_aSegs[iSeg].uFlatAddr = ((uint32_t)g_aSegs[iSeg].Address.sel << 4) + g_aSegs[iSeg].Address.off;
            g_cSegs++;
            if (g_cVerbose > 2)
                RTStrmPrintf(g_pStdErr, "read segment at %08x / %04x:%04x LB %04x %s / %s / %s\n",
                             g_aSegs[iSeg].uFlatAddr,
                             g_aSegs[iSeg].Address.sel,
                             g_aSegs[iSeg].Address.off,
                             g_aSegs[iSeg].cb,
                             g_aSegs[iSeg].szName,
                             g_aSegs[iSeg].szClass,
                             g_aSegs[iSeg].szGroup);

            while (RT_C_IS_SPACE(*psz))
                psz++;
            if (!*psz)
                continue;
            RTMsgError("%s:%u: Junk at end of line", pMap->pszMapFile, pMap->iLine);
        }
        return false;
    }
}


/**
 * Sorts the segment array by flat address and adds them to the debug module.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 */
static bool mapSortAndAddSegments(void)
{
    for (uint32_t i = 0; i < g_cSegs; i++)
    {
        for (uint32_t j = i + 1; j < g_cSegs; j++)
            if (g_aSegs[j].uFlatAddr < g_aSegs[i].uFlatAddr)
            {
                BIOSSEG Tmp = g_aSegs[i];
                g_aSegs[i] = g_aSegs[j];
                g_aSegs[j] = Tmp;
            }
        if (g_cVerbose > 0)
            RTStrmPrintf(g_pStdErr, "segment at %08x / %04x:%04x LB %04x %s / %s / %s\n",
                         g_aSegs[i].uFlatAddr,
                         g_aSegs[i].Address.sel,
                         g_aSegs[i].Address.off,
                         g_aSegs[i].cb,
                         g_aSegs[i].szName,
                         g_aSegs[i].szClass,
                         g_aSegs[i].szGroup);

        RTDBGSEGIDX idx = i;
        int rc = RTDbgModSegmentAdd(g_hMapMod, g_aSegs[i].uFlatAddr, g_aSegs[i].cb, g_aSegs[i].szName, 0 /*fFlags*/, &idx);
        if (RT_FAILURE(rc))
        {
            RTMsgError("RTDbgModSegmentAdd failed on %s: %Rrc", g_aSegs[i].szName);
            return false;
        }
    }
    return true;
}


/**
 * Parses a segment list.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapParseSymbols(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLineStripRight(pMap, NULL))
            return false;

        /* The end? The line should be empty. Expectes segment name to not
           start with a space. */
        if (!pMap->szLine[0] || RT_C_IS_SPACE(pMap->szLine[0]))
        {
            if (!pMap->szLine[0])
                return true;
            RTMsgError("%s:%u: Malformed symbol line", pMap->pszMapFile, pMap->iLine);
            return false;
        }

        /* Skip the module name lines for now. */
        if (!strncmp(pMap->szLine, RT_STR_TUPLE("Module: ")))
            continue;

        /* Parse the segment line. */
        char    szName[4096];
        RTFAR16 Addr;
        char   *psz = pMap->szLine;
        if (!mapParseAddress(&psz, &Addr))
            RTMsgError("%s:%u: Symbol address parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, szName, sizeof(szName)))
            RTMsgError("%s:%u: Symbol name parser error", pMap->pszMapFile, pMap->iLine);
        else
        {
            uint32_t uFlatAddr = ((uint32_t)Addr.sel << 4) + Addr.off;
            if (uFlatAddr == 0)
                continue;

            int rc = RTDbgModSymbolAdd(g_hMapMod, szName, RTDBGSEGIDX_RVA, uFlatAddr, 0 /*cb*/,  0 /*fFlags*/, NULL);
            if (RT_SUCCESS(rc) || rc == VERR_DBG_ADDRESS_CONFLICT)
            {

                if (g_cVerbose > 2)
                    RTStrmPrintf(g_pStdErr, "read symbol - %08x %s\n", uFlatAddr, szName);
                while (RT_C_IS_SPACE(*psz))
                    psz++;
                if (!*psz)
                    continue;
                RTMsgError("%s:%u: Junk at end of line", pMap->pszMapFile, pMap->iLine);
            }
            else
                RTMsgError("%s:%u: RTDbgModSymbolAdd failed: %Rrc", pMap->pszMapFile, pMap->iLine, rc);
        }
        return false;
    }
}


/**
 * Parses the given map file.
 *
 * @returns RTEXITCODE_SUCCESS and lots of globals, or RTEXITCODE_FAILURE and a
 *          error message.
 * @param   pMap                The map file handle.
 */
static RTEXITCODE mapParseFile(PBIOSMAP pMap)
{
    int rc = RTDbgModCreate(&g_hMapMod, "VBoxBios", 0 /*cbSeg*/, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDbgModCreate failed: %Rrc", rc);

    /*
     * Read the header.
     */
    if (!mapReadLine(pMap))
        return RTEXITCODE_FAILURE;
    if (strncmp(pMap->szLine, RT_STR_TUPLE("Open Watcom Linker Version")))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unexpected map-file header: '%s'", pMap->szLine);
    if (   !mapSkipNonEmptyLines(pMap)
        || !mapSkipEmptyLines(pMap))
        return RTEXITCODE_FAILURE;

    /*
     * Skip groups.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Groups", 3, "Group", "Address", "Size", NULL))
        return RTEXITCODE_FAILURE;
    if (!mapSkipNonEmptyLines(pMap))
        return RTEXITCODE_FAILURE;

    /*
     * Parse segments.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Segments", 5, "Segment", "Class", "Group", "Address", "Size"))
        return RTEXITCODE_FAILURE;
    if (!mapParseSegments(pMap))
        return RTEXITCODE_FAILURE;
    if (!mapSortAndAddSegments())
        return RTEXITCODE_FAILURE;

    /*
     * Parse symbols.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Memory Map", 2, "Address", "Symbol"))
        return RTEXITCODE_FAILURE;
    if (!mapParseSymbols(pMap))
        return RTEXITCODE_FAILURE;

    /* Ignore the rest of the file. */
    return RTEXITCODE_SUCCESS;
}


/**
 * Parses the linker map file for the BIOS.
 *
 * This is generated by the Watcom linker.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosMap          Path to the map file.
 */
static RTEXITCODE ParseMapFile(const char *pszBiosMap)
{
    BIOSMAP Map;
    Map.pszMapFile = pszBiosMap;
    Map.hStrm      = NULL;
    Map.iLine      = 0;
    Map.fEof       = false;
    Map.cch        = 0;
    Map.offNW      = 0;
    int rc = RTStrmOpen(pszBiosMap, "r", &Map.hStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening '%s': %Rrc", pszBiosMap, rc);
    RTEXITCODE rcExit = mapParseFile(&Map);
    RTStrmClose(Map.hStrm);
    return rcExit;
}


/**
 * Reads the BIOS image into memory (g_pbImg and g_cbImg).
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosImg          Path to the image file.
 */
static RTEXITCODE ReadBiosImage(const char *pszBiosImg)
{
    void  *pvImg;
    size_t cbImg;
    int rc = RTFileReadAll(pszBiosImg, &pvImg, &cbImg);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading '%s': %Rrc", pszBiosImg, rc);
    if (cbImg != _64K)
    {
        RTFileReadAllFree(pvImg, cbImg);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The BIOS image %u bytes intead of 64KB", cbImg);
    }

    g_pbImg = (uint8_t *)pvImg;
    g_cbImg = cbImg;
    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--bios-image",               'i',                    RTGETOPT_REQ_STRING },
        { "--bios-map",                 'm',                    RTGETOPT_REQ_STRING },
        { "--bios-sym",                 's',                    RTGETOPT_REQ_STRING },
        { "--output",                   'o',                    RTGETOPT_REQ_STRING },
        { "--verbose",                  'v',                    RTGETOPT_REQ_NOTHING },
        { "--quiet",                    'q',                    RTGETOPT_REQ_NOTHING },
    };

    const char *pszBiosMap = NULL;
    const char *pszBiosSym = NULL;
    const char *pszBiosImg = NULL;
    const char *pszOutput  = NULL;

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'i':
                if (pszBiosImg)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-image is given more than once");
                pszBiosImg = ValueUnion.psz;
                break;

            case 'm':
                if (pszBiosMap)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-map is given more than once");
                pszBiosMap = ValueUnion.psz;
                break;

            case 's':
                if (pszBiosSym)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-sym is given more than once");
                pszBiosSym = ValueUnion.psz;
                break;

            case 'o':
                if (pszOutput)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--output is given more than once");
                pszOutput = ValueUnion.psz;
                break;

            case 'v':
                g_cVerbose++;
                break;

            case 'q':
                g_cVerbose = 0;
                break;

            case 'H':
                RTPrintf("usage: %Rbn --bios-image <file.img> --bios-map <file.map> [--output <file.asm>]\n",
                         argv[0]);
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                RTPrintf("r%u\n", RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Got it all?
     */
    if (!pszBiosImg)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-image is required");
    if (!pszBiosMap)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-map is required");
    if (!pszBiosSym)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-sym is required");

    /*
     * Do the job.
     */
    RTEXITCODE rcExit;
    rcExit = ReadBiosImage(pszBiosImg);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = ParseMapFile(pszBiosMap);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = ParseSymFile(pszBiosSym);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = OpenOutputFile(pszOutput);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = DisassembleBiosImage();

    return rcExit;
}

