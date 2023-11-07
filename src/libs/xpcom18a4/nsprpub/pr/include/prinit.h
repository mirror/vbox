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

#ifndef prinit_h___
#define prinit_h___

#include "prthread.h"
#include "prtypes.h"
#include <stdio.h>

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_Abort VBoxNsprPR_Abort
#define PR_Cleanup VBoxNsprPR_Cleanup
#define PR_Init VBoxNsprPR_Init
#define PR_Initialize VBoxNsprPR_Initialize
#define PR_Initialized VBoxNsprPR_Initialized
#define PR_SetFDCacheSize VBoxNsprPR_SetFDCacheSize
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/************************************************************************/
/**************************IDENTITY AND VERSIONING***********************/
/************************************************************************/

/*
** NSPR's name, this should persist until at least the turn of the
** century.
*/
#define PR_NAME     "NSPR"

/*
** NSPR's version is used to determine the likelihood that the version you
** used to build your component is anywhere close to being compatible with
** what is in the underlying library.
**
** The format of the version string is
**     "<major version>.<minor version>[.<patch level>] [<Beta>]"
*/
#define PR_VERSION  "4.6 Beta"
#define PR_VMAJOR   4
#define PR_VMINOR   6
#define PR_VPATCH   0
#define PR_BETA     PR_TRUE

/************************************************************************/
/*******************************INITIALIZATION***************************/
/************************************************************************/

/*
** Initialize the runtime. Attach a thread object to the currently
** executing native thread of type "type".
**
** The specificaiton of 'maxPTDs' is ignored.
*/
NSPR_API(void) PR_Init(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs);

/*
** And alternate form of initialization, one that may become the default if
** not the only mechanism, provides a method to get the NSPR runtime init-
** ialized and place NSPR between the caller and the runtime library. This
** allows main() to be treated as any other thread root function, signalling
** its compeletion by returning and allowing the runtime to coordinate the
** completion of the other threads of the runtime.
**
** The priority of the main (or primordial) thread will be PR_PRIORITY_NORMAL.
** The thread may adjust its own priority by using PR_SetPriority(), though
** at this time the support for priorities is somewhat weak.
**
** The specificaiton of 'maxPTDs' is ignored.
**
** The value returned by PR_Initialize is the value returned from the root
** function, 'prmain'.
*/

typedef PRIntn (PR_CALLBACK *PRPrimordialFn)(PRIntn argc, char **argv);

NSPR_API(PRIntn) PR_Initialize(
    PRPrimordialFn prmain, PRIntn argc, char **argv, PRUintn maxPTDs);

/*
** Return PR_TRUE if PR_Init has already been called.
*/
NSPR_API(PRBool) PR_Initialized(void);

/*
 * Perform a graceful shutdown of NSPR.  PR_Cleanup() may be called by
 * the primordial thread near the end of the main() function.
 *
 * PR_Cleanup() attempts to synchronize the natural termination of
 * process.  It does that by blocking the caller, if and only if it is
 * the primordial thread, until the number of user threads has dropped
 * to zero.  When the primordial thread returns from main(), the process
 * will immediately and silently exit.  That is, it will (if necessary)
 * forcibly terminate any existing threads and exit without significant
 * blocking and there will be no error messages or core files.
 *
 * PR_Cleanup() returns PR_SUCCESS if NSPR is successfully shutdown,
 * or PR_FAILURE if the calling thread of this function is not the
 * primordial thread.
 */
NSPR_API(PRStatus) PR_Cleanup(void);

/*
** Control the method and size of the file descriptor (PRFileDesc*)
** cache used by the runtime. Setting 'high' to zero is for performance,
** any other value probably for debugging (see memo on FD caching).
*/
NSPR_API(PRStatus) PR_SetFDCacheSize(PRIntn low, PRIntn high);

/*
** Abort the process in a non-graceful manner. This will cause a core file,
** call to the debugger or other moral equivalent as well as causing the
** entire process to stop.
*/
NSPR_API(void) PR_Abort(void);

PR_END_EXTERN_C

#endif /* prinit_h___ */
