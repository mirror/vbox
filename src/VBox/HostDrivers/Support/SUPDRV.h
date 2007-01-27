/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Shared code:
 * Shared definitions for all host platforms
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

#ifndef __SUPDRV_h__
#define __SUPDRV_h__


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/sup.h>
#ifdef USE_NEW_OS_INTERFACE
# include <iprt/memobj.h>
# include <iprt/time.h>
# include <iprt/timer.h>
#endif


#if defined(__WIN__)
    __BEGIN_DECLS
#   if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#       define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#       define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#       define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#       define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#       include <ntddk.h>
#       undef  _InterlockedExchange
#       undef  _InterlockedExchangeAdd
#       undef  _InterlockedCompareExchange
#       undef  _InterlockedAddLargeStatistic
#   else
#       include <ntddk.h>
#   endif
#   include <memory.h>
#   define memcmp(a,b,c) mymemcmp(a,b,c)
    int VBOXCALL mymemcmp(const void *, const void *, size_t);
    __END_DECLS

#elif defined(__LINUX__)
#   include <linux/autoconf.h>
#   include <linux/version.h>
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
#   include <asm/semaphore.h>

#elif defined(__DARWIN__)
#   include <libkern/libkern.h>
#   include <iprt/string.h>

#else
#   error "unsupported OS."
#endif

#include "SUPDRVIOC.h"



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/*
 * Hardcoded cookies.
 */
#define BIRD        0x64726962 /* 'bird' */
#define BIRD_INV    0x62697264 /* 'drib' */


/*
 * Win32
 */
#if defined(__WIN__)

/* debug printf */
# define OSDBGPRINT(a) DbgPrint a

/** Maximum number of bytes we try to lock down in one go.
 * This is supposed to have a limit right below 256MB, but this appears
 * to actually be much lower. The values here have been determined experimentally.
 */
#ifdef __WIN32__
# define MAX_LOCK_MEM_SIZE   (32*1024*1024) /* 32mb */
#endif
#ifdef __WIN64__
# define MAX_LOCK_MEM_SIZE   (24*1024*1024) /* 24mb */
#endif


/*
 * Linux
 */
#elif defined(__LINUX__)

/* check kernel version */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
# error Unsupported kernel version!
#endif

__BEGIN_DECLS
int  linux_dprintf(const char *format, ...);
__END_DECLS

/* debug printf */
# define OSDBGPRINT(a) printk a


/*
 * Darwin
 */
#elif defined(__DARWIN__)

/* debug printf */
# define OSDBGPRINT(a) printf a

#else
/** @todo other os'es */
# error "OS interface defines is not done for this OS!"
#endif


/* dprintf */
#if defined(DEBUG) && !defined(NO_LOGGING)
# ifdef LOG_TO_COM
#  include <VBox/log.h>
#  define dprintf(a) RTLogComPrintf a
# else
#  define dprintf(a) OSDBGPRINT(a)
# endif
#else
# define dprintf(a) do {} while (0)
#endif

/* dprintf2 - extended logging. */
#if defined(__DARWIN__)
# define dprintf2 dprintf
#else
# define dprintf2(a) do { } while (0)
#endif


/*
 * Error codes.
 */
/** Invalid parameter. */
#define SUPDRV_ERR_GENERAL_FAILURE  (-1)
/** Invalid parameter. */
#define SUPDRV_ERR_INVALID_PARAM    (-2)
/** Invalid magic or cookie. */
#define SUPDRV_ERR_INVALID_MAGIC    (-3)
/** Invalid loader handle. */
#define SUPDRV_ERR_INVALID_HANDLE   (-4)
/** Failed to lock the address range. */
#define SUPDRV_ERR_LOCK_FAILED      (-5)
/** Invalid memory pointer. */
#define SUPDRV_ERR_INVALID_POINTER  (-6)
/** Failed to patch the IDT. */
#define SUPDRV_ERR_IDT_FAILED       (-7)
/** Memory allocation failed. */
#define SUPDRV_ERR_NO_MEMORY        (-8)
/** Already loaded. */
#define SUPDRV_ERR_ALREADY_LOADED   (-9)
/** Permission denied. */
#define SUPDRV_ERR_PERMISSION_DENIED (-10)
/** Version mismatch. */
#define SUPDRV_ERR_VERSION_MISMATCH (-11)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to the device extension. */
typedef struct SUPDRVDEVEXT *PSUPDRVDEVEXT;

