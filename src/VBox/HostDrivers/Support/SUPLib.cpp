/* $Id$ */
/** @file
 * VirtualBox Support Library - Common code.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 * tranfer an argument set and call a predefined entry point in the Host
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
#include <VBox/vmm.h>
#include <VBox/log.h>
#include <VBox/x86.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/ldr.h>
#include <iprt/asm.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/env.h>
#include <iprt/rand.h>

#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"



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
/** Pointer to the Global Information Page.
 *
 * This pointer is valid as long as SUPLib has a open session. Anyone using
 * the page must treat this pointer as higly volatile and not trust it beyond
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
uint32_t            g_u32Cookie = 0;
/** The negotiated session cookie. */
uint32_t            g_u32SessionCookie;
/** Session handle. */
PSUPDRVSESSION      g_pSession;
/** R0 SUP Functions used for resolving referenced to the SUPR0 module. */
static PSUPQUERYFUNCS g_pFunctions;

#ifdef VBOX_WITH_IDT_PATCHING
/** The negotiated interrupt number. */
static uint8_t      g_u8Interrupt = 3;
/** Pointer to the generated code fore calling VMMR0. */
static PFNCALLVMMR0 g_pfnCallVMMR0;
#endif
/** VMMR0 Load Address. */
static RTR0PTR      g_pvVMMR0 = NIL_RTR0PTR;
/** Init counter. */
static unsigned     g_cInits = 0;
/** PAGE_ALLOC support indicator. */
static bool         g_fSupportsPageAllocLocked = true;
/** Fake mode indicator. (~0 at first, 0 or 1 after first test) */
static uint32_t     g_u32FakeMode = ~0;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int supInitFake(PSUPDRVSESSION *ppSession);
static int supLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase);
#ifdef VBOX_WITH_IDT_PATCHING
static int supInstallIDTE(void);
#endif
static DECLCALLBACK(int) supLoadModuleResolveImport(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser);


SUPR3DECL(int) SUPInstall(void)
{
    return suplibOsInstall();
}


SUPR3DECL(int) SUPUninstall(void)
{
    return suplibOsUninstall();
}


