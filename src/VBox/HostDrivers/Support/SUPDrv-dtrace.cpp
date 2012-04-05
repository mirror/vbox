#error FIXME
/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - DTrace Provider.
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

/** @todo Convert this to a generic tracer implementation. */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
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

#ifdef RT_OS_DARWIN /** @todo figure this! */
# include "/Developer/SDKs/MacOSX10.6.sdk/usr/include/sys/dtrace.h"
#else
# include <sys/dtrace.h>
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Data for a provider.
 */
typedef struct SUPDRVDTPROVIDER
{
    /** The entry in the provider list for this image. */
    RTLISTNODE          ListEntry;

    /** The provider descriptor. */
    PVTGDESCPROVIDER    pDesc;
    /** The VTG header. */
    PVTGOBJHDR          pHdr;

    /** Pointer to the image this provider resides in.  NULL if it's a
     * driver. */
    PSUPDRVLDRIMAGE     pImage;
    /** The session this provider is associated with if registered via
     * SUPR0VtgRegisterDrv.  NULL if pImage is set. */
    PSUPDRVSESSION      pSession;
    /** The module name. */
    const char         *pszModName;

    /** Dtrace provider attributes. */
    dtrace_pattr_t      DtAttrs;
    /** The ID of this provider. */
    dtrace_provider_id_t idDtProv;
    /** The number of probes we've provided to DTrace. */
    uint32_t            cProvidedProbes;
    /** Set when the module is unloaded or the driver deregisters its probes. */
    bool                fZombie;
    /** The provider name (for logging purposes). */
    char                szName[1];
} SUPDRVDTPROVIDER;
/** Pointer to the data for a provider. */
typedef SUPDRVDTPROVIDER *PSUPDRVDTPROVIDER;

/* Seems there is some return code difference here. Keep the return code and
   case it to whatever the host desires. */
#ifdef RT_OS_DARWIN
typedef void FNPOPS_ENABLE(void *, dtrace_id_t, void *);
#else
typedef int  FNPOPS_ENABLE(void *, dtrace_id_t, void *);
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if 0
# define LOG_DTRACE(a_Args)  SUPR0Printf a_Args
#else
# define LOG_DTRACE(a_Args)  do { } while (0)
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     supdrvDTracePOps_Provide(void *pvProv, const dtrace_probedesc_t *pDtProbeDesc);
static int      supdrvDTracePOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                            dtrace_argdesc_t *pArgDesc);
#ifdef RT_OS_SOLARIS
static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames);
#endif
static void     supdrvDTracePOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe);



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * DTrace provider method table.
 */
static const dtrace_pops_t g_SupDrvDTraceProvOps =
{
    /* .dtps_provide         = */ supdrvDTracePOps_Provide,
    /* .dtps_provide_module  = */ NULL,
    /* .dtps_enable          = */ (FNPOPS_ENABLE *)supdrvDTracePOps_Enable,
    /* .dtps_disable         = */ supdrvDTracePOps_Disable,
    /* .dtps_suspend         = */ NULL,
    /* .dtps_resume          = */ NULL,
    /* .dtps_getargdesc      = */ supdrvDTracePOps_GetArgDesc,
#ifdef RT_OS_SOLARIS
    /* .dtps_getargval       = */ supdrvDTracePOps_GetArgVal,
#else
    /* .dtps_getargval       = */ NULL/*supdrvDTracePOps_GetArgVal*/,
#endif
    /* .dtps_usermode        = */ NULL,
    /* .dtps_destroy         = */ supdrvDTracePOps_Destroy
};


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
 * Converts an attribute from VTG description speak to DTrace.
 *
 * @param   pDtAttr             The DTrace attribute (dst).
 * @param   pVtgAttr            The VTG attribute descriptor (src).
 */
