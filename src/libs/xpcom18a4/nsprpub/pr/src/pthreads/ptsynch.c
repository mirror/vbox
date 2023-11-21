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
** File:            ptsynch.c
** Descritpion:        Implemenation for thread synchronization using pthreads
** Exports:            prlock.h, prcvar.h, prmon.h, prcmon.h
*/

#include "primpl.h"

#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include <iprt/asm.h>
#include <iprt/assert.h>

#ifdef VBOX
/* From the removed obsolete/prsem.h */
typedef struct PRSemaphore PRSemaphore;
#endif

static pthread_mutexattr_t _pt_mattr;
static pthread_condattr_t _pt_cvar_attr;

/**************************************************************/
/**************************************************************/
/*****************************LOCKS****************************/
/**************************************************************/
/**************************************************************/

void _PR_InitLocks(void)
{
    int rv;
    rv = _PT_PTHREAD_MUTEXATTR_INIT(&_pt_mattr); 
    Assert(0 == rv);

#ifdef LINUX
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2)
    rv = pthread_mutexattr_settype(&_pt_mattr, PTHREAD_MUTEX_ADAPTIVE_NP);
    Assert(0 == rv);
#endif
#endif

    rv = _PT_PTHREAD_CONDATTR_INIT(&_pt_cvar_attr);
    Assert(0 == rv);
}

static void pt_PostNotifies(PRLock *lock, PRBool unlock)
{
    PRIntn index, rv;
    _PT_Notified post;
    _PT_Notified *notified, *prev = NULL;
    /*
     * Time to actually notify any conditions that were affected
     * while the lock was held. Get a copy of the list that's in
     * the lock structure and then zero the original. If it's
     * linked to other such structures, we own that storage.
     */
    post = lock->notified;  /* a safe copy; we own the lock */

#if defined(DEBUG)
    memset(&lock->notified, 0, sizeof(_PT_Notified));  /* reset */
#else
    lock->notified.length = 0;  /* these are really sufficient */
    lock->notified.link = NULL;
#endif

    /* should (may) we release lock before notifying? */
    if (unlock)
    {
        rv = pthread_mutex_unlock(&lock->mutex);
        Assert(0 == rv);
    }

    notified = &post;  /* this is where we start */
    do
    {
        for (index = 0; index < notified->length; ++index)
        {
            PRCondVar *cv = notified->cv[index].cv;
            Assert(NULL != cv);
            Assert(0 != notified->cv[index].times);
            if (-1 == notified->cv[index].times)
            {
                rv = pthread_cond_broadcast(&cv->cv);
                Assert(0 == rv);
            }
            else
            {
                while (notified->cv[index].times-- > 0)
                {
                    rv = pthread_cond_signal(&cv->cv);
                    Assert(0 == rv);
                }
            }
            if (0 > ASMAtomicDecS32(&cv->notify_pending))
                PR_DestroyCondVar(cv);
        }
        prev = notified;
        notified = notified->link;
        if (&post != prev) PR_DELETE(prev);
    } while (NULL != notified);
}  /* pt_PostNotifies */

PR_IMPLEMENT(PRLock*) PR_NewLock(void)
{
    PRIntn rv;
    PRLock *lock;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    lock = PR_NEWZAP(PRLock);
    if (lock != NULL)
    {
        rv = _PT_PTHREAD_MUTEX_INIT(lock->mutex, _pt_mattr); 
        Assert(0 == rv);
    }
    return lock;
}  /* PR_NewLock */

PR_IMPLEMENT(void) PR_DestroyLock(PRLock *lock)
{
    PRIntn rv;
    Assert(NULL != lock);
    Assert(PR_FALSE == lock->locked);
    Assert(0 == lock->notified.length);
    Assert(NULL == lock->notified.link);
    rv = pthread_mutex_destroy(&lock->mutex);
    Assert(0 == rv);
#if defined(DEBUG)
    memset(lock, 0xaf, sizeof(PRLock));
#endif
    PR_DELETE(lock);
}  /* PR_DestroyLock */

PR_IMPLEMENT(void) PR_Lock(PRLock *lock)
{
    PRIntn rv;
    Assert(lock != NULL);
    rv = pthread_mutex_lock(&lock->mutex);
    Assert(0 == rv);
    Assert(0 == lock->notified.length);
    Assert(NULL == lock->notified.link);
    Assert(PR_FALSE == lock->locked);
    lock->locked = PR_TRUE;
    lock->owner = pthread_self();
}  /* PR_Lock */

