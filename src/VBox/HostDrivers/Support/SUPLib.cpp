/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Shared code:
 * Support library that implements the basic lowlevel OS interfaces
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/** @page   pg_sup          SUP - The Support Library
 *
 * The support library is responsible for providing facilities to load
 * VMM Host Ring-0 code, to call Host VMM Ring-0 code from Ring-3 Host
 * code, and to pin down physical memory.
 *
 * The VMM Host Ring-0 code can be combined in the support driver if
 * permitted by kernel module license policies. If it is not combined
 * it will be externalized in a Win32 PE binary and will use the PDM
 * PE loader to load it into memory.
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
#ifdef VBOX_WITHOUT_IDT_PATCHING
# include <VBox/vmm.h>
#endif
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/ldr.h>
#include <iprt/asm.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"

#include <stdlib.h>



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** R0 VMM module name. */
#define VMMR0_NAME      "VMMR0"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DECLCALLBACK(int) FNCALLVMMR0(PVM pVM, unsigned uOperation, void *pvArg);
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
DECLEXPORT(PCSUPGLOBALINFOPAGE) g_pSUPGlobalInfoPage;
/** Address of the ring-0 mapping of the GIP. */
static PCSUPGLOBALINFOPAGE      g_pSUPGlobalInfoPageR0;
/** The physical address of the GIP. */
static RTHCPHYS                 g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS;

/** The negotiated cookie. */
uint32_t            g_u32Cookie = 0;
/** The negotiated session cookie. */
uint32_t            g_u32SessionCookie;
/** Session handle. */
PSUPDRVSESSION      g_pSession;
/** R0 SUP Functions used for resolving referenced to the SUPR0 module. */
static PSUPQUERYFUNCS_OUT g_pFunctions;

