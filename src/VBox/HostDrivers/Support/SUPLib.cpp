/* $Id$ */
/** @file
 * VirtualBox Support Library - Common code.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

/** @page   pg_sup          SUP - The Support Library
 *
 * The support library is responsible for providing facilities to load
 * VMM Host Ring-0 code, to call Host VMM Ring-0 code from Ring-3 Host
 * code, to pin down physical memory, and more.
 *
 * The VMM Host Ring-0 code can be combined in the support driver if
 * permitted by kernel module license policies. If it is not combined
 * it will be externalized in a .r0 module that will be loaded using
 * the IPRT loader.
 *
 * The Ring-0 calling is done thru a generic SUP interface which will
 * transfer an argument set and call a predefined entry point in the Host
 * VMM Ring-0 code.
 *
 * See @ref grp_sup "SUP - Support APIs" for API details.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/ldr.h>
#include <iprt/asm.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/env.h>
#include <iprt/rand.h>
#include <iprt/x86.h>

#include "SUPLibInternal.h"
#include "SUPDrvIOC.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** R0 VMM module name. */
#define VMMR0_NAME      "VMMR0"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DECLCALLBACK(int) FNCALLVMMR0(PVMR0 pVMR0, unsigned uOperation, void *pvArg);
typedef FNCALLVMMR0 *PFNCALLVMMR0;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Init counter. */
static uint32_t                 g_cInits = 0;
/** Whether we've been preinitied. */
static bool                     g_fPreInited = false;
/** The SUPLib instance data.
 * Well, at least parts of it, specifically the parts that are being handed over
 * via the pre-init mechanism from the hardened executable stub.  */
SUPLIBDATA                      g_supLibData =
{
    SUP_HDEVICE_NIL
#if   defined(RT_OS_DARWIN)
    , NULL
#elif defined(RT_OS_LINUX)
    , false
#endif
};

/** Pointer to the Global Information Page.
 *
 * This pointer is valid as long as SUPLib has a open session. Anyone using
 * the page must treat this pointer as highly volatile and not trust it beyond
 * one transaction.
 *
 * @todo This will probably deserve it's own session or some other good solution...
 */
DECLEXPORT(PSUPGLOBALINFOPAGE)  g_pSUPGlobalInfoPage;
/** Address of the ring-0 mapping of the GIP. */
static PSUPGLOBALINFOPAGE       g_pSUPGlobalInfoPageR0;
/** The physical address of the GIP. */
static RTHCPHYS                 g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS;

/** The negotiated cookie. */
uint32_t                        g_u32Cookie = 0;
/** The negotiated session cookie. */
uint32_t                        g_u32SessionCookie;
/** Session handle. */
PSUPDRVSESSION                  g_pSession;
/** R0 SUP Functions used for resolving referenced to the SUPR0 module. */
static PSUPQUERYFUNCS           g_pFunctions;

/** VMMR0 Load Address. */
static RTR0PTR                  g_pvVMMR0 = NIL_RTR0PTR;
/** PAGE_ALLOC_EX sans kernel mapping support indicator. */
static bool                     g_fSupportsPageAllocNoKernel = true;
/** Fake mode indicator. (~0 at first, 0 or 1 after first test) */
static uint32_t                 g_u32FakeMode = ~0;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int supInitFake(PSUPDRVSESSION *ppSession);
static int supLoadModule(const char *pszFilename, const char *pszModule, const char *pszSrvReqHandler, void **ppvImageBase);
static DECLCALLBACK(int) supLoadModuleResolveImport(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser);


/** Touch a range of pages. */
DECLINLINE(void) supR3TouchPages(void *pv, size_t cPages)
{
    uint32_t volatile *pu32 = (uint32_t volatile *)pv;
    while (cPages-- > 0)
    {
        ASMAtomicCmpXchgU32(pu32, 0, 0);
        pu32 += PAGE_SIZE / sizeof(uint32_t);
    }
}


SUPR3DECL(int) SUPR3Install(void)
{
    return suplibOsInstall();
}


SUPR3DECL(int) SUPR3Uninstall(void)
{
    return suplibOsUninstall();
}


DECLEXPORT(int) supR3PreInit(PSUPPREINITDATA pPreInitData, uint32_t fFlags)
{
    /*
     * The caller is kind of trustworthy, just perform some basic checks.
     *
     * Note! Do not do any fancy stuff here because IPRT has NOT been
     *       initialized at this point.
     */
    if (!VALID_PTR(pPreInitData))
        return VERR_INVALID_POINTER;
    if (g_fPreInited || g_cInits > 0)
        return VERR_WRONG_ORDER;

    if (    pPreInitData->u32Magic != SUPPREINITDATA_MAGIC
        ||  pPreInitData->u32EndMagic != SUPPREINITDATA_MAGIC)
        return VERR_INVALID_MAGIC;
    if (    !(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV)
        &&  pPreInitData->Data.hDevice == SUP_HDEVICE_NIL)
        return VERR_INVALID_HANDLE;
    if (    (fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV)
        &&  pPreInitData->Data.hDevice != SUP_HDEVICE_NIL)
        return VERR_INVALID_PARAMETER;

    /*
     * Hand out the data.
     */
    int rc = supR3HardenedRecvPreInitData(pPreInitData);
    if (RT_FAILURE(rc))
        return rc;

    /** @todo This may need some small restructuring later, it doesn't quite work with a root service flag... */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
    {
        g_supLibData = pPreInitData->Data;
        g_fPreInited = true;
    }

    return VINF_SUCCESS;
}


SUPR3DECL(int) SUPR3Init(PSUPDRVSESSION *ppSession)
{
    /*
     * Perform some sanity checks.
     * (Got some trouble with compile time member alignment assertions.)
     */
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, u64NanoTSLastUpdateHz) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs) & 0x1f));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[1]) & 0x1f));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64NanoTS) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64TSC) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64CpuHz) & 0x7));

    /*
     * Check if already initialized.
     */
    if (ppSession)
        *ppSession = g_pSession;
    if (g_cInits++ > 0)
        return VINF_SUCCESS;

    /*
     * Check for fake mode.
     *
     * Fake mode is used when we're doing smoke testing and debugging.
     * It's also useful on platforms where we haven't root access or which
     * we haven't ported the support driver to.
     */
    if (g_u32FakeMode == ~0U)
    {
        const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
        if (psz && !strcmp(psz, "fake"))
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 1, ~0U);
        else
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 0, ~0U);
    }
    if (RT_UNLIKELY(g_u32FakeMode))
        return supInitFake(ppSession);

    /*
     * Open the support driver.
     */
    int rc = suplibOsInit(&g_supLibData, g_fPreInited);
    if (RT_SUCCESS(rc))
    {
        /*
         * Negotiate the cookie.
         */
        SUPCOOKIE CookieReq;
        memset(&CookieReq, 0xff, sizeof(CookieReq));
        CookieReq.Hdr.u32Cookie = SUPCOOKIE_INITIAL_COOKIE;
        CookieReq.Hdr.u32SessionCookie = RTRandU32();
        CookieReq.Hdr.cbIn = SUP_IOCTL_COOKIE_SIZE_IN;
        CookieReq.Hdr.cbOut = SUP_IOCTL_COOKIE_SIZE_OUT;
        CookieReq.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        CookieReq.Hdr.rc = VERR_INTERNAL_ERROR;
        strcpy(CookieReq.u.In.szMagic, SUPCOOKIE_MAGIC);
        CookieReq.u.In.u32ReqVersion = SUPDRV_IOC_VERSION;
        const uint32_t uMinVersion = (SUPDRV_IOC_VERSION & 0xffff0000) == 0x00190000
                                   ? 0x00190002
                                   : SUPDRV_IOC_VERSION & 0xffff0000;
        CookieReq.u.In.u32MinVersion = uMinVersion;
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_COOKIE, &CookieReq, SUP_IOCTL_COOKIE_SIZE);
        if (    RT_SUCCESS(rc)
            &&  RT_SUCCESS(CookieReq.Hdr.rc))
        {
            if (    (CookieReq.u.Out.u32SessionVersion & 0xffff0000) == (SUPDRV_IOC_VERSION & 0xffff0000)
                &&  CookieReq.u.Out.u32SessionVersion >= uMinVersion)
            {
                /*
                 * Query the functions.
                 */
                PSUPQUERYFUNCS pFuncsReq = (PSUPQUERYFUNCS)RTMemAllocZ(SUP_IOCTL_QUERY_FUNCS_SIZE(CookieReq.u.Out.cFunctions));
                if (pFuncsReq)
                {
                    pFuncsReq->Hdr.u32Cookie            = CookieReq.u.Out.u32Cookie;
                    pFuncsReq->Hdr.u32SessionCookie     = CookieReq.u.Out.u32SessionCookie;
                    pFuncsReq->Hdr.cbIn                 = SUP_IOCTL_QUERY_FUNCS_SIZE_IN;
                    pFuncsReq->Hdr.cbOut                = SUP_IOCTL_QUERY_FUNCS_SIZE_OUT(CookieReq.u.Out.cFunctions);
                    pFuncsReq->Hdr.fFlags               = SUPREQHDR_FLAGS_DEFAULT;
                    pFuncsReq->Hdr.rc                   = VERR_INTERNAL_ERROR;
                    rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_QUERY_FUNCS(CookieReq.u.Out.cFunctions), pFuncsReq, SUP_IOCTL_QUERY_FUNCS_SIZE(CookieReq.u.Out.cFunctions));
                    if (RT_SUCCESS(rc))
                        rc = pFuncsReq->Hdr.rc;
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Map the GIP into userspace.
                         */
                        Assert(!g_pSUPGlobalInfoPage);
                        SUPGIPMAP GipMapReq;
                        GipMapReq.Hdr.u32Cookie         = CookieReq.u.Out.u32Cookie;
                        GipMapReq.Hdr.u32SessionCookie  = CookieReq.u.Out.u32SessionCookie;
                        GipMapReq.Hdr.cbIn              = SUP_IOCTL_GIP_MAP_SIZE_IN;
                        GipMapReq.Hdr.cbOut             = SUP_IOCTL_GIP_MAP_SIZE_OUT;
                        GipMapReq.Hdr.fFlags            = SUPREQHDR_FLAGS_DEFAULT;
                        GipMapReq.Hdr.rc                = VERR_INTERNAL_ERROR;
                        GipMapReq.u.Out.HCPhysGip       = NIL_RTHCPHYS;
                        GipMapReq.u.Out.pGipR0          = NIL_RTR0PTR;
                        GipMapReq.u.Out.pGipR3          = NULL;
                        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_GIP_MAP, &GipMapReq, SUP_IOCTL_GIP_MAP_SIZE);
                        if (RT_SUCCESS(rc))
                            rc = GipMapReq.Hdr.rc;
                        if (RT_SUCCESS(rc))
                        {
                            AssertRelease(GipMapReq.u.Out.pGipR3->u32Magic == SUPGLOBALINFOPAGE_MAGIC);
                            AssertRelease(GipMapReq.u.Out.pGipR3->u32Version >= SUPGLOBALINFOPAGE_VERSION);

                            /*
                             * Set the globals and return success.
                             */
                            ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, GipMapReq.u.Out.HCPhysGip);
                            ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, GipMapReq.u.Out.pGipR3, NULL);
                            ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, (void *)GipMapReq.u.Out.pGipR0, NULL);

                            g_u32Cookie         = CookieReq.u.Out.u32Cookie;
                            g_u32SessionCookie  = CookieReq.u.Out.u32SessionCookie;
                            g_pSession          = CookieReq.u.Out.pSession;
                            g_pFunctions        = pFuncsReq;
                            if (ppSession)
                                *ppSession = CookieReq.u.Out.pSession;
                            return VINF_SUCCESS;
                        }
                    }

                    /* bailout */
                    RTMemFree(pFuncsReq);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
            {
                LogRel(("Support driver version mismatch: SessionVersion=%#x DriverVersion=%#x ClientVersion=%#x MinVersion=%#x\n",
                        CookieReq.u.Out.u32SessionVersion, CookieReq.u.Out.u32DriverVersion, SUPDRV_IOC_VERSION, uMinVersion));
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
        }
        else
        {
            if (RT_SUCCESS(rc))
            {
                rc = CookieReq.Hdr.rc;
                LogRel(("Support driver version mismatch: DriverVersion=%#x ClientVersion=%#x rc=%Rrc\n",
                        CookieReq.u.Out.u32DriverVersion, SUPDRV_IOC_VERSION, rc));
                if (rc != VERR_VM_DRIVER_VERSION_MISMATCH)
                    rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
            else
            {
                /* for pre 0x00060000 drivers */
                LogRel(("Support driver version mismatch: DriverVersion=too-old ClientVersion=%#x\n", SUPDRV_IOC_VERSION));
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
        }

        suplibOsTerm(&g_supLibData);
    }
    g_cInits--;

    return rc;
}