#ifndef VBOX_WITHOUT_IDT_PATCHING

/**
 * An IDT Entry.
 */
typedef struct SUPDRVIDTE
{
    /** Low offset word. */
    uint32_t    u16OffsetLow : 16;
    /** Segment Selector. */
    uint32_t    u16SegSel : 16;
#ifdef __AMD64__
    /** Interrupt Stack Table index. */
    uint32_t    u3IST : 3;
    /** Reserved, ignored. */
    uint32_t    u5Reserved : 5;
#else
    /** Reserved. */
    uint32_t    u5Reserved : 5;
    /** IDT Type part one (not used for task gate). */
    uint32_t    u3Type1 : 3;
#endif
    /** IDT Type part two. */
    uint32_t    u5Type2 : 5;
    /** Descriptor Privilege level. */
    uint32_t    u2DPL : 2;
    /** Present flag. */
    uint32_t    u1Present : 1;
    /** High offset word. */
    uint32_t    u16OffsetHigh : 16;
#ifdef __AMD64__
    /** The upper top part of the address. */
    uint32_t    u32OffsetTop;
    /** Reserved dword for qword (aligning the struct), ignored. */
    uint32_t    u32Reserved;
#endif
} SUPDRVIDTE, *PSUPDRVIDTE;

/** The u5Type2 value for an interrupt gate. */
#define SUPDRV_IDTE_TYPE2_INTERRUPT_GATE    0x0e


/**
 * Patch code.
 */
typedef struct SUPDRVPATCH
{
#define SUPDRV_PATCH_CODE_SIZE  0x50
    /** Patch code. */
    uint8_t                 auCode[SUPDRV_PATCH_CODE_SIZE];
    /** Changed IDT entry (for parnoid UnpatchIdt()). */
    SUPDRVIDTE              ChangedIdt;
    /** Saved IDT entry. */
    SUPDRVIDTE              SavedIdt;
    /** Pointer to the IDT.
     * We ASSUME the IDT is not re(al)located after bootup and use this as key
     * for the patches rather than processor number. This prevents some
     * stupid nesting stuff from happening in case of processors sharing the
     * IDT.
     * We're fucked if the processors have different physical mapping for
     * the(se) page(s), but we'll find that out soon enough in VBOX_STRICT mode.
     */
    void                   *pvIdt;
    /** Pointer to the IDT entry. */
    SUPDRVIDTE volatile    *pIdtEntry;
    /** Usage counter. */
    uint32_t volatile       cUsage;
    /** The offset into auCode of the VMMR0Entry fixup. */
    uint16_t                offVMMR0EntryFixup;
    /** The offset into auCode of the stub function. */
    uint16_t                offStub;
    /** Pointer to the next patch. */
    struct SUPDRVPATCH * volatile pNext;
} SUPDRVPATCH, *PSUPDRVPATCH;

/**
 * Usage record for a patch.
 */
typedef struct SUPDRVPATCHUSAGE
{
    /** Next in the chain. */
    struct SUPDRVPATCHUSAGE * volatile pNext;
    /** The patch this usage applies to. */
    PSUPDRVPATCH        pPatch;
    /** Usage count. */
    uint32_t volatile   cUsage;
} SUPDRVPATCHUSAGE, *PSUPDRVPATCHUSAGE;

#endif /* !VBOX_WITHOUT_IDT_PATCHING */


/**
 * Memory reference types.
 */