#ifndef VBOX_WITHOUT_IDT_PATCHING
/** The negotiated interrupt number. */
static uint8_t      g_u8Interrupt = 3;
/** Pointer to the generated code fore calling VMMR0. */
static PFNCALLVMMR0 g_pfnCallVMMR0;
#endif
/** VMMR0 Load Address. */
static void        *g_pvVMMR0 = NULL;
/** Init counter. */
static unsigned     g_cInits = 0;
/** Fake mode indicator. (~0 at first, 0 or 1 after first test) */
static uint32_t     g_u32FakeMode = ~0;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int supLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase);
#ifndef VBOX_WITHOUT_IDT_PATCHING
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
     * Check if already initialized.
     */
    if (ppSession)
        *ppSession = g_pSession;
    if (g_cInits++ > 0)
        return VINF_SUCCESS;

    /*
     * Check for fake mode.
     * Fake mode is used when we're doing smoke testing and debugging.
     * It's also useful on platforms where we haven't root access or which
     * we haven't ported the support driver to.
     */
    if (g_u32FakeMode == ~0U)
    {
        const char *psz = getenv("VBOX_SUPLIB_FAKE");
        if (psz && !strcmp(psz, "fake"))
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 1, ~0U);
        else
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 0, ~0U);
    }
    if (g_u32FakeMode)
    {
        Log(("SUP: Fake mode!\n"));

        /* fake r0 functions. */
        g_pFunctions = (PSUPQUERYFUNCS_OUT)RTMemAllocZ(RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions[8]));
        if (g_pFunctions)
        {
            g_pFunctions->aFunctions[0].pfn = (void *)0xefefefef;
            strcpy(g_pFunctions->aFunctions[0].szName, "SUPR0ContAlloc");
            g_pFunctions->aFunctions[1].pfn = (void *)0xefefefdf;
            strcpy(g_pFunctions->aFunctions[1].szName, "SUPR0ContFree");
            g_pFunctions->aFunctions[2].pfn = (void *)0xefefefcf;
            strcpy(g_pFunctions->aFunctions[2].szName, "SUPR0LockMem");
            g_pFunctions->aFunctions[3].pfn = (void *)0xefefefbf;
            strcpy(g_pFunctions->aFunctions[3].szName, "SUPR0UnlockMem");
            g_pFunctions->aFunctions[4].pfn = (void *)0xefefefaf;
            strcpy(g_pFunctions->aFunctions[4].szName, "SUPR0LockedAlloc");
            g_pFunctions->aFunctions[5].pfn = (void *)0xefefef9f;
            strcpy(g_pFunctions->aFunctions[5].szName, "SUPR0LockedFree");
            g_pFunctions->aFunctions[6].pfn = (void *)0xefefef8f;
            strcpy(g_pFunctions->aFunctions[6].szName, "SUPR0Printf");
            g_pFunctions->cFunctions = 7;
            g_pSession = (PSUPDRVSESSION)(void *)g_pFunctions;
            if (ppSession)
                *ppSession = g_pSession;
#ifndef VBOX_WITHOUT_IDT_PATCHING
            Assert(g_u8Interrupt == 3);
#endif
            /* fake the GIP. */
            g_pSUPGlobalInfoPage = (PCSUPGLOBALINFOPAGE)RTMemPageAlloc(PAGE_SIZE);
            if (g_pSUPGlobalInfoPage)
            {
                g_pSUPGlobalInfoPageR0 = g_pSUPGlobalInfoPage;
                g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS & ~(RTHCPHYS)PAGE_OFFSET_MASK;
                /* the page is supposed to be invalid, so don't set a correct magic. */
                return VINF_SUCCESS;
            }
            RTMemFree(g_pFunctions);
            g_pFunctions = NULL;
        }
        return VERR_NO_MEMORY;
    }

    /**
     * Open the support driver.
     */
    int rc = suplibOsInit(cbReserve);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Negotiate the cookie.
         */
        SUPCOOKIE_IN    In;
        SUPCOOKIE_OUT   Out = {0,0};
        strcpy(In.szMagic, SUPCOOKIE_MAGIC);
        In.u32Version = SUPDRVIOC_VERSION;
        rc = suplibOsIOCtl(SUP_IOCTL_COOKIE, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_SUCCESS(rc))
        {
            if (Out.u32Version == SUPDRVIOC_VERSION)
            {
                /*
                 * Query the functions.
                 */
                SUPQUERYFUNCS_IN    FuncsIn;
                FuncsIn.u32Cookie           = Out.u32Cookie;
                FuncsIn.u32SessionCookie    = Out.u32SessionCookie;
                unsigned            cbFuncsOut = RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions[Out.cFunctions]);
                PSUPQUERYFUNCS_OUT  pFuncsOut = (PSUPQUERYFUNCS_OUT)RTMemAllocZ(cbFuncsOut);
                if (pFuncsOut)
                {
                    rc = suplibOsIOCtl(SUP_IOCTL_QUERY_FUNCS, &FuncsIn, sizeof(FuncsIn), pFuncsOut, cbFuncsOut);
                    if (VBOX_SUCCESS(rc))
                    {
                        g_u32Cookie         = Out.u32Cookie;
                        g_u32SessionCookie  = Out.u32SessionCookie;
                        g_pSession          = Out.pSession;
                        g_pFunctions        = pFuncsOut;
                        if (ppSession)
                            *ppSession = Out.pSession;

                        /*
                         * Map the GIP into userspace.
                         * This is an optional feature, so we will ignore any failures here.
                         */
                        if (!g_pSUPGlobalInfoPage)
                        {
                            SUPGIPMAP_IN GipIn = {0};
                            SUPGIPMAP_OUT GipOut = {NULL, 0};
                            GipIn.u32Cookie           = Out.u32Cookie;
                            GipIn.u32SessionCookie    = Out.u32SessionCookie;
                            rc = suplibOsIOCtl(SUP_IOCTL_GIP_MAP, &GipIn, sizeof(GipIn), &GipOut, sizeof(GipOut));
                            if (VBOX_SUCCESS(rc))
                            {
                                ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, GipOut.HCPhysGip);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, (void *)GipOut.pGipR3, NULL);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, (void *)GipOut.pGipR0, NULL);
                            }
                            else
                                rc = VINF_SUCCESS;
                        }
                        return rc;
                    }
                    RTMemFree(pFuncsOut);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
        }

        suplibOsTerm();
    }
    AssertMsgFailed(("SUPInit() failed rc=%Vrc\n", rc));
    g_cInits--;

    return rc;
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
#ifndef VBOX_WITHOUT_IDT_PATCHING
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
    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPGETPAGINGMODE_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    SUPGETPAGINGMODE_OUT Out = {SUPPAGINGMODE_INVALID};
    int rc;
    if (!g_u32FakeMode)
    {
        rc = suplibOsIOCtl(SUP_IOCTL_GET_PAGING_MODE, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_FAILURE(rc))
            Out.enmMode = SUPPAGINGMODE_INVALID;
    }
    else
        Out.enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;

    return Out.enmMode;
}

