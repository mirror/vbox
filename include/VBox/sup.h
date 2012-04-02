/** @file
 * SUP - Support Library. (HDrv)
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

#ifndef ___VBox_sup_h
#define ___VBox_sup_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <iprt/stdarg.h>
#include <iprt/cpuset.h>

RT_C_DECLS_BEGIN

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
 *
 * @remarks Users are making assumptions about the order here!
 */
typedef enum SUPPAGINGMODE
{
    /** The usual invalid entry.
     * This is returned by SUPR3GetPagingMode()  */
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


/**
 * The CPU state.
 */
typedef enum SUPGIPCPUSTATE
{
    /** Invalid CPU state / unused CPU entry. */
    SUPGIPCPUSTATE_INVALID = 0,
    /** The CPU is not present. */
    SUPGIPCPUSTATE_ABSENT,
    /** The CPU is offline. */
    SUPGIPCPUSTATE_OFFLINE,
    /** The CPU is online. */
    SUPGIPCPUSTATE_ONLINE,
    /** Force 32-bit enum type. */
    SUPGIPCPUSTATE_32_BIT_HACK = 0x7fffffff
} SUPGIPCPUSTATE;

/**
 * Per CPU data.
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
    /** The interval between the last two NanoTS updates. (experiment for now) */
    volatile uint32_t   u32PrevUpdateIntervalNS;

    /** Reserved for future per processor data. */
    volatile uint32_t   au32Reserved[5+5];

    /** @todo Add topology/NUMA info. */
    /** The CPU state. */
    SUPGIPCPUSTATE volatile enmState;
    /** The host CPU ID of this CPU (the SUPGIPCPU is indexed by APIC ID). */
    RTCPUID                 idCpu;
    /** The CPU set index of this CPU. */
    int16_t                 iCpuSet;
    /** The APIC ID of this CPU. */
    uint16_t                idApic;
} SUPGIPCPU;
AssertCompileSize(RTCPUID, 4);
AssertCompileSize(SUPGIPCPU, 128);
AssertCompileMemberAlignment(SUPGIPCPU, u64NanoTS, 8);
AssertCompileMemberAlignment(SUPGIPCPU, u64TSC, 8);

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
    /** The number of entries in the CPU table.
     * (This can work as RTMpGetArraySize().)  */
    uint16_t            cCpus;
    /** The size of the GIP in pages. */
    uint16_t            cPages;
    /** The update frequency of the of the NanoTS. */
    volatile uint32_t   u32UpdateHz;
    /** The update interval in nanoseconds. (10^9 / u32UpdateHz) */
    volatile uint32_t   u32UpdateIntervalNS;
    /** The timestamp of the last time we update the update frequency. */
    volatile uint64_t   u64NanoTSLastUpdateHz;
    /** The set of online CPUs. */
    RTCPUSET            OnlineCpuSet;
    /** The set of present CPUs. */
    RTCPUSET            PresentCpuSet;
    /** The set of possible CPUs. */
    RTCPUSET            PossibleCpuSet;
    /** The number of CPUs that are online. */
    volatile uint16_t   cOnlineCpus;
    /** The number of CPUs present in the system. */
    volatile uint16_t   cPresentCpus;
    /** The highest number of CPUs possible. */
    uint16_t            cPossibleCpus;
    /** The highest number of CPUs possible. */
    uint16_t            u16Padding0;
    /** The max CPU ID (RTMpGetMaxCpuId). */
    RTCPUID             idCpuMax;
    /** Padding to 8 byte alignment. */
    uint16_t            au16Padding1[2];
    /** Spinlock protecting the GIP array members. */
    RTSPINLOCK          Spinlock;                       /* 144 */

    /** Padding / reserved space for future data. */
    uint32_t            au32Padding1[26];

    /** Table indexed by the CPU APIC ID to get the CPU table index. */
    uint16_t            aiCpuFromApicId[256];
    /** CPU set index to CPU table index. */
    uint16_t            aiCpuFromCpuSetIdx[RTCPUSET_MAX_CPUS];

    /** Array of per-cpu data.
     * This is index by ApicId via the aiCpuFromApicId table.
     *
     * The clock and frequency information is updated for all CPUs if u32Mode
     * is SUPGIPMODE_ASYNC_TSC, otherwise (SUPGIPMODE_SYNC_TSC) only the first
     * entry is updated. */
    SUPGIPCPU           aCPUs[1];
} SUPGLOBALINFOPAGE;
AssertCompileMemberAlignment(SUPGLOBALINFOPAGE, u64NanoTSLastUpdateHz, 8);
AssertCompileMemberAlignment(SUPGLOBALINFOPAGE, aCPUs, 256);

/** Pointer to the global info page.
 * @remark there is no const version of this typedef, see g_pSUPGlobalInfoPage for details. */
typedef SUPGLOBALINFOPAGE *PSUPGLOBALINFOPAGE;


/** The value of the SUPGLOBALINFOPAGE::u32Magic field. (Soryo Fuyumi) */
#define SUPGLOBALINFOPAGE_MAGIC     0x19590106
/** The GIP version.
 * Upper 16 bits is the major version. Major version is only changed with
 * incompatible changes in the GIP. */
