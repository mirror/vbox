/* $Id$ */
/** @file
 * IOM - Input / Output Monitor, I/O port related APIs.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM_IOPORT
#include <VBox/vmm/iom.h>
#include <VBox/sup.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmdev.h>
#include "IOMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/err.h>

#include "IOMInline.h"


#ifdef VBOX_WITH_STATISTICS

/**
 * Register statistics for an I/O port entry.
 */
void iomR3IoPortRegStats(PVM pVM, PIOMIOPORTENTRYR3 pRegEntry)
{
    PIOMIOPORTSTATSENTRY pStats     = &pVM->iom.s.paIoPortStats[pRegEntry->idxStats];
    PCIOMIOPORTDESC      pExtDesc   = pRegEntry->paExtDescs;
    unsigned             uPort      = pRegEntry->uPort;
    unsigned const       uFirstPort = uPort;
    unsigned const       uEndPort   = uPort + pRegEntry->cPorts;

    /* Register a dummy statistics for the prefix. */
    char                 szName[80];
    size_t cchPrefix;
    if (uFirstPort < uEndPort - 1)
        cchPrefix = RTStrPrintf(szName, sizeof(szName), "/IOM/NewPorts/%04x-%04x", uFirstPort, uEndPort - 1);
    else
        cchPrefix = RTStrPrintf(szName, sizeof(szName), "/IOM/NewPorts/%04x", uPort);
    const char *pszDesc     = pRegEntry->pszDesc;
    char       *pszFreeDesc = NULL;
    if (pRegEntry->pDevIns && pRegEntry->pDevIns->iInstance > 0 && pszDesc)
        pszDesc = pszFreeDesc = RTStrAPrintf2("%u / %s", pRegEntry->pDevIns->iInstance, pszDesc);
    int rc = STAMR3Register(pVM, &pRegEntry->idxSelf, STAMTYPE_U16, STAMVISIBILITY_ALWAYS, szName,
                            STAMUNIT_NONE, pRegEntry->pszDesc);
    AssertRC(rc);
    RTStrFree(pszFreeDesc);

    /* Register stats for each port under it */
    do
    {
        size_t cchBaseNm;
        if (uFirstPort < uEndPort - 1)
            cchBaseNm = cchPrefix + RTStrPrintf(&szName[cchPrefix], sizeof(szName) - cchPrefix, "/%04x-", uPort);
        else
        {
            szName[cchPrefix] = '/';
            cchBaseNm = cchPrefix + 1;
        }

# define SET_NM_SUFFIX(a_sz) memcpy(&szName[cchBaseNm], a_sz, sizeof(a_sz));
        const char * const pszInDesc  = pExtDesc ? pExtDesc->pszIn  : NULL;
        const char * const pszOutDesc = pExtDesc ? pExtDesc->pszOut : NULL;

        /* register the statistics counters. */
        SET_NM_SUFFIX("In-R3");
        rc = STAMR3Register(pVM, &pStats->InR3,      STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszInDesc); AssertRC(rc);
        SET_NM_SUFFIX("Out-R3");
        rc = STAMR3Register(pVM, &pStats->OutR3,     STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszOutDesc); AssertRC(rc);
        SET_NM_SUFFIX("In-RZ");
        rc = STAMR3Register(pVM, &pStats->InRZ,      STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszInDesc); AssertRC(rc);
        SET_NM_SUFFIX("Out-RZ");
        rc = STAMR3Register(pVM, &pStats->OutRZ,     STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszOutDesc); AssertRC(rc);
        SET_NM_SUFFIX("In-RZtoR3");
        rc = STAMR3Register(pVM, &pStats->InRZToR3,  STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, NULL); AssertRC(rc);
        SET_NM_SUFFIX("Out-RZtoR3");
        rc = STAMR3Register(pVM, &pStats->OutRZToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, NULL); AssertRC(rc);

        /* Profiling */
        SET_NM_SUFFIX("In-R3-Prof");
        rc = STAMR3Register(pVM, &pStats->ProfInR3,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszInDesc); AssertRC(rc);
        SET_NM_SUFFIX("Out-R3-Prof");
        rc = STAMR3Register(pVM, &pStats->ProfOutR3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszOutDesc); AssertRC(rc);
        SET_NM_SUFFIX("In-RZ-Prof");
        rc = STAMR3Register(pVM, &pStats->ProfInRZ,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszInDesc); AssertRC(rc);
        SET_NM_SUFFIX("Out-RZ-Prof");
        rc = STAMR3Register(pVM, &pStats->ProfOutRZ, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszOutDesc); AssertRC(rc);

        pStats++;
        uPort++;
        if (pExtDesc)
            pExtDesc = pszInDesc || pszOutDesc ? pExtDesc + 1 : NULL;
    } while (uPort < uEndPort);
}