SUPR3DECL(int) SUPCallVMMR0Ex(PVM pVM, unsigned uOperation, void *pvArg, unsigned cbArg)
{
    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCALLVMMR0_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pVM              = pVM;
    In.uOperation       = uOperation;
    In.cbArg            = cbArg;
    In.pvArg            = pvArg;
    Assert(!g_u32FakeMode);
    SUPCALLVMMR0_OUT Out = {VINF_SUCCESS};
    int rc = suplibOsIOCtl(SUP_IOCTL_CALL_VMMR0, &In, sizeof(In), &Out, sizeof(Out));
    if (VBOX_SUCCESS(rc))
        rc = Out.rc;
    return rc;
}


SUPR3DECL(int) SUPCallVMMR0(PVM pVM, unsigned uOperation, void *pvArg)
{
#ifndef VBOX_WITHOUT_IDT_PATCHING
    return g_pfnCallVMMR0(pVM, uOperation, pvArg);
#else
    if (uOperation == VMMR0_DO_RUN_GC)
    {
        Assert(!pvArg);
        return suplibOSIOCtlFast(SUP_IOCTL_FAST_DO_RAW_RUN);
    }
    if (uOperation == VMMR0_HWACC_RUN_GUEST)
    {
        Assert(!pvArg);
        return suplibOSIOCtlFast(SUP_IOCTL_FAST_DO_HWACC_RUN);
    }
    return SUPCallVMMR0Ex(pVM, uOperation, pvArg, pvArg ? sizeof(pvArg) : 0);
#endif
}


SUPR3DECL(int) SUPSetVMForFastIOCtl(PVMR0 pVMR0)
{
    SUPSETVMFORFAST_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pVMR0            = pVMR0;
    Assert(!g_u32FakeMode);
    return suplibOsIOCtl(SUP_IOCTL_SET_VM_FOR_FAST, &In, sizeof(In), NULL, 0);
}


SUPR3DECL(int) SUPPageLock(void *pvStart, size_t cbMemory, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));
    AssertMsg(RT_ALIGN_Z(cbMemory, PAGE_SIZE) == cbMemory, ("cbMemory (%#zx) must be page aligned\n", cbMemory));
    AssertPtr(paPages);

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPPINPAGES_IN      In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pv               = pvStart;
    In.cb               = (uint32_t)cbMemory; AssertRelease(In.cb == cbMemory);
    PSUPPINPAGES_OUT pOut = (PSUPPINPAGES_OUT)(void*)paPages;
    Assert(RT_OFFSETOF(SUPPINPAGES_OUT, aPages) == 0 && sizeof(paPages[0]) == sizeof(pOut->aPages[0]));
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_PINPAGES, &In, sizeof(In), pOut, RT_OFFSETOF(SUPPINPAGES_OUT, aPages[cbMemory >> PAGE_SHIFT]));
    else
    {
        /* fake a successfull result. */
        RTHCPHYS    Phys = (uintptr_t)pvStart + PAGE_SIZE * 1024;
        unsigned    iPage = (unsigned)cbMemory >> PAGE_SHIFT;
        while (iPage-- > 0)
            paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        rc = VINF_SUCCESS;
    }

    return rc;
}


SUPR3DECL(int) SUPPageUnlock(void *pvStart)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPUNPINPAGES_IN  In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pv               = pvStart;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_UNPINPAGES, &In, sizeof(In), NULL, 0);
    else
        rc = VINF_SUCCESS;

    return rc;
}


SUPR3DECL(void *) SUPContAlloc(unsigned cb, PRTHCPHYS pHCPhys)
{
    return SUPContAlloc2(cb, NULL, pHCPhys);
}


SUPR3DECL(void *) SUPContAlloc2(unsigned cb, void **ppvR0, PRTHCPHYS pHCPhys)
{
    /*
     * Validate.
     */
    AssertMsg(cb > 64 && cb < PAGE_SIZE * 256, ("cb=%d must be > 64 and < %d (256 pages)\n", cb, PAGE_SIZE * 256));
    AssertPtr(pHCPhys);
    *pHCPhys = NIL_RTHCPHYS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTALLOC_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.cb               = RT_ALIGN_32(cb, PAGE_SIZE);
    SUPCONTALLOC_OUT    Out;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_CONT_ALLOC, &In, sizeof(In), &Out, sizeof(Out));
    else
    {
        rc = SUPPageAlloc(In.cb >> PAGE_SHIFT, &Out.pvR3);
        Out.HCPhys = (uintptr_t)Out.pvR3 + (PAGE_SHIFT * 1024);
        Out.pvR0 = Out.pvR3;
    }
    if (VBOX_SUCCESS(rc))
    {
        *pHCPhys = (RTHCPHYS)Out.HCPhys;
        if (ppvR0)
            *ppvR0 = Out.pvR0;
        return Out.pvR3;
    }

    return NULL;
}


