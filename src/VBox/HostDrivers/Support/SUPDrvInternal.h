/* $Revision$ */
/** @file
 * VirtualBox Support Driver - Internal header.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

#ifndef ___SUPDrvInternal_h
#define ___SUPDrvInternal_h


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/sup.h>

#include <iprt/assert.h>
#include <iprt/list.h>
#include <iprt/memobj.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/string.h>
#include <iprt/err.h>

#ifdef SUPDRV_AGNOSTIC
/* do nothing */

#elif defined(RT_OS_WINDOWS)
    RT_C_DECLS_BEGIN
#   if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#       define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#       define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#       define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#       define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#       pragma warning(disable : 4163)
#       include <ntddk.h>
#       pragma warning(default : 4163)
#       undef  _InterlockedExchange
#       undef  _InterlockedExchangeAdd
#       undef  _InterlockedCompareExchange
#       undef  _InterlockedAddLargeStatistic
#       undef  _interlockedbittestandset
#       undef  _interlockedbittestandreset
#       undef  _interlockedbittestandset64
#       undef  _interlockedbittestandreset64
#   else
#       include <ntddk.h>
#   endif
#   include <memory.h>
#   define memcmp(a,b,c) mymemcmp(a,b,c)
    int VBOXCALL mymemcmp(const void *, const void *, size_t);
    RT_C_DECLS_END

#elif defined(RT_OS_LINUX)
#   include <linux/version.h>
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
#    include <generated/autoconf.h>
#   else
#    ifndef AUTOCONF_INCLUDED
#     include <linux/autoconf.h>
#    endif
#   endif
#   if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#       define MODVERSIONS
#       if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 71)
#           include <linux/modversions.h>
#       endif
#   endif
#   if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)
#       undef ALIGN
#   endif
#   ifndef KBUILD_STR
#       if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#            define KBUILD_STR(s) s
#       else
#            define KBUILD_STR(s) #s
#       endif
#   endif
#   include <linux/string.h>
#   include <linux/spinlock.h>
#   include <linux/slab.h>
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#       include <linux/semaphore.h>
#   else /* older kernels */
#       include <asm/semaphore.h>
#   endif /* older kernels */
#   include <linux/timer.h>

#elif defined(RT_OS_DARWIN)
#   include <libkern/libkern.h>
#   include <iprt/string.h>

#elif defined(RT_OS_OS2)

#elif defined(RT_OS_FREEBSD)
#   define memset  libkern_memset /** @todo these are just hacks to get it compiling, check out later. */
#   define memcmp  libkern_memcmp
#   define strchr  libkern_strchr
#   define strrchr libkern_strrchr
#   define ffsl    libkern_ffsl
#   define fls     libkern_fls
#   define flsl    libkern_flsl
#   include <sys/libkern.h>
#   undef  memset
#   undef  memcmp
#   undef  strchr
#   undef  strrchr
#   undef  ffs
#   undef  ffsl
#   undef  fls
#   undef  flsl
#   include <iprt/string.h>

#elif defined(RT_OS_SOLARIS)
#   include <sys/cmn_err.h>
#   include <iprt/string.h>

#else
#   error "unsupported OS."
#endif

#include "SUPDrvIOC.h"
#include "SUPDrvIDC.h"



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/*
 * Hardcoded cookies.
 */
#define BIRD        0x64726962 /* 'bird' */
#define BIRD_INV    0x62697264 /* 'drib' */


#ifdef RT_OS_WINDOWS
/** Use a normal mutex for the loader so we remain at the same IRQL after
 * taking it.
 * @todo fix the mutex implementation on linux and make this the default. */
# define SUPDRV_USE_MUTEX_FOR_LDR

/** Use a normal mutex for the GIP so we remain at the same IRQL after
 * taking it.
 * @todo fix the mutex implementation on linux and make this the default. */
# define SUPDRV_USE_MUTEX_FOR_GIP
#endif


