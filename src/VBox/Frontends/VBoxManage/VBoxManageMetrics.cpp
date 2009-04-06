/* $Id$ */
/** @file
 * VBoxManage - The 'metrics' command.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/asm.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static char *toBaseMetricNames(const char *metricList)
{
    char *newList = (char*)RTMemAlloc(strlen(metricList) + 1);
    int cSlashes = 0;
    bool fSkip = false;
    const char *src = metricList;
    char c, *dst = newList;
    while ((c = *src++))
        if (c == ':')
            fSkip = true;
        else if (c == '/' && ++cSlashes == 2)
            fSkip = true;
        else if (c == ',')
        {
            fSkip = false;
            cSlashes = 0;
            *dst++ = c;
        }
        else
            if (!fSkip)
                *dst++ = c;
    *dst = 0;
    return newList;
}

static int parseFilterParameters(int argc, char *argv[],
                                 ComPtr<IVirtualBox> aVirtualBox,
                                 ComSafeArrayOut(BSTR, outMetrics),
                                 ComSafeArrayOut(BSTR, outBaseMetrics),
                                 ComSafeArrayOut(IUnknown *, outObjects))
{
    HRESULT rc = S_OK;
    com::SafeArray<BSTR> retMetrics(1);
    com::SafeArray<BSTR> retBaseMetrics(1);
    com::SafeIfaceArray <IUnknown> retObjects;

    Bstr metricNames, baseNames;

    /* Metric list */
    if (argc > 1)
    {
        metricNames = argv[1];
        char *tmp   = toBaseMetricNames(argv[1]);
        if (!tmp)
            return VERR_NO_MEMORY;
        baseNames   = tmp;
        RTMemFree(tmp);
    }
    else
    {
        metricNames = L"*";
        baseNames = L"*";
    }
    metricNames.cloneTo(&retMetrics[0]);
    baseNames.cloneTo(&retBaseMetrics[0]);

    /* Object name */
    if (argc > 0 && strcmp(argv[0], "*"))
    {
        if (!strcmp(argv[0], "host"))
        {
            ComPtr<IHost> host;
            CHECK_ERROR(aVirtualBox, COMGETTER(Host)(host.asOutParam()));
            retObjects.reset(1);
            host.queryInterfaceTo(&retObjects[0]);
        }
        else
        {
            ComPtr <IMachine> machine;
            rc = aVirtualBox->FindMachine(Bstr(argv[0]), machine.asOutParam());
            if (SUCCEEDED (rc))
            {
                retObjects.reset(1);
                machine.queryInterfaceTo(&retObjects[0]);
            }
            else
            {
                errorArgument("Invalid machine name: '%s'", argv[0]);
                return rc;
            }
        }

    }

    retMetrics.detachTo(ComSafeArrayOutArg(outMetrics));
    retBaseMetrics.detachTo(ComSafeArrayOutArg(outBaseMetrics));
    retObjects.detachTo(ComSafeArrayOutArg(outObjects));

    return rc;
}

static Bstr getObjectName(ComPtr<IVirtualBox> aVirtualBox,
                                  ComPtr<IUnknown> aObject)
{
    HRESULT rc;

    ComPtr<IHost> host = aObject;
    if (!host.isNull())
        return Bstr("host");

    ComPtr<IMachine> machine = aObject;
    if (!machine.isNull())
    {
        Bstr name;
        CHECK_ERROR(machine, COMGETTER(Name)(name.asOutParam()));
        if (SUCCEEDED(rc))
            return name;
    }
    return Bstr("unknown");
}

