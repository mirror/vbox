/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** File:               l4env/threads.c
** Descritpion:        Implemenation for L4 threads using l4env
** Exports:            ptthread.h
*/

#include "prlog.h"
#include "primpl.h"
#include "prpdce.h"

PR_IMPLEMENT(PRThread*) PR_CreateThread(
    PRThreadType type, void (*start)(void *arg), void *arg,
    PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize)
{
    PR_ASSERT("PR_CreateThread called!");
    return NULL;
} /* PR_CreateThread */

PR_IMPLEMENT(PRThread*) PR_CreateThreadGCAble(
    PRThreadType type, void (*start)(void *arg), void *arg, 
    PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize)
{
    PR_ASSERT("PR_CreateThreadGCAble called!");
    return NULL;
}  /* PR_CreateThreadGCAble */

PR_IMPLEMENT(void*) GetExecutionEnvironment(PRThread *thred)
{
    PR_ASSERT("GetExecutionEnvironment called!");
    return NULL;
}  /* GetExecutionEnvironment */
 
PR_IMPLEMENT(void) SetExecutionEnvironment(PRThread *thred, void *env)
{
    PR_ASSERT("SetExecutionEnvironment called!");
}  /* SetExecutionEnvironment */

PR_IMPLEMENT(PRThread*) PR_AttachThread(
    PRThreadType type, PRThreadPriority priority, PRThreadStack *stack)
{
    PR_ASSERT("PR_AttachThread called!");
    return NULL;
}  /* PR_AttachThread */


PR_IMPLEMENT(PRStatus) PR_JoinThread(PRThread *thred)
{
    PR_ASSERT("PR_JoinThread called!");
    return 0;
}  /* PR_JoinThread */

PR_IMPLEMENT(void) PR_DetachThread(void)
{
    PR_ASSERT("PR_DetachThread called!");
}  /* PR_DetachThread */

PR_IMPLEMENT(PRThread*) PR_GetCurrentThread(void)
{
    PR_ASSERT("PR_GetCurrentThread called!");
    return NULL;
}  /* PR_GetCurrentThread */

PR_IMPLEMENT(PRThreadScope) PR_GetThreadScope(const PRThread *thred)
{
    PR_ASSERT("PR_GetThreadScope called!");
    return 0;
}  /* PR_GetThreadScope() */

PR_IMPLEMENT(PRThreadType) PR_GetThreadType(const PRThread *thred)
{
    PR_ASSERT("PR_GetThreadType called!");
    return 0;
}

PR_IMPLEMENT(PRThreadState) PR_GetThreadState(const PRThread *thred)
{
    PR_ASSERT("PR_GetThreadState called!");
    return 0;
}  /* PR_GetThreadState */

PR_IMPLEMENT(PRThreadPriority) PR_GetThreadPriority(const PRThread *thred)
{
    PR_ASSERT("PR_GetThreadPriority called!");
    return 0;
}  /* PR_GetThreadPriority */

PR_IMPLEMENT(void) PR_SetThreadPriority(PRThread *thred, PRThreadPriority newPri)
{
    PR_ASSERT("PR_SetThreadPriority called!");
}  /* PR_SetThreadPriority */

PR_IMPLEMENT(PRStatus) PR_Interrupt(PRThread *thred)
{
    PR_ASSERT("PR_Interrupt called!");
    return 0;
}  /* PR_Interrupt */

PR_IMPLEMENT(void) PR_ClearInterrupt(void)
{
    PR_ASSERT("PR_ClearInterrupt called!");
}  /* PR_ClearInterrupt */

PR_IMPLEMENT(void) PR_BlockInterrupt(void)
{
    PR_ASSERT("PR_BlockInterrupt called!");
}  /* PR_BlockInterrupt */

PR_IMPLEMENT(void) PR_UnblockInterrupt(void)
{
    PR_ASSERT("PR_UnblockInterrupt called!");
}  /* PR_UnblockInterrupt */

