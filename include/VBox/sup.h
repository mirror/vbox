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
#include <iprt/stdarg.h>
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
 *
 * @remarks Users are making assumptions about the order here!
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
 * @retval  VINF_SUCCESS if the handle was successfully closed but the sempahore
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
 * @retval  VINF_SUCCESS if the handle was successfully closed but the sempahore
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
 * Trusted main entry point.
 *
 * This is exported as "TrustedMain" by the dynamic libraries which contains the
 * "real" application binary for which the hardened stub is built.  The entry
 * point is invoked upon successfull initialization of the support library and
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
 * @param   pszWhere        Where the error occured (function name).
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
 * Each succesful call to SUPR3Init() must be countered by a
 * call to SUPTerm(false).
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
 * @param   idCpu       The virtual CPU ID.
 * @param   uOperation  Operation to execute.
 * @param   pvArg       Argument.
 */
SUPR3DECL(int) SUPCallVMMR0(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, void *pvArg);

/**
 * Variant of SUPCallVMMR0, except that this takes the fast ioclt path
 * regardsless of compile-time defaults.
 *
 * @returns VBox status code.
 * @param   pVMR0       The ring-0 VM handle.
 * @param   uOperation  The operation; only the SUP_VMMR0_DO_* ones are valid.
 * @param   idCpu       The virtual CPU ID.
 */
SUPR3DECL(int) SUPCallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu);

/**
 * Calls the HC R0 VMM entry point, in a safer but slower manner than SUPCallVMMR0.
 * When entering using this call the R0 components can call into the host kernel
 * (i.e. use the SUPR0 and RT APIs).
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
SUPR3DECL(int) SUPCallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr);

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
 * @param   pszDest     The destionation specificier.
 */
SUPR3DECL(int) SUPR3LoggerSettings(SUPLOGGER enmWhich, const char *pszFlags, const char *pszGroups, const char *pszDest);

/**
 * Creates a ring-0 logger instance.
 *
 * @returns VBox status code.
 * @param   enmWhich    Which logger to create.
 * @param   pszFlags    The flags settings.
 * @param   pszGroups   The groups settings.
 * @param   pszDest     The destionation specificier.
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
 * Allocate non-zeroed, locked, pages with user and, optionally, kernel
 * mappings.
 *
 * Use SUPR3PageFreeEx() to free memory allocated with this function.
 *
 * This SUPR3PageAllocEx and SUPR3PageFreeEx replaces SUPPageAllocLocked,
 * SUPPageAllocLockedEx, SUPPageFreeLocked, SUPPageAlloc, SUPPageLock,
 * SUPPageUnlock and SUPPageFree.
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
 * @return VBox status code.
 *
 * @param  pvR3             The address SUPR3PageAllocEx return.
 * @param  off              Offset to start mapping at. Must be page aligned.
 * @param  cb               Number of bytes to map. Must be page aligned.
 * @param  fFlags           Flags, must be zero.
 * @param  pR0Ptr           Where to store the address on success.
 *
 */
SUPR3DECL(int) SUPR3PageMapKernel(void *pvR3, uint32_t off, uint32_t cb, uint32_t fFlags, PRTR0PTR pR0Ptr);

/**
 * Free pages allocated by SUPR3PageAllocEx.
 *
 * @returns VBox status code.
 * @param   pvPages         The address of the user mapping.
 * @param   cPages          The number of pages.
 */
SUPR3DECL(int) SUPR3PageFreeEx(void *pvPages, size_t cPages);

/**
 * Allocate non-zeroed locked pages.
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
 * This will verify the file integrity in a similar manner as
 * SUPR3HardenedVerifyFile before loading it.
 *
 * @returns VBox status code.
 * @param   pszFilename     The path to the image file.
 * @param   pszModule       The module name. Max 32 bytes.
 * @param   ppvImageBase        Where to store the image address.
 */
SUPR3DECL(int) SUPLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase);

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
 */
SUPR3DECL(int) SUPR3HardenedVerifyFile(const char *pszFilename, const char *pszWhat, PRTFILE phFile);

/**
 * Same as RTLdrLoad() but will verify the files it loads (hardened builds).
 *
 * Will add dll suffix if missing and try load the file.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename. This must have a path.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
SUPR3DECL(int) SUPR3HardenedLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod);

/**
 * Same as RTLdrLoadAppPriv() but it will verify the files it loads (hardened
 * builds).
 *
 * Will add dll suffix to the file if missing, then look for it in the
 * architecture dependent application directory.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
SUPR3DECL(int) SUPR3HardenedLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod);

/** @} */
#endif /* IN_RING3 */


#ifdef IN_RING0
/** @defgroup   grp_sup_r0     SUP Host Context Ring 0 API
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
SUPR0DECL(int) SUPR0PageAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR3PTR ppvR3, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0PageAllocEx(PSUPDRVSESSION pSession, uint32_t cPages, uint32_t fFlags, PRTR3PTR ppvR3, PRTR0PTR ppvR0, PRTHCPHYS paPages);
SUPR0DECL(int) SUPR0PageMapKernel(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t offSub, uint32_t cbSub, uint32_t fFlags, PRTR0PTR ppvR0);
SUPR0DECL(int) SUPR0PageFree(PSUPDRVSESSION pSession, RTR3PTR pvR3);
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGip);
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
 * Inter-Driver Communcation Handle.
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

/** @} */
#endif

/** @} */

__END_DECLS

#endif

