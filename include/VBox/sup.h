/** @file
 * SUP - Support Library.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_sup_h
#define ___VBox_sup_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

__BEGIN_DECLS

/** @defgroup   grp_sup     The Support Library API
 * @{
 */

/**
 * Physical page descriptor.
 */
#pragma pack(4) /* space is more important. */
typedef struct SUPPAGE
{
    /** Physical memory address. */
    RTHCPHYS        Phys;
    /** Reserved entry for internal use by the caller. */
    RTHCUINTPTR     uReserved;
} SUPPAGE;
#pragma pack()
/** Pointer to a page descriptor. */
typedef SUPPAGE *PSUPPAGE;
/** Pointer to a const page descriptor. */
typedef const SUPPAGE *PCSUPPAGE;

/**
 * The paging mode.
 */
typedef enum SUPPAGINGMODE
{
    /** The usual invalid entry.
     * This is returned by SUPGetPagingMode()  */
    SUPPAGINGMODE_INVALID = 0,
    /** Normal 32-bit paging, no global pages */
    SUPPAGINGMODE_32_BIT,
    /** Normal 32-bit paging with global pages. */
    SUPPAGINGMODE_32_BIT_GLOBAL,
    /** PAE mode, no global pages, no NX. */
    SUPPAGINGMODE_PAE,
    /** PAE mode with global pages. */
    SUPPAGINGMODE_PAE_GLOBAL,
    /** PAE mode with NX, no global pages. */
    SUPPAGINGMODE_PAE_NX,
    /** PAE mode with global pages and NX. */
    SUPPAGINGMODE_PAE_GLOBAL_NX,
    /** AMD64 mode, no global pages. */
    SUPPAGINGMODE_AMD64,
    /** AMD64 mode with global pages, no NX. */
    SUPPAGINGMODE_AMD64_GLOBAL,
    /** AMD64 mode with NX, no global pages. */
    SUPPAGINGMODE_AMD64_NX,
    /** AMD64 mode with global pages and NX. */
    SUPPAGINGMODE_AMD64_GLOBAL_NX
} SUPPAGINGMODE;


#pragma pack(1) /* paranoia */

/**
 * Per CPU data.
 * This is only used when
 */
typedef struct SUPGIPCPU
{
    /** Update transaction number.
     * This number is incremented at the start and end of each update. It follows
     * thusly that odd numbers indicates update in progress, while even numbers
     * indicate stable data. Use this to make sure that the data items you fetch
     * are consistent. */
    volatile uint32_t   u32TransactionId;
    /** The interval in TSC ticks between two NanoTS updates.
     * This is the average interval over the last 2, 4 or 8 updates + a little slack.
     * The slack makes the time go a tiny tiny bit slower and extends the interval enough
     * to avoid ending up with too many 1ns increments. */
    volatile uint32_t   u32UpdateIntervalTSC;
    /** Current nanosecond timestamp. */
    volatile uint64_t   u64NanoTS;
    /** The TSC at the time of u64NanoTS. */
    volatile uint64_t   u64TSC;
    /** Current CPU Frequency. */
    volatile uint64_t   u64CpuHz;
    /** Number of errors during updating.
     * Typical errors are under/overflows. */
    volatile uint32_t   cErrors;
    /** Index of the head item in au32TSCHistory. */
    volatile uint32_t   iTSCHistoryHead;
    /** Array of recent TSC interval deltas.
     * The most recent item is at index iTSCHistoryHead.
     * This history is used to calculate u32UpdateIntervalTSC.
     */
    volatile uint32_t   au32TSCHistory[8];
    /** Reserved for future per processor data. */
    volatile uint32_t   au32Reserved[6];
} SUPGIPCPU;
AssertCompileSize(SUPGIPCPU, 96);
/*AssertCompileMemberAlignment(SUPGIPCPU, u64TSC, 8); -fixme */

/** Pointer to per cpu data.
 * @remark there is no const version of this typedef, see g_pSUPGlobalInfoPage for details. */
typedef SUPGIPCPU *PSUPGIPCPU;

/**
 * Global Information Page.
 *
 * This page contains useful information and can be mapped into any
 * process or VM. It can be accessed thru the g_pSUPGlobalInfoPage
 * pointer when a session is open.
 */
