/* $Id$ */
/** @file
 * tstOhciRegisterAccess - OHCI Register Access Tests / Experiments.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/string.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/param.h>

#include <VBox/sup.h>
#undef LogRel
#define LogRel(a) SUPR0Printf a


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Register names. */
static const char * const g_apszRegNms[] =
{
    "HcRevision",
    "HcControl",
    "HcCommandStatus",
    "HcInterruptStatus",
    "HcInterruptEnable",
    "HcInterruptDisable",
    "HcHCCA",
    "HcPeriodCurrentED",
    "HcControlHeadED",
    "HcControlCurrentED",
    "HcBulkHeadED",
    "HcBulkCurrentED",
    "HcDoneHead",
    "HcFmInterval",

    "HcFmRemaining",
    "HcFmNumber",
    "HcPeriodicStart",
    "HcLSThreshold",
    "HcRhDescriptorA",
    "HcRhDescriptorB",
    "HcRhStatus",
    /* Variable number of root hub ports: */
    "HcRhPortStatus[0]",
    "HcRhPortStatus[1]",
    "HcRhPortStatus[2]",
    "HcRhPortStatus[3]",
    "HcRhPortStatus[4]",
    "HcRhPortStatus[5]",
    "HcRhPortStatus[6]",
    "HcRhPortStatus[7]"
};

static bool TestOhciWrites(RTVPTRUNION uPtr)
{
    static struct
    {
        unsigned iReg;
        uint32_t uVal1;
        uint32_t uVal2;
    }  const s_aRegs[] =
    {
        { 13 /* HcFmInterval */, 0x58871120, 0 }
    };

    bool fSuccess = true;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aRegs); i++)
    {
        uint32_t const      iReg = s_aRegs[i].iReg;
        RTVPTRUNION         uPtrReg;
        uPtrReg.pu32 = &uPtr.pu32[iReg];

        uint32_t uInitialValue = *uPtrReg.pu32;
        LogRel(("TestOhciWrites: %p iReg=%2d %20s = %08RX32\n", uPtrReg.pv, iReg, g_apszRegNms[iReg], uInitialValue));

        bool                fDone    = true;
        const char         *pszError = NULL;
        RTCCUINTREG const   fFlags   = ASMIntDisableFlags();

        /*
         * DWORD writes.
         */
        uInitialValue = *uPtrReg.pu32;
        *uPtrReg.pu32 = uInitialValue;
        uint32_t u32A = *uPtrReg.pu32;
        uint32_t const uChangedValue = s_aRegs[i].uVal1 != uInitialValue ? s_aRegs[i].uVal1 : s_aRegs[i].uVal2;
        if (u32A == uInitialValue)
        {
            /* Change the value. */
            *uPtrReg.pu32 = uChangedValue;
            u32A = *uPtrReg.pu32;
            *uPtrReg.pu32 = uInitialValue;
            if (u32A != uChangedValue)
                pszError = "Writing changed value failed";
        }
        else
            pszError = "Writing back initial value failed";


        /*
         * Write byte changes.
         */
        if (!pszError)
        {

        }


        *uPtrReg.pu32 = uInitialValue;

        ASMSetFlags(fFlags);
        ASMNopPause();

        /*
         * Complain on failure.
         */
        if (!fDone)
            LogRel(("TestOhciWrites: Warning! Register %s was never stable enough for testing! %08RX32 %08RX32 %08RX32\n",
                    g_apszRegNms[iReg], uInitialValue, u32A, uChangedValue, uInitialValue));
        else if (pszError)
        {
            LogRel(("TestOhciWrites: Error! Register %s failed: %s; uInitialValue=%08RX32 uChangedValue=%08RX32 u32A=%08RX32\n",
                    g_apszRegNms[iReg], pszError, uInitialValue, uChangedValue, u32A));
            fSuccess = false;
        }
    }

    return fSuccess;
}


