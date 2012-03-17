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
#include <iprt/assert.h>

#include <sys/dtrace.h>



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     supdrvDTracePOps_ProvideModule(void *pvProv, struct modctl *pMod);
static int      supdrvDTracePOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe);
static void     supdrvDTracePOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe, 
                                            dtrace_argdesc_t *pArgDesc);
static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames);
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
    /* .dtps_getargval       = */ supdrvDTracePOps_GetArgVal,
    /* .dtps_usermode        = */ NULL,
    /* .dtps_destroy         = */ supdrvDTracePOps_Destroy
};



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


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargval}
 */
static uint64_t supdrvDTracePOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                           int iArg, int cFrames)
{
    return 0xbeef;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_destroy}
 */
static void    supdrvDTracePOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
}


