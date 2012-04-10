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
#include <VBox/log.h>
#include <VBox/VBoxTpG.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>

#ifdef RT_OS_DARWIN /** @todo figure this! */
# include "/Developer/SDKs/MacOSX10.6.sdk/usr/include/sys/dtrace.h"
#else
# include <sys/dtrace.h>
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/* Seems there is some return code difference here. Keep the return code and
   case it to whatever the host desires. */
#ifdef RT_OS_DARWIN
typedef void FNPOPS_ENABLE(void *, dtrace_id_t, void *);
#else
typedef int  FNPOPS_ENABLE(void *, dtrace_id_t, void *);
#endif


/**
 * Stack data planted before calling dtrace_probe so that we can easily find the
 * stack argument later.
 */
typedef struct SUPDRVDTSTACKDATA
{
    /** Eyecatcher no. 1 (SUPDRVDT_STACK_DATA_MAGIC2). */
    uint32_t                    u32Magic1;
    /** Eyecatcher no. 2 (SUPDRVDT_STACK_DATA_MAGIC2). */
    uint32_t                    u32Magic2;
    /** Pointer to the stack arguments of a probe function call. */
    uintptr_t                  *pauStackArgs;
    /** Pointer to this structure.
     * This is the final bit of integrity checking. */
    struct SUPDRVDTSTACKDATA   *pSelf;
} SUPDRVDTSTACKDATA;
/** Pointer to the on-stack thread specific data. */
typedef SUPDRVDTSTACKDATA *PSUPDRVDTSTACKDATA;

/** The first magic value. */
#define SUPDRVDT_STACK_DATA_MAGIC1      RT_MAKE_U32_FROM_U8('S', 'U', 'P', 'D')
/** The second magic value. */
#define SUPDRVDT_STACK_DATA_MAGIC2      RT_MAKE_U32_FROM_U8('D', 'T', 'r', 'c')

/** The alignment of the stack data.
 * The data doesn't require more than sizeof(uintptr_t) alignment, but the
 * greater alignment the quicker lookup. */
#define SUPDRVDT_STACK_DATA_ALIGN       32

/** Plants the stack data. */
#define SUPDRVDT_SETUP_STACK_DATA() \
    uint8_t abBlob[sizeof(SUPDRVDTSTACKDATA) + SUPDRVDT_STACK_DATA_ALIGN - 1]; \
    PSUPDRVDTSTACKDATA pStackData = (PSUPDRVDTSTACKDATA)(   (uintptr_t)&abBlob[SUPDRVDT_STACK_DATA_ALIGN - 1] \
                                                         & ~(uintptr_t)(SUPDRVDT_STACK_DATA_ALIGN - 1)); \
    pStackData->u32Magic1   = SUPDRVDT_STACK_DATA_MAGIC1; \
    pStackData->u32Magic2   = SUPDRVDT_STACK_DATA_MAGIC2; \
    pStackData->pSelf       = pStackData