PR_IMPLEMENT(PRStatus) PR_Unlock(PRLock *lock)
{
    PRIntn rv;

    Assert(lock != NULL);
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(lock->mutex));
    Assert(PR_TRUE == lock->locked);
    Assert(pthread_equal(lock->owner, pthread_self()));

    if (!lock->locked || !pthread_equal(lock->owner, pthread_self()))
        return PR_FAILURE;

    lock->locked = PR_FALSE;
    if (0 == lock->notified.length)  /* shortcut */
    {
        rv = pthread_mutex_unlock(&lock->mutex);
        Assert(0 == rv);
    }
    else pt_PostNotifies(lock, PR_TRUE);

    return PR_SUCCESS;
}  /* PR_Unlock */


/**************************************************************/
/**************************************************************/
/***************************CONDITIONS*************************/
/**************************************************************/
/**************************************************************/


/*
 * This code is used to compute the absolute time for the wakeup.
 * It's moderately ugly, so it's defined here and called in a
 * couple of places.
 */
#define PT_NANOPERMICRO 1000UL
#define PT_BILLION 1000000000UL

static PRIntn pt_TimedWait(
    pthread_cond_t *cv, pthread_mutex_t *ml, PRIntervalTime timeout)
{
    int rv;
    struct timeval now;
    struct timespec tmo;
    PRUint32 ticks = PR_TicksPerSecond();

    tmo.tv_sec = (PRInt32)(timeout / ticks);
    tmo.tv_nsec = (PRInt32)(timeout - (tmo.tv_sec * ticks));
    tmo.tv_nsec = (PRInt32)PR_IntervalToMicroseconds(PT_NANOPERMICRO * tmo.tv_nsec);

    /* pthreads wants this in absolute time, off we go ... */
    (void)GETTIMEOFDAY(&now);
    /* that one's usecs, this one's nsecs - grrrr! */
    tmo.tv_sec += now.tv_sec;
    tmo.tv_nsec += (PT_NANOPERMICRO * now.tv_usec);
    tmo.tv_sec += tmo.tv_nsec / PT_BILLION;
    tmo.tv_nsec %= PT_BILLION;

    rv = pthread_cond_timedwait(cv, ml, &tmo);

    /* NSPR doesn't report timeouts */
    return (rv == ETIMEDOUT) ? 0 : rv;
}  /* pt_TimedWait */


/*
 * Notifies just get posted to the protecting mutex. The
 * actual notification is done when the lock is released so that
 * MP systems don't contend for a lock that they can't have.
 */
static void pt_PostNotifyToCvar(PRCondVar *cvar, PRBool broadcast)
{
    PRIntn index = 0;
    _PT_Notified *notified = &cvar->lock->notified;

    Assert(PR_TRUE == cvar->lock->locked);
    Assert(pthread_equal(cvar->lock->owner, pthread_self()));
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(cvar->lock->mutex));

    while (1)
    {
        for (index = 0; index < notified->length; ++index)
        {
            if (notified->cv[index].cv == cvar)
            {
                if (broadcast)
                    notified->cv[index].times = -1;
                else if (-1 != notified->cv[index].times)
                    notified->cv[index].times += 1;
                goto finished;  /* we're finished */
            }
        }
        /* if not full, enter new CV in this array */
        if (notified->length < PT_CV_NOTIFIED_LENGTH) break;

        /* if there's no link, create an empty array and link it */
        if (NULL == notified->link)
            notified->link = PR_NEWZAP(_PT_Notified);
        notified = notified->link;
    }

    /* A brand new entry in the array */
    (void)ASMAtomicIncS32(&cvar->notify_pending);
    notified->cv[index].times = (broadcast) ? -1 : 1;
    notified->cv[index].cv = cvar;
    notified->length += 1;

finished:
    Assert(PR_TRUE == cvar->lock->locked);
    Assert(pthread_equal(cvar->lock->owner, pthread_self()));
}  /* pt_PostNotifyToCvar */

PR_IMPLEMENT(PRCondVar*) PR_NewCondVar(PRLock *lock)
{
    PRCondVar *cv = PR_NEW(PRCondVar);
    Assert(lock != NULL);
    if (cv != NULL)
    {
        int rv = _PT_PTHREAD_COND_INIT(cv->cv, _pt_cvar_attr); 
        Assert(0 == rv);
        cv->lock = lock;
        cv->notify_pending = 0;
    }
    return cv;
}  /* PR_NewCondVar */

