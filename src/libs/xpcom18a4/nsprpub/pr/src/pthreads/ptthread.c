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

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
static PRIntn pt_PriorityMap(PRThreadPriority pri)
{
    return pt_book.minPrio +
	    pri * (pt_book.maxPrio - pt_book.minPrio) / PR_PRIORITY_LAST;
}
#endif

static void *_pt_root(void *arg)
{
    PRIntn rv;
    PRThread *thred = (PRThread*)arg;
    PRBool detached = (thred->state & PT_THREAD_DETACHED) ? PR_TRUE : PR_FALSE;

    /*
     * Both the parent thread and this new thread set thred->id.
     * The new thread must ensure that thred->id is set before
     * it executes its startFunc.  The parent thread must ensure
     * that thred->id is set before PR_CreateThread() returns.
     * Both threads set thred->id without holding a lock.  Since
     * they are writing the same value, this unprotected double
     * write should be safe.
     */
    thred->id = pthread_self();

    /*
     * Set within the current thread the pointer to our object.
     * This object will be deleted when the thread termintates,
     * whether in a join or detached (see _PR_InitThreads()).
     */
    rv = pthread_setspecific(pt_book.key, thred);
    PR_ASSERT(0 == rv);

    /* make the thread visible to the rest of the runtime */
    PR_Lock(pt_book.ml);

    /* If this is a GCABLE thread, set its state appropriately */
    if (thred->suspend & PT_THREAD_SETGCABLE)
	    thred->state |= PT_THREAD_GCABLE;
    thred->suspend = 0;

    thred->prev = pt_book.last;
    pt_book.last->next = thred;
    thred->next = NULL;
    pt_book.last = thred;
    PR_Unlock(pt_book.ml);

    thred->startFunc(thred->arg);  /* make visible to the client */

    /* unhook the thread from the runtime */
    PR_Lock(pt_book.ml);
    /*
     * At this moment, PR_CreateThread() may not have set thred->id yet.
     * It is safe for a detached thread to free thred only after
     * PR_CreateThread() has set thred->id.
     */
    if (detached)
    {
        while (!thred->okToDelete)
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
    }

    if (thred->state & PT_THREAD_SYSTEM)
        pt_book.system -= 1;
    else if (--pt_book.user == pt_book.this_many)
        PR_NotifyAllCondVar(pt_book.cv);
    thred->prev->next = thred->next;
    if (NULL == thred->next)
        pt_book.last = thred->prev;
    else
        thred->next->prev = thred->prev;
    PR_Unlock(pt_book.ml);

    /*
    * Here we set the pthread's backpointer to the PRThread to NULL.
    * Otherwise the desctructor would get called eagerly as the thread
    * returns to the pthread runtime. The joining thread would them be
    * the proud possessor of a dangling reference. However, this is the
    * last chance to delete the object if the thread is detached, so
    * just let the destuctor do the work.
    */
    if (PR_FALSE == detached)
    {
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
    }

    return NULL;
}  /* _pt_root */

static DECLCALLBACK(int) _pt_iprt_root(
    RTTHREAD Thread, void *pvUser)
{
    PRThread *thred = (PRThread *)pvUser;
    _pt_root(thred);
    return VINF_SUCCESS;
}

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
        PR_ASSERT(0 == rv);

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