/**
 * Deregister statistics for an I/O port entry.
 */
static void iomR3IoPortDeregStats(PVM pVM, PIOMIOPORTENTRYR3 pRegEntry, unsigned uPort)
{
    char   szPrefix[80];
    size_t cchPrefix;
    if (pRegEntry->cPorts > 1)
        cchPrefix = RTStrPrintf(szPrefix, sizeof(szPrefix), "/IOM/NewPorts/%04x-%04x", uPort, uPort + pRegEntry->cPorts - 1);
    else
        cchPrefix = RTStrPrintf(szPrefix, sizeof(szPrefix), "/IOM/NewPorts/%04x", uPort);
    STAMR3DeregisterByPrefix(pVM->pUVM, szPrefix);
}

#endif /* VBOX_WITH_STATISTICS */


/**
 * @callback_method_impl{FNIOMIOPORTNEWIN,
 *      Dummy Port I/O Handler for IN operations.}
 */
static DECLCALLBACK(VBOXSTRICTRC)
iomR3IOPortDummyNewIn(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pDevIns); NOREF(pvUser); NOREF(Port);
    switch (cb)
    {
        case 1: *pu32 = 0xff; break;
        case 2: *pu32 = 0xffff; break;
        case 4: *pu32 = UINT32_C(0xffffffff); break;
        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb));
            return VERR_IOM_IOPORT_IPE_2;
    }
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWINSTRING,
 *      Dummy Port I/O Handler for string IN operations.}
 */
static DECLCALLBACK(VBOXSTRICTRC)
iomR3IOPortDummyNewInStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint8_t *pbDst, uint32_t *pcTransfer, unsigned cb)
{
    NOREF(pDevIns); NOREF(pvUser); NOREF(Port); NOREF(pbDst); NOREF(pcTransfer); NOREF(cb);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT,
 *      Dummy Port I/O Handler for OUT operations.}
 */
static DECLCALLBACK(VBOXSTRICTRC)
iomR3IOPortDummyNewOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pDevIns); NOREF(pvUser); NOREF(Port); NOREF(u32); NOREF(cb);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWOUTSTRING,
 *      Dummy Port I/O Handler for string OUT operations.}
 */
static DECLCALLBACK(VBOXSTRICTRC)
iomR3IOPortDummyNewOutStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint8_t const *pbSrc, uint32_t *pcTransfer, unsigned cb)
{
    NOREF(pDevIns); NOREF(pvUser); NOREF(Port); NOREF(pbSrc); NOREF(pcTransfer); NOREF(cb);
    return VINF_SUCCESS;
}



/**
 * Worker for PDMDEVHLPR3::pfnIoPortCreateEx.
 */
VMMR3_INT_DECL(int)  IOMR3IoPortCreate(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT cPorts, uint32_t fFlags, PPDMPCIDEV pPciDev,
                                       uint32_t iPciRegion, PFNIOMIOPORTNEWOUT pfnOut, PFNIOMIOPORTNEWIN pfnIn,
                                       PFNIOMIOPORTNEWOUTSTRING pfnOutStr, PFNIOMIOPORTNEWINSTRING pfnInStr, RTR3PTR pvUser,
                                       const char *pszDesc, PCIOMIOPORTDESC paExtDescs, PIOMIOPORTHANDLE phIoPorts)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(phIoPorts, VERR_INVALID_POINTER);
    *phIoPorts = UINT32_MAX;
    VM_ASSERT_EMT0_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    VM_ASSERT_STATE_RETURN(pVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);

    AssertMsgReturn(cPorts > 0 && cPorts <= _8K, ("cPorts=%#x\n", cPorts), VERR_OUT_OF_RANGE);
    AssertReturn(!(fFlags & ~IOM_IOPORT_F_VALID_MASK), VERR_INVALID_FLAGS);

    AssertReturn(pfnOut || pfnIn || pfnOutStr || pfnInStr, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfnOut, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnIn, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnOutStr, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnInStr, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc != '\0', VERR_INVALID_POINTER);
    AssertReturn(strlen(pszDesc) < 128, VERR_INVALID_POINTER);
    if (paExtDescs)
    {
        AssertPtrReturn(paExtDescs, VERR_INVALID_POINTER);
        for (size_t i = 0;; i++)
        {
            const char *pszIn  = paExtDescs[i].pszIn;
            const char *pszOut = paExtDescs[i].pszIn;
            if (!pszIn && !pszOut)
                break;
            AssertReturn(i < _8K, VERR_OUT_OF_RANGE);
            AssertReturn(!pszIn  || strlen(pszIn)  < 128, VERR_INVALID_POINTER);
            AssertReturn(!pszOut || strlen(pszOut) < 128, VERR_INVALID_POINTER);
        }
    }

    /*
     * Ensure that we've got table space for it.
     */
