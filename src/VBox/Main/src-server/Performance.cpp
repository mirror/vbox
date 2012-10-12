/* $Id$ */
/** @file
 * VBox Performance Classes implementation.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * @todo list:
 *
 * 1) Detection of erroneous metric names
 */

#ifndef VBOX_COLLECTOR_TEST_CASE
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#endif
#include "Performance.h"

#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/cpuset.h>

#include <algorithm>

#include "Logging.h"

using namespace pm;

// Stubs for non-pure virtual methods

int CollectorHAL::getHostCpuLoad(ULONG * /* user */, ULONG * /* kernel */, ULONG * /* idle */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getProcessCpuLoad(RTPROCESS  /* process */, ULONG * /* user */, ULONG * /* kernel */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawHostCpuLoad(uint64_t * /* user */, uint64_t * /* kernel */, uint64_t * /* idle */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawHostNetworkLoad(const char * /* name */, uint64_t * /* rx */, uint64_t * /* tx */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawHostDiskLoad(const char * /* name */, uint64_t * /* disk_ms */, uint64_t * /* total_ms */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawProcessCpuLoad(RTPROCESS  /* process */, uint64_t * /* user */, uint64_t * /* kernel */, uint64_t * /* total */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getHostMemoryUsage(ULONG * /* total */, ULONG * /* used */, ULONG * /* available */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getHostFilesystemUsage(const char * /* name */, ULONG * /* total */, ULONG * /* used */, ULONG * /* available */)
{
    return E_NOTIMPL;
}

int CollectorHAL::getProcessMemoryUsage(RTPROCESS /* process */, ULONG * /* used */)
{
    return E_NOTIMPL;
}

/* Generic implementations */

int CollectorHAL::getHostCpuMHz(ULONG *mhz)
{
    unsigned cCpus = 0;
    uint64_t u64TotalMHz = 0;
    RTCPUSET OnlineSet;
    RTMpGetOnlineSet(&OnlineSet);
    for (RTCPUID iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
    {
        LogAleksey(("{%p} " LOG_FN_FMT ": Checking if CPU %d is member of online set...\n",
                    this, __PRETTY_FUNCTION__, (int)iCpu));
        if (RTCpuSetIsMemberByIndex(&OnlineSet, iCpu))
        {
            LogAleksey(("{%p} " LOG_FN_FMT ": Getting frequency for CPU %d...\n",
                        this, __PRETTY_FUNCTION__, (int)iCpu));
            uint32_t uMHz = RTMpGetCurFrequency(RTMpCpuIdFromSetIndex(iCpu));
            if (uMHz != 0)
            {
                LogAleksey(("{%p} " LOG_FN_FMT ": CPU %d %u MHz\n",
                            this, __PRETTY_FUNCTION__, (int)iCpu, uMHz));
                u64TotalMHz += uMHz;
                cCpus++;
            }
        }
    }

    AssertReturn(cCpus, VERR_NOT_IMPLEMENTED);
    *mhz = (ULONG)(u64TotalMHz / cCpus);

    return VINF_SUCCESS;
}

#ifndef VBOX_COLLECTOR_TEST_CASE

CollectorGuestQueue::CollectorGuestQueue()
{
    mEvent = NIL_RTSEMEVENT;
    RTSemEventCreate(&mEvent);
}

CollectorGuestQueue::~CollectorGuestQueue()
{
    RTSemEventDestroy(mEvent);
}

void CollectorGuestQueue::push(CollectorGuestRequest* rq)
{
    RTCLock lock(mLockMtx);

    mQueue.push(rq);
    RTSemEventSignal(mEvent);
}

CollectorGuestRequest* CollectorGuestQueue::pop()
{
    int rc = VINF_SUCCESS;
    CollectorGuestRequest* rq = NULL;

    do
    {
        {
            RTCLock lock(mLockMtx);

            if (!mQueue.empty())
            {
                rq = mQueue.front();
                mQueue.pop();
            }
        }

        if (rq)
            return rq;
        else
            rc = RTSemEventWaitNoResume(mEvent, RT_INDEFINITE_WAIT);
    }
    while (RT_SUCCESS(rc));

    return NULL;
}

int CGRQEnable::execute()
{
    Assert(mCGuest);
    return mCGuest->enableInternal(mMask);
}

void CGRQEnable::debugPrint(void *aObject, const char *aFunction, const char *aText)
{
    NOREF(aObject);
    NOREF(aFunction);
    NOREF(aText);
    LogAleksey(("{%p} " LOG_FN_FMT ": CGRQEnable(mask=0x%x) %s\n",
                aObject, aFunction, mMask, aText));
}

int CGRQDisable::execute()
{
    Assert(mCGuest);
    return mCGuest->disableInternal(mMask);
}

void CGRQDisable::debugPrint(void *aObject, const char *aFunction, const char *aText)
{
    NOREF(aObject);
    NOREF(aFunction);
    NOREF(aText);
    LogAleksey(("{%p} " LOG_FN_FMT ": CGRQDisable(mask=0x%x) %s\n",
                aObject, aFunction, mMask, aText));
}

int CGRQAbort::execute()
{
    return E_ABORT;
}

void CGRQAbort::debugPrint(void *aObject, const char *aFunction, const char *aText)
{
    NOREF(aObject);
    NOREF(aFunction);
    NOREF(aText);
    LogAleksey(("{%p} " LOG_FN_FMT ": CGRQAbort %s\n",
                aObject, aFunction, aText));
}

CollectorGuest::CollectorGuest(Machine *machine, RTPROCESS process) :
    mUnregistered(false), mEnabled(false), mValid(false), mMachine(machine), mProcess(process),
    mCpuUser(0), mCpuKernel(0), mCpuIdle(0),
    mMemTotal(0), mMemFree(0), mMemBalloon(0), mMemShared(0), mMemCache(0), mPageTotal(0),
    mAllocVMM(0), mFreeVMM(0), mBalloonedVMM(0), mSharedVMM(0)
{
    Assert(mMachine);
    /* cannot use ComObjPtr<Machine> in Performance.h, do it manually */
    mMachine->AddRef();
}

CollectorGuest::~CollectorGuest()
{
    /* cannot use ComObjPtr<Machine> in Performance.h, do it manually */
    mMachine->Release();
    // Assert(!cEnabled); why?
}

int CollectorGuest::enableVMMStats(bool mCollectVMMStats)
{
    HRESULT ret = S_OK;

    if (mGuest)
    {
        /* @todo: replace this with a direct call to mGuest in trunk! */
        AutoCaller autoCaller(mMachine);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        ComPtr<IInternalSessionControl> directControl;

        ret = mMachine->getDirectControl(&directControl);
        if (ret != S_OK)
            return ret;

        /* enable statistics collection; this is a remote call (!) */
        ret = directControl->EnableVMMStatistics(mCollectVMMStats);
        LogAleksey(("{%p} " LOG_FN_FMT ": %sable VMM stats (%s)\n",
                    this, __PRETTY_FUNCTION__, mCollectVMMStats?"En":"Dis",
                    SUCCEEDED(ret)?"success":"failed"));
    }

    return ret;
}

int CollectorGuest::enable(ULONG mask)
{
    return enqueueRequest(new CGRQEnable(mask));
}

int CollectorGuest::disable(ULONG mask)
{
    return enqueueRequest(new CGRQDisable(mask));
}

int CollectorGuest::enableInternal(ULONG mask)
{
    HRESULT ret = S_OK;

    if ((mEnabled & mask) == mask)
        return E_UNEXPECTED;

    if (!mEnabled)
    {
        /* Must make sure that the machine object does not get uninitialized
         * in the middle of enabling this collector. Causes timing-related
         * behavior otherwise, which we don't want. In particular the
         * GetRemoteConsole call below can hang if the VM didn't completely
         * terminate (the VM processes stop processing events shortly before
         * closing the session). This avoids the hang. */
        AutoCaller autoCaller(mMachine);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        mMachineName = mMachine->getName();

        ComPtr<IInternalSessionControl> directControl;

        ret = mMachine->getDirectControl(&directControl);
        if (ret != S_OK)
            return ret;

        /* get the associated console; this is a remote call (!) */
        ret = directControl->GetRemoteConsole(mConsole.asOutParam());
        if (ret != S_OK)
            return ret;

        ret = mConsole->COMGETTER(Guest)(mGuest.asOutParam());
        if (ret == S_OK)
        {
            ret = mGuest->COMSETTER(StatisticsUpdateInterval)(1 /* 1 sec */);
            LogAleksey(("{%p} " LOG_FN_FMT ": Set guest statistics update interval to 1 sec (%s)\n",
                        this, __PRETTY_FUNCTION__, SUCCEEDED(ret)?"success":"failed"));
        }
    }
    if ((mask & GUESTSTATS_VMMRAM) == GUESTSTATS_VMMRAM)
        enableVMMStats(true);
    mEnabled |= mask;

    return ret;
}

int CollectorGuest::disableInternal(ULONG mask)
{
    if (!(mEnabled & mask))
        return E_UNEXPECTED;

    if ((mask & GUESTSTATS_VMMRAM) == GUESTSTATS_VMMRAM)
        enableVMMStats(false);
    mEnabled &= ~mask;
    if (!mEnabled)
    {
        Assert(mGuest && mConsole);
        HRESULT ret = mGuest->COMSETTER(StatisticsUpdateInterval)(0 /* off */);
        NOREF(ret);
        LogAleksey(("{%p} " LOG_FN_FMT ": Set guest statistics update interval to 0 sec (%s)\n",
                    this, __PRETTY_FUNCTION__, SUCCEEDED(ret)?"success":"failed"));
        invalidate(GUESTSTATS_ALL);
    }

    return S_OK;
}

int CollectorGuest::enqueueRequest(CollectorGuestRequest *aRequest)
{
    if (mManager)
    {
        aRequest->setGuest(this);
        return mManager->enqueueRequest(aRequest);
    }

    LogAleksey(("{%p} " LOG_FN_FMT ": Attempted enqueue guest request when mManager is null\n",
                this, __PRETTY_FUNCTION__));
    return E_POINTER;
}

void CollectorGuest::updateStats(ULONG aValidStats, ULONG aCpuUser,
                                 ULONG aCpuKernel, ULONG aCpuIdle,
                                 ULONG aMemTotal, ULONG aMemFree,
                                 ULONG aMemBalloon, ULONG aMemShared,
                                 ULONG aMemCache, ULONG aPageTotal,
                                 ULONG aAllocVMM, ULONG aFreeVMM,
                                 ULONG aBalloonedVMM, ULONG aSharedVMM)
{
    if ((aValidStats & GUESTSTATS_CPULOAD) == GUESTSTATS_CPULOAD)
    {
        mCpuUser   = aCpuUser;
        mCpuKernel = aCpuKernel,
        mCpuIdle   = aCpuIdle;
    }
    if ((aValidStats & GUESTSTATS_RAMUSAGE) == GUESTSTATS_RAMUSAGE)
    {
        mMemTotal   = aMemTotal;
        mMemFree    = aMemFree;
        mMemBalloon = aMemBalloon;
        mMemShared  = aMemShared;
        mMemCache   = aMemCache;
        mPageTotal  = aPageTotal;
    }
    if ((aValidStats & GUESTSTATS_VMMRAM) == GUESTSTATS_VMMRAM)
    {
        mAllocVMM     = aAllocVMM;
        mFreeVMM      = aFreeVMM;
        mBalloonedVMM = aBalloonedVMM;
        mSharedVMM    = aSharedVMM;
    }
    mValid = aValidStats;
}

CollectorGuestManager::CollectorGuestManager()
  : mVMMStatsProvider(NULL), mGuestBeingCalled(NULL)
{
    int rc = RTThreadCreate(&mThread, CollectorGuestManager::requestProcessingThread,
                            this, 0, RTTHREADTYPE_MAIN_WORKER, RTTHREADFLAGS_WAITABLE,
                            "CGMgr");
    NOREF(rc);
    LogAleksey(("{%p} " LOG_FN_FMT ": RTThreadCreate returned %u (mThread=%p)\n",
                this, __PRETTY_FUNCTION__, rc));
}

CollectorGuestManager::~CollectorGuestManager()
{
    Assert(mGuests.size() == 0);
    int rcThread = 0;
    int rc = enqueueRequest(new CGRQAbort());
    if (SUCCEEDED(rc))
    {
        /* We wait only if we were able to put the abort request to a queue */
        LogAleksey(("{%p} " LOG_FN_FMT ": Waiting for CGM request processing thread to stop...\n",
                    this, __PRETTY_FUNCTION__));
        rc = RTThreadWait(mThread, 1000 /* 1 sec */, &rcThread);
        LogAleksey(("{%p} " LOG_FN_FMT ": RTThreadWait returned %u (thread exit code: %u)\n",
                    this, __PRETTY_FUNCTION__, rc, rcThread));
    }
}

void CollectorGuestManager::registerGuest(CollectorGuest* pGuest)
{
    pGuest->setManager(this);
    mGuests.push_back(pGuest);
    /*
     * If no VMM stats provider was elected previously than this is our
     * candidate.
     */
    if (!mVMMStatsProvider)
        mVMMStatsProvider = pGuest;
    LogAleksey(("{%p} " LOG_FN_FMT ": Registered guest=%p provider=%p\n",
                this, __PRETTY_FUNCTION__, pGuest, mVMMStatsProvider));
}

void CollectorGuestManager::unregisterGuest(CollectorGuest* pGuest)
{
    int rc = S_OK;

    LogAleksey(("{%p} " LOG_FN_FMT ": About to unregister guest=%p provider=%p\n",
                this, __PRETTY_FUNCTION__, pGuest, mVMMStatsProvider));
    //mGuests.remove(pGuest); => destroyUnregistered()
    pGuest->unregister();
    if (pGuest == mVMMStatsProvider)
    {
        /* This was our VMM stats provider, it is time to re-elect */
        CollectorGuestList::iterator it;
        /* Assume that nobody can provide VMM stats */
        mVMMStatsProvider = NULL;

        for (it = mGuests.begin(); it != mGuests.end(); it++)
        {
            /* Skip unregistered as they are about to be destroyed */
            if ((*it)->isUnregistered())
                continue;

            if ((*it)->isEnabled())
            {
                /* Found the guest already collecting stats, elect it */
                mVMMStatsProvider = *it;
                rc = mVMMStatsProvider->enqueueRequest(new CGRQEnable(GUESTSTATS_VMMRAM));
                if (FAILED(rc))
                {
                    /* This is not a good candidate -- try to find another */
                    mVMMStatsProvider = NULL;
                    continue;
                }
                break;
            }
        }
        if (!mVMMStatsProvider)
        {
            /* If nobody collects stats, take the first registered */
            for (it = mGuests.begin(); it != mGuests.end(); it++)
            {
                /* Skip unregistered as they are about to be destroyed */
                if ((*it)->isUnregistered())
                    continue;

                mVMMStatsProvider = *it;
                //mVMMStatsProvider->enable(GUESTSTATS_VMMRAM);
                rc = mVMMStatsProvider->enqueueRequest(new CGRQEnable(GUESTSTATS_VMMRAM));
                if (SUCCEEDED(rc))
                    break;
                /* This was not a good candidate -- try to find another */
                mVMMStatsProvider = NULL;
            }
        }
    }
    LogAleksey(("{%p} " LOG_FN_FMT ": LEAVE new provider=%p\n",
                this, __PRETTY_FUNCTION__, mVMMStatsProvider));
}

void CollectorGuestManager::destroyUnregistered()
{
    CollectorGuestList::iterator it;

    for (it = mGuests.begin(); it != mGuests.end();)
        if ((*it)->isUnregistered())
        {
            delete *it;
            it = mGuests.erase(it);
            LogAleksey(("{%p} " LOG_FN_FMT ": Number of guests after erasing unregistered is %d\n",
                        this, __PRETTY_FUNCTION__, mGuests.size()));
        }
        else
            ++it;
}

int CollectorGuestManager::enqueueRequest(CollectorGuestRequest *aRequest)
{
#ifdef DEBUG
    aRequest->debugPrint(this, __PRETTY_FUNCTION__, "added to CGM queue");
#endif /* DEBUG */
    /*
     * It is very unlikely that we will get high frequency calls to configure
     * guest metrics collection, so we rely on this fact to detect blocked
     * guests. If the guest has not finished processing the previous request
     * after half a second we consider it blocked.
     */
    if (aRequest->getGuest() && aRequest->getGuest() == mGuestBeingCalled)
    {
        /* 
         * Before we can declare a guest blocked we need to wait for a while
         * and then check again as it may never had a chance to process
         * the previous request. Half a second is an eternity for processes
         * and is barely noticable by humans.
         */
        LogAleksey(("{%p} " LOG_FN_FMT ": Suspecting %s is stalled. Waiting for .5 sec...\n",
                    this, __PRETTY_FUNCTION__,
                    aRequest->getGuest()->getVMName().c_str()));
        RTThreadSleep(500 /* ms */);
        if (aRequest->getGuest() == mGuestBeingCalled) {
            LogAleksey(("{%p} " LOG_FN_FMT ": Request processing stalled for %s\n",
                        this, __PRETTY_FUNCTION__,
                        aRequest->getGuest()->getVMName().c_str()));
            /* Request execution got stalled for this guest -- report an error */
            return E_FAIL;
        }
    }
    mQueue.push(aRequest);
    return S_OK;
}

/* static */
DECLCALLBACK(int) CollectorGuestManager::requestProcessingThread(RTTHREAD /* aThread */, void *pvUser)
{
    CollectorGuestRequest *pReq;
    CollectorGuestManager *mgr = static_cast<CollectorGuestManager*>(pvUser);

    HRESULT rc = S_OK;

    LogAleksey(("{%p} " LOG_FN_FMT ": Starting request processing loop...\n",
                mgr, __PRETTY_FUNCTION__));
    while ((pReq = mgr->mQueue.pop()) != NULL)
    {
#ifdef DEBUG
        pReq->debugPrint(mgr, __PRETTY_FUNCTION__, "is being executed...");
#endif /* DEBUG */
        mgr->mGuestBeingCalled = pReq->getGuest();
        rc = pReq->execute();
        mgr->mGuestBeingCalled = NULL;
        delete pReq;
        if (rc == E_ABORT)
            break;
        if (FAILED(rc))
            LogAleksey(("{%p} " LOG_FN_FMT ": request::execute returned %u\n",
                        mgr, __PRETTY_FUNCTION__, rc));
    }
    LogAleksey(("{%p} " LOG_FN_FMT ": Exiting request processing loop... rc=%u\n",
                        mgr, __PRETTY_FUNCTION__, rc));

    return VINF_SUCCESS;
}


#endif /* !VBOX_COLLECTOR_TEST_CASE */

bool BaseMetric::collectorBeat(uint64_t nowAt)
{
    if (isEnabled())
    {
        if (mLastSampleTaken == 0)
        {
            mLastSampleTaken = nowAt;
            Log4(("{%p} " LOG_FN_FMT ": Collecting %s for obj(%p)...\n",
                        this, __PRETTY_FUNCTION__, getName(), (void *)mObject));
            return true;
        }
        /*
         * We use low resolution timers which may fire just a little bit early.
         * We compensate for that by jumping into the future by several
         * milliseconds (see @bugref{6345}).
         */
        if (nowAt - mLastSampleTaken + PM_SAMPLER_PRECISION_MS >= mPeriod * 1000)
        {
            /*
             * We don't want the beat to drift. This is why the timestamp of
             * the last taken sample is not the actual time but the time we
             * should have taken the measurement at.
             */
            mLastSampleTaken += mPeriod * 1000;
            Log4(("{%p} " LOG_FN_FMT ": Collecting %s for obj(%p)...\n",
                        this, __PRETTY_FUNCTION__, getName(), (void *)mObject));
            return true;
        }
        Log4(("{%p} " LOG_FN_FMT ": Enabled but too early to collect %s for obj(%p)\n",
              this, __PRETTY_FUNCTION__, getName(), (void *)mObject));
    }
    return false;
}

void HostCpuLoad::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
    mIdle->init(mLength);
}

void HostCpuLoad::collect()
{
    ULONG user, kernel, idle;
    int rc = mHAL->getHostCpuLoad(&user, &kernel, &idle);
    if (RT_SUCCESS(rc))
    {
        mUser->put(user);
        mKernel->put(kernel);
        mIdle->put(idle);
    }
}

void HostCpuLoadRaw::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectHostCpuLoad();
}

void HostCpuLoadRaw::collect()
{
    uint64_t user, kernel, idle;
    uint64_t userDiff, kernelDiff, idleDiff, totalDiff;

    int rc = mHAL->getRawHostCpuLoad(&user, &kernel, &idle);
    if (RT_SUCCESS(rc))
    {
        userDiff   = user   - mUserPrev;
        kernelDiff = kernel - mKernelPrev;
        idleDiff   = idle   - mIdlePrev;
        totalDiff  = userDiff + kernelDiff + idleDiff;

        if (totalDiff == 0)
        {
            /* This is only possible if none of counters has changed! */
            LogFlowThisFunc(("Impossible! User, kernel and idle raw "
                "counters has not changed since last sample.\n" ));
            mUser->put(0);
            mKernel->put(0);
            mIdle->put(0);
        }
        else
        {
            mUser->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * userDiff / totalDiff));
            mKernel->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * kernelDiff / totalDiff));
            mIdle->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * idleDiff / totalDiff));
        }

        mUserPrev   = user;
        mKernelPrev = kernel;
        mIdlePrev   = idle;
    }
}