typedef struct SUPGLOBALINFOPAGE
{
    /** Magic (SUPGLOBALINFOPAGE_MAGIC). */
    uint32_t            u32Magic;
    /** The GIP version. */
    uint32_t            u32Version;

    /** The GIP update mode, see SUPGIPMODE. */
    uint32_t            u32Mode;
    /** Reserved / padding. */
    uint32_t            u32Padding0;
    /** The update frequency of the of the NanoTS. */
    volatile uint32_t   u32UpdateHz;
    /** The update interval in nanoseconds. (10^9 / u32UpdateHz) */
    volatile uint32_t   u32UpdateIntervalNS;
    /** The timestamp of the last time we update the update frequency. */
    volatile uint64_t   u64NanoTSLastUpdateHz;

    /** Padding / reserved space for future data. */
    uint32_t            au32Padding1[56];

    /** Array of per-cpu data.
     * If u32Mode == SUPGIPMODE_SYNC_TSC then only the first entry is used.
     * If u32Mode == SUPGIPMODE_ASYNC_TSC then the CPU ACPI ID is used as an
     * index into the array. */
    SUPGIPCPU           aCPUs[32];
} SUPGLOBALINFOPAGE;
AssertCompile(sizeof(SUPGLOBALINFOPAGE) <= 0x1000);
/* AssertCompileMemberAlignment(SUPGLOBALINFOPAGE, aCPU, 32); - fixme */

/** Pointer to the global info page.
 * @remark there is no const version of this typedef, see g_pSUPGlobalInfoPage for details. */
typedef SUPGLOBALINFOPAGE *PSUPGLOBALINFOPAGE;

#pragma pack() /* end of paranoia */

/** The value of the SUPGLOBALINFOPAGE::u32Magic field. (Soryo Fuyumi) */
#define SUPGLOBALINFOPAGE_MAGIC     0x19590106
/** The GIP version.
 * Upper 16 bits is the major version. Major version is only changed with
 * incompatible changes in the GIP. */
#define SUPGLOBALINFOPAGE_VERSION   0x00020000

/**
 * SUPGLOBALINFOPAGE::u32Mode values.
 */
typedef enum SUPGIPMODE
{
    /** The usual invalid null entry. */
    SUPGIPMODE_INVALID = 0,
    /** The TSC of the cores and cpus in the system is in sync. */
    SUPGIPMODE_SYNC_TSC,
    /** Each core has it's own TSC. */
    SUPGIPMODE_ASYNC_TSC,
    /** The usual 32-bit hack. */
    SUPGIPMODE_32BIT_HACK = 0x7fffffff
} SUPGIPMODE;

/** Pointer to the Global Information Page.
 *
 * This pointer is valid as long as SUPLib has a open session. Anyone using
 * the page must treat this pointer as higly volatile and not trust it beyond
 * one transaction.
 *
 * @remark  The GIP page is read-only to everyone but the support driver and
 *          is actually mapped read only everywhere but in ring-0. However
 *          it is not marked 'const' as this might confuse compilers into
 *          thinking that values doesn't change even if members are marked
 *          as volatile. Thus, there is no PCSUPGLOBALINFOPAGE type.
 */
#if defined(IN_SUP_R0) || defined(IN_SUP_R3) || defined(IN_SUP_GC)
extern DECLEXPORT(PSUPGLOBALINFOPAGE)   g_pSUPGlobalInfoPage;
#elif defined(IN_RING0)
extern DECLIMPORT(SUPGLOBALINFOPAGE)    g_SUPGlobalInfoPage;
# if defined(__GNUC__) && !defined(RT_OS_DARWIN) && defined(RT_ARCH_AMD64)
/** Workaround for ELF+GCC problem on 64-bit hosts.
 * (GCC emits a mov with a R_X86_64_32 reloc, we need R_X86_64_64.) */
DECLINLINE(PSUPGLOBALINFOPAGE) SUPGetGIP(void)
{
    PSUPGLOBALINFOPAGE pGIP;
    __asm__ __volatile__ ("movabs $g_SUPGlobalInfoPage,%0\n\t"
                          : "=a" (pGIP));
    return pGIP;
}
#  define g_pSUPGlobalInfoPage          (SUPGetGIP())
# else
#  define g_pSUPGlobalInfoPage          (&g_SUPGlobalInfoPage)
# endif
#else
extern DECLIMPORT(PSUPGLOBALINFOPAGE)   g_pSUPGlobalInfoPage;
#endif