/**
 * OS debug print macro.
 */
#define OSDBGPRINT(a) SUPR0Printf a


/** @name Context values for the per-session handle tables.
 * The context value is used to distinguish between the different kinds of
 * handles, making the handle table API do all the work.
 * @{ */
/** Handle context value for single release event handles.  */
#define SUPDRV_HANDLE_CTX_EVENT         ((void *)(uintptr_t)(SUPDRVOBJTYPE_SEM_EVENT))
/** Handle context value for multiple release event handles.  */
#define SUPDRV_HANDLE_CTX_EVENT_MULTI   ((void *)(uintptr_t)(SUPDRVOBJTYPE_SEM_EVENT_MULTI))
/** @} */


/**
 * Validates a session pointer.
 *
 * @returns true/false accordingly.
 * @param   pSession    The session.
 */
#define SUP_IS_SESSION_VALID(pSession)  \
    (   VALID_PTR(pSession) \
     && pSession->u32Cookie == BIRD_INV)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to the device extension. */
typedef struct SUPDRVDEVEXT *PSUPDRVDEVEXT;


/**
 * Memory reference types.
 */
typedef enum
{
    /** Unused entry */
    MEMREF_TYPE_UNUSED = 0,
    /** Locked memory (r3 mapping only). */
    MEMREF_TYPE_LOCKED,
    /** Continuous memory block (r3 and r0 mapping). */
    MEMREF_TYPE_CONT,
    /** Low memory block (r3 and r0 mapping). */
    MEMREF_TYPE_LOW,
    /** Memory block (r3 and r0 mapping). */
    MEMREF_TYPE_MEM,
    /** Locked memory (r3 mapping only) allocated by the support driver. */
    MEMREF_TYPE_PAGE,
    /** Blow the type up to 32-bit and mark the end. */
    MEMREF_TYPE_32BIT_HACK = 0x7fffffff
} SUPDRVMEMREFTYPE, *PSUPDRVMEMREFTYPE;


/**
 * Structure used for tracking memory a session
 * references in one way or another.
 */
typedef struct SUPDRVMEMREF
{
    /** The memory object handle. */
    RTR0MEMOBJ                      MemObj;
    /** The ring-3 mapping memory object handle. */
    RTR0MEMOBJ                      MapObjR3;
    /** Type of memory. */
    SUPDRVMEMREFTYPE                eType;
} SUPDRVMEMREF, *PSUPDRVMEMREF;


/**
 * Bundle of locked memory ranges.
 */
typedef struct SUPDRVBUNDLE
{
    /** Pointer to the next bundle. */
    struct SUPDRVBUNDLE * volatile  pNext;
    /** Referenced memory. */
    SUPDRVMEMREF                    aMem[64];
    /** Number of entries used. */
    uint32_t volatile   cUsed;
} SUPDRVBUNDLE, *PSUPDRVBUNDLE;


/**
 * Loaded image.
 */
