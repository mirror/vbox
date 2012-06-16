/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Disassembler.
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include "DBGFInternal.h"
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/vmm/vm.h>
#include "internal/pgm.h"

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloca.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Structure used when disassembling and instructions in DBGF.
 * This is used so the reader function can get the stuff it needs.
 */
typedef struct
{
    /** The core structure. */
    DISCPUSTATE     Cpu;
    /** Pointer to the VM. */
    PVM             pVM;
    /** The VMCPU handle. */
    PVMCPU          pVCpu;
    /** The address space for resolving symbol. */
    RTDBGAS         hAs;
    /** Pointer to the first byte in the segment. */
    RTGCUINTPTR     GCPtrSegBase;
    /** Pointer to the byte after the end of the segment. (might have wrapped!) */
    RTGCUINTPTR     GCPtrSegEnd;
    /** The size of the segment minus 1. */
    RTGCUINTPTR     cbSegLimit;
    /** The guest paging mode. */
    PGMMODE         enmMode;
    /** Pointer to the current page - R3 Ptr. */
    void const     *pvPageR3;
    /** Pointer to the current page - GC Ptr. */
    RTGCPTR         GCPtrPage;
    /** Pointer to the next instruction (relative to GCPtrSegBase). */
    RTGCUINTPTR     GCPtrNext;
    /** The lock information that PGMPhysReleasePageMappingLock needs. */
    PGMPAGEMAPLOCK  PageMapLock;
    /** Whether the PageMapLock is valid or not. */
    bool            fLocked;
    /** 64 bits mode or not. */
    bool            f64Bits;
} DBGFDISASSTATE, *PDBGFDISASSTATE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static FNDISREADBYTES dbgfR3DisasInstrRead;



/**
 * Calls the disassembler with the proper reader functions and such for disa
 *
 * @returns VBox status code.
 * @param   pVM         VM handle
 * @param   pVCpu       VMCPU handle
 * @param   pSelInfo    The selector info.
 * @param   enmMode     The guest paging mode.
 * @param   fFlags      DBGF_DISAS_FLAGS_XXX.
 * @param   GCPtr       The GC pointer (selector offset).
 * @param   pState      The disas CPU state.
 */
