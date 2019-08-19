/* $Id$ */
/** @file
 * VBox Qt GUI - UIPerformanceMonitor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h
#define FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QMap>
#include <QQueue>


/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CPerformanceCollector.h"

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QTimer;
class QGridLayout;
class QLabel;
class UIChart;
class UISession;

class UISubMetric
{
public:

    UISubMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize);
    UISubMetric();
    const QString &name() const;

    void setMaximum(ULONG iMaximum);
    ULONG maximum() const;

    void setUnit(QString strUnit);
    const QString &unit() const;

    void addData(int iDataSeriesIndex, ULONG fData);
    const QQueue<ULONG> *data(int iDataSeriesIndex) const;

    bool requiresGuestAdditions() const;
    void setRequiresGuestAdditions(bool fRequiresGAs);

private:

    QString m_strName;
    QString m_strUnit;
    ULONG m_iMaximum;
    QQueue<ULONG> m_data[2];
    int m_iMaximumQueueSize;
    bool m_fRequiresGuestAdditions;
};

/** UIPerformanceMonitor class displays some high level performance metric of the guest system.
  * The values are read in certain periods and cached in the GUI side. Currently we draw some line charts
  * and pie charts (where applicable) alongside with some text. */
class UIPerformanceMonitor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIPerformanceMonitor(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession);
    ~UIPerformanceMonitor();

protected:
    void retranslateUi();

private slots:

    void sltTimeout();
    void sltGuestAdditionsStateChange();

private:

    /** Prepares layout. */
    void prepareObjects();
    void preparePerformaceCollector();
    bool guestAdditionsAvailable(int iMinimumMajorVersion);
    void enableDisableGuestAdditionDependedWidgets(bool fEnable);
    void updateCPUGraphsAndMetric(ULONG iLoadPercentage, ULONG iOtherPercentage);
    void updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM);
    void updateNewGraphsAndMetric(ULONG iReceiveRate, ULONG iTransmitRate);
    QString dataColorString(const QString &strChartName, int iDataIndex);

    bool m_fGuestAdditionsAvailable;
    /** Holds the machine instance. */
    CMachine m_machine;
    /** Holds the console instance. */
    CConsole m_console;
    CGuest m_comGuest;

    CPerformanceCollector m_performanceMonitor;
    CMachineDebugger      m_machineDebugger;
    /** Holds the instance of layout we create. */
    QGridLayout *m_pMainLayout;
    QTimer *m_pTimer;

    QVector<QString> m_nameList;
    QVector<CUnknown> m_objectList;

    QMap<QString, UISubMetric> m_subMetrics;
    QMap<QString,UIChart*>  m_charts;
    QMap<QString,QLabel*>  m_infoLabels;

    /** @name These metric names are used for map keys to identify metrics.
      * @{ */
        QString m_strCPUMetricName;
        QString m_strRAMMetricName;
        QString m_strDiskMetricName;
        QString m_strNetMetricName;
    /** @} */

    /** @name Cached translated string.
      * @{ */
        /** CPU info label strings. */
        QString m_strCPUInfoLabelTitle;
        QString m_strCPUInfoLabelGuest;
        QString  m_strCPUInfoLabelVMM;
        /** RAM usage info label strings. */
        QString m_strRAMInfoLabelTitle;
        QString m_strRAMInfoLabelTotal;
        QString m_strRAMInfoLabelFree;
        QString m_strRAMInfoLabelUsed;
        /** Net traffic info label strings. */
        QString m_strNetInfoLabelTitle;
        QString m_strNetInfoLabelReceived;
        QString m_strNetInfoLabelTransmitted;
        QString m_strNetInfoLabelMaximum;
    /** @} */


};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h */
