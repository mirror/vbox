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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "SUPDrvInternal.h"

#include <VBox/err.h>
#include <VBox/VBoxTpG.h>
#include <iprt/assert.h>

#include <sys/dtrace.h>


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
} SUPDRVDTPROVIDER;
/** Pointer to the data for a provider. */
typedef SUPDRVDTPROVIDER *PSUPDRVDTPROVIDER;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     supdrvDTracePOps_ProvideModule(void *pvProv, struct modctl *pMod);
static int      supdrvDTracePOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                            dtrace_argdesc_t *pArgDesc);
/*static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames);*/
static void     supdrvDTracePOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe);



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The default provider attributes. */
static dtrace_pattr_t g_DefProvAttrs =
{   /*                       .dtat_name,                .dtat_data,                .dtat_class */
    /* .dtpa_provider = */ { DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_ISA },
    /* .dtpa_mod      = */ { DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_ISA },
    /* .dtpa_func     = */ { DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_ISA },
    /* .dtpa_name     = */ { DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_ISA },
    /* .dtpa_args     = */ { DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_ISA },
};

/**
 * DTrace provider method table.
 */
static const dtrace_pops_t g_SupDrvDTraceProvOps =
{
    /* .dtps_provide         = */ NULL,
    /* .dtps_provide_module  = */ supdrvDTracePOps_ProvideModule,
    /* .dtps_enable          = */ supdrvDTracePOps_Enable,
    /* .dtps_disable         = */ supdrvDTracePOps_Disable,
    /* .dtps_suspend         = */ NULL,
    /* .dtps_resume          = */ NULL,
    /* .dtps_getargdesc      = */ supdrvDTracePOps_GetArgDesc,
    /* .dtps_getargval       = */ NULL/*supdrvDTracePOps_GetArgVal*/,
    /* .dtps_usermode        = */ NULL,
    /* .dtps_destroy         = */ supdrvDTracePOps_Destroy
};