typedef struct SUPDRVLDRIMAGE
{
    /** Next in chain. */
    struct SUPDRVLDRIMAGE * volatile pNext;
    /** Pointer to the image. */
    void                           *pvImage;
    /** Pointer to the allocated image buffer.
     * pvImage is 32-byte aligned or it may governed by the native loader (this
     * member is NULL then). */
    void                           *pvImageAlloc;
    /** Size of the image including the tables. This is mainly for verification
     * of the load request. */
    uint32_t                        cbImageWithTabs;
    /** Size of the image. */
    uint32_t                        cbImageBits;
    /** The number of entries in the symbol table. */
    uint32_t                        cSymbols;
    /** Pointer to the symbol table. */
    PSUPLDRSYM                      paSymbols;
    /** The offset of the string table. */
    char                           *pachStrTab;
    /** Size of the string table. */
    uint32_t                        cbStrTab;
    /** Pointer to the optional module initialization callback. */
    PFNR0MODULEINIT                 pfnModuleInit;
    /** Pointer to the optional module termination callback. */
    PFNR0MODULETERM                 pfnModuleTerm;
    /** Service request handler. This is NULL for non-service modules. */
    PFNSUPR0SERVICEREQHANDLER       pfnServiceReqHandler;
    /** The ldr image state. (IOCtl code of last operation.) */
    uint32_t                        uState;
    /** Usage count. */
    uint32_t volatile               cUsage;
    /** Pointer to the device extension. */
    struct SUPDRVDEVEXT            *pDevExt;
#ifdef RT_OS_WINDOWS
    /** The section object for the loaded image (fNative=true). */
    void                           *pvNtSectionObj;
    /** Lock object. */
    RTR0MEMOBJ                      hMemLock;
#endif
#if defined(RT_OS_SOLARIS) && defined(VBOX_WITH_NATIVE_SOLARIS_LOADING)
    /** The Solaris module ID. */
    int                             idSolMod;
    /** Pointer to the module control structure. */
    struct modctl                  *pSolModCtl;
#endif
    /** Whether it's loaded by the native loader or not. */
    bool                            fNative;
    /** Image name. */
    char                            szName[32];
} SUPDRVLDRIMAGE, *PSUPDRVLDRIMAGE;


/** Image usage record. */
typedef struct SUPDRVLDRUSAGE
{
    /** Next in chain. */
    struct SUPDRVLDRUSAGE * volatile pNext;
    /** The image. */
    PSUPDRVLDRIMAGE                 pImage;
    /** Load count. */
    uint32_t volatile               cUsage;
} SUPDRVLDRUSAGE, *PSUPDRVLDRUSAGE;


/**
 * Component factory registration record.
 */
typedef struct SUPDRVFACTORYREG
{
    /** Pointer to the next registration. */
    struct SUPDRVFACTORYREG        *pNext;
    /** Pointer to the registered factory. */
    PCSUPDRVFACTORY                 pFactory;
    /** The session owning the factory.
     * Used for deregistration and session cleanup. */
    PSUPDRVSESSION                  pSession;
    /** Length of the name. */
    size_t                          cchName;
} SUPDRVFACTORYREG;
/** Pointer to a component factory registration record. */
typedef SUPDRVFACTORYREG *PSUPDRVFACTORYREG;
/** Pointer to a const component factory registration record. */
typedef SUPDRVFACTORYREG const *PCSUPDRVFACTORYREG;


/**
 * Registered object.
 * This takes care of reference counting and tracking data for access checks.
 */
typedef struct SUPDRVOBJ
{
    /** Magic value (SUPDRVOBJ_MAGIC). */
    uint32_t                        u32Magic;
    /** The object type. */
    SUPDRVOBJTYPE                   enmType;
    /** Pointer to the next in the global list. */
    struct SUPDRVOBJ * volatile     pNext;
    /** Pointer to the object destructor.
     * This may be set to NULL if the image containing the destructor get unloaded. */
    PFNSUPDRVDESTRUCTOR             pfnDestructor;
    /** User argument 1. */
    void                           *pvUser1;
    /** User argument 2. */
    void                           *pvUser2;
    /** The total sum of all per-session usage. */
    uint32_t volatile               cUsage;
    /** The creator user id. */
    RTUID                           CreatorUid;
    /** The creator group id. */
    RTGID                           CreatorGid;
    /** The creator process id. */
    RTPROCESS                       CreatorProcess;
} SUPDRVOBJ, *PSUPDRVOBJ;

/** Magic number for SUPDRVOBJ::u32Magic. (Dame Agatha Mary Clarissa Christie). */
#define SUPDRVOBJ_MAGIC             0x18900915
/** Dead number magic for SUPDRVOBJ::u32Magic. */
#define SUPDRVOBJ_MAGIC_DEAD        0x19760112

/**
 * The per-session object usage record.
 */
