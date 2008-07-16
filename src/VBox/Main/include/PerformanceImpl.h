/* $Id$ */

/** @file
 *
 * VBox Performance COM Classes declaration.
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

#ifndef ____H_PERFORMANCEIMPL
#define ____H_PERFORMANCEIMPL

#include "VirtualBoxBase.h"

#include <VBox/com/com.h>
#include <VBox/com/array.h>
//#ifdef VBOX_WITH_RESOURCE_USAGE_API
//#include <iprt/system.h>
#include <iprt/timer.h>
//#endif /* VBOX_WITH_RESOURCE_USAGE_API */

#include <list>
#include <set>

#include "Performance.h"

/* Each second we obtain new CPU load stats. */
#define VBOX_USAGE_SAMPLER_MIN_INTERVAL 1000

class Machine;
class HostUSBDevice;

class ATL_NO_VTABLE PerformanceMetric :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportTranslation <PerformanceMetric>,
    public IPerformanceMetric
{
private:

    struct Data
    {
        /* Constructor. */
        Data() { }

        bool operator== (const Data &that) const
        {
            return this == &that;
        }
    };

public:

    DECLARE_NOT_AGGREGATABLE (PerformanceMetric)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (PerformanceMetric)
        COM_INTERFACE_ENTRY (IPerformanceMetric)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (PerformanceMetric)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (pm::Metric *aMetric);
    void uninit();

    // IPerformanceMetric properties
    STDMETHOD(COMGETTER(MetricName)) (BSTR *aMetricName);
    STDMETHOD(COMGETTER(Object)) (IUnknown **anObject);
    STDMETHOD(COMGETTER(Period)) (unsigned long *aPeriod);
    STDMETHOD(COMGETTER(Count)) (unsigned long *aCount);
    STDMETHOD(COMGETTER(Unit)) (BSTR *aUnit);
    STDMETHOD(COMGETTER(MinValue)) (long *aMinValue);
    STDMETHOD(COMGETTER(MaxValue)) (long *aMaxValue);

    // IPerformanceMetric methods

    // public methods only for internal purposes

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

private:

    pm::Metric *mMetric;
};


#if 0
class ATL_NO_VTABLE PerformanceData :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportTranslation <PerformanceData>,
    public IPerformanceData
{
private:

    struct Data
    {
        /* Constructor. */
        Data() { }

        bool operator== (const Data &that) const
        {
            return this == &that;
        }
    };

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (PerformanceData)

    DECLARE_NOT_AGGREGATABLE (PerformanceData)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PerformanceData)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IPerformanceData)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (PerformanceData)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (cosnt BSTR aMetricName, IUnknown *anObject,
                  ULONG *data, ULONG aLength);
    void uninit();

    // IPerformanceData properties
    STDMETHOD(COMGETTER(MetricName)) (BSTR *aMetricName);
    STDMETHOD(COMGETTER(Object)) (IUnknown **anObject);
    STDMETHOD(COMGETTER(Values)) (ComSafeArrayOut (LONG, values));

    // IPerformanceData methods

    // public methods only for internal purposes

private:

    Bstr mMetricName;
    IUnknown *mObject;
    ULONG *mData;
    ULONG mLength;
};
#endif

class ATL_NO_VTABLE PerformanceCollector :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <PerformanceCollector, IPerformanceCollector>,
    public VirtualBoxSupportTranslation <PerformanceCollector>,
    public IPerformanceCollector
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (PerformanceCollector)

    DECLARE_NOT_AGGREGATABLE (PerformanceCollector)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PerformanceCollector)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IPerformanceCollector)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (PerformanceCollector)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers only for internal purposes
    HRESULT init();
    void uninit();

    // IPerformanceCollector properties
    STDMETHOD(COMGETTER(MetricNames)) (ComSafeArrayOut (BSTR, metricNames));

    // IPerformanceCollector methods
    STDMETHOD(GetMetrics) (ComSafeArrayIn (const BSTR, metricNames),
                           ComSafeArrayIn (IUnknown *, objects),
                           ComSafeArrayOut (IPerformanceMetric *, metrics));
    STDMETHOD(SetupMetrics) (ComSafeArrayIn (INPTR BSTR, metricNames),
                             ComSafeArrayIn (IUnknown *, objects),
                             ULONG aPeriod, ULONG aCount);
    STDMETHOD(EnableMetrics) (ComSafeArrayIn (const BSTR, metricNames),
                              ComSafeArrayIn (IUnknown *, objects));
    STDMETHOD(DisableMetrics) (ComSafeArrayIn (const BSTR, metricNames),
                               ComSafeArrayIn (IUnknown *, objects));
    STDMETHOD(QueryMetricsData) (ComSafeArrayIn (const BSTR, metricNames),
                                 ComSafeArrayIn (IUnknown *, objects),
                                 ComSafeArrayOut (BSTR, outMetricNames),
                                 ComSafeArrayOut (IUnknown *, outObjects),
                                 ComSafeArrayOut (ULONG, outDataIndices),
                                 ComSafeArrayOut (ULONG, outDataLengths),
                                 ComSafeArrayOut (LONG, outData));

    // public methods only for internal purposes

    void registerBaseMetric (pm::BaseMetric *baseMetric);
    void registerMetric (pm::Metric *metric);
    void unregisterBaseMetricsFor (const ComPtr <IUnknown> &object);
    void unregisterMetricsFor (const ComPtr <IUnknown> &object);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    //
    pm::MetricFactory *getMetricFactory() { return m.mFactory; };

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"PerformanceCollector"; }

private:

    static void staticSamplerCallback (PRTTIMER pTimer, void *pvUser, uint64_t iTick);
    void samplerCallback();

    typedef std::list<pm::Metric*> MetricList;
    typedef std::list<pm::BaseMetric*> BaseMetricList;

    enum
    {
        MAGIC = 0xABBA1972u
    };

    unsigned int mMagic;

    struct Data {
        Data() : mFactory(0) {};

        BaseMetricList     mBaseMetrics;
        MetricList         mMetrics;
        PRTTIMER           mSampler;
        pm::MetricFactory *mFactory;
    } m;
};

#endif //!____H_PERFORMANCEIMPL