SUPR3DECL(int) SUPContFree(void *pv)
{
    /*
     * Validate.
     */
    AssertPtr(pv);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTFREE_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pv               = pv;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_CONT_FREE, &In, sizeof(In), NULL, 0);
    else
        rc = SUPPageFree(pv);

    return rc;
}


SUPR3DECL(int) SUPLowAlloc(unsigned cPages, void **ppvPages, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertMsg(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages));
    AssertPtr(ppvPages);
    *ppvPages = NULL;
    AssertPtr(paPages);

    int rc;
    if (!g_u32FakeMode)
    {
        /*
         * Issue IOCtl to the SUPDRV kernel module.
         */
        SUPLOWALLOC_IN      In;
        In.u32Cookie        = g_u32Cookie;
        In.u32SessionCookie = g_u32SessionCookie;
        In.cPages           = cPages;
        size_t              cbOut = RT_OFFSETOF(SUPLOWALLOC_OUT, aPages[cPages]);
        PSUPLOWALLOC_OUT    pOut = (PSUPLOWALLOC_OUT)RTMemAllocZ(cbOut);
        if (pOut)
        {
            rc = suplibOsIOCtl(SUP_IOCTL_LOW_ALLOC, &In, sizeof(In), pOut, cbOut);
            if (VBOX_SUCCESS(rc))
            {
                *ppvPages = pOut->pvVirt;
                AssertCompile(sizeof(paPages[0]) == sizeof(pOut->aPages[0]));
                memcpy(paPages, &pOut->aPages[0], sizeof(paPages[0]) * cPages);
            }
            RTMemFree(pOut);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        rc = SUPPageAlloc(cPages, ppvPages);
        if (VBOX_SUCCESS(rc))
        {
            /* fake physical addresses. */
            RTHCPHYS    Phys = (uintptr_t)*ppvPages + PAGE_SIZE * 1024;
            unsigned    iPage = cPages;
            while (iPage-- > 0)
                paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        }
    }

    return rc;
}


SUPR3DECL(int) SUPLowFree(void *pv)
{
    /*
     * Validate.
     */
    AssertPtr(pv);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPLOWFREE_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pv               = pv;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_LOW_FREE, &In, sizeof(In), NULL, 0);
    else
        rc = SUPPageFree(pv);

    return rc;
}


SUPR3DECL(int) SUPPageAlloc(size_t cPages, void **ppvPages)
{
    /*
     * Validate.
     */
    if (cPages == 0)
    {
        AssertMsgFailed(("Invalid param cPages=0, must be > 0\n"));
        return VERR_INVALID_PARAMETER;
    }
    AssertPtr(ppvPages);
    if (!ppvPages)
        return VERR_INVALID_PARAMETER;
    *ppvPages = NULL;

    /*
     * Call OS specific worker.
     */
    return suplibOsPageAlloc(cPages, ppvPages);
}


SUPR3DECL(int) SUPPageFree(void *pvPages)
{
    /*
     * Validate.
     */
    AssertPtr(pvPages);
    if (!pvPages)
        return VINF_SUCCESS;

    /*
     * Call OS specific worker.
     */
    return suplibOsPageFree(pvPages);
}


SUPR3DECL(int) SUPLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase)
{
    /*
     * Load the module.
     * If it's VMMR0.r0 we need to install the IDTE.
     */
    int rc = supLoadModule(pszFilename, pszModule, ppvImageBase);
#ifndef VBOX_WITHOUT_IDT_PATCHING
    if (    VBOX_SUCCESS(rc)
        &&  !strcmp(pszModule, "VMMR0.r0"))
    {
        rc = supInstallIDTE();
        if (VBOX_FAILURE(rc))
            SUPFreeModule(*ppvImageBase);
    }
#endif /* VBOX_WITHOUT_IDT_PATCHING */

    return rc;
}