/** Passifies the stack data and frees up resource held within it. */
#define SUPDRVDT_CLEAR_STACK_DATA() \
    do \
    { \
        pStackData->u32Magic1   = 0; \
        pStackData->u32Magic2   = 0; \
        pStackData->pSelf       = NULL; \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef RT_OS_DARWIN
/** @name DTrace kernel interface used on Darwin
 * @{ */ 
static void        (* dtrace_probe)(dtrace_id_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static dtrace_id_t (* dtrace_probe_create)(dtrace_provider_id_t, const char *, const char *, const char *, int, void *);
static dtrace_id_t (* dtrace_probe_lookup)(dtrace_provider_id_t, const char *, const char *, const char *);
static int         (* dtrace_register)(const char *, const dtrace_pattr_t *, uint32_t, /*cred_t*/ void *, 
                                       const dtrace_pops_t *, void *, dtrace_provider_id_t *);
static void        (* dtrace_invalidate)(dtrace_provider_id_t);
static int         (* dtrace_unregister)(dtrace_provider_id_t);
/** @} */
#endif


/*
 *
 * Helpers for handling VTG structures.
 * Helpers for handling VTG structures.
 * Helpers for handling VTG structures.
 *
 */



/**
 * Converts an attribute from VTG description speak to DTrace.
 *
 * @param   pDtAttr             The DTrace attribute (dst).
 * @param   pVtgAttr            The VTG attribute descriptor (src).
 */
static void vboxDtVtgConvAttr(dtrace_attribute_t *pDtAttr, PCVTGDESCATTR pVtgAttr)
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
static const char *vboxDtVtgGetString(PVTGOBJHDR pVtgHdr,  uint32_t offStrTab)
{
    Assert(offStrTab < pVtgHdr->cbStrTab);
    return &pVtgHdr->pachStrTab[offStrTab];
}



/*
 *
 * DTrace Provider Interface.
 * DTrace Provider Interface.
 * DTrace Provider Interface.
 *
 */


/**
 * @callback_method_impl{dtrace_pops_t,dtps_provide}
 */
static void     supdrvDtPOps_Provide(void *pvProv, const dtrace_probedesc_t *pDtProbeDesc)
{
    PSUPDRVVDTPROVIDERCORE  pProv        = (PSUPDRVVDTPROVIDERCORE)pvProv;
    PVTGPROBELOC            pProbeLoc    = pProv->pHdr->paProbLocs;
    PVTGPROBELOC            pProbeLocEnd = pProv->pHdr->paProbLocsEnd;
    dtrace_provider_id_t    idProvider   = pProv->TracerData.DTrace.idProvider;
    size_t const            cbFnNmBuf    = _4K + _1K;
    char                   *pszFnNmBuf;
    uint16_t                idxProv;

    if (pDtProbeDesc)
        return;  /* We don't generate probes, so never mind these requests. */

    if (pProv->TracerData.DTrace.fZombie)
        return;

    if (pProv->TracerData.DTrace.cProvidedProbes >= pProbeLocEnd - pProbeLoc)
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
             const char *pszPrbName = vboxDtVtgGetString(pProv->pHdr, pProbeDesc->offName);
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
             if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
             {
                 RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u",  pProbeLoc->uLine);
                 if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
                 {
                     unsigned iOrd = 2;
                     while (iOrd < 128)
                     {
                         RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u-%u",  pProbeLoc->uLine, iOrd);
                         if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) == DTRACE_IDNONE)
                             break;
                         iOrd++;
                     }
                     if (iOrd >= 128)
                     {
                         LogRel(("VBoxDrv: More than 128 duplicate probe location instances %s at line %u in function %s [%s], probe %s\n",
                                 pProbeLoc->uLine, pProbeLoc->pszFunction, pszFnNmBuf, pszPrbName));
                         continue;
                     }
                 }
             }

             /* Create the probe. */
             AssertCompile(sizeof(pProbeLoc->idProbe) == sizeof(dtrace_id_t));
             pProbeLoc->idProbe = dtrace_probe_create(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName,
                                                      1 /*aframes*/, pProbeLoc);
             pProv->TracerData.DTrace.cProvidedProbes++;
         }

         pProbeLoc++;
     }

     RTMemFree(pszFnNmBuf);
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_enable}
 */
static int      supdrvDtPOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
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
static void     supdrvDtPOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
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
static void     supdrvDtPOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                        dtrace_argdesc_t *pArgDesc)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    unsigned                uArg   = pArgDesc->dtargd_ndx;

    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;
        PVTGDESCARGLIST pArgList   = (PVTGDESCARGLIST)((uintptr_t)pProv->pHdr->paArgLists + pProbeDesc->offArgList);

        Assert(pProbeDesc->offArgList < pProv->pHdr->cbArgLists);
        if (pArgList->cArgs > uArg)
        {
            const char *pszType = vboxDtVtgGetString(pProv->pHdr, pArgList->aArgs[uArg].offType);
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


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargval} 
 *  
 *  
 * We just cook our own stuff here, using a stack marker for finding the 
 * required information.  That's more reliable than subjecting oneself to the 
 * solaris bugs and 32-bit apple peculiarities. 
 *  
 *  
 * @remarks Solaris Bug
 *  
 * dtrace_getarg on AMD64 has a different opinion about how to use the cFrames 
 * argument than dtrace_caller() and/or dtrace_getpcstack(), at least when the 
 * probe is fired by dtrace_probe() the way we do. 
 * 
 * Setting aframes to 1 when calling dtrace_probe_create gives me the right
 * arguments, but the wrong 'caller'.  Since I cannot do anything about
 * 'caller', the only solution is this hack.
 * 
 * Not sure why the  Solaris guys hasn't seen this issue before, but maybe there
 * isn't anyone using the default argument getter path for ring-0 dtrace_probe() 
 * calls, SDT surely isn't. 
 * 
 * @todo File a solaris bug on dtrace_probe() + dtrace_getarg().
 *  
 *  
 * @remarks 32-bit XNU (Apple) 
 *  
 * The dtrace_probe arguments are 64-bit unsigned integers instead of uintptr_t, 
 * so we need to make an extra call. 
 *  
 */