#define SUPGLOBALINFOPAGE_VERSION   0x00030000

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
 * the page must treat this pointer as highly volatile and not trust it beyond
 * one transaction.
 *
 * @remark  The GIP page is read-only to everyone but the support driver and
 *          is actually mapped read only everywhere but in ring-0. However
 *          it is not marked 'const' as this might confuse compilers into
 *          thinking that values doesn't change even if members are marked
 *          as volatile. Thus, there is no PCSUPGLOBALINFOPAGE type.
 */
#if defined(IN_SUP_R0) || defined(IN_SUP_R3) || defined(IN_SUP_RC)
extern DECLEXPORT(PSUPGLOBALINFOPAGE)   g_pSUPGlobalInfoPage;

#elif !defined(IN_RING0) || defined(RT_OS_WINDOWS) || defined(RT_OS_SOLARIS)
extern DECLIMPORT(PSUPGLOBALINFOPAGE)   g_pSUPGlobalInfoPage;

#else /* IN_RING0 && !RT_OS_WINDOWS */
# if !defined(__GNUC__) || defined(RT_OS_DARWIN) || !defined(RT_ARCH_AMD64)
#  define g_pSUPGlobalInfoPage          (&g_SUPGlobalInfoPage)
# else
#  define g_pSUPGlobalInfoPage          (SUPGetGIPHlp())
/** Workaround for ELF+GCC problem on 64-bit hosts.
 * (GCC emits a mov with a R_X86_64_32 reloc, we need R_X86_64_64.) */
DECLINLINE(PSUPGLOBALINFOPAGE) SUPGetGIPHlp(void)
{
    PSUPGLOBALINFOPAGE pGIP;
    __asm__ __volatile__ ("movabs $g_SUPGlobalInfoPage,%0\n\t"
                          : "=a" (pGIP));
    return pGIP;
}
# endif
/** The GIP.
 * We save a level of indirection by exporting the GIP instead of a variable
 * pointing to it. */
extern DECLIMPORT(SUPGLOBALINFOPAGE)    g_SUPGlobalInfoPage;
#endif

/**
 * Gets the GIP pointer.
 *
 * @returns Pointer to the GIP or NULL.
 */
SUPDECL(PSUPGLOBALINFOPAGE)             SUPGetGIP(void);

#ifdef ___iprt_asm_amd64_x86_h
/**
 * Gets the TSC frequency of the calling CPU.
 *
 * @returns TSC frequency, UINT64_MAX on failure.
 * @param   pGip        The GIP pointer.
 */
DECLINLINE(uint64_t) SUPGetCpuHzFromGIP(PSUPGLOBALINFOPAGE pGip)
{
    unsigned iCpu;

    if (RT_UNLIKELY(!pGip || pGip->u32Magic != SUPGLOBALINFOPAGE_MAGIC))
        return UINT64_MAX;

    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        iCpu = 0;
    else
    {
        iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];
        if (iCpu >= pGip->cCpus)
            return UINT64_MAX;
    }

    return pGip->aCPUs[iCpu].u64CpuHz;
}
#endif

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
#define SUPVMMR0REQHDR_MAGIC        UINT32_C(0x19730211)


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

/** SUPR3QueryVTCaps capability flags
 * @{
 */
#define SUPVTCAPS_AMD_V             RT_BIT(0)
#define SUPVTCAPS_VT_X              RT_BIT(1)
#define SUPVTCAPS_NESTED_PAGING     RT_BIT(2)
/** @} */

/**
 * Request for generic FNSUPR0SERVICEREQHANDLER calls.
 */
typedef struct SUPR0SERVICEREQHDR
{
    /** The magic. (SUPR0SERVICEREQHDR_MAGIC) */
    uint32_t    u32Magic;
    /** The size of the request. */
    uint32_t    cbReq;
} SUPR0SERVICEREQHDR;
/** Pointer to a ring-0 service request header. */
typedef SUPR0SERVICEREQHDR *PSUPR0SERVICEREQHDR;
/** the SUPVMMR0REQHDR::u32Magic value (Esbjoern Svensson - E.S.P.).  */
#define SUPR0SERVICEREQHDR_MAGIC    UINT32_C(0x19640416)


/** Event semaphore handle. Ring-0 / ring-3. */
typedef R0PTRTYPE(struct SUPSEMEVENTHANDLE *) SUPSEMEVENT;
/** Pointer to an event semaphore handle. */
typedef SUPSEMEVENT *PSUPSEMEVENT;
/** Nil event semaphore handle. */
#define NIL_SUPSEMEVENT         ((SUPSEMEVENT)0)

/**
 * Creates a single release event semaphore.
 *
 * @returns VBox status code.
 * @param   pSession        The session handle of the caller.
 * @param   phEvent         Where to return the handle to the event semaphore.
 */
SUPDECL(int) SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent);

/**
 * Closes a single release event semaphore handle.
 *
 * @returns VBox status code.
 * @retval  VINF_OBJECT_DESTROYED if the semaphore was destroyed.
 * @retval  VINF_SUCCESS if the handle was successfully closed but the semaphore
 *          object remained alive because of other references.
 *
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The handle. Nil is quietly ignored.
 */
SUPDECL(int) SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent);

/**
 * Signals a single release event semaphore.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The semaphore handle.
 */
SUPDECL(int) SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent);

#ifdef IN_RING0
/**
 * Waits on a single release event semaphore, not interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The semaphore handle.
 * @param   cMillies            The number of milliseconds to wait.
 * @remarks Not available in ring-3.
 */
SUPDECL(int) SUPSemEventWait(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint32_t cMillies);
#endif

