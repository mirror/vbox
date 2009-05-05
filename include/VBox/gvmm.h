/* $Id$ */
/** @file
 * GVMM - The Global VM Manager.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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

#ifndef ___VBox_gvmm_h
#define ___VBox_gvmm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/sup.h>

__BEGIN_DECLS

/** @defgroup grp_GVMM  GVMM - The Global VM Manager.
 * @{
 */

/** @def IN_GVMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global VM Manager or not.
 */
#ifdef DOXYGEN_RUNNING
# define IN_GVMM_R0
#endif
/** @def GVMMR0DECL
 * Ring 0 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GVMM_R0
# define GVMMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GVMMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def NIL_GVM_HANDLE
 * The nil GVM VM handle value (VM::hSelf).
 */
#define NIL_GVM_HANDLE 0


/**
 * The scheduler statistics
 */
typedef struct GVMMSTATSSCHED
{
    /** The number of calls to GVMMR0SchedHalt. */
    uint64_t        cHaltCalls;
    /** The number of times we did go to sleep in GVMMR0SchedHalt. */
    uint64_t        cHaltBlocking;
    /** The number of times we timed out in GVMMR0SchedHalt. */
    uint64_t        cHaltTimeouts;
    /** The number of times we didn't go to sleep in GVMMR0SchedHalt. */
    uint64_t        cHaltNotBlocking;
    /** The number of wake ups done during GVMMR0SchedHalt. */
    uint64_t        cHaltWakeUps;

    /** The number of calls to GVMMR0WakeUp. */
    uint64_t        cWakeUpCalls;
    /** The number of times the EMT thread wasn't actually halted when GVMMR0WakeUp was called. */
    uint64_t        cWakeUpNotHalted;
    /** The number of wake ups done during GVMMR0WakeUp (not counting the explicit one). */
    uint64_t        cWakeUpWakeUps;

    /** The number of calls to GVMMR0SchedPoll. */
    uint64_t        cPollCalls;
    /** The number of times the EMT has halted in a GVMMR0SchedPoll call. */
    uint64_t        cPollHalts;
    /** The number of wake ups done during GVMMR0SchedPoll. */
    uint64_t        cPollWakeUps;
    uint64_t        u64Alignment; /**< padding */
} GVMMSTATSSCHED;
/** Pointer to the GVMM scheduler statistics. */
typedef GVMMSTATSSCHED *PGVMMSTATSSCHED;

/**
 * The GMM statistics.
 */
typedef struct GVMMSTATS
{
    /** The VM statistics if a VM was specified. */
    GVMMSTATSSCHED  SchedVM;
    /** The sum statistics of all VMs accessible to the caller. */
    GVMMSTATSSCHED  SchedSum;
    /** The number of VMs accessible to the caller. */
    uint32_t        cVMs;
    /** Alignment padding. */
    uint32_t        u32Padding;
} GVMMSTATS;
/** Pointer to the GVMM statistics. */
typedef GVMMSTATS *PGVMMSTATS;
/** Const pointer to the GVMM statistics. */
typedef const GVMMSTATS *PCGVMMSTATS;



GVMMR0DECL(int)     GVMMR0Init(void);
GVMMR0DECL(void)    GVMMR0Term(void);
GVMMR0DECL(int)     GVMMR0SetConfig(PSUPDRVSESSION pSession, const char *pszName, uint64_t u64Value);
GVMMR0DECL(int)     GVMMR0QueryConfig(PSUPDRVSESSION pSession, const char *pszName, uint64_t *pu64Value);