static uint64_t supdrvDtPOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                       int iArg, int cFrames)
{
    AssertReturn(iArg >= 5, UINT64_MAX);

    /* Locate the caller of probe_dtrace, . */
    int volatile        iDummy = 1; /* use this to get the stack address. */
    PSUPDRVDTSTACKDATA  pData = (PSUPDRVDTSTACKDATA)(  ((uintptr_t)&iDummy + SUPDRVDT_STACK_DATA_ALIGN - 1)
                                                     & ~(uintptr_t)(SUPDRVDT_STACK_DATA_ALIGN - 1));
    for (;;)
    {
        if (   pData->u32Magic1 == SUPDRVDT_STACK_DATA_MAGIC1
            && pData->u32Magic2 == SUPDRVDT_STACK_DATA_MAGIC2
            && pData->pSelf     == pData)
            break;
        pData = (PSUPDRVDTSTACKDATA)((uintptr_t)pData + SUPDRVDT_STACK_DATA_ALIGN);
    }

    /* Get the stack data. */
    return pData->pauStackArgs[iArg - 5];
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_destroy}
 */
static void    supdrvDtPOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        Assert(!pProbeLoc->fEnabled);
        Assert(pProbeLoc->idProbe == idProbe); NOREF(idProbe);
        pProbeLoc->idProbe = UINT32_MAX;
    }
    pProv->TracerData.DTrace.cProvidedProbes--;
}



/**
 * DTrace provider method table.
 */
static const dtrace_pops_t g_vboxDtVtgProvOps =
{
    /* .dtps_provide         = */ supdrvDtPOps_Provide,
    /* .dtps_provide_module  = */ NULL,
    /* .dtps_enable          = */ (FNPOPS_ENABLE *)supdrvDtPOps_Enable,
    /* .dtps_disable         = */ supdrvDtPOps_Disable,
    /* .dtps_suspend         = */ NULL,
    /* .dtps_resume          = */ NULL,
    /* .dtps_getargdesc      = */ supdrvDtPOps_GetArgDesc,
    /* .dtps_getargval       = */ supdrvDtPOps_GetArgVal,
    /* .dtps_usermode        = */ NULL,
    /* .dtps_destroy         = */ supdrvDtPOps_Destroy
};




/*
 *
 * Support Driver Tracer Interface.
 * Support Driver Tracer Interface.
 * Support Driver Tracer Interface.
 *
 */



/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProbeFireUser}
 */
static DECLCALLBACK(void) supdrvDtTOps_ProbeFireKernel(struct VTGPROBELOC *pVtgProbeLoc, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                                       uintptr_t uArg3, uintptr_t uArg4)
{
    SUPDRVDT_SETUP_STACK_DATA();

    pStackData->pauStackArgs = &uArg4 + 1;
    dtrace_probe(pVtgProbeLoc->idProbe, uArg0, uArg1, uArg2, uArg3, uArg4);

    SUPDRVDT_CLEAR_STACK_DATA();
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProbeFireUser}
 */
static DECLCALLBACK(void) supdrvDtTOps_ProbeFireUser(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, PCSUPDRVTRACERUSRCTX pCtx)
{
    NOREF(pThis); NOREF(pSession); NOREF(pCtx);
    return;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerOpen}
 */
static DECLCALLBACK(int) supdrvDtTOps_TracerOpen(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uint32_t uCookie, 
                                                 uintptr_t uArg, uintptr_t *puSessionData)
{
    NOREF(pThis); NOREF(pSession); NOREF(uCookie); NOREF(uArg);
    *puSessionData = 0;
    return VERR_NOT_SUPPORTED;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerClose}
 */
static DECLCALLBACK(int) supdrvDtTOps_TracerIoCtl(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uintptr_t uSessionData,
                                                  uintptr_t uCmd, uintptr_t uArg, int32_t *piRetVal)
{
    NOREF(pThis); NOREF(pSession); NOREF(uSessionData);
    NOREF(uCmd); NOREF(uArg); NOREF(piRetVal);
    return VERR_NOT_SUPPORTED;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerClose}
 */
static DECLCALLBACK(void) supdrvDtTOps_TracerClose(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uintptr_t uSessionData)
{
    NOREF(pThis); NOREF(pSession); NOREF(uSessionData);
    return;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderRegister}
 */
static DECLCALLBACK(int) supdrvDtTOps_ProviderRegister(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    AssertReturn(pCore->TracerData.DTrace.idProvider == UINT32_MAX || pCore->TracerData.DTrace.idProvider == 0,
                 VERR_INTERNAL_ERROR_3);

    PVTGDESCPROVIDER    pDesc = pCore->pDesc;
    dtrace_pattr_t      DtAttrs;
    vboxDtVtgConvAttr(&DtAttrs.dtpa_provider, &pDesc->AttrSelf);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_mod,      &pDesc->AttrModules);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_func,     &pDesc->AttrFunctions);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_name,     &pDesc->AttrNames);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_args,     &pDesc->AttrArguments);

    dtrace_provider_id_t idProvider;
    int rc = dtrace_register(pCore->pszName,
                             &DtAttrs,
                             DTRACE_PRIV_KERNEL,
                             NULL /* cred */,
                             &g_vboxDtVtgProvOps,
                             pCore,
                             &idProvider);
    if (!rc)
    {
        Assert(idProvider != UINT32_MAX && idProvider != 0);
        pCore->TracerData.DTrace.idProvider = idProvider;
        Assert(pCore->TracerData.DTrace.idProvider == idProvider);
        rc = VINF_SUCCESS;
    }
    else
        rc = RTErrConvertFromErrno(rc);

    return rc;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderDeregister}
 */