void HostNetworkLoadRaw::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mRx->init(mLength);
    mTx->init(mLength);
    int rc = mHAL->getRawHostNetworkLoad(mShortName.c_str(), &mRxPrev, &mTxPrev);
    //AssertRC(rc);
}

void HostNetworkLoadRaw::preCollect(CollectorHints& /* hints */, uint64_t /* iTick */)
{
    if (RT_FAILURE(mRc))
    {
        ComPtr<IHostNetworkInterface> networkInterface;
        ComPtr<IHost> host = getObject();
        HRESULT hrc = host->FindHostNetworkInterfaceByName(com::Bstr(mInterfaceName).raw(), networkInterface.asOutParam());
        if (SUCCEEDED(hrc))
        {
            LogRel(("Failed to collect network metrics for %s: %Rrc (%d).\n", mInterfaceName.c_str(), mRc, mRc));
            mRc = VINF_SUCCESS;
        }
    }
}

void HostNetworkLoadRaw::collect()
{
    uint64_t rx, tx;

    mRc = mHAL->getRawHostNetworkLoad(mShortName.c_str(), &rx, &tx);
    if (RT_SUCCESS(mRc))
    {
        uint64_t rxDiff = rx - mRxPrev;
        uint64_t txDiff = tx - mTxPrev;

        if (RT_UNLIKELY(mSpeed * getPeriod() == 0))
        {
            LogFlowThisFunc(("Check cable for %s! speed=%llu period=%d.\n", mShortName.c_str(), mSpeed, getPeriod()));
            mRx->put(0);
            mTx->put(0);
        }
        else
        {
            mRx->put((ULONG)(PM_NETWORK_LOAD_MULTIPLIER * rxDiff / (mSpeed * getPeriod())));
            mTx->put((ULONG)(PM_NETWORK_LOAD_MULTIPLIER * txDiff / (mSpeed * getPeriod())));
        }

        mRxPrev = rx;
        mTxPrev = tx;
    }
    else
        LogFlowThisFunc(("Failed to collect data: %Rrc (%d)."
                         " Will update the list of interfaces...\n", mRc,mRc));
}