SUPR3DECL(int) SUPInit(PSUPDRVSESSION *ppSession /* NULL */, size_t cbReserve /* 0 */)
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

    /**
     * Open the support driver.
     */
    int rc = suplibOsInit(cbReserve);
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
        CookieReq.u.In.u32ReqVersion = SUPDRVIOC_VERSION;
        CookieReq.u.In.u32MinVersion = SUPDRVIOC_VERSION & 0xffff0000;
        rc = suplibOsIOCtl(SUP_IOCTL_COOKIE, &CookieReq, SUP_IOCTL_COOKIE_SIZE);
        if (    RT_SUCCESS(rc)
            &&  RT_SUCCESS(CookieReq.Hdr.rc))
        {
            if ((CookieReq.u.Out.u32SessionVersion & 0xffff0000) == (SUPDRVIOC_VERSION & 0xffff0000))
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
                    rc = suplibOsIOCtl(SUP_IOCTL_QUERY_FUNCS(CookieReq.u.Out.cFunctions), pFuncsReq, SUP_IOCTL_QUERY_FUNCS_SIZE(CookieReq.u.Out.cFunctions));
                    if (RT_SUCCESS(rc))
                        rc = pFuncsReq->Hdr.rc;
                    if (RT_SUCCESS(rc))
                    {
                        g_u32Cookie         = CookieReq.u.Out.u32Cookie;
                        g_u32SessionCookie  = CookieReq.u.Out.u32SessionCookie;
                        g_pSession          = CookieReq.u.Out.pSession;
                        g_pFunctions        = pFuncsReq;
                        if (ppSession)
                            *ppSession = CookieReq.u.Out.pSession;

                        /*
                         * Map the GIP into userspace.
                         * This is an optional feature, so we will ignore any failures here.
                         */
                        if (!g_pSUPGlobalInfoPage)
                        {
                            SUPGIPMAP GipMapReq;
                            GipMapReq.Hdr.u32Cookie = g_u32Cookie;
                            GipMapReq.Hdr.u32SessionCookie = g_u32SessionCookie;
                            GipMapReq.Hdr.cbIn = SUP_IOCTL_GIP_MAP_SIZE_IN;
                            GipMapReq.Hdr.cbOut = SUP_IOCTL_GIP_MAP_SIZE_OUT;
                            GipMapReq.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
                            GipMapReq.Hdr.rc = VERR_INTERNAL_ERROR;
                            GipMapReq.u.Out.HCPhysGip = NIL_RTHCPHYS;
                            GipMapReq.u.Out.pGipR0 = NIL_RTR0PTR;
                            GipMapReq.u.Out.pGipR3 = NULL;
                            rc = suplibOsIOCtl(SUP_IOCTL_GIP_MAP, &GipMapReq, SUP_IOCTL_GIP_MAP_SIZE);
                            if (RT_SUCCESS(rc))
                                rc = GipMapReq.Hdr.rc;
                            if (RT_SUCCESS(rc))
                            {
                                AssertRelease(GipMapReq.u.Out.pGipR3->u32Magic == SUPGLOBALINFOPAGE_MAGIC);
                                AssertRelease(GipMapReq.u.Out.pGipR3->u32Version >= SUPGLOBALINFOPAGE_VERSION);
                                ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, GipMapReq.u.Out.HCPhysGip);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, GipMapReq.u.Out.pGipR3, NULL);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, (void *)GipMapReq.u.Out.pGipR0, NULL);
                            }
                        }
                        return VINF_SUCCESS;
                    }

                    /* bailout */
                    RTMemFree(pFuncsReq);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
            {
                LogRel(("Support driver version mismatch: SessionVersion=%#x DriverVersion=%#x ClientVersion=%#x\n",
                        CookieReq.u.Out.u32SessionVersion, CookieReq.u.Out.u32DriverVersion, SUPDRVIOC_VERSION));
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
        }
        else
        {
            if (RT_SUCCESS(rc))
            {
                rc = CookieReq.Hdr.rc;
                LogRel(("Support driver version mismatch: DriverVersion=%#x ClientVersion=%#x rc=%Rrc\n",
                        CookieReq.u.Out.u32DriverVersion, SUPDRVIOC_VERSION, rc));
                if (rc != VERR_VM_DRIVER_VERSION_MISMATCH)
                    rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
            else
            {
                /* for pre 0x00060000 drivers */
                LogRel(("Support driver version mismatch: DriverVersion=too-old ClientVersion=%#x\n", SUPDRVIOC_VERSION));
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
        }

        suplibOsTerm();
    }
    AssertMsgFailed(("SUPInit() failed rc=%Vrc\n", rc));
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
        { "SUPR0ObjRegister",                       0xefef0000 },
        { "SUPR0ObjAddRef",                         0xefef0001 },
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
        { "SUPR0ExecuteCallback",                   0xefef000c },
        { "RTMemAlloc",                             0xefef000d },
        { "RTMemAllocZ",                            0xefef000e },
        { "RTMemFree",                              0xefef000f },
        { "RTR0MemObjAddress",                      0xefef0010 },
        { "RTR0MemObjAddressR3",                    0xefef0011 },
        { "RTR0MemObjAllocPage",                    0xefef0012 },
        { "RTR0MemObjAllocPhysNC",                  0xefef0013 },
        { "RTR0MemObjAllocLow",                     0xefef0014 },
        { "RTR0MemObjFree",                         0xefef0015 },
        { "RTR0MemObjGetPagePhysAddr",              0xefef0016 },
        { "RTR0MemObjMapUser",                      0xefef0017 },
        { "RTProcSelf",                             0xefef0038 },
        { "RTR0ProcHandleSelf",                     0xefef0039 },
        { "RTSemEventCreate",                       0xefef0018 },
        { "RTSemEventSignal",                       0xefef0019 },
        { "RTSemEventWait",                         0xefef001a },
        { "RTSemEventWaitNoResume",                 0xefef001b },
        { "RTSemEventDestroy",                      0xefef001c },
        { "RTSemEventMultiCreate",                  0xefef001d },
        { "RTSemEventMultiSignal",                  0xefef001e },
        { "RTSemEventMultiReset",                   0xefef001f },
        { "RTSemEventMultiWait",                    0xefef0020 },
        { "RTSemEventMultiWaitNoResume",            0xefef0021 },
        { "RTSemEventMultiDestroy",                 0xefef0022 },
        { "RTSemFastMutexCreate",                   0xefef0023 },
        { "RTSemFastMutexDestroy",                  0xefef0024 },
        { "RTSemFastMutexRequest",                  0xefef0025 },
        { "RTSemFastMutexRelease",                  0xefef0026 },
        { "RTSpinlockCreate",                       0xefef0027 },
        { "RTSpinlockDestroy",                      0xefef0028 },
        { "RTSpinlockAcquire",                      0xefef0029 },
        { "RTSpinlockRelease",                      0xefef002a },
        { "RTSpinlockAcquireNoInts",                0xefef002b },
        { "RTSpinlockReleaseNoInts",                0xefef002c },
        { "RTThreadNativeSelf",                     0xefef002d },
        { "RTThreadSleep",                          0xefef002e },
        { "RTThreadYield",                          0xefef002f },
        { "RTLogDefaultInstance",                   0xefef0030 },
        { "RTLogRelDefaultInstance",                0xefef0031 },
        { "RTLogSetDefaultInstanceThread",          0xefef0032 },
        { "RTLogLogger",                            0xefef0033 },
        { "RTLogLoggerEx",                          0xefef0034 },
        { "RTLogLoggerExV",                         0xefef0035 },
        { "AssertMsg1",                             0xefef0036 },
        { "AssertMsg2",                             0xefef0037 },
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
#ifdef VBOX_WITH_IDT_PATCHING
        Assert(g_u8Interrupt == 3);
#endif

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


SUPR3DECL(int) SUPTerm(bool fForced)
{
    /*
     * Verify state.
     */
    AssertMsg(g_cInits > 0, ("SUPTerm() is called before SUPInit()!\n"));
    if (g_cInits == 0)
        return VERR_WRONG_ORDER;
    if (g_cInits == 1 || fForced)
    {
        /*
         * NULL the GIP pointer.
         */
        if (g_pSUPGlobalInfoPage)
        {
            ASMAtomicXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, NULL);
            ASMAtomicXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, NULL);
            ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, NIL_RTHCPHYS);
            /* just a little safe guard against threads using the page. */
            RTThreadSleep(50);
        }

        /*
         * Close the support driver.
         */
        int rc = suplibOsTerm();
        if (rc)
            return rc;

        g_u32Cookie         = 0;
        g_u32SessionCookie  = 0;