PR_IMPLEMENT(void) PR_DestroyCondVar(PRCondVar *cvar)
{
    if (0 > ASMAtomicDecS32(&cvar->notify_pending))
    {
        PRIntn rv = pthread_cond_destroy(&cvar->cv); Assert(0 == rv);
        PR_DELETE(cvar);
    }
}  /* PR_DestroyCondVar */

PR_IMPLEMENT(PRStatus) PR_WaitCondVar(PRCondVar *cvar, PRIntervalTime timeout)
{
    PRIntn rv;
    PRThread *thred = PR_GetCurrentThread();

    Assert(cvar != NULL);
    /* We'd better be locked */
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(cvar->lock->mutex));
    Assert(PR_TRUE == cvar->lock->locked);
    /* and it better be by us */
    Assert(pthread_equal(cvar->lock->owner, pthread_self()));

    if (_PT_THREAD_INTERRUPTED(thred)) goto aborted;

    /*
     * The thread waiting is used for PR_Interrupt
     */
    thred->waiting = cvar;  /* this is where we're waiting */

    /*
     * If we have pending notifies, post them now.
     *
     * This is not optimal. We're going to post these notifies
     * while we're holding the lock. That means on MP systems
     * that they are going to collide for the lock that we will
     * hold until we actually wait.
     */
    if (0 != cvar->lock->notified.length)
        pt_PostNotifies(cvar->lock, PR_FALSE);

    /*
     * We're surrendering the lock, so clear out the locked field.
     */
    cvar->lock->locked = PR_FALSE;

    if (timeout == PR_INTERVAL_NO_TIMEOUT)
        rv = pthread_cond_wait(&cvar->cv, &cvar->lock->mutex);
    else
        rv = pt_TimedWait(&cvar->cv, &cvar->lock->mutex, timeout);

    /* We just got the lock back - this better be empty */
    Assert(PR_FALSE == cvar->lock->locked);
    cvar->lock->locked = PR_TRUE;
    cvar->lock->owner = pthread_self();

    Assert(0 == cvar->lock->notified.length);
    thred->waiting = NULL;  /* and now we're not */
    if (_PT_THREAD_INTERRUPTED(thred)) goto aborted;
    if (rv != 0)
    {
        return PR_FAILURE;
    }
    return PR_SUCCESS;

aborted:
    thred->state &= ~PT_THREAD_ABORTED;
    return PR_FAILURE;
}  /* PR_WaitCondVar */

PR_IMPLEMENT(PRStatus) PR_NotifyCondVar(PRCondVar *cvar)
{
    Assert(cvar != NULL);   
    pt_PostNotifyToCvar(cvar, PR_FALSE);
    return PR_SUCCESS;
}  /* PR_NotifyCondVar */

PR_IMPLEMENT(PRStatus) PR_NotifyAllCondVar(PRCondVar *cvar)
{
    Assert(cvar != NULL);
    pt_PostNotifyToCvar(cvar, PR_TRUE);
    return PR_SUCCESS;
}  /* PR_NotifyAllCondVar */

/**************************************************************/
/**************************************************************/
/***************************MONITORS***************************/
/**************************************************************/
/**************************************************************/

PR_IMPLEMENT(PRMonitor*) PR_NewMonitor(void)
{
    PRMonitor *mon;
    PRCondVar *cvar;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    cvar = PR_NEWZAP(PRCondVar);
    if (NULL == cvar)
    {
        return NULL;
    }
    mon = PR_NEWZAP(PRMonitor);
    if (mon != NULL)
    {
        int rv;
        rv = _PT_PTHREAD_MUTEX_INIT(mon->lock.mutex, _pt_mattr); 
        Assert(0 == rv);

        _PT_PTHREAD_INVALIDATE_THR_HANDLE(mon->owner);

        mon->cvar = cvar;
        rv = _PT_PTHREAD_COND_INIT(mon->cvar->cv, _pt_cvar_attr); 
        Assert(0 == rv);
        mon->entryCount = 0;
        mon->cvar->lock = &mon->lock;
        if (0 != rv)
        {
            PR_DELETE(mon);
            PR_DELETE(cvar);
            mon = NULL;
        }
    }
    return mon;
}  /* PR_NewMonitor */

PR_IMPLEMENT(PRMonitor*) PR_NewNamedMonitor(const char* name)
{
    PRMonitor* mon = PR_NewMonitor();
    if (mon)
        mon->name = name;
    return mon;
}