/**
 * Gets the TSC frequency of the calling CPU.
 *
 * @returns TSC frequency.
 * @param   pGip        The GIP pointer.
 */
DECLINLINE(uint64_t) SUPGetCpuHzFromGIP(PSUPGLOBALINFOPAGE pGip)
{
    unsigned iCpu;

    if (RT_UNLIKELY(!pGip || pGip->u32Magic != SUPGLOBALINFOPAGE_MAGIC))
        return ~(uint64_t)0;

    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        iCpu = 0;
    else
    {
        iCpu = ASMGetApicId();
        if (RT_UNLIKELY(iCpu >= RT_ELEMENTS(pGip->aCPUs)))
            return ~(uint64_t)0;
    }

    return pGip->aCPUs[iCpu].u64CpuHz;
}


/**
 * Request for generic VMMR0Entry calls.
 */
typedef struct SUPVMMR0REQHDR
{
    /** The magic. (SUPVMMR0REQHDR_MAGIC) */
    uint32_t    u32Magic;
    /** The size of the request. */
    uint32_t    cbReq;
} SUPVMMR0REQHDR;
/** Pointer to a ring-0 request header. */
typedef SUPVMMR0REQHDR *PSUPVMMR0REQHDR;
/** the SUPVMMR0REQHDR::u32Magic value (Ethan Iverson - The Bad Plus). */
#define SUPVMMR0REQHDR_MAGIC    UINT32_C(0x19730211)


/** For the fast ioctl path.
 * @{
 */
/** @see VMMR0_DO_RAW_RUN. */
#define SUP_VMMR0_DO_RAW_RUN    0
/** @see VMMR0_DO_HWACC_RUN. */
#define SUP_VMMR0_DO_HWACC_RUN  1
/** @see VMMR0_DO_NOP */
#define SUP_VMMR0_DO_NOP        2
/** @} */



#ifdef IN_RING3

/** @defgroup   grp_sup_r3     SUP Host Context Ring 3 API
 * @ingroup grp_sup
 * @{
 */

/**
 * Installs the support library.
 *
 * @returns VBox status code.
 */
SUPR3DECL(int) SUPInstall(void);

/**
 * Uninstalls the support library.
 *
 * @returns VBox status code.
 */
SUPR3DECL(int) SUPUninstall(void);

/**
 * Initializes the support library.
 * Each succesful call to SUPInit() must be countered by a
 * call to SUPTerm(false).
 *
 * @returns VBox status code.
 * @param   ppSession       Where to store the session handle. Defaults to NULL.
 * @param   cbReserve       The number of bytes of contiguous memory that should be reserved by
 *                          the runtime / support library.
 *                          Set this to 0 if no reservation is required. (default)
 *                          Set this to ~0 if the maximum amount supported by the VM is to be
 *                          attempted reserved, or the maximum available.
 */
#ifdef __cplusplus
SUPR3DECL(int) SUPInit(PSUPDRVSESSION *ppSession = NULL, size_t cbReserve = 0);
#else
SUPR3DECL(int) SUPInit(PSUPDRVSESSION *ppSession, size_t cbReserve);
#endif

/**
 * Terminates the support library.
 *
 * @returns VBox status code.
 * @param   fForced     Forced termination. This means to ignore the
 *                      init call count and just terminated.
 */
#ifdef __cplusplus
SUPR3DECL(int) SUPTerm(bool fForced = false);
#else
SUPR3DECL(int) SUPTerm(int fForced);
#endif

/**
 * Sets the ring-0 VM handle for use with fast IOCtls.
 *
 * @returns VBox status code.
 * @param   pVMR0       The ring-0 VM handle.
 *                      NIL_RTR0PTR can be used to unset the handle when the
 *                      VM is about to be destroyed.
 */
SUPR3DECL(int) SUPSetVMForFastIOCtl(PVMR0 pVMR0);

/**
 * Calls the HC R0 VMM entry point.
 * See VMMR0Entry() for more details.
 *
 * @returns error code specific to uFunction.
 * @param   pVMR0       Pointer to the Ring-0 (Host Context) mapping of the VM structure.
 * @param   uOperation  Operation to execute.
 * @param   pvArg       Argument.
 */
