/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
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
#include <QApplication>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIInformationRuntime.h"
#include "UIInformationDataItem.h"
#include "UIInformationItem.h"
#include "UIInformationView.h"
#include "UIExtraDataManager.h"
#include "UIInformationModel.h"

/* COM includes: */
#include "CDisplay.h"
#include "CMachineDebugger.h"
#include "CVRDEServerInfo.h"

UIInformationRuntime::UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : UIInformationWidget(pParent,machine, console)
{
    retranslateUi();
    createTableItems();
}

void UIInformationRuntime::retranslateUi()
{
    m_strRuntimeTitle = QApplication::translate("UIVMInformationDialog", "Runtime Attributes");
}

void UIInformationRuntime::createTableItems()
{
    if (!m_pTableWidget)
        return;
    QFontMetrics fontMetrics(m_pTableWidget->font());
    QTextDocument textDocument;
    int iMaxColumn1Length = 0;

    insertTitleRow(m_strRuntimeTitle, UIIconPool::iconSet(":/state_running_16px.png"), fontMetrics);


    insertInfoRows(runTimeAttributes(),
                   fontMetrics, textDocument, iMaxColumn1Length);



    m_pTableWidget->resizeColumnToContents(0);
    /* Resize the column 1 a bit larger than the max string if contains: */
    m_pTableWidget->setColumnWidth(1, 1.5 * iMaxColumn1Length);
    m_pTableWidget->resizeColumnToContents(2);
}