typedef enum
{
    /** Unused entry */
    MEMREF_TYPE_UNUSED = 0,
    /** Locked memory (r3 mapping only). */
    MEMREF_TYPE_LOCKED,
    /** Continous memory block (r3 and r0 mapping). */
    MEMREF_TYPE_CONT,
    /** Low memory block (r3 and r0 mapping). */
    MEMREF_TYPE_LOW,
    /** Memory block (r3 and r0 mapping). */
    MEMREF_TYPE_MEM,
    /** Blow the type up to 32-bit and mark the end. */
    MEMREG_TYPE_32BIT_HACK = 0x7fffffff
} SUPDRVMEMREFTYPE, *PSUPDRVMEMREFTYPE;


/**
 * Structure used for tracking memory a session
 * references in one way or another.
 */
typedef struct SUPDRVMEMREF
{
#ifdef USE_NEW_OS_INTERFACE
    /** The memory object handle. */
    RTR0MEMOBJ          MemObj;
    /** The ring-3 mapping memory object handle. */
    RTR0MEMOBJ          MapObjR3;
    /** Type of memory. */
    SUPDRVMEMREFTYPE    eType;

#else /* !USE_NEW_OS_INTERFACE */
    /** Pointer to the R0 mapping of the memory.
     * Set to NULL if N/A. */
    void               *pvR0;
    /** Pointer to the R3 mapping of the memory.
     * Set to NULL if N/A. */
    void               *pvR3;
    /** Size of the locked memory. */
    unsigned            cb;
    /** Type of memory. */
    SUPDRVMEMREFTYPE    eType;

    /** memory type specific information. */
    union
    {
        struct
        {
#if defined(__WIN__)
            /** Pointer to memory descriptor list (MDL). */
            PMDL               *papMdl;
            unsigned            cMdls;
#elif defined(__LINUX__)
            struct page       **papPages;
	    unsigned            cPages;
#else
# error "Either no target was defined or we haven't ported the driver to the target yet."
#endif
        } locked;
        struct
        {
#if defined(__WIN__)
            /** Pointer to memory descriptor list (MDL). */
            PMDL                pMdl;
#elif defined(__LINUX__)
            struct page        *paPages;
	    unsigned            cPages;
#else
# error "Either no target was defined or we haven't ported the driver to the target yet."
#endif
        } cont;
        struct
        {
#if defined(__WIN__)
            /** Pointer to memory descriptor list (MDL). */
            PMDL                pMdl;
#elif defined(__LINUX__)
            /** Pointer to the array of page pointers. */
            struct page       **papPages;
            /** Number of pages in papPages. */
	    unsigned            cPages;
#else
# error "Either no target was defined or we haven't ported the driver to the target yet."
#endif
        } mem;
    } u;
#endif /* !USE_NEW_OS_INTERFACE */
} SUPDRVMEMREF, *PSUPDRVMEMREF;


/**
 * Bundle of locked memory ranges.
 */
typedef struct SUPDRVBUNDLE
{
    /** Pointer to the next bundle. */
    struct SUPDRVBUNDLE * volatile pNext;
    /** Referenced memory. */
    SUPDRVMEMREF        aMem[64];
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
    void               *pvImage;
    /** Pointer to the optional module initialization callback. */
    PFNR0MODULEINIT     pfnModuleInit;
    /** Pointer to the optional module termination callback. */
    PFNR0MODULETERM     pfnModuleTerm;
    /** Size of the image. */
    uint32_t            cbImage;
    /** The offset of the symbol table. */
    uint32_t            offSymbols;
    /** The number of entries in the symbol table. */
    uint32_t            cSymbols;
    /** The offset of the string table. */
    uint32_t            offStrTab;
    /** Size of the string table. */
    uint32_t            cbStrTab;
    /** The ldr image state. (IOCtl code of last opration.) */
    uint32_t            uState;
    /** Usage count. */
    uint32_t volatile   cUsage;
    /** Image name. */
    char                szName[32];
} SUPDRVLDRIMAGE, *PSUPDRVLDRIMAGE;