static int dbgfR3DisasInstrFirst(PVM pVM, PVMCPU pVCpu, PDBGFSELINFO pSelInfo, PGMMODE enmMode,
                                 RTGCPTR GCPtr, uint32_t fFlags, PDBGFDISASSTATE pState)
{
    pState->GCPtrSegBase    = pSelInfo->GCPtrBase;
    pState->GCPtrSegEnd     = pSelInfo->cbLimit + 1 + (RTGCUINTPTR)pSelInfo->GCPtrBase;
    pState->cbSegLimit      = pSelInfo->cbLimit;
    pState->enmMode         = enmMode;
    pState->GCPtrPage       = 0;
    pState->pvPageR3        = NULL;
    pState->hAs             = pSelInfo->fFlags & DBGFSELINFO_FLAGS_HYPER /** @todo Deal more explicitly with RC in DBGFR3Disas*. */
                            ? DBGF_AS_RC_AND_GC_GLOBAL
                            : DBGF_AS_GLOBAL;
    pState->pVM             = pVM;
    pState->pVCpu           = pVCpu;
    pState->fLocked         = false;
    pState->f64Bits         = enmMode >= PGMMODE_AMD64 && pSelInfo->u.Raw.Gen.u1Long;

    DISCPUMODE enmCpuMode;
    switch (fFlags & DBGF_DISAS_FLAGS_MODE_MASK)
    {
        default:
            AssertFailed();
        case DBGF_DISAS_FLAGS_DEFAULT_MODE:
            enmCpuMode   = pState->f64Bits
                         ? DISCPUMODE_64BIT
                         : pSelInfo->u.Raw.Gen.u1DefBig
                         ? DISCPUMODE_32BIT
                         : DISCPUMODE_16BIT;
            break;
        case DBGF_DISAS_FLAGS_16BIT_MODE:
        case DBGF_DISAS_FLAGS_16BIT_REAL_MODE:
            enmCpuMode = DISCPUMODE_16BIT;
            break;
        case DBGF_DISAS_FLAGS_32BIT_MODE:
            enmCpuMode = DISCPUMODE_32BIT;
            break;
        case DBGF_DISAS_FLAGS_64BIT_MODE:
            enmCpuMode = DISCPUMODE_64BIT;
            break;
    }

    uint32_t cbInstr;
    int rc = DISInstrWithReader(GCPtr,
                                enmCpuMode,
                                dbgfR3DisasInstrRead,
                                &pState->Cpu,
                                &pState->Cpu,
                                &cbInstr);
    if (RT_SUCCESS(rc))
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
 * Calls the disassembler for disassembling the next instruction.
 *
 * @returns VBox status code.
 * @param   pState      The disas CPU state.
 */
static int dbgfR3DisasInstrNext(PDBGFDISASSTATE pState)
{
    uint32_t cbInstr;
    int rc = DISInstr(&pState->Cpu, (void *)pState->GCPtrNext, 0, &cbInstr, NULL);
    if (RT_SUCCESS(rc))
    {
        pState->GCPtrNext = GCPtr + cbInstr;
        return VINF_SUCCESS;
    }
    return rc;
}
#endif


/**
 * Done with the disassembler state, free associated resources.
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
 * @callback_method_impl{FNDISREADBYTES}
 *
 * @remarks The source is relative to the base address indicated by
 *          DBGFDISASSTATE::GCPtrSegBase.
 */
static DECLCALLBACK(int) dbgfR3DisasInstrRead(PDISCPUSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
    PDBGFDISASSTATE pState = (PDBGFDISASSTATE)pDis;
    for (;;)
    {
        RTGCUINTPTR GCPtr = pDis->uInstrAddr + offInstr + pState->GCPtrSegBase;

        /*
         * Need to update the page translation?
         */
        if (    !pState->pvPageR3
            ||  (GCPtr >> PAGE_SHIFT) != (pState->GCPtrPage >> PAGE_SHIFT))
        {
            int rc = VINF_SUCCESS;

            /* translate the address */
            pState->GCPtrPage = GCPtr & PAGE_BASE_GC_MASK;
            if (MMHyperIsInsideArea(pState->pVM, pState->GCPtrPage))
            {
                pState->pvPageR3 = MMHyperRCToR3(pState->pVM, (RTRCPTR)pState->GCPtrPage);
                if (!pState->pvPageR3)
                    rc = VERR_INVALID_POINTER;
            }
            else
            {
                if (pState->fLocked)
                    PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);

                if (pState->enmMode <= PGMMODE_PROTECTED)
                    rc = PGMPhysGCPhys2CCPtrReadOnly(pState->pVM, pState->GCPtrPage, &pState->pvPageR3, &pState->PageMapLock);
                else
                    rc = PGMPhysGCPtr2CCPtrReadOnly(pState->pVCpu, pState->GCPtrPage, &pState->pvPageR3, &pState->PageMapLock);
                pState->fLocked = RT_SUCCESS_NP(rc);
            }
            if (RT_FAILURE(rc))
            {
                pState->pvPageR3 = NULL;
                return rc;
            }
        }

        /*
         * Check the segment limit.
         */
        if (!pState->f64Bits && pDis->uInstrAddr + offInstr > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /*
         * Calc how much we can read, maxing out the read.
         */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        if (!pState->f64Bits)
        {
            RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
            if (cb > cbSeg && cbSeg)
                cb = cbSeg;
        }
        if (cb > cbMaxRead)
            cb = cbMaxRead;

        /*
         * Read and advance,
         */
        memcpy(&pDis->abInstr[offInstr], (char *)pState->pvPageR3 + (GCPtr & PAGE_OFFSET_MASK), cb);
        offInstr  += (uint8_t)cb;
        if (cb >= cbMinRead)
        {
            pDis->cbCachedInstr = offInstr;
            return VINF_SUCCESS;
        }
        cbMaxRead -= (uint8_t)cb;
        cbMinRead -= (uint8_t)cb;
    }
}


/**
 * @copydoc FNDISGETSYMBOL
 */
static DECLCALLBACK(int) dbgfR3DisasGetSymbol(PCDISCPUSTATE pCpu, uint32_t u32Sel, RTUINTPTR uAddress, char *pszBuf, size_t cchBuf, RTINTPTR *poff, void *pvUser)
{
    PDBGFDISASSTATE pState   = (PDBGFDISASSTATE)pCpu;
    PCDBGFSELINFO   pSelInfo = (PCDBGFSELINFO)pvUser;
    DBGFADDRESS     Addr;
    RTDBGSYMBOL     Sym;
    RTGCINTPTR      off;
    int             rc;

    if (   DIS_FMT_SEL_IS_REG(u32Sel)
        ?  DIS_FMT_SEL_GET_REG(u32Sel) == DISSELREG_CS
        :  pSelInfo->Sel == DIS_FMT_SEL_GET_VALUE(u32Sel))
    {
        rc = DBGFR3AddrFromSelInfoOff(pState->pVM, &Addr, pSelInfo, uAddress);
        if (RT_SUCCESS(rc))
            rc = DBGFR3AsSymbolByAddr(pState->pVM, pState->hAs, &Addr, &off, &Sym, NULL /*phMod*/);
    }
    else
        rc = VERR_SYMBOL_NOT_FOUND; /** @todo implement this */
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
 * Disassembles the one instruction according to the specified flags and
 * address, internal worker executing on the EMT of the specified virtual CPU.
 *
 * @returns VBox status code.
 * @param       pVM             Pointer to the VM.
 * @param       pVCpu           The virtual CPU handle.
 * @param       Sel             The code selector. This used to determine the 32/16 bit ness and
 *                              calculation of the actual instruction address.
 * @param       pGCPtr          Pointer to the variable holding the code address
 *                              relative to the base of Sel.
 * @param       fFlags          Flags controlling where to start and how to format.
 *                              A combination of the DBGF_DISAS_FLAGS_* \#defines.
 * @param       pszOutput       Output buffer.
 * @param       cbOutput        Size of the output buffer.
 * @param       pcbInstr        Where to return the size of the instruction.
 */
static DECLCALLBACK(int)
dbgfR3DisasInstrExOnVCpu(PVM pVM, PVMCPU pVCpu, RTSEL Sel, PRTGCPTR pGCPtr, uint32_t fFlags,
                         char *pszOutput, uint32_t cbOutput, uint32_t *pcbInstr)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RTGCPTR GCPtr = *pGCPtr;

    /*
     * Get the Sel and GCPtr if fFlags requests that.
     */
    PCCPUMCTXCORE  pCtxCore   = NULL;
    PCPUMSELREGHID pHiddenSel = NULL;
    int rc;
    if (fFlags & (DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_CURRENT_HYPER))
    {
        if (fFlags & DBGF_DISAS_FLAGS_CURRENT_GUEST)
            pCtxCore = CPUMGetGuestCtxCore(pVCpu);
        else
            pCtxCore = CPUMGetHyperCtxCore(pVCpu);
        Sel        = pCtxCore->cs;
        pHiddenSel = (CPUMSELREGHID *)&pCtxCore->csHid;
        GCPtr      = pCtxCore->rip;
    }

    /*
     * Read the selector info - assume no stale selectors and nasty stuff like that.
     * Since the selector flags in the CPUMCTX structures aren't up to date unless
     * we recently visited REM, we'll not search for the selector there.
     */
    DBGFSELINFO     SelInfo;
    const PGMMODE   enmMode          = PGMGetGuestMode(pVCpu);
    bool            fRealModeAddress = false;

    if (    pHiddenSel
        &&  (   (fFlags & DBGF_DISAS_FLAGS_HID_SEL_REGS_VALID)
             || CPUMAreHiddenSelRegsValid(pVCpu)))
    {
        SelInfo.Sel                     = Sel;
        SelInfo.SelGate                 = 0;
        SelInfo.GCPtrBase               = pHiddenSel->u64Base;
        SelInfo.cbLimit                 = pHiddenSel->u32Limit;
        SelInfo.fFlags                  = PGMMODE_IS_LONG_MODE(enmMode)
                                        ? DBGFSELINFO_FLAGS_LONG_MODE
                                        : enmMode != PGMMODE_REAL && (!pCtxCore || !pCtxCore->eflags.Bits.u1VM)
                                        ? DBGFSELINFO_FLAGS_PROT_MODE
                                        : DBGFSELINFO_FLAGS_REAL_MODE;

        SelInfo.u.Raw.au32[0]           = 0;
        SelInfo.u.Raw.au32[1]           = 0;
        SelInfo.u.Raw.Gen.u16LimitLow   = 0xffff;
        SelInfo.u.Raw.Gen.u4LimitHigh   = 0xf;
        SelInfo.u.Raw.Gen.u1Present     = pHiddenSel->Attr.n.u1Present;
        SelInfo.u.Raw.Gen.u1Granularity = pHiddenSel->Attr.n.u1Granularity;;
        SelInfo.u.Raw.Gen.u1DefBig      = pHiddenSel->Attr.n.u1DefBig;
        SelInfo.u.Raw.Gen.u1Long        = pHiddenSel->Attr.n.u1Long;
        SelInfo.u.Raw.Gen.u1DescType    = pHiddenSel->Attr.n.u1DescType;
        SelInfo.u.Raw.Gen.u4Type        = pHiddenSel->Attr.n.u4Type;
        fRealModeAddress                = !!(SelInfo.fFlags & DBGFSELINFO_FLAGS_REAL_MODE);
    }
    else if (Sel == DBGF_SEL_FLAT)
    {
        SelInfo.Sel                     = Sel;
        SelInfo.SelGate                 = 0;
        SelInfo.GCPtrBase               = 0;
        SelInfo.cbLimit                 = ~0;
        SelInfo.fFlags                  = PGMMODE_IS_LONG_MODE(enmMode)
                                        ? DBGFSELINFO_FLAGS_LONG_MODE
                                        : enmMode != PGMMODE_REAL
                                        ? DBGFSELINFO_FLAGS_PROT_MODE
                                        : DBGFSELINFO_FLAGS_REAL_MODE;
        SelInfo.u.Raw.au32[0]           = 0;
        SelInfo.u.Raw.au32[1]           = 0;
        SelInfo.u.Raw.Gen.u16LimitLow   = 0xffff;
        SelInfo.u.Raw.Gen.u4LimitHigh   = 0xf;

        if (   (fFlags & DBGF_DISAS_FLAGS_HID_SEL_REGS_VALID)
            || CPUMAreHiddenSelRegsValid(pVCpu))
        {   /* Assume the current CS defines the execution mode. */
            pCtxCore   = CPUMGetGuestCtxCore(pVCpu);
            pHiddenSel = (CPUMSELREGHID *)&pCtxCore->csHid;

            SelInfo.u.Raw.Gen.u1Present     = pHiddenSel->Attr.n.u1Present;
            SelInfo.u.Raw.Gen.u1Granularity = pHiddenSel->Attr.n.u1Granularity;;
            SelInfo.u.Raw.Gen.u1DefBig      = pHiddenSel->Attr.n.u1DefBig;
            SelInfo.u.Raw.Gen.u1Long        = pHiddenSel->Attr.n.u1Long;
            SelInfo.u.Raw.Gen.u1DescType    = pHiddenSel->Attr.n.u1DescType;
            SelInfo.u.Raw.Gen.u4Type        = pHiddenSel->Attr.n.u4Type;
        }
        else
        {
            SelInfo.u.Raw.Gen.u1Present     = 1;
            SelInfo.u.Raw.Gen.u1Granularity = 1;
            SelInfo.u.Raw.Gen.u1DefBig      = 1;
            SelInfo.u.Raw.Gen.u1DescType    = 1;
            SelInfo.u.Raw.Gen.u4Type        = X86_SEL_TYPE_EO;
        }
    }
    else if (   !(fFlags & DBGF_DISAS_FLAGS_CURRENT_HYPER)
             && (   (pCtxCore && pCtxCore->eflags.Bits.u1VM)
                 || enmMode == PGMMODE_REAL
                 || (fFlags & DBGF_DISAS_FLAGS_MODE_MASK) == DBGF_DISAS_FLAGS_16BIT_REAL_MODE
                )
            )
    {   /* V86 mode or real mode - real mode addressing */
        SelInfo.Sel                     = Sel;
        SelInfo.SelGate                 = 0;
        SelInfo.GCPtrBase               = Sel * 16;
        SelInfo.cbLimit                 = ~0;
        SelInfo.fFlags                  = DBGFSELINFO_FLAGS_REAL_MODE;
        SelInfo.u.Raw.au32[0]           = 0;
        SelInfo.u.Raw.au32[1]           = 0;
        SelInfo.u.Raw.Gen.u16LimitLow   = 0xffff;
        SelInfo.u.Raw.Gen.u4LimitHigh   = 0xf;
        SelInfo.u.Raw.Gen.u1Present     = 1;
        SelInfo.u.Raw.Gen.u1Granularity = 1;
        SelInfo.u.Raw.Gen.u1DefBig      = 0; /* 16 bits */
        SelInfo.u.Raw.Gen.u1DescType    = 1;
        SelInfo.u.Raw.Gen.u4Type        = X86_SEL_TYPE_EO;
        fRealModeAddress                = true;
    }
    else
    {
        rc = SELMR3GetSelectorInfo(pVM, pVCpu, Sel, &SelInfo);
        if (RT_FAILURE(rc))
        {
            RTStrPrintf(pszOutput, cbOutput, "Sel=%04x -> %Rrc\n", Sel, rc);
            return rc;
        }
    }

    /*
     * Disassemble it.
     */
    DBGFDISASSTATE State;
    rc = dbgfR3DisasInstrFirst(pVM, pVCpu, &SelInfo, enmMode, GCPtr, fFlags, &State);
    if (RT_FAILURE(rc))
    {
        RTStrPrintf(pszOutput, cbOutput, "Disas -> %Rrc\n", rc);
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
            RTStrPrintf(pszOutput, cbOutput, "%s", szBuf);
        else if (fRealModeAddress)
            RTStrPrintf(pszOutput, cbOutput, "%04x:%04x  %s", Sel, (unsigned)GCPtr, szBuf);
        else if (Sel == DBGF_SEL_FLAT)
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cbOutput, "%RGv  %s", GCPtr, szBuf);
            else
                RTStrPrintf(pszOutput, cbOutput, "%08RX32  %s", (uint32_t)GCPtr, szBuf);
        }
        else
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cbOutput, "%04x:%RGv  %s", Sel, GCPtr, szBuf);
            else
                RTStrPrintf(pszOutput, cbOutput, "%04x:%08RX32  %s", Sel, (uint32_t)GCPtr, szBuf);
        }
    }
    else
    {
        uint32_t        cbInstr  = State.Cpu.cbInstr;
        uint8_t const  *pabInstr = State.Cpu.abInstr;
        if (fFlags & DBGF_DISAS_FLAGS_NO_ADDRESS)
            RTStrPrintf(pszOutput, cbOutput, "%.*Rhxs%*s %s",
                        cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                        szBuf);
        else if (fRealModeAddress)
            RTStrPrintf(pszOutput, cbOutput, "%04x:%04x %.*Rhxs%*s %s",
                        Sel, (unsigned)GCPtr,
                        cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                        szBuf);
        else if (Sel == DBGF_SEL_FLAT)
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cbOutput, "%RGv %.*Rhxs%*s %s",
                            GCPtr,
                            cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                            szBuf);
            else
                RTStrPrintf(pszOutput, cbOutput, "%08RX32 %.*Rhxs%*s %s",
                            (uint32_t)GCPtr,
                            cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                            szBuf);
        }
        else
        {
            if (enmMode >= PGMMODE_AMD64)
                RTStrPrintf(pszOutput, cbOutput, "%04x:%RGv %.*Rhxs%*s %s",
                            Sel, GCPtr,
                            cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                            szBuf);
            else
                RTStrPrintf(pszOutput, cbOutput, "%04x:%08RX32 %.*Rhxs%*s %s",
                            Sel, (uint32_t)GCPtr,
                            cbInstr, pabInstr, cbInstr < 8 ? (8 - cbInstr) * 3 : 0, "",
                            szBuf);
        }
    }

    if (pcbInstr)
        *pcbInstr = State.Cpu.cbInstr;

    dbgfR3DisasInstrDone(&State);
    return VINF_SUCCESS;
}