static void listAffectedMetrics(ComPtr<IVirtualBox> aVirtualBox,
                                ComSafeArrayIn(IPerformanceMetric*, aMetrics))
{
    HRESULT rc;
    com::SafeIfaceArray<IPerformanceMetric> metrics(ComSafeArrayInArg(aMetrics));
    if (metrics.size())
    {
        ComPtr<IUnknown> object;
        Bstr metricName;
        RTPrintf("The following metrics were modified:\n\n"
                 "Object     Metric\n"
                 "---------- --------------------\n");
        for (size_t i = 0; i < metrics.size(); i++)
        {
            CHECK_ERROR(metrics[i], COMGETTER(Object)(object.asOutParam()));
            CHECK_ERROR(metrics[i], COMGETTER(MetricName)(metricName.asOutParam()));
            RTPrintf("%-10ls %-20ls\n",
                getObjectName(aVirtualBox, object).raw(), metricName.raw());
        }
        RTPrintf("\n");
    }
    else
    {
        RTPrintf("No metrics match the specified filter!\n");
    }
}

/**
 * list                                                               *
 */
static int handleMetricsList(int argc, char *argv[],
                             ComPtr<IVirtualBox> aVirtualBox,
                             ComPtr<IPerformanceCollector> performanceCollector)
{
    HRESULT rc;
    com::SafeArray<BSTR>          metrics;
    com::SafeArray<BSTR>          baseMetrics;
    com::SafeIfaceArray<IUnknown> objects;

    rc = parseFilterParameters(argc - 1, &argv[1], aVirtualBox,
                               ComSafeArrayAsOutParam(metrics),
                               ComSafeArrayAsOutParam(baseMetrics),
                               ComSafeArrayAsOutParam(objects));
    if (FAILED(rc))
        return 1;

    com::SafeIfaceArray<IPerformanceMetric> metricInfo;

    CHECK_ERROR(performanceCollector,
        GetMetrics(ComSafeArrayAsInParam(metrics),
                   ComSafeArrayAsInParam(objects),
                   ComSafeArrayAsOutParam(metricInfo)));

    ComPtr<IUnknown> object;
    Bstr metricName, unit, description;
    ULONG period, count;
    LONG minimum, maximum;
    RTPrintf(
"Object     Metric               Unit Minimum    Maximum    Period     Count      Description\n"
"---------- -------------------- ---- ---------- ---------- ---------- ---------- -----------\n");
    for (size_t i = 0; i < metricInfo.size(); i++)
    {
        CHECK_ERROR(metricInfo[i], COMGETTER(Object)(object.asOutParam()));
        CHECK_ERROR(metricInfo[i], COMGETTER(MetricName)(metricName.asOutParam()));
        CHECK_ERROR(metricInfo[i], COMGETTER(Period)(&period));
        CHECK_ERROR(metricInfo[i], COMGETTER(Count)(&count));
        CHECK_ERROR(metricInfo[i], COMGETTER(MinimumValue)(&minimum));
        CHECK_ERROR(metricInfo[i], COMGETTER(MaximumValue)(&maximum));
        CHECK_ERROR(metricInfo[i], COMGETTER(Unit)(unit.asOutParam()));
        CHECK_ERROR(metricInfo[i], COMGETTER(Description)(description.asOutParam()));
        RTPrintf("%-10ls %-20ls %-4ls %10d %10d %10u %10u %ls\n",
            getObjectName(aVirtualBox, object).raw(), metricName.raw(), unit.raw(),
            minimum, maximum, period, count, description.raw());
    }

    return 0;
}

/**
 * Metics setup
 */
