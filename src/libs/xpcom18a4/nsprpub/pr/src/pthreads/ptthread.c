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
** File:            ptthread.c
** Descritpion:        Implemenation for threds using pthreds
** Exports:            ptthread.h
*/

#include "primpl.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <VBox/log.h>

/*
 * Record whether or not we have the privilege to set the scheduling
 * policy and priority of threads.  0 means that privilege is available.
 * EPERM means that privilege is not available.
 */

static PRIntn pt_schedpriv = 0;

static struct _PT_Bookeeping
{
    PRLock *ml;                 /* a lock to protect ourselves */
    PRCondVar *cv;              /* used to signal global things */
    PRInt32 system, user;       /* a count of the two different types */
    PRUintn this_many;          /* number of threads allowed for exit */
    pthread_key_t key;          /* private private data key */
    PRThread *first, *last;     /* list of threads we know about */
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
    PRInt32 minPrio, maxPrio;   /* range of scheduling priorities */
#endif
} pt_book = {0};

static void _pt_thread_death(void *arg);

static PRThread* pt_AttachThread(void)
{
    PRThread *thred = NULL;

    /*
     * NSPR must have been initialized when PR_AttachThread is called.
     * We cannot have PR_AttachThread call implicit initialization
     * because if multiple threads call PR_AttachThread simultaneously,
     * NSPR may be initialized more than once.
     * We can't call any function that calls PR_GetCurrentThread()
     * either (e.g., PR_SetError()) as that will result in infinite
     * recursion.
     */
    if (!_pr_initialized) return NULL;

    /* PR_NEWZAP must not call PR_GetCurrentThread() */
    thred = PR_NEWZAP(PRThread);
    if (NULL != thred)
    {
        int rv;

        thred->priority = PR_PRIORITY_NORMAL;
        thred->id = pthread_self();
        rv = pthread_setspecific(pt_book.key, thred);
        Assert(0 == rv);

        thred->state = PT_THREAD_GLOBAL | PT_THREAD_FOREIGN;
        PR_Lock(pt_book.ml);

        /* then put it into the list */
        thred->prev = pt_book.last;
	    pt_book.last->next = thred;
        thred->next = NULL;
        pt_book.last = thred;
        PR_Unlock(pt_book.ml);

    }
    return thred;  /* may be NULL */
}  /* pt_AttachThread */

PR_IMPLEMENT(PRThread*) PR_GetCurrentThread(void)
{
    void *thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL == thred) thred = pt_AttachThread();
    Assert(NULL != thred);
    return (PRThread*)thred;
}  /* PR_GetCurrentThread */

static void _pt_thread_death(void *arg)
{
    PRThread *thred = (PRThread*)arg;

    if (thred->state & PT_THREAD_FOREIGN)
    {
        PR_Lock(pt_book.ml);
        thred->prev->next = thred->next;
        if (NULL == thred->next)
            pt_book.last = thred->prev;
        else
            thred->next->prev = thred->prev;
        PR_Unlock(pt_book.ml);
    }

    PR_Free(thred->privateData);
    if (NULL != thred->syspoll_list)
        PR_Free(thred->syspoll_list);
#if defined(DEBUG)
    memset(thred, 0xaf, sizeof(PRThread));
#endif /* defined(DEBUG) */
    PR_Free(thred);
}  /* _pt_thread_death */

void _PR_InitThreads(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
    int rv;
    PRThread *thred;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
#if defined(FREEBSD)
    {
    pthread_attr_t attr;
    int policy;
    /* get the min and max priorities of the default policy */
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_getschedpolicy(&attr, &policy);
    pt_book.minPrio = sched_get_priority_min(policy);
    Assert(-1 != pt_book.minPrio);
    pt_book.maxPrio = sched_get_priority_max(policy);
    Assert(-1 != pt_book.maxPrio);
    pthread_attr_destroy(&attr);
    }
#else
    /*
    ** These might be function evaluations
    */
    pt_book.minPrio = PT_PRIO_MIN;
    pt_book.maxPrio = PT_PRIO_MAX;
#endif
#endif
    
    Assert(NULL == pt_book.ml);
    pt_book.ml = PR_NewLock();
    Assert(NULL != pt_book.ml);
    pt_book.cv = PR_NewCondVar(pt_book.ml);
    Assert(NULL != pt_book.cv);
    thred = PR_NEWZAP(PRThread);
    Assert(NULL != thred);
    thred->arg = NULL;
    thred->startFunc = NULL;
    thred->priority = priority;
    thred->id = pthread_self();

    thred->state = (PT_THREAD_DETACHED | PT_THREAD_PRIMORD);
    if (PR_SYSTEM_THREAD == type)
    {
        thred->state |= PT_THREAD_SYSTEM;
        pt_book.system += 1;
	    pt_book.this_many = 0;
    }
    else
    {
	    pt_book.user += 1;
	    pt_book.this_many = 1;
    }
    thred->next = thred->prev = NULL;
    pt_book.first = pt_book.last = thred;

    /*
     * Create a key for our use to store a backpointer in the pthread
     * to our PRThread object. This object gets deleted when the thread
     * returns from its root in the case of a detached thread. Other
     * threads delete the objects in Join.
     *
     * NB: The destructor logic seems to have a bug so it isn't used.
     * NBB: Oh really? I'm going to give it a spin - AOF 19 June 1998.
     * More info - the problem is that pthreads calls the destructor
     * eagerly as the thread returns from its root, rather than lazily
     * after the thread is joined. Therefore, threads that are joining
     * and holding PRThread references are actually holding pointers to
     * nothing.
     */
    rv = _PT_PTHREAD_KEY_CREATE(&pt_book.key, _pt_thread_death);
    Assert(0 == rv);
    rv = pthread_setspecific(pt_book.key, thred);
    Assert(0 == rv);    
}  /* _PR_InitThreads */

PR_IMPLEMENT(PRStatus) PR_Cleanup(void)
{
    PRThread *me = PR_GetCurrentThread();
    Log(("PR_Cleanup: shutting down NSPR\n"));
    Assert(me->state & PT_THREAD_PRIMORD);
    if (me->state & PT_THREAD_PRIMORD)
    {
        PR_Lock(pt_book.ml);
        while (pt_book.user > pt_book.this_many)
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
        PR_Unlock(pt_book.ml);

        /*
         * I am not sure if it's safe to delete the cv and lock here,
         * since there may still be "system" threads around. If this
         * call isn't immediately prior to exiting, then there's a
         * problem.
         */
        if (0 == pt_book.system)
        {
            PR_DestroyCondVar(pt_book.cv); pt_book.cv = NULL;
            PR_DestroyLock(pt_book.ml); pt_book.ml = NULL;
        }
        _pt_thread_death(me);

        _pr_initialized = PR_FALSE;
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}  /* PR_Cleanup */

/* ptthread.c */