#ifndef VBOX_WITH_STATISTICS
    uint16_t const idxStats        = UINT16_MAX;
#else
    uint32_t const idxStats        = pVM->iom.s.cIoPortStats;
    uint32_t const cNewIoPortStats = idxStats + cPorts;
    AssertReturn(cNewIoPortStats <= _64K, VERR_IOM_TOO_MANY_IOPORT_REGISTRATIONS);
    if (cNewIoPortStats > pVM->iom.s.cIoPortStatsAllocation)
    {
        int rc = VMMR3CallR0Emt(pVM, pVM->apCpusR3[0], VMMR0_DO_IOM_GROW_IO_PORT_STATS, cNewIoPortStats, NULL);
        AssertLogRelRCReturn(rc, rc);
        AssertReturn(idxStats == pVM->iom.s.cIoPortStats, VERR_IOM_IOPORT_IPE_1);
        AssertReturn(cNewIoPortStats <= pVM->iom.s.cIoPortStatsAllocation, VERR_IOM_IOPORT_IPE_2);
    }
#endif

    uint32_t idx = pVM->iom.s.cIoPortRegs;
    if (idx >= pVM->iom.s.cIoPortAlloc)
    {
        int rc = VMMR3CallR0Emt(pVM, pVM->apCpusR3[0], VMMR0_DO_IOM_GROW_IO_PORTS, pVM->iom.s.cIoPortAlloc + 1, NULL);
        AssertLogRelRCReturn(rc, rc);
        AssertReturn(idx == pVM->iom.s.cIoPortRegs, VERR_IOM_IOPORT_IPE_1);
        AssertReturn(idx < pVM->iom.s.cIoPortAlloc, VERR_IOM_IOPORT_IPE_2);
    }

    /*
     * Enter it.
     */
    pVM->iom.s.paIoPortRegs[idx].pvUser             = pvUser;
    pVM->iom.s.paIoPortRegs[idx].pDevIns            = pDevIns;
    pVM->iom.s.paIoPortRegs[idx].pfnOutCallback     = pfnOut    ? pfnOut    : iomR3IOPortDummyNewOut;
    pVM->iom.s.paIoPortRegs[idx].pfnInCallback      = pfnIn     ? pfnIn     : iomR3IOPortDummyNewIn;
    pVM->iom.s.paIoPortRegs[idx].pfnOutStrCallback  = pfnOutStr ? pfnOutStr : iomR3IOPortDummyNewOutStr;
    pVM->iom.s.paIoPortRegs[idx].pfnInStrCallback   = pfnInStr  ? pfnInStr  : iomR3IOPortDummyNewInStr;
    pVM->iom.s.paIoPortRegs[idx].pszDesc            = pszDesc;
    pVM->iom.s.paIoPortRegs[idx].paExtDescs         = paExtDescs;
    pVM->iom.s.paIoPortRegs[idx].pPciDev            = pPciDev;
    pVM->iom.s.paIoPortRegs[idx].iPciRegion         = iPciRegion;
    pVM->iom.s.paIoPortRegs[idx].cPorts             = cPorts;
    pVM->iom.s.paIoPortRegs[idx].uPort              = UINT16_MAX;
    pVM->iom.s.paIoPortRegs[idx].idxStats           = (uint16_t)idxStats;
    pVM->iom.s.paIoPortRegs[idx].fMapped            = false;
    pVM->iom.s.paIoPortRegs[idx].fFlags             = (uint8_t)fFlags;
    pVM->iom.s.paIoPortRegs[idx].idxSelf            = idx;

    pVM->iom.s.cIoPortRegs = idx + 1;