/**
 * Disassembles the one instruction according to the specified flags and address.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   idCpu           The ID of virtual CPU.
 * @param   Sel             The code selector. This used to determine the 32/16 bit ness and
 *                          calculation of the actual instruction address.
 * @param   GCPtr           The code address relative to the base of Sel.
 * @param   fFlags          Flags controlling where to start and how to format.
 *                          A combination of the DBGF_DISAS_FLAGS_* \#defines.
 * @param   pszOutput       Output buffer.  This will always be properly
 *                          terminated if @a cbOutput is greater than zero.
 * @param   cbOutput        Size of the output buffer.
 * @param   pcbInstr        Where to return the size of the instruction.
 *
 * @remarks May have to switch to the EMT of the virtual CPU in order to do
 *          address conversion.
 */
VMMR3DECL(int) DBGFR3DisasInstrEx(PVM pVM, VMCPUID idCpu, RTSEL Sel, RTGCPTR GCPtr, uint32_t fFlags,
                                  char *pszOutput, uint32_t cbOutput, uint32_t *pcbInstr)
{
    AssertReturn(cbOutput > 0, VERR_INVALID_PARAMETER);
    *pszOutput = '\0';
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    AssertReturn(!(fFlags & ~DBGF_DISAS_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & DBGF_DISAS_FLAGS_MODE_MASK) <= DBGF_DISAS_FLAGS_64BIT_MODE, VERR_INVALID_PARAMETER);

    /*
     * Optimize the common case where we're called on the EMT of idCpu since
     * we're using this all the time when logging.
     */
    int     rc;
    PVMCPU  pVCpu = VMMGetCpu(pVM);
    if (    pVCpu
        &&  pVCpu->idCpu == idCpu)
        rc = dbgfR3DisasInstrExOnVCpu(pVM, pVCpu, Sel, &GCPtr, fFlags, pszOutput, cbOutput, pcbInstr);
    else
        rc = VMR3ReqPriorityCallWait(pVM, idCpu, (PFNRT)dbgfR3DisasInstrExOnVCpu, 8,
                                     pVM, VMMGetCpuById(pVM, idCpu), Sel, &GCPtr, fFlags, pszOutput, cbOutput, pcbInstr);
    return rc;
}


