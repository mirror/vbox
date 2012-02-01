/* $Id: */
/** @file
 * VBoxWatchdogUtils - Misc. utility functions for modules.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/array.h>
#include "VBoxWatchdogInternal.h"


/**
 * Retrieves a metric from a specified machine.
 *
 * @return  IPRT status code.
 * @param   pMachine                Pointer to the machine's internal structure.
 * @param   strName                 Name of metric to retrieve.
 * @param   pulData                 Pointer to value to retrieve the actual metric value.
 */
int getMetric(PVBOXWATCHDOG_MACHINE pMachine, const Bstr& strName, LONG *pulData)
{
    AssertPtrReturn(pMachine, VERR_INVALID_POINTER);
    AssertPtrReturn(pulData, VERR_INVALID_POINTER);

    /* Input. */
    com::SafeArray<BSTR> metricNames(1);
    com::SafeIfaceArray<IUnknown> metricObjects(1);
    pMachine->machine.queryInterfaceTo(&metricObjects[0]);

    /* Output. */
    com::SafeArray<BSTR>          retNames;
    com::SafeIfaceArray<IUnknown> retObjects;
    com::SafeArray<BSTR>          retUnits;
    com::SafeArray<ULONG>         retScales;
    com::SafeArray<ULONG>         retSequenceNumbers;
    com::SafeArray<ULONG>         retIndices;
    com::SafeArray<ULONG>         retLengths;
    com::SafeArray<LONG>          retData;

    /* Query current memory free. */
    strName.cloneTo(&metricNames[0]);
#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
    Assert(!g_pPerfCollector.isNull());
    HRESULT hrc = g_pPerfCollector->QueryMetricsData(
#else
    Assert(!pMachine->collector.isNull());
    HRESULT hrc = pMachine->collector->QueryMetricsData(
#endif
                                                ComSafeArrayAsInParam(metricNames),
                                                ComSafeArrayAsInParam(metricObjects),
                                                ComSafeArrayAsOutParam(retNames),
                                                ComSafeArrayAsOutParam(retObjects),
                                                ComSafeArrayAsOutParam(retUnits),
                                                ComSafeArrayAsOutParam(retScales),
                                                ComSafeArrayAsOutParam(retSequenceNumbers),
                                                ComSafeArrayAsOutParam(retIndices),
                                                ComSafeArrayAsOutParam(retLengths),
                                                ComSafeArrayAsOutParam(retData));
#if 0
    /* Useful for metrics debugging. */
    for (unsigned j = 0; j < retNames.size(); j++)
    {
        Bstr metricUnit(retUnits[j]);
        Bstr metricName(retNames[j]);
        RTPrintf("%-20ls ", metricName.raw());
        const char *separator = "";
        for (unsigned k = 0; k < retLengths[j]; k++)
        {
            if (retScales[j] == 1)
                RTPrintf("%s%d %ls", separator, retData[retIndices[j] + k], metricUnit.raw());
            else
                RTPrintf("%s%d.%02d%ls", separator, retData[retIndices[j] + k] / retScales[j],
                         (retData[retIndices[j] + k] * 100 / retScales[j]) % 100, metricUnit.raw());
            separator = ", ";
        }
        RTPrintf("\n");
    }
#endif

    if (SUCCEEDED(hrc))
        *pulData = retData.size() ? retData[retIndices[0]] : 0;

    return SUCCEEDED(hrc) ? VINF_SUCCESS : VINF_NOT_SUPPORTED;
}

/**
 * Returns the payload of a machine.
 *
 * @return  void*                   Pointer to payload data. Mutable!
 * @param   pMachine                Machine to get payload for.
 * @param   pszModule               Module name to get payload from.
 */
void* getPayload(PVBOXWATCHDOG_MACHINE pMachine, const char *pszModule)
{
    AssertPtrReturn(pMachine, NULL);
    AssertPtrReturn(pszModule, NULL);
    mapPayloadIter it = pMachine->payload.find(pszModule);
    if (it == pMachine->payload.end())
        return NULL;
    Assert(it->second.cbPayload);
    return it->second.pvPayload;
}