static void supdrvVtgConvAttr(dtrace_attribute_t *pDtAttr, PCVTGDESCATTR pVtgAttr)
{
    pDtAttr->dtat_name  = pVtgAttr->u8Code - 1;
    pDtAttr->dtat_data  = pVtgAttr->u8Data - 1;
    pDtAttr->dtat_class = pVtgAttr->u8DataDep - 1;
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
static void supdrvVtgFreeProvider(PSUPDRVDTPROVIDER pProv)
{
    LOG_DTRACE(("Freeing DTrace provider '%s' / %p\n", pProv->szName, pProv->idDtProv));
    pProv->fZombie = true;
    pProv->pDesc   = NULL;
    pProv->pHdr    = NULL;
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
 * @remarks The caller owns mtxDTrace.
 */
static void supdrvVtgDeregister(PSUPDRVDEVEXT pDevExt, PSUPDRVDTPROVIDER pProv)
{
    int rc;

    dtrace_invalidate(pProv->idDtProv);
    rc = dtrace_unregister(pProv->idDtProv);
    if (!rc)
    {
        supdrvVtgFreeProvider(pProv);
        return;
    }

    pProv->fZombie = true;
    RTListAppend(&pDevExt->DtProviderZombieList, &pProv->ListEntry);
    LOG_DTRACE(("Invalidate DTrace provider '%s' / %p and put it on the zombie list\n", pProv->szName, pProv->idDtProv));
}


/**
 * Processes the zombie list.
 *
 * @param   pDevExt             The device extension.
 */
static void supdrvVtgProcessZombies(PSUPDRVDEVEXT pDevExt)
{
    PSUPDRVDTPROVIDER pProv, pProvNext;

    RTSemFastMutexRequest(pDevExt->mtxDTrace);
    RTListForEachSafe(&pDevExt->DtProviderZombieList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
    {
        int rc = dtrace_unregister(pProv->idDtProv);
        if (!rc)
        {
            RTListNodeRemove(&pProv->ListEntry);
            supdrvVtgFreeProvider(pProv);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxDTrace);
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
static int supdrvVtgRegister(PSUPDRVDEVEXT pDevExt, PVTGOBJHDR pVtgHdr, size_t cbVtgObj, PSUPDRVLDRIMAGE pImage,
                             PSUPDRVSESSION pSession, const char *pszModName)
{
    int                 rc;
    unsigned            i;
    PSUPDRVDTPROVIDER   pProv;

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

    rc = RTSemFastMutexRequest(pDevExt->mtxDTrace);
    if (RT_FAILURE(rc))
        return rc;
    RTListForEach(&pDevExt->DtProviderList, pProv, SUPDRVDTPROVIDER, ListEntry)
    {
        if (pProv->pHdr == pVtgHdr)
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
    RTSemFastMutexRelease(pDevExt->mtxDTrace);
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
        pProv = (PSUPDRVDTPROVIDER)RTMemAllocZ(RT_OFFSETOF(SUPDRVDTPROVIDER, szName[cchName + 1]));
        if (pProv)
        {
            pProv->pDesc            = pDesc;
            pProv->pHdr             = pVtgHdr;
            pProv->pImage           = pImage;
            pProv->pSession         = pSession;
            pProv->pszModName       = pszModName;
            pProv->idDtProv         = 0;
            pProv->cProvidedProbes  = 0;
            pProv->fZombie          = false;
            memcpy(pProv->szName, pszName, cchName + 1);
            supdrvVtgConvAttr(&pProv->DtAttrs.dtpa_provider, &pDesc->AttrSelf);
            supdrvVtgConvAttr(&pProv->DtAttrs.dtpa_mod,      &pDesc->AttrModules);
            supdrvVtgConvAttr(&pProv->DtAttrs.dtpa_func,     &pDesc->AttrFunctions);
            supdrvVtgConvAttr(&pProv->DtAttrs.dtpa_name,     &pDesc->AttrNames);
            supdrvVtgConvAttr(&pProv->DtAttrs.dtpa_args,     &pDesc->AttrArguments);

            rc = dtrace_register(pProv->szName,
                                 &pProv->DtAttrs,
                                 DTRACE_PRIV_KERNEL,
                                 NULL /* cred */,
                                 &g_SupDrvDTraceProvOps,
                                 pProv,
                                 &pProv->idDtProv);
            if (!rc)
            {
                rc = RTSemFastMutexRequest(pDevExt->mtxDTrace);
                if (RT_SUCCESS(rc))
                {
                    RTListAppend(&pDevExt->DtProviderList, &pProv->ListEntry);
                    RTSemFastMutexRelease(pDevExt->mtxDTrace);
                    LOG_DTRACE(("Registered DTrace provider '%s' in '%s' -> %p\n", pProv->szName, pszModName, pProv->idDtProv));
                }
                else
                    dtrace_unregister(pProv->idDtProv);
            }
            else
                rc = RTErrConvertFromErrno(rc);
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
        {
            PSUPDRVDTPROVIDER   pProvNext;
            supdrvVtgFreeProvider(pProv);

            RTSemFastMutexRequest(pDevExt->mtxDTrace);
            RTListForEachReverseSafe(&pDevExt->DtProviderList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
            {
                if (pProv->pHdr == pVtgHdr)
                {
                    RTListNodeRemove(&pProv->ListEntry);
                    supdrvVtgDeregister(pDevExt, pProv);
                }
            }
            RTSemFastMutexRelease(pDevExt->mtxDTrace);
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

    rc = supdrvVtgRegister(pSession->pDevExt, pVtgHdr, _1M, NULL /*pImage*/, pSession, pszName);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvVtgProcessZombies(pSession->pDevExt);

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
    PSUPDRVDTPROVIDER pProv, pProvNext;
    PSUPDRVDEVEXT     pDevExt;
    AssertReturnVoid(SUP_IS_SESSION_VALID(pSession));
    AssertReturnVoid(pSession->R0Process == NIL_RTR0PROCESS);

    pDevExt = pSession->pDevExt;

    /*
     * Search for providers belonging to this driver session.
     */
    RTSemFastMutexRequest(pDevExt->mtxDTrace);
    RTListForEachSafe(&pDevExt->DtProviderList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
    {
        if (pProv->pSession == pSession)
        {
            RTListNodeRemove(&pProv->ListEntry);
            supdrvVtgDeregister(pDevExt, pProv);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxDTrace);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvVtgProcessZombies(pDevExt);
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

    rc = supdrvVtgRegister(pDevExt, pVtgHdr, cbVtgObj, pImage, NULL, pImage->szName);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvVtgProcessZombies(pDevExt);

    return rc;
}


/**
 * Module unloading hook, called after execution in the module have ceased.
 *
 * @param   pDevExt             The device extension structure.
 * @param   pImage              The image being unloaded.
 */
void VBOXCALL supdrvVtgModuleUnloading(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVDTPROVIDER pProv, pProvNext;

    /*
     * Unregister all providers belonging to this image.
     */
    RTSemFastMutexRequest(pDevExt->mtxDTrace);
    RTListForEachSafe(&pDevExt->DtProviderList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
    {
        if (pProv->pImage == pImage)
        {
            RTListNodeRemove(&pProv->ListEntry);
            supdrvVtgDeregister(pDevExt, pProv);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxDTrace);

    /*
     * Try unregister zombies while we have a chance.
     */
    supdrvVtgProcessZombies(pDevExt);
}


/**
 * Early module initialization hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 * @param   pFireProbeEntry     Pointer to the SUPR0TracerFireProbe entry.
 */
int VBOXCALL supdrvVtgInit(PSUPDRVDEVEXT pDevExt, PSUPFUNC pFireProbeEntry)
{
    Assert(!strcmp(pFireProbeEntry->szName, "SUPR0TracerFireProbe"));

    /*
     * Register a provider for this module.
     */
    int rc = RTSemFastMutexCreate(&pDevExt->mtxDTrace);
    if (RT_SUCCESS(rc))
    {
#ifdef RT_OS_SOLARIS
        pFireProbeEntry->pfn = (void *)(uintptr_t)dtrace_probe;
#endif
        RTListInit(&pDevExt->DtProviderList);
        RTListInit(&pDevExt->DtProviderZombieList);
        rc = supdrvVtgRegister(pDevExt, &g_VTGObjHeader, _1M, NULL /*pImage*/, NULL /*pSession*/, "vboxdrv");
        if (RT_SUCCESS(rc))
            return rc;
        RTSemFastMutexDestroy(pDevExt->mtxDTrace);
    }
    pDevExt->mtxDTrace = NIL_RTSEMFASTMUTEX;
    return rc;
}


/**
 * Late module termination hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
void VBOXCALL supdrvVtgTerm(PSUPDRVDEVEXT pDevExt)
{
    PSUPDRVDTPROVIDER pProv, pProvNext;
    uint32_t i;
    LOG_DTRACE(("supdrvVtgTerm\n"));

    /*
     * Unregister all probes (there should only be one).
     */
    RTSemFastMutexRequest(pDevExt->mtxDTrace);
    RTListForEachSafe(&pDevExt->DtProviderList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
    {
        RTListNodeRemove(&pProv->ListEntry);
        supdrvVtgDeregister(pDevExt, pProv);
    }
    RTSemFastMutexRelease(pDevExt->mtxDTrace);

    /*
     * Try unregister zombies now, sleep on busy ones.
     */
    for (i = 0; ; i++)
    {
        bool fEmpty;

        RTSemFastMutexRequest(pDevExt->mtxDTrace);
        RTListForEachSafe(&pDevExt->DtProviderZombieList, pProv, pProvNext, SUPDRVDTPROVIDER, ListEntry)
        {
            int rc;
            LOG_DTRACE(("supdrvVtgTerm: Attemting to unregister '%s' / %p...\n", pProv->szName, pProv->idDtProv));
            rc = dtrace_unregister(pProv->idDtProv);
            if (!rc)
            {
                RTListNodeRemove(&pProv->ListEntry);
                supdrvVtgFreeProvider(pProv);
            }
            else if (!(i & 0xf))
                SUPR0Printf("supdrvVtgTerm: Waiting on busy provider '%s' / %p (rc=%d)\n", pProv->szName, pProv->idDtProv, rc);
            else
                LOG_DTRACE(("supdrvVtgTerm: Failed to unregister provider '%s' / %p - rc=%d\n", pProv->szName, pProv->idDtProv, rc));
        }

        fEmpty = RTListIsEmpty(&pDevExt->DtProviderZombieList);
        RTSemFastMutexRelease(pDevExt->mtxDTrace);
        if (fEmpty)
            break;

        /* Delay...*/
        RTThreadSleep(1000);
    }

    RTSemFastMutexDestroy(pDevExt->mtxDTrace);
    pDevExt->mtxDTrace = NIL_RTSEMFASTMUTEX;
    LOG_DTRACE(("supdrvVtgTerm: Done\n"));
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_provide}
 */
static void     supdrvDTracePOps_Provide(void *pvProv, const dtrace_probedesc_t *pDtProbeDesc)
{
    PSUPDRVDTPROVIDER   pProv      = (PSUPDRVDTPROVIDER)pvProv;
    uint16_t            idxProv;
    PVTGPROBELOC        pProbeLoc;
    PVTGPROBELOC        pProbeLocEnd;
    char               *pszFnNmBuf;
    size_t const        cbFnNmBuf = _4K + _1K;

    if (pDtProbeDesc)
        return;  /* We don't generate probes, so never mind these requests. */

    if (pProv->fZombie)
        return;

    if (pProv->cProvidedProbes >= pProv->pDesc->cProbes)
        return;

     /* Need a buffer for extracting the function names and mangling them in
        case of collision. */
     pszFnNmBuf = (char *)RTMemAlloc(cbFnNmBuf);
     if (!pszFnNmBuf)
         return;

     /*
      * Itereate the probe location list and register all probes related to
      * this provider.
      */
     idxProv      = (uint16_t)(&pProv->pHdr->paProviders[0] - pProv->pDesc);
     pProbeLoc    = pProv->pHdr->paProbLocs;
     pProbeLocEnd = pProv->pHdr->paProbLocsEnd;
     while ((uintptr_t)pProbeLoc < (uintptr_t)pProbeLocEnd)
     {
         PVTGDESCPROBE pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;
         if (   pProbeDesc->idxProvider == idxProv
             && pProbeLoc->idProbe      == UINT32_MAX)
         {
             /* The function name normally needs to be stripped since we're
                using C++ compilers for most of the code.  ASSUMES nobody are
                brave/stupid enough to use function pointer returns without
                typedef'ing properly them. */
             const char *pszPrbName = supdrvVtgGetString(pProv->pHdr, pProbeDesc->offName);
             const char *pszFunc    = pProbeLoc->pszFunction;
             const char *psz        = strchr(pProbeLoc->pszFunction, '(');
             size_t      cch;
             if (psz)
             {
                 /* skip blanks preceeding the parameter parenthesis. */
                 while (   (uintptr_t)psz > (uintptr_t)pProbeLoc->pszFunction
                        && RT_C_IS_BLANK(psz[-1]))
                     psz--;

                 /* Find the start of the function name. */
                 pszFunc = psz - 1;
                 while ((uintptr_t)pszFunc > (uintptr_t)pProbeLoc->pszFunction)
                 {
                     char ch = pszFunc[-1];
                     if (!RT_C_IS_ALNUM(ch) && ch != '_' && ch != ':')
                         break;
                     pszFunc--;
                 }
                 cch = psz - pszFunc;
             }
             else
                 cch = strlen(pszFunc);
             RTStrCopyEx(pszFnNmBuf, cbFnNmBuf, pszFunc, cch);

             /* Look up the probe, if we have one in the same function, mangle
                the function name a little to avoid having to deal with having
                multiple location entries with the same probe ID. (lazy bird) */
             Assert(pProbeLoc->idProbe == UINT32_MAX);
             if (dtrace_probe_lookup(pProv->idDtProv, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
             {
                 RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u",  pProbeLoc->uLine);
                 if (dtrace_probe_lookup(pProv->idDtProv, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
                 {
                     unsigned iOrd = 2;
                     while (iOrd < 128)
                     {
                         RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u-%u",  pProbeLoc->uLine, iOrd);
                         if (dtrace_probe_lookup(pProv->idDtProv, pProv->pszModName, pszFnNmBuf, pszPrbName) == DTRACE_IDNONE)
                             break;
                         iOrd++;
                     }
                     if (iOrd >= 128)
                     {
                         LogRel(("VBoxDrv: More than 128 duplicate probe location instances in file %s at line %u, function %s [%s], probe %s\n",
                                 pProbeLoc->pszFile, pProbeLoc->uLine, pProbeLoc->pszFunction, pszFnNmBuf, pszPrbName));
                         continue;
                     }
                 }
             }

             /* Create the probe. */
             AssertCompile(sizeof(pProbeLoc->idProbe) == sizeof(dtrace_id_t));
             pProbeLoc->idProbe = dtrace_probe_create(pProv->idDtProv, pProv->pszModName, pszFnNmBuf, pszPrbName,
                                                      0 /*aframes*/, pProbeLoc);
             pProv->cProvidedProbes++;
         }

         pProbeLoc++;
     }

     RTMemFree(pszFnNmBuf);
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_enable}
 */
static int      supdrvDTracePOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVDTPROVIDER   pProv      = (PSUPDRVDTPROVIDER)pvProv;
    if (!pProv->fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;

        if (!pProbeLoc->fEnabled)
        {
            pProbeLoc->fEnabled = 1;
            if (ASMAtomicIncU32(&pProbeDesc->u32User) == 1)
                pProv->pHdr->pafProbeEnabled[pProbeDesc->idxEnabled] = 1;
        }
    }

    return 0;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_disable}
 */
static void     supdrvDTracePOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVDTPROVIDER   pProv      = (PSUPDRVDTPROVIDER)pvProv;
    if (!pProv->fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;

        if (pProbeLoc->fEnabled)
        {
            pProbeLoc->fEnabled = 0;
            if (ASMAtomicDecU32(&pProbeDesc->u32User) == 0)
                pProv->pHdr->pafProbeEnabled[pProbeDesc->idxEnabled] = 1;
        }
    }
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargdesc}
 */
static void     supdrvDTracePOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                            dtrace_argdesc_t *pArgDesc)
{
    PSUPDRVDTPROVIDER   pProv      = (PSUPDRVDTPROVIDER)pvProv;
    unsigned            uArg       = pArgDesc->dtargd_ndx;

    if (!pProv->fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;
        PVTGDESCARGLIST pArgList   = (PVTGDESCARGLIST)((uintptr_t)pProv->pHdr->paArgLists + pProbeDesc->offArgList);

        Assert(pProbeDesc->offArgList < pProv->pHdr->cbArgLists);
        if (pArgList->cArgs > uArg)
        {
            const char *pszType = supdrvVtgGetString(pProv->pHdr, pArgList->aArgs[uArg].offType);
            size_t      cchType = strlen(pszType);
            if (cchType < sizeof(pArgDesc->dtargd_native))
            {
                memcpy(pArgDesc->dtargd_native, pszType, cchType + 1);
                /** @todo mapping */
                return;
            }
        }
    }

    pArgDesc->dtargd_ndx = DTRACE_ARGNONE;
}


#ifdef RT_OS_SOLARIS

# ifdef __cplusplus
extern "C"
#endif
uint64_t dtrace_getarg(int iArg, int cFrames);

/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargval}
 */
static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames)
{
    /* dtrace_getarg on AMD64 has a different opinion about how to use the
       cFrames argument than dtrace_caller() and/or dtrace_getpcstack(), at
       least when the probe is fired by dtrace_probe() the way we do.

       Setting aframes to 1 when calling dtrace_probe_create gives me the right
       arguments, but the wrong 'caller'.  Since I cannot do anything about
       'caller', the only solution is this hack.

       Not sure why the Solaris guys hasn't seen this issue before, but maybe
       there isn't anyone using the default argument getter path for ring-0
       dtrace_probe() calls, SDT surely isn't.

       WARNING! This code is subject to dtrace_getarg interface unstability! */
    /** @todo File a solaris bug on dtrace_probe() + dtrace_getarg(). */
    return dtrace_getarg(iArg, cFrames + 1);
}

#endif /* RT_OS_SOLARIS */


/**
 * @callback_method_impl{dtrace_pops_t,dtps_destroy}
 */
static void    supdrvDTracePOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVDTPROVIDER   pProv      = (PSUPDRVDTPROVIDER)pvProv;
    if (!pProv->fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        Assert(!pProbeLoc->fEnabled);
        Assert(pProbeLoc->idProbe == idProbe); NOREF(idProbe);
        pProbeLoc->idProbe = UINT32_MAX;
    }
    pProv->cProvidedProbes--;
}