static int handleMetricsSetup(int argc, char *argv[],
                              ComPtr<IVirtualBox> aVirtualBox,
                              ComPtr<IPerformanceCollector> performanceCollector)
{
    HRESULT rc;
    com::SafeArray<BSTR>          metrics;
    com::SafeArray<BSTR>          baseMetrics;
    com::SafeIfaceArray<IUnknown> objects;
    uint32_t period = 1, samples = 1;
    bool listMatches = false;
    int i;

    for (i = 1; i < argc; i++)
    {
        if (   !strcmp(argv[i], "--period")
            || !strcmp(argv[i], "-period"))
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            if (   VINF_SUCCESS != RTStrToUInt32Full(argv[++i], 10, &period)
                || !period)
                return errorArgument("Invalid value for 'period' parameter: '%s'", argv[i]);
        }
        else if (   !strcmp(argv[i], "--samples")
                 || !strcmp(argv[i], "-samples"))
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            if (   VINF_SUCCESS != RTStrToUInt32Full(argv[++i], 10, &samples)
                || !samples)
                return errorArgument("Invalid value for 'samples' parameter: '%s'", argv[i]);
        }
        else if (   !strcmp(argv[i], "--list")
                 || !strcmp(argv[i], "-list"))
            listMatches = true;
        else
            break; /* The rest of params should define the filter */
    }

    rc = parseFilterParameters(argc - i, &argv[i], aVirtualBox,
                               ComSafeArrayAsOutParam(metrics),
                               ComSafeArrayAsOutParam(baseMetrics),
                               ComSafeArrayAsOutParam(objects));
    if (FAILED(rc))
        return 1;

    com::SafeIfaceArray<IPerformanceMetric> affectedMetrics;
    CHECK_ERROR(performanceCollector,
        SetupMetrics(ComSafeArrayAsInParam(metrics),
                     ComSafeArrayAsInParam(objects), period, samples,
                     ComSafeArrayAsOutParam(affectedMetrics)));
    if (listMatches)
        listAffectedMetrics(aVirtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));

    return 0;
}

/**
 * metrics query
 */
static int handleMetricsQuery(int argc, char *argv[],
                              ComPtr<IVirtualBox> aVirtualBox,
                              ComPtr<IPerformanceCollector> performanceCollector)
{
    HRESULT rc;
    com::SafeArray<BSTR>          metrics;
    com::SafeArray<BSTR>          baseMetrics;
    com::SafeIfaceArray<IUnknown> objects;

    rc = parseFilterParameters(argc - 1, &argv[1], aVirtualBox,
                               ComSafeArrayAsOutParam(metrics),
                               ComSafeArrayAsOutParam(baseMetrics),
                               ComSafeArrayAsOutParam(objects));
    if (FAILED(rc))
        return 1;

    com::SafeArray<BSTR>          retNames;
    com::SafeIfaceArray<IUnknown> retObjects;
    com::SafeArray<BSTR>          retUnits;
    com::SafeArray<ULONG>         retScales;
    com::SafeArray<ULONG>         retSequenceNumbers;
    com::SafeArray<ULONG>         retIndices;
    com::SafeArray<ULONG>         retLengths;
    com::SafeArray<LONG>          retData;
    CHECK_ERROR (performanceCollector, QueryMetricsData(ComSafeArrayAsInParam(metrics),
                                             ComSafeArrayAsInParam(objects),
                                             ComSafeArrayAsOutParam(retNames),
                                             ComSafeArrayAsOutParam(retObjects),
                                             ComSafeArrayAsOutParam(retUnits),
                                             ComSafeArrayAsOutParam(retScales),
                                             ComSafeArrayAsOutParam(retSequenceNumbers),
                                             ComSafeArrayAsOutParam(retIndices),
                                             ComSafeArrayAsOutParam(retLengths),
                                             ComSafeArrayAsOutParam(retData)) );

    RTPrintf("Object     Metric               Values\n"
             "---------- -------------------- --------------------------------------------\n");
    for (unsigned i = 0; i < retNames.size(); i++)
    {
        Bstr metricUnit(retUnits[i]);
        Bstr metricName(retNames[i]);
        RTPrintf("%-10ls %-20ls ", getObjectName(aVirtualBox, retObjects[i]).raw(), metricName.raw());
        const char *separator = "";
        for (unsigned j = 0; j < retLengths[i]; j++)
        {
            if (retScales[i] == 1)
                RTPrintf("%s%d %ls", separator, retData[retIndices[i] + j], metricUnit.raw());
            else
                RTPrintf("%s%d.%02d%ls", separator, retData[retIndices[i] + j] / retScales[i],
                         (retData[retIndices[i] + j] * 100 / retScales[i]) % 100, metricUnit.raw());
            separator = ", ";
        }
        RTPrintf("\n");
    }

    return 0;
}