UITextTable UIInformationRuntime::runTimeAttributes()
{
    UITextTable textTable;

    ULONG cGuestScreens = m_machine.GetMonitorCount();
    QVector<QString> aResolutions(cGuestScreens);
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
    {
        /* Determine resolution: */
        ULONG uWidth = 0;
        ULONG uHeight = 0;
        ULONG uBpp = 0;
        LONG xOrigin = 0;
        LONG yOrigin = 0;
        KGuestMonitorStatus monitorStatus = KGuestMonitorStatus_Enabled;
        m_console.GetDisplay().GetScreenResolution(iScreen, uWidth, uHeight, uBpp, xOrigin, yOrigin, monitorStatus);
        QString strResolution = QString("%1x%2").arg(uWidth).arg(uHeight);
        if (uBpp)
            strResolution += QString("x%1").arg(uBpp);
        strResolution += QString(" @%1,%2").arg(xOrigin).arg(yOrigin);
        if (monitorStatus == KGuestMonitorStatus_Disabled)
        {
            strResolution += QString(" ");
            strResolution += QString(QApplication::translate("UIVMInformationDialog", "turned off"));
        }
        aResolutions[iScreen] = strResolution;
    }

    /* Determine uptime: */
    CMachineDebugger debugger = m_console.GetDebugger();
    uint32_t uUpSecs = (debugger.GetUptime() / 5000) * 5;
    char szUptime[32];
    uint32_t uUpDays = uUpSecs / (60 * 60 * 24);
    uUpSecs -= uUpDays * 60 * 60 * 24;
    uint32_t uUpHours = uUpSecs / (60 * 60);
    uUpSecs -= uUpHours * 60 * 60;
    uint32_t uUpMins  = uUpSecs / 60;
    uUpSecs -= uUpMins * 60;
    RTStrPrintf(szUptime, sizeof(szUptime), "%dd %02d:%02d:%02d",
                uUpDays, uUpHours, uUpMins, uUpSecs);
    QString strUptime = QString(szUptime);

    /* Determine clipboard mode: */
    QString strClipboardMode = gpConverter->toString(m_machine.GetClipboardMode());
    /* Determine Drag&Drop mode: */
    QString strDnDMode = gpConverter->toString(m_machine.GetDnDMode());

    /* Determine virtualization attributes: */
    QString strVirtualization = debugger.GetHWVirtExEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active") :
        QApplication::translate("UIVMInformationDialog", "Inactive");

    QString strExecutionEngine;
    switch (debugger.GetExecutionEngine())
    {
        case KVMExecutionEngine_HwVirt:
            strExecutionEngine = "VT-x/AMD-V";  /* no translation */
            break;
        case KVMExecutionEngine_RawMode:
            strExecutionEngine = "raw-mode";    /* no translation */
            break;
        case KVMExecutionEngine_NativeApi:
            strExecutionEngine = "native API";  /* no translation */
            break;
        default:
            AssertFailed();
            RT_FALL_THRU();
        case KVMExecutionEngine_NotSet:
            strExecutionEngine = QApplication::translate("UIVMInformationDialog", "not set");
            break;
    }
    QString strNestedPaging = debugger.GetHWVirtExNestedPagingEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active"):
        QApplication::translate("UIVMInformationDialog", "Inactive");

    QString strUnrestrictedExecution = debugger.GetHWVirtExUXEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active"):
        QApplication::translate("UIVMInformationDialog", "Inactive");

        QString strParavirtProvider = gpConverter->toString(m_machine.GetEffectiveParavirtProvider());

    /* Guest information: */
    CGuest guest = m_console.GetGuest();
    QString strGAVersion = guest.GetAdditionsVersion();
    if (strGAVersion.isEmpty())
        strGAVersion = tr("Not Detected", "guest additions");
    else
    {
        ULONG uRevision = guest.GetAdditionsRevision();
        if (uRevision != 0)
            strGAVersion += QString(" r%1").arg(uRevision);
    }
    QString strOSType = guest.GetOSTypeId();
    if (strOSType.isEmpty())
        strOSType = tr("Not Detected", "guest os type");
    else
        strOSType = uiCommon().vmGuestOSTypeDescription(strOSType);

    /* VRDE information: */
    int iVRDEPort = m_console.GetVRDEServerInfo().GetPort();
    QString strVRDEInfo = (iVRDEPort == 0 || iVRDEPort == -1)?
        tr("Not Available", "details report (VRDE server port)") :
        QString("%1").arg(iVRDEPort);

    /* Searching for longest string: */
    QStringList values;
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
        values << aResolutions[iScreen];
    values << strUptime
           << strExecutionEngine << strNestedPaging << strUnrestrictedExecution
           << strGAVersion << strOSType << strVRDEInfo;
    int iMaxLength = 0;
    foreach (const QString &strValue, values)
        iMaxLength = iMaxLength < QApplication::fontMetrics().width(strValue)
                                  ? QApplication::fontMetrics().width(strValue) : iMaxLength;

    /* Summary: */
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
    {
        QString strLabel(tr("Screen Resolution"));
        /* The screen number makes sense only if there are multiple monitors in the guest: */
        if (cGuestScreens > 1)
            strLabel += QString(" %1").arg(iScreen + 1);
        textTable << UITextTableLine(strLabel, aResolutions[iScreen]);
    }

    textTable << UITextTableLine(tr("VM Uptime"), strUptime);
    textTable << UITextTableLine(tr("Clipboard Mode"), strClipboardMode);
    textTable << UITextTableLine(tr("Drag and Drop Mode"), strDnDMode);
    textTable << UITextTableLine(tr("VM Execution Engine", "details report"), strExecutionEngine);
    textTable << UITextTableLine(tr("Nested Paging", "details report"), strNestedPaging);
    textTable << UITextTableLine(tr("Unrestricted Execution", "details report"), strUnrestrictedExecution);
    textTable << UITextTableLine(tr("Paravirtualization Interface", "details report"), strParavirtProvider);
    textTable << UITextTableLine(tr("Guest Additions"), strGAVersion);
    textTable << UITextTableLine(tr("Guest OS Type", "details report"), strOSType);
    textTable << UITextTableLine(tr("Remote Desktop Server Port", "details report (VRDE Server)"), strVRDEInfo);

    return textTable;
}
