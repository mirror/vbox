/* $Id$ */
/** @file
 * VBox Qt GUI - UIMonitorCommon class declaration.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_monitor_UIMonitorCommon_h
#define FEQT_INCLUDED_SRC_monitor_UIMonitorCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** UIDebuggerMetricData is used as data storage while parsing the xml stream received from IMachineDebugger. */
struct UIDebuggerMetricData
{
    UIDebuggerMetricData()
        : m_counter(0){}
    UIDebuggerMetricData(const QStringRef & strName, quint64 counter)
        : m_strName(strName.toString())
        , m_counter(counter){}
    QString m_strName;
    quint64 m_counter;
};


class SHARED_LIBRARY_STUFF UIMonitorCommon
{

public:

    /** @name Static utlity methods that query and parse IMachineDebugger outputs for specific metrix types.
      * @{ */
        static void getNetworkLoad(CMachineDebugger &debugger, quint64 &uOutNetworkReceived, quint64 &uOutNetworkTransmitted);
        static void getDiskLoad(CMachineDebugger &debugger, quint64 &uOutDiskWritten, quint64 &uOutDiskRead);
        static void getVMMExitCount(CMachineDebugger &debugger, quint64 &uOutVMMExitCount);
    /** @} */
        static void getRAMLoad(CPerformanceCollector &comPerformanceCollector, QVector<QString> &nameList,
                               QVector<CUnknown>& objectList, quint64 &iOutTotalRAM, quint64 &iOutFreeRAM);

private:

    /** Parses the xml string we get from the IMachineDebugger and returns an array of UIDebuggerMetricData. */
    static QVector<UIDebuggerMetricData> getAndParseStatsFromDebugger(CMachineDebugger &debugger, const QString &strQuery);

};

#endif /* !FEQT_INCLUDED_SRC_monitor_UIMonitorCommon_h */