#define VERR_SUPDRV_VTG_MAGIC           (-3704)
#define VERR_SUPDRV_VTG_BITS            (-3705)
#define VERR_SUPDRV_VTG_RESERVED        (-3705)
#define VERR_SUPDRV_VTG_BAD_PTR         (-3706)
#define VERR_SUPDRV_VTG_TOO_FEW         (-3707)
#define VERR_SUPDRV_VTG_TOO_MUCH        (-3708)
#define VERR_SUPDRV_VTG_NOT_MULTIPLE    (-3709)


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
            && ch != '('
            && ch != ')'
            && ch != ','
            && ch != '*'
            && ch != '&' 
           )
            return VERR_SUPDRV_VTG_BAD_STRING;
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
static int supdrvVtgValidate(PVTGOBJHDR pVtgHdr, size_t cbVtgObj, uint8_t *pbImage, size_t cbImage)
{
    uintptr_t   cbTmp;
    uintptr_t   i;
    int         rc;

    if (!pbImage || !cbImage)
    {
        pbImage = NULL;
        cbImage = 0;
    }

#define MY_VALIDATE_PTR(p, cb, cMin, cMax, cbUnit) \
    do { \
        if (   (cb) >= cbVtgObj \
            || (uintptr_t)(p) - (uintptr_t)pVtgHdr < cbVtgObj - (cb) ) \
            return VERR_SUPDRV_VTG_BAD_PTR; \
        if ((cb) <  (cMin) * (cbUnit)) \
            return VERR_SUPDRV_VTG_TOO_FEW; \
        if ((cb) >= (cMax) * (cbUnit)) \
            return VERR_SUPDRV_VTG_TOO_MUCH; \
        if ((cb) / (cbUnit) * (cbUnit) != (cb)) \
            return VERR_SUPDRV_VTG_NOT_MULTIPLE; \
    } while (0)
#define MY_WITHIN_IMAGE(p) \
    do { \
        if (pbImage) \
        { \
            if ((uintptr_t)(p) - (uintptr_t)pbImage > cbImage) \
                return VERR_SUPDRV_VTG_BAD_PTR; \
        } \
        else if (!RT_VALID_PTR(p)) \
            return VERR_SUPDRV_VTG_BAD_PTR; \
    } while (0)
#define MY_WITHIN_IMAGE_RANGE(p, cb)
    do { \
        if (pbImage) \
        { \
            if (   (cb) > cbImage \
                || (uintptr_t)(p) - (uintptr_t)pbImage > cbImage - (cb)) \
                return VERR_SUPDRV_VTG_BAD_PTR; \
        } \
        else if (!RT_VALID_PTR(p) || RT_VALID_PTR((uint8_t *)(p) + cb)) \
            return VERR_SUPDRV_VTG_BAD_PTR; \
    } while (0)
#define MY_VALIDATE_STR(offStrTab) \
    do { \
        if ((offStrTab) >= pVtgHdr->cbStrTab) \
            return VERR_SUPDRV_VTG_STRTAB_OFF; \
        rc = supdrvVtgValidateString(pVtgHdr->pachStrTab + (offStrTab)); \
        if (rc != VINF_SUCCESS) \
            return rc; \
    } while (0)
#define MY_VALIDATE_ATTR(Attr)
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
    if (!memcmp(pVtgHdr->szMagic, VTGOBJHDR_MAGIC, sizeof(pVtgHdr->szMagic)))
        return VERR_SUPDRV_VTG_MAGIC;
    if (pVtgHdr->cBits != ARCH_BITS)
        return VERR_SUPDRV_VTG_BITS;
    if (pVtgHdr->u32Reserved0)
        return VERR_SUPDRV_VTG_RESERVED;

    MY_VALIDATE_PTR(pVtgHdr->paProviders,       pVtgHdr->cbProviders,    1,    16, sizeof(VTGDESCPROVIDER));
    MY_VALIDATE_PTR(pVtgHdr->paProbes,          pVtgHdr->cbProbes,       1,  _32K, sizeof(VTGDESCPROBE));
    MY_VALIDATE_PTR(pVtgHdr->pafProbeEnabled,   pVtgHdr->cbProbeEnabled, 1,  _32K, sizeof(bool));
    MY_VALIDATE_PTR(pVtgHdr->pachStrTab,        pVtgHdr->cbStrTab,       4,   _1M, sizeof(char));
    MY_VALIDATE_PTR(pVtgHdr->paArgLists,        pVtgHdr->cbArgLists,     0,  _32K, sizeof(uint32_t));
    MY_WITHIN_IMAGE(pVtgHdr->paProbLocs);
    MY_WITHIN_IMAGE(pVtgHdr->paProbLocsEnd);
    if ((uintptr_t)pVtgHdr->paProbLocs > (uintptr_t)pVtgHdr->paProbLocsEnd)
        return VERR_SUPDRV_VTG_BAD_PTR;
    cbTmp = (uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs;
    MY_VALIDATE_PTR(pVtgHdr->paProbLocs,        cbTmp,                   1, _128K, sizeof(VTGPROBELOC))
    if (cbTmp < sizeof(VTGPROBELOC))
        return VERR_SUPDRV_VTG_TOO_FEW;

    if (pVtgHdr->cbProbes / sizeof(VTGDESCPROBE) != pVtgHdr->cbProbeEnabled)
        return VERR_SUPDRV_VTG_BAD_HDR;

    /*
     * Validate the providers.
     */
    i = pVtgHdr->cbProviders / sizeof(VTGDESCPROVIDER);
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
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrName);
        MY_VALIDATE_ATTR(pVtgHdr->paProviders[i].AttrArguments);
        if (pVtgHdr->paProviders[i].bReserved)
            return VERR_SUPDRV_VTG_RESERVED;
    }

    /*
     * Validate probes.
     */
    i = pVtgHdr->cbProbes / sizeof(VTGDESCPROBE);
    while (i-- > 0)
    {
        MY_VALIDATE_STR(pVtgHdr->paProbes[i].offName);
    }

    return VINF_SUCCESS;