/**
 * Waits on a single release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The semaphore handle.
 * @param   cMillies            The number of milliseconds to wait.
 */
SUPDECL(int) SUPSemEventWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint32_t cMillies);

/**
 * Waits on a single release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The semaphore handle.
 * @param   uNsTimeout          The deadline given on the RTTimeNanoTS() clock.
 */
SUPDECL(int) SUPSemEventWaitNsAbsIntr(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint64_t uNsTimeout);

/**
 * Waits on a single release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEvent              The semaphore handle.
 * @param   cNsTimeout          The number of nanoseconds to wait.
 */
SUPDECL(int) SUPSemEventWaitNsRelIntr(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint64_t cNsTimeout);

/**
 * Gets the best timeout resolution that SUPSemEventWaitNsAbsIntr and
 * SUPSemEventWaitNsAbsIntr can do.
 *
 * @returns The resolution in nanoseconds.
 * @param   pSession            The session handle of the caller.
 */
SUPDECL(uint32_t) SUPSemEventGetResolution(PSUPDRVSESSION pSession);


/** Multiple release event semaphore handle. Ring-0 / ring-3. */
typedef R0PTRTYPE(struct SUPSEMEVENTMULTIHANDLE *)  SUPSEMEVENTMULTI;
/** Pointer to an multiple release event semaphore handle. */
typedef SUPSEMEVENTMULTI                           *PSUPSEMEVENTMULTI;
/** Nil multiple release event semaphore handle. */
#define NIL_SUPSEMEVENTMULTI                        ((SUPSEMEVENTMULTI)0)

/**
 * Creates a multiple release event semaphore.
 *
 * @returns VBox status code.
 * @param   pSession        The session handle of the caller.
 * @param   phEventMulti    Where to return the handle to the event semaphore.
 */
SUPDECL(int) SUPSemEventMultiCreate(PSUPDRVSESSION pSession, PSUPSEMEVENTMULTI phEventMulti);

/**
 * Closes a multiple release event semaphore handle.
 *
 * @returns VBox status code.
 * @retval  VINF_OBJECT_DESTROYED if the semaphore was destroyed.
 * @retval  VINF_SUCCESS if the handle was successfully closed but the semaphore
 *          object remained alive because of other references.
 *
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The handle. Nil is quietly ignored.
 */
SUPDECL(int) SUPSemEventMultiClose(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti);

/**
 * Signals a multiple release event semaphore.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 */
SUPDECL(int) SUPSemEventMultiSignal(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti);

/**
 * Resets a multiple release event semaphore.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 */
SUPDECL(int) SUPSemEventMultiReset(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti);

#ifdef IN_RING0
/**
 * Waits on a multiple release event semaphore, not interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 * @param   cMillies            The number of milliseconds to wait.
 * @remarks Not available in ring-3.
 */
SUPDECL(int) SUPSemEventMultiWait(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint32_t cMillies);
#endif

/**
 * Waits on a multiple release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 * @param   cMillies            The number of milliseconds to wait.
 */
SUPDECL(int) SUPSemEventMultiWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint32_t cMillies);

/**
 * Waits on a multiple release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 * @param   uNsTimeout          The deadline given on the RTTimeNanoTS() clock.
 */
SUPDECL(int) SUPSemEventMultiWaitNsAbsIntr(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint64_t uNsTimeout);

/**
 * Waits on a multiple release event semaphore, interruptible.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle of the caller.
 * @param   hEventMulti         The semaphore handle.
 * @param   cNsTimeout          The number of nanoseconds to wait.
 */
SUPDECL(int) SUPSemEventMultiWaitNsRelIntr(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint64_t cNsTimeout);

/**
 * Gets the best timeout resolution that SUPSemEventMultiWaitNsAbsIntr and
 * SUPSemEventMultiWaitNsRelIntr can do.
 *
 * @returns The resolution in nanoseconds.
 * @param   pSession            The session handle of the caller.
 */
SUPDECL(uint32_t) SUPSemEventMultiGetResolution(PSUPDRVSESSION pSession);


#ifdef IN_RING3

/** @defgroup   grp_sup_r3     SUP Host Context Ring-3 API
 * @ingroup grp_sup
 * @{
 */

/**
 * Installs the support library.
 *
 * @returns VBox status code.
 */
SUPR3DECL(int) SUPR3Install(void);

/**
 * Uninstalls the support library.
 *
 * @returns VBox status code.
 */
SUPR3DECL(int) SUPR3Uninstall(void);

/**
 * Trusted main entry point.
 *
 * This is exported as "TrustedMain" by the dynamic libraries which contains the
 * "real" application binary for which the hardened stub is built.  The entry
 * point is invoked upon successful initialization of the support library and
 * runtime.
 *
 * @returns main kind of exit code.
 * @param   argc            The argument count.
 * @param   argv            The argument vector.
 * @param   envp            The environment vector.
 */
typedef DECLCALLBACK(int) FNSUPTRUSTEDMAIN(int argc, char **argv, char **envp);
/** Pointer to FNSUPTRUSTEDMAIN(). */
typedef FNSUPTRUSTEDMAIN *PFNSUPTRUSTEDMAIN;

/** Which operation failed. */
typedef enum SUPINITOP
{
    /** Invalid. */
    kSupInitOp_Invalid = 0,
    /** Installation integrity error. */
    kSupInitOp_Integrity,
    /** Setuid related. */
    kSupInitOp_RootCheck,
    /** Driver related. */
    kSupInitOp_Driver,
    /** IPRT init related. */
    kSupInitOp_IPRT,
    /** Place holder. */
    kSupInitOp_End
} SUPINITOP;