/**
 * Disassembles the current guest context instruction.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param   pVCpu           VMCPU handle.
 * @param   pszOutput       Output buffer.  This will always be properly
 *                          terminated if @a cbOutput is greater than zero.
 * @param   cbOutput        Size of the output buffer.
 */
VMMR3DECL(int) DBGFR3DisasInstrCurrent(PVMCPU pVCpu, char *pszOutput, uint32_t cbOutput)
{
    AssertReturn(cbOutput > 0, VERR_INVALID_PARAMETER);
    *pszOutput = '\0';
    AssertReturn(pVCpu, VERR_INVALID_CONTEXT);
    return DBGFR3DisasInstrEx(pVCpu->pVMR3, pVCpu->idCpu, 0, 0,
                              DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                              pszOutput, cbOutput, NULL);
}


/**
 * Disassembles the current guest context instruction and writes it to the log.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param   pVCpu           VMCPU handle.
 * @param   pszPrefix       Short prefix string to the disassembly string. (optional)
 */
VMMR3DECL(int) DBGFR3DisasInstrCurrentLogInternal(PVMCPU pVCpu, const char *pszPrefix)
{
    char szBuf[256];
    szBuf[0] = '\0';
    int rc = DBGFR3DisasInstrCurrent(pVCpu, &szBuf[0], sizeof(szBuf));
    if (RT_FAILURE(rc))
        RTStrPrintf(szBuf, sizeof(szBuf), "DBGFR3DisasInstrCurrentLog failed with rc=%Rrc\n", rc);
    if (pszPrefix && *pszPrefix)
        RTLogPrintf("%s-CPU%u: %s\n", pszPrefix, pVCpu->idCpu, szBuf);
    else
        RTLogPrintf("%s\n", szBuf);
    return rc;
}