#ifdef VBOX_WITH_STATISTICS
    pVM->iom.s.cIoPortStats = cNewIoPortStats;
#endif
    *phIoPorts = idx;
    return VINF_SUCCESS;
}


/**
 * Worker for PDMDEVHLPR3::pfnIoPortMap.
 */
VMMR3_INT_DECL(int)  IOMR3IoPortMap(PVM pVM, PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts, RTIOPORT uPort)
{
    /*
     * Validate input and state.
     */
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(hIoPorts < pVM->iom.s.cIoPortRegs, VERR_IOM_INVALID_IOPORT_HANDLE);
    PIOMIOPORTENTRYR3 const pRegEntry = &pVM->iom.s.paIoPortRegs[hIoPorts];
    AssertReturn(pRegEntry->pDevIns == pDevIns, VERR_IOM_INVALID_IOPORT_HANDLE);

    RTIOPORT const cPorts = pRegEntry->cPorts;
    AssertMsgReturn(cPorts > 0 && cPorts <= _8K, ("cPorts=%s\n", cPorts), VERR_IOM_IOPORT_IPE_1);
    AssertReturn((uint32_t)uPort + cPorts <= _64K, VERR_OUT_OF_RANGE);
    RTIOPORT const uLastPort = uPort + cPorts - 1;

    /*
     * Do the mapping.
     */
    int rc = VINF_SUCCESS;
    IOM_LOCK_EXCL(pVM);

    if (!pRegEntry->fMapped)
    {
        uint32_t const cEntries = RT_MIN(pVM->iom.s.cIoPortLookupEntries, pVM->iom.s.cIoPortRegs);
        Assert(pVM->iom.s.cIoPortLookupEntries == cEntries);

        PIOMIOPORTLOOKUPENTRY paEntries = pVM->iom.s.paIoPortLookup;
        PIOMIOPORTLOOKUPENTRY pEntry;
        if (cEntries > 0)
        {
            uint32_t iFirst = 0;
            uint32_t iEnd   = cEntries;
            uint32_t i      = cEntries / 2;
            for (;;)
            {
                pEntry = &paEntries[i];
                if (pEntry->uLastPort < uPort)
                {
                    i += 1;
                    if (i < iEnd)
                        iFirst = i;
                    else
                    {
                        /* Insert after the entry we just considered: */
                        pEntry += 1;
                        if (i < cEntries)
                            memmove(pEntry + 1, pEntry, sizeof(*pEntry) * (cEntries - i));
                        break;
                    }
                }
                else if (pEntry->uFirstPort > uLastPort)
                {
                    if (i > iFirst)
                        iEnd = i;
                    else
                    {
                        /* Insert at the entry we just considered: */
                        if (i < cEntries)
                            memmove(pEntry + 1, pEntry, sizeof(*pEntry) * (cEntries - i));
                        break;
                    }
                }
                else
                {
                    /* Oops! We've got a conflict. */
                    AssertLogRelMsgFailed(("%x..%x (%s) conflicts with existing mapping %x..%x (%s)\n",
                                           uPort, uLastPort, pRegEntry->pszDesc,
                                           pEntry->uFirstPort, pEntry->uLastPort, pVM->iom.s.paIoPortRegs[pEntry->idx].pszDesc));
                    IOM_UNLOCK_EXCL(pVM);
                    return VERR_IOM_IOPORT_RANGE_CONFLICT;
                }

                i = iFirst + (iEnd - iFirst) / 2;
            }
        }
        else
            pEntry = paEntries;

        /*
         * Fill in the entry and bump the table size.
         */
        pEntry->idx        = hIoPorts;
        pEntry->uFirstPort = uPort;
        pEntry->uLastPort  = uLastPort;
        pVM->iom.s.cIoPortLookupEntries = cEntries + 1;

        pRegEntry->uPort   = uPort;
        pRegEntry->fMapped = true;

#ifdef VBOX_WITH_STATISTICS
        /* Don't register stats here when we're creating the VM as the
           statistics table may still be reallocated. */
        if (pVM->enmVMState >= VMSTATE_CREATED)
            iomR3IoPortRegStats(pVM, pRegEntry);
#endif

#ifdef VBOX_STRICT
        /*
         * Assert table sanity.
         */
        AssertMsg(paEntries[0].uLastPort >= paEntries[0].uFirstPort, ("%#x %#x\n", paEntries[0].uLastPort, paEntries[0].uFirstPort));
        AssertMsg(paEntries[0].idx < pVM->iom.s.cIoPortRegs, ("%#x %#x\n", paEntries[0].idx, pVM->iom.s.cIoPortRegs));

        RTIOPORT uPortPrev = paEntries[0].uLastPort;
        for (size_t i = 1; i <= cEntries; i++)
        {
            AssertMsg(paEntries[i].uLastPort >= paEntries[i].uFirstPort, ("%u: %#x %#x\n", i, paEntries[i].uLastPort, paEntries[i].uFirstPort));
            AssertMsg(paEntries[i].idx < pVM->iom.s.cIoPortRegs, ("%u: %#x %#x\n", i, paEntries[i].idx, pVM->iom.s.cIoPortRegs));
            AssertMsg(uPortPrev < paEntries[i].uFirstPort, ("%u: %#x %#x\n", i, uPortPrev, paEntries[i].uFirstPort));
            uPortPrev = paEntries[i].uLastPort;
        }
#endif
    }
    else
    {
        AssertFailed();
        rc = VERR_IOM_IOPORTS_ALREADY_MAPPED;
    }

    IOM_UNLOCK_EXCL(pVM);
    return rc;
}