/**
 * Trusted error entry point, optional.
 *
 * This is exported as "TrustedError" by the dynamic libraries which contains
 * the "real" application binary for which the hardened stub is built.
 *
 * @param   pszWhere        Where the error occurred (function name).
 * @param   enmWhat         Which operation went wrong.
 * @param   rc              The status code.
 * @param   pszMsgFmt       Error message format string.
 * @param   va              The message format arguments.
 */
typedef DECLCALLBACK(void) FNSUPTRUSTEDERROR(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va);
/** Pointer to FNSUPTRUSTEDERROR. */
typedef FNSUPTRUSTEDERROR *PFNSUPTRUSTEDERROR;

/**
 * Secure main.
 *
 * This is used for the set-user-ID-on-execute binaries on unixy systems
 * and when using the open-vboxdrv-via-root-service setup on Windows.
 *
 * This function will perform the integrity checks of the VirtualBox
 * installation, open the support driver, open the root service (later),
 * and load the DLL corresponding to \a pszProgName and execute its main
 * function.
 *
 * @returns Return code appropriate for main().
 *
 * @param   pszProgName     The program name. This will be used to figure out which
 *                          DLL/SO/DYLIB to load and execute.
 * @param   fFlags          Flags.
 * @param   argc            The argument count.
 * @param   argv            The argument vector.
 * @param   envp            The environment vector.
 */
DECLHIDDEN(int) SUPR3HardenedMain(const char *pszProgName, uint32_t fFlags, int argc, char **argv, char **envp);

/** @name SUPR3SecureMain flags.
 * @{ */
/** Don't open the device. (Intended for VirtualBox without -startvm.) */
#define SUPSECMAIN_FLAGS_DONT_OPEN_DEV      RT_BIT_32(0)
/** @} */

/**
 * Initializes the support library.
 * Each successful call to SUPR3Init() must be countered by a
 * call to SUPR3Term(false).
 *
 * @returns VBox status code.
 * @param   ppSession       Where to store the session handle. Defaults to NULL.
 */
SUPR3DECL(int) SUPR3Init(PSUPDRVSESSION *ppSession);

/**
 * Terminates the support library.
 *
 * @returns VBox status code.
 * @param   fForced     Forced termination. This means to ignore the
 *                      init call count and just terminated.
 */
#ifdef __cplusplus
SUPR3DECL(int) SUPR3Term(bool fForced = false);
#else
SUPR3DECL(int) SUPR3Term(int fForced);
#endif

/**
 * Sets the ring-0 VM handle for use with fast IOCtls.
 *
 * @returns VBox status code.
 * @param   pVMR0       The ring-0 VM handle.
 *                      NIL_RTR0PTR can be used to unset the handle when the
 *                      VM is about to be destroyed.
 */
SUPR3DECL(int) SUPR3SetVMForFastIOCtl(PVMR0 pVMR0);

/**
 * Calls the HC R0 VMM entry point.
 * See VMMR0Entry() for more details.
 *
 * @returns error code specific to uFunction.
 * @param   pVMR0       Pointer to the Ring-0 (Host Context) mapping of the VM structure.
 * @param   idCpu       The virtual CPU ID.
 * @param   uOperation  Operation to execute.
 * @param   pvArg       Argument.
 */
SUPR3DECL(int) SUPR3CallVMMR0(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, void *pvArg);

/**
 * Variant of SUPR3CallVMMR0, except that this takes the fast ioclt path
 * regardsless of compile-time defaults.
 *
 * @returns VBox status code.
 * @param   pVMR0       The ring-0 VM handle.
 * @param   uOperation  The operation; only the SUP_VMMR0_DO_* ones are valid.
 * @param   idCpu       The virtual CPU ID.
 */
SUPR3DECL(int) SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu);

/**
 * Calls the HC R0 VMM entry point, in a safer but slower manner than
 * SUPR3CallVMMR0. When entering using this call the R0 components can call
 * into the host kernel (i.e. use the SUPR0 and RT APIs).
 *
 * See VMMR0Entry() for more details.
 *
 * @returns error code specific to uFunction.
 * @param   pVMR0       Pointer to the Ring-0 (Host Context) mapping of the VM structure.
 * @param   idCpu       The virtual CPU ID.
 * @param   uOperation  Operation to execute.
 * @param   u64Arg      Constant argument.
 * @param   pReqHdr     Pointer to a request header. Optional.
 *                      This will be copied in and out of kernel space. There currently is a size
 *                      limit on this, just below 4KB.
 */
SUPR3DECL(int) SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr);

/**
 * Calls a ring-0 service.
 *
 * The operation and the request packet is specific to the service.
 *
 * @returns error code specific to uFunction.
 * @param   pszService  The service name.
 * @param   cchService  The length of the service name.
 * @param   uReq        The request number.
 * @param   u64Arg      Constant argument.
 * @param   pReqHdr     Pointer to a request header. Optional.
 *                      This will be copied in and out of kernel space. There currently is a size
 *                      limit on this, just below 4KB.
 */
SUPR3DECL(int) SUPR3CallR0Service(const char *pszService, size_t cchService, uint32_t uOperation, uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);