void HostDiskLoadRaw::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUtil->init(mLength);
    int rc = mHAL->getRawHostDiskLoad(mDiskName.c_str(), &mDiskPrev, &mTotalPrev);
    AssertRC(rc);
}

void HostDiskLoadRaw::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectHostCpuLoad();
}

void HostDiskLoadRaw::collect()
{
    uint64_t disk, total;

    int rc = mHAL->getRawHostDiskLoad(mDiskName.c_str(), &disk, &total);
    if (RT_SUCCESS(rc))
    {
        uint64_t diskDiff = disk - mDiskPrev;
        uint64_t totalDiff = total - mTotalPrev;

        if (RT_UNLIKELY(totalDiff == 0))
        {
            Assert(totalDiff);
            LogFlowThisFunc(("Improbable! Less than millisecond passed! Disk=%s\n", mDiskName.c_str()));
            mUtil->put(0);
        }
        else if (diskDiff > totalDiff)
        {
            /*
             * It is possible that the disk spent more time than CPU because
             * CPU measurements are taken during the pre-collect phase. We try
             * to compensate for than by adding the extra to the next round of
             * measurements.
             */
            mUtil->put(PM_NETWORK_LOAD_MULTIPLIER);
            Assert((diskDiff - totalDiff) < mPeriod * 1000);
            if ((diskDiff - totalDiff) > mPeriod * 1000)
            {
                LogRel(("Disk utilization time exceeds CPU time by more"
                        " than the collection period (%llu ms)\n", diskDiff - totalDiff));
            }
            else
            {
                disk = mDiskPrev + totalDiff;
                LogFlowThisFunc(("Moved %u milliseconds to the next period.\n", (unsigned)(diskDiff - totalDiff)));
            }
        }
        else
        {
            mUtil->put((ULONG)(PM_NETWORK_LOAD_MULTIPLIER * diskDiff / totalDiff));
        }

        mDiskPrev = disk;
        mTotalPrev = total;
    }
    else
        LogFlowThisFunc(("Failed to collect data: %Rrc (%d).\n", rc));
}

