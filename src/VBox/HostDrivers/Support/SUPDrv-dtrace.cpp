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
    /*
     * The header.
     */
    if (!memcmp(pVtgHdr->szMagic, VTGOBJHDR_MAGIC, sizeof(pVtgHdr->szMagic)))
        return VERR_SUPDRV_VTG_MAGIC;
    if (pVtgHdr->cBits != ARCH_BITS)
        return VERR_SUPDRV_VTG_BITS;
    if (pVtgHdr->u32Reserved0)
        return VERR_SUPDRV_VTG_RESERVED;

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

    MY_VALIDATE_PTR(pVtgHdr->paProviders,       pVtgHdr->cbProviders,    1,   16, sizeof(VTGDESCPROVIDER));
    MY_VALIDATE_PTR(pVtgHdr->paProbes,          pVtgHdr->cbProbes,       1, _32K, sizeof(VTGDESCPROBE));
    MY_VALIDATE_PTR(pVtgHdr->pafProbeEnabled,   pVtgHdr->cbProbeEnabled, 1, _32K, sizeof(bool));
    MY_VALIDATE_PTR(pVtgHdr->pachStrTab,        pVtgHdr->cbStrTab,       4,  _1M,  sizeof(char));
    MY_VALIDATE_PTR(pVtgHdr->paArgLists,        pVtgHdr->cbArgLists,     0, _32K, sizeof(uint32_t));
#undef MY_VALIDATE_PTR

    if (!RT_VALID_PTR(pVtgHdr->paProbLocs))
        return VERR_SUPDRV_VTG_BAD_PTR;
    if (!RT_VALID_PTR(pVtgHdr->paProbLocsEnd))
        return VERR_SUPDRV_VTG_BAD_PTR;
    if ((uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs < sizeof(VTGPROBELOC))
        return VERR_SUPDRV_VTG_TOO_FEW;
    if ((uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs > sizeof(VTGPROBELOC) * _128K)
        return VERR_SUPDRV_VTG_TOO_MUCH;
    if (   ((uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs) 
           / sizeof(VTGPROBELOC) * sizeof(VTGPROBELOC)
        != (uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pVtgHdr->paProbLocs)
        return VERR_SUPDRV_VTG_NOT_MULTIPLE;
    if (pbImage && cbImage)
    {
        if ((uintptr_t)pVtgHdr->paProbLocs - (uintptr_t)pbImage >= cbImage)
            return VERR_SUPDRV_VTG_BAD_PTR;
        if ((uintptr_t)pVtgHdr->paProbLocsEnd - (uintptr_t)pbImage > cbImage)
            return VERR_SUPDRV_VTG_BAD_PTR;
    }

    /*
     * Validate the providers.
     */


    return VINF_SUCCESS;
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