/** Which logger. */
typedef enum SUPLOGGER
{
    SUPLOGGER_DEBUG = 1,
    SUPLOGGER_RELEASE
} SUPLOGGER;

/**
 * Changes the settings of the specified ring-0 logger.
 *
 * @returns VBox status code.
 * @param   enmWhich    Which logger.
 * @param   pszFlags    The flags settings.
 * @param   pszGroups   The groups settings.
 * @param   pszDest     The destination specificier.
 */
SUPR3DECL(int) SUPR3LoggerSettings(SUPLOGGER enmWhich, const char *pszFlags, const char *pszGroups, const char *pszDest);

/**
 * Creates a ring-0 logger instance.
 *
 * @returns VBox status code.
 * @param   enmWhich    Which logger to create.
 * @param   pszFlags    The flags settings.
 * @param   pszGroups   The groups settings.
 * @param   pszDest     The destination specificier.
 */
SUPR3DECL(int) SUPR3LoggerCreate(SUPLOGGER enmWhich, const char *pszFlags, const char *pszGroups, const char *pszDest);

/**
 * Destroys a ring-0 logger instance.
 *
 * @returns VBox status code.
 * @param   enmWhich    Which logger.
 */
SUPR3DECL(int) SUPR3LoggerDestroy(SUPLOGGER enmWhich);

/**
 * Queries the paging mode of the host OS.
 *
 * @returns The paging mode.
 */
SUPR3DECL(SUPPAGINGMODE) SUPR3GetPagingMode(void);

/**
 * Allocate zero-filled pages.
 *
 * Use this to allocate a number of pages suitable for seeding / locking.
 * Call SUPR3PageFree() to free the pages once done with them.
 *
 * @returns VBox status.
 * @param   cPages          Number of pages to allocate.
 * @param   ppvPages        Where to store the base pointer to the allocated pages.
 */
SUPR3DECL(int) SUPR3PageAlloc(size_t cPages, void **ppvPages);

/**
 * Frees pages allocated with SUPR3PageAlloc().
 *
 * @returns VBox status.
 * @param   pvPages         Pointer returned by SUPR3PageAlloc().
 * @param   cPages          Number of pages that was allocated.
 */
SUPR3DECL(int) SUPR3PageFree(void *pvPages, size_t cPages);

/**
 * Allocate non-zeroed, locked, pages with user and, optionally, kernel
 * mappings.
 *
 * Use SUPR3PageFreeEx() to free memory allocated with this function.
 *
 * @returns VBox status code.
 * @param   cPages          The number of pages to allocate.
 * @param   fFlags          Flags, reserved. Must be zero.
 * @param   ppvPages        Where to store the address of the user mapping.
 * @param   pR0Ptr          Where to store the address of the kernel mapping.
 *                          NULL if no kernel mapping is desired.
 * @param   paPages         Where to store the physical addresses of each page.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3PageAllocEx(size_t cPages, uint32_t fFlags, void **ppvPages, PRTR0PTR pR0Ptr, PSUPPAGE paPages);

/**
 * Maps a portion of a ring-3 only allocation into kernel space.
 *
 * @returns VBox status code.
 *
 * @param   pvR3            The address SUPR3PageAllocEx return.
 * @param   off             Offset to start mapping at. Must be page aligned.
 * @param   cb              Number of bytes to map. Must be page aligned.
 * @param   fFlags          Flags, must be zero.
 * @param   pR0Ptr          Where to store the address on success.
 *
 */
SUPR3DECL(int) SUPR3PageMapKernel(void *pvR3, uint32_t off, uint32_t cb, uint32_t fFlags, PRTR0PTR pR0Ptr);

/**
 * Changes the protection of
 *
 * @returns VBox status code.
 * @retval  VERR_NOT_SUPPORTED if the OS doesn't allow us to change page level
 *          protection. See also RTR0MemObjProtect.
 *
 * @param   pvR3            The ring-3 address SUPR3PageAllocEx returned.
 * @param   R0Ptr           The ring-0 address SUPR3PageAllocEx returned if it
 *                          is desired that the corresponding ring-0 page
 *                          mappings should change protection as well. Pass
 *                          NIL_RTR0PTR if the ring-0 pages should remain
 *                          unaffected.
 * @param   off             Offset to start at which to start chagning the page
 *                          level protection. Must be page aligned.
 * @param   cb              Number of bytes to change. Must be page aligned.
 * @param   fProt           The new page level protection, either a combination
 *                          of RTMEM_PROT_READ, RTMEM_PROT_WRITE and
 *                          RTMEM_PROT_EXEC, or just RTMEM_PROT_NONE.
 */
SUPR3DECL(int) SUPR3PageProtect(void *pvR3, RTR0PTR R0Ptr, uint32_t off, uint32_t cb, uint32_t fProt);

/**
 * Free pages allocated by SUPR3PageAllocEx.
 *
 * @returns VBox status code.
 * @param   pvPages         The address of the user mapping.
 * @param   cPages          The number of pages.
 */
SUPR3DECL(int) SUPR3PageFreeEx(void *pvPages, size_t cPages);

