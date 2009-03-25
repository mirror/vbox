/* $Id$ */

/** @file
 *
 * VBox Performance API COM Classes implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "PerformanceImpl.h"

#include "Logging.h"

#include <iprt/process.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <vector>
#include <algorithm>
#include <functional>

#include "Performance.h"

static Bstr gMetricNames[] =
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
};

////////////////////////////////////////////////////////////////////////////////
// PerformanceCollector class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

PerformanceCollector::PerformanceCollector() : mMagic(0) {}

PerformanceCollector::~PerformanceCollector() {}

HRESULT PerformanceCollector::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    return S_OK;
}

void PerformanceCollector::FinalRelease()
{
    LogFlowThisFunc (("\n"));
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the PerformanceCollector object.
 */
HRESULT PerformanceCollector::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();

    HRESULT rc = S_OK;

    m.hal = pm::createHAL();

    /* Let the sampler know it gets a valid collector.  */
    mMagic = MAGIC;

    /* Start resource usage sampler */
    int vrc = RTTimerLRCreate (&m.sampler, VBOX_USAGE_SAMPLER_MIN_INTERVAL,
                               &PerformanceCollector::staticSamplerCallback, this);
    AssertMsgRC (vrc, ("Failed to create resource usage "
                       "sampling timer(%Rra)\n", vrc));
    if (RT_FAILURE (vrc))
        rc = E_FAIL;

    if (SUCCEEDED (rc))
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
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc (("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    mMagic = 0;

    /* Destroy resource usage sampler */
    int vrc = RTTimerLRDestroy (m.sampler);
    AssertMsgRC (vrc, ("Failed to destroy resource usage "
                       "sampling timer (%Rra)\n", vrc));
    m.sampler = NULL;

    //delete m.factory;
    //m.factory = NULL;

    delete m.hal;
    m.hal = NULL;

    LogFlowThisFuncLeave();
}

// IPerformanceCollector properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP
PerformanceCollector::COMGETTER(MetricNames) (ComSafeArrayOut (BSTR, theMetricNames))
{
    if (ComSafeArrayOutIsNull (theMetricNames))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    com::SafeArray <BSTR> metricNames(RT_ELEMENTS(gMetricNames));
    for (size_t i = 0; i < RT_ELEMENTS(gMetricNames); i++)
    {
        gMetricNames[i].detachTo(&metricNames[i]);
    }
    //gMetricNames.detachTo(ComSafeArrayOutArg (theMetricNames));
    metricNames.detachTo (ComSafeArrayOutArg (theMetricNames));

    return S_OK;
}

// IPerformanceCollector methods
////////////////////////////////////////////////////////////////////////////////

HRESULT PerformanceCollector::toIPerformanceMetric(pm::Metric *src, IPerformanceMetric **dst)
{
    ComObjPtr <PerformanceMetric> metric;
    HRESULT rc = metric.createObject();
    if (SUCCEEDED (rc))
        rc = metric->init (src);
    AssertComRCReturnRC (rc);
    metric.queryInterfaceTo (dst);
    return rc;
}

HRESULT PerformanceCollector::toIPerformanceMetric(pm::BaseMetric *src, IPerformanceMetric **dst)
{
    ComObjPtr <PerformanceMetric> metric;
    HRESULT rc = metric.createObject();
    if (SUCCEEDED (rc))
        rc = metric->init (src);
    AssertComRCReturnRC (rc);
    metric.queryInterfaceTo (dst);
    return rc;
}

STDMETHODIMP
PerformanceCollector::GetMetrics (ComSafeArrayIn (IN_BSTR, metricNames),
                                  ComSafeArrayIn (IUnknown *, objects),
                                  ComSafeArrayOut (IPerformanceMetric *, outMetrics))
{
    LogFlowThisFuncEnter();
    //LogFlowThisFunc (("mState=%d, mType=%d\n", mState, mType));

    HRESULT rc = S_OK;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoReadLock alock (this);

    MetricList filteredMetrics;
    MetricList::iterator it;
    for (it = m.metrics.begin(); it != m.metrics.end(); ++it)
        if (filter.match ((*it)->getObject(), (*it)->getName()))
            filteredMetrics.push_back (*it);

    com::SafeIfaceArray<IPerformanceMetric> retMetrics (filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin(); it != filteredMetrics.end(); ++it)
    {
        ComObjPtr <PerformanceMetric> metric;
        rc = metric.createObject();
        if (SUCCEEDED (rc))
            rc = metric->init (*it);
        AssertComRCReturnRC (rc);
        LogFlow (("PerformanceCollector::GetMetrics() store a metric at "
                  "retMetrics[%d]...\n", i));
        metric.queryInterfaceTo (&retMetrics [i++]);
    }
    retMetrics.detachTo (ComSafeArrayOutArg(outMetrics));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP
PerformanceCollector::SetupMetrics (ComSafeArrayIn (IN_BSTR, metricNames),
                                    ComSafeArrayIn (IUnknown *, objects),
                                    ULONG aPeriod, ULONG aCount,
                                    ComSafeArrayOut (IPerformanceMetric *,
                                                     outMetrics))
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoWriteLock alock (this);

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
                (*it)->disable();
            }
            else
            {
                LogFlow (("PerformanceCollector::SetupMetrics() enabling %s\n",
                          (*it)->getName()));
                (*it)->enable();
            }
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics (filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED (rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics [i++]);
    retMetrics.detachTo (ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP
PerformanceCollector::EnableMetrics (ComSafeArrayIn (IN_BSTR, metricNames),
                                     ComSafeArrayIn (IUnknown *, objects),
                                     ComSafeArrayOut (IPerformanceMetric *,
                                                      outMetrics))
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoWriteLock alock (this); /* Write lock is not needed atm since we are */
                                /* fiddling with enable bit only, but we */
                                /* care for those who come next :-). */

    HRESULT rc = S_OK;
    BaseMetricList filteredMetrics;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if (filter.match((*it)->getObject(), (*it)->getName()))
        {
            (*it)->enable();
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics (filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED (rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics [i++]);
    retMetrics.detachTo (ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP
PerformanceCollector::DisableMetrics (ComSafeArrayIn (IN_BSTR, metricNames),
                                      ComSafeArrayIn (IUnknown *, objects),
                                      ComSafeArrayOut (IPerformanceMetric *,
                                                       outMetrics))
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoWriteLock alock (this); /* Write lock is not needed atm since we are */
                                /* fiddling with enable bit only, but we */
                                /* care for those who come next :-). */

    HRESULT rc = S_OK;
    BaseMetricList filteredMetrics;
    BaseMetricList::iterator it;
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); ++it)
        if (filter.match((*it)->getObject(), (*it)->getName()))
        {
            (*it)->disable();
            filteredMetrics.push_back(*it);
        }

    com::SafeIfaceArray<IPerformanceMetric> retMetrics (filteredMetrics.size());
    int i = 0;
    for (it = filteredMetrics.begin();
         it != filteredMetrics.end() && SUCCEEDED (rc); ++it)
        rc = toIPerformanceMetric(*it, &retMetrics [i++]);
    retMetrics.detachTo (ComSafeArrayOutArg(outMetrics));

    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP
PerformanceCollector::QueryMetricsData (ComSafeArrayIn (IN_BSTR, metricNames),
                                        ComSafeArrayIn (IUnknown *, objects),
                                        ComSafeArrayOut (BSTR, outMetricNames),
                                        ComSafeArrayOut (IUnknown *, outObjects),
                                        ComSafeArrayOut (BSTR, outUnits),
                                        ComSafeArrayOut (ULONG, outScales),
                                        ComSafeArrayOut (ULONG, outSequenceNumbers),
                                        ComSafeArrayOut (ULONG, outDataIndices),
                                        ComSafeArrayOut (ULONG, outDataLengths),
                                        ComSafeArrayOut (LONG, outData))
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    pm::Filter filter (ComSafeArrayInArg (metricNames),
                       ComSafeArrayInArg (objects));

    AutoReadLock alock (this);

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
    com::SafeArray <BSTR> retNames (numberOfMetrics);
    com::SafeIfaceArray <IUnknown> retObjects (numberOfMetrics);
    com::SafeArray <BSTR> retUnits (numberOfMetrics);
    com::SafeArray <ULONG> retScales (numberOfMetrics);
    com::SafeArray <ULONG> retSequenceNumbers (numberOfMetrics);
    com::SafeArray <ULONG> retIndices (numberOfMetrics);
    com::SafeArray <ULONG> retLengths (numberOfMetrics);
    com::SafeArray <LONG> retData (flatSize);

    for (it = filteredMetrics.begin(); it != filteredMetrics.end(); ++it, ++i)
    {
        ULONG *values, length, sequenceNumber;
        /* @todo We may want to revise the query method to get rid of excessive alloc/memcpy calls. */
        (*it)->query(&values, &length, &sequenceNumber);
        LogFlow (("PerformanceCollector::QueryMetricsData() querying metric %s "
                  "returned %d values.\n", (*it)->getName(), length));
        memcpy(retData.raw() + flatIndex, values, length * sizeof(*values));
        Bstr tmp((*it)->getName());
        tmp.detachTo(&retNames[i]);
        (*it)->getObject().queryInterfaceTo (&retObjects[i]);
        tmp = (*it)->getUnit();
        tmp.detachTo(&retUnits[i]);
        retScales[i] = (*it)->getScale();
        retSequenceNumbers[i] = sequenceNumber;
        retLengths[i] = length;
        retIndices[i] = flatIndex;
        flatIndex += length;
    }

    retNames.detachTo (ComSafeArrayOutArg (outMetricNames));
    retObjects.detachTo (ComSafeArrayOutArg (outObjects));
    retUnits.detachTo (ComSafeArrayOutArg (outUnits));
    retScales.detachTo (ComSafeArrayOutArg (outScales));
    retSequenceNumbers.detachTo (ComSafeArrayOutArg (outSequenceNumbers));
    retIndices.detachTo (ComSafeArrayOutArg (outDataIndices));
    retLengths.detachTo (ComSafeArrayOutArg (outDataLengths));
    retData.detachTo (ComSafeArrayOutArg (outData));
    return S_OK;
}

// public methods for internal purposes
///////////////////////////////////////////////////////////////////////////////

void PerformanceCollector::registerBaseMetric (pm::BaseMetric *baseMetric)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller (this);
    if (!SUCCEEDED (autoCaller.rc())) return;

    AutoWriteLock alock (this);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p name=%s\n", this, __PRETTY_FUNCTION__, (void *)baseMetric->getObject(), baseMetric->getName()));
    m.baseMetrics.push_back (baseMetric);
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::registerMetric (pm::Metric *metric)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller (this);
    if (!SUCCEEDED (autoCaller.rc())) return;

    AutoWriteLock alock (this);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p name=%s\n", this, __PRETTY_FUNCTION__, (void *)metric->getObject(), metric->getName()));
    m.metrics.push_back (metric);
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::unregisterBaseMetricsFor (const ComPtr <IUnknown> &aObject)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller (this);
    if (!SUCCEEDED (autoCaller.rc())) return;

    AutoWriteLock alock (this);
    LogAleksey(("{%p} " LOG_FN_FMT ": before remove_if: m.baseMetrics.size()=%d\n", this, __PRETTY_FUNCTION__, m.baseMetrics.size()));
    BaseMetricList::iterator it = std::remove_if (
        m.baseMetrics.begin(), m.baseMetrics.end(), std::bind2nd (
            std::mem_fun (&pm::BaseMetric::associatedWith), aObject));
    m.baseMetrics.erase(it, m.baseMetrics.end());
    LogAleksey(("{%p} " LOG_FN_FMT ": after remove_if: m.baseMetrics.size()=%d\n", this, __PRETTY_FUNCTION__, m.baseMetrics.size()));
    //LogFlowThisFuncLeave();
}

void PerformanceCollector::unregisterMetricsFor (const ComPtr <IUnknown> &aObject)
{
    //LogFlowThisFuncEnter();
    AutoCaller autoCaller (this);
    if (!SUCCEEDED (autoCaller.rc())) return;

    AutoWriteLock alock (this);
    LogAleksey(("{%p} " LOG_FN_FMT ": obj=%p\n", this, __PRETTY_FUNCTION__, (void *)aObject));
    MetricList::iterator it = std::remove_if (
        m.metrics.begin(), m.metrics.end(), std::bind2nd (
            std::mem_fun (&pm::Metric::associatedWith), aObject));
    m.metrics.erase(it, m.metrics.end());
    //LogFlowThisFuncLeave();
}

// private methods
///////////////////////////////////////////////////////////////////////////////

/* static */
void PerformanceCollector::staticSamplerCallback (RTTIMERLR hTimerLR, void *pvUser,
                                                  uint64_t /* iTick */)
{
    AssertReturnVoid (pvUser != NULL);
    PerformanceCollector *collector = static_cast <PerformanceCollector *> (pvUser);
    Assert (collector->mMagic == MAGIC);
    if (collector->mMagic == MAGIC)
    {
        collector->samplerCallback();
    }
    NOREF (hTimerLR);
}

void PerformanceCollector::samplerCallback()
{
    Log4(("{%p} " LOG_FN_FMT ": ENTER\n", this, __PRETTY_FUNCTION__));
    AutoWriteLock alock (this);

    pm::CollectorHints hints;
    uint64_t timestamp = RTTimeMilliTS();
    BaseMetricList toBeCollected;
    BaseMetricList::iterator it;
    /* Compose the list of metrics being collected at this moment */
    for (it = m.baseMetrics.begin(); it != m.baseMetrics.end(); it++)
        if ((*it)->collectorBeat(timestamp))
        {
            (*it)->preCollect(hints);
            toBeCollected.push_back(*it);
        }

    if (toBeCollected.size() == 0)
        return;

    /* Let know the platform specific code what is being collected */
    m.hal->preCollect(hints);

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
    LogFlowThisFunc (("\n"));

    return S_OK;
}

void PerformanceMetric::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit ();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

HRESULT PerformanceMetric::init (pm::Metric *aMetric)
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

HRESULT PerformanceMetric::init (pm::BaseMetric *aMetric)
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

STDMETHODIMP PerformanceMetric::COMGETTER(MetricName) (BSTR *aMetricName)
{
    /// @todo (r=dmik) why do all these getters not do AutoCaller and
    /// AutoReadLock? Is the underlying metric a constant object?

    m.name.cloneTo (aMetricName);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Object) (IUnknown **anObject)
{
    m.object.queryInterfaceTo(anObject);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Description) (BSTR *aDescription)
{
    m.description.cloneTo (aDescription);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Period) (ULONG *aPeriod)
{
    *aPeriod = m.period;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Count) (ULONG *aCount)
{
    *aCount = m.count;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Unit) (BSTR *aUnit)
{
    m.unit.cloneTo(aUnit);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MinimumValue) (LONG *aMinValue)
{
    *aMinValue = m.min;
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MaximumValue) (LONG *aMaxValue)
{
    *aMaxValue = m.max;
    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
