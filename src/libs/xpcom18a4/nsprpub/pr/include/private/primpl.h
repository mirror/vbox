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

#ifndef primpl_h___
#define primpl_h___

/*
 * HP-UX 10.10's pthread.h (DCE threads) includes dce/cma.h, which
 * has:
 *     #define sigaction _sigaction_sys
 * This macro causes chaos if signal.h gets included before pthread.h.
 * To be safe, we include pthread.h first.
 */

#include <pthread.h>

#include "nspr.h"

#include "md/prosdep.h"

PR_BEGIN_EXTERN_C

typedef struct _MDLock _MDLock;
typedef struct _MDCVar _MDCVar;

typedef struct PRLock    PRLock;
typedef struct PRCondVar PRCondVar;

/*
** The following definitions are unique to implementing NSPR using pthreads.
** Since pthreads defines most of the thread and thread synchronization
** stuff, this is a pretty small set.
*/

#define PT_CV_NOTIFIED_LENGTH 6
typedef struct _PT_Notified _PT_Notified;
struct _PT_Notified
{
    PRIntn length;              /* # of used entries in this structure */
    struct
    {
        PRCondVar *cv;          /* the condition variable notified */
        PRIntn times;           /* and the number of times notified */
    } cv[PT_CV_NOTIFIED_LENGTH];
    _PT_Notified *link;         /* link to another of these | NULL */
};

/************************************************************************/
/*************************************************************************
** The remainder of the definitions are shared by pthreads and the classic
** NSPR code. These too may be conditionalized.
*************************************************************************/
/************************************************************************/

struct PRLock {
    pthread_mutex_t mutex;          /* the underlying lock */
    _PT_Notified notified;          /* array of conditions notified */
    PRBool locked;                  /* whether the mutex is locked */
    pthread_t owner;                /* if locked, current lock owner */
};

extern void _PR_InitLocks(void);

struct PRCondVar {
    PRLock *lock;               /* associated lock that protects the condition */
    pthread_cond_t cv;          /* underlying pthreads condition */
    volatile int32_t notify_pending;     /* CV has destroy pending notification */
};

/************************************************************************/

struct PRMonitor {
    const char* name;           /* monitor name for debugging */
    PRLock lock;                /* the lock structure */
    pthread_t owner;            /* the owner of the lock or invalid */
    PRCondVar *cvar;            /* condition variable queue */
    PRUint32 entryCount;        /* # of times re-entered */
};

/************************************************************************/

extern void _PR_InitClock(void);

extern PRBool _pr_initialized;
extern void _PR_ImplicitInitialization(void);

/*************************************************************************
* External machine-dependent code provided by each OS.                     *                                                                     *
*************************************************************************/

/* Initialization related */
extern void _PR_MD_EARLY_INIT(void);
#define    _PR_MD_EARLY_INIT _MD_EARLY_INIT

extern void _PR_MD_INTERVAL_INIT(void);
#define    _PR_MD_INTERVAL_INIT _MD_INTERVAL_INIT

NSPR_API(void) _PR_MD_FINAL_INIT(void);
#define    _PR_MD_FINAL_INIT _MD_FINAL_INIT

/* Time intervals */

extern PRIntervalTime _PR_MD_GET_INTERVAL(void);
#define _PR_MD_GET_INTERVAL _MD_GET_INTERVAL

extern PRIntervalTime _PR_MD_INTERVAL_PER_SEC(void);
#define _PR_MD_INTERVAL_PER_SEC _MD_INTERVAL_PER_SEC

PR_END_EXTERN_C

#endif /* primpl_h___ */