/**
 * Fake mode init.
 */
static int supInitFake(PSUPDRVSESSION *ppSession)
{
    Log(("SUP: Fake mode!\n"));
    static const SUPFUNC s_aFakeFunctions[] =
    {
        /* name                                     function */
        { "SUPR0AbsIs64bit",                        0 },
        { "SUPR0Abs64bitKernelCS",                  0 },
        { "SUPR0Abs64bitKernelSS",                  0 },
        { "SUPR0Abs64bitKernelDS",                  0 },
        { "SUPR0AbsKernelCS",                       8 },
        { "SUPR0AbsKernelSS",                       16 },
        { "SUPR0AbsKernelDS",                       16 },
        { "SUPR0AbsKernelES",                       16 },
        { "SUPR0AbsKernelFS",                       24 },
        { "SUPR0AbsKernelGS",                       32 },
        { "SUPR0ComponentRegisterFactory",          0xefeefffd },
        { "SUPR0ComponentDeregisterFactory",        0xefeefffe },
        { "SUPR0ComponentQueryFactory",             0xefeeffff },
        { "SUPR0ObjRegister",                       0xefef0000 },
        { "SUPR0ObjAddRef",                         0xefef0001 },
        { "SUPR0ObjAddRefEx",                       0xefef0001 },
        { "SUPR0ObjRelease",                        0xefef0002 },
        { "SUPR0ObjVerifyAccess",                   0xefef0003 },
        { "SUPR0LockMem",                           0xefef0004 },
        { "SUPR0UnlockMem",                         0xefef0005 },
        { "SUPR0ContAlloc",                         0xefef0006 },
        { "SUPR0ContFree",                          0xefef0007 },
        { "SUPR0MemAlloc",                          0xefef0008 },
        { "SUPR0MemGetPhys",                        0xefef0009 },
        { "SUPR0MemFree",                           0xefef000a },
        { "SUPR0Printf",                            0xefef000b },
        { "SUPR0GetPagingMode",                     0xefef000c },
        { "SUPR0EnableVTx",                         0xefef000e },
        { "RTMemAlloc",                             0xefef000f },
        { "RTMemAllocZ",                            0xefef0010 },
        { "RTMemFree",                              0xefef0011 },
        { "RTR0MemObjAddress",                      0xefef0012 },
        { "RTR0MemObjAddressR3",                    0xefef0013 },
        { "RTR0MemObjAllocPage",                    0xefef0014 },
        { "RTR0MemObjAllocPhysNC",                  0xefef0015 },
        { "RTR0MemObjAllocLow",                     0xefef0016 },
        { "RTR0MemObjEnterPhys",                    0xefef0017 },
        { "RTR0MemObjFree",                         0xefef0018 },
        { "RTR0MemObjGetPagePhysAddr",              0xefef0019 },
        { "RTR0MemObjMapUser",                      0xefef001a },
        { "RTR0MemObjMapKernel",                    0xefef001b },
        { "RTR0MemObjMapKernelEx",                  0xefef001c },
        { "RTMpGetArraySize",                       0xefef001c },
        { "RTProcSelf",                             0xefef001d },
        { "RTR0ProcHandleSelf",                     0xefef001e },
        { "RTSemEventCreate",                       0xefef001f },
        { "RTSemEventSignal",                       0xefef0020 },
        { "RTSemEventWait",                         0xefef0021 },
        { "RTSemEventWaitNoResume",                 0xefef0022 },
        { "RTSemEventDestroy",                      0xefef0023 },
        { "RTSemEventMultiCreate",                  0xefef0024 },
        { "RTSemEventMultiSignal",                  0xefef0025 },
        { "RTSemEventMultiReset",                   0xefef0026 },
        { "RTSemEventMultiWait",                    0xefef0027 },
        { "RTSemEventMultiWaitNoResume",            0xefef0028 },
        { "RTSemEventMultiDestroy",                 0xefef0029 },
        { "RTSemFastMutexCreate",                   0xefef002a },
        { "RTSemFastMutexDestroy",                  0xefef002b },
        { "RTSemFastMutexRequest",                  0xefef002c },
        { "RTSemFastMutexRelease",                  0xefef002d },
        { "RTSpinlockCreate",                       0xefef002e },
        { "RTSpinlockDestroy",                      0xefef002f },
        { "RTSpinlockAcquire",                      0xefef0030 },
        { "RTSpinlockRelease",                      0xefef0031 },
        { "RTSpinlockAcquireNoInts",                0xefef0032 },
        { "RTSpinlockReleaseNoInts",                0xefef0033 },
        { "RTTimeNanoTS",                           0xefef0034 },
        { "RTTimeMillieTS",                         0xefef0035 },
        { "RTTimeSystemNanoTS",                     0xefef0036 },
        { "RTTimeSystemMillieTS",                   0xefef0037 },
        { "RTThreadNativeSelf",                     0xefef0038 },
        { "RTThreadSleep",                          0xefef0039 },
        { "RTThreadYield",                          0xefef003a },
        { "RTTimerCreate",                          0xefef003a },
        { "RTTimerCreateEx",                        0xefef003a },
        { "RTTimerDestroy",                         0xefef003a },
        { "RTTimerStart",                           0xefef003a },
        { "RTTimerStop",                            0xefef003a },
        { "RTTimerChangeInterval",                  0xefef003a },
        { "RTTimerGetSystemGranularity",            0xefef003a },
        { "RTTimerRequestSystemGranularity",        0xefef003a },
        { "RTTimerReleaseSystemGranularity",        0xefef003a },
        { "RTTimerCanDoHighResolution",             0xefef003a },
        { "RTLogDefaultInstance",                   0xefef003b },
        { "RTLogRelDefaultInstance",                0xefef003c },
        { "RTLogSetDefaultInstanceThread",          0xefef003d },
        { "RTLogLogger",                            0xefef003e },
        { "RTLogLoggerEx",                          0xefef003f },
        { "RTLogLoggerExV",                         0xefef0040 },
        { "RTAssertMsg1",                           0xefef0041 },
        { "RTAssertMsg2",                           0xefef0042 },
        { "RTAssertMsg2V",                          0xefef0043 },
        { "SUPR0QueryVTCaps",                       0xefef0044 },
    };

    /* fake r0 functions. */
    g_pFunctions = (PSUPQUERYFUNCS)RTMemAllocZ(SUP_IOCTL_QUERY_FUNCS_SIZE(RT_ELEMENTS(s_aFakeFunctions)));
    if (g_pFunctions)
    {
        g_pFunctions->u.Out.cFunctions = RT_ELEMENTS(s_aFakeFunctions);
        memcpy(&g_pFunctions->u.Out.aFunctions[0], &s_aFakeFunctions[0], sizeof(s_aFakeFunctions));
        g_pSession = (PSUPDRVSESSION)(void *)g_pFunctions;
        if (ppSession)
            *ppSession = g_pSession;

        /* fake the GIP. */
        g_pSUPGlobalInfoPage = (PSUPGLOBALINFOPAGE)RTMemPageAllocZ(PAGE_SIZE);
        if (g_pSUPGlobalInfoPage)
        {
            g_pSUPGlobalInfoPageR0 = g_pSUPGlobalInfoPage;
            g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS & ~(RTHCPHYS)PAGE_OFFSET_MASK;
            /* the page is supposed to be invalid, so don't set the magic. */
            return VINF_SUCCESS;
        }

        RTMemFree(g_pFunctions);
        g_pFunctions = NULL;
    }
    return VERR_NO_MEMORY;
}


SUPR3DECL(int) SUPR3Term(bool fForced)
{
    /*
     * Verify state.
     */
    AssertMsg(g_cInits > 0, ("SUPR3Term() is called before SUPR3Init()!\n"));
    if (g_cInits == 0)
        return VERR_WRONG_ORDER;
    if (g_cInits == 1 || fForced)
    {
        /*
         * NULL the GIP pointer.
         */
        if (g_pSUPGlobalInfoPage)
        {
            ASMAtomicWriteNullPtr((void * volatile *)&g_pSUPGlobalInfoPage);
            ASMAtomicWriteNullPtr((void * volatile *)&g_pSUPGlobalInfoPageR0);
            ASMAtomicWriteSize(&g_HCPhysSUPGlobalInfoPage, NIL_RTHCPHYS);
            /* just a little safe guard against threads using the page. */
            RTThreadSleep(50);
        }

        /*
         * Close the support driver.
         */
        int rc = suplibOsTerm(&g_supLibData);
        if (rc)
            return rc;

        g_u32Cookie         = 0;
        g_u32SessionCookie  = 0;
        g_cInits            = 0;
    }
    else
        g_cInits--;

    return 0;
}


