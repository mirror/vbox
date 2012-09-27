/* $Id$ */

/** @file
 *
 * VBox Performance API COM Classes implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
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
 * Rules of engagement:
 * 1) All performance objects must be destroyed by PerformanceCollector only!
 * 2) All public methods of PerformanceCollector must be protected with
 *    read or write lock.
 * 3) samplerCallback only uses the write lock during the third phase
 *    which pulls data into SubMetric objects. This is where object destruction
 *    and all list modifications are done. The pre-collection phases are
 *    run without any locks which is only possible because:
 * 4) Public methods of PerformanceCollector as well as pre-collection methods
      cannot modify lists or destroy objects, and:
 * 5) Pre-collection methods cannot modify metric data.
 */

#include "PerformanceImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/process.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <vector>
#include <algorithm>
#include <functional>

#include "Performance.h"

static const char *g_papcszMetricNames[] =
{
    "CPU/Load/User",
    "CPU/Load/User:avg",
    "CPU/Load/User:min",
    "CPU/Load/User:max",
    "CPU/Load/Kernel",
    "CPU/Load/Kernel:avg",
    "CPU/Load/Kernel:min",
    "CPU/Load/Kernel:max",
    "CPU/Load/Idle",
    "CPU/Load/Idle:avg",
    "CPU/Load/Idle:min",
    "CPU/Load/Idle:max",
    "CPU/MHz",
    "CPU/MHz:avg",
    "CPU/MHz:min",
    "CPU/MHz:max",
    "Net/*/Load/Rx",
    "Net/*/Load/Rx:avg",
    "Net/*/Load/Rx:min",
    "Net/*/Load/Rx:max",
    "Net/*/Load/Tx",
    "Net/*/Load/Tx:avg",
    "Net/*/Load/Tx:min",
    "Net/*/Load/Tx:max",
    "RAM/Usage/Total",
    "RAM/Usage/Total:avg",
    "RAM/Usage/Total:min",
    "RAM/Usage/Total:max",
    "RAM/Usage/Used",
    "RAM/Usage/Used:avg",
    "RAM/Usage/Used:min",
    "RAM/Usage/Used:max",
    "RAM/Usage/Free",
    "RAM/Usage/Free:avg",
    "RAM/Usage/Free:min",
    "RAM/Usage/Free:max",
    "RAM/VMM/Used",
    "RAM/VMM/Used:avg",
    "RAM/VMM/Used:min",
    "RAM/VMM/Used:max",
    "RAM/VMM/Free",
    "RAM/VMM/Free:avg",
    "RAM/VMM/Free:min",
    "RAM/VMM/Free:max",
    "RAM/VMM/Ballooned",
    "RAM/VMM/Ballooned:avg",
    "RAM/VMM/Ballooned:min",
    "RAM/VMM/Ballooned:max",
    "RAM/VMM/Shared",
    "RAM/VMM/Shared:avg",
    "RAM/VMM/Shared:min",
    "RAM/VMM/Shared:max",
    "Guest/CPU/Load/User",
    "Guest/CPU/Load/User:avg",
    "Guest/CPU/Load/User:min",
    "Guest/CPU/Load/User:max",
    "Guest/CPU/Load/Kernel",
    "Guest/CPU/Load/Kernel:avg",
    "Guest/CPU/Load/Kernel:min",
    "Guest/CPU/Load/Kernel:max",
    "Guest/CPU/Load/Idle",
    "Guest/CPU/Load/Idle:avg",
    "Guest/CPU/Load/Idle:min",
    "Guest/CPU/Load/Idle:max",
    "Guest/RAM/Usage/Total",
    "Guest/RAM/Usage/Total:avg",
    "Guest/RAM/Usage/Total:min",
    "Guest/RAM/Usage/Total:max",
    "Guest/RAM/Usage/Free",
    "Guest/RAM/Usage/Free:avg",
    "Guest/RAM/Usage/Free:min",
    "Guest/RAM/Usage/Free:max",
    "Guest/RAM/Usage/Balloon",
    "Guest/RAM/Usage/Balloon:avg",
    "Guest/RAM/Usage/Balloon:min",
    "Guest/RAM/Usage/Balloon:max",
    "Guest/RAM/Usage/Shared",
    "Guest/RAM/Usage/Shared:avg",
    "Guest/RAM/Usage/Shared:min",
    "Guest/RAM/Usage/Shared:max",
    "Guest/RAM/Usage/Cache",
    "Guest/RAM/Usage/Cache:avg",
    "Guest/RAM/Usage/Cache:min",
    "Guest/RAM/Usage/Cache:max",
    "Guest/Pagefile/Usage/Total",
    "Guest/Pagefile/Usage/Total:avg",
    "Guest/Pagefile/Usage/Total:min",
    "Guest/Pagefile/Usage/Total:max",
};

