/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Disassembler.
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include "DBGFInternal.h"
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloca.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgfR3DisasInstrRead(RTUINTPTR pSrc, uint8_t *pDest, uint32_t size, void *pvUserdata);


/**
 * Structure used when disassembling and instructions in DBGF.
 * This is used so the reader function can get the stuff it needs.
 */
typedef struct
{
    /** The core structure. */
    DISCPUSTATE     Cpu;
    /** The VM handle. */
    PVM             pVM;
    /** Pointer to the first byte in the segemnt. */
    RTGCUINTPTR     GCPtrSegBase;
    /** Pointer to the byte after the end of the segment. (might have wrapped!) */
    RTGCUINTPTR     GCPtrSegEnd;
    /** The size of the segment minus 1. */
    RTGCUINTPTR     cbSegLimit;
    /** The guest paging mode. */
    PGMMODE         enmMode;
    /** Pointer to the current page - HC Ptr. */
    void const     *pvPageHC;
    /** Pointer to the current page - GC Ptr. */
    RTGCPTR         pvPageGC;
    /** Pointer to the next instruction (relative to GCPtrSegBase). */
    RTGCUINTPTR     GCPtrNext;
    /** The lock information that PGMPhysReleasePageMappingLock needs. */
    PGMPAGEMAPLOCK  PageMapLock;
    /** Whether the PageMapLock is valid or not. */
    bool            fLocked;
    /** 64 bits mode or not. */
    bool            f64Bits;
} DBGFDISASSTATE, *PDBGFDISASSTATE;



/**
 * Calls the dissassembler with the proper reader functions and such for disa
 *
 * @returns VBox status code.
 * @param   pVM         VM handle
 * @param   pSelInfo    The selector info.
 * @param   enmMode     The guest paging mode.
 * @param   GCPtr       The GC pointer (selector offset).
 * @param   pState      The disas CPU state.
 */
static int dbgfR3DisasInstrFirst(PVM pVM, PSELMSELINFO pSelInfo, PGMMODE enmMode, RTGCPTR GCPtr, PDBGFDISASSTATE pState)
{
    pState->GCPtrSegBase    = pSelInfo->GCPtrBase;
    pState->GCPtrSegEnd     = pSelInfo->cbLimit + 1 + (RTGCUINTPTR)pSelInfo->GCPtrBase;
    pState->cbSegLimit      = pSelInfo->cbLimit;
    pState->enmMode         = enmMode;
    pState->pvPageGC        = 0;
    pState->pvPageHC        = NULL;
    pState->pVM             = pVM;
    pState->fLocked         = false;
    pState->f64Bits         = enmMode >= PGMMODE_AMD64 && pSelInfo->Raw.Gen.u1Long;
    Assert((uintptr_t)GCPtr == GCPtr);
    uint32_t cbInstr;
    int rc = DISCoreOneEx(GCPtr,
                          pState->f64Bits
                          ? CPUMODE_64BIT
                          : pSelInfo->Raw.Gen.u1DefBig
                          ? CPUMODE_32BIT
                          : CPUMODE_16BIT,
                          dbgfR3DisasInstrRead,
                          &pState->Cpu,
                          &pState->Cpu,
                          &cbInstr);
    if (VBOX_SUCCESS(rc))
    {
        pState->GCPtrNext = GCPtr + cbInstr;
        return VINF_SUCCESS;
    }

    /* cleanup */
    if (pState->fLocked)
    {
        PGMPhysReleasePageMappingLock(pVM, &pState->PageMapLock);
        pState->fLocked = false;
    }
    return rc;
}


#if 0
/**
 * Calls the dissassembler for disassembling the next instruction.
 *
 * @returns VBox status code.
 * @param   pState      The disas CPU state.
 */
static int dbgfR3DisasInstrNext(PDBGFDISASSTATE pState)
{
    uint32_t cbInstr;
    int rc = DISInstr(&pState->Cpu, (void *)pState->GCPtrNext, 0, &cbInstr, NULL);
    if (VBOX_SUCCESS(rc))
    {
        pState->GCPtrNext = GCPtr + cbInstr;
        return VINF_SUCCESS;
    }
    return rc;
}
#endif


/**
 * Done with the dissassembler state, free associated resources.
 *
 * @param   pState      The disas CPU state ++.
 */