GVMMR0DECL(int)     GVMMR0CreateVM(PSUPDRVSESSION pSession, uint32_t cCPUs, PVM *ppVM);
GVMMR0DECL(int)     GVMMR0InitVM(PVM pVM);
GVMMR0DECL(void)    GVMMR0DoneInitVM(PVM pVM);
GVMMR0DECL(bool)    GVMMR0DoingTermVM(PVM pVM, PGVM pGVM);
GVMMR0DECL(int)     GVMMR0DestroyVM(PVM pVM);
GVMMR0DECL(PGVM)    GVMMR0ByHandle(uint32_t hGVM);
GVMMR0DECL(PGVM)    GVMMR0ByVM(PVM pVM);
GVMMR0DECL(int)     GVMMR0ByVMAndEMT(PVM pVM, PGVM *ppGVM);
GVMMR0DECL(PVM)     GVMMR0GetVMByHandle(uint32_t hGVM);
GVMMR0DECL(PVM)     GVMMR0GetVMByEMT(RTNATIVETHREAD hEMT);
GVMMR0DECL(int)     GVMMR0SchedHalt(PVM pVM, unsigned idCpu, uint64_t u64ExpireGipTime);
GVMMR0DECL(int)     GVMMR0SchedWakeUp(PVM pVM, unsigned idCpu);
GVMMR0DECL(int)     GVMMR0SchedPoll(PVM pVM, bool fYield);
GVMMR0DECL(int)     GVMMR0QueryStatistics(PGVMMSTATS pStats, PSUPDRVSESSION pSession, PVM pVM);
GVMMR0DECL(int)     GVMMR0ResetStatistics(PCGVMMSTATS pStats, PSUPDRVSESSION pSession, PVM pVM);


/**
 * Request packet for calling GVMMR0CreateVM.
 */
typedef struct GVMMCREATEVMREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** The support driver session. (IN) */
    PSUPDRVSESSION  pSession;
    /** Number of virtual CPUs for the new VM. (IN) */
    uint32_t        cCPUs;
    /** Pointer to the ring-3 mapping of the shared VM structure on return. (OUT) */
    PVMR3           pVMR3;
    /** Pointer to the ring-0 mapping of the shared VM structure on return. (OUT) */
    PVMR0           pVMR0;
} GVMMCREATEVMREQ;
/** Pointer to a GVMMR0CreateVM request packet. */
typedef GVMMCREATEVMREQ *PGVMMCREATEVMREQ;

GVMMR0DECL(int)     GVMMR0CreateVMReq(PGVMMCREATEVMREQ pReq);


/**
 * Request buffer for GVMMR0QueryStatisticsReq / VMMR0_DO_GVMM_QUERY_STATISTICS.
 * @see GVMMR0QueryStatistics.
 */
typedef struct GVMMQUERYSTATISTICSSREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The support driver session. */
    PSUPDRVSESSION  pSession;
    /** The statistics. */
    GVMMSTATS       Stats;
} GVMMQUERYSTATISTICSSREQ;
/** Pointer to a GVMMR0QueryStatisticsReq / VMMR0_DO_GVMM_QUERY_STATISTICS request buffer. */
typedef GVMMQUERYSTATISTICSSREQ *PGVMMQUERYSTATISTICSSREQ;

GVMMR0DECL(int)     GVMMR0QueryStatisticsReq(PVM pVM, PGVMMQUERYSTATISTICSSREQ pReq);


/**
 * Request buffer for GVMMR0ResetStatisticsReq / VMMR0_DO_GVMM_RESET_STATISTICS.
 * @see GVMMR0ResetStatistics.
 */
typedef struct GVMMRESETSTATISTICSSREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The support driver session. */
    PSUPDRVSESSION  pSession;
    /** The statistics to reset.
     * Any non-zero entry will be reset (if permitted). */
    GVMMSTATS       Stats;
} GVMMRESETSTATISTICSSREQ;
/** Pointer to a GVMMR0ResetStatisticsReq / VMMR0_DO_GVMM_RESET_STATISTICS request buffer. */
typedef GVMMRESETSTATISTICSSREQ *PGVMMRESETSTATISTICSSREQ;

GVMMR0DECL(int)     GVMMR0ResetStatisticsReq(PVM pVM, PGVMMRESETSTATISTICSSREQ pReq);


/** @} */

__END_DECLS

#endif