typedef struct SUPDRVUSAGE
{
    /** Pointer to the next in the list. */
    struct SUPDRVUSAGE * volatile   pNext;
    /** Pointer to the object we're recording usage for. */
    PSUPDRVOBJ                      pObj;
    /** The usage count. */
    uint32_t volatile               cUsage;
} SUPDRVUSAGE, *PSUPDRVUSAGE;


/**
 * Per session data.
 * This is mainly for memory tracking.
 */
typedef struct SUPDRVSESSION
{
    /** Pointer to the device extension. */
    PSUPDRVDEVEXT                   pDevExt;
    /** Session Cookie. */
    uint32_t                        u32Cookie;

    /** The VM associated with the session. */
    PVM                             pVM;
    /** Handle table for IPRT semaphore wrapper APIs.
     * Programmable from R0 and R3. */
    RTHANDLETABLE                   hHandleTable;
    /** Load usage records. (protected by SUPDRVDEVEXT::mtxLdr) */
    PSUPDRVLDRUSAGE volatile        pLdrUsage;

    /** Spinlock protecting the bundles and the GIP members. */
    RTSPINLOCK                      Spinlock;
    /** The ring-3 mapping of the GIP (readonly). */
    RTR0MEMOBJ                      GipMapObjR3;
    /** Set if the session is using the GIP. */
    uint32_t                        fGipReferenced;
    /** Bundle of locked memory objects. */
    SUPDRVBUNDLE                    Bundle;
    /** List of generic usage records. (protected by SUPDRVDEVEXT::SpinLock) */
    PSUPDRVUSAGE volatile           pUsage;

    /** The user id of the session. (Set by the OS part.) */
    RTUID                           Uid;
    /** The group id of the session. (Set by the OS part.) */
    RTGID                           Gid;
    /** The process (id) of the session. */
    RTPROCESS                       Process;
    /** Which process this session is associated with.
     * This is NIL_RTR0PROCESS for kernel sessions and valid for user ones. */
    RTR0PROCESS                     R0Process;
    /** Per session tracer specfic data. */
    uintptr_t                       uTracerData;
    /** The thread currently actively talking to the tracer. (One at the time!) */
    RTNATIVETHREAD                  hTracerCaller;
#ifndef SUPDRV_AGNOSTIC
# if defined(RT_OS_DARWIN)
    /** Pointer to the associated org_virtualbox_SupDrvClient object. */
    void                           *pvSupDrvClient;
    /** Whether this session has been opened or not. */
    bool                            fOpened;
# endif
# if defined(RT_OS_OS2)
    /** The system file number of this session. */
    uint16_t                        sfn;
    uint16_t                        Alignment; /**< Alignment */
# endif
# if defined(RT_OS_DARWIN) || defined(RT_OS_OS2) || defined(RT_OS_SOLARIS)
    /** Pointer to the next session with the same hash. */
    PSUPDRVSESSION                  pNextHash;
# endif
#endif /* !SUPDRV_AGNOSTIC */
} SUPDRVSESSION;


/**
 * Device extension.
 */