#ifdef VBOX_WITH_IDT_PATCHING
        g_u8Interrupt       = 3;
#endif
        g_cInits            = 0;
    }
    else
        g_cInits--;

    return 0;
}


SUPR3DECL(SUPPAGINGMODE) SUPGetPagingMode(void)
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
    int rc = suplibOsIOCtl(SUP_IOCTL_GET_PAGING_MODE, &Req, SUP_IOCTL_GET_PAGING_MODE_SIZE);
    if (    RT_FAILURE(rc)
        ||  RT_FAILURE(Req.Hdr.rc))
    {
        LogRel(("SUPGetPagingMode: %Rrc %Rrc\n", rc, Req.Hdr.rc));
        Req.u.Out.enmMode = SUPPAGINGMODE_INVALID;
    }

    return Req.u.Out.enmMode;
}


/**
 * For later.
 */
static int supCallVMMR0ExFake(PVMR0 pVMR0, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
    AssertMsgFailed(("%d\n", uOperation));
    return VERR_NOT_SUPPORTED;
}


SUPR3DECL(int) SUPCallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation)
{
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_RAW_RUN))
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_RAW_RUN);
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_HWACC_RUN))
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_HWACC_RUN);
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_NOP))
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_NOP);

    AssertMsgFailed(("%#x\n", uOperation));
    return VERR_INTERNAL_ERROR;
}


SUPR3DECL(int) SUPCallVMMR0Ex(PVMR0 pVMR0, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
#if 0 /* temp hack. */
    /*
     * The following operations don't belong here.
     */
    AssertMsgReturn(    uOperation != SUP_VMMR0_DO_RAW_RUN
                    &&  uOperation != SUP_VMMR0_DO_HWACC_RUN
                    &&  uOperation != SUP_VMMR0_DO_NOP,
                    ("%#x\n", uOperation),
                    VERR_INTERNAL_ERROR);
#else
    if (    (    uOperation == SUP_VMMR0_DO_RAW_RUN
             ||  uOperation == SUP_VMMR0_DO_HWACC_RUN
             ||  uOperation == SUP_VMMR0_DO_NOP)
        &&  !pReqHdr
        &&  !u64Arg)
        return (int) SUPCallVMMR0Fast(pVMR0, uOperation);
#endif

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
        Req.u.In.uOperation = uOperation;
        Req.u.In.u64Arg = u64Arg;
        rc = suplibOsIOCtl(SUP_IOCTL_CALL_VMMR0(0), &Req, SUP_IOCTL_CALL_VMMR0_SIZE(0));
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
        pReq->u.In.uOperation = uOperation;
        pReq->u.In.u64Arg = u64Arg;
        memcpy(&pReq->abReqPkt[0], pReqHdr, cbReq);
        rc = suplibOsIOCtl(SUP_IOCTL_CALL_VMMR0(cbReq), pReq, SUP_IOCTL_CALL_VMMR0_SIZE(cbReq));
        if (RT_SUCCESS(rc))
            rc = pReq->Hdr.rc;
        memcpy(pReqHdr, &pReq->abReqPkt[0], cbReq);
    }
    else /** @todo may have to remove the size limits one this request... */
        AssertMsgFailedReturn(("cbReq=%#x\n", pReqHdr->cbReq), VERR_INTERNAL_ERROR);
    return rc;
}


SUPR3DECL(int) SUPCallVMMR0(PVMR0 pVMR0, unsigned uOperation, void *pvArg)
{
#if defined(VBOX_WITH_IDT_PATCHING)
    return g_pfnCallVMMR0(pVMR0, uOperation, pvArg);

#else
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_RAW_RUN))
    {
        Assert(!pvArg);
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_RAW_RUN);
    }
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_HWACC_RUN))
    {
        Assert(!pvArg);
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_HWACC_RUN);
    }
    if (RT_LIKELY(uOperation == SUP_VMMR0_DO_NOP))
    {
        Assert(!pvArg);
        return suplibOsIOCtlFast(SUP_IOCTL_FAST_DO_NOP);
    }
    return SUPCallVMMR0Ex(pVMR0, uOperation, (uintptr_t)pvArg, NULL);
#endif
}