/**
 * Worker for PDMDEVHLPR3::pfnIoPortUnmap.
 */
VMMR3_INT_DECL(int)  IOMR3IoPortUnmap(PVM pVM, PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts)
{
    /*
     * Validate input and state.
     */
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(hIoPorts < pVM->iom.s.cIoPortRegs, VERR_IOM_INVALID_IOPORT_HANDLE);
    PIOMIOPORTENTRYR3 const pRegEntry = &pVM->iom.s.paIoPortRegs[hIoPorts];
    AssertReturn(pRegEntry->pDevIns == pDevIns, VERR_IOM_INVALID_IOPORT_HANDLE);

    /*
     * Do the mapping.
     */
    int rc;
    IOM_LOCK_EXCL(pVM);

    if (pRegEntry->fMapped)
    {
        RTIOPORT const uPort     = pRegEntry->uPort;
        RTIOPORT const uLastPort = uPort + pRegEntry->cPorts - 1;
        uint32_t const cEntries  = RT_MIN(pVM->iom.s.cIoPortLookupEntries, pVM->iom.s.cIoPortRegs);
        Assert(pVM->iom.s.cIoPortLookupEntries == cEntries);
        Assert(cEntries > 0);

        PIOMIOPORTLOOKUPENTRY paEntries = pVM->iom.s.paIoPortLookup;
        uint32_t iFirst = 0;
        uint32_t iEnd   = cEntries;
        uint32_t i      = cEntries / 2;
        for (;;)
        {
            PIOMIOPORTLOOKUPENTRY pEntry = &paEntries[i];
            if (pEntry->uLastPort < uPort)
            {
                i += 1;
                if (i < iEnd)
                    iFirst = i;
                else
                {
                    rc = VERR_IOM_IOPORT_IPE_1;
                    AssertLogRelMsgFailedBreak(("%x..%x (%s) not found!\n", uPort, uLastPort, pRegEntry->pszDesc));
                }
            }
            else if (pEntry->uFirstPort > uLastPort)
            {
                if (i > iFirst)
                    iEnd = i;
                else
                {
                    rc = VERR_IOM_IOPORT_IPE_1;
                    AssertLogRelMsgFailedBreak(("%x..%x (%s) not found!\n", uPort, uLastPort, pRegEntry->pszDesc));
                }
            }
            else if (pEntry->idx == hIoPorts)
            {
                Assert(pEntry->uFirstPort == uPort);
                Assert(pEntry->uLastPort == uLastPort);
#ifdef VBOX_WITH_STATISTICS
                iomR3IoPortDeregStats(pVM, pRegEntry, uPort);
#endif
                if (i + 1 < cEntries)
                    memmove(pEntry, pEntry + 1, sizeof(*pEntry) * (cEntries - i - 1));
                pVM->iom.s.cIoPortLookupEntries = cEntries - 1;
                pRegEntry->uPort   = UINT16_MAX;
                pRegEntry->fMapped = false;
                rc = VINF_SUCCESS;
                break;
            }
            else
            {
                AssertLogRelMsgFailed(("Lookig for %x..%x (%s), found %x..%x (%s) instead!\n",
                                       uPort, uLastPort, pRegEntry->pszDesc,
                                       pEntry->uFirstPort, pEntry->uLastPort, pVM->iom.s.paIoPortRegs[pEntry->idx].pszDesc));
                rc = VERR_IOM_IOPORT_IPE_1;
                break;
            }

            i = iFirst + (iEnd - iFirst) / 2;
        }

#ifdef VBOX_STRICT
        /*
         * Assert table sanity.
         */
        AssertMsg(paEntries[0].uLastPort >= paEntries[0].uFirstPort, ("%#x %#x\n", paEntries[0].uLastPort, paEntries[0].uFirstPort));
        AssertMsg(paEntries[0].idx < pVM->iom.s.cIoPortRegs, ("%#x %#x\n", paEntries[0].idx, pVM->iom.s.cIoPortRegs));

        RTIOPORT uPortPrev = paEntries[0].uLastPort;
        for (i = 1; i < cEntries - 1; i++)
        {
            AssertMsg(paEntries[i].uLastPort >= paEntries[i].uFirstPort, ("%u: %#x %#x\n", i, paEntries[i].uLastPort, paEntries[i].uFirstPort));
            AssertMsg(paEntries[i].idx < pVM->iom.s.cIoPortRegs, ("%u: %#x %#x\n", i, paEntries[i].idx, pVM->iom.s.cIoPortRegs));
            AssertMsg(uPortPrev < paEntries[i].uFirstPort, ("%u: %#x %#x\n", i, uPortPrev, paEntries[i].uFirstPort));
            uPortPrev = paEntries[i].uLastPort;
        }
#endif
    }
    else
    {
        AssertFailed();
        rc = VERR_IOM_IOPORTS_NOT_MAPPED;
    }

    IOM_UNLOCK_EXCL(pVM);
    return rc;
}