static void dbgfR3DisasInstrDone(PDBGFDISASSTATE pState)
{
    if (pState->fLocked)
    {
        PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);
        pState->fLocked = false;
    }
}


/**
 * Instruction reader.
 *
 * @returns VBox status code. (Why this is a int32_t and not just an int is also beyond me.)
 * @param   PtrSrc      Address to read from.
 *                      In our case this is relative to the selector pointed to by the 2nd user argument of uDisCpu.
 * @param   pu8Dst      Where to store the bytes.
 * @param   cbRead      Number of bytes to read.
 * @param   uDisCpu     Pointer to the disassembler cpu state. (Why this is a VBOXHUINTPTR is beyond me...)
 *                      In this context it's always pointer to the Core of a DBGFDISASSTATE.
 */
static DECLCALLBACK(int) dbgfR3DisasInstrRead(RTUINTPTR PtrSrc, uint8_t *pu8Dst, uint32_t cbRead, void *pvDisCpu)
{
    PDBGFDISASSTATE pState = (PDBGFDISASSTATE)pvDisCpu;
    Assert(cbRead > 0);
    for (;;)
    {
        RTGCUINTPTR GCPtr = PtrSrc + pState->GCPtrSegBase;

        /* Need to update the page translation? */
        if (    !pState->pvPageHC
            ||  (GCPtr >> PAGE_SHIFT) != (pState->pvPageGC >> PAGE_SHIFT))
        {
            int rc = VINF_SUCCESS;

            /* translate the address */
            pState->pvPageGC = GCPtr & PAGE_BASE_GC_MASK;
            if (MMHyperIsInsideArea(pState->pVM, pState->pvPageGC))
            {
                pState->pvPageHC = MMHyperGC2HC(pState->pVM, pState->pvPageGC);
                if (!pState->pvPageHC)
                    rc = VERR_INVALID_POINTER;
            }
            else
            {
                if (pState->fLocked)
                    PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);

                if (pState->enmMode <= PGMMODE_PROTECTED)
                    rc = PGMPhysGCPhys2CCPtrReadOnly(pState->pVM, pState->pvPageGC, &pState->pvPageHC, &pState->PageMapLock);
                else
                    rc = PGMPhysGCPtr2CCPtrReadOnly(pState->pVM, pState->pvPageGC, &pState->pvPageHC, &pState->PageMapLock);
                pState->fLocked = RT_SUCCESS_NP(rc);
            }
            if (VBOX_FAILURE(rc))
            {
                pState->pvPageHC = NULL;
                return rc;
            }
        }

        /* check the segemnt limit */
        if (!pState->f64Bits && PtrSrc > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /* calc how much we can read */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        if (!pState->f64Bits)
        {
            RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
            if (cb > cbSeg && cbSeg)
                cb = cbSeg;
        }
        if (cb > cbRead)
            cb = cbRead;

        /* read and advance */
        memcpy(pu8Dst, (char *)pState->pvPageHC + (GCPtr & PAGE_OFFSET_MASK), cb);
        cbRead -= cb;
        if (!cbRead)
            return VINF_SUCCESS;
        pu8Dst += cb;
        PtrSrc += cb;
    }
}


/**
 * @copydoc FNDISGETSYMBOL
 */
static DECLCALLBACK(int) dbgfR3DisasGetSymbol(PCDISCPUSTATE pCpu, uint32_t u32Sel, RTUINTPTR uAddress, char *pszBuf, size_t cchBuf, RTINTPTR *poff, void *pvUser)
{
    PDBGFDISASSTATE pState = (PDBGFDISASSTATE)pCpu;
    PCSELMSELINFO   pSelInfo = (PCSELMSELINFO)pvUser;
    DBGFSYMBOL      Sym;
    RTGCINTPTR      off;
    int             rc;

    if (DIS_FMT_SEL_IS_REG(u32Sel))
    {
        if (DIS_FMT_SEL_GET_REG(u32Sel) == DIS_SELREG_CS)
            rc = DBGFR3SymbolByAddr(pState->pVM, uAddress + pSelInfo->GCPtrBase, &off, &Sym);
        else
            rc = VERR_SYMBOL_NOT_FOUND; /** @todo implement this */
    }
    else
    {
        if (pSelInfo->Sel == DIS_FMT_SEL_GET_VALUE(u32Sel))
            rc = DBGFR3SymbolByAddr(pState->pVM, uAddress + pSelInfo->GCPtrBase, &off, &Sym);
        else
            rc = VERR_SYMBOL_NOT_FOUND; /** @todo implement this */
    }

    if (RT_SUCCESS(rc))
    {
        size_t cchName = strlen(Sym.szName);
        if (cchName >= cchBuf)
            cchName = cchBuf - 1;
        memcpy(pszBuf, Sym.szName, cchName);
        pszBuf[cchName] = '\0';

        *poff = off;
    }

    return rc;
}


