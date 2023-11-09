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

#include "primpl.h"
#include <ctype.h>
#include <string.h>
#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/initterm.h>
#endif

#ifdef VBOX
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/process.h>
#endif

PRFileDesc *_pr_stdin;
PRFileDesc *_pr_stdout;
PRFileDesc *_pr_stderr;

PRBool _pr_initialized = PR_FALSE;


PR_IMPLEMENT(PRBool) PR_Initialized(void)
{
    return _pr_initialized;
}

PRInt32 _native_threads_only = 0;

static void _PR_InitStuff(void)
{

    if (_pr_initialized) return;
    _pr_initialized = PR_TRUE;
#ifdef VBOX_USE_IPRT_IN_NSPR
    RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
#endif

    /* NOTE: These init's cannot depend on _PR_MD_CURRENT_THREAD() */
    _PR_MD_EARLY_INIT();

    _PR_InitLocks();
    _PR_InitClock();

    _PR_InitThreads(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    _PR_InitIO();

    nspr_InitializePRErrorTable();

    _PR_MD_FINAL_INIT();
}

void _PR_ImplicitInitialization(void)
{
	_PR_InitStuff();
}

PR_IMPLEMENT(void) PR_Init(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
    _PR_ImplicitInitialization();
}

PR_IMPLEMENT(PRIntn) PR_Initialize(
    PRPrimordialFn prmain, PRIntn argc, char **argv, PRUintn maxPTDs)
{
    PRIntn rv;
    _PR_ImplicitInitialization();
    rv = prmain(argc, argv);
	PR_Cleanup();
    return rv;
}  /* PR_Initialize */

/* prinit.c */