/**
 * Allocated memory with page aligned memory with a contiguous and locked physical
 * memory backing below 4GB.
 *
 * @returns Pointer to the allocated memory (virtual address).
 *          *pHCPhys is set to the physical address of the memory.
 *          If ppvR0 isn't NULL, *ppvR0 is set to the ring-0 mapping.
 *          The returned memory must be freed using SUPR3ContFree().
 * @returns NULL on failure.
 * @param   cPages      Number of pages to allocate.
 * @param   pR0Ptr      Where to store the ring-0 mapping of the allocation. (optional)
 * @param   pHCPhys     Where to store the physical address of the memory block.
 *
 * @remark  This 2nd version of this API exists because we're not able to map the
 *          ring-3 mapping executable on WIN64. This is a serious problem in regard to
 *          the world switchers.
 */
SUPR3DECL(void *) SUPR3ContAlloc(size_t cPages, PRTR0PTR pR0Ptr, PRTHCPHYS pHCPhys);

/**
 * Frees memory allocated with SUPR3ContAlloc().
 *
 * @returns VBox status code.
 * @param   pv          Pointer to the memory block which should be freed.
 * @param   cPages      Number of pages to be freed.
 */
SUPR3DECL(int) SUPR3ContFree(void *pv, size_t cPages);

/**
 * Allocated non contiguous physical memory below 4GB.
 *
 * The memory isn't zeroed.
 *
 * @returns VBox status code.
 * @returns NULL on failure.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvPages    Where to store the pointer to the allocated memory.
 *                      The pointer stored here on success must be passed to
 *                      SUPR3LowFree when the memory should be released.
 * @param   ppvPagesR0  Where to store the ring-0 pointer to the allocated memory. optional.
 * @param   paPages     Where to store the physical addresses of the individual pages.
 */
SUPR3DECL(int) SUPR3LowAlloc(size_t cPages, void **ppvPages, PRTR0PTR ppvPagesR0, PSUPPAGE paPages);

/**
 * Frees memory allocated with SUPR3LowAlloc().
 *
 * @returns VBox status code.
 * @param   pv          Pointer to the memory block which should be freed.
 * @param   cPages      Number of pages that was allocated.
 */
SUPR3DECL(int) SUPR3LowFree(void *pv, size_t cPages);

/**
 * Load a module into R0 HC.
 *
 * This will verify the file integrity in a similar manner as
 * SUPR3HardenedVerifyFile before loading it.
 *
 * @returns VBox status code.
 * @param   pszFilename     The path to the image file.
 * @param   pszModule       The module name. Max 32 bytes.
 * @param   ppvImageBase    Where to store the image address.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3LoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase, PRTERRINFO pErrInfo);

/**
 * Load a module into R0 HC.
 *
 * This will verify the file integrity in a similar manner as
 * SUPR3HardenedVerifyFile before loading it.
 *
 * @returns VBox status code.
 * @param   pszFilename         The path to the image file.
 * @param   pszModule           The module name. Max 32 bytes.
 * @param   pszSrvReqHandler    The name of the service request handler entry
 *                              point. See FNSUPR0SERVICEREQHANDLER.
 * @param   ppvImageBase        Where to store the image address.
 */
SUPR3DECL(int) SUPR3LoadServiceModule(const char *pszFilename, const char *pszModule,
                                      const char *pszSrvReqHandler, void **ppvImageBase);

/**
 * Frees a R0 HC module.
 *
 * @returns VBox status code.
 * @param   pszModule       The module to free.
 * @remark  This will not actually 'free' the module, there are of course usage counting.
 */
SUPR3DECL(int) SUPR3FreeModule(void *pvImageBase);

/**
 * Get the address of a symbol in a ring-0 module.
 *
 * @returns VBox status code.
 * @param   pszModule       The module name.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
SUPR3DECL(int) SUPR3GetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue);

/**
 * Load R0 HC VMM code.
 *
 * @returns VBox status code.
 * @deprecated  Use SUPR3LoadModule(pszFilename, "VMMR0.r0", &pvImageBase)
 */
SUPR3DECL(int) SUPR3LoadVMM(const char *pszFilename);

/**
 * Unloads R0 HC VMM code.
 *
 * @returns VBox status code.
 * @deprecated  Use SUPR3FreeModule().
 */
SUPR3DECL(int) SUPR3UnloadVMM(void);

/**
 * Get the physical address of the GIP.
 *
 * @returns VBox status code.
 * @param   pHCPhys     Where to store the physical address of the GIP.
 */
SUPR3DECL(int) SUPR3GipGetPhys(PRTHCPHYS pHCPhys);

/**
 * Verifies the integrity of a file, and optionally opens it.
 *
 * The integrity check is for whether the file is suitable for loading into
 * the hypervisor or VM process. The integrity check may include verifying
 * the authenticode/elfsign/whatever signature of the file, which can take
 * a little while.
 *
 * @returns VBox status code. On failure it will have printed a LogRel message.
 *
 * @param   pszFilename     The file.
 * @param   pszWhat         For the LogRel on failure.
 * @param   phFile          Where to store the handle to the opened file. This is optional, pass NULL
 *                          if the file should not be opened.
 * @deprecated Write a new one.
 */
SUPR3DECL(int) SUPR3HardenedVerifyFile(const char *pszFilename, const char *pszWhat, PRTFILE phFile);

/**
 * Verifies the integrity of a the current process, including the image
 * location and that the invocation was absolute.
 *
 * This must currently be called after initializing the runtime.  The intended
 * audience is set-uid-to-root applications, root services and similar.
 *
 * @returns VBox status code.  On failure
 *          message.
 * @param   pszArgv0        The first argument to main().
 * @param   fInternal       Set this to @c true if this is an internal
 *                          VirtualBox application.  Otherwise pass @c false.
 * @param   pErrInfo        Where to return extended error information.
 */