SUPR3DECL(int) SUPCallVMMR0(PVMR0 pVMR0, unsigned uOperation, void *pvArg);

/**
 * Variant of SUPCallVMMR0, except that this takes the fast ioclt path
 * regardsless of compile-time defaults.
 *
 * @returns VBox status code.
 * @param   pVMR0       The ring-0 VM handle.
 * @param   uOperation  The operation; only the SUP_VMMR0_DO_* ones are valid.
 */
SUPR3DECL(int) SUPCallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation);

/**
 * Calls the HC R0 VMM entry point, in a safer but slower manner than SUPCallVMMR0.
 * When entering using this call the R0 components can call into the host kernel
 * (i.e. use the SUPR0 and RT APIs).
 *
 * See VMMR0Entry() for more details.
 *
 * @returns error code specific to uFunction.
 * @param   pVMR0       Pointer to the Ring-0 (Host Context) mapping of the VM structure.
 * @param   uOperation  Operation to execute.
 * @param   u64Arg      Constant argument.
 * @param   pReqHdr     Pointer to a request header. Optional.
 *                      This will be copied in and out of kernel space. There currently is a size
 *                      limit on this, just below 4KB.
 */
SUPR3DECL(int) SUPCallVMMR0Ex(PVMR0 pVMR0, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr);

/**
 * Queries the paging mode of the host OS.
 *
 * @returns The paging mode.
 */
SUPR3DECL(SUPPAGINGMODE) SUPGetPagingMode(void);

/**
 * Allocate zero-filled pages.
 *
 * Use this to allocate a number of pages rather than using RTMem*() and mess with
 * alignment. The returned address is of course page aligned. Call SUPPageFree()
 * to free the pages once done with them.
 *
 * @returns VBox status.
 * @param   cPages          Number of pages to allocate.
 * @param   ppvPages        Where to store the base pointer to the allocated pages.
 */
SUPR3DECL(int) SUPPageAlloc(size_t cPages, void **ppvPages);

/**
 * Frees pages allocated with SUPPageAlloc().
 *
 * @returns VBox status.
 * @param   pvPages         Pointer returned by SUPPageAlloc().
 * @param   cPages          Number of pages that was allocated.
 */
SUPR3DECL(int) SUPPageFree(void *pvPages, size_t cPages);

/**
 * Allocate zero-filled locked pages.
 *
 * Use this to allocate a number of pages rather than using RTMem*() and mess with
 * alignment. The returned address is of course page aligned. Call SUPPageFreeLocked()
 * to free the pages once done with them.
 *
 * @returns VBox status.
 * @param   cPages          Number of pages to allocate.
 * @param   ppvPages        Where to store the base pointer to the allocated pages.
 */
SUPR3DECL(int) SUPPageAllocLocked(size_t cPages, void **ppvPages);

/**
 * Allocate zero-filled locked pages.
 *
 * Use this to allocate a number of pages rather than using RTMem*() and mess with
 * alignment. The returned address is of course page aligned. Call SUPPageFreeLocked()
 * to free the pages once done with them.
 *
 * @returns VBox status code.
 * @param   cPages          Number of pages to allocate.
 * @param   ppvPages        Where to store the base pointer to the allocated pages.
 * @param   paPages         Where to store the physical page addresses returned.
 *                          On entry this will point to an array of with cbMemory >> PAGE_SHIFT entries.
 *                          NULL is allowed.
 */
SUPR3DECL(int) SUPPageAllocLockedEx(size_t cPages, void **ppvPages, PSUPPAGE paPages);

/**
 * Frees locked pages allocated with SUPPageAllocLocked().
 *
 * @returns VBox status.
 * @param   pvPages         Pointer returned by SUPPageAlloc().
 * @param   cPages          Number of pages that was allocated.
 */
SUPR3DECL(int) SUPPageFreeLocked(void *pvPages, size_t cPages);

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
SUPR3DECL(int) SUPPageLock(void *pvStart, size_t cPages, PSUPPAGE paPages);

/**
 * Releases locked down pages.
 *
 * @returns VBox status code.
 * @param   pvStart         Start of virtual memory range previously locked
 *                          down by SUPPageLock().
 */
SUPR3DECL(int) SUPPageUnlock(void *pvStart);