void HostCpuMhz::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mMHz->init(mLength);
}

void HostCpuMhz::collect()
{
    ULONG mhz;
    int rc = mHAL->getHostCpuMHz(&mhz);
    if (RT_SUCCESS(rc))
        mMHz->put(mhz);
}

void HostRamUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mTotal->init(mLength);
    mUsed->init(mLength);
    mAvailable->init(mLength);
}

void HostRamUsage::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectHostRamUsage();
}

void HostRamUsage::collect()
{
    ULONG total, used, available;
    int rc = mHAL->getHostMemoryUsage(&total, &used, &available);
    if (RT_SUCCESS(rc))
    {
        mTotal->put(total);
        mUsed->put(used);
        mAvailable->put(available);

    }
}

void HostFilesystemUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mTotal->init(mLength);
    mUsed->init(mLength);
    mAvailable->init(mLength);
}

void HostFilesystemUsage::preCollect(CollectorHints& /* hints */, uint64_t /* iTick */)
{
}

void HostFilesystemUsage::collect()
{
    ULONG total, used, available;
    int rc = mHAL->getHostFilesystemUsage(mFsName.c_str(), &total, &used, &available);
    if (RT_SUCCESS(rc))
    {
        mTotal->put(total);
        mUsed->put(used);
        mAvailable->put(available);

    }
}