SUPR3DECL(int) SUPR3HardenedVerifySelf(const char *pszArgv0, bool fInternal, PRTERRINFO pErrInfo);

/**
 * Verifies the integrity of an installation directory.
 *
 * The integrity check verifies that the directory cannot be tampered with by
 * normal users on the system.  On Unix this translates to root ownership and
 * no symbolic linking.
 *
 * @returns VBox status code. On failure a message will be stored in @a pszErr.
 *
 * @param   pszDirPath      The directory path.
 * @param   fRecursive      Whether the check should be recursive or
 *                          not.  When set, all sub-directores will be checked,
 *                          including files (@a fCheckFiles is ignored).
 * @param   fCheckFiles     Whether to apply the same basic integrity check to
 *                          the files in the directory as the directory itself.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3HardenedVerifyDir(const char *pszDirPath, bool fRecursive, bool fCheckFiles, PRTERRINFO pErrInfo);

/**
 * Verifies the integrity of a plug-in module.
 *
 * This is similar to SUPR3HardenedLdrLoad, except it does not load the module
 * and that the module does not have to be shipped with VirtualBox.
 *
 * @returns VBox status code. On failure a message will be stored in @a pszErr.
 *
 * @param   pszFilename     The filename of the plug-in module (nothing can be
 *                          omitted here).
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3HardenedVerifyPlugIn(const char *pszFilename, PRTERRINFO pErrInfo);

/**
 * Same as RTLdrLoad() but will verify the files it loads (hardened builds).
 *
 * Will add dll suffix if missing and try load the file.
 *
 * @returns iprt status code.
 * @param   pszFilename     Image filename. This must have a path.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @param   fFlags          See RTLDRLOAD_FLAGS_XXX.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3HardenedLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo);

/**
 * Same as RTLdrLoadAppPriv() but it will verify the files it loads (hardened
 * builds).
 *
 * Will add dll suffix to the file if missing, then look for it in the
 * architecture dependent application directory.
 *
 * @returns iprt status code.
 * @param   pszFilename     Image filename.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @param   fFlags          See RTLDRLOAD_FLAGS_XXX.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3HardenedLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo);

/**
 * Same as RTLdrLoad() but will verify the files it loads (hardened builds).
 *
 * This differs from SUPR3HardenedLdrLoad() in that it can load modules from
 * extension packs and anything else safely installed on the system, provided
 * they pass the hardening tests.
 *
 * @returns iprt status code.
 * @param   pszFilename     The full path to the module, with extension.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @param   pErrInfo        Where to return extended error information.
 *                          Optional.
 */
SUPR3DECL(int) SUPR3HardenedLdrLoadPlugIn(const char *pszFilename, PRTLDRMOD phLdrMod, PRTERRINFO pErrInfo);

/**
 * Check if the host kernel can run in VMX root mode.
 *
 * @returns VINF_SUCCESS if supported, error code indicating why if not.
 */
SUPR3DECL(int) SUPR3QueryVTxSupported(void);

/**
 * Return VT-x/AMD-V capabilities.
 *
 * @returns VINF_SUCCESS if supported, error code indicating why if not.
 * @param   pfCaps      Pointer to capability dword (out).
 * @todo Intended for main, which means we need to relax the privilege requires
 *       when accessing certain vboxdrv functions.
 */
SUPR3DECL(int) SUPR3QueryVTCaps(uint32_t *pfCaps);

/** @} */
#endif /* IN_RING3 */


#ifdef IN_RING0
/** @defgroup   grp_sup_r0     SUP Host Context Ring-0 API
 * @ingroup grp_sup
 * @{
 */

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
    /** Single release event semaphore. */
    SUPDRVOBJTYPE_SEM_EVENT,
    /** Multiple release event semaphore. */
    SUPDRVOBJTYPE_SEM_EVENT_MULTI,
    /** Raw PCI device. */
    SUPDRVOBJTYPE_RAW_PCI_DEVICE,
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
SUPR0DECL(int) SUPR0ObjAddRefEx(void *pvObj, PSUPDRVSESSION pSession, bool fNoBlocking);
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
SUPR0DECL(int) SUPR0PageAllocEx(PSUPDRVSESSION pSession, uint32_t cPages, uint32_t fFlags, PRTR3PTR ppvR3, PRTR0PTR ppvR0, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0PageMapKernel(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t offSub, uint32_t cbSub, uint32_t fFlags, PRTR0PTR ppvR0);
SUPR0DECL(int) SUPR0PageProtect(PSUPDRVSESSION pSession, RTR3PTR pvR3, RTR0PTR pvR0, uint32_t offSub, uint32_t cbSub, uint32_t fProt);
SUPR0DECL(int) SUPR0PageFree(PSUPDRVSESSION pSession, RTR3PTR pvR3);
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGip);
SUPR0DECL(int) SUPR0QueryVTCaps(PSUPDRVSESSION pSession, uint32_t *pfCaps);
SUPR0DECL(int) SUPR0GipUnmap(PSUPDRVSESSION pSession);
SUPR0DECL(int) SUPR0Printf(const char *pszFormat, ...);
SUPR0DECL(SUPPAGINGMODE) SUPR0GetPagingMode(void);
SUPR0DECL(int) SUPR0EnableVTx(bool fEnable);