/**
 * Allocated memory with page aligned memory with a contiguous and locked physical
 * memory backing below 4GB.
 *
 * @returns Pointer to the allocated memory (virtual address).
 *          *pHCPhys is set to the physical address of the memory.
 *          The returned memory must be freed using SUPContFree().
 * @returns NULL on failure.
 * @param   cPages      Number of pages to allocate.
 * @param   pHCPhys     Where to store the physical address of the memory block.
 */
SUPR3DECL(void *) SUPContAlloc(size_t cPages, PRTHCPHYS pHCPhys);

/**
 * Allocated memory with page aligned memory with a contiguous and locked physical
 * memory backing below 4GB.
 *
 * @returns Pointer to the allocated memory (virtual address).
 *          *pHCPhys is set to the physical address of the memory.
 *          If ppvR0 isn't NULL, *ppvR0 is set to the ring-0 mapping.
 *          The returned memory must be freed using SUPContFree().
 * @returns NULL on failure.
 * @param   cPages      Number of pages to allocate.
 * @param   pR0Ptr      Where to store the ring-0 mapping of the allocation. (optional)
 * @param   pHCPhys     Where to store the physical address of the memory block.
 *
 * @remark  This 2nd version of this API exists because we're not able to map the
 *          ring-3 mapping executable on WIN64. This is a serious problem in regard to
 *          the world switchers.
 */
SUPR3DECL(void *) SUPContAlloc2(size_t cPages, PRTR0PTR pR0Ptr, PRTHCPHYS pHCPhys);

/**
 * Frees memory allocated with SUPContAlloc().
 *
 * @returns VBox status code.
 * @param   pv          Pointer to the memory block which should be freed.
 * @param   cPages      Number of pages to be freed.
 */
SUPR3DECL(int) SUPContFree(void *pv, size_t cPages);

/**
 * Allocated non contiguous physical memory below 4GB.
 *
 * The memory isn't zeroed.
 *
 * @returns VBox status code.
 * @returns NULL on failure.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvPages    Where to store the pointer to the allocated memory.
 *                      The pointer stored here on success must be passed to SUPLowFree when
 *                      the memory should be released.
 * @param   ppvPagesR0  Where to store the ring-0 pointer to the allocated memory. optional.
 * @param   paPages     Where to store the physical addresses of the individual pages.
 */
SUPR3DECL(int) SUPLowAlloc(size_t cPages, void **ppvPages, PRTR0PTR ppvPagesR0, PSUPPAGE paPages);

/**
 * Frees memory allocated with SUPLowAlloc().
 *
 * @returns VBox status code.
 * @param   pv          Pointer to the memory block which should be freed.
 * @param   cPages      Number of pages that was allocated.
 */
SUPR3DECL(int) SUPLowFree(void *pv, size_t cPages);

/**
 * Load a module into R0 HC.
 *
 * @returns VBox status code.
 * @param   pszFilename     The path to the image file.
 * @param   pszModule       The module name. Max 32 bytes.
 */
SUPR3DECL(int) SUPLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase);

/**
 * Frees a R0 HC module.
 *
 * @returns VBox status code.
 * @param   pszModule       The module to free.
 * @remark  This will not actually 'free' the module, there are of course usage counting.
 */
SUPR3DECL(int) SUPFreeModule(void *pvImageBase);

/**
 * Get the address of a symbol in a ring-0 module.
 *
 * @returns VBox status code.
 * @param   pszModule       The module name.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
SUPR3DECL(int) SUPGetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue);

/**
 * Load R0 HC VMM code.
 *
 * @returns VBox status code.
 * @deprecated  Use SUPLoadModule(pszFilename, "VMMR0.r0", &pvImageBase)
 */
SUPR3DECL(int) SUPLoadVMM(const char *pszFilename);

/**
 * Unloads R0 HC VMM code.
 *
 * @returns VBox status code.
 * @deprecated  Use SUPFreeModule().
 */
SUPR3DECL(int) SUPUnloadVMM(void);

/**
 * Get the physical address of the GIP.
 *
 * @returns VBox status code.
 * @param   pHCPhys     Where to store the physical address of the GIP.
 */
SUPR3DECL(int) SUPGipGetPhys(PRTHCPHYS pHCPhys);

/** @} */
#endif /* IN_RING3 */


#ifdef IN_RING0
/** @defgroup   grp_sup_r0     SUP Host Context Ring 0 API
 * @ingroup grp_sup
 * @{
 */