SUPR3DECL(SUPPAGINGMODE) SUPR3GetPagingMode(void)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
#ifdef RT_ARCH_AMD64
        return SUPPAGINGMODE_AMD64_GLOBAL_NX;
#else
        return SUPPAGINGMODE_32_BIT_GLOBAL;
#endif

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPGETPAGINGMODE Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_GET_PAGING_MODE_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_GET_PAGING_MODE_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_GET_PAGING_MODE, &Req, SUP_IOCTL_GET_PAGING_MODE_SIZE);
    if (    RT_FAILURE(rc)
        ||  RT_FAILURE(Req.Hdr.rc))
    {
        LogRel(("SUPR3GetPagingMode: %Rrc %Rrc\n", rc, Req.Hdr.rc));
        Req.u.Out.enmMode = SUPPAGINGMODE_INVALID;
    }

    return Req.u.Out.enmMode;
}


/**
 * For later.
 */
static int supCallVMMR0ExFake(PVMR0 pVMR0, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
    AssertMsgFailed(("%d\n", uOperation)); NOREF(pVMR0); NOREF(uOperation); NOREF(u64Arg); NOREF(pReqHdr);
    return VERR_NOT_SUPPORTED;
}


SUPR3DECL(int) SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu)
{
    NOREF(pVMR0);
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_RAW_RUN))
        return suplibOsIOCtlFast(&g_supLibData, SUP_IOCTL_FAST_DO_RAW_RUN, idCpu);
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_HWACC_RUN))
        return suplibOsIOCtlFast(&g_supLibData, SUP_IOCTL_FAST_DO_HWACC_RUN, idCpu);
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_NOP))
        return suplibOsIOCtlFast(&g_supLibData, SUP_IOCTL_FAST_DO_NOP, idCpu);

    AssertMsgFailed(("%#x\n", uOperation));
    return VERR_INTERNAL_ERROR;
}


SUPR3DECL(int) SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
    /*
     * The following operations don't belong here.
     */
    AssertMsgReturn(    uOperation != SUP_VMMR0_DO_RAW_RUN
                    &&  uOperation != SUP_VMMR0_DO_HWACC_RUN
                    &&  uOperation != SUP_VMMR0_DO_NOP,
                    ("%#x\n", uOperation),
                    VERR_INTERNAL_ERROR);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return supCallVMMR0ExFake(pVMR0, uOperation, u64Arg, pReqHdr);

    int rc;
    if (!pReqHdr)
    {
        /* no data. */
        SUPCALLVMMR0 Req;
        Req.Hdr.u32Cookie = g_u32Cookie;
        Req.Hdr.u32SessionCookie = g_u32SessionCookie;
        Req.Hdr.cbIn = SUP_IOCTL_CALL_VMMR0_SIZE_IN(0);
        Req.Hdr.cbOut = SUP_IOCTL_CALL_VMMR0_SIZE_OUT(0);
        Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        Req.Hdr.rc = VERR_INTERNAL_ERROR;
        Req.u.In.pVMR0 = pVMR0;
        Req.u.In.idCpu = idCpu;
        Req.u.In.uOperation = uOperation;
        Req.u.In.u64Arg = u64Arg;
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CALL_VMMR0(0), &Req, SUP_IOCTL_CALL_VMMR0_SIZE(0));
        if (RT_SUCCESS(rc))
            rc = Req.Hdr.rc;
    }
    else if (SUP_IOCTL_CALL_VMMR0_SIZE(pReqHdr->cbReq) < _4K) /* FreeBSD won't copy more than 4K. */
    {
        AssertPtrReturn(pReqHdr, VERR_INVALID_POINTER);
        AssertReturn(pReqHdr->u32Magic == SUPVMMR0REQHDR_MAGIC, VERR_INVALID_MAGIC);
        const size_t cbReq = pReqHdr->cbReq;

        PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)alloca(SUP_IOCTL_CALL_VMMR0_SIZE(cbReq));
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_CALL_VMMR0_SIZE_IN(cbReq);
        pReq->Hdr.cbOut = SUP_IOCTL_CALL_VMMR0_SIZE_OUT(cbReq);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        pReq->u.In.pVMR0 = pVMR0;
        pReq->u.In.idCpu = idCpu;
        pReq->u.In.uOperation = uOperation;
        pReq->u.In.u64Arg = u64Arg;
        memcpy(&pReq->abReqPkt[0], pReqHdr, cbReq);
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CALL_VMMR0(cbReq), pReq, SUP_IOCTL_CALL_VMMR0_SIZE(cbReq));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        memcpy(pReqHdr, &pReq->abReqPkt[0], cbReq);
    }
    else if (pReqHdr->cbReq <= _512K)
    {
        AssertPtrReturn(pReqHdr, VERR_INVALID_POINTER);
        AssertReturn(pReqHdr->u32Magic == SUPVMMR0REQHDR_MAGIC, VERR_INVALID_MAGIC);
        const size_t cbReq = pReqHdr->cbReq;

        PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)RTMemTmpAlloc(SUP_IOCTL_CALL_VMMR0_BIG_SIZE(cbReq));
        pReq->Hdr.u32Cookie         = g_u32Cookie;
        pReq->Hdr.u32SessionCookie  = g_u32SessionCookie;
        pReq->Hdr.cbIn              = SUP_IOCTL_CALL_VMMR0_BIG_SIZE_IN(cbReq);
        pReq->Hdr.cbOut             = SUP_IOCTL_CALL_VMMR0_BIG_SIZE_OUT(cbReq);
        pReq->Hdr.fFlags            = SUPREQHDR_FLAGS_DEFAULT;
        pReq->Hdr.rc                = VERR_INTERNAL_ERROR;
        pReq->u.In.pVMR0            = pVMR0;
        pReq->u.In.idCpu            = idCpu;
        pReq->u.In.uOperation       = uOperation;
        pReq->u.In.u64Arg           = u64Arg;
        memcpy(&pReq->abReqPkt[0], pReqHdr, cbReq);
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CALL_VMMR0_BIG, pReq, SUP_IOCTL_CALL_VMMR0_BIG_SIZE(cbReq));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        memcpy(pReqHdr, &pReq->abReqPkt[0], cbReq);
        RTMemTmpFree(pReq);
    }
    else
        AssertMsgFailedReturn(("cbReq=%#x\n", pReqHdr->cbReq), VERR_OUT_OF_RANGE);
    return rc;
}


SUPR3DECL(int) SUPR3CallVMMR0(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, void *pvArg)
{
    /*
     * The following operations don't belong here.
     */
    AssertMsgReturn(    uOperation != SUP_VMMR0_DO_RAW_RUN
                    &&  uOperation != SUP_VMMR0_DO_HWACC_RUN
                    &&  uOperation != SUP_VMMR0_DO_NOP,
                    ("%#x\n", uOperation),
                    VERR_INTERNAL_ERROR);
    return SUPR3CallVMMR0Ex(pVMR0, idCpu, uOperation, (uintptr_t)pvArg, NULL);
}


SUPR3DECL(int) SUPR3SetVMForFastIOCtl(PVMR0 pVMR0)
{
    if (RT_UNLIKELY(g_u32FakeMode))
        return VINF_SUCCESS;

    SUPSETVMFORFAST Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_SET_VM_FOR_FAST_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_SET_VM_FOR_FAST_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pVMR0 = pVMR0;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_SET_VM_FOR_FAST, &Req, SUP_IOCTL_SET_VM_FOR_FAST_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3CallR0Service(const char *pszService, size_t cchService, uint32_t uOperation, uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr)
{
    AssertReturn(cchService < RT_SIZEOFMEMB(SUPCALLSERVICE, u.In.szName), VERR_INVALID_PARAMETER);
    Assert(strlen(pszService) == cchService);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VERR_NOT_SUPPORTED;

    int rc;
    if (!pReqHdr)
    {
        /* no data. */
        SUPCALLSERVICE Req;
        Req.Hdr.u32Cookie = g_u32Cookie;
        Req.Hdr.u32SessionCookie = g_u32SessionCookie;
        Req.Hdr.cbIn = SUP_IOCTL_CALL_SERVICE_SIZE_IN(0);
        Req.Hdr.cbOut = SUP_IOCTL_CALL_SERVICE_SIZE_OUT(0);
        Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        Req.Hdr.rc = VERR_INTERNAL_ERROR;
        memcpy(Req.u.In.szName, pszService, cchService);
        Req.u.In.szName[cchService] = '\0';
        Req.u.In.uOperation = uOperation;
        Req.u.In.u64Arg = u64Arg;
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CALL_SERVICE(0), &Req, SUP_IOCTL_CALL_SERVICE_SIZE(0));
        if (RT_SUCCESS(rc))
            rc = Req.Hdr.rc;
    }
    else if (SUP_IOCTL_CALL_SERVICE_SIZE(pReqHdr->cbReq) < _4K) /* FreeBSD won't copy more than 4K. */
    {
        AssertPtrReturn(pReqHdr, VERR_INVALID_POINTER);
        AssertReturn(pReqHdr->u32Magic == SUPR0SERVICEREQHDR_MAGIC, VERR_INVALID_MAGIC);
        const size_t cbReq = pReqHdr->cbReq;

        PSUPCALLSERVICE pReq = (PSUPCALLSERVICE)alloca(SUP_IOCTL_CALL_SERVICE_SIZE(cbReq));
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_CALL_SERVICE_SIZE_IN(cbReq);
        pReq->Hdr.cbOut = SUP_IOCTL_CALL_SERVICE_SIZE_OUT(cbReq);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        memcpy(pReq->u.In.szName, pszService, cchService);
        pReq->u.In.szName[cchService] = '\0';
        pReq->u.In.uOperation = uOperation;
        pReq->u.In.u64Arg = u64Arg;
        memcpy(&pReq->abReqPkt[0], pReqHdr, cbReq);
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CALL_SERVICE(cbReq), pReq, SUP_IOCTL_CALL_SERVICE_SIZE(cbReq));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        memcpy(pReqHdr, &pReq->abReqPkt[0], cbReq);
    }
    else /** @todo may have to remove the size limits one this request... */
        AssertMsgFailedReturn(("cbReq=%#x\n", pReqHdr->cbReq), VERR_INTERNAL_ERROR);
    return rc;
}


/**
 * Worker for the SUPR3Logger* APIs.
 *
 * @returns VBox status code.
 * @param   enmWhich    Which logger.
 * @param   fWhat       What to do with the logger.
 * @param   pszFlags    The flags settings.
 * @param   pszGroups   The groups settings.
 * @param   pszDest     The destination specificier.
 */