PR_IMPLEMENT(PRStatus) PR_Yield(void)
{
    PR_ASSERT("PR_Yield called!");
    return 0;
}

PR_IMPLEMENT(PRStatus) PR_Sleep(PRIntervalTime ticks)
{
    PR_ASSERT("PR_Sleep called!");
    return 0;
}  /* PR_Sleep */

/* The next two functions are defined in the pthreads code, but also in prinit.c if
  _PR_PTHREADS and _PR_BTHREADS are not defined */
#if 0
PR_IMPLEMENT(PRStatus) PR_Cleanup(void)
{
    PR_ASSERT("PR_Cleanup called!");
    return 0;
}  /* PR_Cleanup */
#endif /* 0 */

#if 0
PR_IMPLEMENT(void) PR_ProcessExit(PRIntn status)
{
    exit(status);
}
#endif

PR_IMPLEMENT(PRUint32) PR_GetThreadID(PRThread *thred)
{
    PR_ASSERT("PR_GetThreadID called!");
    return 0;
}

/*
 * $$$
 * The following two thread-to-processor affinity functions are not
 * yet implemented for pthreads.  By the way, these functions should return
 * PRStatus rather than PRInt32 to indicate the success/failure status.
 * $$$
 */

PR_IMPLEMENT(PRInt32) PR_GetThreadAffinityMask(PRThread *thread, PRUint32 *mask)
{
    return 0;  /* not implemented */
}

PR_IMPLEMENT(PRInt32) PR_SetThreadAffinityMask(PRThread *thread, PRUint32 mask )
{
    return 0;  /* not implemented */
}

PR_IMPLEMENT(struct _PRCPU *)
PR_GetCurrentCPU(void)
{
    PR_ASSERT("PR_GetCurrentCPU called!");
    return NULL;
}  /* PR_GetCurrentCPU */

PR_IMPLEMENT(void)
PR_SetThreadDumpProc(PRThread* thread, PRThreadDumpProc dump, void *arg)
{
    PR_ASSERT("PR_SetThreadDumpProc called!");
}

/* 
 * Garbage collection support follows.
 */

PR_IMPLEMENT(void) PR_SetThreadGCAble(void)
{
    PR_ASSERT("PR_SetThreadGCAble called!");
}  /* PR_SetThreadGCAble */

PR_IMPLEMENT(void) PR_ClearThreadGCAble(void)
{
    PR_ASSERT("PR_ClearThreadGCAble called!");
}  /* PR_ClearThreadGCAble */

PR_IMPLEMENT(PRStatus) PR_EnumerateThreads(PREnumerator func, void *arg)
{
    PR_ASSERT("PR_SetThreadGCAble called!");
    return 0;
}  /* PR_EnumerateThreads */

/*
 * PR_SuspendAll and PR_ResumeAll are called during garbage collection.  The strategy 
 * we use is to send a SIGUSR2 signal to every gc able thread that we intend to suspend.
 * The signal handler will record the stack pointer and will block until resumed by
 * the resume call.  Since the signal handler is the last routine called for the
 * suspended thread, the stack pointer will also serve as a place where all the
 * registers have been saved on the stack for the previously executing routines.
 *
 * Through global variables, we also make sure that PR_Suspend and PR_Resume does not
 * proceed until the thread is suspended or resumed.
 */

PR_IMPLEMENT(void) PR_SuspendAll(void)
{
    PR_ASSERT("PR_SuspendAll called!");
}  /* PR_SuspendAll */

PR_IMPLEMENT(void) PR_ResumeAll(void)
{
    PR_ASSERT("PR_ResumeAll called!");
}  /* PR_ResumeAll */

/* Return the stack pointer for the given thread- used by the GC */
PR_IMPLEMENT(void *)PR_GetSP(PRThread *thred)
{
    PR_ASSERT("PR_SetSP called!");
    return NULL;
}  /* PR_GetSP */

/* ptthread.c */