/**
 * Execute callback on all cpus/cores (SUPR0ExecuteCallback)
 */
#define SUPDRVEXECCALLBACK_CPU_ALL      (~0)

/**
 * Security objectype.
 */
typedef enum SUPDRVOBJTYPE
{
    /** The usual invalid object. */
    SUPDRVOBJTYPE_INVALID = 0,
    /** A Virtual Machine instance. */
    SUPDRVOBJTYPE_VM,
    /** Internal network. */
    SUPDRVOBJTYPE_INTERNAL_NETWORK,
    /** Internal network interface. */
    SUPDRVOBJTYPE_INTERNAL_NETWORK_INTERFACE,
    /** The first invalid object type in this end. */
    SUPDRVOBJTYPE_END,
    /** The usual 32-bit type size hack. */
    SUPDRVOBJTYPE_32_BIT_HACK = 0x7ffffff
} SUPDRVOBJTYPE;

/**
 * Object destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     The first user argument.
 * @param   pvUser2     The second user argument.
 */
typedef DECLCALLBACK(void) FNSUPDRVDESTRUCTOR(void *pvObj, void *pvUser1, void *pvUser2);
/** Pointer to a FNSUPDRVDESTRUCTOR(). */
typedef FNSUPDRVDESTRUCTOR *PFNSUPDRVDESTRUCTOR;

SUPR0DECL(void *) SUPR0ObjRegister(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2);
SUPR0DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession);
SUPR0DECL(int) SUPR0ObjRelease(void *pvObj, PSUPDRVSESSION pSession);
SUPR0DECL(int) SUPR0ObjVerifyAccess(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName);

SUPR0DECL(int) SUPR0LockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0UnlockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3);
SUPR0DECL(int) SUPR0ContAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys);
SUPR0DECL(int) SUPR0ContFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
SUPR0DECL(int) SUPR0LowAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0LowFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
SUPR0DECL(int) SUPR0MemAlloc(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3);
SUPR0DECL(int) SUPR0MemGetPhys(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, PSUPPAGE paPages);
SUPR0DECL(int) SUPR0MemFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
SUPR0DECL(int) SUPR0PageAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR3PTR ppvR3, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0PageFree(PSUPDRVSESSION pSession, RTR3PTR pvR3);
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGip);
SUPR0DECL(int) SUPR0GipUnmap(PSUPDRVSESSION pSession);
SUPR0DECL(int) SUPR0Printf(const char *pszFormat, ...);

/**
 * Support driver component factory.
 *
 * Component factories are registered by drivers that provides services
 * such as the host network interface filtering and access to the host
 * TCP/IP stack.
 *
 * @remark Module dependencies and making sure that a component doesn't
 *         get unloaded while in use, is the sole responsibility of the
 *         driver/kext/whatever implementing the component.
 */
typedef struct SUPDRVFACTORY
{
    /** The (unique) name of the component factory. */
    char szName[56];
    /**
     * Queries a factory interface.
     *
     * The factory interface is specific to each component and will be be
     * found in the header(s) for the component alongside its UUID.
     *
     * @returns Pointer to the factory interfaces on success, NULL on failure.
     *
     * @param   pSession            The SUPDRV session making the query.
     * @param   pSupDrvFactory      Pointer to this structure.
     * @param   pInterfaceUuid      The UUID of the factory interface.
     */
    DECLR0CALLBACKMEMBER(void *, pfnQueryFactoryInterface,(PSUPDRVSESSION pSession, struct SUPDRVFACTORY *pSupDrvFactory, PCRTUUID pInterfaceUuid));
} SUPDRVFACTORY;
/** Pointer to a support driver factory. */
typedef SUPDRVFACTORY *PSUPDRVFACTORY;

/**
 * Register a component factory with the support driver.
 *
 * @returns VBox status code.
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure.
 *
 * @remarks This interface is also available thru the IDC interfaces.
 */
SUPR0DECL(int) SUPR0ComponentRegisterFactory(PSUPDRVSESSION pSession, PSUPDRVFACTORY pFactory);

/**
 * Deregister a component factory.
 *
 * @returns VBox status code.
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure
 *                          previously passed SUPR0ComponentRegisterFactory().
 *
 * @remarks This interface is also available thru the IDC interfaces.
 */
SUPR0DECL(int) SUPR0ComponentDeregisterFactory(PSUPDRVSESSION pSession, PSUPDRVFACTORY pFactory);

/**
 * Queries a component factory.
 *
 * @returns VBox status code.
 * @retval  VBOX_SUPDRV_COMPONENT_NOT_FOUND if the component factory wasn't found.
 * @retval  VBOX_SUPDRV_INTERFACE_NOT_SUPPORTED if the interface wasn't supported.
 *
 * @param   pSession        The SUPDRV session.
 * @param   pszName         The name of the component factory.
 * @param   pInterfaceUuid  The UUID of the factory interface.
 * @param   ppvFactoryIf    Where to store the factory interface.
 */
SUPR0DECL(int) SUPR0ComponentQueryFactory(PSUPDRVSESSION pSession, const char *pszName, PCRTUUID pInterfaceUuid, void **ppvFactoryIf);


/** @defgroup   grp_sup_r0_idc  The IDC Interface
 * @ingroup grp_sup_r0
 * @{
 */

/** The current SUPDRV IDC version.
 * This follows the usual high word / low word rules, i.e. high word is the
 * major number and it signifies incompatible interface changes. */
#define SUPDRV_IDC_VERSION                  0x00010000

/**
 * The SUPDRV IDC entry point.
 *
 * (IDC = Inter-Driver Communication)
 *
 * @returns VBox status code.
 *
 * @param   pSession    The session. (This is NULL for SUPDRV_IDC_REQ_CONNECT.)
 * @param   iReq        The request number.
 * @param   pvReq       Pointer to the request packet. Optional for some requests.
 * @param   cbReq       The size of the request packet.
 */
typedef DECLCALLBACK(int) FNSUPDRVIDCENTRY(PSUPDRVSESSION pSession, uint32_t iReq, void *pvReq, uint32_t cbReq);
/** Pointer to the SUPDRV IDC entry point. */
typedef FNSUPDRVIDCENTRY *PFNSUPDRVIDCENTRY;


/**
 * SUPDRV IDC: Connect request.
 * This request takes a SUPDRVIDCREQCONNECT packet.
 */
#define SUPDRV_IDC_REQ_CONNECT                          UINT32_C(0xc0ffee01)
/** A SUPDRV IDC connect request packet. */
typedef union SUPDRVIDCREQCONNECT
{
    /** The input. */
    struct SUPDRVIDCREQCONNECTIN
    {
        /** The magic cookie (SUPDRVIDCREQ_CONNECT_MAGIC). */
        uint32_t        u32MagicCookie;
        /** The desired version of the IDC interface. */
        uint32_t        iReqVersion;
        /** The minimum version of the IDC interface. */
        uint32_t        iMinVersion;
    } In;

    /** The output. */
    struct SUPDRVIDCREQCONNECTOUT
    {
        /** The support driver session. (An opaque.) */
        PSUPDRVSESSION  pSession;
        /** The version of the IDC interface for this session. */
        uint32_t        iVersion;
    } Out;
} SUPDRVIDCREQCONNECT;
/** Pointer to a SUPDRV IDC connect request. */
typedef SUPDRVIDCREQCONNECT *PSUPDRVIDCREQCONNECT;
/** Magic cookie value (SUPDRVIDCREQCONNECT::In.u32MagicCookie). ('tori') */
#define SUPDRVIDCREQ_CONNECT_MAGIC_COOKIE               UINT32_C(0x69726f74)


/**
 * SUPDRV IDC: Disconnect request.
 * This request does not take request packet.
 */
#define SUPDRV_IDC_REQ_DISCONNECT                       UINT32_C(0xc0ffee02)


/**
 * SUPDRV IDC: Disconnect request.
 * This request takes a SUPDRVFACTORY packet.
 */
#define SUPDRV_IDC_REQ_COMPONENT_REGISTER_FACTORY       UINT32_C(0xc0ffee03)


/**
 * SUPDRV IDC: Dergister a component factory.
 * This request takes a SUPDRVFACTORY packet.
 */
#define SUPDRV_IDC_REQ_COMPONENT_DEREGISTER_FACTORY     UINT32_C(0xc0ffee04)

/** @} */

/** @} */
#endif

/** @} */

__END_DECLS

#endif