static int supR3LoggerSettings(SUPLOGGER enmWhich, uint32_t fWhat, const char *pszFlags, const char *pszGroups, const char *pszDest)
{
    uint32_t const cchFlags  = pszFlags  ? (uint32_t)strlen(pszFlags)  : 0;
    uint32_t const cchGroups = pszGroups ? (uint32_t)strlen(pszGroups) : 0;
    uint32_t const cchDest   = pszDest   ? (uint32_t)strlen(pszDest)   : 0;
    uint32_t const cbStrTab  = cchFlags  + !!cchFlags
                             + cchGroups + !!cchGroups
                             + cchDest   + !!cchDest
                             + (!cchFlags && !cchGroups && !cchDest);

    PSUPLOGGERSETTINGS pReq  = (PSUPLOGGERSETTINGS)alloca(SUP_IOCTL_LOGGER_SETTINGS_SIZE(cbStrTab));
    pReq->Hdr.u32Cookie = g_u32Cookie;
    pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
    pReq->Hdr.cbIn  = SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(cbStrTab);
    pReq->Hdr.cbOut = SUP_IOCTL_LOGGER_SETTINGS_SIZE_OUT;
    pReq->Hdr.fFlags= SUPREQHDR_FLAGS_DEFAULT;
    pReq->Hdr.rc    = VERR_INTERNAL_ERROR;
    switch (enmWhich)
    {
        case SUPLOGGER_DEBUG:   pReq->u.In.fWhich = SUPLOGGERSETTINGS_WHICH_DEBUG; break;
        case SUPLOGGER_RELEASE: pReq->u.In.fWhich = SUPLOGGERSETTINGS_WHICH_RELEASE; break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    pReq->u.In.fWhat = fWhat;

    uint32_t off = 0;
    if (cchFlags)
    {
        pReq->u.In.offFlags = off;
        memcpy(&pReq->u.In.szStrings[off], pszFlags, cchFlags + 1);
        off += cchFlags + 1;
    }
    else
        pReq->u.In.offFlags = cbStrTab - 1;

    if (cchGroups)
    {
        pReq->u.In.offGroups = off;
        memcpy(&pReq->u.In.szStrings[off], pszGroups, cchGroups + 1);
        off += cchGroups + 1;
    }
    else
        pReq->u.In.offGroups = cbStrTab - 1;

    if (cchDest)
    {
        pReq->u.In.offDestination = off;
        memcpy(&pReq->u.In.szStrings[off], pszDest, cchDest + 1);
        off += cchDest + 1;
    }
    else
        pReq->u.In.offDestination = cbStrTab - 1;

    if (!off)
    {
        pReq->u.In.szStrings[0] = '\0';
        off++;
    }
    Assert(off == cbStrTab);
    Assert(pReq->u.In.szStrings[cbStrTab - 1] == '\0');


    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LOGGER_SETTINGS(cbStrTab), pReq, SUP_IOCTL_LOGGER_SETTINGS_SIZE(cbStrTab));
    if (RT_SUCCESS(rc))
        rc = pReq->Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3LoggerSettings(SUPLOGGER enmWhich, const char *pszFlags, const char *pszGroups, const char *pszDest)
{
    return supR3LoggerSettings(enmWhich, SUPLOGGERSETTINGS_WHAT_SETTINGS, pszFlags, pszGroups, pszDest);
}


SUPR3DECL(int) SUPR3LoggerCreate(SUPLOGGER enmWhich, const char *pszFlags, const char *pszGroups, const char *pszDest)
{
    return supR3LoggerSettings(enmWhich, SUPLOGGERSETTINGS_WHAT_CREATE, pszFlags, pszGroups, pszDest);
}


SUPR3DECL(int) SUPR3LoggerDestroy(SUPLOGGER enmWhich)
{
    return supR3LoggerSettings(enmWhich, SUPLOGGERSETTINGS_WHAT_DESTROY, NULL, NULL, NULL);
}


SUPR3DECL(int) SUPR3PageAlloc(size_t cPages, void **ppvPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);

    /*
     * Call OS specific worker.
     */
    return suplibOsPageAlloc(&g_supLibData, cPages, ppvPages);
}


SUPR3DECL(int) SUPR3PageFree(void *pvPages, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);

    /*
     * Call OS specific worker.
     */
    return suplibOsPageFree(&g_supLibData, pvPages, cPages);
}


/**
 * Locks down the physical memory backing a virtual memory
 * range in the current process.
 *
 * @returns VBox status code.
 * @param   pvStart         Start of virtual memory range.
 *                          Must be page aligned.
 * @param   cPages          Number of pages.
 * @param   paPages         Where to store the physical page addresses returned.
 *                          On entry this will point to an array of with cbMemory >> PAGE_SHIFT entries.
 */
SUPR3DECL(int) supR3PageLock(void *pvStart, size_t cPages, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));
    AssertPtr(paPages);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTHCPHYS    Phys = (uintptr_t)pvStart + PAGE_SIZE * 1024;
        size_t      iPage = cPages;
        while (iPage-- > 0)
            paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        return VINF_SUCCESS;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    int rc;
    PSUPPAGELOCK pReq = (PSUPPAGELOCK)RTMemTmpAllocZ(SUP_IOCTL_PAGE_LOCK_SIZE(cPages));
    if (RT_LIKELY(pReq))
    {
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_PAGE_LOCK_SIZE_IN;
        pReq->Hdr.cbOut = SUP_IOCTL_PAGE_LOCK_SIZE_OUT(cPages);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_MAGIC | SUPREQHDR_FLAGS_EXTRA_OUT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        pReq->u.In.pvR3 = pvStart;
        pReq->u.In.cPages = (uint32_t)cPages; AssertRelease(pReq->u.In.cPages == cPages);
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_LOCK, pReq, SUP_IOCTL_PAGE_LOCK_SIZE(cPages));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        if (RT_SUCCESS(rc))
        {
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                paPages[iPage].uReserved = 0;
                paPages[iPage].Phys = pReq->u.Out.aPages[iPage];
                Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
            }
        }
        RTMemTmpFree(pReq);
    }
    else
        rc = VERR_NO_TMP_MEMORY;

    return rc;
}


/**
 * Releases locked down pages.
 *
 * @returns VBox status code.
 * @param   pvStart         Start of virtual memory range previously locked
 *                          down by SUPPageLock().
 */
SUPR3DECL(int) supR3PageUnlock(void *pvStart)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPPAGEUNLOCK Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_PAGE_UNLOCK_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_PAGE_UNLOCK_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pvStart;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_UNLOCK, &Req, SUP_IOCTL_PAGE_UNLOCK_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


/**
 * Fallback for SUPR3PageAllocEx on systems where RTR0MemObjPhysAllocNC isn't
 * supported.
 */
static int supPagePageAllocNoKernelFallback(size_t cPages, void **ppvPages, PSUPPAGE paPages)
{
    int rc = suplibOsPageAlloc(&g_supLibData, cPages, ppvPages);
    if (RT_SUCCESS(rc))
    {
        if (!paPages)
            paPages = (PSUPPAGE)alloca(sizeof(paPages[0]) * cPages);
        rc = supR3PageLock(*ppvPages, cPages, paPages);
        if (RT_FAILURE(rc))
            suplibOsPageFree(&g_supLibData, *ppvPages, cPages);
    }
    return rc;
}


SUPR3DECL(int) SUPR3PageAllocEx(size_t cPages, uint32_t fFlags, void **ppvPages, PRTR0PTR pR0Ptr, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertPtrNullReturn(pR0Ptr, VERR_INVALID_POINTER);
    if (pR0Ptr)
        *pR0Ptr = NIL_RTR0PTR;
    AssertPtrNullReturn(paPages, VERR_INVALID_POINTER);
    AssertMsgReturn(cPages > 0 && cPages <= VBOX_MAX_ALLOC_PAGE_COUNT, ("cPages=%zu\n", cPages), VERR_PAGE_COUNT_OUT_OF_RANGE);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        void *pv = RTMemPageAllocZ(cPages * PAGE_SIZE);
        if (!pv)
            return VERR_NO_MEMORY;
        *ppvPages = pv;
        if (pR0Ptr)
            *pR0Ptr = (RTR0PTR)pv;
        if (paPages)
            for (size_t iPage = 0; iPage < cPages; iPage++)
            {
                paPages[iPage].uReserved = 0;
                paPages[iPage].Phys = (iPage + 4321) << PAGE_SHIFT;
                Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
            }
        return VINF_SUCCESS;
    }

    /*
     * Use fallback for non-R0 mapping?
     */
    if (    !pR0Ptr
        &&  !g_fSupportsPageAllocNoKernel)
        return supPagePageAllocNoKernelFallback(cPages, ppvPages, paPages);

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    int rc;
    PSUPPAGEALLOCEX pReq = (PSUPPAGEALLOCEX)RTMemTmpAllocZ(SUP_IOCTL_PAGE_ALLOC_EX_SIZE(cPages));
    if (pReq)
    {
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN;
        pReq->Hdr.cbOut = SUP_IOCTL_PAGE_ALLOC_EX_SIZE_OUT(cPages);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_MAGIC | SUPREQHDR_FLAGS_EXTRA_OUT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        pReq->u.In.cPages = (uint32_t)cPages; AssertRelease(pReq->u.In.cPages == cPages);
        pReq->u.In.fKernelMapping = pR0Ptr != NULL;
        pReq->u.In.fUserMapping = true;
        pReq->u.In.fReserved0 = false;
        pReq->u.In.fReserved1 = false;
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_ALLOC_EX, pReq, SUP_IOCTL_PAGE_ALLOC_EX_SIZE(cPages));
        if (RT_SUCCESS(rc))
        {
            rc = pReq->Hdr.rc;
            if (RT_SUCCESS(rc))
            {
                *ppvPages = pReq->u.Out.pvR3;
                if (pR0Ptr)
                    *pR0Ptr   = pReq->u.Out.pvR0;
                if (paPages)
                    for (size_t iPage = 0; iPage < cPages; iPage++)
                    {
                        paPages[iPage].uReserved = 0;
                        paPages[iPage].Phys = pReq->u.Out.aPages[iPage];
                        Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
                    }
#ifdef RT_OS_DARWIN /* HACK ALERT! */
                supR3TouchPages(pReq->u.Out.pvR3, cPages);
#endif
            }
            else if (   rc == VERR_NOT_SUPPORTED
                     && !pR0Ptr)
            {
                g_fSupportsPageAllocNoKernel = false;
                rc = supPagePageAllocNoKernelFallback(cPages, ppvPages, paPages);
            }
        }

        RTMemTmpFree(pReq);
    }
    else
        rc = VERR_NO_TMP_MEMORY;
    return rc;

}