static PRThread* _PR_CreateThread(
    PRThreadType type, void (*start)(void *arg),
    void *arg, PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize, PRBool isGCAble)
{
    int rv;
    PRThread *thred;
    static uint32_t volatile s_iThread = 0;
    RTTHREADTYPE enmType;
    RTTHREAD hThread;
    uint32_t fFlags = 0;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)priority)
        priority = PR_PRIORITY_FIRST;
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)priority)
        priority = PR_PRIORITY_LAST;

    /* calc priority */
    switch (priority)
    {
        default:
        case PR_PRIORITY_NORMAL:    enmType = RTTHREADTYPE_DEFAULT; break;
        case PR_PRIORITY_LOW:       enmType = RTTHREADTYPE_MAIN_HEAVY_WORKER; break;
        case PR_PRIORITY_HIGH:      enmType = RTTHREADTYPE_MAIN_WORKER; break;
        case PR_PRIORITY_URGENT:    enmType = RTTHREADTYPE_IO; break;
    }

    if (state == PR_JOINABLE_THREAD)
        fFlags |= RTTHREADFLAGS_WAITABLE;

    thred = PR_NEWZAP(PRThread);
    if (NULL == thred)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, errno);
        goto done;
    }
    else
    {
        pthread_t id;

        thred->arg = arg;
        thred->startFunc = start;
        thred->priority = priority;
        if (PR_UNJOINABLE_THREAD == state)
            thred->state |= PT_THREAD_DETACHED;

        if (PR_LOCAL_THREAD == scope)
        	scope = PR_GLOBAL_THREAD;
        if (PR_GLOBAL_THREAD == scope)
            thred->state |= PT_THREAD_GLOBAL;
        else if (PR_GLOBAL_BOUND_THREAD == scope)
            thred->state |= (PT_THREAD_GLOBAL | PT_THREAD_BOUND);
		else	/* force it global */
            thred->state |= PT_THREAD_GLOBAL;
        if (PR_SYSTEM_THREAD == type)
            thred->state |= PT_THREAD_SYSTEM;

        thred->suspend =(isGCAble) ? PT_THREAD_SETGCABLE : 0;

#ifdef PT_NO_SIGTIMEDWAIT
        pthread_mutex_init(&thred->suspendResumeMutex,NULL);
        pthread_cond_init(&thred->suspendResumeCV,NULL);
#endif

        /* make the thread counted to the rest of the runtime */
        PR_Lock(pt_book.ml);
        if (PR_SYSTEM_THREAD == type)
            pt_book.system += 1;
        else pt_book.user += 1;
        PR_Unlock(pt_book.ml);

        /*
         * We pass a pointer to a local copy (instead of thred->id)
         * to pthread_create() because who knows what wacky things
         * pthread_create() may be doing to its argument.
         */
		rv = RTThreadCreateF(&hThread, _pt_iprt_root, thred, stackSize, enmType, fFlags, "nspr-%u", ASMAtomicIncU32(&s_iThread));
		if (RT_SUCCESS(rv)) {
			RTMEM_WILL_LEAK(hThread);
			id = (pthread_t)RTThreadGetNative(hThread);
            rv = 0;
        } 

        if (0 != rv)
        {
            PRIntn oserr = rv;
            PR_Lock(pt_book.ml);
            if (thred->state & PT_THREAD_SYSTEM)
                pt_book.system -= 1;
            else if (--pt_book.user == pt_book.this_many)
                PR_NotifyAllCondVar(pt_book.cv);
            PR_Unlock(pt_book.ml);

            PR_Free(thred);  /* all that work ... poof! */
            PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, oserr);
            thred = NULL;  /* and for what? */
            goto done;
        }

        /*
         * Both the parent thread and this new thread set thred->id.
         * The parent thread must ensure that thred->id is set before
         * PR_CreateThread() returns.  (See comments in _pt_root().)
         */
        thred->id = id;

        /*
         * If the new thread is detached, tell it that PR_CreateThread()
         * has set thred->id so it's ok to delete thred.
         */
        if (PR_UNJOINABLE_THREAD == state)
        {
            PR_Lock(pt_book.ml);
            thred->okToDelete = PR_TRUE;
            PR_NotifyAllCondVar(pt_book.cv);
            PR_Unlock(pt_book.ml);
        }
    }

done:
    return thred;
}  /* _PR_CreateThread */

PR_IMPLEMENT(PRThread*) PR_CreateThread(
    PRThreadType type, void (*start)(void *arg), void *arg,
    PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize)
{
    return _PR_CreateThread(
        type, start, arg, priority, scope, state, stackSize, PR_FALSE);
} /* PR_CreateThread */

PR_IMPLEMENT(PRStatus) PR_JoinThread(PRThread *thred)
{
    int rv = -1;
    void *result = NULL;
    PR_ASSERT(thred != NULL);

    if ((0xafafafaf == thred->state)
    || (PT_THREAD_DETACHED == (PT_THREAD_DETACHED & thred->state))
    || (PT_THREAD_FOREIGN == (PT_THREAD_FOREIGN & thred->state)))
    {
        /*
         * This might be a bad address, but if it isn't, the state should
         * either be an unjoinable thread or it's already had the object
         * deleted. However, the client that called join on a detached
         * thread deserves all the rath I can muster....
         */
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        Log(("PR_JoinThread: 0x%X not joinable | already smashed\n", thred));
    }
    else
    {
        rv = VERR_INVALID_HANDLE;
        RTTHREAD hThread = RTThreadFromNative((RTNATIVETHREAD)thred->id);
        if (hThread != NIL_RTTHREAD)
        {
            int rcThread = 0;
            rv = RTThreadWait(hThread, RT_INDEFINITE_WAIT, &rcThread);
            PR_ASSERT(RT_SUCCESS(rv) && rcThread == VINF_SUCCESS);
            if (RT_SUCCESS(rv))
            {
                rv = 0;
                _pt_thread_death(thred);
            }
            else
                PR_SetError(rv == VERR_THREAD_NOT_WAITABLE 
                            ? PR_INVALID_ARGUMENT_ERROR 
                            : PR_UNKNOWN_ERROR, 
                            rv);
        }
    }
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_JoinThread */

PR_IMPLEMENT(PRThread*) PR_GetCurrentThread(void)
{
    void *thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL == thred) thred = pt_AttachThread();
    PR_ASSERT(NULL != thred);
    return (PRThread*)thred;
}  /* PR_GetCurrentThread */

