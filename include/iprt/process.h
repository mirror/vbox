/** @file
 * innotek Portable Runtime - Process Management.
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

#ifndef ___iprt_process_h
#define ___iprt_process_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_process    RTProc - Process Management
 * @ingroup grp_rt
 * @{
 */


/**
 * Process priority.
 *
 * The process priority is used to select how scheduling properties
 * are assigned to the different thread types (see THREADTYPE).
 *
 * In addition to using the policy assigned to the process at startup (DEFAULT)
 * it is possible to change the process priority at runtime. This allows for
 * a GUI, resource manager or admin to adjust the general priorty of a task
 * without upsetting the fine-tuned priority of the threads within.
 */
typedef enum RTPROCPRIORITY
{
    /** Invalid priority. */
    RTPROCPRIORITY_INVALID = 0,
    /** Default priority.
     * Derive the schedulding policy from the priority of the RTR3Init()
     * and RTProcSetPriority() callers and the rights the process have
     * to alter its own priority.
     */
    RTPROCPRIORITY_DEFAULT,
    /** Flat priority.
     * Assumes a scheduling policy which puts the process at the default priority
     * and with all thread at the same priority.
     */
    RTPROCPRIORITY_FLAT,
    /** Low priority.
     * Assumes a scheduling policy which puts the process mostly below the
     * default priority of the host OS.
     */
    RTPROCPRIORITY_LOW,
    /** Normal priority.
     * Assume a scheduling policy which shares the cpu resources fairly with
     * other processes running with the default priority of the host OS.
     */
    RTPROCPRIORITY_NORMAL,
    /** High priority.
     * Assumes a scheduling policy which puts the task above the default
     * priority of the host OS. This policy might easily cause other tasks
     * in the system to starve.
     */
    RTPROCPRIORITY_HIGH,
    /** Last priority, used for validation. */
    RTPROCPRIORITY_LAST
} RTPROCPRIORITY;


/**
 * Get the current process identifier.
 *
 * @returns Process identifier.
 */
RTDECL(RTPROCESS) RTProcSelf(void);


#ifdef IN_RING0
/**
 * Get the current process handle.
 *
 * @returns Ring-0 process handle.
 */
RTR0DECL(RTR0PROCESS) RTR0ProcHandleSelf(void);
#endif


#ifdef IN_RING3

/**
 * Attempts to alter the priority of the current process.
 *
 * @returns iprt status code.
 * @param   enmPriority     The new priority.
 */
RTR3DECL(int) RTProcSetPriority(RTPROCPRIORITY enmPriority);

/**
 * Gets the current priority of this process.
 *
 * @returns The priority (see RTPROCPRIORITY).
 */
RTR3DECL(RTPROCPRIORITY) RTProcGetPriority(void);

/**
 * Create a child process.
 *
 * @returns iprt status code.
 * @param   pszExec     Executable image to use to create the child process.
 * @param   papszArgs   Pointer to an array of arguments to the child. The array terminated by an entry containing NULL.
 * @param   Env         Handle to the environment block for the child.
 * @param   fFlags      Flags. This is currently reserved and must be 0.
 * @param   pProcess    Where to store the process identifier on successful return.
 *                      The content is not changed on failure. NULL is allowed.
 */
RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess);

/**
 * Process exit reason.
 */
typedef enum RTPROCEXITREASON
{
    /** Normal exit. iStatus contains the exit code. */
    RTPROCEXITREASON_NORMAL = 1,
    /** Any abnormal exit. iStatus is undefined. */
    RTPROCEXITREASON_ABEND,
    /** Killed by a signal. The iStatus field contains the signal number. */
    RTPROCEXITREASON_SIGNAL
} RTPROCEXITREASON;

/**
 * Process exit status.
 */
typedef struct RTPROCSTATUS
{
    /** The process exit status if the exit was a normal one. */
    int                 iStatus;
    /** The reason the process terminated. */
    RTPROCEXITREASON    enmReason;
} RTPROCSTATUS;
/** Pointer to a process exit status structure. */
typedef RTPROCSTATUS *PRTPROCSTATUS;
/** Pointer to a const process exit status structure. */
typedef const RTPROCSTATUS *PCRTPROCSTATUS;


/** Flags for RTProcWait().
 * @{ */
/** Block indefinitly waiting for the process to exit. */
#define RTPROCWAIT_FLAGS_BLOCK      0
/** Don't block, just check if the process have exitted. */
#define RTPROCWAIT_FLAGS_NOBLOCK    1
/** @} */

/**
 * Waits for a process, resumes on interruption.
 *
 * @returns VINF_SUCCESS when the status code for the process was collected and put in *pProcStatus.
 * @returns VERR_PROCESS_NOT_FOUND if the specified process wasn't found.
 * @returns VERR_PROCESS_RUNNING when the RTPROCWAIT_FLAG_NOBLOCK and the process haven't exitted yet.
 *
 * @param   Process         The process to wait for.
 * @param   fFlags          The wait flags, any of the RTPROCWAIT_FLAGS_ \#defines.
 * @param   pProcStatus     Where to store the exit status on success.
 */
RTR3DECL(int) RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus);

/**
 * Waits for a process, returns on interruption.
 *
 * @returns VINF_SUCCESS when the status code for the process was collected and put in *pProcStatus.
 * @returns VERR_PROCESS_NOT_FOUND if the specified process wasn't found.
 * @returns VERR_PROCESS_RUNNING when the RTPROCWAIT_FLAG_NOBLOCK and the process haven't exitted yet.
 * @returns VERR_INTERRUPTED when the wait was interrupted by the arrival of a signal or other async event.
 *
 * @param   Process         The process to wait for.
 * @param   fFlags          The wait flags, any of the RTPROCWAIT_FLAGS_ \#defines.
 * @param   pProcStatus     Where to store the exit status on success.
 */
RTR3DECL(int) RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus);

/**
 * Terminates (kills) a running process.
 *
 * @returns IPRT status code.
 * @param   Process     The process to terminate.
 */
RTR3DECL(int) RTProcTerminate(RTPROCESS Process);

/**
 * Gets the processor affinity mask of the current process.
 *
 * @returns The affinity mask.
 */
RTR3DECL(uint64_t) RTProcGetAffinityMask(void);

/**
 * Gets the executable image name of the current process.
 *
 *
 * @returns pszExecName on success. NULL on buffer overflow or other errors.
 *
 * @param   pszExecName     Where to store the name.
 * @param   cchExecName     The size of the buffer.
 */
RTR3DECL(char *) RTProcGetExecutableName(char *pszExecName, size_t cchExecName);

#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif

