/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Tracer Interface.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/VBoxTpG.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Data for a tracepoint provider.
 */
typedef struct SUPDRVTPPROVIDER
{
    /** The entry in the provider list for this image. */
    RTLISTNODE              ListEntry;

    /** The core structure. */
    SUPDRVVDTPROVIDERCORE   Core;

    /** Pointer to the image this provider resides in.  NULL if it's a
     * driver. */
    PSUPDRVLDRIMAGE         pImage;
    /** The session this provider is associated with if registered via
     * SUPR0VtgRegisterDrv.  NULL if pImage is set. */
    PSUPDRVSESSION          pSession;

    /** Set when the module is unloaded or the driver deregisters its probes. */
    bool                    fZombie;
    /** Set if the provider has been successfully registered with the
     *  tracer. */
    bool                    fRegistered;
    /** The provider name (for logging purposes). */
    char                    szName[1];
} SUPDRVTPPROVIDER;
/** Pointer to the data for a tracepoint provider. */
typedef SUPDRVTPPROVIDER *PSUPDRVTPPROVIDER;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if 0
# define LOG_TRACER(a_Args)  SUPR0Printf a_Args
#else
# define LOG_TRACER(a_Args)  do { } while (0)
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The address of the current probe fire routine for kernel mode. */
/*PFNRT       g_pfnSupdrvProbeFireKernel = supdrvTracerProbeFireStub;*/



/**
 * Validates a VTG string against length and characterset limitations.
 *
 * @returns VINF_SUCCESS, VERR_SUPDRV_VTG_BAD_STRING or
 *          VERR_SUPDRV_VTG_STRING_TOO_LONG.
 * @param   psz                 The string.
 */
static int supdrvVtgValidateString(const char *psz)
{
    size_t off = 0;
    while (off < _4K)
    {
        char const ch = psz[off++];
        if (!ch)
            return VINF_SUCCESS;
        if (   !RTLocCIsAlNum(ch)
            && ch != ' '
            && ch != '_'
            && ch != '-'
            && ch != '('
            && ch != ')'
            && ch != ','
            && ch != '*'
            && ch != '&'
           )
        {
            /*RTAssertMsg2("off=%u '%s'\n",  off, psz);*/
            return VERR_SUPDRV_VTG_BAD_STRING;
        }
    }
    return VERR_SUPDRV_VTG_STRING_TOO_LONG;
}


/**
 * Validates the VTG data.
 *
 * @returns VBox status code.
 * @param   pVtgHdr             The VTG object header of the data to validate.
 * @param   cbVtgObj            The size of the VTG object.
 * @param   pbImage             The image base. For validating the probe
 *                              locations.
 * @param   cbImage             The image size to go with @a pbImage.
 */
static int supdrvVtgValidate(PVTGOBJHDR pVtgHdr, size_t cbVtgObj, const uint8_t *pbImage, size_t cbImage)
{
    uintptr_t   cbTmp;
    uintptr_t   offTmp;
    uintptr_t   i;
    int         rc;
    uint32_t    cProviders;

    if (!pbImage || !cbImage)
    {
        pbImage = NULL;
        cbImage = 0;
    }

#define MY_VALIDATE_PTR(p, cb, cMin, cMax, cbUnit, rcBase) \
    do { \
        if (   (cb) >= cbVtgObj \
            || (uintptr_t)(p) - (uintptr_t)pVtgHdr > cbVtgObj - (cb) ) \
            return rcBase ## _PTR; \
        if ((cb) <  (cMin) * (cbUnit)) \
            return rcBase ## _TOO_FEW; \
        if ((cb) >= (cMax) * (cbUnit)) \
            return rcBase ## _TOO_MUCH; \
        if ((cb) / (cbUnit) * (cbUnit) != (cb)) \
            return rcBase ## _NOT_MULTIPLE; \
    } while (0)
#define MY_WITHIN_IMAGE(p, rc) \
    do { \
        if (pbImage) \
        { \
            if ((uintptr_t)(p) - (uintptr_t)pbImage > cbImage) \
                return (rc); \
        } \
        else if (!RT_VALID_PTR(p)) \
            return (rc); \
    } while (0)
#define MY_VALIDATE_STR(offStrTab) \
    do { \
        if ((offStrTab) >= pVtgHdr->cbStrTab) \
            return VERR_SUPDRV_VTG_STRTAB_OFF; \
        rc = supdrvVtgValidateString(pVtgHdr->pachStrTab + (offStrTab)); \
        if (rc != VINF_SUCCESS) \
            return rc; \
    } while (0)
#define MY_VALIDATE_ATTR(Attr) \
    do { \
        if ((Attr).u8Code    <= (uint8_t)kVTGStability_Invalid || (Attr).u8Code    >= (uint8_t)kVTGStability_End) \
            return VERR_SUPDRV_VTG_BAD_ATTR; \
        if ((Attr).u8Data    <= (uint8_t)kVTGStability_Invalid || (Attr).u8Data    >= (uint8_t)kVTGStability_End) \
            return VERR_SUPDRV_VTG_BAD_ATTR; \
        if ((Attr).u8DataDep <= (uint8_t)kVTGClass_Invalid     || (Attr).u8DataDep >= (uint8_t)kVTGClass_End) \
            return VERR_SUPDRV_VTG_BAD_ATTR; \
    } while (0)

    /*
     * The header.
     */
    if (memcmp(pVtgHdr->szMagic, VTGOBJHDR_MAGIC, sizeof(pVtgHdr->szMagic)))
        return VERR_SUPDRV_VTG_MAGIC;
    if (pVtgHdr->cBits != ARCH_BITS)
        return VERR_SUPDRV_VTG_BITS;
    if (pVtgHdr->u32Reserved0)
        return VERR_SUPDRV_VTG_BAD_HDR;

    MY_VALIDATE_PTR(pVtgHdr->paProviders,       pVtgHdr->cbProviders,    1,    16, sizeof(VTGDESCPROVIDER), VERR_SUPDRV_VTG_BAD_HDR);
    MY_VALIDATE_PTR(pVtgHdr->paProbes,          pVtgHdr->cbProbes,       1,  _32K, sizeof(VTGDESCPROBE),    VERR_SUPDRV_VTG_BAD_HDR);
    MY_VALIDATE_PTR(pVtgHdr->pafProbeEnabled,   pVtgHdr->cbProbeEnabled, 1,  _32K, sizeof(bool),            VERR_SUPDRV_VTG_BAD_HDR);
    MY_VALIDATE_PTR(pVtgHdr->pachStrTab,        pVtgHdr->cbStrTab,       4,   _1M, sizeof(char),            VERR_SUPDRV_VTG_BAD_HDR);
    MY_VALIDATE_PTR(pVtgHdr->paArgLists,        pVtgHdr->cbArgLists,     1,  _32K, sizeof(uint32_t),        VERR_SUPDRV_VTG_BAD_HDR);

    MY_WITHIN_IMAGE(pVtgHdr->paProbLocs,    VERR_SUPDRV_VTG_BAD_HDR_PTR);
    MY_WITHIN_IMAGE(pVtgHdr->paProbLocsEnd, VERR_SUPDRV_VTG_BAD_HDR_PTR);
    if ((uintptr_t)pVtgHdr->paProbLocs > (uintptr_t)pVtgHdr->paProbLocsEnd)
        return VERR_SUPDRV_VTG_BAD_HDR_PTR;
    cbTmp = (uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs;
    if (cbTmp < sizeof(VTGPROBELOC))
        return VERR_SUPDRV_VTG_BAD_HDR_TOO_FEW;
    if (cbTmp >= _128K * sizeof(VTGPROBELOC))
        return VERR_SUPDRV_VTG_BAD_HDR_TOO_MUCH;
    if (cbTmp / sizeof(VTGPROBELOC) * sizeof(VTGPROBELOC) != cbTmp)
        return VERR_SUPDRV_VTG_BAD_HDR_NOT_MULTIPLE;

    if (pVtgHdr->cbProbes / sizeof(VTGDESCPROBE) != pVtgHdr->cbProbeEnabled)
        return VERR_SUPDRV_VTG_BAD_HDR;

    /*
     * Validate the providers.
     */
    cProviders = i = pVtgHdr->cbProviders / sizeof(VTGDESCPROVIDER);
    while (i-- > 0)
    {
        MY_VALIDATE_STR(pVtgHdr->paProviders[i].offName);
        if (pVtgHdr->paProviders[i].iFirstProbe >= pVtgHdr->cbProbeEnabled)
            return VERR_SUPDRV_VTG_BAD_PROVIDER;
        if (pVtgHdr->paProviders[i].iFirstProbe + pVtgHdr->paProviders[i].cProbes > pVtgHdr->cbProbeEnabled)
            return VERR_SUPDRV_VTG_BAD_PROVIDER;
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrSelf);
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrModules);
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrFunctions);
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrNames);
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrArguments);
        if (pVtgHdr->paProviders[i].bReserved)
            return VERR_SUPDRV_VTG_BAD_PROVIDER;
    }

    /*
     * Validate probes.
     */
    i = pVtgHdr->cbProbes / sizeof(VTGDESCPROBE);
    while (i-- > 0)
    {
        PVTGDESCARGLIST pArgList;
        unsigned        iArg;

        MY_VALIDATE_STR(pVtgHdr->paProbes[i].offName);
        if (pVtgHdr->paProbes[i].offArgList >= pVtgHdr->cbArgLists)
            return VERR_SUPDRV_VTG_BAD_PROBE;
        if (pVtgHdr->paProbes[i].offArgList & 3)
            return VERR_SUPDRV_VTG_BAD_PROBE;
        if (pVtgHdr->paProbes[i].idxEnabled != i) /* The lists are parallel. */
            return VERR_SUPDRV_VTG_BAD_PROBE;
        if (pVtgHdr->paProbes[i].idxProvider >= cProviders)
            return VERR_SUPDRV_VTG_BAD_PROBE;
        if (  i - pVtgHdr->paProviders[pVtgHdr->paProbes[i].idxProvider].iFirstProbe
            >= pVtgHdr->paProviders[pVtgHdr->paProbes[i].idxProvider].cProbes)
            return VERR_SUPDRV_VTG_BAD_PROBE;
        if (pVtgHdr->paProbes[i].u32User)
            return VERR_SUPDRV_VTG_BAD_PROBE;

        /* The referenced argument list. */
        pArgList = (PVTGDESCARGLIST)((uintptr_t)pVtgHdr->paArgLists + pVtgHdr->paProbes[i].offArgList);
        if (pArgList->cArgs > 16)
            return VERR_SUPDRV_VTG_BAD_ARGLIST;
        if (   pArgList->abReserved[0]
            || pArgList->abReserved[1]
            || pArgList->abReserved[2])
            return VERR_SUPDRV_VTG_BAD_ARGLIST;
        iArg = pArgList->cArgs;
        while (iArg-- > 0)
        {
            MY_VALIDATE_STR(pArgList->aArgs[iArg].offType);
            MY_VALIDATE_STR(pArgList->aArgs[iArg].offName);
        }
    }

    /*
     * Check that pafProbeEnabled is all zero.
     */
    i = pVtgHdr->cbProbeEnabled;
    while (i-- > 0)
        if (pVtgHdr->pafProbeEnabled[0])
            return VERR_SUPDRV_VTG_BAD_PROBE_ENABLED;

    /*
     * Probe locations.
     */
    i = pVtgHdr->paProbLocsEnd - pVtgHdr->paProbLocs;
    while (i-- > 0)
    {
        if (pVtgHdr->paProbLocs[i].uLine >= _1G)
            return VERR_SUPDRV_VTG_BAD_PROBE_LOC;
        if (pVtgHdr->paProbLocs[i].fEnabled)
            return VERR_SUPDRV_VTG_BAD_PROBE_LOC;
        if (pVtgHdr->paProbLocs[i].idProbe != UINT32_MAX)
            return VERR_SUPDRV_VTG_BAD_PROBE_LOC;
        MY_WITHIN_IMAGE(pVtgHdr->paProbLocs[i].pszFunction, VERR_SUPDRV_VTG_BAD_PROBE_LOC);
        MY_WITHIN_IMAGE(pVtgHdr->paProbLocs[i].pszFile,     VERR_SUPDRV_VTG_BAD_PROBE_LOC);
        offTmp = (uintptr_t)pVtgHdr->paProbLocs[i].pbProbe - (uintptr_t)pVtgHdr->paProbes;
        if (offTmp >= pVtgHdr->cbProbes)
            return VERR_SUPDRV_VTG_BAD_PROBE_LOC;
        if (offTmp / sizeof(VTGDESCPROBE) * sizeof(VTGDESCPROBE) != offTmp)
            return VERR_SUPDRV_VTG_BAD_PROBE_LOC;
    }

    return VINF_SUCCESS;