/** Image usage record. */
typedef struct SUPDRVLDRUSAGE
{
    /** Next in chain. */
    struct SUPDRVLDRUSAGE * volatile pNext;
    /** The image. */
    PSUPDRVLDRIMAGE     pImage;
    /** Load count. */
    uint32_t volatile   cUsage;
} SUPDRVLDRUSAGE, *PSUPDRVLDRUSAGE;


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
    /** Pointer to the object destructor. */
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
    PSUPDRVDEVEXT               pDevExt;
    /** Session Cookie. */
    uint32_t                    u32Cookie;

    /** Load usage records. (protected by SUPDRVDEVEXT::mtxLdr) */
    PSUPDRVLDRUSAGE volatile    pLdrUsage;
#ifndef VBOX_WITHOUT_IDT_PATCHING
    /** Patch usage records. (protected by SUPDRVDEVEXT::SpinLock) */
    PSUPDRVPATCHUSAGE volatile  pPatchUsage;
#else
    /** The VM associated with the session. */
    PVM                         pVM;
#endif
    /** List of generic usage records. (protected by SUPDRVDEVEXT::SpinLock) */
    PSUPDRVUSAGE volatile       pUsage;

    /** Spinlock protecting the bundles and the GIP members. */
    RTSPINLOCK                  Spinlock;
#ifdef USE_NEW_OS_INTERFACE
    /** The ring-3 mapping of the GIP (readonly). */
    RTR0MEMOBJ                  GipMapObjR3;
#else
    /** The read-only usermode mapping address of the GID.
     * This is NULL if the GIP hasn't been mapped. */
    PCSUPGLOBALINFOPAGE         pGip;
#endif
    /** Set if the session is using the GIP. */
    uint32_t                    fGipReferenced;
    /** Bundle of locked memory objects. */
    SUPDRVBUNDLE                Bundle;

    /** The user id of the session. (Set by the OS part.) */
    RTUID                       Uid;
    /** The group id of the session. (Set by the OS part.) */
    RTGID                       Gid;
    /** The process (id) of the session. (Set by the OS part.) */
    RTPROCESS                   Process;
    /** Which process this session is associated with. */
    RTR0PROCESS                 R0Process;
#if defined(__DARWIN__) || defined(__OS2__)
    /** Pointer to the next session with the same has. */
    PSUPDRVSESSION              pNextHash;
#endif
} SUPDRVSESSION;


/**
 * Device extension.
 */
typedef struct SUPDRVDEVEXT
{
    /** Spinlock to serialize the initialization,
     * usage counting and destruction of the IDT entry override. */
    RTSPINLOCK              Spinlock;

#ifndef VBOX_WITHOUT_IDT_PATCHING
    /** List of patches. */
    PSUPDRVPATCH volatile   pIdtPatches;
    /** List of patches Free. */
    PSUPDRVPATCH volatile   pIdtPatchesFree;
#endif

    /** List of registered objects. */
    PSUPDRVOBJ volatile     pObjs;
    /** List of free object usage records. */
    PSUPDRVUSAGE volatile   pUsageFree;

    /** Global cookie. */
    uint32_t                u32Cookie;

    /** The IDT entry number.
     * Only valid if pIdtPatches is set. */
    uint8_t volatile        u8Idt;

    /** Loader mutex.
     * This protects pvVMMR0, pvVMMR0Entry, pImages and SUPDRVSESSION::pLdrUsage. */
    RTSEMFASTMUTEX          mtxLdr;

    /** VMM Module 'handle'.
     * 0 if the code VMM isn't loaded and Idt are nops. */
    void * volatile         pvVMMR0;
    /** VMMR0Entry() pointer. */
    DECLCALLBACKMEMBER(int, pfnVMMR0Entry)(PVM pVM, unsigned uOperation, void *pvArg);

    /** Linked list of loaded code. */
    PSUPDRVLDRIMAGE volatile pLdrImages;

    /** GIP mutex.
     * Any changes to any of the GIP members requires ownership of this mutex,
     * except on driver init and termination. */
    RTSEMFASTMUTEX          mtxGip;
    /** Pointer to the Global Info Page (GIP). */
    PSUPGLOBALINFOPAGE      pGip;
    /** The physical address of the GIP. */
    RTHCPHYS                HCPhysGip;
    /** Number of processes using the GIP.
     * (The updates are suspend while cGipUsers is 0.)*/
    uint32_t volatile       cGipUsers;
#ifdef USE_NEW_OS_INTERFACE
    /** The ring-0 memory object handle for the GIP page. */
    RTR0MEMOBJ              GipMemObj;
    /** The GIP timer handle. */
    PRTTIMER                pGipTimer;
    /** If non-zero we've successfully called RTTimerRequestSystemGranularity(). */
    uint32_t                u32SystemTimerGranularityGrant;
#endif
#ifdef __WIN__
    /** The GIP timer object. */
    KTIMER                  GipTimer;
    /** The GIP DPC object associated with GipTimer. */
    KDPC                    GipDpc;
    /** Pointer to the MDL for the pGip page. */
    PMDL                    pGipMdl;
    /** GIP timer interval (ms). */
    ULONG                   ulGipTimerInterval;
#endif
#ifdef __LINUX__
    /** The last jiffies. */
    unsigned long           ulLastJiffies;
    /** The last mono time stamp. */
    uint64_t                u64LastMonotime;
#endif
} SUPDRVDEVEXT;


