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

#if defined(RT_OS_WINDOWS)
#elif defined(RT_OS_LINUX)
#endif

#include "PerformanceImpl.h"

#include "Logging.h"

#include <VBox/err.h>
#include <iprt/process.h>

#include <vector>
#include <algorithm>
#include <functional>

static Bstr gMetricNames[] =
{
    "CPU/User:avg",
    "CPU/User:min",
    "CPU/User:max",
    "CPU/Kernel:avg",
    "CPU/Kernel:min",
    "CPU/Kernel:max",
    "CPU/Idle:avg",
    "CPU/Idle:min",
    "CPU/Idle:max",
    "CPU/MHz:avg",
    "CPU/MHz:min",
    "CPU/MHz:max",
    "RAM/Total:avg",
    "RAM/Total:min",
    "RAM/Total:max",
    "RAM/Used:avg",
    "RAM/Used:min",
    "RAM/Used:max",
    "RAM/Free:avg",
    "RAM/Free:min",
    "RAM/Free:max",
};

////////////////////////////////////////////////////////////////////////////////
// PerformanceCollector class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

PerformanceCollector::PerformanceCollector() : mFactory(0) {}

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
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    LogFlowThisFuncEnter();

    HRESULT rc = S_OK;

    /* @todo Obviously other platforms must be added as well. */
    mFactory = new pm::MetricFactoryLinux();
    /* Start resource usage sampler */

    int vrc = RTTimerCreate (&mSampler, VBOX_USAGE_SAMPLER_MIN_INTERVAL,
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

    /* Destroy resource usage sampler */
    int vrc = RTTimerDestroy (mSampler);
    AssertMsgRC (vrc, ("Failed to destroy resource usage "
                       "sampling timer (%Rra)\n", vrc));
    mSampler = NULL;

    delete mFactory;
    mFactory = NULL;

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

STDMETHODIMP PerformanceCollector::GetMetrics(ComSafeArrayIn(const BSTR, metricNames),
                                              ComSafeArrayIn(IUnknown *, objects),
                                              ComSafeArrayOut(IPerformanceMetric *, metrics))
{
    //LogFlowThisFunc (("mState=%d, mType=%d\n", mState, mType));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    return E_NOTIMPL;
}

STDMETHODIMP PerformanceCollector::SetupMetrics (ComSafeArrayIn(const BSTR, metricNames),
                                                 ComSafeArrayIn(IUnknown *, objects),
                                                 ULONG aPeriod, ULONG aCount)
{
#if 0
    pm::Filter filter(ComSafeArrayInArg(metricNames), ComSafeArrayInArg(objects));

    std::list<pm::Metric*>::iterator it;
    for (it = mMetrics.begin(); it != mMetrics.end(); ++it)
        if (filter.match(*it))
            (*it)->init(aPeriod, aCount);
#endif
    return E_NOTIMPL;
}

STDMETHODIMP PerformanceCollector::EnableMetrics (ComSafeArrayIn(const BSTR, metricNames),
                                                  ComSafeArrayIn(IUnknown *, objects))
{
    return E_NOTIMPL;
}

STDMETHODIMP PerformanceCollector::DisableMetrics (ComSafeArrayIn(const BSTR, metricNames),
                                                   ComSafeArrayIn(IUnknown *, objects))
{
    return E_NOTIMPL;
}

STDMETHODIMP PerformanceCollector::QueryMetricsData (ComSafeArrayIn(const BSTR, metricNames),
                                                     ComSafeArrayIn(IUnknown *, objects),
                                                     ComSafeArrayOut(BSTR, outMetricNames),
                                                     ComSafeArrayOut(IUnknown *, outObjects),
                                                     ComSafeArrayOut(ULONG, outDataIndices),
                                                     ComSafeArrayOut(ULONG, outDataLengths),
                                                     ComSafeArrayOut(LONG, outData))
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /// @todo r=dmik don't we need to lock this for reading?

    int i;
    std::list<pm::Metric*>::const_iterator it;
    /* Let's compute the size of the resulting flat array */
    size_t flatSize = 0, numberOfMetrics = 0;
    for (it = mMetrics.begin(); it != mMetrics.end(); ++it)
    {
        /* @todo Filtering goes here! */
        flatSize += (*it)->getLength();
        ++numberOfMetrics;
    }
    size_t flatIndex = 0;
    com::SafeArray<BSTR> retNames(numberOfMetrics);
    com::SafeIfaceArray<IUnknown> retObjects(numberOfMetrics);
    com::SafeArray<ULONG> retIndices(numberOfMetrics);
    com::SafeArray<ULONG> retLengths(numberOfMetrics);
    com::SafeArray<LONG> retData(flatSize);
    for (it = mMetrics.begin(), i = 0; it != mMetrics.end(); ++it)
    {
        /* @todo Filtering goes here! */
        unsigned long *values, length;
        /* @todo We may want to revise the query method to get rid of excessive alloc/memcpy calls. */
        (*it)->query(&values, &length);
        memcpy(retData.raw() + flatIndex, values, length * sizeof(*values));
        Bstr tmp((*it)->getName());
        tmp.detachTo(&retNames[i]);
        retObjects[i] = (*it)->getObject();
        retLengths[i] = length;
        retIndices[i] = flatIndex;
        ++i;
        flatIndex += length;
    }
    retNames.detachTo(ComSafeArrayOutArg(outMetricNames));
    retObjects.detachTo(ComSafeArrayOutArg(outObjects));
    retIndices.detachTo(ComSafeArrayOutArg(outDataIndices));
    retLengths.detachTo(ComSafeArrayOutArg(outDataLengths));
    retData.detachTo(ComSafeArrayOutArg(outData));
    return S_OK;
}