SUPR3DECL(int) SUPSetVMForFastIOCtl(PVMR0 pVMR0)
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
    int rc = suplibOsIOCtl(SUP_IOCTL_SET_VM_FOR_FAST, &Req, SUP_IOCTL_SET_VM_FOR_FAST_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPPageAlloc(size_t cPages, void **ppvPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

#ifdef RT_OS_WINDOWS
    /*
     * Temporary hack for windows until we've sorted out the
     * locked memory that doesn't need to be accessible from kernel space.
     */
    return SUPPageAllocLockedEx(cPages, ppvPages, NULL);
#else
    /*
     * Call OS specific worker.
     */
    return suplibOsPageAlloc(cPages, ppvPages);
#endif
}


SUPR3DECL(int) SUPPageFree(void *pvPages, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

#ifdef RT_OS_WINDOWS
    /*
     * Temporary hack for windows, see above.
     */
    return SUPPageFreeLocked(pvPages, cPages);
#else
    /*
     * Call OS specific worker.
     */
    return suplibOsPageFree(pvPages, cPages);
#endif
}


SUPR3DECL(int) SUPPageLock(void *pvStart, size_t cPages, PSUPPAGE paPages)
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
        unsigned    iPage = cPages;
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
        pReq->u.In.cPages = cPages; AssertRelease(pReq->u.In.cPages == cPages);
        rc = suplibOsIOCtl(SUP_IOCTL_PAGE_LOCK, pReq, SUP_IOCTL_PAGE_LOCK_SIZE(cPages));
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


SUPR3DECL(int) SUPPageUnlock(void *pvStart)
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
    int rc = suplibOsIOCtl(SUP_IOCTL_PAGE_UNLOCK, &Req, SUP_IOCTL_PAGE_UNLOCK_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPPageAllocLocked(size_t cPages, void **ppvPages)
{
    return SUPPageAllocLockedEx(cPages, ppvPages, NULL);
}


/**
 * Fallback for SUPPageAllocLockedEx on systems where RTR0MemObjPhysAllocNC isn't supported.
 */
static int supPageAllocLockedFallback(size_t cPages, void **ppvPages, PSUPPAGE paPages)
{
    int rc = suplibOsPageAlloc(cPages, ppvPages);
    if (RT_SUCCESS(rc))
    {
        if (!paPages)
            paPages = (PSUPPAGE)alloca(sizeof(paPages[0]) * cPages);
        rc = SUPPageLock(*ppvPages, cPages, paPages);
        if (RT_FAILURE(rc))
            suplibOsPageFree(*ppvPages, cPages);
    }
    return rc;
}


SUPR3DECL(int) SUPPageAllocLockedEx(size_t cPages, void **ppvPages, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        *ppvPages = RTMemPageAllocZ((size_t)cPages * PAGE_SIZE);
        if (!*ppvPages)
            return VERR_NO_MEMORY;
        if (paPages)
            for (size_t iPage = 0; iPage < cPages; iPage++)
            {
                paPages[iPage].uReserved = 0;
                paPages[iPage].Phys = (iPage + 1234) << PAGE_SHIFT;
                Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
            }
        return VINF_SUCCESS;
    }

    /* use fallback? */
    if (!g_fSupportsPageAllocLocked)
        return supPageAllocLockedFallback(cPages, ppvPages, paPages);

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    int rc;
    PSUPPAGEALLOC pReq = (PSUPPAGEALLOC)RTMemTmpAllocZ(SUP_IOCTL_PAGE_ALLOC_SIZE(cPages));
    if (pReq)
    {
        pReq->Hdr.u32Cookie = g_u32Cookie;
        pReq->Hdr.u32SessionCookie = g_u32SessionCookie;
        pReq->Hdr.cbIn = SUP_IOCTL_PAGE_ALLOC_SIZE_IN;
        pReq->Hdr.cbOut = SUP_IOCTL_PAGE_ALLOC_SIZE_OUT(cPages);
        pReq->Hdr.fFlags = SUPREQHDR_FLAGS_MAGIC | SUPREQHDR_FLAGS_EXTRA_OUT;
        pReq->Hdr.rc = VERR_INTERNAL_ERROR;
        pReq->u.In.cPages = cPages; AssertRelease(pReq->u.In.cPages == cPages);
        rc = suplibOsIOCtl(SUP_IOCTL_PAGE_ALLOC, pReq, SUP_IOCTL_PAGE_ALLOC_SIZE(cPages));
        if (RT_SUCCESS(rc))
        {
            rc = pReq->Hdr.rc;
            if (RT_SUCCESS(rc))
            {
                *ppvPages = pReq->u.Out.pvR3;
                if (paPages)
                    for (size_t iPage = 0; iPage < cPages; iPage++)
                    {
                        paPages[iPage].uReserved = 0;
                        paPages[iPage].Phys = pReq->u.Out.aPages[iPage];
                        Assert(!(paPages[iPage].Phys & ~X86_PTE_PAE_PG_MASK));
                    }
            }
            else if (rc == VERR_NOT_SUPPORTED)
            {
                g_fSupportsPageAllocLocked = false;
                rc = supPageAllocLockedFallback(cPages, ppvPages, paPages);
            }
        }

        RTMemTmpFree(pReq);
    }
    else
        rc = VERR_NO_TMP_MEMORY;
    return rc;
}


SUPR3DECL(int) SUPPageFreeLocked(void *pvPages, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pvPages);
        return VINF_SUCCESS;
    }

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    int rc;
    if (g_fSupportsPageAllocLocked)
    {
        SUPPAGEFREE Req;
        Req.Hdr.u32Cookie = g_u32Cookie;
        Req.Hdr.u32SessionCookie = g_u32SessionCookie;
        Req.Hdr.cbIn = SUP_IOCTL_PAGE_FREE_SIZE_IN;
        Req.Hdr.cbOut = SUP_IOCTL_PAGE_FREE_SIZE_OUT;
        Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        Req.Hdr.rc = VERR_INTERNAL_ERROR;
        Req.u.In.pvR3 = pvPages;
        rc = suplibOsIOCtl(SUP_IOCTL_PAGE_FREE, &Req, SUP_IOCTL_PAGE_FREE_SIZE);
        if (RT_SUCCESS(rc))
            rc = Req.Hdr.rc;
    }
    else
    {
        /* fallback */
        rc = SUPPageUnlock(pvPages);
        if (RT_SUCCESS(rc))
            rc = suplibOsPageFree(pvPages, cPages);
    }
    return rc;
}