#ifndef VBOX_COLLECTOR_TEST_CASE
void HostRamVmm::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mAllocVMM->init(mLength);
    mFreeVMM->init(mLength);
    mBalloonVMM->init(mLength);
    mSharedVMM->init(mLength);
}

int HostRamVmm::enable()
{
    int rc = S_OK;
    CollectorGuest *provider = mCollectorGuestManager->getVMMStatsProvider();
    if (provider)
        rc = provider->enable(GUESTSTATS_VMMRAM);
    BaseMetric::enable();
    return rc;
}

int HostRamVmm::disable()
{
    int rc = S_OK;
    BaseMetric::disable();
    CollectorGuest *provider = mCollectorGuestManager->getVMMStatsProvider();
    if (provider)
        rc = provider->disable(GUESTSTATS_VMMRAM);
    return rc;
}

void HostRamVmm::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectHostRamVmm();
}

void HostRamVmm::collect()
{
    CollectorGuest *provider = mCollectorGuestManager->getVMMStatsProvider();
    if (provider)
    {
        LogAleksey(("{%p} " LOG_FN_FMT ": provider=%p enabled=%s valid=%s...\n",
                    this, __PRETTY_FUNCTION__, provider, provider->isEnabled()?"y":"n",
                    provider->isValid(GUESTSTATS_VMMRAM)?"y":"n"));
        if (provider->isValid(GUESTSTATS_VMMRAM))
        {
            /* Provider is ready, get updated stats */
            mAllocCurrent     = provider->getAllocVMM();
            mFreeCurrent      = provider->getFreeVMM();
            mBalloonedCurrent = provider->getBalloonedVMM();
            mSharedCurrent    = provider->getSharedVMM();
            provider->invalidate(GUESTSTATS_VMMRAM);
        }
        /*
         * Note that if there are no new values from the provider we will use
         * the ones most recently provided instead of zeros, which is probably
         * a desirable behavior.
         */
    }
    else
    {
        mAllocCurrent     = 0;
        mFreeCurrent      = 0;
        mBalloonedCurrent = 0;
        mSharedCurrent    = 0;
    }
    LogAleksey(("{%p} " LOG_FN_FMT ": mAllocCurrent=%u mFreeCurrent=%u mBalloonedCurrent=%u mSharedCurrent=%u\n",
                this, __PRETTY_FUNCTION__,
                mAllocCurrent, mFreeCurrent, mBalloonedCurrent, mSharedCurrent));
    mAllocVMM->put(mAllocCurrent);
    mFreeVMM->put(mFreeCurrent);
    mBalloonVMM->put(mBalloonedCurrent);
    mSharedVMM->put(mSharedCurrent);
}
#endif /* !VBOX_COLLECTOR_TEST_CASE */