#ifndef VBOX_WITHOUT_IDT_PATCHING
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
#ifdef __AMD64__
    /*
     * reg params:
     *      <GCC>   <MSC>   <argument>
     *      rdi     rcx     pVM
     *      esi     edx     uOperation
     *      rdx     r8      pvArg
     *
     *      eax     eax     [g_u32Gookie]
     */
#ifndef __WIN__
*pb++ = 0xcc; /* fix me!! */
*pb++ = 0xc3;
#endif
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
        SUPIDTINSTALL_IN  In;
        In.u32Cookie        = g_u32Cookie;
        In.u32SessionCookie = g_u32SessionCookie;
        SUPIDTINSTALL_OUT Out = {3};

        rc = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_SUCCESS(rc))
        {
            g_u8Interrupt = Out.u8Idt;
            rc = suplibGenerateCallVMMR0(Out.u8Idt);
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
            if (VBOX_FAILURE(rc2))
            {
                u64AffMaskPatched &= ~u64Mask;
                Log(("SUPLoadVMM: Failed to set affinity to cpu no. %d, rc=%Vrc.\n", i, rc2));
                continue;
            }

            /* Patch the CPU. */
            SUPIDTINSTALL_IN  In;
            In.u32Cookie        = g_u32Cookie;
            In.u32SessionCookie = g_u32SessionCookie;
            SUPIDTINSTALL_OUT Out = {3};

            rc2 = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &In, sizeof(In), &Out, sizeof(Out));
            if (VBOX_SUCCESS(rc2))
            {
                if (!cCpusPatched)
                {
                    g_u8Interrupt = Out.u8Idt;
                    rc2 = suplibGenerateCallVMMR0(Out.u8Idt);
                    if (VBOX_FAILURE(rc))
                        rc2 = rc;
                }
                else
                    Assert(g_u8Interrupt == Out.u8Idt);
                cCpusPatched++;
            }
            else
            {

                Log(("SUPLoadVMM: Failed to patch cpu no. %d, rc=%Vrc.\n", i, rc2));
                if (VBOX_SUCCESS(rc))
                    rc = rc2;
            }
        }

        /* Fail if no CPUs was patched! */
        if (VBOX_SUCCESS(rc) && cCpusPatched <= 0)
            rc = VERR_GENERAL_FAILURE;
        /* Ignore failures if a CPU was patched. */
        else if (VBOX_FAILURE(rc) && cCpusPatched > 0)
        {
            /** @todo add an eventlog/syslog line out this. */
            rc = VINF_SUCCESS;
        }

        /* Set/restore the thread affinity. */
        if (VBOX_SUCCESS(rc))
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
#endif /* !VBOX_WITHOUT_IDT_PATCHING */


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

    /* iterate the function table. */
    int c = g_pFunctions->cFunctions;
    PSUPFUNC pFunc = &g_pFunctions->aFunctions[0];
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
     * Check the VMMR0.r0 module if loaded.
     */
    /** @todo call the SUPLoadModule caller.... */
    /** @todo proper reference counting and such. */
    if (g_pvVMMR0)
    {
        void *pvValue;
        if (!SUPGetSymbolR0(g_pvVMMR0, pszSymbol, &pvValue))
        {
            *pValue = (uintptr_t)pvValue;
            return VINF_SUCCESS;
        }
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
    c = g_pFunctions->cFunctions;
    pFunc = &g_pFunctions->aFunctions[0];
    while (c-- > 0)
    {
        AssertMsg2("%d: %s\n", g_pFunctions->cFunctions - c, pFunc->szName);
        pFunc++;
    }

    AssertMsgFailed(("%s is importing %s which we couldn't find\n", pvUser, pszSymbol));
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
    AssertReturn(strlen(pszModule) < SIZEOFMEMB(SUPLDROPEN_IN, szName), VERR_FILENAME_TOO_LONG);

    const bool fIsVMMR0 = !strcmp(pszModule, "VMMR0.r0");
    *ppvImageBase = NULL;

    /*
     * Open image file and figure its size.
     */
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(pszFilename, &hLdrMod);
    if (!VBOX_SUCCESS(rc))
        return rc;

    SUPLDRCALCSIZEARGS CalcArgs;
    CalcArgs.cbStrings = 0;
    CalcArgs.cSymbols = 0;
    CalcArgs.cbImage = RTLdrSize(hLdrMod);
    rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCalcSizeCB, &CalcArgs);
    if (VBOX_SUCCESS(rc))
    {
        const uint32_t  offSymTab = RT_ALIGN_32(CalcArgs.cbImage, 8);
        const uint32_t  offStrTab = offSymTab + CalcArgs.cSymbols * sizeof(SUPLDRSYM);
        const uint32_t  cbImage   = RT_ALIGN_32(offStrTab + CalcArgs.cbStrings, 8);

        /*
         * Open the R0 image.
         */
        SUPLDROPEN_IN OpenIn;
        OpenIn.u32Cookie        = g_u32Cookie;
        OpenIn.u32SessionCookie = g_u32SessionCookie;
        OpenIn.cbImage          = cbImage;
        strcpy(OpenIn.szName, pszModule);
        SUPLDROPEN_OUT OpenOut;
        if (!g_u32FakeMode)
            rc = suplibOsIOCtl(SUP_IOCTL_LDR_OPEN, &OpenIn, sizeof(OpenIn), &OpenOut, sizeof(OpenOut));
        else
        {
            OpenOut.fNeedsLoading = true;
            OpenOut.pvImageBase = (void *)0xef423420;
        }
        *ppvImageBase = OpenOut.pvImageBase;
        if (    VBOX_SUCCESS(rc)
            &&  OpenOut.fNeedsLoading)
        {
            /*
             * We need to load it.
             * Allocate memory for the image bits.
             */
            unsigned        cbIn = RT_OFFSETOF(SUPLDRLOAD_IN, achImage[cbImage]);
            PSUPLDRLOAD_IN  pIn = (PSUPLDRLOAD_IN)RTMemTmpAlloc(cbIn);
            if (pIn)
            {
                /*
                 * Get the image bits.
                 */
                rc = RTLdrGetBits(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase,
                                  supLoadModuleResolveImport, (void *)pszModule);

                /*
                 * Get the entry points.
                 */
                RTUINTPTR VMMR0Entry = 0;
                RTUINTPTR ModuleInit = 0;
                RTUINTPTR ModuleTerm = 0;
                if (fIsVMMR0 && VBOX_SUCCESS(rc))
                    rc = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "VMMR0Entry", &VMMR0Entry);
                if (VBOX_SUCCESS(rc))
                {
                    rc = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "ModuleInit", &ModuleInit);
                    if (VBOX_FAILURE(rc))
                        ModuleInit = 0;

                    rc = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "ModuleTerm", &ModuleTerm);
                    if (VBOX_FAILURE(rc))
                        ModuleTerm = 0;
                }

                /*
                 * Create the symbol and string tables.
                 */
                SUPLDRCREATETABSARGS CreateArgs;
                CreateArgs.cbImage = CalcArgs.cbImage;
                CreateArgs.pSym    = (PSUPLDRSYM)&pIn->achImage[offSymTab];
                CreateArgs.pszBase =     (char *)&pIn->achImage[offStrTab];
                CreateArgs.psz     = CreateArgs.pszBase;
                rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCreateTabsCB, &CreateArgs);
                if (VBOX_SUCCESS(rc))
                {
                    AssertRelease((size_t)(CreateArgs.psz - CreateArgs.pszBase) <= CalcArgs.cbStrings);
                    AssertRelease((size_t)(CreateArgs.pSym - (PSUPLDRSYM)&pIn->achImage[offSymTab]) <= CalcArgs.cSymbols);

                    /*
                     * Upload the image.
                     */
                    pIn->u32Cookie                  = g_u32Cookie;
                    pIn->u32SessionCookie           = g_u32SessionCookie;
                    pIn->pfnModuleInit              = (PFNR0MODULEINIT)(uintptr_t)ModuleInit;
                    pIn->pfnModuleTerm              = (PFNR0MODULETERM)(uintptr_t)ModuleTerm;
                    if (fIsVMMR0)
                    {
                        pIn->eEPType                = pIn->EP_VMMR0;
                        pIn->EP.VMMR0.pvVMMR0       = OpenOut.pvImageBase;
                        pIn->EP.VMMR0.pvVMMR0Entry  = (void *)(uintptr_t)VMMR0Entry;
                    }
                    else
                        pIn->eEPType                = pIn->EP_NOTHING;
                    pIn->offStrTab                  = offStrTab;
                    pIn->cbStrTab                   = CalcArgs.cbStrings;
                    pIn->offSymbols                 = offSymTab;
                    pIn->cSymbols                   = CalcArgs.cSymbols;
                    pIn->cbImage                    = cbImage;
                    pIn->pvImageBase                = OpenOut.pvImageBase;
                    if (!g_u32FakeMode)
                        rc = suplibOsIOCtl(SUP_IOCTL_LDR_LOAD, pIn, cbIn, NULL, 0);
                    else
                        rc = VINF_SUCCESS;
                    if (    VBOX_SUCCESS(rc)
                        ||  rc == VERR_ALREADY_LOADED /* this is because of a competing process. */
                       )
                    {
                        if (fIsVMMR0)
                            g_pvVMMR0 = OpenOut.pvImageBase;
                        RTMemTmpFree(pIn);
                        RTLdrClose(hLdrMod);
                        return VINF_SUCCESS;
                    }
                }
                RTMemTmpFree(pIn);
            }
            else
            {
                AssertMsgFailed(("failed to allocated %d bytes for SUPLDRLOAD_IN structure!\n", cbIn));
                rc = VERR_NO_TMP_MEMORY;
            }
        }
    }
    RTLdrClose(hLdrMod);
    return rc;
}