////////////////////////////////////////////////////////////////////////////////
// PerformanceCollector class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

PerformanceCollector::PerformanceCollector()
  : mMagic(0), mUnknownGuest("unknown guest")
{
}

PerformanceCollector::~PerformanceCollector() {}

HRESULT PerformanceCollector::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    return BaseFinalConstruct();
}

void PerformanceCollector::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the PerformanceCollector object.
 */
HRESULT PerformanceCollector::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();

    HRESULT rc = S_OK;

    m.hal = pm::createHAL();
    m.gm = new pm::CollectorGuestManager;

    /* Let the sampler know it gets a valid collector.  */
    mMagic = MAGIC;

    /* Start resource usage sampler */
    int vrc = RTTimerLRCreate (&m.sampler, VBOX_USAGE_SAMPLER_MIN_INTERVAL,
                               &PerformanceCollector::staticSamplerCallback, this);
    AssertMsgRC (vrc, ("Failed to create resource usage "
                       "sampling timer(%Rra)\n", vrc));
    if (RT_FAILURE(vrc))
        rc = E_FAIL;

    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();

    return rc;
}

/**
 * Uninitializes the PerformanceCollector object.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void PerformanceCollector::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc(("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    mMagic = 0;

    /* Destroy unregistered metrics */
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end();)
        if ((*it)->isUnregistered())
        {
            delete *it;
            it = m.baseMetrics.erase(it);
        }
        else
            ++it;
    Assert(m.baseMetrics.size() == 0);
    /*
     * Now when we have destroyed all base metrics that could
     * try to pull data from unregistered CollectorGuest objects
     * it is safe to destroy them as well.
     */
    m.gm->destroyUnregistered();

    /* Destroy resource usage sampler */
    int vrc = RTTimerLRDestroy (m.sampler);
    AssertMsgRC (vrc, ("Failed to destroy resource usage "
                       "sampling timer (%Rra)\n", vrc));
    m.sampler = NULL;

    //delete m.factory;
    //m.factory = NULL;

    delete m.gm;
    m.gm = NULL;
    delete m.hal;
    m.hal = NULL;

    LogFlowThisFuncLeave();
}

// IPerformanceCollector properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP PerformanceCollector::COMGETTER(MetricNames)(ComSafeArrayOut(BSTR, theMetricNames))
{
    if (ComSafeArrayOutIsNull(theMetricNames))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> metricNames(RT_ELEMENTS(g_papcszMetricNames));
    for (size_t i = 0; i < RT_ELEMENTS(g_papcszMetricNames); i++)
    {
        Bstr tmp(g_papcszMetricNames[i]); /* gcc-3.3 cruft */
        tmp.cloneTo(&metricNames[i]);
    }
    //gMetricNames.detachTo(ComSafeArrayOutArg(theMetricNames));
    metricNames.detachTo(ComSafeArrayOutArg(theMetricNames));

    return S_OK;
}

// IPerformanceCollector methods
////////////////////////////////////////////////////////////////////////////////

HRESULT PerformanceCollector::toIPerformanceMetric(pm::Metric *src, IPerformanceMetric **dst)
{
    ComObjPtr<PerformanceMetric> metric;
    HRESULT rc = metric.createObject();
    if (SUCCEEDED(rc))
        rc = metric->init (src);
    AssertComRCReturnRC(rc);
    metric.queryInterfaceTo(dst);
    return rc;
}

HRESULT PerformanceCollector::toIPerformanceMetric(pm::BaseMetric *src, IPerformanceMetric **dst)
{
    ComObjPtr<PerformanceMetric> metric;
    HRESULT rc = metric.createObject();
    if (SUCCEEDED(rc))
        rc = metric->init (src);
    AssertComRCReturnRC(rc);
    metric.queryInterfaceTo(dst);
    return rc;
}