typedef struct SUPDRVDEVEXT
{
    /** Spinlock to serialize the initialization, usage counting and objects. */
    RTSPINLOCK                      Spinlock;

    /** List of registered objects. Protected by the spinlock. */
    PSUPDRVOBJ volatile             pObjs;
    /** List of free object usage records. */
    PSUPDRVUSAGE volatile           pUsageFree;

    /** Global cookie. */
    uint32_t                        u32Cookie;
    /** The actual size of SUPDRVSESSION. (SUPDRV_AGNOSTIC) */
    uint32_t                        cbSession;

    /** Loader mutex.
     * This protects pvVMMR0, pvVMMR0Entry, pImages and SUPDRVSESSION::pLdrUsage. */
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    RTSEMMUTEX                      mtxLdr;
#else
    RTSEMFASTMUTEX                  mtxLdr;
#endif

    /** VMM Module 'handle'.
     * 0 if the code VMM isn't loaded and Idt are nops. */
    void * volatile                 pvVMMR0;
    /** VMMR0EntryInt() pointer. */
    DECLR0CALLBACKMEMBER(int,       pfnVMMR0EntryInt, (PVM pVM, unsigned uOperation, void *pvArg));
    /** VMMR0EntryFast() pointer. */
    DECLR0CALLBACKMEMBER(void,      pfnVMMR0EntryFast, (PVM pVM, VMCPUID idCpu, unsigned uOperation));
    /** VMMR0EntryEx() pointer. */
    DECLR0CALLBACKMEMBER(int,       pfnVMMR0EntryEx, (PVM pVM, VMCPUID idCpu, unsigned uOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession));

    /** Linked list of loaded code. */
    PSUPDRVLDRIMAGE volatile        pLdrImages;

    /** @name These members for detecting whether an API caller is in ModuleInit.
     * Certain APIs are only permitted from ModuleInit, like for instance tracepoint
     * registration.
     * @{ */
    /** The image currently executing its ModuleInit. */
    PSUPDRVLDRIMAGE volatile        pLdrInitImage;
    /** The thread currently executing a ModuleInit function. */
    RTNATIVETHREAD volatile         hLdrInitThread;
    /** @} */


    /** GIP mutex.
     * Any changes to any of the GIP members requires ownership of this mutex,
     * except on driver init and termination. */
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSEMMUTEX                      mtxGip;
#else
    RTSEMFASTMUTEX                  mtxGip;
#endif
    /** GIP spinlock protecting GIP members during Mp events. */
    RTSPINLOCK                      hGipSpinlock;
    /** Pointer to the Global Info Page (GIP). */
    PSUPGLOBALINFOPAGE              pGip;
    /** The physical address of the GIP. */
    RTHCPHYS                        HCPhysGip;
    /** Number of processes using the GIP.
     * (The updates are suspend while cGipUsers is 0.)*/
    uint32_t volatile               cGipUsers;
    /** The ring-0 memory object handle for the GIP page. */
    RTR0MEMOBJ                      GipMemObj;
    /** The GIP timer handle. */
    PRTTIMER                        pGipTimer;
    /** If non-zero we've successfully called RTTimerRequestSystemGranularity(). */
    uint32_t                        u32SystemTimerGranularityGrant;
    /** The CPU id of the GIP master.
     * This CPU is responsible for the updating the common GIP data. */
    RTCPUID volatile                idGipMaster;

    /** Component factory mutex.
     * This protects pComponentFactoryHead and component factory querying. */
    RTSEMFASTMUTEX                  mtxComponentFactory;
    /** The head of the list of registered component factories. */
    PSUPDRVFACTORYREG               pComponentFactoryHead;

    /** Lock protecting The tracer members. */
    RTSEMFASTMUTEX                  mtxTracer;
    /** List of tracer providers (SUPDRVTPPROVIDER). */
    RTLISTANCHOR                    TracerProviderList;
    /** List of zombie tracer providers (SUPDRVTPPROVIDER). */
    RTLISTANCHOR                    TracerProviderZombieList;
    /** Pointer to the tracer registration record. */
    PCSUPDRVTRACERREG               pTracerOps;
    /** The ring-0 session of a native tracer provider. */
    PSUPDRVSESSION                  pTracerSession;
    /** The image containing the tracer. */
    PSUPDRVLDRIMAGE                 pTracerImage;
    /** The tracer helpers. */
    SUPDRVTRACERHLP                 TracerHlp;
    /** The number of session having opened the tracer currently. */
    uint32_t                        cTracerOpens;
    /** Set if the tracer is being unloaded. */
    bool                            fTracerUnloading;

    /*
     * Note! The non-agnostic bits must be a the very end of the structure!
     */
#ifndef SUPDRV_AGNOSTIC
# ifdef RT_OS_WINDOWS
    /** Callback object returned by ExCreateCallback. */
    PCALLBACK_OBJECT                pObjPowerCallback;
    /** Callback handle returned by ExRegisterCallback. */
    PVOID                           hPowerCallback;
# endif
#endif
} SUPDRVDEVEXT;