/**
 * Disassembles the one instruction according to the specified flags and address.
 *
 * @returns VBox status code.
 * @param       pVM             VM handle.
 * @param       Sel             The code selector. This used to determin the 32/16 bit ness and
 *                              calculation of the actual instruction address.
 * @param       GCPtr           The code address relative to the base of Sel.
 * @param       fFlags          Flags controlling where to start and how to format.
 *                              A combination of the DBGF_DISAS_FLAGS_* \#defines.
 * @param       pszOutput       Output buffer.
 * @param       cchOutput       Size of the output buffer.
 * @param       pcbInstr        Where to return the size of the instruction.
 */
DBGFR3DECL(int) DBGFR3DisasInstrEx(PVM pVM, RTSEL Sel, RTGCPTR GCPtr, unsigned fFlags, char *pszOutput, uint32_t cchOutput, uint32_t *pcbInstr)
{
    /*
     * Get the Sel and GCPtr if fFlags requests that.
     */
    PCCPUMCTXCORE pCtxCore = NULL;
    CPUMSELREGHID *pHiddenSel = NULL;
    int rc;
    if (fFlags & (DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_CURRENT_HYPER))
    {
        if (fFlags & DBGF_DISAS_FLAGS_CURRENT_GUEST)
            pCtxCore = CPUMGetGuestCtxCore(pVM);
        else
            pCtxCore = CPUMGetHyperCtxCore(pVM);
        Sel        = pCtxCore->cs;
        pHiddenSel = (CPUMSELREGHID *)&pCtxCore->csHid;
        GCPtr      = pCtxCore->rip;
    }

    /*
     * Read the selector info - assume no stale selectors and nasty stuff like that.
     * Since the selector flags in the CPUMCTX structures aren't up to date unless
     * we recently visited REM, we'll not search for the selector there.
     */
    SELMSELINFO SelInfo;
    const PGMMODE enmMode = PGMGetGuestMode(pVM);
    bool fRealModeAddress = false;

    if (    pHiddenSel
        &&  CPUMAreHiddenSelRegsValid(pVM))
    {
        SelInfo.GCPtrBase           = pHiddenSel->u64Base;
        SelInfo.cbLimit             = pHiddenSel->u32Limit;
        SelInfo.fHyper              = false;
        SelInfo.fRealMode           = !!((pCtxCore && pCtxCore->eflags.Bits.u1VM) || enmMode == PGMMODE_REAL);
        SelInfo.Raw.au32[0]         = 0;
        SelInfo.Raw.au32[1]         = 0;
        SelInfo.Raw.Gen.u16LimitLow = ~0;
        SelInfo.Raw.Gen.u4LimitHigh = ~0;
        SelInfo.Raw.Gen.u1Present   = pHiddenSel->Attr.n.u1Present;
        SelInfo.Raw.Gen.u1Granularity = pHiddenSel->Attr.n.u1Granularity;;
        SelInfo.Raw.Gen.u1DefBig    = pHiddenSel->Attr.n.u1DefBig;
        SelInfo.Raw.Gen.u1Long      = pHiddenSel->Attr.n.u1Long;
        SelInfo.Raw.Gen.u1DescType  = pHiddenSel->Attr.n.u1DescType;
        SelInfo.Raw.Gen.u4Type      = pHiddenSel->Attr.n.u4Type;
        fRealModeAddress            = SelInfo.fRealMode;
    }
    else if (Sel == DBGF_SEL_FLAT)
    {
        SelInfo.GCPtrBase           = 0;
        SelInfo.cbLimit             = ~0;
        SelInfo.fHyper              = false;
        SelInfo.fRealMode           = false;
        SelInfo.Raw.au32[0]         = 0;
        SelInfo.Raw.au32[1]         = 0;
        SelInfo.Raw.Gen.u16LimitLow = ~0;
        SelInfo.Raw.Gen.u4LimitHigh = ~0;
        SelInfo.Raw.Gen.u1Present   = 1;
        SelInfo.Raw.Gen.u1Granularity = 1;
        SelInfo.Raw.Gen.u1DefBig    = 1;
        SelInfo.Raw.Gen.u1DescType  = 1;
        SelInfo.Raw.Gen.u4Type      = X86_SEL_TYPE_EO;
    }
    else if (   !(fFlags & DBGF_DISAS_FLAGS_CURRENT_HYPER)
             && (   (pCtxCore && pCtxCore->eflags.Bits.u1VM)
                 || enmMode == PGMMODE_REAL) )
    {   /* V86 mode or real mode - real mode addressing */
        SelInfo.GCPtrBase           = Sel * 16;
        SelInfo.cbLimit             = ~0;
        SelInfo.fHyper              = false;
        SelInfo.fRealMode           = true;
        SelInfo.Raw.au32[0]         = 0;
        SelInfo.Raw.au32[1]         = 0;
        SelInfo.Raw.Gen.u16LimitLow = ~0;
        SelInfo.Raw.Gen.u4LimitHigh = ~0;
        SelInfo.Raw.Gen.u1Present   = 1;
        SelInfo.Raw.Gen.u1Granularity = 1;
        SelInfo.Raw.Gen.u1DefBig    = 0; /* 16 bits */
        SelInfo.Raw.Gen.u1DescType  = 1;
        SelInfo.Raw.Gen.u4Type      = X86_SEL_TYPE_EO;
        fRealModeAddress            = true;
    }
    else
    {
        rc = SELMR3GetSelectorInfo(pVM, Sel, &SelInfo);
        if (VBOX_FAILURE(rc))
        {
            RTStrPrintf(pszOutput, cchOutput, "Sel=%04x -> %Vrc\n", Sel, rc);
            return rc;
        }
    }

    /*
     * Disassemble it.
     */
    DBGFDISASSTATE State;
    rc = dbgfR3DisasInstrFirst(pVM, &SelInfo, enmMode, GCPtr, &State);
    if (VBOX_FAILURE(rc))
    {
        RTStrPrintf(pszOutput, cchOutput, "Disas -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Format it.
     */
    char szBuf[512];
    DISFormatYasmEx(&State.Cpu, szBuf, sizeof(szBuf),
                    DIS_FMT_FLAGS_RELATIVE_BRANCH,
                    fFlags & DBGF_DISAS_FLAGS_NO_SYMBOLS ? NULL : dbgfR3DisasGetSymbol,
                    &SelInfo);

    /*
     * Print it to the user specified buffer.
     */
    if (fFlags & DBGF_DISAS_FLAGS_NO_BYTES)
    {
        if (fFlags & DBGF_DISAS_FLAGS_NO_ADDRESS)
            RTStrPrintf(pszOutput, cchOutput, "%s", szBuf);
        else if (fRealModeAddress)
            RTStrPrintf(pszOutput, cchOutput, "%04x:%04x  %s", Sel, (unsigned)GCPtr, szBuf);
        else if (Sel == DBGF_SEL_FLAT)
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cchOutput, "%VGv  %s", GCPtr, szBuf);
            else
                RTStrPrintf(pszOutput, cchOutput, "%VRv  %s", (RTRCPTR)GCPtr, szBuf);
        }
        else
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cchOutput, "%04x:%VGv  %s", Sel, GCPtr, szBuf);
            else
                RTStrPrintf(pszOutput, cchOutput, "%04x:%VRv  %s", Sel, (RTRCPTR)GCPtr, szBuf);
        }
    }
    else
    {
        uint32_t cbBits = State.Cpu.opsize;
        uint8_t *pau8Bits = (uint8_t *)alloca(cbBits);
        rc = dbgfR3DisasInstrRead(GCPtr, pau8Bits, cbBits, &State);
        AssertRC(rc);
        if (fFlags & DBGF_DISAS_FLAGS_NO_ADDRESS)
            RTStrPrintf(pszOutput, cchOutput, "%.*Vhxs%*s %s",
                        cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                        szBuf);
        else if (fRealModeAddress)
            RTStrPrintf(pszOutput, cchOutput, "%04x:%04x %.*Vhxs%*s %s",
                        Sel, (unsigned)GCPtr,
                        cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                        szBuf);
        else if (Sel == DBGF_SEL_FLAT)
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cchOutput, "%VGv %.*Vhxs%*s %s",
                            GCPtr,
                            cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                            szBuf);
            else
                RTStrPrintf(pszOutput, cchOutput, "%VRv %.*Vhxs%*s %s",
                            (RTRCPTR)GCPtr,
                            cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                            szBuf);
        }
        else
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cchOutput, "%04x:%VGv %.*Vhxs%*s %s",
                            Sel, GCPtr,
                            cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                            szBuf);
            else
                RTStrPrintf(pszOutput, cchOutput, "%04x:%VRv %.*Vhxs%*s %s",
                            Sel, (RTRCPTR)GCPtr,
                            cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                            szBuf);
        }
    }

    if (pcbInstr)
        *pcbInstr = State.Cpu.opsize;

    dbgfR3DisasInstrDone(&State);
    return VINF_SUCCESS;
}