/**
 * Validates @a hIoPorts, making sure it belongs to @a pDevIns.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pDevIns     The device which allegedly owns @a hIoPorts.
 * @param   hIoPorts    The handle to validate.
 */
VMMR3_INT_DECL(int)  IOMR3IoPortValidateHandle(PVM pVM, PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts)
{
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(hIoPorts < RT_MIN(pVM->iom.s.cIoPortRegs, pVM->iom.s.cIoPortAlloc), VERR_IOM_INVALID_IOPORT_HANDLE);
    PIOMIOPORTENTRYR3 const pRegEntry = &pVM->iom.s.paIoPortRegs[hIoPorts];
    AssertReturn(pRegEntry->pDevIns == pDevIns, VERR_IOM_INVALID_IOPORT_HANDLE);
    return VINF_SUCCESS;
}


/**
 * Gets the mapping address of I/O ports @a hIoPorts.
 *
 * @returns Mapping address if mapped, UINT32_MAX if not mapped or invalid
 *          input.
 * @param   pVM         The cross context VM structure.
 * @param   pDevIns     The device which allegedly owns @a hRegion.
 * @param   hIoPorts    The handle to I/O port region.
 */
VMMR3_INT_DECL(uint32_t) IOMR3IoPortGetMappingAddress(PVM pVM, PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts)
{
    AssertPtrReturn(pDevIns, UINT32_MAX);
    AssertReturn(hIoPorts < RT_MIN(pVM->iom.s.cMmioRegs, pVM->iom.s.cMmioAlloc), UINT32_MAX);
    IOMIOPORTENTRYR3 volatile * const pRegEntry = &pVM->iom.s.paIoPortRegs[hIoPorts];
    AssertReturn(pRegEntry->pDevIns == pDevIns, UINT32_MAX);
    for (uint32_t iTry = 0; ; iTry++)
    {
        bool        fMapped = pRegEntry->fMapped;
        RTIOPORT    uPort   = pRegEntry->uPort;
        if (   (   ASMAtomicReadBool(&pRegEntry->fMapped) == fMapped
                && uPort == pRegEntry->uPort)
            || iTry > 1024)
            return fMapped ? uPort : UINT32_MAX;
        ASMNopPause();
    }
}


/**
 * Display a single I/O port ring-3 range.
 *
 * @returns 0
 * @param   pNode   Pointer to I/O port HC range.
 * @param   pvUser  Pointer to info output callback structure.
 */