PR_IMPLEMENT(void) PR_DestroyMonitor(PRMonitor *mon)
{
    int rv;
    Assert(mon != NULL);
    PR_DestroyCondVar(mon->cvar);
    rv = pthread_mutex_destroy(&mon->lock.mutex); Assert(0 == rv);
#if defined(DEBUG)
        memset(mon, 0xaf, sizeof(PRMonitor));
#endif
    PR_DELETE(mon);    
}  /* PR_DestroyMonitor */


/* The GC uses this; it is quite arguably a bad interface.  I'm just 
 * duplicating it for now - XXXMB
 */
PR_IMPLEMENT(PRIntn) PR_GetMonitorEntryCount(PRMonitor *mon)
{
    pthread_t self = pthread_self();
    if (pthread_equal(mon->owner, self))
        return mon->entryCount;
    return 0;
}

PR_IMPLEMENT(void) PR_EnterMonitor(PRMonitor *mon)
{
    pthread_t self = pthread_self();

    Assert(mon != NULL);
    /*
     * This is safe only if mon->owner (a pthread_t) can be
     * read in one instruction.  Perhaps mon->owner should be
     * a "PRThread *"?
     */
    if (!pthread_equal(mon->owner, self))
    {
        PR_Lock(&mon->lock);
        /* and now I have the lock */
        Assert(0 == mon->entryCount);
        Assert(_PT_PTHREAD_THR_HANDLE_IS_INVALID(mon->owner));
        _PT_PTHREAD_COPY_THR_HANDLE(self, mon->owner);
    }
    mon->entryCount += 1;
}  /* PR_EnterMonitor */

PR_IMPLEMENT(PRStatus) PR_ExitMonitor(PRMonitor *mon)
{
    pthread_t self = pthread_self();

    Assert(mon != NULL);
    /* The lock better be that - locked */
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(mon->lock.mutex));
    /* we'd better be the owner */
    Assert(pthread_equal(mon->owner, self));
    if (!pthread_equal(mon->owner, self))
        return PR_FAILURE;

    /* if it's locked and we have it, then the entries should be > 0 */
    Assert(mon->entryCount > 0);
    mon->entryCount -= 1;  /* reduce by one */
    if (mon->entryCount == 0)
    {
        /* and if it transitioned to zero - unlock */
        _PT_PTHREAD_INVALIDATE_THR_HANDLE(mon->owner);  /* make the owner unknown */
        PR_Unlock(&mon->lock);
    }
    return PR_SUCCESS;
}  /* PR_ExitMonitor */

PR_IMPLEMENT(PRStatus) PR_Wait(PRMonitor *mon, PRIntervalTime timeout)
{
    PRStatus rv;
    PRInt16 saved_entries;
    pthread_t saved_owner;

    Assert(mon != NULL);
    /* we'd better be locked */
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(mon->lock.mutex));
    /* and the entries better be positive */
    Assert(mon->entryCount > 0);
    /* and it better be by us */
    Assert(pthread_equal(mon->owner, pthread_self()));

    /* tuck these away 'till later */
    saved_entries = mon->entryCount; 
    mon->entryCount = 0;
    _PT_PTHREAD_COPY_THR_HANDLE(mon->owner, saved_owner);
    _PT_PTHREAD_INVALIDATE_THR_HANDLE(mon->owner);
    
    rv = PR_WaitCondVar(mon->cvar, timeout);

    /* reinstate the intresting information */
    mon->entryCount = saved_entries;
    _PT_PTHREAD_COPY_THR_HANDLE(saved_owner, mon->owner);

    return rv;
}  /* PR_Wait */

PR_IMPLEMENT(PRStatus) PR_Notify(PRMonitor *mon)
{
    Assert(NULL != mon);
    /* we'd better be locked */
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(mon->lock.mutex));
    /* and the entries better be positive */
    Assert(mon->entryCount > 0);
    /* and it better be by us */
    Assert(pthread_equal(mon->owner, pthread_self()));

    pt_PostNotifyToCvar(mon->cvar, PR_FALSE);

    return PR_SUCCESS;
}  /* PR_Notify */

PR_IMPLEMENT(PRStatus) PR_NotifyAll(PRMonitor *mon)
{
    Assert(mon != NULL);
    /* we'd better be locked */
    Assert(_PT_PTHREAD_MUTEX_IS_LOCKED(mon->lock.mutex));
    /* and the entries better be positive */
    Assert(mon->entryCount > 0);
    /* and it better be by us */
    Assert(pthread_equal(mon->owner, pthread_self()));

    pt_PostNotifyToCvar(mon->cvar, PR_TRUE);

    return PR_SUCCESS;
}  /* PR_NotifyAll */

/* ptsynch.c */