SUPR3DECL(void *) SUPContAlloc(size_t cPages, PRTHCPHYS pHCPhys)
{
    return SUPContAlloc2(cPages, NIL_RTR0PTR, pHCPhys);
}


SUPR3DECL(void *) SUPContAlloc2(size_t cPages, PRTR0PTR pR0Ptr, PRTHCPHYS pHCPhys)
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
    Req.u.In.cPages = cPages;
    int rc = suplibOsIOCtl(SUP_IOCTL_CONT_ALLOC, &Req, SUP_IOCTL_CONT_ALLOC_SIZE);
    if (    RT_SUCCESS(rc)
        &&  RT_SUCCESS(Req.Hdr.rc))
    {
        *pHCPhys = Req.u.Out.HCPhys;
        if (pR0Ptr)
            *pR0Ptr = Req.u.Out.pvR0;
        return Req.u.Out.pvR3;
    }

    return NULL;
}


SUPR3DECL(int) SUPContFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    if (!pv)
        return VINF_SUCCESS;
    AssertPtrReturn(pv, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pv);
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
    int rc = suplibOsIOCtl(SUP_IOCTL_CONT_FREE, &Req, SUP_IOCTL_CONT_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPLowAlloc(size_t cPages, void **ppvPages, PRTR0PTR ppvPagesR0, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtrReturn(ppvPages, VERR_INVALID_POINTER);
    *ppvPages = NULL;
    AssertPtrReturn(paPages, VERR_INVALID_POINTER);
    AssertMsgReturn(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages), VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        *ppvPages = RTMemPageAllocZ((size_t)cPages * PAGE_SIZE);
        if (!*ppvPages)
            return VERR_NO_LOW_MEMORY;

        /* fake physical addresses. */
        RTHCPHYS    Phys = (uintptr_t)*ppvPages + PAGE_SIZE * 1024;
        unsigned    iPage = cPages;
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
        pReq->u.In.cPages = cPages; AssertRelease(pReq->u.In.cPages == cPages);
        rc = suplibOsIOCtl(SUP_IOCTL_LOW_ALLOC, pReq, SUP_IOCTL_LOW_ALLOC_SIZE(cPages));
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
        }
        RTMemTmpFree(pReq);
    }
    else
        rc = VERR_NO_TMP_MEMORY;

    return rc;
}


SUPR3DECL(int) SUPLowFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    if (!pv)
        return VINF_SUCCESS;
    AssertPtrReturn(pv, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_INVALID_PARAMETER);

    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
        RTMemPageFree(pv);
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
    int rc = suplibOsIOCtl(SUP_IOCTL_LOW_FREE, &Req, SUP_IOCTL_LOW_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    return rc;
}


SUPR3DECL(int) SUPLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase)
{
    /*
     * Load the module.
     * If it's VMMR0.r0 we need to install the IDTE.
     */
    int rc = supLoadModule(pszFilename, pszModule, ppvImageBase);
#ifdef VBOX_WITH_IDT_PATCHING
    if (    RT_SUCCESS(rc)
        &&  !strcmp(pszModule, "VMMR0.r0"))
    {
        rc = supInstallIDTE();
        if (RT_FAILURE(rc))
            SUPFreeModule(*ppvImageBase);
    }
#endif /* VBOX_WITH_IDT_PATCHING */

    return rc;
}


#ifdef VBOX_WITH_IDT_PATCHING
/**
 * Generates the code for calling the interrupt gate.
 *
 * @returns VBox status code.
 *          g_pfnCallVMMR0 is changed on success.
 * @param   u8Interrupt     The interrupt number.
 */
static int suplibGenerateCallVMMR0(uint8_t u8Interrupt)
{
    /*
     * Allocate memory.
     */
    uint8_t *pb = (uint8_t *)RTMemExecAlloc(256);
    AssertReturn(pb, VERR_NO_MEMORY);
    memset(pb, 0xcc, 256);
    Assert(!g_pfnCallVMMR0);
    g_pfnCallVMMR0 = *(PFNCALLVMMR0*)&pb;

    /*
     * Generate the code.
     */
#ifdef RT_ARCH_AMD64
    /*
     * reg params:
     *      <GCC>   <MSC>   <argument>
     *      rdi     rcx     pVMR0
     *      esi     edx     uOperation
     *      rdx     r8      pvArg
     *
     *      eax     eax     [g_u32Gookie]
     */
    *pb++ = 0xb8;                       /* mov eax, <g_u32Cookie> */
    *(uint32_t *)pb = g_u32Cookie;
    pb += sizeof(uint32_t);

    *pb++ = 0xcd;                       /* int <u8Interrupt> */
    *pb++ = u8Interrupt;

    *pb++ = 0xc3;                       /* ret */

#else
    /*
     * x86 stack:
     *          0   saved esi
     *      0   4   ret
     *      4   8   pVM
     *      8   c   uOperation
     *      c  10   pvArg
     */
    *pb++ = 0x56;                       /* push esi */

    *pb++ = 0x8b;                       /* mov eax, [pVM] */
    *pb++ = 0x44;
    *pb++ = 0x24;
    *pb++ = 0x08;                       /* esp+08h */

    *pb++ = 0x8b;                       /* mov edx, [uOperation] */
    *pb++ = 0x54;
    *pb++ = 0x24;
    *pb++ = 0x0c;                       /* esp+0ch */

    *pb++ = 0x8b;                       /* mov ecx, [pvArg] */
    *pb++ = 0x4c;
    *pb++ = 0x24;
    *pb++ = 0x10;                       /* esp+10h */

    *pb++ = 0xbe;                       /* mov esi, <g_u32Cookie> */
    *(uint32_t *)pb = g_u32Cookie;
    pb += sizeof(uint32_t);

    *pb++ = 0xcd;                       /* int <u8Interrupt> */
    *pb++ = u8Interrupt;

    *pb++ = 0x5e;                       /* pop esi */

    *pb++ = 0xc3;                       /* ret */
#endif

    return VINF_SUCCESS;
}