void MachineCpuLoad::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
}

void MachineCpuLoad::collect()
{
    ULONG user, kernel;
    int rc = mHAL->getProcessCpuLoad(mProcess, &user, &kernel);
    if (RT_SUCCESS(rc))
    {
        mUser->put(user);
        mKernel->put(kernel);
    }
}

void MachineCpuLoadRaw::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectProcessCpuLoad(mProcess);
}

void MachineCpuLoadRaw::collect()
{
    uint64_t processUser, processKernel, hostTotal;

    int rc = mHAL->getRawProcessCpuLoad(mProcess, &processUser, &processKernel, &hostTotal);
    if (RT_SUCCESS(rc))
    {
        if (hostTotal == mHostTotalPrev)
        {
            /* Nearly impossible, but... */
            mUser->put(0);
            mKernel->put(0);
        }
        else
        {
            mUser->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * (processUser - mProcessUserPrev) / (hostTotal - mHostTotalPrev)));
            mKernel->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * (processKernel - mProcessKernelPrev ) / (hostTotal - mHostTotalPrev)));
        }

        mHostTotalPrev     = hostTotal;
        mProcessUserPrev   = processUser;
        mProcessKernelPrev = processKernel;
    }
}

void MachineRamUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUsed->init(mLength);
}

void MachineRamUsage::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectProcessRamUsage(mProcess);
}

void MachineRamUsage::collect()
{
    ULONG used;
    int rc = mHAL->getProcessMemoryUsage(mProcess, &used);
    if (RT_SUCCESS(rc))
        mUsed->put(used);
}


#ifndef VBOX_COLLECTOR_TEST_CASE
void GuestCpuLoad::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;

    mUser->init(mLength);
    mKernel->init(mLength);
    mIdle->init(mLength);
}

void GuestCpuLoad::preCollect(CollectorHints& hints, uint64_t /* iTick */)
{
    hints.collectGuestStats(mCGuest->getProcess());
}