/**
 * Disassembles the specified guest context instruction and writes it to the log.
 * Addresses will be attempted resolved to symbols.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pVCpu           The virtual CPU handle, defaults to CPU 0 if NULL.
 * @param   Sel             The code selector. This used to determine the 32/16 bit-ness and
 *                          calculation of the actual instruction address.
 * @param   GCPtr           The code address relative to the base of Sel.
 * @param   pszPrefix       Short prefix string to the disassembly string. (optional)
 */
VMMR3DECL(int) DBGFR3DisasInstrLogInternal(PVMCPU pVCpu, RTSEL Sel, RTGCPTR GCPtr, const char *pszPrefix)
{
    char szBuf[256];
    int rc = DBGFR3DisasInstrEx(pVCpu->pVMR3, pVCpu->idCpu, Sel, GCPtr, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                &szBuf[0], sizeof(szBuf), NULL);
    if (RT_FAILURE(rc))
        RTStrPrintf(szBuf, sizeof(szBuf), "DBGFR3DisasInstrLog(, %RTsel, %RGv) failed with rc=%Rrc\n", Sel, GCPtr, rc);
    if (pszPrefix && *pszPrefix)
        RTLogPrintf("%s-CPU%u: %s\n", pszPrefix, pVCpu->idCpu, szBuf);
    else
        RTLogPrintf("%s\n", szBuf);
    return rc;
}