#undef MY_VALIDATE_STR
#undef MY_VALIDATE_PTR
#undef MY_WITHIN_IMAGE
#undef MY_WITHIN_IMAGE_RANGE
}


/**
 * Registers the VTG tracepoint providers of a driver.
 *  
 * @returns VBox status code. 
 * @param   pszName             The driver name.
 * @param   pVtgHdr             The VTG header. 
 * @param   pImage              The image if applicable. 
 * @param   pSession            The session if applicable. 
 * @param   pszModName          The module name. 
 */
static int supdrvVtgRegister(PSUPDRVDEVEXT pDevExt, PVTGOBJHDR pVtgHdr, PSUPDRVLDRIMAGE pImage, PSUPDRVSESSION pSession,
                             const char *pszModName)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertPtrReturn(pVtgHdr, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pImage, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pszModName, VERR_INVALID_POINTER);
    AssertReturn((pImage == NULL) != (pSession != NULL), VERR_INVALID_PARAMETER);



    /*
     *
     */

}



/**
 * Registers the VTG tracepoint providers of a driver.
 *  
 * @returns VBox status code. 
 * @param   pSession            The support driver session handle. 
 * @param   pVtgHdr             The VTG header.
 * @param   pszName             The driver name.
 */
SUPR0DECL(int) SUPR0VtgRegisterDrv(PSUPDRVSESSION pSession, PVTGOBJHDR pVtgHdr, const char *pszName)
{
    AssertReturn(SUP_IS_SESSION_VALID(pSession), NULL);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pVtgHdr, VERR_INVALID_POINTER);

    return supdrvVtgRegister(pSession->pDevExt, pVtgHdr, NULL, pSession, pszName);
}



/**
 * Deregister the VTG tracepoint providers of a driver.
 *  
 * @param   pSession            The support driver session handle. 
 * @param   pVtgHdr             The VTG header.
 */
SUPR0DECL(void) SUPR0VtgDeregisterDrv(PSUPDRVSESSION pSession, PVTGOBJHDR pVtgHdr)
{
}



/**
 * Early module initialization hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
int supdrvDTraceInit(PSUPDRVDEVEXT pDevExt)
{
    /*
     * Register a provider for this module.
     */

    return VINF_SUCCESS;
}


/**
 * Late module termination hook.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
int supdrvDTraceTerm(PSUPDRVDEVEXT pDevExt)
{
    /*
     * Undo what we did in supdrvDTraceInit.
     */

    return VINF_SUCCESS;
}


/**
 * Module loading hook, called before calling into the module.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
int supdrvDTraceModuleLoading(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    /*
     * Check for DTrace probes in the module, register a new provider for them
     * if found.
     */

    return VINF_SUCCESS;
}


/**
 * Module unloading hook, called after execution in the module
 * have ceased.
 *
 * @returns VBox status code.
 * @param   pDevExt             The device extension structure.
 */
int supdrvDTraceModuleUnloading(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    /*
     * Undo what we did in supdrvDTraceModuleLoading.
     */
    return VINF_SUCCESS;
}





/**
 * @callback_method_impl{dtrace_pops_t,dtps_provide_module}
 */
static void     supdrvDTracePOps_ProvideModule(void *pvProv, struct modctl *pMod)
{
    return;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_enable}
 */
static int      supdrvDTracePOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    return -1;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_disable}
 */
static void     supdrvDTracePOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargdesc}
 */
static void     supdrvDTracePOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                            dtrace_argdesc_t *pArgDesc)
{
    pArgDesc->dtargd_ndx = DTRACE_ARGNONE;
}


#if 0
/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargval}
 */
static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames)
{
    return 0xbeef;
}
#endif


/**
 * @callback_method_impl{dtrace_pops_t,dtps_destroy}
 */
static void    supdrvDTracePOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
}