static DECLCALLBACK(int) iomR3IOPortInfoOneR3(PAVLROIOPORTNODECORE pNode, void *pvUser)
{
    PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)pNode;
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp,
                    "%04x-%04x %p %p %p %p %s\n",
                    pRange->Core.Key,
                    pRange->Core.KeyLast,
                    pRange->pDevIns,
                    pRange->pfnInCallback,
                    pRange->pfnOutCallback,
                    pRange->pvUser,
                    pRange->pszDesc);
    return 0;
}


/**
 * Display all registered I/O port ranges.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
DECLCALLBACK(void) iomR3IoPortInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /* No locking needed here as registerations are only happening during VMSTATE_CREATING. */
    pHlp->pfnPrintf(pHlp,
                    "I/O port registrations: %u (%u allocated)\n"
                    " ## Ctx    Ports Mapping   PCI    Description\n",
                    pVM->iom.s.cIoPortRegs, pVM->iom.s.cIoPortAlloc);
    PIOMIOPORTENTRYR3 paRegs = pVM->iom.s.paIoPortRegs;
    for (uint32_t i = 0; i < pVM->iom.s.cIoPortRegs; i++)
    {
        const char * const pszRing = paRegs[i].fRing0 ? paRegs[i].fRawMode ? "+0+C" : "+0  "
                                   : paRegs[i].fRawMode ? "+C  " : "    ";
        if (paRegs[i].fMapped && paRegs[i].pPciDev)
            pHlp->pfnPrintf(pHlp, "%3u R3%s %04x  %04x-%04x pci%u/%u %s\n", paRegs[i].idxSelf, pszRing, paRegs[i].cPorts,
                            paRegs[i].uPort, paRegs[i].uPort + paRegs[i].cPorts - 1,
                            paRegs[i].pPciDev->idxSubDev, paRegs[i].iPciRegion, paRegs[i].pszDesc);
        else if (paRegs[i].fMapped && !paRegs[i].pPciDev)
            pHlp->pfnPrintf(pHlp, "%3u R3%s %04x  %04x-%04x        %s\n", paRegs[i].idxSelf, pszRing, paRegs[i].cPorts,
                            paRegs[i].uPort, paRegs[i].uPort + paRegs[i].cPorts - 1, paRegs[i].pszDesc);
        else if (paRegs[i].pPciDev)
            pHlp->pfnPrintf(pHlp, "%3u R3%s %04x  unmapped  pci%u/%u %s\n", paRegs[i].idxSelf, pszRing, paRegs[i].cPorts,
                            paRegs[i].pPciDev->idxSubDev, paRegs[i].iPciRegion, paRegs[i].pszDesc);
        else
            pHlp->pfnPrintf(pHlp, "%3u R3%s %04x  unmapped         %s\n",
                            paRegs[i].idxSelf, pszRing, paRegs[i].cPorts, paRegs[i].pszDesc);
    }

    /* Legacy registration: */
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "I/O Port R3 ranges (pVM=%p)\n"
                    "Range     %.*s %.*s %.*s %.*s Description\n",
                    pVM,
                    sizeof(RTHCPTR) * 2,      "pDevIns         ",
                    sizeof(RTHCPTR) * 2,      "In              ",
                    sizeof(RTHCPTR) * 2,      "Out             ",
                    sizeof(RTHCPTR) * 2,      "pvUser          ");
    IOM_LOCK_SHARED(pVM);
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesR3->IOPortTreeR3, true, iomR3IOPortInfoOneR3, (void *)pHlp);
    IOM_UNLOCK_SHARED(pVM);

    pHlp->pfnPrintf(pHlp,
                    "I/O Port R0 ranges (pVM=%p)\n"
                    "Range     %.*s %.*s %.*s %.*s Description\n",
                    pVM,
                    sizeof(RTHCPTR) * 2,      "pDevIns         ",
                    sizeof(RTHCPTR) * 2,      "In              ",
                    sizeof(RTHCPTR) * 2,      "Out             ",
                    sizeof(RTHCPTR) * 2,      "pvUser          ");
    IOM_LOCK_SHARED(pVM);
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesR3->IOPortTreeR0, true, iomR3IOPortInfoOneR3, (void *)pHlp);
    IOM_UNLOCK_SHARED(pVM);
}