SUPR3DECL(int) SUPR3PageMapKernel(void *pvR3, uint32_t off, uint32_t cb, uint32_t fFlags, PRTR0PTR pR0Ptr)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvR3, VERR_INVALID_POINTER);
    AssertPtrReturn(pR0Ptr, VERR_INVALID_POINTER);
    Assert(!(off & PAGE_OFFSET_MASK));
    Assert(!(cb & PAGE_OFFSET_MASK) && cb);
    Assert(!fFlags);
    *pR0Ptr = NIL_RTR0PTR;

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VERR_NOT_SUPPORTED;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPPAGEMAPKERNEL Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_PAGE_MAP_KERNEL_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_PAGE_MAP_KERNEL_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pvR3;
    Req.u.In.offSub = off;
    Req.u.In.cbSub = cb;
    Req.u.In.fFlags = fFlags;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_MAP_KERNEL, &Req, SUP_IOCTL_PAGE_MAP_KERNEL_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    if (RT_SUCCESS(rc))
        *pR0Ptr = Req.u.Out.pvR0;
    return rc;
}


SUPR3DECL(int) SUPR3PageProtect(void *pvR3, RTR0PTR R0Ptr, uint32_t off, uint32_t cb, uint32_t fProt)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvR3, VERR_INVALID_POINTER);
    Assert(!(off & PAGE_OFFSET_MASK));
    Assert(!(cb & PAGE_OFFSET_MASK) && cb);
    AssertReturn(!(fProt & ~(RTMEM_PROT_NONE | RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC)), VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return RTMemProtect((uint8_t *)pvR3 + off, cb, fProt);

    /*
     * Some OSes can do this from ring-3, so try that before we
     * issue the IOCtl to the SUPDRV kernel module.
     * (Yea, this isn't very nice, but just try get the job done for now.)
     */
#if !defined(RT_OS_SOLARIS)
    RTMemProtect((uint8_t *)pvR3 + off, cb, fProt);
#endif

    SUPPAGEPROTECT Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_PAGE_PROTECT_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_PAGE_PROTECT_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pvR3;
    Req.u.In.pvR0 = R0Ptr;
    Req.u.In.offSub = off;
    Req.u.In.cbSub = cb;
    Req.u.In.fProt = fProt;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_PROTECT, &Req, SUP_IOCTL_PAGE_PROTECT_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3PageFreeEx(void *pvPages, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pvPages, cPages * PAGE_SIZE);
        return VINF_SUCCESS;
    }

    /*
     * Try normal free first, then if it fails check if we're using the fallback
     * for the allocations without kernel mappings and attempt unlocking it.
     */
    NOREF(cPages);
    SUPPAGEFREE Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_PAGE_FREE_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_PAGE_FREE_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pvPages;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_PAGE_FREE, &Req, SUP_IOCTL_PAGE_FREE_SIZE);
    if (RT_SUCCESS(rc))
    {
        rc = Req.Hdr.rc;
        if (    rc == VERR_INVALID_PARAMETER
            &&  !g_fSupportsPageAllocNoKernel)
        {
            int rc2 = supR3PageUnlock(pvPages);
            if (RT_SUCCESS(rc2))
                rc = suplibOsPageFree(&g_supLibData, pvPages, cPages);
        }
    }
    return rc;
}


SUPR3DECL(void *) SUPR3ContAlloc(size_t cPages, PRTR0PTR pR0Ptr, PRTHCPHYS pHCPhys)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pHCPhys, NULL);
    *pHCPhys = NIL_RTHCPHYS;
    AssertPtrNullReturn(pR0Ptr, NULL);
    if (pR0Ptr)
        *pR0Ptr = NIL_RTR0PTR;
    AssertPtrNullReturn(pHCPhys, NULL);
    AssertMsgReturn(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages), NULL);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        void *pv = RTMemPageAllocZ(cPages * PAGE_SIZE);
        if (pR0Ptr)
            *pR0Ptr = (RTR0PTR)pv;
        if (pHCPhys)
            *pHCPhys = (uintptr_t)pv + (PAGE_SHIFT * 1024);
        return pv;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTALLOC Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_CONT_ALLOC_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_CONT_ALLOC_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.cPages = (uint32_t)cPages;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CONT_ALLOC, &Req, SUP_IOCTL_CONT_ALLOC_SIZE);
    if (    RT_SUCCESS(rc)
        &&  RT_SUCCESS(Req.Hdr.rc))
    {
        *pHCPhys = Req.u.Out.HCPhys;
        if (pR0Ptr)
            *pR0Ptr = Req.u.Out.pvR0;
#ifdef RT_OS_DARWIN /* HACK ALERT! */
        supR3TouchPages(Req.u.Out.pvR3, cPages);
#endif
        return Req.u.Out.pvR3;
    }

    return NULL;
}


SUPR3DECL(int) SUPR3ContFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    if (!pv)
        return VINF_SUCCESS;
    AssertPtrReturn(pv, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pv, cPages * PAGE_SIZE);
        return VINF_SUCCESS;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTFREE Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_CONT_FREE_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_CONT_FREE_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pv;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_CONT_FREE, &Req, SUP_IOCTL_CONT_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3LowAlloc(size_t cPages, void **ppvPages, PRTR0PTR ppvPagesR0, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertPtrReturn(paPages, VERR_INVALID_POINTER);
    AssertMsgReturn(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages), VERR_PAGE_COUNT_OUT_OF_RANGE);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        *ppvPages = RTMemPageAllocZ((size_t)cPages * PAGE_SIZE);
        if (!*ppvPages)
            return VERR_NO_LOW_MEMORY;

        /* fake physical addresses. */
        RTHCPHYS    Phys = (uintptr_t)*ppvPages + PAGE_SIZE * 1024;
        size_t      iPage = cPages;
        while (iPage-- > 0)
            paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        return VINF_SUCCESS;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    int rc;
    PSUPLOWALLOC pReq = (PSUPLOWALLOC)RTMemTmpAllocZ(SUP_IOCTL_LOW_ALLOC_SIZE(cPages));
    if (pReq)
    {
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_LOW_ALLOC_SIZE_IN;
        pReq->Hdr.cbOut = SUP_IOCTL_LOW_ALLOC_SIZE_OUT(cPages);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_MAGIC | SUPREQHDR_FLAGS_EXTRA_OUT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        pReq->u.In.cPages = (uint32_t)cPages; AssertRelease(pReq->u.In.cPages == cPages);
        rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LOW_ALLOC, pReq, SUP_IOCTL_LOW_ALLOC_SIZE(cPages));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        if (RT_SUCCESS(rc))
        {
            *ppvPages = pReq->u.Out.pvR3;
            if (ppvPagesR0)
                *ppvPagesR0 = pReq->u.Out.pvR0;
            if (paPages)
                for (size_t iPage = 0; iPage < cPages; iPage++)
                {
                    paPages[iPage].uReserved = 0;
                    paPages[iPage].Phys = pReq->u.Out.aPages[iPage];
                    Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
                    Assert(paPages[iPage].Phys <= UINT32_C(0xfffff000));
                }
#ifdef RT_OS_DARWIN /* HACK ALERT! */
            supR3TouchPages(pReq->u.Out.pvR3, cPages);
#endif
        }
        RTMemTmpFree(pReq);
    }
    else
        rc = VERR_NO_TMP_MEMORY;

    return rc;
}


SUPR3DECL(int) SUPR3LowFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    if (!pv)
        return VINF_SUCCESS;
    AssertPtrReturn(pv, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pv, cPages * PAGE_SIZE);
        return VINF_SUCCESS;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTFREE Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_LOW_FREE_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_LOW_FREE_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvR3 = pv;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LOW_FREE, &Req, SUP_IOCTL_LOW_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3HardenedVerifyFile(const char *pszFilename, const char *pszMsg, PRTFILE phFile)
{
    /*
     * Quick input validation.
     */
    AssertPtr(pszFilename);
    AssertPtr(pszMsg);
    AssertReturn(!phFile, VERR_NOT_IMPLEMENTED); /** @todo Implement this. The deal is that we make sure the
                                                     file is the same we verified after opening it. */

    /*
     * Only do the actual check in hardened builds.
     */
#ifdef VBOX_WITH_HARDENING
    int rc = supR3HardenedVerifyFixedFile(pszFilename, false /* fFatal */);
    if (RT_FAILURE(rc))
        LogRel(("SUPR3HardenedVerifyFile: %s: Verification of \"%s\" failed, rc=%Rrc\n", pszMsg, pszFilename, rc));
    return rc;
#else
    return VINF_SUCCESS;
#endif
}


SUPR3DECL(int) SUPR3HardenedVerifySelf(const char *pszArgv0, bool fInternal, PRTERRINFO pErrInfo)
{
    /*
     * Quick input validation.
     */
    AssertPtr(pszArgv0);
    RTErrInfoClear(pErrInfo);

    /*
     * Get the executable image path as we need it for all the tests here.
     */
    char szExecPath[RTPATH_MAX];
    if (!RTProcGetExecutablePath(szExecPath, sizeof(szExecPath)))
        return RTErrInfoSet(pErrInfo, VERR_INTERNAL_ERROR_2, "RTProcGetExecutablePath failed");

    int rc;
    if (fInternal)
    {
        /*
         * Internal applications must be launched directly without any PATH
         * searching involved.
         */
        if (RTPathCompare(pszArgv0, szExecPath) != 0)
            return RTErrInfoSetF(pErrInfo, VERR_SUPLIB_INVALID_ARGV0_INTERNAL,
                                 "argv[0] does not match the executable image path: '%s' != '%s'", pszArgv0, szExecPath);

        /*
         * Internal applications must reside in or under the
         * RTPathAppPrivateArch directory.
         */
        char szAppPrivateArch[RTPATH_MAX];
        rc = RTPathAppPrivateArch(szAppPrivateArch, sizeof(szAppPrivateArch));
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, VERR_SUPLIB_INVALID_ARGV0_INTERNAL,
                                 "RTPathAppPrivateArch failed with rc=%Rrc", rc);
        size_t cchAppPrivateArch = strlen(szAppPrivateArch);
        if (   cchAppPrivateArch >= strlen(szExecPath)
            || !RTPATH_IS_SLASH(szExecPath[cchAppPrivateArch]))
            return RTErrInfoSet(pErrInfo, VERR_SUPLIB_INVALID_INTERNAL_APP_DIR,
                                "Internal executable does reside under RTPathAppPrivateArch");
        szExecPath[cchAppPrivateArch] = '\0';
        if (RTPathCompare(szExecPath, szAppPrivateArch) != 0)
            return RTErrInfoSet(pErrInfo, VERR_SUPLIB_INVALID_INTERNAL_APP_DIR,
                                "Internal executable does reside under RTPathAppPrivateArch");
        szExecPath[cchAppPrivateArch] = RTPATH_SLASH;
    }

#ifdef VBOX_WITH_HARDENING
    /*
     * Verify that the image file and parent directories are sane.
     */
    rc = supR3HardenedVerifyFile(szExecPath, RTHCUINTPTR_MAX, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}


