/* $Id$ */
/** @file
 * VBox Qt GUI - UIMonitorCommon class implementation.
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

/* Qt includes: */
#include <QXmlStreamReader>


/* GUI includes: */
#include "UICommon.h"
#include "UIMonitorCommon.h"

/* COM includes: */
#include "CMachineDebugger.h"

/* static */
void UIMonitorCommon::getNetworkLoad(CMachineDebugger &debugger, quint64 &uOutNetworkReceived, quint64 &uOutNetworkTransmitted)
{
    uOutNetworkReceived = 0;
    uOutNetworkTransmitted = 0;
    QVector<UIDebuggerMetricData> xmlData = getAndParseStatsFromDebugger(debugger, "/Public/NetAdapter/*/Bytes*");
    foreach (const UIDebuggerMetricData &data, xmlData)
    {
        if (data.m_strName.endsWith("BytesReceived"))
            uOutNetworkReceived += data.m_counter;
        else if (data.m_strName.endsWith("BytesTransmitted"))
            uOutNetworkTransmitted += data.m_counter;
        else
            AssertMsgFailed(("name=%s\n", data.m_strName.toLocal8Bit().data()));
    }
}

/* static */
void UIMonitorCommon::getDiskLoad(CMachineDebugger &debugger, quint64 &uOutDiskWritten, quint64 &uOutDiskRead)
{
    QVector<UIDebuggerMetricData> xmlData = getAndParseStatsFromDebugger(debugger, "/Public/Storage/*/Port*/Bytes*");
    foreach (const UIDebuggerMetricData &data, xmlData)
    {
        if (data.m_strName.endsWith("BytesWritten"))
            uOutDiskWritten += data.m_counter;
        else if (data.m_strName.endsWith("BytesRead"))
            uOutDiskRead += data.m_counter;
        else
            AssertMsgFailed(("name=%s\n", data.m_strName.toLocal8Bit().data()));
    }
}

/* static */
void UIMonitorCommon::getVMMExitCount(CMachineDebugger &debugger, quint64 &uOutVMMExitCount)
{
    QVector<UIDebuggerMetricData> xmlData = getAndParseStatsFromDebugger(debugger, "/PROF/CPU*/EM/RecordedExits");
    foreach (const UIDebuggerMetricData &data, xmlData)
    {
        if (data.m_strName.endsWith("RecordedExits"))
            uOutVMMExitCount += data.m_counter;
        else
            AssertMsgFailed(("name=%s\n", data.m_strName.toLocal8Bit().data()));
    }
}


/* static */
QVector<UIDebuggerMetricData> UIMonitorCommon::getAndParseStatsFromDebugger(CMachineDebugger &debugger, const QString &strQuery)
{
    QVector<UIDebuggerMetricData> xmlData;
    if (strQuery.isEmpty())
        return xmlData;
    QString strStats = debugger.GetStats(strQuery, false);
    QXmlStreamReader xmlReader;
    xmlReader.addData(strStats);
    if (xmlReader.readNextStartElement())
    {
        while (xmlReader.readNextStartElement())
        {
            if (xmlReader.name() == "Counter")
            {
                QXmlStreamAttributes attributes = xmlReader.attributes();
                quint64 iCounter = attributes.value("c").toULongLong();
                xmlData.push_back(UIDebuggerMetricData(attributes.value("name"), iCounter));
            }
            else if (xmlReader.name() == "U64")
            {
                QXmlStreamAttributes attributes = xmlReader.attributes();
                quint64 iCounter = attributes.value("val").toULongLong();
                xmlData.push_back(UIDebuggerMetricData(attributes.value("name"), iCounter));
            }
            xmlReader.skipCurrentElement();
        }
    }
    return xmlData;
}