static bool TestOhciReads(RTVPTRUNION uPtr)
{
    /*
     * We can read just about any register we like since read shouldn't have
     * any side effects.  However, some registers are volatile and makes for
     * difficult targets, thus the ugly code.
     */
    bool        fSuccess = true;
    uint32_t    cMaxReg  = RT_ELEMENTS(g_apszRegNms);
    for (uint32_t iReg = 0; iReg < cMaxReg; iReg++, uPtr.pu32++)
    {
        const char *pszError      = NULL;
        bool        fDone         = false;
        uint32_t    uInitialValue = *uPtr.pu32;
        uint32_t    u32A          = 0;
        uint32_t    u32B          = 0;
        uint32_t    u32C          = 0;
        LogRel(("TestOhciReads: %p iReg=%2d %20s = %08RX32\n", uPtr.pv, iReg, g_apszRegNms[iReg], uInitialValue));

        for (uint32_t iTry = 0; !fDone && iTry < 1024; iTry++)
        {
            pszError = NULL;
            fDone    = true;
            u32A = u32B = u32C = 0;

            RTCCUINTREG const fFlags = ASMIntDisableFlags();
            uInitialValue = *uPtr.pu32;

            /* Test byte access. */
            for (unsigned iByte = 0; iByte < 4; iByte++)
            {
                u32A = *uPtr.pu32;
                u32B = uPtr.pu8[iByte];
                u32C = *uPtr.pu32;
                if (u32A != uInitialValue || u32C != uInitialValue)
                {
                    fDone = false;
                    break;
                }

                static uint32_t const a_au32Masks[] =
                {
                    UINT32_C(0xffffff00), UINT32_C(0xffff00ff), UINT32_C(0xff00ffff), UINT32_C(0x00ffffff)
                };
                u32B <<= iByte * 8;
                u32B |= uInitialValue & a_au32Masks[iByte];
                if (u32B != uInitialValue)
                {
                    static const char * const s_apsz[] = { "byte 0", "byte 1", "byte 2", "byte 3" };
                    pszError = s_apsz[iByte];
                    break;
                }
            }

            /* Test aligned word access. */
            if (fDone)
            {
                for (unsigned iWord = 0; iWord < 2; iWord++)
                {
                    u32A = *uPtr.pu32;
                    u32B = uPtr.pu16[iWord];
                    u32C = *uPtr.pu32;
                    if (u32A != uInitialValue || u32C != uInitialValue)
                    {
                        fDone = false;
                        break;
                    }

                    u32B <<= iWord * 16;
                    u32B |= uInitialValue & (iWord == 0 ? UINT32_C(0xffff0000) : UINT32_C(0x0000ffff));
                    if (u32B != uInitialValue)
                    {
                        pszError = iWord == 0 ? "aligned word 0 access" : "aligned word 1 access";
                        break;
                    }
                }
            }

            /* Test unaligned word access. */
            if (fDone)
            {
                for (int iWord = ((uintptr_t)uPtr.pv & PAGE_OFFSET_MASK) == 0; iWord < 3; iWord++)
                {
                    u32A = *uPtr.pu32;
                    u32B = *(volatile uint16_t *)&uPtr.pu8[iWord * 2 - 1];
                    u32C = *uPtr.pu32;
                    if (u32A != uInitialValue || u32C != uInitialValue)
                    {
                        fDone = false;
                        break;
                    }

                    switch (iWord)
                    {
                        case 0: u32B = (u32B >> 8)  | (u32A & UINT32_C(0xffffff00)); break;
                        case 1: u32B = (u32B << 8)  | (u32A & UINT32_C(0xff0000ff)); break;
                        case 2: u32B = (u32B << 24) | (u32A & UINT32_C(0x00ffffff)); break;
                    }
                    if (u32B != u32A)
                    {
                        static const char * const s_apsz[] = { "unaligned word 0", "unaligned word 1", "unaligned word 2" };
                        pszError = s_apsz[iWord];
                        break;
                    }
                }
            }

            /* Test unaligned dword access. */
            if (fDone)
            {
                for (int iByte = ((uintptr_t)uPtr.pv & PAGE_OFFSET_MASK) == 0 ? 0 : -3; iByte < 4; iByte++)
                {
                    u32A = *uPtr.pu32;
                    u32B = *(volatile uint32_t *)&uPtr.pu8[iByte];
                    u32C = *uPtr.pu32;
                    if (u32A != uInitialValue || u32C != uInitialValue)
                    {
                        fDone = false;
                        break;
                    }

                    switch (iByte)
                    {
                        case -3: u32B = (u32B >> 24) | (uInitialValue & UINT32_C(0xffffff00)); break;
                        case -2: u32B = (u32B >> 16) | (uInitialValue & UINT32_C(0xffff0000)); break;
                        case -1: u32B = (u32B >>  8) | (uInitialValue & UINT32_C(0xff000000)); break;
                        case  0: break;
                        case  1: u32B = (u32B <<  8) | (uInitialValue & UINT32_C(0x000000ff)); break;
                        case  2: u32B = (u32B << 16) | (uInitialValue & UINT32_C(0x0000ffff)); break;
                        case  3: u32B = (u32B << 24) | (uInitialValue & UINT32_C(0x00ffffff)); break;

                    }
                    if (u32B != u32A)
                    {
                        static const char * const s_apsz[] =
                        {
                            "unaligned dword -3", "unaligned dword -2",  "unaligned dword -1",
                            "unaligned dword 0", "unaligned dword 1",  "unaligned dword 2",  "unaligned dword 3"
                        };
                        pszError = s_apsz[iByte + 3];
                        break;
                    }

                }
            }

            ASMSetFlags(fFlags);
            ASMNopPause();
        } /* try loop */

        /*
         * Complain on failure.
         */
        if (!fDone)
            LogRel(("TestOhciReads: Warning! Register %s was never stable enough for testing! %08RX32 %08RX32 %08RX32\n",
                    g_apszRegNms[iReg], uInitialValue, u32A, u32C));
        else if (pszError)
        {
            LogRel(("TestOhciReads: Error! Register %s failed: %s; uInitialValue=%08RX32 u32B=%08RX32\n",
                    g_apszRegNms[iReg], pszError, uInitialValue, u32B));
            fSuccess = false;
        }
    }

    return fSuccess;
}


