/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Disassembler.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
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
static DECLCALLBACK(int) dbgfR3DisasInstrRead(RTHCUINTPTR pSrc, uint8_t *pDest, uint32_t size, void *pvUserdata);


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
    pState->Cpu.mode        = pSelInfo->Raw.Gen.u1DefBig ? CPUMODE_32BIT : CPUMODE_16BIT;
    pState->Cpu.pfnReadBytes = dbgfR3DisasInstrRead;
    pState->GCPtrSegBase    = pSelInfo->GCPtrBase;
    pState->GCPtrSegEnd     = pSelInfo->cbLimit + 1 + (RTGCUINTPTR)pSelInfo->GCPtrBase;
    pState->cbSegLimit      = pSelInfo->cbLimit;
    pState->enmMode         = enmMode;
    pState->pvPageGC        = 0;
    pState->pvPageHC        = NULL;
    pState->pVM             = pVM;
    pState->fLocked         = false;
    Assert((uintptr_t)GCPtr == GCPtr);
    uint32_t cbInstr;
    int rc = DISInstr(&pState->Cpu, GCPtr, 0, &cbInstr, NULL);
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
static DECLCALLBACK(int) dbgfR3DisasInstrRead(RTHCUINTPTR PtrSrc, uint8_t *pu8Dst, uint32_t cbRead, void *pvDisCpu)
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
        if (PtrSrc > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /* calc how much we can read */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
        if (cb > cbSeg && cbSeg)
            cb = cbSeg;
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
 * Copy a string and return pointer to the terminator char in the copy.
 */
inline char *mystrpcpy(char *pszDst, const char *pszSrc)
{
    size_t cch = strlen(pszSrc);
    memcpy(pszDst, pszSrc, cch + 1);
    return pszDst + cch;
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
        GCPtr      = pCtxCore->eip;
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
        SelInfo.GCPtrBase           = pHiddenSel->u32Base;
        SelInfo.cbLimit             = pHiddenSel->u32Limit;
        SelInfo.fHyper              = false;
        SelInfo.fRealMode           = !!(pCtxCore && pCtxCore->eflags.Bits.u1VM || enmMode == PGMMODE_REAL);
        SelInfo.Raw.au32[0]         = 0;
        SelInfo.Raw.au32[1]         = 0;
        SelInfo.Raw.Gen.u16LimitLow = ~0;
        SelInfo.Raw.Gen.u4LimitHigh = ~0;
        SelInfo.Raw.Gen.u1Present   = pHiddenSel->Attr.n.u1Present;
        SelInfo.Raw.Gen.u1Granularity = pHiddenSel->Attr.n.u1Granularity;;
        SelInfo.Raw.Gen.u1DefBig    = pHiddenSel->Attr.n.u1DefBig;
        SelInfo.Raw.Gen.u1DescType  = pHiddenSel->Attr.n.u1DescType;
        SelInfo.Raw.Gen.u4Type      = pHiddenSel->Attr.n.u4Type;
        fRealModeAddress            = SelInfo.fRealMode;
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
    char *psz = &szBuf[0];

    /* prefix */
    if (State.Cpu.prefix & PREFIX_LOCK)
        psz = (char *)memcpy(psz, "lock ",   sizeof("lock "))   + sizeof("lock ") - 1;
    if (State.Cpu.prefix & PREFIX_REP)
        psz = (char *)memcpy(psz, "rep(e) ", sizeof("rep(e) ")) + sizeof("rep(e) ") - 1;
    else if(State.Cpu.prefix & PREFIX_REPNE)
        psz = (char *)memcpy(psz, "repne ",  sizeof("repne "))  + sizeof("repne ") - 1;

    /* the instruction */
    const char *pszFormat = State.Cpu.pszOpcode;
    char ch;
    while ((ch = *pszFormat) && !isspace(ch) && ch != '%')
    {
        *psz++ = ch;
        pszFormat++;
    }
    if (isspace(ch))
    {
        do *psz++ = ' ';
#ifdef DEBUG_bird /* Not sure if Sander want's this because of log size */
        while (psz - szBuf < 8);
#else
        while (0);
#endif
        while (isspace(*pszFormat))
            pszFormat++;
    }

    if (fFlags & DBGF_DISAS_FLAGS_NO_ANNOTATION)
        pCtxCore = NULL;

    /** @todo implement annotation and symbol lookup! */
    int         iParam = 1;
    for (;;)
    {
        ch = *pszFormat;
        if (ch == '%')
        {
            ch = pszFormat[1];
            switch (ch)
            {
                /*
                 * Relative jump offset.
                 */
                case 'J':
                {
                    AssertMsg(iParam == 1, ("Invalid branch parameter nr %d\n", iParam));
                    int32_t i32Disp;
                    if (State.Cpu.param1.flags & USE_IMMEDIATE8_REL)
                        i32Disp = (int32_t)(int8_t)State.Cpu.param1.parval;
                    else if (State.Cpu.param1.flags & USE_IMMEDIATE16_REL)
                        i32Disp = (int32_t)(int16_t)State.Cpu.param1.parval;
                    else if (State.Cpu.param1.flags & USE_IMMEDIATE32_REL)
                        i32Disp = (int32_t)State.Cpu.param1.parval;
                    else
                    {
                        AssertMsgFailed(("Oops!\n"));
                        dbgfR3DisasInstrDone(&State);
                        return VERR_GENERAL_FAILURE;
                    }
                    RTGCUINTPTR GCPtrTarget = (RTGCUINTPTR)GCPtr + State.Cpu.opsize + i32Disp;
                    switch (State.Cpu.opmode)
                    {
                        case CPUMODE_16BIT: GCPtrTarget &= UINT16_MAX; break;
                        case CPUMODE_32BIT: GCPtrTarget &= UINT32_MAX; break;
                    }
#ifdef DEBUG_bird   /* an experiment. */
                    DBGFSYMBOL  Sym;
                    RTGCINTPTR  off;
                    int rc = DBGFR3SymbolByAddr(pVM, GCPtrTarget + SelInfo.GCPtrBase, &off, &Sym);
                    if (    VBOX_SUCCESS(rc)
                        &&  Sym.Value - SelInfo.GCPtrBase <= SelInfo.cbLimit
                        &&  off < _1M * 16 && off > -_1M * 16)
                    {
                        psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz, "%s", Sym.szName);
                        if (off > 0)
                            psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz, "+%#x", (int)off);
                        else if (off > 0)
                            psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz, "-%#x", -(int)off);
                        switch (State.Cpu.opmode)
                        {
                            case CPUMODE_16BIT:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? " (%04VGv/+%x)" : " (%04VGv/-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                            case CPUMODE_32BIT:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? " (%08VGv/+%x)" : " (%08VGv/-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                            default:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? " (%VGv/+%x)"   : " (%VGv/-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                        }
                    }
                    else
#endif /* DEBUG_bird */
                    {
                        switch (State.Cpu.opmode)
                        {
                            case CPUMODE_16BIT:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? "%04VGv (+%x)" : "%04VGv (-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                            case CPUMODE_32BIT:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? "%08VGv (+%x)" : "%08VGv (-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                            default:
                                psz += RTStrPrintf(psz, &szBuf[sizeof(szBuf)] - psz,
                                                   i32Disp >= 0 ? "%VGv (+%x)"   : "%VGv (-%x)",
                                                   GCPtrTarget, i32Disp >= 0 ? i32Disp : -i32Disp);
                                break;
                        }
                    }
                    break;
                }

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
                    switch (iParam)
                    {
                        case 1: psz = mystrpcpy(psz, State.Cpu.param1.szParam); break;
                        case 2: psz = mystrpcpy(psz, State.Cpu.param2.szParam); break;
                        case 3: psz = mystrpcpy(psz, State.Cpu.param3.szParam); break;
                    }
                    pszFormat += 2;
                    break;

                case 'e': //register based on operand size (e.g. %eAX)
                    if (State.Cpu.opmode == CPUMODE_32BIT)
                        *psz++ = 'E';
                    *psz++ = pszFormat[2];
                    *psz++ = pszFormat[3];
                    pszFormat += 4;
                    break;

                default:
                    AssertMsgFailed(("Oops! ch=%c\n", ch));
                    break;
            }

            /* Skip to the next parameter in the format string. */
            pszFormat = strchr(pszFormat, ',');
            if (!pszFormat)
                break;
            pszFormat++;
            *psz++ = ch = ',';
            iParam++;
        }
        else
        {
            /* output char, but check for parameter separator first. */
            if (ch == ',')
                iParam++;
            *psz++ = ch;
            if (!ch)
                break;
            pszFormat++;
        }

#ifdef DEBUG_bird /* Not sure if Sander want's this because of log size */
        /* space after commas */
        if (ch == ',')
        {
            while (isspace(*pszFormat))
                pszFormat++;
            *psz++ = ' ';
        }
#endif
    } /* foreach char in pszFormat */
    *psz = '\0';

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
            RTStrPrintf(pszOutput, cchOutput, "%VGv  %s", GCPtr, szBuf);
        else
            RTStrPrintf(pszOutput, cchOutput, "%04x:%VGv  %s", Sel, GCPtr, szBuf);
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
            RTStrPrintf(pszOutput, cchOutput, "%VGv %.*Vhxs%*s %s",
                        GCPtr,
                        cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                        szBuf);
        else
            RTStrPrintf(pszOutput, cchOutput, "%04x:%VGv %.*Vhxs%*s %s",
                        Sel, GCPtr,
                        cbBits, pau8Bits, cbBits < 8 ? (8 - cbBits) * 3 : 0, "",
                        szBuf);

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