/** @name Absolute symbols
 * Take the address of these, don't try call them.
 * @{ */
SUPR0DECL(void) SUPR0AbsIs64bit(void);
SUPR0DECL(void) SUPR0Abs64bitKernelCS(void);
SUPR0DECL(void) SUPR0Abs64bitKernelSS(void);
SUPR0DECL(void) SUPR0Abs64bitKernelDS(void);
SUPR0DECL(void) SUPR0AbsKernelCS(void);
SUPR0DECL(void) SUPR0AbsKernelSS(void);
SUPR0DECL(void) SUPR0AbsKernelDS(void);
SUPR0DECL(void) SUPR0AbsKernelES(void);
SUPR0DECL(void) SUPR0AbsKernelFS(void);
SUPR0DECL(void) SUPR0AbsKernelGS(void);
/** @} */

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
     * @param   pSupDrvFactory      Pointer to this structure.
     * @param   pSession            The SUPDRV session making the query.
     * @param   pszInterfaceUuid    The UUID of the factory interface.
     */
    DECLR0CALLBACKMEMBER(void *, pfnQueryFactoryInterface,(struct SUPDRVFACTORY const *pSupDrvFactory, PSUPDRVSESSION pSession, const char *pszInterfaceUuid));
} SUPDRVFACTORY;
/** Pointer to a support driver factory. */
typedef SUPDRVFACTORY *PSUPDRVFACTORY;
/** Pointer to a const support driver factory. */
typedef SUPDRVFACTORY const *PCSUPDRVFACTORY;

SUPR0DECL(int) SUPR0ComponentRegisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory);
SUPR0DECL(int) SUPR0ComponentDeregisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory);
SUPR0DECL(int) SUPR0ComponentQueryFactory(PSUPDRVSESSION pSession, const char *pszName, const char *pszInterfaceUuid, void **ppvFactoryIf);


struct VTGOBJHDR;
SUPR0DECL(int)  SUPR0VtgRegisterDrv(PSUPDRVSESSION pSession, struct VTGOBJHDR *pVtgHdr, const char *pszName);
SUPR0DECL(void) SUPR0VtgDeregisterDrv(PSUPDRVSESSION pSession);
SUPR0DECL(void) SUPR0VtgFireProbe(uint32_t idProbe, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                  uintptr_t uArg3, uintptr_t uArg4);
SUPR0DECL(int)  SUPR0VtgRegisterModule(void *hMod, struct VTGOBJHDR *pVtgHdr);

/**
 * Service request callback function.
 *
 * @returns VBox status code.
 * @param   pSession    The caller's session.
 * @param   u64Arg      64-bit integer argument.
 * @param   pReqHdr     The request header. Input / Output. Optional.
 */
typedef DECLCALLBACK(int) FNSUPR0SERVICEREQHANDLER(PSUPDRVSESSION pSession, uint32_t uOperation,
                                                   uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);
/** Pointer to a FNR0SERVICEREQHANDLER(). */
typedef R0PTRTYPE(FNSUPR0SERVICEREQHANDLER *) PFNSUPR0SERVICEREQHANDLER;


/** @defgroup   grp_sup_r0_idc  The IDC Interface
 * @ingroup grp_sup_r0
 * @{
 */

/** The current SUPDRV IDC version.
 * This follows the usual high word / low word rules, i.e. high word is the
 * major number and it signifies incompatible interface changes. */
#define SUPDRV_IDC_VERSION      UINT32_C(0x00010000)

/**
 * Inter-Driver Communication Handle.
 */
typedef union SUPDRVIDCHANDLE
{
    /** Padding for opaque usage.
     * Must be greater or equal in size than the private struct. */
    void *apvPadding[4];
#ifdef SUPDRVIDCHANDLEPRIVATE_DECLARED
    /** The private view. */
    struct SUPDRVIDCHANDLEPRIVATE s;
#endif
} SUPDRVIDCHANDLE;
/** Pointer to a handle. */
typedef SUPDRVIDCHANDLE *PSUPDRVIDCHANDLE;

SUPR0DECL(int) SUPR0IdcOpen(PSUPDRVIDCHANDLE pHandle, uint32_t uReqVersion, uint32_t uMinVersion,
                            uint32_t *puSessionVersion, uint32_t *puDriverVersion, uint32_t *puDriverRevision);
SUPR0DECL(int) SUPR0IdcCall(PSUPDRVIDCHANDLE pHandle, uint32_t iReq, void *pvReq, uint32_t cbReq);
SUPR0DECL(int) SUPR0IdcClose(PSUPDRVIDCHANDLE pHandle);
SUPR0DECL(PSUPDRVSESSION) SUPR0IdcGetSession(PSUPDRVIDCHANDLE pHandle);
SUPR0DECL(int) SUPR0IdcComponentRegisterFactory(PSUPDRVIDCHANDLE pHandle, PCSUPDRVFACTORY pFactory);
SUPR0DECL(int) SUPR0IdcComponentDeregisterFactory(PSUPDRVIDCHANDLE pHandle, PCSUPDRVFACTORY pFactory);

/** @} */

/** @name Ring-0 module entry points.
 *
 * These can be exported by ring-0 modules SUP are told to load.
 *
 * @{ */
DECLEXPORT(int)  ModuleInit(void *hMod);
DECLEXPORT(void) ModuleTerm(void *hMod);
/** @}  */


/** @} */
#endif


/** @} */

RT_C_DECLS_END

#endif