/**
 * Disassembles an instruction.
 * Addresses will be tried resolved to symbols
 *
 * @returns VBox status code.
 * @param       pVM             VM handle.
 * @param       Sel             The code selector. This used to determin the 32/16 bit ness and
 *                              calculation of the actual instruction address.
 * @param       GCPtr           The code address relative to the base of Sel.
 * @param       pszOutput       Output buffer.
 * @param       cchOutput        Size of the output buffer.
 */
DBGFR3DECL(int) DBGFR3DisasInstr(PVM pVM, RTSEL Sel, RTGCPTR GCPtr, char *pszOutput, uint32_t cchOutput)
{
    return DBGFR3DisasInstrEx(pVM, Sel, GCPtr, 0, pszOutput, cchOutput, NULL);
}


/**
 * Disassembles the current guest context instruction.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param       pVM             VM handle.
 * @param       pszOutput       Output buffer.
 * @param       cchOutput       Size of the output buffer.
 */
DBGFR3DECL(int) DBGFR3DisasInstrCurrent(PVM pVM, char *pszOutput, uint32_t cchOutput)
{
    return DBGFR3DisasInstrEx(pVM, 0, 0, DBGF_DISAS_FLAGS_CURRENT_GUEST, pszOutput, cchOutput, NULL);
}