/**
 * Installs the IDTE patch.
 *
 * @return VBox status code.
 */
static int supInstallIDTE(void)
{
    /* already installed? */
    if (g_u8Interrupt != 3 || g_u32FakeMode)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;
    const unsigned  cCpus = RTSystemProcessorGetCount();
    if (cCpus <= 1)
    {
        /* UNI */
        SUPIDTINSTALL Req;
        Req.Hdr.u32Cookie = g_u32Cookie;
        Req.Hdr.u32SessionCookie = g_u32SessionCookie;
        Req.Hdr.cbIn = SUP_IOCTL_IDT_INSTALL_SIZE_IN;
        Req.Hdr.cbOut = SUP_IOCTL_IDT_INSTALL_SIZE_OUT;
        Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        Req.Hdr.rc = VERR_INTERNAL_ERROR;
        rc = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &Req, SUP_IOCTL_IDT_INSTALL_SIZE);
        if (RT_SUCCESS(rc))
            rc = Req.Hdr.rc;
        if (RT_SUCCESS(rc))
        {
            g_u8Interrupt = Req.u.Out.u8Idt;
            rc = suplibGenerateCallVMMR0(Req.u.Out.u8Idt);
        }
    }
    else
    {
        /* SMP */
        uint64_t        u64AffMaskSaved = RTThreadGetAffinity();
        uint64_t        u64AffMaskPatched = RTSystemProcessorGetActiveMask() & u64AffMaskSaved;
        unsigned        cCpusPatched = 0;

        for (int i = 0; i < 64; i++)
        {
            /* Skip absent and inactive processors. */
            uint64_t u64Mask = 1ULL << i;
            if (!(u64Mask & u64AffMaskPatched))
                continue;

            /* Change CPU */
            int rc2 = RTThreadSetAffinity(u64Mask);
            if (RT_FAILURE(rc2))
            {
                u64AffMaskPatched &= ~u64Mask;
                LogRel(("SUPLoadVMM: Failed to set affinity to cpu no. %d, rc=%Vrc.\n", i, rc2));
                continue;
            }

            /* Patch the CPU. */
            SUPIDTINSTALL Req;
            Req.Hdr.u32Cookie = g_u32Cookie;
            Req.Hdr.u32SessionCookie = g_u32SessionCookie;
            Req.Hdr.cbIn = SUP_IOCTL_IDT_INSTALL_SIZE_IN;
            Req.Hdr.cbOut = SUP_IOCTL_IDT_INSTALL_SIZE_OUT;
            Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
            Req.Hdr.rc = VERR_INTERNAL_ERROR;
            rc2 = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &Req, SUP_IOCTL_IDT_INSTALL_SIZE);
            if (RT_SUCCESS(rc2))
                rc2 = Req.Hdr.rc;
            if (RT_SUCCESS(rc2))
            {
                if (!cCpusPatched)
                {
                    g_u8Interrupt = Req.u.Out.u8Idt;
                    rc2 = suplibGenerateCallVMMR0(Req.u.Out.u8Idt);
                    if (RT_FAILURE(rc2))
                    {
                        LogRel(("suplibGenerateCallVMMR0 failed with rc=%Vrc.\n", i, rc2));
                        rc = rc2;
                    }
                }
                else
                    Assert(g_u8Interrupt == Req.u.Out.u8Idt);
                cCpusPatched++;
            }
            else
            {

                LogRel(("SUPLoadVMM: Failed to patch cpu no. %d, rc=%Vrc.\n", i, rc2));
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }

        /* Fail if no CPUs was patched! */
        if (RT_SUCCESS(rc) && cCpusPatched <= 0)
            rc = VERR_GENERAL_FAILURE;
        /* Ignore failures if a CPU was patched. */
        else if (RT_FAILURE(rc) && cCpusPatched > 0)
            rc = VINF_SUCCESS;

        /* Set/restore the thread affinity. */
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadSetAffinity(u64AffMaskPatched);
            AssertRC(rc);
        }
        else
        {
            int rc2 = RTThreadSetAffinity(u64AffMaskSaved);
            AssertRC(rc2);
        }
    }
    return rc;
}
#endif /* VBOX_WITH_IDT_PATCHING */


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
    AssertPtr(pValue);
    AssertPtr(pvUser);

    /*
     * Only SUPR0 and VMMR0.r0
     */
    if (    pszModule
        &&  *pszModule
        &&  strcmp(pszModule, "SUPR0.dll")
        &&  strcmp(pszModule, "VMMR0.r0"))
    {
        AssertMsgFailed(("%s is importing from %s! (expected 'SUPR0.dll' or 'VMMR0.r0', case-sensitiv)\n", pvUser, pszModule));
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
    /* skip the 64-bit ELF import prefix first. */
    if (!strncmp(pszSymbol, "SUPR0$", sizeof("SUPR0$") - 1))
        pszSymbol += sizeof("SUPR0$") - 1;

    /*
     * Check the VMMR0.r0 module if loaded.
     */
    /** @todo call the SUPLoadModule caller.... */
    /** @todo proper reference counting and such. */
    if (g_pvVMMR0 != NIL_RTR0PTR)
    {
        void *pvValue;
        if (!SUPGetSymbolR0((void *)g_pvVMMR0, pszSymbol, &pvValue))
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
    /** @todo R0 mapping? */
    if (    pszSymbol
        &&  g_pSUPGlobalInfoPage
        &&  g_pSUPGlobalInfoPageR0
        &&  !strcmp(pszSymbol, "g_SUPGlobalInfoPage"))
    {
        *pValue = (uintptr_t)g_pSUPGlobalInfoPageR0;
        return VINF_SUCCESS;
    }

    /*
     * Despair.
     */
    c = g_pFunctions->u.Out.cFunctions;
    pFunc = &g_pFunctions->u.Out.aFunctions[0];
    while (c-- > 0)
    {
        AssertMsg2("%d: %s\n", g_pFunctions->u.Out.cFunctions - c, pFunc->szName);
        pFunc++;
    }

    AssertMsg2("%s is importing %s which we couldn't find\n", pvUser, pszSymbol);
    AssertMsgFailed(("%s is importing %s which we couldn't find\n", pvUser, pszSymbol));
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
    return VINF_SUCCESS;
}


/**
 * Worker for SUPLoadModule().
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the VMMR0 image file
 */
static int supLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszModule, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvImageBase, VERR_INVALID_PARAMETER);
    AssertReturn(strlen(pszModule) < RT_SIZEOFMEMB(SUPLDROPEN, u.In.szName), VERR_FILENAME_TOO_LONG);

    const bool fIsVMMR0 = !strcmp(pszModule, "VMMR0.r0");
    *ppvImageBase = NULL;

    /*
     * Open image file and figure its size.
     */
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(pszFilename, &hLdrMod);
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
        const uint32_t  cbImage   = RT_ALIGN_32(offStrTab + CalcArgs.cbStrings, 8);

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
        OpenReq.u.In.cbImage = cbImage;
        strcpy(OpenReq.u.In.szName, pszModule);
        if (!g_u32FakeMode)
        {
            rc = suplibOsIOCtl(SUP_IOCTL_LDR_OPEN, &OpenReq, SUP_IOCTL_LDR_OPEN_SIZE);
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
            PSUPLDRLOAD pLoadReq = (PSUPLDRLOAD)RTMemTmpAlloc(SUP_IOCTL_LDR_LOAD_SIZE(cbImage));
            if (pLoadReq)
            {
                /*
                 * Get the image bits.
                 */
                rc = RTLdrGetBits(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase,
                                  supLoadModuleResolveImport, (void *)pszModule);

                if (RT_SUCCESS(rc))
                {
                    /*
                     * Get the entry points.
                     */
                    RTUINTPTR VMMR0EntryInt = 0;
                    RTUINTPTR VMMR0EntryFast = 0;
                    RTUINTPTR VMMR0EntryEx = 0;
                    RTUINTPTR ModuleInit = 0;
                    RTUINTPTR ModuleTerm = 0;
                    if (fIsVMMR0)
                    {
                        rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryInt", &VMMR0EntryInt);
                        if (RT_SUCCESS(rc))
                            rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryFast", &VMMR0EntryFast);
                        if (RT_SUCCESS(rc))
                            rc = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "VMMR0EntryEx", &VMMR0EntryEx);
                    }
                    if (RT_SUCCESS(rc))
                    {
                        int rc2 = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "ModuleInit", &ModuleInit);
                        if (RT_FAILURE(rc2))
                            ModuleInit = 0;

                        rc2 = RTLdrGetSymbolEx(hLdrMod, &pLoadReq->u.In.achImage[0], (uintptr_t)OpenReq.u.Out.pvImageBase, "ModuleTerm", &ModuleTerm);
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
                        CreateArgs.pSym    = (PSUPLDRSYM)&pLoadReq->u.In.achImage[offSymTab];
                        CreateArgs.pszBase =     (char *)&pLoadReq->u.In.achImage[offStrTab];
                        CreateArgs.psz     = CreateArgs.pszBase;
                        rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCreateTabsCB, &CreateArgs);
                        if (RT_SUCCESS(rc))
                        {
                            AssertRelease((size_t)(CreateArgs.psz - CreateArgs.pszBase) <= CalcArgs.cbStrings);
                            AssertRelease((size_t)(CreateArgs.pSym - (PSUPLDRSYM)&pLoadReq->u.In.achImage[offSymTab]) <= CalcArgs.cSymbols);

                            /*
                             * Upload the image.
                             */
                            pLoadReq->Hdr.u32Cookie = g_u32Cookie;
                            pLoadReq->Hdr.u32SessionCookie = g_u32SessionCookie;
                            pLoadReq->Hdr.cbIn = SUP_IOCTL_LDR_LOAD_SIZE_IN(cbImage);
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
                            else
                                pLoadReq->u.In.eEPType                = SUPLDRLOADEP_NOTHING;
                            pLoadReq->u.In.offStrTab                  = offStrTab;
                            pLoadReq->u.In.cbStrTab                   = (uint32_t)CalcArgs.cbStrings;
                            AssertRelease(pLoadReq->u.In.cbStrTab == CalcArgs.cbStrings);
                            pLoadReq->u.In.offSymbols                 = offSymTab;
                            pLoadReq->u.In.cSymbols                   = CalcArgs.cSymbols;
                            pLoadReq->u.In.cbImage                    = cbImage;
                            pLoadReq->u.In.pvImageBase                = OpenReq.u.Out.pvImageBase;
                            if (!g_u32FakeMode)
                            {
                                rc = suplibOsIOCtl(SUP_IOCTL_LDR_LOAD, pLoadReq, SUP_IOCTL_LDR_LOAD_SIZE(cbImage));
                                if (RT_SUCCESS(rc))
                                    rc = pLoadReq->Hdr.rc;
                            }
                            else
                                rc = VINF_SUCCESS;
                            if (    RT_SUCCESS(rc)
                                ||  rc == VERR_ALREADY_LOADED /* A competing process. */
                               )
                            {
                                LogRel(("SUP: Loaded %s (%s) at %#p - ModuleInit at %RTptr and ModuleTerm at %RTptr\n", pszModule, pszFilename,
                                        OpenReq.u.Out.pvImageBase, ModuleInit, ModuleTerm));
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
                AssertMsgFailed(("failed to allocated %d bytes for SUPLDRLOAD_IN structure!\n", SUP_IOCTL_LDR_LOAD_SIZE(cbImage)));
                rc = VERR_NO_TMP_MEMORY;
            }
        }
        else if (RT_SUCCESS(rc))
        {
            if (fIsVMMR0)
                g_pvVMMR0 = OpenReq.u.Out.pvImageBase;
            LogRel(("SUP: Opened %s (%s) at %#p.\n", pszModule, pszFilename, OpenReq.u.Out.pvImageBase));
#ifdef RT_OS_WINDOWS
            LogRel(("SUP: windbg> .reload /f %s=%#p\n", pszFilename, OpenReq.u.Out.pvImageBase));
#endif
        }
    }
    RTLdrClose(hLdrMod);
    return rc;
}