SUPR3DECL(int) SUPFreeModule(void *pvImageBase)
{
    /*
     * There is one special module. When this is freed we'll
     * free the IDT entry that goes with it.
     *
     * Note that we don't keep count of VMMR0.r0 loads here, so the
     *      first unload will free it.
     */
    if (pvImageBase == g_pvVMMR0)
    {
        /*
         * This is the point where we remove the IDT hook. We do
         * that before unloading the R0 VMM part.
         */
        if (g_u32FakeMode)
        {
#ifndef VBOX_WITHOUT_IDT_PATCHING
            g_u8Interrupt = 3;
            RTMemExecFree(*(void **)&g_pfnCallVMMR0);
            g_pfnCallVMMR0 = NULL;
#endif
            g_pvVMMR0 = NULL;
            return VINF_SUCCESS;
        }

#ifndef VBOX_WITHOUT_IDT_PATCHING
        /*
         * Uninstall IDT entry.
         */
        int rc = 0;
        if (g_u8Interrupt != 3)
        {
            SUPIDTREMOVE_IN  In;
            In.u32Cookie        = g_u32Cookie;
            In.u32SessionCookie = g_u32SessionCookie;
            rc = suplibOsIOCtl(SUP_IOCTL_IDT_REMOVE, &In, sizeof(In), NULL, 0);
            g_u8Interrupt = 3;
            RTMemExecFree(*(void **)&g_pfnCallVMMR0);
            g_pfnCallVMMR0 = NULL;
        }
#endif
    }

    /*
     * Free the requested module.
     */
    SUPLDRFREE_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvImageBase      = pvImageBase;
    int rc = VINF_SUCCESS;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_LDR_FREE, &In, sizeof(In), NULL, 0);
    if (    VBOX_SUCCESS(rc)
        &&  pvImageBase == g_pvVMMR0)
        g_pvVMMR0 = NULL;
    return rc;
}