void GuestCpuLoad::collect()
{
    if (mCGuest->isValid(GUESTSTATS_CPULOAD))
    {
        mUser->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * mCGuest->getCpuUser()) / 100);
        mKernel->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * mCGuest->getCpuKernel()) / 100);
        mIdle->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * mCGuest->getCpuIdle()) / 100);
        mCGuest->invalidate(GUESTSTATS_CPULOAD);
    }
}

int GuestCpuLoad::enable()
{
    int rc = mCGuest->enable(GUESTSTATS_CPULOAD);
    BaseMetric::enable();
    return rc;
}

int GuestCpuLoad::disable()
{
    BaseMetric::disable();
    return mCGuest->disable(GUESTSTATS_CPULOAD);
}

void GuestRamUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;

    mTotal->init(mLength);
    mFree->init(mLength);
    mBallooned->init(mLength);
    mShared->init(mLength);
    mCache->init(mLength);
    mPagedTotal->init(mLength);
}

void GuestRamUsage::collect()
{
    if (mCGuest->isValid(GUESTSTATS_RAMUSAGE))
    {
        mTotal->put(mCGuest->getMemTotal());
        mFree->put(mCGuest->getMemFree());
        mBallooned->put(mCGuest->getMemBalloon());
        mShared->put(mCGuest->getMemShared());
        mCache->put(mCGuest->getMemCache());
        mPagedTotal->put(mCGuest->getPageTotal());
        mCGuest->invalidate(GUESTSTATS_RAMUSAGE);
    }
}

int GuestRamUsage::enable()
{
    int rc = mCGuest->enable(GUESTSTATS_RAMUSAGE);
    BaseMetric::enable();
    return rc;
}

int GuestRamUsage::disable()
{
    BaseMetric::disable();
    return mCGuest->disable(GUESTSTATS_RAMUSAGE);
}

void GuestRamUsage::preCollect(CollectorHints& hints,  uint64_t /* iTick */)
{
    hints.collectGuestStats(mCGuest->getProcess());
}
#endif /* !VBOX_COLLECTOR_TEST_CASE */

void CircularBuffer::init(ULONG ulLength)
{
    if (mData)
        RTMemFree(mData);
    mLength = ulLength;
    if (mLength)
        mData = (ULONG*)RTMemAllocZ(ulLength * sizeof(ULONG));
    else
        mData = NULL;
    mWrapped = false;
    mEnd = 0;
    mSequenceNumber = 0;
}

ULONG CircularBuffer::length()
{
    return mWrapped ? mLength : mEnd;
}

void CircularBuffer::put(ULONG value)
{
    if (mData)
    {
        mData[mEnd++] = value;
        if (mEnd >= mLength)
        {
            mEnd = 0;
            mWrapped = true;
        }
        ++mSequenceNumber;
    }
}

void CircularBuffer::copyTo(ULONG *data)
{
    if (mWrapped)
    {
        memcpy(data, mData + mEnd, (mLength - mEnd) * sizeof(ULONG));
        // Copy the wrapped part
        if (mEnd)
            memcpy(data + (mLength - mEnd), mData, mEnd * sizeof(ULONG));
    }
    else
        memcpy(data, mData, mEnd * sizeof(ULONG));
}

void SubMetric::query(ULONG *data)
{
    copyTo(data);
}

void Metric::query(ULONG **data, ULONG *count, ULONG *sequenceNumber)
{
    ULONG length;
    ULONG *tmpData;

    length = mSubMetric->length();
    *sequenceNumber = mSubMetric->getSequenceNumber() - length;
    if (length)
    {
        tmpData = (ULONG*)RTMemAlloc(sizeof(*tmpData)*length);
        mSubMetric->query(tmpData);
        if (mAggregate)
        {
            *count = 1;
            *data  = (ULONG*)RTMemAlloc(sizeof(**data));
            **data = mAggregate->compute(tmpData, length);
            RTMemFree(tmpData);
        }
        else
        {
            *count = length;
            *data  = tmpData;
        }
    }
    else
    {
        *count = 0;
        *data  = 0;
    }
}

ULONG AggregateAvg::compute(ULONG *data, ULONG length)
{
    uint64_t tmp = 0;
    for (ULONG i = 0; i < length; ++i)
        tmp += data[i];
    return (ULONG)(tmp / length);
}

const char * AggregateAvg::getName()
{
    return "avg";
}

ULONG AggregateMin::compute(ULONG *data, ULONG length)
{
    ULONG tmp = *data;
    for (ULONG i = 0; i < length; ++i)
        if (data[i] < tmp)
            tmp = data[i];
    return tmp;
}

const char * AggregateMin::getName()
{
    return "min";
}

ULONG AggregateMax::compute(ULONG *data, ULONG length)
{
    ULONG tmp = *data;
    for (ULONG i = 0; i < length; ++i)
        if (data[i] > tmp)
            tmp = data[i];
    return tmp;
}

const char * AggregateMax::getName()
{
    return "max";
}