const Utf8Str& PerformanceCollector::getFailedGuestName()
{
    pm::CollectorGuest *pGuest = m.gm->getBlockedGuest();
    if (pGuest)
        return pGuest->getVMName();
    return mUnknownGuest;
}

STDMETHODIMP PerformanceCollector::GetMetrics(ComSafeArrayIn(IN_BSTR, metricNames),
                                              ComSafeArrayIn(IUnknown *, objects),
                                              ComSafeArrayOut(IPerformanceMetric *, outMetrics))
{
    LogFlowThisFuncEnter();
    //LogFlowThisFunc(("mState=%d, mType=%d\n", mState, mType));

    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    MetricList filteredMetrics;
    MetricList::iterator it;
    for (it = m.metrics.begin(); it != m.metrics.end(); ++it)
        if (filter.match ((*it)->getObject(), (*it)->getName()))
            filteredMetrics.push_back (*it);

    com::SafeIfaceArray<IPerformanceMetric> retMetrics (filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin(); it != filteredMetrics.end(); ++it)
    {
        ComObjPtr<PerformanceMetric> metric;
        rc = metric.createObject();
        if (SUCCEEDED(rc))
            rc = metric->init (*it);
        AssertComRCReturnRC(rc);
        LogFlow (("PerformanceCollector::GetMetrics() store a metric at "
                  "retMetrics[%d]...\n", i));
        metric.queryInterfaceTo(&retMetrics[i++]);
    }
    retMetrics.detachTo(ComSafeArrayOutArg(outMetrics));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP PerformanceCollector::SetupMetrics(ComSafeArrayIn(IN_BSTR, metricNames),
                                                ComSafeArrayIn(IUnknown *, objects),
                                                ULONG aPeriod,
                                                ULONG aCount,
                                                ComSafeArrayOut(IPerformanceMetric *, outMetrics))
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    pm::Filter filter(ComSafeArrayInArg (metricNames),
                      ComSafeArrayInArg (objects));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    BaseMetricList filteredMetrics;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if (filter.match((*it)->getObject(), (*it)->getName()))
        {
            LogFlow (("PerformanceCollector::SetupMetrics() setting period to %u,"
                      " count to %u for %s\n", aPeriod, aCount, (*it)->getName()));
            (*it)->init(aPeriod, aCount);
            if (aPeriod == 0 || aCount == 0)
            {
                LogFlow (("PerformanceCollector::SetupMetrics() disabling %s\n",
                          (*it)->getName()));
                rc = (*it)->disable();
                if (FAILED(rc))
                    break;
            }
            else
            {
                LogFlow (("PerformanceCollector::SetupMetrics() enabling %s\n",
                          (*it)->getName()));
                rc = (*it)->enable();
                if (FAILED(rc))
                    break;
            }
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics(filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED(rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics[i++]);
    retMetrics.detachTo(ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();

    if (FAILED(rc))
        return setError(E_FAIL, "Failed to setup metrics for '%s'",
                        getFailedGuestName().c_str());
    return rc;
}

STDMETHODIMP PerformanceCollector::EnableMetrics(ComSafeArrayIn(IN_BSTR, metricNames),
                                                 ComSafeArrayIn(IUnknown *, objects),
                                                 ComSafeArrayOut(IPerformanceMetric *, outMetrics))
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    pm::Filter filter(ComSafeArrayInArg(metricNames),
                      ComSafeArrayInArg(objects));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); /* Write lock is not needed atm since we are */
                                /* fiddling with enable bit only, but we */
                                /* care for those who come next :-). */

    HRESULT rc = S_OK;
    BaseMetricList filteredMetrics;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if (filter.match((*it)->getObject(), (*it)->getName()))
        {
            rc = (*it)->enable();
            if (FAILED(rc))
                break;
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics(filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED(rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics[i++]);
    retMetrics.detachTo(ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();

    if (FAILED(rc))
        return setError(E_FAIL, "Failed to enable metrics for '%s'",
                        getFailedGuestName().c_str());
    return rc;
}

STDMETHODIMP PerformanceCollector::DisableMetrics(ComSafeArrayIn(IN_BSTR, metricNames),
                                                  ComSafeArrayIn(IUnknown *, objects),
                                                  ComSafeArrayOut(IPerformanceMetric *, outMetrics))
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    pm::Filter filter(ComSafeArrayInArg(metricNames),
                      ComSafeArrayInArg(objects));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); /* Write lock is not needed atm since we are */
                                /* fiddling with enable bit only, but we */
                                /* care for those who come next :-). */

    HRESULT rc = S_OK;
    BaseMetricList filteredMetrics;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if (filter.match((*it)->getObject(), (*it)->getName()))
        {
            rc = (*it)->disable();
            if (FAILED(rc))
                break;
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics(filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED(rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics[i++]);
    retMetrics.detachTo(ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();

    if (FAILED(rc))
        return setError(E_FAIL, "Failed to disable metrics for '%s'",
                        getFailedGuestName().c_str());
    return rc;
}

STDMETHODIMP PerformanceCollector::QueryMetricsData(ComSafeArrayIn (IN_BSTR, metricNames),
                                                    ComSafeArrayIn (IUnknown *, objects),
                                                    ComSafeArrayOut(BSTR, outMetricNames),
                                                    ComSafeArrayOut(IUnknown *, outObjects),
                                                    ComSafeArrayOut(BSTR, outUnits),
                                                    ComSafeArrayOut(ULONG, outScales),
                                                    ComSafeArrayOut(ULONG, outSequenceNumbers),
                                                    ComSafeArrayOut(ULONG, outDataIndices),
                                                    ComSafeArrayOut(ULONG, outDataLengths),
                                                    ComSafeArrayOut(LONG, outData))
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    pm::Filter filter(ComSafeArrayInArg(metricNames),
                      ComSafeArrayInArg(objects));

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Let's compute the size of the resulting flat array */
    size_t flatSize = 0;
    MetricList filteredMetrics;
    MetricList::iterator it;
    for (it = m.metrics.begin(); it != m.metrics.end(); ++it)
        if (filter.match ((*it)->getObject(), (*it)->getName()))
        {
            filteredMetrics.push_back (*it);
            flatSize += (*it)->getLength();
        }

    int i = 0;
    size_t flatIndex = 0;
    size_t numberOfMetrics = filteredMetrics.size();
    com::SafeArray<BSTR> retNames(numberOfMetrics);
    com::SafeIfaceArray<IUnknown> retObjects(numberOfMetrics);
    com::SafeArray<BSTR> retUnits(numberOfMetrics);
    com::SafeArray<ULONG> retScales(numberOfMetrics);
    com::SafeArray<ULONG> retSequenceNumbers(numberOfMetrics);
    com::SafeArray<ULONG> retIndices(numberOfMetrics);
    com::SafeArray<ULONG> retLengths(numberOfMetrics);
    com::SafeArray<LONG> retData(flatSize);

    for (it = filteredMetrics.begin(); it != filteredMetrics.end(); ++it, ++i)
    {
        ULONG *values, length, sequenceNumber;
        /* @todo We may want to revise the query method to get rid of excessive alloc/memcpy calls. */
        (*it)->query(&values, &length, &sequenceNumber);
        LogFlow (("PerformanceCollector::QueryMetricsData() querying metric %s "
                  "returned %d values.\n", (*it)->getName(), length));
        memcpy(retData.raw() + flatIndex, values, length * sizeof(*values));
        RTMemFree(values);
        Bstr tmp((*it)->getName());
        tmp.detachTo(&retNames[i]);
        (*it)->getObject().queryInterfaceTo(&retObjects[i]);
        tmp = (*it)->getUnit();
        tmp.detachTo(&retUnits[i]);
        retScales[i] = (*it)->getScale();
        retSequenceNumbers[i] = sequenceNumber;
        retLengths[i] = length;
        retIndices[i] = (ULONG)flatIndex;
        flatIndex += length;
    }

    retNames.detachTo(ComSafeArrayOutArg(outMetricNames));
    retObjects.detachTo(ComSafeArrayOutArg(outObjects));
    retUnits.detachTo(ComSafeArrayOutArg(outUnits));
    retScales.detachTo(ComSafeArrayOutArg(outScales));
    retSequenceNumbers.detachTo(ComSafeArrayOutArg(outSequenceNumbers));
    retIndices.detachTo(ComSafeArrayOutArg(outDataIndices));
    retLengths.detachTo(ComSafeArrayOutArg(outDataLengths));
    retData.detachTo(ComSafeArrayOutArg(outData));
    return S_OK;
}

// public methods for internal purposes
///////////////////////////////////////////////////////////////////////////////

void PerformanceCollector::registerBaseMetric(pm::BaseMetric *baseMetric)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p name=%s\n", this, __PRETTY_FUNCTION__, (void *)baseMetric->getObject(), baseMetric->getName()));
    m.baseMetrics.push_back (baseMetric);
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::registerMetric(pm::Metric *metric)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p name=%s\n", this, __PRETTY_FUNCTION__, (void *)metric->getObject(), metric->getName()));
    m.metrics.push_back (metric);
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::unregisterBaseMetricsFor(const ComPtr<IUnknown> &aObject)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    int n = 0;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if ((*it)->associatedWith(aObject))
        {
            (*it)->unregister();
            ++n;
        }
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p, marked %d metrics\n",
                this, __PRETTY_FUNCTION__, (void *)aObject, n));
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::unregisterMetricsFor(const ComPtr<IUnknown> &aObject)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p\n", this, __PRETTY_FUNCTION__, (void *)aObject));
    MetricList::iterator it;
    for (it = m.metrics.begin(); it != m.metrics.end();)
        if ((*it)->associatedWith(aObject))
        {
            delete *it;
            it = m.metrics.erase(it);
        }
        else
            ++it;
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::registerGuest(pm::CollectorGuest* pGuest)
{
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.gm->registerGuest(pGuest);
}

void PerformanceCollector::unregisterGuest(pm::CollectorGuest* pGuest)
{
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.gm->unregisterGuest(pGuest);
}

void PerformanceCollector::suspendSampling()
{
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    int rc = RTTimerLRStop(m.sampler);
    AssertRC(rc);
}

void PerformanceCollector::resumeSampling()
{
    AutoCaller autoCaller(this);
    if (!SUCCEEDED(autoCaller.rc())) return;

    int rc = RTTimerLRStart(m.sampler, 0);
    AssertRC(rc);
}


// private methods
///////////////////////////////////////////////////////////////////////////////

/* static */
void PerformanceCollector::staticSamplerCallback(RTTIMERLR hTimerLR, void *pvUser,
                                                 uint64_t iTick)
{
    AssertReturnVoid (pvUser != NULL);
    PerformanceCollector *collector = static_cast <PerformanceCollector *> (pvUser);
    Assert(collector->mMagic == MAGIC);
    if (collector->mMagic == MAGIC)
        collector->samplerCallback(iTick);

    NOREF (hTimerLR);
}

/*
 * Metrics collection is a three stage process:
 * 1) Pre-collection (hinting)
 *    At this stage we compose the list of all metrics to be collected
 *    If any metrics cannot be collected separately or if it is more
 *    efficient to collect several metric at once, these metrics should
 *    use hints to mark that they will need to be collected.
 * 2) Pre-collection (bulk)
 *    Using hints set at stage 1 platform-specific HAL
 *    instance collects all marked host-related metrics.
 *    Hinted guest-related metrics then get collected by CollectorGuestManager.
 * 3) Collection
 *    Metrics that are collected individually get collected and stored. Values
 *    saved in HAL and CollectorGuestManager are extracted and stored to
 *    individual metrics.
 */
void PerformanceCollector::samplerCallback(uint64_t iTick)
{
    Log4(("{%p} " LOG_FN_FMT ": ENTER\n", this, __PRETTY_FUNCTION__));
    /* No locking until stage 3!*/

    pm::CollectorHints hints;
    uint64_t timestamp = RTTimeMilliTS();
    BaseMetricList toBeCollected;
    BaseMetricList::iterator it;
    /* Compose the list of metrics being collected at this moment */
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); it++)
        if ((*it)->collectorBeat(timestamp))
        {
            (*it)->preCollect(hints, iTick);
            toBeCollected.push_back(*it);
        }

    if (toBeCollected.size() == 0)
    {
        Log4(("{%p} " LOG_FN_FMT ": LEAVE (nothing to collect)\n", this, __PRETTY_FUNCTION__));
        return;
    }

    /* Let know the platform specific code what is being collected */
    m.hal->preCollect(hints, iTick);
#if 0
    /* Guest stats are now pushed by guests themselves */
    /* Collect the data in bulk from all hinted guests */
    m.gm->preCollect(hints, iTick);
#endif

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    /*
     * Before we can collect data we need to go through both lists
     * again to see if any base metrics are marked as unregistered.
     * Those should be destroyed now.
     */
    LogAleksey(("{%p} " LOG_FN_FMT ": before remove_if: toBeCollected.size()=%d\n", this, __PRETTY_FUNCTION__, toBeCollected.size()));
    toBeCollected.remove_if(std::mem_fun(&pm::BaseMetric::isUnregistered));
    LogAleksey(("{%p} " LOG_FN_FMT ": after remove_if: toBeCollected.size()=%d\n", this, __PRETTY_FUNCTION__, toBeCollected.size()));
    LogAleksey(("{%p} " LOG_FN_FMT ": before remove_if: m.baseMetrics.size()=%d\n", this, __PRETTY_FUNCTION__, m.baseMetrics.size()));
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end();)
        if ((*it)->isUnregistered())
        {
            delete *it;
            it = m.baseMetrics.erase(it);
        }
        else
            ++it;
    LogAleksey(("{%p} " LOG_FN_FMT ": after remove_if: m.baseMetrics.size()=%d\n", this, __PRETTY_FUNCTION__, m.baseMetrics.size()));
    /*
     * Now when we have destroyed all base metrics that could
     * try to pull data from unregistered CollectorGuest objects
     * it is safe to destroy them as well.
     */
    m.gm->destroyUnregistered();

    /* Finally, collect the data */
    std::for_each (toBeCollected.begin(), toBeCollected.end(),
                   std::mem_fun (&pm::BaseMetric::collect));
    Log4(("{%p} " LOG_FN_FMT ": LEAVE\n", this, __PRETTY_FUNCTION__));
}