SUPR3DECL(int) SUPFreeModule(void *pvImageBase)
{
    /* fake */
    if (RT_UNLIKELY(g_u32FakeMode))
    {
#ifdef VBOX_WITH_IDT_PATCHING
        g_u8Interrupt = 3;
        RTMemExecFree(*(void **)&g_pfnCallVMMR0);
        g_pfnCallVMMR0 = NULL;
#endif
        g_pvVMMR0 = NIL_RTR0PTR;
        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_IDT_PATCHING
    /*
     * There is one special module. When this is freed we'll
     * free the IDT entry that goes with it.
     *
     * Note that we don't keep count of VMMR0.r0 loads here, so the
     *      first unload will free it.
     */
    if (    (RTR0PTR)pvImageBase == g_pvVMMR0
        &&  g_u8Interrupt != 3)
    {
        SUPIDTREMOVE Req;
        Req.Hdr.u32Cookie = g_u32Cookie;
        Req.Hdr.u32SessionCookie = g_u32SessionCookie;
        Req.Hdr.cbIn = SUP_IOCTL_IDT_REMOVE_SIZE_IN;
        Req.Hdr.cbOut = SUP_IOCTL_IDT_REMOVE_SIZE_OUT;
        Req.Hdr.fFlags = SUPREQHDR_FLAGS_DEFAULT;
        Req.Hdr.rc = VERR_INTERNAL_ERROR;
        int rc = suplibOsIOCtl(SUP_IOCTL_IDT_REMOVE, &Req, SUP_IOCTL_IDT_REMOVE_SIZE);
        if (RT_SUCCESS(rc))
            rc = Req.Hdr.rc;
        AssertRC(rc);
        g_u8Interrupt = 3;
        RTMemExecFree(*(void **)&g_pfnCallVMMR0);
        g_pfnCallVMMR0 = NULL;
    }
#endif /* VBOX_WITH_IDT_PATCHING */

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
    int rc = suplibOsIOCtl(SUP_IOCTL_LDR_FREE, &Req, SUP_IOCTL_LDR_FREE_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    if (    RT_SUCCESS(rc)
        &&  (RTR0PTR)pvImageBase == g_pvVMMR0)
        g_pvVMMR0 = NIL_RTR0PTR;
    return rc;
}


SUPR3DECL(int) SUPGetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue)
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
    int rc = suplibOsIOCtl(SUP_IOCTL_LDR_GET_SYMBOL, &Req, SUP_IOCTL_LDR_GET_SYMBOL_SIZE);
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;
    if (RT_SUCCESS(rc))
        *ppvValue = (void *)Req.u.Out.pvSymbol;
    return rc;
}


SUPR3DECL(int) SUPLoadVMM(const char *pszFilename)
{
    void *pvImageBase;
    return SUPLoadModule(pszFilename, "VMMR0.r0", &pvImageBase);
}


SUPR3DECL(int) SUPUnloadVMM(void)
{
    return SUPFreeModule((void*)g_pvVMMR0);
}


SUPR3DECL(int) SUPGipGetPhys(PRTHCPHYS pHCPhys)
{
    if (g_pSUPGlobalInfoPage)
    {
        *pHCPhys = g_HCPhysSUPGlobalInfoPage;
        return VINF_SUCCESS;
    }
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_WRONG_ORDER;
}