/**
 * Disassembles the current guest context instruction and writes it to the log.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param       pVM             VM handle.
 * @param       pszPrefix       Short prefix string to the dissassembly string. (optional)
 */
DBGFR3DECL(int) DBGFR3DisasInstrCurrentLogInternal(PVM pVM, const char *pszPrefix)
{
    char szBuf[256];
    szBuf[0] = '\0';
    int rc = DBGFR3DisasInstrCurrent(pVM, &szBuf[0], sizeof(szBuf));
    if (VBOX_FAILURE(rc))
        RTStrPrintf(szBuf, sizeof(szBuf), "DBGFR3DisasInstrCurrentLog failed with rc=%Vrc\n", rc);
    if (pszPrefix && *pszPrefix)
        RTLogPrintf("%s: %s\n", pszPrefix, szBuf);
    else
        RTLogPrintf("%s\n", szBuf);
    return rc;
}



/**
 * Disassembles the specified guest context instruction and writes it to the log.
 * Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param       pVM             VM handle.
 * @param       Sel             The code selector. This used to determin the 32/16 bit-ness and
 *                              calculation of the actual instruction address.
 * @param       GCPtr           The code address relative to the base of Sel.
 */
DBGFR3DECL(int) DBGFR3DisasInstrLogInternal(PVM pVM, RTSEL Sel, RTGCPTR GCPtr)
{
    char szBuf[256];
    szBuf[0] = '\0';
    int rc = DBGFR3DisasInstr(pVM, Sel, GCPtr, &szBuf[0], sizeof(szBuf));
    if (VBOX_FAILURE(rc))
        RTStrPrintf(szBuf, sizeof(szBuf), "DBGFR3DisasInstrLog(, %RTsel, %RGv) failed with rc=%Vrc\n", Sel, GCPtr, rc);
    RTLogPrintf("%s\n", szBuf);
    return rc;
}