PR_IMPLEMENT(PRThreadScope) PR_GetThreadScope(const PRThread *thred)
{
    return (thred->state & PT_THREAD_BOUND) ?
        PR_GLOBAL_BOUND_THREAD : PR_GLOBAL_THREAD;
}  /* PR_GetThreadScope() */

PR_IMPLEMENT(PRThreadType) PR_GetThreadType(const PRThread *thred)
{
    return (thred->state & PT_THREAD_SYSTEM) ?
        PR_SYSTEM_THREAD : PR_USER_THREAD;
}

PR_IMPLEMENT(PRThreadState) PR_GetThreadState(const PRThread *thred)
{
    return (thred->state & PT_THREAD_DETACHED) ?
        PR_UNJOINABLE_THREAD : PR_JOINABLE_THREAD;
}  /* PR_GetThreadState */

PR_IMPLEMENT(PRThreadPriority) PR_GetThreadPriority(const PRThread *thred)
{
    PR_ASSERT(thred != NULL);
    return thred->priority;
}  /* PR_GetThreadPriority */

PR_IMPLEMENT(void) PR_SetThreadPriority(PRThread *thred, PRThreadPriority newPri)
{
    PRIntn rv = -1;

    PR_ASSERT(NULL != thred);

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)newPri)
        newPri = PR_PRIORITY_FIRST;
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)newPri)
        newPri = PR_PRIORITY_LAST;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
    if (EPERM != pt_schedpriv)
    {
        int policy;
        struct sched_param schedule;

        rv = pthread_getschedparam(thred->id, &policy, &schedule);
        if(0 == rv) {
			schedule.sched_priority = pt_PriorityMap(newPri);
			rv = pthread_setschedparam(thred->id, policy, &schedule);
			if (EPERM == rv)
			{
				pt_schedpriv = EPERM;
				Log(("PR_SetThreadPriority: no thread scheduling privilege\n"));
			}
		}
		if (rv != 0)
			rv = -1;
    }
#endif

    thred->priority = newPri;
}  /* PR_SetThreadPriority */

PR_IMPLEMENT(PRStatus) PR_Interrupt(PRThread *thred)
{
    /*
    ** If the target thread indicates that it's waiting,
    ** find the condition and broadcast to it. Broadcast
    ** since we don't know which thread (if there are more
    ** than one). This sounds risky, but clients must
    ** test their invariants when resumed from a wait and
    ** I don't expect very many threads to be waiting on
    ** a single condition and I don't expect interrupt to
    ** be used very often.
    **
    ** I don't know why I thought this would work. Must have
    ** been one of those weaker momements after I'd been
    ** smelling the vapors.
    **
    ** Even with the followng changes it is possible that
    ** the pointer to the condition variable is pointing
    ** at a bogus value. Will the unerlying code detect
    ** that?
    */
    PRCondVar *cv;
    PR_ASSERT(NULL != thred);
    if (NULL == thred) return PR_FAILURE;

    thred->state |= PT_THREAD_ABORTED;

    cv = thred->waiting;
    if ((NULL != cv) && !thred->interrupt_blocked)
    {
        PRIntn rv;
        ASMAtomicIncU32(&cv->notify_pending);
        rv = pthread_cond_broadcast(&cv->cv);
        PR_ASSERT(0 == rv);
        if (0 > ASMAtomicDecU32(&cv->notify_pending))
            PR_DestroyCondVar(cv);
    }
    return PR_SUCCESS;
}  /* PR_Interrupt */

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
    if (NULL != thred->errorString)
        PR_Free(thred->errorString);
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
    PR_ASSERT(-1 != pt_book.minPrio);
    pt_book.maxPrio = sched_get_priority_max(policy);
    PR_ASSERT(-1 != pt_book.maxPrio);
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
    
    PR_ASSERT(NULL == pt_book.ml);
    pt_book.ml = PR_NewLock();
    PR_ASSERT(NULL != pt_book.ml);
    pt_book.cv = PR_NewCondVar(pt_book.ml);
    PR_ASSERT(NULL != pt_book.cv);
    thred = PR_NEWZAP(PRThread);
    PR_ASSERT(NULL != thred);
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
    PR_ASSERT(0 == rv);
    rv = pthread_setspecific(pt_book.key, thred);
    PR_ASSERT(0 == rv);    
    PR_SetThreadPriority(thred, priority);
}  /* _PR_InitThreads */

PR_IMPLEMENT(PRStatus) PR_Cleanup(void)
{
    PRThread *me = PR_CurrentThread();
    Log(("PR_Cleanup: shutting down NSPR\n"));
    PR_ASSERT(me->state & PT_THREAD_PRIMORD);
    if (me->state & PT_THREAD_PRIMORD)
    {
        PR_Lock(pt_book.ml);
        while (pt_book.user > pt_book.this_many)
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
        PR_Unlock(pt_book.ml);

        _PR_CleanupDtoa();
        _PR_LogCleanup();
        /* Close all the fd's before calling _PR_CleanupIO */
        _PR_CleanupIO();

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