__BEGIN_DECLS

/*******************************************************************************
*   OS Specific Functions                                                      *
*******************************************************************************/
void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession);
bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc);
#ifndef USE_NEW_OS_INTERFACE
int  VBOXCALL   supdrvOSLockMemOne(PSUPDRVMEMREF pMem, PSUPPAGE paPages);
void VBOXCALL   supdrvOSUnlockMemOne(PSUPDRVMEMREF pMem);
int  VBOXCALL   supdrvOSContAllocOne(PSUPDRVMEMREF pMem, void **ppvR0, void **ppvR3, PRTHCPHYS pHCPhys);
void VBOXCALL   supdrvOSContFreeOne(PSUPDRVMEMREF pMem);
int  VBOXCALL   supdrvOSLowAllocOne(PSUPDRVMEMREF pMem, void **ppvR3, PSUPPAGE paPages);
void VBOXCALL   supdrvOSLowFreeOne(PSUPDRVMEMREF pMem);
int  VBOXCALL   supdrvOSMemAllocOne(PSUPDRVMEMREF pMem, void **ppvR0, void **ppvR3);
void VBOXCALL   supdrvOSMemGetPages(PSUPDRVMEMREF pMem, PSUPPAGE paPages);
void VBOXCALL   supdrvOSMemFreeOne(PSUPDRVMEMREF pMem);
int  VBOXCALL   supdrvOSGipMap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE *ppGip);
int  VBOXCALL   supdrvOSGipUnmap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE pGip);
void  VBOXCALL  supdrvOSGipResume(PSUPDRVDEVEXT pDevExt);
void  VBOXCALL  supdrvOSGipSuspend(PSUPDRVDEVEXT pDevExt);
#endif


/*******************************************************************************
*   Shared Functions                                                           *
*******************************************************************************/
int  VBOXCALL   supdrvIOCtl(unsigned uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession,
                            void *pvIn, unsigned cbIn, void *pvOut, unsigned cbOut, unsigned *pcbReturned);
#ifdef VBOX_WITHOUT_IDT_PATCHING
int  VBOXCALL   supdrvIOCtlFast(unsigned uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
#endif
int  VBOXCALL   supdrvInitDevExt(PSUPDRVDEVEXT pDevExt);
int  VBOXCALL   supdrvDeleteDevExt(PSUPDRVDEVEXT pDevExt);
int  VBOXCALL   supdrvCreateSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION *ppSession);
void VBOXCALL   supdrvCloseSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
void VBOXCALL   supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
int  VBOXCALL   supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys, uint64_t u64NanoTS, unsigned uUpdateHz);
void VBOXCALL   supdrvGipTerm(PSUPGLOBALINFOPAGE pGip);
void VBOXCALL   supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS);

__END_DECLS

#endif