////////////////////////////////////////////////////////////////////////////////
// PerformanceMetric class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

PerformanceMetric::PerformanceMetric()
{
}

PerformanceMetric::~PerformanceMetric()
{
}

HRESULT PerformanceMetric::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    return BaseFinalConstruct();
}

void PerformanceMetric::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit ();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

HRESULT PerformanceMetric::init(pm::Metric *aMetric)
{
    m.name        = aMetric->getName();
    m.object      = aMetric->getObject();
    m.description = aMetric->getDescription();
    m.period      = aMetric->getPeriod();
    m.count       = aMetric->getLength();
    m.unit        = aMetric->getUnit();
    m.min         = aMetric->getMinValue();
    m.max         = aMetric->getMaxValue();
    return S_OK;
}

HRESULT PerformanceMetric::init(pm::BaseMetric *aMetric)
{
    m.name        = aMetric->getName();
    m.object      = aMetric->getObject();
    m.description = "";
    m.period      = aMetric->getPeriod();
    m.count       = aMetric->getLength();
    m.unit        = aMetric->getUnit();
    m.min         = aMetric->getMinValue();
    m.max         = aMetric->getMaxValue();
    return S_OK;
}

void PerformanceMetric::uninit()
{
}

STDMETHODIMP PerformanceMetric::COMGETTER(MetricName)(BSTR *aMetricName)
{
    /// @todo (r=dmik) why do all these getters not do AutoCaller and
    /// AutoReadLock? Is the underlying metric a constant object?

    m.name.cloneTo(aMetricName);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Object)(IUnknown **anObject)
{
    m.object.queryInterfaceTo(anObject);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Description)(BSTR *aDescription)
{
    m.description.cloneTo(aDescription);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Period)(ULONG *aPeriod)
{
    *aPeriod = m.period;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Count)(ULONG *aCount)
{
    *aCount = m.count;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Unit)(BSTR *aUnit)
{
    m.unit.cloneTo(aUnit);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MinimumValue)(LONG *aMinValue)
{
    *aMinValue = m.min;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MaximumValue)(LONG *aMaxValue)
{
    *aMaxValue = m.max;
    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