Filter::Filter(ComSafeArrayIn(IN_BSTR, metricNames),
               ComSafeArrayIn(IUnknown *, objects))
{
    /*
     * Let's work around null/empty safe array mess. I am not sure there is
     * a way to pass null arrays via webservice, I haven't found one. So I
     * guess the users will be forced to use empty arrays instead. Constructing
     * an empty SafeArray is a bit awkward, so what we do in this method is
     * actually convert null arrays to empty arrays and pass them down to
     * init() method. If someone knows how to do it better, please be my guest,
     * fix it.
     */
    if (ComSafeArrayInIsNull(metricNames))
    {
        com::SafeArray<BSTR> nameArray;
        if (ComSafeArrayInIsNull(objects))
        {
            com::SafeIfaceArray<IUnknown> objectArray;
            objectArray.reset(0);
            init(ComSafeArrayAsInParam(nameArray),
                 ComSafeArrayAsInParam(objectArray));
        }
        else
        {
            com::SafeIfaceArray<IUnknown> objectArray(ComSafeArrayInArg(objects));
            init(ComSafeArrayAsInParam(nameArray),
                 ComSafeArrayAsInParam(objectArray));
        }
    }
    else
    {
        com::SafeArray<IN_BSTR> nameArray(ComSafeArrayInArg(metricNames));
        if (ComSafeArrayInIsNull(objects))
        {
            com::SafeIfaceArray<IUnknown> objectArray;
            objectArray.reset(0);
            init(ComSafeArrayAsInParam(nameArray),
                 ComSafeArrayAsInParam(objectArray));
        }
        else
        {
            com::SafeIfaceArray<IUnknown> objectArray(ComSafeArrayInArg(objects));
            init(ComSafeArrayAsInParam(nameArray),
                 ComSafeArrayAsInParam(objectArray));
        }
    }
}

Filter::Filter(const com::Utf8Str name, const ComPtr<IUnknown> &aObject)
{
    processMetricList(name, aObject);
}

void Filter::init(ComSafeArrayIn(IN_BSTR, metricNames),
                  ComSafeArrayIn(IUnknown *, objects))
{
    com::SafeArray<IN_BSTR> nameArray(ComSafeArrayInArg(metricNames));
    com::SafeIfaceArray<IUnknown> objectArray(ComSafeArrayInArg(objects));

    if (!objectArray.size())
    {
        if (nameArray.size())
        {
            for (size_t i = 0; i < nameArray.size(); ++i)
                processMetricList(com::Utf8Str(nameArray[i]), ComPtr<IUnknown>());
        }
        else
            processMetricList("*", ComPtr<IUnknown>());
    }
    else
    {
        for (size_t i = 0; i < objectArray.size(); ++i)
            switch (nameArray.size())
            {
                case 0:
                    processMetricList("*", objectArray[i]);
                    break;
                case 1:
                    processMetricList(com::Utf8Str(nameArray[0]), objectArray[i]);
                    break;
                default:
                    processMetricList(com::Utf8Str(nameArray[i]), objectArray[i]);
                    break;
            }
    }
}

void Filter::processMetricList(const com::Utf8Str &name, const ComPtr<IUnknown> object)
{
    size_t startPos = 0;

    for (size_t pos = name.find(",");
         pos != com::Utf8Str::npos;
         pos = name.find(",", startPos))
    {
        mElements.push_back(std::make_pair(object, RTCString(name.substr(startPos, pos - startPos).c_str())));
        startPos = pos + 1;
    }
    mElements.push_back(std::make_pair(object, RTCString(name.substr(startPos).c_str())));
}

/**
 * The following method was borrowed from stamR3Match (VMM/STAM.cpp) and
 * modified to handle the special case of trailing colon in the pattern.
 *
 * @returns True if matches, false if not.
 * @param   pszPat      Pattern.
 * @param   pszName     Name to match against the pattern.
 * @param   fSeenColon  Seen colon (':').
 */
bool Filter::patternMatch(const char *pszPat, const char *pszName,
                          bool fSeenColon)
{
    /* ASSUMES ASCII */
    for (;;)
    {
        char chPat = *pszPat;
        switch (chPat)
        {
            default:
                if (*pszName != chPat)
                    return false;
                break;

            case '*':
            {
                while ((chPat = *++pszPat) == '*' || chPat == '?')
                    /* nothing */;

                /* Handle a special case, the mask terminating with a colon. */
                if (chPat == ':')
                {
                    if (!fSeenColon && !pszPat[1])
                        return !strchr(pszName, ':');
                    fSeenColon = true;
                }

                for (;;)
                {
                    char ch = *pszName++;
                    if (    ch == chPat
                        &&  (   !chPat
                             || patternMatch(pszPat + 1, pszName, fSeenColon)))
                        return true;
                    if (!ch)
                        return false;
                }
                /* won't ever get here */
                break;
            }

            case '?':
                if (!*pszName)
                    return false;
                break;

            /* Handle a special case, the mask terminating with a colon. */
            case ':':
                if (!fSeenColon && !pszPat[1])
                    return !*pszName;
                if (*pszName != ':')
                    return false;
                fSeenColon = true;
                break;

            case '\0':
                return !*pszName;
        }
        pszName++;
        pszPat++;
    }
    return true;
}

bool Filter::match(const ComPtr<IUnknown> object, const RTCString &name) const
{
    ElementList::const_iterator it;

    //LogAleksey(("Filter::match(%p, %s)\n", static_cast<const IUnknown*> (object), name.c_str()));
    for (it = mElements.begin(); it != mElements.end(); it++)
    {
        //LogAleksey(("...matching against(%p, %s)\n", static_cast<const IUnknown*> ((*it).first), (*it).second.c_str()));
        if ((*it).first.isNull() || (*it).first == object)
        {
            // Objects match, compare names
            if (patternMatch((*it).second.c_str(), name.c_str()))
            {
                //LogFlowThisFunc(("...found!\n"));
                return true;
            }
        }
    }
    //LogAleksey(("...no matches!\n"));
    return false;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