int tstOhciRegisterAccess(RTHCPHYS HCPhysOHCI)
{
    LogRel(("tstOhciRegisterAccess: HCPhysOHCI=%RHp\n", HCPhysOHCI));

    /*
     * Map the OHCI registers so we can access them.
     */
    RTR0MEMOBJ hMemObj;
    int rc = RTR0MemObjEnterPhys(&hMemObj, HCPhysOHCI, PAGE_SIZE, RTMEM_CACHE_POLICY_MMIO);
    if (RT_FAILURE(rc))
    {
        LogRel(("tstOhciRegisterAccess: Failed to enter OHCI memory at %RHp: %Rrc\n", HCPhysOHCI, rc));
        return rc;
    }
    RTR0MEMOBJ hMapObj;
    rc = RTR0MemObjMapKernel(&hMapObj, hMemObj, (void *)-1, 0 /*uAlignment*/, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
    if (RT_SUCCESS(rc))
    {
        RTVPTRUNION uPtr;
        uPtr.pv = (void volatile *)RTR0MemObjAddress(hMapObj);
        LogRel(("tstOhciRegisterAccess: mapping address %p\n", uPtr.pv));
        if (RT_VALID_PTR(uPtr.pv))
        {
            LogRel(("tstOhciRegisterAccess: HcRevision=%#x\n", *uPtr.pu32));

            /*
             * Do the access tests.
             */
            bool fSuccess = TestOhciReads(uPtr);
            if (fSuccess)
                fSuccess = TestOhciWrites(uPtr);
            if (fSuccess)
                LogRel(("tstOhciRegisterAccess: Success!\n"));
            else
                LogRel(("tstOhciRegisterAccess: Failed!\n"));
        }
        else
            rc = VERR_INTERNAL_ERROR_2;

        /*
         * Clean up.
         */
        RTR0MemObjFree(hMapObj, false);
    }
    else
        LogRel(("tstOhciRegisterAccess: Failed to map OHCI memory at %RHp: %Rrc\n", HCPhysOHCI, rc));
    RTR0MemObjFree(hMemObj, false);
    LogRel(("tstOhciRegisterAccess: returns %Rrc\n", rc));
    return rc;
}