#undef MY_VALIDATE_STR
#undef MY_VALIDATE_PTR
#undef MY_WITHIN_IMAGE
}


/**
 * Gets a string from the string table.
 *
 * @returns Pointer to the string.
 * @param   pVtgHdr             The VTG object header.
 * @param   offStrTab           The string table offset.
 */
static const char *supdrvVtgGetString(PVTGOBJHDR pVtgHdr,  uint32_t offStrTab)
{
    Assert(offStrTab < pVtgHdr->cbStrTab);
    return &pVtgHdr->pachStrTab[offStrTab];
}


/**
 * Frees the provider structure and associated resources.
 *
 * @param   pProv               The provider to free.
 */
static void supdrvTracerFreeProvider(PSUPDRVTPPROVIDER pProv)
{
    LOG_TRACER(("Freeing DTrace provider '%s' / %p\n", pProv->szName, pProv->Core.TracerData.DTrace.idProvider));
    pProv->fRegistered = false;
    pProv->fZombie     = true;
    pProv->Core.pDesc  = NULL;
    pProv->Core.pHdr   = NULL;
    RT_ZERO(pProv->Core.TracerData);
    RTMemFree(pProv);
}


/**
 * Deregisters a provider.
 *
 * If the provider is still busy, it will be put in the zombie list.
 *
 * @param   pDevExt             The device extension.
 * @param   pProv               The provider.
 *
 * @remarks The caller owns mtxTracer.
 */
