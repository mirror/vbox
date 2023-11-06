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

#ifndef pprthred_h___
#define pprthred_h___

/*
** API for PR private functions.  These calls are to be used by internal
** developers only.
*/
#include "nspr.h"

#if defined(XP_OS2)
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>
#endif

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_DetachThread VBoxNsprPR_DetachThread
#define PR_GetThreadID VBoxNsprPR_GetThreadID
#define PR_SetThreadDumpProc VBoxNsprPR_SetThreadDumpProc
#define PR_GetThreadAffinityMask VBoxNsprPR_GetThreadAffinityMask
#define PR_SetThreadAffinityMask VBoxNsprPR_SetThreadAffinityMask
#define PR_SetCPUAffinityMask VBoxNsprPR_SetCPUAffinityMask
#define PR_ShowStatus VBoxNsprPR_ShowStatus
#define PR_ThreadScanStackPointers VBoxNsprPR_ThreadScanStackPointers
#define PR_ScanStackPointers VBoxNsprPR_ScanStackPointers
#define PR_GetStackSpaceLeft VBoxNsprPR_GetStackSpaceLeft
#define PR_NewNamedMonitor VBoxNsprPR_NewNamedMonitor
#define PR_TestAndLock VBoxNsprPR_TestAndLock
#define PR_TestAndEnterMonitor VBoxNsprPR_TestAndEnterMonitor
#define PR_GetMonitorEntryCount VBoxNsprPR_GetMonitorEntryCount
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/*---------------------------------------------------------------------------
** THREAD PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

/*
** Get the id of the named thread. Each thread is assigned a unique id
** when it is created or attached.
*/
NSPR_API(PRUint32) PR_GetThreadID(PRThread *thread);

/*
** Set the procedure that is called when a thread is dumped. The procedure
** will be applied to the argument, arg, when called. Setting the procedure
** to NULL effectively removes it.
*/
typedef void (*PRThreadDumpProc)(PRFileDesc *fd, PRThread *t, void *arg);
NSPR_API(void) PR_SetThreadDumpProc(
    PRThread* thread, PRThreadDumpProc dump, void *arg);

/*
** Get this thread's affinity mask.  The affinity mask is a 32 bit quantity
** marking a bit for each processor this process is allowed to run on.
** The processor mask is returned in the mask argument.
** The least-significant-bit represents processor 0.
**
** Returns 0 on success, -1 on failure.
*/
NSPR_API(PRInt32) PR_GetThreadAffinityMask(PRThread *thread, PRUint32 *mask);

/*
** Set this thread's affinity mask.  
**
** Returns 0 on success, -1 on failure.
*/
NSPR_API(PRInt32) PR_SetThreadAffinityMask(PRThread *thread, PRUint32 mask );

/*
** Set the default CPU Affinity mask.
**
*/
NSPR_API(PRInt32) PR_SetCPUAffinityMask(PRUint32 mask);


/*---------------------------------------------------------------------------
** THREAD SYNCHRONIZATION PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

/*
** Create a new named monitor (named for debugging purposes).
**  Monitors are re-entrant locks with a built-in condition variable.
**
** This may fail if memory is tight or if some operating system resource
** is low.
*/
NSPR_API(PRMonitor*) PR_NewNamedMonitor(const char* name);

/*
** Test and then lock the lock if it's not already locked by some other
** thread. Return PR_FALSE if some other thread owned the lock at the
** time of the call.
*/
NSPR_API(PRBool) PR_TestAndLock(PRLock *lock);

/*
** Test and then enter the mutex associated with the monitor if it's not
** already entered by some other thread. Return PR_FALSE if some other
** thread owned the mutex at the time of the call.
*/
NSPR_API(PRBool) PR_TestAndEnterMonitor(PRMonitor *mon);

/*
** Return the number of times that the current thread has entered the
** mutex. Returns zero if the current thread has not entered the mutex.
*/
NSPR_API(PRIntn) PR_GetMonitorEntryCount(PRMonitor *mon);

/*---------------------------------------------------------------------------
** PLATFORM-SPECIFIC INITIALIZATION FUNCTIONS
---------------------------------------------------------------------------*/
/* I think PR_GetMonitorEntryCount is useless. All you really want is this... */
#define PR_InMonitor(m)		(PR_GetMonitorEntryCount(m) > 0)

PR_END_EXTERN_C

#endif /* pprthred_h___ */