SUPR3DECL(int) SUPR3HardenedVerifyDir(const char *pszDirPath, bool fRecursive, bool fCheckFiles, PRTERRINFO pErrInfo)
{
    /*
     * Quick input validation
     */
    AssertPtr(pszDirPath);
    RTErrInfoClear(pErrInfo);

    /*
     * Only do the actual check in hardened builds.
     */
#ifdef VBOX_WITH_HARDENING
    int rc = supR3HardenedVerifyDir(pszDirPath, fRecursive, fCheckFiles, pErrInfo);
    if (RT_FAILURE(rc) && !RTErrInfoIsSet(pErrInfo))
        LogRel(("supR3HardenedVerifyDir: Verification of \"%s\" failed, rc=%Rrc\n", pszDirPath, rc));
    return rc;
#else
    NOREF(pszDirPath); NOREF(fRecursive); NOREF(fCheckFiles);
    return VINF_SUCCESS;
#endif
}


SUPR3DECL(int) SUPR3HardenedVerifyPlugIn(const char *pszFilename, PRTERRINFO pErrInfo)
{
    /*
     * Quick input validation
     */
    AssertPtr(pszFilename);
    RTErrInfoClear(pErrInfo);

    /*
     * Only do the actual check in hardened builds.
     */
#ifdef VBOX_WITH_HARDENING
    int rc = supR3HardenedVerifyFile(pszFilename, RTHCUINTPTR_MAX, pErrInfo);
    if (RT_FAILURE(rc) && !RTErrInfoIsSet(pErrInfo))
        LogRel(("supR3HardenedVerifyFile: Verification of \"%s\" failed, rc=%Rrc\n", pszFilename, rc));
    return rc;
#else
    return VINF_SUCCESS;
#endif
}


SUPR3DECL(int) SUPR3LoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase, PRTERRINFO pErrInfo)
{
    /*
     * Check that the module can be trusted.
     */
    int rc = SUPR3HardenedVerifyPlugIn(pszFilename, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        rc = supLoadModule(pszFilename, pszModule, NULL, ppvImageBase);
        if (RT_FAILURE(rc))
            RTErrInfoSetF(pErrInfo, rc, "supLoadModule returned %Rrc", rc);
    }
    return rc;
}


SUPR3DECL(int) SUPR3LoadServiceModule(const char *pszFilename, const char *pszModule,
                                      const char *pszSrvReqHandler, void **ppvImageBase)
{
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pszSrvReqHandler, VERR_INVALID_PARAMETER);

#ifdef VBOX_WITH_HARDENING
    /*
     * Check that the module can be trusted.
     */
    rc = supR3HardenedVerifyFixedFile(pszFilename, false /* fFatal */);
#endif
    if (RT_SUCCESS(rc))
        rc = supLoadModule(pszFilename, pszModule, pszSrvReqHandler, ppvImageBase);
    else
        LogRel(("SUPR3LoadServiceModule: Verification of \"%s\" failed, rc=%Rrc\n", rc));
    return rc;
}


/**
 * Resolve an external symbol during RTLdrGetBits().
 *
 * @returns VBox status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) supLoadModuleResolveImport(RTLDRMOD hLdrMod, const char *pszModule,
                                                    const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser)
{
    NOREF(hLdrMod); NOREF(pvUser); NOREF(uSymbol);
    AssertPtr(pValue);
    AssertPtr(pvUser);

    /*
     * Only SUPR0 and VMMR0.r0
     */
    if (    pszModule
        &&  *pszModule
        &&  strcmp(pszModule, "VBoxDrv.sys")
        &&  strcmp(pszModule, "VMMR0.r0"))
    {
        AssertMsgFailed(("%s is importing from %s! (expected 'SUPR0.dll' or 'VMMR0.r0', case-sensitive)\n", pvUser, pszModule));
        return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * No ordinals.
     */
    if (pszSymbol < (const char*)0x10000)
    {
        AssertMsgFailed(("%s is importing by ordinal (ord=%d)\n", pvUser, (int)(uintptr_t)pszSymbol));
        return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * Lookup symbol.
     */
    /** @todo is this actually used??? */
    /* skip the 64-bit ELF import prefix first. */
    if (!strncmp(pszSymbol, "SUPR0$", sizeof("SUPR0$") - 1))
        pszSymbol += sizeof("SUPR0$") - 1;

    /*
     * Check the VMMR0.r0 module if loaded.
     */
    /** @todo call the SUPR3LoadModule caller.... */
    /** @todo proper reference counting and such. */
    if (g_pvVMMR0 != NIL_RTR0PTR)
    {
        void *pvValue;
        if (!SUPR3GetSymbolR0((void *)g_pvVMMR0, pszSymbol, &pvValue))
        {
            *pValue = (uintptr_t)pvValue;
            return VINF_SUCCESS;
        }
    }

    /* iterate the function table. */
    int c = g_pFunctions->u.Out.cFunctions;
    PSUPFUNC pFunc = &g_pFunctions->u.Out.aFunctions[0];
    while (c-- > 0)
    {
        if (!strcmp(pFunc->szName, pszSymbol))
        {
            *pValue = (uintptr_t)pFunc->pfn;
            return VINF_SUCCESS;
        }
        pFunc++;
    }

    /*
     * The GIP.
     */
    if (    pszSymbol
        &&  g_pSUPGlobalInfoPage
        &&  g_pSUPGlobalInfoPageR0
        &&  !strcmp(pszSymbol, "g_SUPGlobalInfoPage")
       )
    {
        *pValue = (uintptr_t)g_pSUPGlobalInfoPageR0;
        return VINF_SUCCESS;
    }

    /*
     * Symbols that are undefined by convention.
     */
#ifdef RT_OS_SOLARIS
    static const char * const s_apszConvSyms[] =
    {
        "", "mod_getctl",
        "", "mod_install",
        "", "mod_remove",
        "", "mod_info",
        "", "mod_miscops",
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszConvSyms); i += 2)
    {
        if (   !RTStrCmp(s_apszConvSyms[i],     pszModule)
            && !RTStrCmp(s_apszConvSyms[i + 1], pszSymbol))
        {
            *pValue = ~(uintptr_t)0;
            return VINF_SUCCESS;
        }
    }
#endif

    /*
     * Despair.
     */
    c = g_pFunctions->u.Out.cFunctions;
    pFunc = &g_pFunctions->u.Out.aFunctions[0];
    while (c-- > 0)
    {
        RTAssertMsg2Weak("%d: %s\n", g_pFunctions->u.Out.cFunctions - c, pFunc->szName);
        pFunc++;
    }

    AssertLogRelMsgFailed(("%s is importing %s which we couldn't find\n", pvUser, pszSymbol));
    if (g_u32FakeMode)
    {
        *pValue = 0xdeadbeef;
        return VINF_SUCCESS;
    }
    return VERR_SYMBOL_NOT_FOUND;
}


/** Argument package for supLoadModuleCalcSizeCB. */
typedef struct SUPLDRCALCSIZEARGS
{
    size_t          cbStrings;
    uint32_t        cSymbols;
    size_t          cbImage;
} SUPLDRCALCSIZEARGS, *PSUPLDRCALCSIZEARGS;

/**
 * Callback used to calculate the image size.
 * @return VINF_SUCCESS
 */
static DECLCALLBACK(int) supLoadModuleCalcSizeCB(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PSUPLDRCALCSIZEARGS pArgs = (PSUPLDRCALCSIZEARGS)pvUser;
    if (    pszSymbol != NULL
        &&  *pszSymbol
        &&  Value <= pArgs->cbImage)
    {
        pArgs->cSymbols++;
        pArgs->cbStrings += strlen(pszSymbol) + 1;
    }
    NOREF(hLdrMod); NOREF(uSymbol);
    return VINF_SUCCESS;
}


/** Argument package for supLoadModuleCreateTabsCB. */
typedef struct SUPLDRCREATETABSARGS
{
    size_t          cbImage;
    PSUPLDRSYM      pSym;
    char           *pszBase;
    char           *psz;
} SUPLDRCREATETABSARGS, *PSUPLDRCREATETABSARGS;

/**
 * Callback used to calculate the image size.
 * @return VINF_SUCCESS
 */
static DECLCALLBACK(int) supLoadModuleCreateTabsCB(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PSUPLDRCREATETABSARGS pArgs = (PSUPLDRCREATETABSARGS)pvUser;
    if (    pszSymbol != NULL
        &&  *pszSymbol
        &&  Value <= pArgs->cbImage)
    {
        pArgs->pSym->offSymbol = (uint32_t)Value;
        pArgs->pSym->offName = pArgs->psz - pArgs->pszBase;
        pArgs->pSym++;

        size_t cbCopy = strlen(pszSymbol) + 1;
        memcpy(pArgs->psz, pszSymbol, cbCopy);
        pArgs->psz += cbCopy;
    }
    NOREF(hLdrMod); NOREF(uSymbol);
    return VINF_SUCCESS;
}


/**
 * Worker for SUPR3LoadModule().
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the VMMR0 image file
 */
static int supLoadModule(const char *pszFilename, const char *pszModule, const char *pszSrvReqHandler, void **ppvImageBase)
{
    int rc;

    /*
     * Validate input.
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszModule, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvImageBase, VERR_INVALID_PARAMETER);
    AssertReturn(strlen(pszModule) < RT_SIZEOFMEMB(SUPLDROPEN, u.In.szName), VERR_FILENAME_TOO_LONG);
    char szAbsFilename[RT_SIZEOFMEMB(SUPLDROPEN, u.In.szFilename)];
    rc = RTPathAbs(pszFilename, szAbsFilename, sizeof(szAbsFilename));
    if (RT_FAILURE(rc))
        return rc;
    pszFilename = szAbsFilename;

    const bool fIsVMMR0 = !strcmp(pszModule, "VMMR0.r0");
    AssertReturn(!pszSrvReqHandler || !fIsVMMR0, VERR_INTERNAL_ERROR);
    *ppvImageBase = NULL;

    /*
     * Open image file and figure its size.
     */
    RTLDRMOD hLdrMod;
    rc = RTLdrOpen(pszFilename, 0, RTLDRARCH_HOST, &hLdrMod);
    if (!RT_SUCCESS(rc))
        return rc;

    SUPLDRCALCSIZEARGS CalcArgs;
    CalcArgs.cbStrings = 0;
    CalcArgs.cSymbols = 0;
    CalcArgs.cbImage = RTLdrSize(hLdrMod);
    rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCalcSizeCB, &CalcArgs);
    if (RT_SUCCESS(rc))
    {
        const uint32_t  offSymTab = RT_ALIGN_32(CalcArgs.cbImage, 8);
        const uint32_t  offStrTab = offSymTab + CalcArgs.cSymbols * sizeof(SUPLDRSYM);
        const uint32_t  cbImageWithTabs = RT_ALIGN_32(offStrTab + CalcArgs.cbStrings, 8);

        /*
         * Open the R0 image.
         */
        SUPLDROPEN OpenReq;
        OpenReq.Hdr.u32Cookie = g_u32Cookie;
        OpenReq.Hdr.u32SessionCookie = g_u32SessionCookie;
        OpenReq.Hdr.cbIn = SUP_IOCTL_LDR_OPEN_SIZE_IN;
        OpenReq.Hdr.cbOut = SUP_IOCTL_LDR_OPEN_SIZE_OUT;
        OpenReq.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        OpenReq.Hdr.rc = VERR_INTERNAL_ERROR;
        OpenReq.u.In.cbImageWithTabs = cbImageWithTabs;
        OpenReq.u.In.cbImageBits = (uint32_t)CalcArgs.cbImage;
        strcpy(OpenReq.u.In.szName, pszModule);
        strcpy(OpenReq.u.In.szFilename, pszFilename);
        if (!g_u32FakeMode)
        {
            rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LDR_OPEN, &OpenReq, SUP_IOCTL_LDR_OPEN_SIZE);
            if (RT_SUCCESS(rc))
                rc = OpenReq.Hdr.rc;
        }
        else
        {
            OpenReq.u.Out.fNeedsLoading = true;
            OpenReq.u.Out.pvImageBase = 0xef423420;
        }
        *ppvImageBase = (void *)OpenReq.u.Out.pvImageBase;
        if (    RT_SUCCESS(rc)
            &&  OpenReq.u.Out.fNeedsLoading)
        {
            /*
             * We need to load it.
             * Allocate memory for the image bits.
             */
            PSUPLDRLOAD pLoadReq = (PSUPLDRLOAD)RTMemTmpAlloc(SUP_IOCTL_LDR_LOAD_SIZE(cbImageWithTabs));
            if (pLoadReq)
            {
                /*
                 * Get the image bits.
                 */
                rc = RTLdrGetBits(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase,
                                  supLoadModuleResolveImport, (void *)pszModule);

                if (RT_SUCCESS(rc))
                {
                    /*
                     * Get the entry points.
                     */
                    RTUINTPTR VMMR0EntryInt = 0;
                    RTUINTPTR VMMR0EntryFast = 0;
                    RTUINTPTR VMMR0EntryEx = 0;
                    RTUINTPTR SrvReqHandler = 0;
                    RTUINTPTR ModuleInit = 0;
                    RTUINTPTR ModuleTerm = 0;
                    if (fIsVMMR0)
                    {
                        rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryInt", &VMMR0EntryInt);
                        if (RT_SUCCESS(rc))
                            rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryFast", &VMMR0EntryFast);
                        if (RT_SUCCESS(rc))
                            rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryEx", &VMMR0EntryEx);
                    }
                    else if (pszSrvReqHandler)
                        rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, pszSrvReqHandler, &SrvReqHandler);
                    if (RT_SUCCESS(rc))
                    {
                        int rc2 = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "ModuleInit", &ModuleInit);
                        if (RT_FAILURE(rc2))
                            ModuleInit = 0;

                        rc2 = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.abImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "ModuleTerm", &ModuleTerm);
                        if (RT_FAILURE(rc2))
                            ModuleTerm = 0;
                    }
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create the symbol and string tables.
                         */
                        SUPLDRCREATETABSARGS CreateArgs;
                        CreateArgs.cbImage = CalcArgs.cbImage;
                        CreateArgs.pSym    = (PSUPLDRSYM)&pLoadReq->u.In.abImage[offSymTab];
                        CreateArgs.pszBase =     (char *)&pLoadReq->u.In.abImage[offStrTab];
                        CreateArgs.psz     = CreateArgs.pszBase;
                        rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCreateTabsCB, &CreateArgs);
                        if (RT_SUCCESS(rc))
                        {
                            AssertRelease((size_t)(CreateArgs.psz - CreateArgs.pszBase) <= CalcArgs.cbStrings);
                            AssertRelease((size_t)(CreateArgs.pSym - (PSUPLDRSYM)&pLoadReq->u.In.abImage[offSymTab]) <= CalcArgs.cSymbols);

                            /*
                             * Upload the image.
                             */
                            pLoadReq->Hdr.u32Cookie = g_u32Cookie;
                            pLoadReq->Hdr.u32SessionCookie = g_u32SessionCookie;
                            pLoadReq->Hdr.cbIn = SUP_IOCTL_LDR_LOAD_SIZE_IN(cbImageWithTabs);
                            pLoadReq->Hdr.cbOut = SUP_IOCTL_LDR_LOAD_SIZE_OUT;
                            pLoadReq->Hdr.fFlags = SUPREQHDR_FLAGS_MAGIC | SUPREQHDR_FLAGS_EXTRA_IN;
                            pLoadReq->Hdr.rc = VERR_INTERNAL_ERROR;

                            pLoadReq->u.In.pfnModuleInit              = (RTR0PTR)ModuleInit;
                            pLoadReq->u.In.pfnModuleTerm              = (RTR0PTR)ModuleTerm;
                            if (fIsVMMR0)
                            {
                                pLoadReq->u.In.eEPType                = SUPLDRLOADEP_VMMR0;
                                pLoadReq->u.In.EP.VMMR0.pvVMMR0       = OpenReq.u.Out.pvImageBase;
                                pLoadReq->u.In.EP.VMMR0.pvVMMR0EntryInt = (RTR0PTR)VMMR0EntryInt;
                                pLoadReq->u.In.EP.VMMR0.pvVMMR0EntryFast= (RTR0PTR)VMMR0EntryFast;
                                pLoadReq->u.In.EP.VMMR0.pvVMMR0EntryEx  = (RTR0PTR)VMMR0EntryEx;
                            }
                            else if (pszSrvReqHandler)
                            {
                                pLoadReq->u.In.eEPType                = SUPLDRLOADEP_SERVICE;
                                pLoadReq->u.In.EP.Service.pfnServiceReq = (RTR0PTR)SrvReqHandler;
                                pLoadReq->u.In.EP.Service.apvReserved[0] = NIL_RTR0PTR;
                                pLoadReq->u.In.EP.Service.apvReserved[1] = NIL_RTR0PTR;
                                pLoadReq->u.In.EP.Service.apvReserved[2] = NIL_RTR0PTR;
                            }
                            else
                                pLoadReq->u.In.eEPType                = SUPLDRLOADEP_NOTHING;
                            pLoadReq->u.In.offStrTab                  = offStrTab;
                            pLoadReq->u.In.cbStrTab                   = (uint32_t)CalcArgs.cbStrings;
                            AssertRelease(pLoadReq->u.In.cbStrTab == CalcArgs.cbStrings);
                            pLoadReq->u.In.cbImageBits                = (uint32_t)CalcArgs.cbImage;
                            pLoadReq->u.In.offSymbols                 = offSymTab;
                            pLoadReq->u.In.cSymbols                   = CalcArgs.cSymbols;
                            pLoadReq->u.In.cbImageWithTabs            = cbImageWithTabs;
                            pLoadReq->u.In.pvImageBase                = OpenReq.u.Out.pvImageBase;
                            if (!g_u32FakeMode)
                            {
                                rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LDR_LOAD, pLoadReq, SUP_IOCTL_LDR_LOAD_SIZE(cbImageWithTabs));
                                if (RT_SUCCESS(rc))
                                    rc = pLoadReq->Hdr.rc;
                            }
                            else
                                rc = VINF_SUCCESS;
                            if (    RT_SUCCESS(rc)
                                ||  rc == VERR_ALREADY_LOADED /* A competing process. */
                               )
                            {
                                LogRel(("SUP: Loaded %s (%s) at %#p - ModuleInit at %RTptr and ModuleTerm at %RTptr%s\n",
                                        pszModule, pszFilename, OpenReq.u.Out.pvImageBase, ModuleInit, ModuleTerm,
                                        OpenReq.u.Out.fNativeLoader ? " using the native ring-0 loader" : ""));
                                if (fIsVMMR0)
                                {
                                    g_pvVMMR0 = OpenReq.u.Out.pvImageBase;
                                    LogRel(("SUP: VMMR0EntryEx located at %RTptr, VMMR0EntryFast at %RTptr and VMMR0EntryInt at %RTptr\n",
                                            VMMR0EntryEx, VMMR0EntryFast, VMMR0EntryInt));
                                }
#ifdef RT_OS_WINDOWS
                                LogRel(("SUP: windbg> .reload /f %s=%#p\n", pszFilename, OpenReq.u.Out.pvImageBase));
#endif

                                RTMemTmpFree(pLoadReq);
                                RTLdrClose(hLdrMod);
                                return VINF_SUCCESS;
                            }
                        }
                    }
                }
                RTMemTmpFree(pLoadReq);
            }
            else
            {
                AssertMsgFailed(("failed to allocated %u bytes for SUPLDRLOAD_IN structure!\n", SUP_IOCTL_LDR_LOAD_SIZE(cbImageWithTabs)));
                rc = VERR_NO_TMP_MEMORY;
            }
        }
        else if (RT_SUCCESS(rc))
        {
            if (fIsVMMR0)
                g_pvVMMR0 = OpenReq.u.Out.pvImageBase;
            LogRel(("SUP: Opened %s (%s) at %#p.\n", pszModule, pszFilename, OpenReq.u.Out.pvImageBase,
                    OpenReq.u.Out.fNativeLoader ? " loaded by the native ring-0 loader" : ""));
#ifdef RT_OS_WINDOWS
            LogRel(("SUP: windbg> .reload /f %s=%#p\n", pszFilename, OpenReq.u.Out.pvImageBase));
#endif
        }
    }
    RTLdrClose(hLdrMod);
    return rc;
}


SUPR3DECL(int) SUPR3FreeModule(void *pvImageBase)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        g_pvVMMR0 = NIL_RTR0PTR;
        return VINF_SUCCESS;
    }

    /*
     * Free the requested module.
     */
    SUPLDRFREE Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_LDR_FREE_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_LDR_FREE_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvImageBase = (RTR0PTR)pvImageBase;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LDR_FREE, &Req, SUP_IOCTL_LDR_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    if (    RT_SUCCESS(rc)
        &&  (RTR0PTR)pvImageBase == g_pvVMMR0)
        g_pvVMMR0 = NIL_RTR0PTR;
    return rc;
}