// public methods for internal purposes
///////////////////////////////////////////////////////////////////////////////

void PerformanceCollector::registerBaseMetric (pm::BaseMetric *baseMetric)
{
    mBaseMetrics.push_back (baseMetric);
}

void PerformanceCollector::registerMetric (pm::Metric *metric)
{
    mMetrics.push_back (metric);
}

void PerformanceCollector::unregisterBaseMetricsFor (const ComPtr <IUnknown> &aObject)
{
    std::remove_if (mBaseMetrics.begin(), mBaseMetrics.end(),
                    std::bind2nd (std::mem_fun (&pm::BaseMetric::associatedWith),
                                  aObject));
}

void PerformanceCollector::unregisterMetricsFor (const ComPtr <IUnknown> &aObject)
{
    std::remove_if (mMetrics.begin(), mMetrics.end(),
                    std::bind2nd (std::mem_fun (&pm::Metric::associatedWith),
                                  aObject));
}

// private methods
///////////////////////////////////////////////////////////////////////////////

/* static */
void PerformanceCollector::staticSamplerCallback (PRTTIMER pTimer, void *pvUser,
                                                  uint64_t iTick)
{
    AssertReturnVoid (pvUser != NULL);
    static_cast <PerformanceCollector *> (pvUser)->samplerCallback();
}

void PerformanceCollector::samplerCallback()
{
}

#if 0
PerformanceData::PerformanceData()
{
}

PerformanceData::~PerformanceData()
{
}

HRESULT PerformanceData::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    return S_OK;
}

void PerformanceData::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit ();
}

HRESULT PerformanceData::init (const char *aMetricName, IUnknown *anObject,
                               unsigned long *data, unsigned long aLength)
{
    mMetricName = aMetricName;
    mObject = anObject;
    mData = data;
    mLength = aLength;
    return S_OK;
}

void PerformanceData::uninit()
{
    RTMemFree(mData);
    mData = 0;
    mLength = 0;
}

STDMETHODIMP PerformanceData::COMGETTER(MetricName) (BSTR *aMetricName)
{
    Bstr tmp(mMetricName);
    tmp.detachTo(aMetricName);
    return S_OK;
}

STDMETHODIMP PerformanceData::COMGETTER(Object) (IUnknown **anObject)
{
    *anObject = mObject;
    return S_OK;
}

STDMETHODIMP PerformanceData::COMGETTER(Values) (ComSafeArrayOut (LONG, values))
{
    SafeArray <LONG> ret (mLength);
    for (size_t i = 0; i < mLength; ++ i)
        ret[i] = mData[i];

    ret.detachTo(ComSafeArrayOutArg(values));
    return S_OK;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// PerformanceMetric class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

PerformanceMetric::PerformanceMetric()
{
    mMetric = 0;
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
    mMetric = aMetric;
    return S_OK;
}

void PerformanceMetric::uninit()
{
}

STDMETHODIMP PerformanceMetric::COMGETTER(MetricName) (BSTR *aMetricName)
{
    Bstr tmp (mMetric->getName());
    tmp.detachTo (aMetricName);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Object) (IUnknown **anObject)
{
    *anObject = mMetric->getObject();
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Period) (unsigned long *aPeriod)
{
    *aPeriod = mMetric->getPeriod();
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Count) (unsigned long *aCount)
{
    *aCount = mMetric->getLength();
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(Unit) (BSTR *aUnit)
{
    Bstr tmp(mMetric->getUnit());
    tmp.detachTo(aUnit);
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MinValue) (long *aMinValue)
{
    *aMinValue = mMetric->getMinValue();
    return S_OK;
}

STDMETHODIMP PerformanceMetric::COMGETTER(MaxValue) (long *aMaxValue)
{
    *aMaxValue = mMetric->getMaxValue();
    return S_OK;
}