static void getTimestamp(char *pts, size_t tsSize)
{
    *pts = 0;
    AssertReturnVoid(tsSize >= 13); /* 3+3+3+3+1 */
    RTTIMESPEC TimeSpec;
    RTTIME Time;
    RTTimeExplode(&Time, RTTimeNow(&TimeSpec));
    pts += RTStrFormatNumber(pts, Time.u8Hour, 10, 2, 0, RTSTR_F_ZEROPAD);
    *pts++ = ':';
    pts += RTStrFormatNumber(pts, Time.u8Minute, 10, 2, 0, RTSTR_F_ZEROPAD);
    *pts++ = ':';
    pts += RTStrFormatNumber(pts, Time.u8Second, 10, 2, 0, RTSTR_F_ZEROPAD);
    *pts++ = '.';
    pts += RTStrFormatNumber(pts, Time.u32Nanosecond / 1000000, 10, 3, 0, RTSTR_F_ZEROPAD);
    *pts = 0;
}

/** Used by the handleMetricsCollect loop. */
static bool volatile g_fKeepGoing = true;

#ifdef RT_OS_WINDOWS
/**
 * Handler routine for catching Ctrl-C, Ctrl-Break and closing of
 * the console.
 *
 * @returns true if handled, false if not handled.
 * @param   dwCtrlType      The type of control signal.
 *
 * @remarks This is called on a new thread.
 */
static BOOL WINAPI ctrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        /* Ctrl-C or Ctrl-Break or Close */
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            /* Let's shut down gracefully. */
            ASMAtomicWriteBool(&g_fKeepGoing, false);
            return TRUE;
    }
    /* Don't care about the rest -- let it die a horrible death. */
    return FALSE;
}
#endif /* RT_OS_WINDOWS */

/**
 * collect
 */
static int handleMetricsCollect(int argc, char *argv[],
                                ComPtr<IVirtualBox> aVirtualBox,
                                ComPtr<IPerformanceCollector> performanceCollector)
{
    HRESULT rc;
    com::SafeArray<BSTR>          metrics;
    com::SafeArray<BSTR>          baseMetrics;
    com::SafeIfaceArray<IUnknown> objects;
    uint32_t period = 1, samples = 1;
    bool isDetached = false, listMatches = false;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (   !strcmp(argv[i], "--period")
            || !strcmp(argv[i], "-period"))
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            if (   VINF_SUCCESS != RTStrToUInt32Full(argv[++i], 10, &period)
                || !period)
                return errorArgument("Invalid value for 'period' parameter: '%s'", argv[i]);
        }
        else if (   !strcmp(argv[i], "--samples")
                 || !strcmp(argv[i], "-samples"))
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            if (   VINF_SUCCESS != RTStrToUInt32Full(argv[++i], 10, &samples)
                || !samples)
                return errorArgument("Invalid value for 'samples' parameter: '%s'", argv[i]);
        }
        else if (   !strcmp(argv[i], "--list")
                 || !strcmp(argv[i], "-list"))
            listMatches = true;
        else if (   !strcmp(argv[i], "--detach")
                 || !strcmp(argv[i], "-detach"))
            isDetached = true;
        else
            break; /* The rest of params should define the filter */
    }

    rc = parseFilterParameters(argc - i, &argv[i], aVirtualBox,
                               ComSafeArrayAsOutParam(metrics),
                               ComSafeArrayAsOutParam(baseMetrics),
                               ComSafeArrayAsOutParam(objects));
    if (FAILED(rc))
        return 1;


    com::SafeIfaceArray<IPerformanceMetric> affectedMetrics;
    CHECK_ERROR(performanceCollector,
        SetupMetrics(ComSafeArrayAsInParam(baseMetrics),
                     ComSafeArrayAsInParam(objects), period, samples,
                     ComSafeArrayAsOutParam(affectedMetrics)));
    if (listMatches)
        listAffectedMetrics(aVirtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));
    if (!affectedMetrics.size())
        return 1;

    if (isDetached)
    {
        RTPrintf("Warning! The background process holding collected metrics will shutdown\n"
                 "in few seconds, discarding all collected data and parameters.\n");
        return 0;
    }