SUPR3DECL(int) SUPR3GetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue)
{
    *ppvValue = NULL;

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        *ppvValue = (void *)(uintptr_t)0xdeadf00d;
        return VINF_SUCCESS;
    }

    /*
     * Do ioctl.
     */
    SUPLDRGETSYMBOL Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_LDR_GET_SYMBOL_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_LDR_GET_SYMBOL_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.In.pvImageBase = (RTR0PTR)pvImageBase;
    size_t cchSymbol = strlen(pszSymbol);
    if (cchSymbol >= sizeof(Req.u.In.szSymbol))
        return VERR_SYMBOL_NOT_FOUND;
    memcpy(Req.u.In.szSymbol, pszSymbol, cchSymbol + 1);
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_LDR_GET_SYMBOL, &Req, SUP_IOCTL_LDR_GET_SYMBOL_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    if (RT_SUCCESS(rc))
        *ppvValue = (void *)Req.u.Out.pvSymbol;
    return rc;
}


SUPR3DECL(int) SUPR3LoadVMM(const char *pszFilename)
{
    void *pvImageBase;
    return SUPR3LoadModule(pszFilename, "VMMR0.r0", &pvImageBase, NULL /*pErrInfo*/);
}


SUPR3DECL(int) SUPR3UnloadVMM(void)
{
    return SUPR3FreeModule((void*)g_pvVMMR0);
}


SUPR3DECL(int) SUPR3GipGetPhys(PRTHCPHYS pHCPhys)
{
    if (g_pSUPGlobalInfoPage)
    {
        *pHCPhys = g_HCPhysSUPGlobalInfoPage;
        return VINF_SUCCESS;
    }
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_WRONG_ORDER;
}


/**
 * Worker for SUPR3HardenedLdrLoad and SUPR3HardenedLdrLoadAppPriv.
 *
 * @returns iprt status code.
 * @param   pszFilename     The full file name.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @param   fFlags          See RTLDFLAGS_.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 *
 */
static int supR3HardenedLdrLoadIt(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo)
{
#ifdef VBOX_WITH_HARDENING
    /*
     * Verify the image file.
     */
    int rc = supR3HardenedVerifyFixedFile(pszFilename, false /* fFatal */);
    if (RT_FAILURE(rc))
    {
        LogRel(("supR3HardenedLdrLoadIt: Verification of \"%s\" failed, rc=%Rrc\n", pszFilename, rc));
        return RTErrInfoSet(pErrInfo, rc, "supR3HardenedVerifyFixedFile failed");
    }
#endif

    /*
     * Try load it.
     */
    return RTLdrLoadEx(pszFilename, phLdrMod, fFlags, pErrInfo);
}


SUPR3DECL(int) SUPR3HardenedLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    /*
     * Validate input.
     */
    RTErrInfoClear(pErrInfo);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(phLdrMod, VERR_INVALID_POINTER);
    *phLdrMod = NIL_RTLDRMOD;
    AssertReturn(RTPathHavePath(pszFilename), VERR_INVALID_PARAMETER);

    /*
     * Add the default extension if it's missing.
     */
    if (!RTPathHaveExt(pszFilename))
    {
        const char *pszSuff = RTLdrGetSuff();
        size_t      cchSuff = strlen(pszSuff);
        size_t      cchFilename = strlen(pszFilename);
        char       *psz = (char *)alloca(cchFilename + cchSuff + 1);
        AssertReturn(psz, VERR_NO_TMP_MEMORY);
        memcpy(psz, pszFilename, cchFilename);
        memcpy(psz + cchFilename, pszSuff, cchSuff + 1);
        pszFilename = psz;
    }

    /*
     * Pass it on to the common library loader.
     */
    return supR3HardenedLdrLoadIt(pszFilename, phLdrMod, fFlags, pErrInfo);
}


SUPR3DECL(int) SUPR3HardenedLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    LogFlow(("SUPR3HardenedLdrLoadAppPriv: pszFilename=%p:{%s} phLdrMod=%p fFlags=%08x pErrInfo=%p\n", pszFilename, pszFilename, phLdrMod, fFlags, pErrInfo));

    /*
     * Validate input.
     */
    RTErrInfoClear(pErrInfo);
    AssertPtrReturn(phLdrMod, VERR_INVALID_PARAMETER);
    *phLdrMod = NIL_RTLDRMOD;
    AssertPtrReturn(pszFilename, VERR_INVALID_PARAMETER);
    AssertMsgReturn(!RTPathHavePath(pszFilename), ("%s\n", pszFilename), VERR_INVALID_PARAMETER);

    /*
     * Check the filename.
     */
    size_t cchFilename = strlen(pszFilename);
    AssertMsgReturn(cchFilename < (RTPATH_MAX / 4) * 3, ("%zu\n", cchFilename), VERR_INVALID_PARAMETER);

    const char *pszExt = "";
    size_t cchExt = 0;
    if (!RTPathHaveExt(pszFilename))
    {
        pszExt = RTLdrGetSuff();
        cchExt = strlen(pszExt);
    }

    /*
     * Construct the private arch path and check if the file exists.
     */
    char szPath[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szPath, sizeof(szPath) - 1 - cchExt - cchFilename);
    AssertRCReturn(rc, rc);

    char *psz = strchr(szPath, '\0');
    *psz++ = RTPATH_SLASH;
    memcpy(psz, pszFilename, cchFilename);
    psz += cchFilename;
    memcpy(psz, pszExt, cchExt + 1);

    if (!RTPathExists(szPath))
    {
        LogRel(("SUPR3HardenedLdrLoadAppPriv: \"%s\" not found\n", szPath));
        return VERR_FILE_NOT_FOUND;
    }

    /*
     * Pass it on to SUPR3HardenedLdrLoad.
     */
    rc = SUPR3HardenedLdrLoad(szPath, phLdrMod, fFlags, pErrInfo);

    LogFlow(("SUPR3HardenedLdrLoadAppPriv: returns %Rrc\n", rc));
    return rc;
}


SUPR3DECL(int) SUPR3HardenedLdrLoadPlugIn(const char *pszFilename, PRTLDRMOD phLdrMod, PRTERRINFO pErrInfo)
{
    /*
     * Validate input.
     */
    RTErrInfoClear(pErrInfo);
    AssertPtrReturn(phLdrMod, VERR_INVALID_PARAMETER);
    *phLdrMod = NIL_RTLDRMOD;
    AssertPtrReturn(pszFilename, VERR_INVALID_PARAMETER);
    AssertReturn(RTPathStartsWithRoot(pszFilename), VERR_INVALID_PARAMETER);

#ifdef VBOX_WITH_HARDENING
    /*
     * Verify the image file.
     */
    int rc = supR3HardenedVerifyFile(pszFilename, RTHCUINTPTR_MAX, pErrInfo);
    if (RT_FAILURE(rc))
    {
        if (!RTErrInfoIsSet(pErrInfo))
            LogRel(("supR3HardenedVerifyFile: Verification of \"%s\" failed, rc=%Rrc\n", pszFilename, rc));
        return rc;
    }
#endif

    /*
     * Try load it.
     */
    return RTLdrLoadEx(pszFilename, phLdrMod, RTLDRLOAD_FLAGS_LOCAL, pErrInfo);
}


SUPR3DECL(int) SUPR3QueryVTxSupported(void)
{
#ifdef RT_OS_LINUX
    return suplibOsQueryVTxSupported();
#else
    return VINF_SUCCESS;
#endif
}


SUPR3DECL(int) SUPR3QueryVTCaps(uint32_t *pfCaps)
{
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);

    *pfCaps = 0;

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPVTCAPS Req;
    Req.Hdr.u32Cookie = g_u32Cookie;
    Req.Hdr.u32SessionCookie = g_u32SessionCookie;
    Req.Hdr.cbIn = SUP_IOCTL_VT_CAPS_SIZE_IN;
    Req.Hdr.cbOut = SUP_IOCTL_VT_CAPS_SIZE_OUT;
    Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc = VERR_INTERNAL_ERROR;
    Req.u.Out.Caps = 0;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_VT_CAPS, &Req, SUP_IOCTL_VT_CAPS_SIZE);
    if (RT_SUCCESS(rc))
    {
        rc = Req.Hdr.rc;
        if (RT_SUCCESS(rc))
            *pfCaps = Req.u.Out.Caps;
    }
    return rc;
}


SUPR3DECL(int) SUPR3TracerOpen(uint32_t uCookie, uintptr_t uArg)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPTRACEROPEN Req;
    Req.Hdr.u32Cookie       = g_u32Cookie;
    Req.Hdr.u32SessionCookie= g_u32SessionCookie;
    Req.Hdr.cbIn            = SUP_IOCTL_TRACER_OPEN_SIZE_IN;
    Req.Hdr.cbOut           = SUP_IOCTL_TRACER_OPEN_SIZE_OUT;
    Req.Hdr.fFlags          = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc              = VERR_INTERNAL_ERROR;
    Req.u.In.uCookie        = uCookie;
    Req.u.In.uArg           = uArg;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_TRACER_OPEN, &Req, SUP_IOCTL_TRACER_OPEN_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPR3TracerClose(void)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPREQHDR Req;
    Req.u32Cookie       = g_u32Cookie;
    Req.u32SessionCookie= g_u32SessionCookie;
    Req.cbIn            = SUP_IOCTL_TRACER_OPEN_SIZE_IN;
    Req.cbOut           = SUP_IOCTL_TRACER_OPEN_SIZE_OUT;
    Req.fFlags          = SUPREQHDR_FLAGS_DEFAULT;
    Req.rc              = VERR_INTERNAL_ERROR;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_TRACER_CLOSE, &Req, SUP_IOCTL_TRACER_CLOSE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.rc;
    return rc;
}


SUPR3DECL(int) SUPR3TracerIoCtl(uintptr_t uCmd, uintptr_t uArg, int32_t *piRetVal)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        *piRetVal = -1;
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPTRACERIOCTL Req;
    Req.Hdr.u32Cookie       = g_u32Cookie;
    Req.Hdr.u32SessionCookie= g_u32SessionCookie;
    Req.Hdr.cbIn            = SUP_IOCTL_TRACER_IOCTL_SIZE_IN;
    Req.Hdr.cbOut           = SUP_IOCTL_TRACER_IOCTL_SIZE_OUT;
    Req.Hdr.fFlags          = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc              = VERR_INTERNAL_ERROR;
    Req.u.In.uCmd           = uCmd;
    Req.u.In.uArg           = uArg;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_TRACER_IOCTL, &Req, SUP_IOCTL_TRACER_IOCTL_SIZE);
    if (RT_SUCCESS(rc))
    {
        rc = Req.Hdr.rc;
        *piRetVal = Req.u.Out.iRetVal;
    }
    return rc;
}

extern "C"
{
    SUPDECL(void) SUPTracerFireProbe(uint32_t idProbe, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                     uintptr_t uArg3, uintptr_t uArg4);
}

SUPDECL(void) SUPTracerFireProbe(uint32_t idProbe, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                 uintptr_t uArg3, uintptr_t uArg4)
{
    /* C and stubbed for now. */
}