static void supdrvTracerDeregisterVtgObj(PSUPDRVDEVEXT pDevExt, PSUPDRVTPPROVIDER pProv)
{
    int rc;
    if (!pProv->fRegistered || !pDevExt->pTracerOps)
        rc = VINF_SUCCESS;
    else
        rc = pDevExt->pTracerOps->pfnProviderDeregister(pDevExt->pTracerOps, &pProv->Core);
    if (RT_SUCCESS(rc))
    {
        supdrvTracerFreeProvider(pProv);
        return;
    }

    pProv->fZombie = true;
    RTListAppend(&pDevExt->TracerProviderZombieList, &pProv->ListEntry);
    LOG_TRACER(("Invalidated provider '%s' / %p and put it on the zombie list (rc=%Rrc)\n",
                pProv->szName, pProv->Core.TracerData.DTrace.idProvider, rc));
}


/**
 * Processes the zombie list.
 *
 * @param   pDevExt             The device extension.
 */
static void supdrvTracerProcessZombies(PSUPDRVDEVEXT pDevExt)
{
    PSUPDRVTPPROVIDER pProv, pProvNext;

    RTSemFastMutexRequest(pDevExt->mtxTracer);
    RTListForEachSafe(&pDevExt->TracerProviderZombieList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
    {
        int rc = pDevExt->pTracerOps->pfnProviderDeregisterZombie(pDevExt->pTracerOps, &pProv->Core);
        if (RT_SUCCESS(rc))
        {
            RTListNodeRemove(&pProv->ListEntry);
            supdrvTracerFreeProvider(pProv);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxTracer);
}


/**
 * Unregisters all providers, including zombies, waiting for busy providers to
 * go idle and unregister smoothly.
 *
 * This may block.
 *
 * @param   pDevExt             The device extension.
 */
static void supdrvTracerRemoveAllProviders(PSUPDRVDEVEXT pDevExt)
{
    uint32_t            i;
    PSUPDRVTPPROVIDER   pProv;
    PSUPDRVTPPROVIDER   pProvNext;

    /*
     * Unregister all probes (there should only be one).
     */
    RTSemFastMutexRequest(pDevExt->mtxTracer);
    RTListForEachSafe(&pDevExt->TracerProviderList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
    {
        RTListNodeRemove(&pProv->ListEntry);
        supdrvTracerDeregisterVtgObj(pDevExt, pProv);
    }
    RTSemFastMutexRelease(pDevExt->mtxTracer);

    /*
     * Try unregister zombies now, sleep on busy ones.
     */
    for (i = 0; ; i++)
    {
        bool fEmpty;

        RTSemFastMutexRequest(pDevExt->mtxTracer);
        RTListForEachSafe(&pDevExt->TracerProviderZombieList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
        {
            int rc;
            LOG_TRACER(("supdrvTracerRemoveAllProviders: Attemting to unregister '%s' / %p...\n",
                        pProv->szName, pProv->Core.TracerData.DTrace.idProvider));

            if (pDevExt->pTracerOps)
                rc = pDevExt->pTracerOps->pfnProviderDeregisterZombie(pDevExt->pTracerOps, &pProv->Core);
            else
                rc = VINF_SUCCESS;
            if (!rc)
            {
                RTListNodeRemove(&pProv->ListEntry);
                supdrvTracerFreeProvider(pProv);
            }
            else if (!(i & 0xf))
                SUPR0Printf("supdrvTracerRemoveAllProviders: Waiting on busy provider '%s' / %p (rc=%d)\n",
                            pProv->szName, pProv->Core.TracerData.DTrace.idProvider, rc);
            else
                LOG_TRACER(("supdrvTracerRemoveAllProviders: Failed to unregister provider '%s' / %p - rc=%d\n",
                            pProv->szName, pProv->Core.TracerData.DTrace.idProvider, rc));
        }

        fEmpty = RTListIsEmpty(&pDevExt->TracerProviderZombieList);
        RTSemFastMutexRelease(pDevExt->mtxTracer);
        if (fEmpty)
            break;

        /* Delay...*/
        RTThreadSleep(1000);
    }
}


/**
 * Registers the VTG tracepoint providers of a driver.
 *
 * @returns VBox status code.
 * @param   pszName             The driver name.
 * @param   pVtgHdr             The VTG object header.
 * @param   pVtgObj             The size of the VTG object.
 * @param   pImage              The image if applicable.
 * @param   pSession            The session if applicable.
 * @param   pszModName          The module name.
 */
static int supdrvTracerRegisterVtgObj(PSUPDRVDEVEXT pDevExt, PVTGOBJHDR pVtgHdr, size_t cbVtgObj, PSUPDRVLDRIMAGE pImage,
                                      PSUPDRVSESSION pSession, const char *pszModName)
{
    int                 rc;
    unsigned            i;
    PSUPDRVTPPROVIDER   pProv;

    /*
     * Validate input.
     */
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertPtrReturn(pVtgHdr, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pImage, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pszModName, VERR_INVALID_POINTER);

    if (pImage)
        rc = supdrvVtgValidate(pVtgHdr, cbVtgObj, (const uint8_t *)pImage->pvImage, pImage->cbImageBits);
    else
        rc = supdrvVtgValidate(pVtgHdr, cbVtgObj, NULL, 0);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTSemFastMutexRequest(pDevExt->mtxTracer);
    if (RT_FAILURE(rc))
        return rc;
    RTListForEach(&pDevExt->TracerProviderList, pProv, SUPDRVTPPROVIDER, ListEntry)
    {
        if (pProv->Core.pHdr == pVtgHdr)
        {
            rc = VERR_SUPDRV_VTG_ALREADY_REGISTERED;
            break;
        }
        if (   pProv->pSession == pSession
            && pProv->pImage   == pImage)
        {
            rc = VERR_SUPDRV_VTG_ONLY_ONCE_PER_SESSION;
            break;
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxTracer);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the providers.
     */
    i = pVtgHdr->cbProviders / sizeof(VTGDESCPROVIDER);
    while (i-- > 0)
    {
        PVTGDESCPROVIDER pDesc   = &pVtgHdr->paProviders[i];
        const char      *pszName = supdrvVtgGetString(pVtgHdr, pDesc->offName);
        size_t const     cchName = strlen(pszName);
        pProv = (PSUPDRVTPPROVIDER)RTMemAllocZ(RT_OFFSETOF(SUPDRVTPPROVIDER, szName[cchName + 1]));
        if (pProv)
        {
            pProv->Core.pDesc       = pDesc;
            pProv->Core.pHdr        = pVtgHdr;
            pProv->Core.pszName     = &pProv->szName[0];
            pProv->Core.pszModName  = pszModName;
            pProv->pImage           = pImage;
            pProv->pSession         = pSession;
            pProv->fZombie          = false;
            pProv->fRegistered      = true;
            memcpy(&pProv->szName[0], pszName, cchName + 1);

            rc = RTSemFastMutexRequest(pDevExt->mtxTracer);
            if (RT_SUCCESS(rc))
            {
                if (pDevExt->pTracerOps)
                    rc = pDevExt->pTracerOps->pfnProviderRegister(pDevExt->pTracerOps, &pProv->Core);
                else
                {
                    pProv->fRegistered = false;
                    rc = VINF_SUCCESS;
                }
                if (RT_SUCCESS(rc))
                {
                    RTListAppend(&pDevExt->TracerProviderList, &pProv->ListEntry);
                    RTSemFastMutexRelease(pDevExt->mtxTracer);
                    LOG_TRACER(("Registered DTrace provider '%s' in '%s' -> %p\n",
                                pProv->szName, pszModName, pProv->Core.TracerData.DTrace.idProvider));
                }
                else
                {
                    RTSemFastMutexRelease(pDevExt->mtxTracer);
                    RTMemFree(pProv);
                }
            }
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
        {
            PSUPDRVTPPROVIDER   pProvNext;
            supdrvTracerFreeProvider(pProv);

            RTSemFastMutexRequest(pDevExt->mtxTracer);
            RTListForEachReverseSafe(&pDevExt->TracerProviderList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
            {
                if (pProv->Core.pHdr == pVtgHdr)
                {
                    RTListNodeRemove(&pProv->ListEntry);
                    supdrvTracerDeregisterVtgObj(pDevExt, pProv);
                }
            }
            RTSemFastMutexRelease(pDevExt->mtxTracer);
            return rc;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Registers the VTG tracepoint providers of a driver.
 *
 * @returns VBox status code.
 * @param   pSession            The support driver session handle.
 * @param   pVtgHdr             The VTG header.
 * @param   pszName             The driver name.
 */
SUPR0DECL(int) SUPR0TracerRegisterDrv(PSUPDRVSESSION pSession, PVTGOBJHDR pVtgHdr, const char *pszName)
{
    int rc;

    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pVtgHdr, VERR_INVALID_POINTER);
    AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);

    rc = supdrvTracerRegisterVtgObj(pSession->pDevExt, pVtgHdr, _1M, NULL /*pImage*/, pSession, pszName);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvTracerProcessZombies(pSession->pDevExt);

    return rc;
}


/**
 * Deregister the VTG tracepoint providers of a driver.
 *
 * @param   pSession            The support driver session handle.
 * @param   pVtgHdr             The VTG header.
 */
SUPR0DECL(void) SUPR0TracerDeregisterDrv(PSUPDRVSESSION pSession)
{
    PSUPDRVTPPROVIDER pProv, pProvNext;
    PSUPDRVDEVEXT     pDevExt;
    AssertReturnVoid(SUP_IS_SESSION_VALID(pSession));
    AssertReturnVoid(pSession->R0Process == NIL_RTR0PROCESS);

    pDevExt = pSession->pDevExt;

    /*
     * Search for providers belonging to this driver session.
     */
    RTSemFastMutexRequest(pDevExt->mtxTracer);
    RTListForEachSafe(&pDevExt->TracerProviderList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
    {
        if (pProv->pSession == pSession)
        {
            RTListNodeRemove(&pProv->ListEntry);
            supdrvTracerDeregisterVtgObj(pDevExt, pProv);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxTracer);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvTracerProcessZombies(pDevExt);
}


/**
 * Registers the VTG tracepoint providers of a module loaded by
 * the support driver.
 *
 * This should be called from the ModuleInit code.
 *
 * @returns VBox status code.
 * @param   hMod                The module handle.
 * @param   pVtgHdr             The VTG header.
 */
SUPR0DECL(int) SUPR0TracerRegisterModule(void *hMod, PVTGOBJHDR pVtgHdr)
{
    PSUPDRVLDRIMAGE pImage = (PSUPDRVLDRIMAGE)hMod;
    PSUPDRVDEVEXT   pDevExt;
    uintptr_t       cbVtgObj;
    int             rc;

    /*
     * Validate input and context.
     */
    AssertPtrReturn(pImage,  VERR_INVALID_HANDLE);
    AssertPtrReturn(pVtgHdr, VERR_INVALID_POINTER);

    AssertPtrReturn(pImage, VERR_INVALID_POINTER);
    pDevExt = pImage->pDevExt;
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertReturn(pDevExt->pLdrInitImage  == pImage, VERR_WRONG_ORDER);
    AssertReturn(pDevExt->hLdrInitThread == RTThreadNativeSelf(), VERR_WRONG_ORDER);

    /*
     * Calculate the max VTG object size and hand it over to the common code.
     */
    cbVtgObj = (uintptr_t)pVtgHdr - (uintptr_t)pImage->pvImage;
    AssertMsgReturn(cbVtgObj /*off*/ < pImage->cbImageBits,
                    ("pVtgHdr=%p offVtgObj=%p cbImageBits=%p\n", pVtgHdr, cbVtgObj, pImage->cbImageBits),
                    VERR_INVALID_PARAMETER);
    cbVtgObj = pImage->cbImageBits - cbVtgObj;

    rc = supdrvTracerRegisterVtgObj(pDevExt, pVtgHdr, cbVtgObj, pImage, NULL, pImage->szName);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvTracerProcessZombies(pDevExt);

    return rc;
}


/**
 * Registers the tracer implementation.
 *
 * This should be called from the ModuleInit code or from a ring-0 session.
 *
 * @returns VBox status code.
 * @param   hMod                The module handle.
 * @param   pSession            Ring-0 session handle.
 * @param   pReg                Pointer to the tracer registration structure.
 * @param   ppHlp               Where to return the tracer helper method table.
 */
SUPR0DECL(int) SUPR0TracerRegisterImpl(void *hMod, PSUPDRVSESSION pSession, PCSUPDRVTRACERREG pReg, PCSUPDRVTRACERHLP *ppHlp)
{
    PSUPDRVLDRIMAGE pImage = (PSUPDRVLDRIMAGE)hMod;
    PSUPDRVDEVEXT   pDevExt;
    int             rc;

    /*
     * Validate input and context.
     */
    AssertPtrReturn(ppHlp, VERR_INVALID_POINTER);
    *ppHlp = NULL;
    AssertPtrReturn(pReg,  VERR_INVALID_HANDLE);

    if (pImage)
    {
        AssertPtrReturn(pImage, VERR_INVALID_POINTER);
        AssertReturn(pSession == NULL, VERR_INVALID_PARAMETER);
        pDevExt = pImage->pDevExt;
        AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
        AssertReturn(pDevExt->pLdrInitImage  == pImage, VERR_WRONG_ORDER);
        AssertReturn(pDevExt->hLdrInitThread == RTThreadNativeSelf(), VERR_WRONG_ORDER);
    }
    else
    {
        AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
        AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);
        pDevExt = pSession->pDevExt;
        AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    }

    AssertPtrReturn(pReg->pfnProbeFireKernel, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnProbeFireUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnTracerOpen, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnTracerIoCtl, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnTracerClose, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnProviderRegister, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnProviderDeregister, VERR_INVALID_POINTER);
    AssertPtrReturn(pReg->pfnProviderDeregisterZombie, VERR_INVALID_POINTER);

    /*
     * Do the job.
     */
    rc = RTSemFastMutexRequest(pDevExt->mtxTracer);
    if (RT_SUCCESS(rc))
    {
        if (!pDevExt->pTracerOps)
        {
            pDevExt->pTracerOps     = pReg;
            pDevExt->pTracerSession = pSession;
            pDevExt->pTracerImage   = pImage;

            *ppHlp = &pDevExt->TracerHlp;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_SUPDRV_TRACER_ALREADY_REGISTERED;
        RTSemFastMutexRelease(pDevExt->mtxTracer);
    }

    return rc;

}


/**
 * Common tracer implementation deregistration code.
 *
 * The caller sets fTracerUnloading prior to calling this function.
 *
 * @param   pDevExt             The device extension structure.
 */
static void supdrvTracerCommonDeregisterImpl(PSUPDRVDEVEXT pDevExt)
{
    supdrvTracerRemoveAllProviders(pDevExt);

    RTSemFastMutexRequest(pDevExt->mtxTracer);
    pDevExt->pTracerImage     = NULL;
    pDevExt->pTracerSession   = NULL;
    pDevExt->pTracerOps       = NULL;
    pDevExt->fTracerUnloading = false;
    RTSemFastMutexRelease(pDevExt->mtxTracer);
}


/**
 * Deregister a tracer implementation.
 *
 * This should be called from the ModuleTerm code or from a ring-0 session.
 *
 * @returns VBox status code.
 * @param   hMod                The module handle.
 * @param   pSession            Ring-0 session handle.
 */
SUPR0DECL(int) SUPR0TracerDeregisterImpl(void *hMod, PSUPDRVSESSION pSession)
{
    PSUPDRVLDRIMAGE pImage = (PSUPDRVLDRIMAGE)hMod;
    PSUPDRVDEVEXT   pDevExt;
    int             rc;

    /*
     * Validate input and context.
     */
    if (pImage)
    {
        AssertPtrReturn(pImage, VERR_INVALID_POINTER);
        AssertReturn(pSession == NULL, VERR_INVALID_PARAMETER);
        pDevExt = pImage->pDevExt;
    }
    else
    {
        AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
        AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);
        pDevExt = pSession->pDevExt;
    }
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);

    /*
     * Do the job.
     */
    rc = RTSemFastMutexRequest(pDevExt->mtxTracer);
    if (RT_SUCCESS(rc))
    {
        if (  pImage
            ? pDevExt->pTracerImage   == pImage
            : pDevExt->pTracerSession == pSession)
        {
            pDevExt->fTracerUnloading = true;
            RTSemFastMutexRelease(pDevExt->mtxTracer);
            supdrvTracerCommonDeregisterImpl(pDevExt);
        }
        else
        {
            rc = VERR_SUPDRV_TRACER_NOT_REGISTERED;
            RTSemFastMutexRelease(pDevExt->mtxTracer);
        }
    }

    return rc;
}


/*
 * The probe function is a bit more fun since we need tail jump optimizating.
 *
 * Since we cannot ship yasm sources for linux and freebsd, owing to the cursed
 * rebuilding of the kernel module from scratch at install time, we have to
 * deploy some ugly gcc inline assembly here.
 */
#if defined(__GNUC__) && (defined(RT_OS_FREEBSD) || defined(RT_OS_LINUX))
# if 0 /* Need to check this out on linux (on mac now) */
/*DECLASM(void)   supdrvTracerProbeFireStub(void);*/
__asm__ __volatile__("\
    .pushsection .text                                                  \
                                                                        \
    .p2align 2,,3                                                       \
    .type supdrvTracerProbeFireStub,@function                           \
    .global supdrvTracerProbeFireStub                                   \
supdrvTracerProbeFireStub:                                              \
    ret                                                                 \
supdrvTracerProbeFireStub_End:                                          \
    .size supdrvTracerProbeFireStub_End - supdrvTracerProbeFireStub     \
                                                                        \
    .p2align 2,,3                                                       \
    .global SUPR0TracerFireProbe                                        \
SUPR0TracerFireProbe:                                                   \
");
# if   defined(RT_ARCH_AMD64)
__asm__ __volatile__("                                                  \
    mov     g_pfnSupdrvProbeFireKernel(%rip), %rax                      \
    jmp     *%rax                                                       \
");
# elif defined(RT_ARCH_X86)
__asm__ __volatile__("                                                  \
    mov     g_pfnSupdrvProbeFireKernel, %eax                            \
    jmp     *%eax                                                       \
");
# else
#  error "Which arch is this?"
#endif
__asm__ __volatile__("                                                  \
SUPR0TracerFireProbe_End:                                               \
    .size SUPR0TracerFireProbe_End - SUPR0TracerFireProbe               \
    .popsection                                                         \
");

# else
SUPR0DECL(void) SUPR0TracerFireProbe(uint32_t idProbe, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                     uintptr_t uArg3, uintptr_t uArg4)
{
    return;
}
# endif
#endif


/**
 * Module unloading hook, called after execution in the module have ceased.
 *
 * @param   pDevExt             The device extension structure.
 * @param   pImage              The image being unloaded.
 */
void VBOXCALL supdrvTracerModuleUnloading(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVTPPROVIDER pProv, pProvNext;
    AssertPtrReturnVoid(pImage);        /* paranoia */

    RTSemFastMutexRequest(pDevExt->mtxTracer);

    /*
     * If it is the tracer image, we have to unload all the providers.
     */
    if (pDevExt->pTracerImage == pImage)
    {
        pDevExt->fTracerUnloading = true;
        RTSemFastMutexRelease(pDevExt->mtxTracer);
        supdrvTracerCommonDeregisterImpl(pDevExt);
    }
    else
    {
        /*
         * Unregister all providers belonging to this image.
         */
        RTListForEachSafe(&pDevExt->TracerProviderList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
        {
            if (pProv->pImage == pImage)
            {
                RTListNodeRemove(&pProv->ListEntry);
                supdrvTracerDeregisterVtgObj(pDevExt, pProv);
            }
        }

        RTSemFastMutexRelease(pDevExt->mtxTracer);

        /*
         * Try unregister zombies while we have a chance.
         */
        supdrvTracerProcessZombies(pDevExt);
    }
}


/**
 * Called when a session is being cleaned up.
 *
 * @param   pDevExt             The device extension structure.
 * @param   pSession            The session that is being torn down.
 */
void VBOXCALL supdrvTracerCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * If ring-0 session, make sure it has deregistered VTG objects and the tracer.
     */
    if (pSession->R0Process == NIL_RTR0PROCESS)
    {
        SUPDRVTPPROVIDER *pProvNext;
        SUPDRVTPPROVIDER *pProv;

        RTSemFastMutexRequest(pDevExt->mtxTracer);
        RTListForEachSafe(&pDevExt->TracerProviderList, pProv, pProvNext, SUPDRVTPPROVIDER, ListEntry)
        {
            if (pProv->pSession == pSession)
            {
                RTListNodeRemove(&pProv->ListEntry);
                supdrvTracerDeregisterVtgObj(pDevExt, pProv);
            }
        }
        RTSemFastMutexRelease(pDevExt->mtxTracer);

        (void)SUPR0TracerDeregisterImpl(NULL, pSession);
    }

    /*
     * Clean up instance data the trace may have associated with the session.
     */
    if (pSession->uTracerData)
    {
        RTSemFastMutexRequest(pDevExt->mtxTracer);
        if (   pSession->uTracerData
            && pDevExt->pTracerOps)
            pDevExt->pTracerOps->pfnTracerClose(pDevExt->pTracerOps, pSession, pSession->uTracerData);
        pSession->uTracerData = 0;
        RTSemFastMutexRelease(pDevExt->mtxTracer);
    }
}


/**
 * Early module initialization hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
int VBOXCALL supdrvTracerInit(PSUPDRVDEVEXT pDevExt)
{
    /*
     * Register a provider for this module.
     */
    int rc = RTSemFastMutexCreate(&pDevExt->mtxTracer);
    if (RT_SUCCESS(rc))
    {
        pDevExt->TracerHlp.uVersion    = SUPDRVTRACERHLP_VERSION;
        /** @todo  */
        pDevExt->TracerHlp.uEndVersion = SUPDRVTRACERHLP_VERSION;
        RTListInit(&pDevExt->TracerProviderList);
        RTListInit(&pDevExt->TracerProviderZombieList);

#ifdef VBOX_WITH_DTRACE_R0DRV
        rc = supdrvTracerRegisterVtgObj(pDevExt, &g_VTGObjHeader, _1M, NULL /*pImage*/, NULL /*pSession*/, "vboxdrv");
        if (RT_SUCCESS(rc))
#endif
            return rc;
        RTSemFastMutexDestroy(pDevExt->mtxTracer);
    }
    pDevExt->mtxTracer = NIL_RTSEMFASTMUTEX;
    return rc;
}


/**
 * Late module termination hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
void VBOXCALL supdrvTracerTerm(PSUPDRVDEVEXT pDevExt)
{
    LOG_TRACER(("supdrvTracerTerm\n"));

    supdrvTracerRemoveAllProviders(pDevExt);

    RTSemFastMutexDestroy(pDevExt->mtxTracer);
    pDevExt->mtxTracer = NIL_RTSEMFASTMUTEX;
    LOG_TRACER(("supdrvTracerTerm: Done\n"));
}