RT_C_DECLS_BEGIN

/*******************************************************************************
*   OS Specific Functions                                                      *
*******************************************************************************/
void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession);
bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc);
bool VBOXCALL   supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt);
int  VBOXCALL   supdrvOSEnableVTx(bool fEnabled);

/**
 * Try open the image using the native loader.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if native loading isn't supported.
 *
 * @param   pDevExt             The device globals.
 * @param   pImage              The image handle.  pvImage should be set on
 *                              success, pvImageAlloc can also be set if
 *                              appropriate.
 * @param   pszFilename         The file name - UTF-8, may containing UNIX
 *                              slashes on non-UNIX systems.
 */
int  VBOXCALL   supdrvOSLdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename);

/**
 * Validates an entry point address.
 *
 * Called before supdrvOSLdrLoad.
 *
 * @returns IPRT status code.
 * @param   pDevExt             The device globals.
 * @param   pImage              The image data (still in the open state).
 * @param   pv                  The address within the image.
 * @param   pbImageBits         The image bits as loaded by ring-3.
 */
int  VBOXCALL   supdrvOSLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage,
                                           void *pv, const uint8_t *pbImageBits);

/**
 * Load the image.
 *
 * @returns IPRT status code.
 * @param   pDevExt             The device globals.
 * @param   pImage              The image data (up to date). Adjust entrypoints
 *                              and exports if necessary.
 * @param   pbImageBits         The image bits as loaded by ring-3.
 * @param   pReq                Pointer to the request packet so that the VMMR0
 *                              entry points can be adjusted.
 */
int  VBOXCALL   supdrvOSLdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const uint8_t *pbImageBits, PSUPLDRLOAD pReq);


/**
 * Unload the image.
 *
 * @param   pDevExt             The device globals.
 * @param   pImage              The image data (mostly still valid).
 */
void VBOXCALL   supdrvOSLdrUnload(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);


/*******************************************************************************
*   Shared Functions                                                           *
*******************************************************************************/
/* SUPDrv.c */
int  VBOXCALL   supdrvIOCtl(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr);
int  VBOXCALL   supdrvIOCtlFast(uintptr_t uIOCtl, VMCPUID idCpu, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
int  VBOXCALL   supdrvIDC(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQHDR pReqHdr);
int  VBOXCALL   supdrvInitDevExt(PSUPDRVDEVEXT pDevExt, size_t cbSession);
void VBOXCALL   supdrvDeleteDevExt(PSUPDRVDEVEXT pDevExt);
int  VBOXCALL   supdrvCreateSession(PSUPDRVDEVEXT pDevExt, bool fUser, PSUPDRVSESSION *ppSession);
void VBOXCALL   supdrvCloseSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
void VBOXCALL   supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);

int  VBOXCALL   supdrvTracerInit(PSUPDRVDEVEXT pDevExt);
void VBOXCALL   supdrvTracerTerm(PSUPDRVDEVEXT pDevExt);
void VBOXCALL   supdrvTracerModuleUnloading(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);
void VBOXCALL   supdrvTracerCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
int  VBOXCALL   supdrvIOCtl_TracerOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, uint32_t uCookie, uintptr_t uArg);
int  VBOXCALL   supdrvIOCtl_TracerClose(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
int  VBOXCALL   supdrvIOCtl_TracerIOCtl(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, uintptr_t uCmd, uintptr_t uArg, int32_t *piRetVal);
extern PFNRT    g_pfnSupdrvProbeFireKernel;
DECLASM(void)   supdrvTracerProbeFireStub(void);


RT_C_DECLS_END

#endif