#ifdef RT_OS_WINDOWS
    SetConsoleCtrlHandler(ctrlHandler, true);
#endif /* RT_OS_WINDOWS */

    RTPrintf("Time stamp   Object     Metric               Value\n");

    while (g_fKeepGoing)
    {
        RTPrintf("------------ ---------- -------------------- --------------------\n");
        RTThreadSleep(period * 1000); // Sleep for 'period' seconds
        char ts[15];

        getTimestamp(ts, sizeof(ts));
        com::SafeArray<BSTR>          retNames;
        com::SafeIfaceArray<IUnknown> retObjects;
        com::SafeArray<BSTR>          retUnits;
        com::SafeArray<ULONG>         retScales;
        com::SafeArray<ULONG>         retSequenceNumbers;
        com::SafeArray<ULONG>         retIndices;
        com::SafeArray<ULONG>         retLengths;
        com::SafeArray<LONG>          retData;
        CHECK_ERROR (performanceCollector, QueryMetricsData(ComSafeArrayAsInParam(metrics),
                                                 ComSafeArrayAsInParam(objects),
                                                 ComSafeArrayAsOutParam(retNames),
                                                 ComSafeArrayAsOutParam(retObjects),
                                                 ComSafeArrayAsOutParam(retUnits),
                                                 ComSafeArrayAsOutParam(retScales),
                                                 ComSafeArrayAsOutParam(retSequenceNumbers),
                                                 ComSafeArrayAsOutParam(retIndices),
                                                 ComSafeArrayAsOutParam(retLengths),
                                                 ComSafeArrayAsOutParam(retData)) );
        for (unsigned i = 0; i < retNames.size(); i++)
        {
            Bstr metricUnit(retUnits[i]);
            Bstr metricName(retNames[i]);
            RTPrintf("%-12s %-10ls %-20ls ", ts, getObjectName(aVirtualBox, retObjects[i]).raw(), metricName.raw());
            const char *separator = "";
            for (unsigned j = 0; j < retLengths[i]; j++)
            {
                if (retScales[i] == 1)
                    RTPrintf("%s%d %ls", separator, retData[retIndices[i] + j], metricUnit.raw());
                else
                    RTPrintf("%s%d.%02d%ls", separator, retData[retIndices[i] + j] / retScales[i],
                             (retData[retIndices[i] + j] * 100 / retScales[i]) % 100, metricUnit.raw());
                separator = ", ";
            }
            RTPrintf("\n");
        }
    }

#ifdef RT_OS_WINDOWS
    SetConsoleCtrlHandler(ctrlHandler, false);
#endif /* RT_OS_WINDOWS */

    return 0;
}

int handleMetrics(HandlerArg *a)
{
    int rc;

    /* at least one option: subcommand name */
    if (a->argc < 1)
        return errorSyntax(USAGE_METRICS, "Subcommand missing");

    ComPtr<IPerformanceCollector> performanceCollector;
    CHECK_ERROR(a->virtualBox, COMGETTER(PerformanceCollector)(performanceCollector.asOutParam()));

    if (!strcmp(a->argv[0], "list"))
        rc = handleMetricsList(a->argc, a->argv, a->virtualBox, performanceCollector);
    else if (!strcmp(a->argv[0], "setup"))
        rc = handleMetricsSetup(a->argc, a->argv, a->virtualBox, performanceCollector);
    else if (!strcmp(a->argv[0], "query"))
        rc = handleMetricsQuery(a->argc, a->argv, a->virtualBox, performanceCollector);
    else if (!strcmp(a->argv[0], "collect"))
        rc = handleMetricsCollect(a->argc, a->argv, a->virtualBox, performanceCollector);
    else
        return errorSyntax(USAGE_METRICS, "Invalid subcommand '%s'", a->argv[0]);

    return rc;
}

#endif /* VBOX_ONLY_DOCS */