static DECLCALLBACK(int) supdrvDtTOps_ProviderDeregister(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    uintptr_t idProvider = pCore->TracerData.DTrace.idProvider;
    AssertReturn(idProvider != UINT32_MAX && idProvider != 0, VERR_INTERNAL_ERROR_4);

    dtrace_invalidate(idProvider);
    int rc = dtrace_unregister(idProvider);
    if (!rc)
    {
        pCore->TracerData.DTrace.idProvider = UINT32_MAX;
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsg(rc == EBUSY, ("%d\n", rc));
        pCore->TracerData.DTrace.fZombie = true;
        rc = VERR_TRY_AGAIN;
    }

    return rc;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderDeregisterZombie}
 */
static DECLCALLBACK(int) supdrvDtTOps_ProviderDeregisterZombie(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    uintptr_t idProvider = pCore->TracerData.DTrace.idProvider;
    AssertReturn(idProvider != UINT32_MAX && idProvider != 0, VERR_INTERNAL_ERROR_4);
    Assert(pCore->TracerData.DTrace.fZombie);

    int rc = dtrace_unregister(idProvider);
    if (!rc)
    {
        pCore->TracerData.DTrace.idProvider = UINT32_MAX;
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsg(rc == EBUSY, ("%d\n", rc));
        rc = VERR_TRY_AGAIN;
    }

    return rc;
}



/**
 * The tracer registration record of the VBox DTrace implementation
 */
static SUPDRVTRACERREG g_supdrvDTraceReg =
{
    SUPDRVTRACERREG_MAGIC,
    SUPDRVTRACERREG_VERSION,
    supdrvDtTOps_ProbeFireKernel,
    supdrvDtTOps_ProbeFireUser,
    supdrvDtTOps_TracerOpen,
    supdrvDtTOps_TracerIoCtl,
    supdrvDtTOps_TracerClose,
    supdrvDtTOps_ProviderRegister,
    supdrvDtTOps_ProviderDeregister,
    supdrvDtTOps_ProviderDeregisterZombie,
    SUPDRVTRACERREG_MAGIC
};



/**
 * Module initialization code.
 *
 * @param   hMod            Opque module handle.
 */
const SUPDRVTRACERREG * VBOXCALL supdrvDTraceInit(void)
{
#ifdef RT_OS_DARWIN
    /*
     * Resolve the kernel symbols we need.
     */
    RTDBGKRNLINFO hKrnlInfo;
    int rc = RTR0DbgKrnlInfoOpen(&hKrnlInfo, 0);
    if (RT_FAILURE(rc))
    {
        SUPR0Printf("supdrvDTraceInit: RTR0DbgKrnlInfoOpen failed with rc=%d.\n", rc);
        return NULL;
    }

    static const struct
    {
        const char *pszName;
        PFNRT      *ppfn;
    } s_aDTraceFunctions[] =
    {   
        { "dtrace_probe",        (PFNRT*)&dtrace_probe        },
        { "dtrace_probe_create", (PFNRT*)&dtrace_probe_create },
        { "dtrace_probe_lookup", (PFNRT*)&dtrace_probe_lookup },
        { "dtrace_register",     (PFNRT*)&dtrace_register     },
        { "dtrace_invalidate",   (PFNRT*)&dtrace_invalidate   },
        { "dtrace_unregister",   (PFNRT*)&dtrace_unregister   },
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aDTraceFunctions); i++)
    {
        rc = RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, NULL, s_aDTraceFunctions[i].pszName, 
                                        (void **)s_aDTraceFunctions[i].ppfn);
        if (RT_FAILURE(rc))
        {
            SUPR0Printf("supdrvDTraceInit: Failed to resolved '%s' (rc=%Rrc, i=%u).\n", s_aDTraceFunctions[i].pszName, rc, i);
            break;
        }
    }

    RTR0DbgKrnlInfoRelease(hKrnlInfo);
    if (RT_FAILURE(rc))
        return NULL;
#endif

    return &g_supdrvDTraceReg;
}

#ifndef VBOX_WITH_NATIVE_DTRACE_R0DRV
# error "VBOX_WITH_NATIVE_DTRACE_R0DRV is not defined as it should"
#endif