SUPR3DECL(int) SUPGetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue)
{
    *ppvValue = NULL;

    /*
     * Do ioctl.
     */
    size_t              cchSymbol = strlen(pszSymbol);
    const size_t        cbIn = RT_OFFSETOF(SUPLDRGETSYMBOL_IN, szSymbol[cchSymbol + 1]);
    SUPLDRGETSYMBOL_OUT Out = { NULL };
    PSUPLDRGETSYMBOL_IN pIn = (PSUPLDRGETSYMBOL_IN)alloca(cbIn);
    pIn->u32Cookie        = g_u32Cookie;
    pIn->u32SessionCookie = g_u32SessionCookie;
    pIn->pvImageBase      = pvImageBase;
    memcpy(pIn->szSymbol, pszSymbol, cchSymbol + 1);
    int rc = suplibOsIOCtl(SUP_IOCTL_LDR_GET_SYMBOL, pIn, cbIn, &Out, sizeof(Out));
    if (VBOX_SUCCESS(rc))
        *ppvValue = Out.pvSymbol;
    return rc;
}


SUPR3DECL(int) SUPLoadVMM(const char *pszFilename)
{
    void *pvImageBase;
    return SUPLoadModule(pszFilename, "VMMR0.r0", &pvImageBase);
}


SUPR3DECL(int) SUPUnloadVMM(void)
{
    return SUPFreeModule(g_pvVMMR0);
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

