/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationPerformanceMonitor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationPerformanceMonitor_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationPerformanceMonitor_h
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
class QVBoxLayout;
class QLabel;
class UIChart;
class UISession;
class UIRuntimeInfoWidget;

#define DATA_SERIES_SIZE 2

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

/** UIMetric represents a performance metric and is used to store data related to the corresponding metric. */
class UIMetric
{

public:

    UIMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize);
    UIMetric();
    const QString &name() const;

    void setMaximum(quint64 iMaximum);
    quint64 maximum() const;

    void setUnit(QString strUnit);
    const QString &unit() const;

    void addData(int iDataSeriesIndex, quint64 fData);
    const QQueue<quint64> *data(int iDataSeriesIndex) const;

    void setTotal(int iDataSeriesIndex, quint64 iTotal);
    quint64 total(int iDataSeriesIndex) const;

    bool requiresGuestAdditions() const;
    void setRequiresGuestAdditions(bool fRequiresGAs);

    void setIsInitialized(bool fIsInitialized);
    bool isInitialized() const;

    void reset();

private:

    QString m_strName;
    QString m_strUnit;
    quint64 m_iMaximum;
    QQueue<quint64> m_data[DATA_SERIES_SIZE];
    /** The total data (the counter value we get from IMachineDebugger API). For the metrics
      * we get from IMachineDebugger m_data values are computed as deltas of total values t - (t-1) */
    quint64 m_iTotal[DATA_SERIES_SIZE];
    int m_iMaximumQueueSize;
    bool m_fRequiresGuestAdditions;
    /** Used for metrices whose data is computed as total deltas. That is we receieve only total value and
      * compute time step data from total deltas. m_isInitialised is true if the total has been set first time. */
    bool m_fIsInitialized;
};

/** UIInformationPerformanceMonitor class displays some high level performance metric of the guest system.
  * The values are read in certain periods and cached in the GUI side. Currently we draw some line charts
  * and pie charts (where applicable) alongside with some text. IPerformanceCollector and IMachineDebugger are
  * two sources of the performance metrics. Unfortunately these two have very distinct APIs resulting a bit too much
  * special casing etc.*/
class UIInformationPerformanceMonitor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationPerformanceMonitor(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession);
    ~UIInformationPerformanceMonitor();

protected:

    void retranslateUi();

private slots:

    /** Reads the metric values for several sources and calls corresponding update functions. */
    void sltTimeout();
    /** @name These functions are connected to API events and implement necessary updates.
      * @{ */
        void sltGuestAdditionsStateChange();
    /** @} */

private:

    void prepareObjects();
    void prepareMetrics();
    bool guestAdditionsAvailable(int iMinimumMajorVersion);
    void enableDisableGuestAdditionDependedWidgets(bool fEnable);

    /** @name The following functions update corresponding metric charts and labels with new values
      * @{ */
        void updateCPUGraphsAndMetric(ULONG iLoadPercentage, ULONG iOtherPercentage);
        void updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM);
        void updateNetworkGraphsAndMetric(quint64 iReceiveTotal, quint64 iTransmitTotal);
        void updateDiskIOGraphsAndMetric(quint64 uDiskIOTotalWritten, quint64 uDiskIOTotalRead);
        void updateVMExitMetric(quint64 uTotalVMExits);
    /** @} */

    /** Returns a QColor for the chart with @p strChartName and data series with @p iDataIndex. */
    QString dataColorString(const QString &strChartName, int iDataIndex);
    /** Parses the xml string we get from the IMachineDebugger and returns an array of UIDebuggerMetricData. */
    QVector<UIDebuggerMetricData> getAndParseStatsFromDebugger(const QString &strQuery);

    bool m_fGuestAdditionsAvailable;
    CMachine m_machine;
    CConsole m_console;
    CGuest m_comGuest;

    CPerformanceCollector m_performanceMonitor;
    CMachineDebugger      m_machineDebugger;
    /** Holds the instance of layout we create. */
    QVBoxLayout *m_pMainLayout;
    QTimer *m_pTimer;

    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
      * @{ */
        QVector<QString> m_nameList;
        QVector<CUnknown> m_objectList;
    /** @} */

    QMap<QString, UIMetric> m_metrics;
    QMap<QString,UIChart*>  m_charts;
    /** Stores the QLabel instances which we show next to each UIChart. The value is the name of the metric. */
    QMap<QString,QLabel*>   m_infoLabels;

    /** @name These metric names are used for map keys to identify metrics.
      * @{ */
        QString m_strCPUMetricName;
        QString m_strRAMMetricName;
        QString m_strDiskMetricName;
        QString m_strNetworkMetricName;
        QString m_strDiskIOMetricName;
        QString m_strVMExitMetricName;
    /** @} */

    /** @name Cached translated strings.
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
        QString m_strNetworkInfoLabelTitle;
        QString m_strNetworkInfoLabelReceived;
        QString m_strNetworkInfoLabelTransmitted;
        QString m_strNetworkInfoLabelReceivedTotal;
        QString m_strNetworkInfoLabelTransmittedTotal;
        /** Disk IO info label strings. */
        QString m_strDiskIOInfoLabelTitle;
        QString m_strDiskIOInfoLabelWritten;
        QString m_strDiskIOInfoLabelRead;
        QString m_strDiskIOInfoLabelWrittenTotal;
        QString m_strDiskIOInfoLabelReadTotal;
        /** VM Exit info label strings. */
        QString m_strVMExitInfoLabelTitle;
        QString m_strVMExitLabelCurrent;
        QString m_strVMExitLabelTotal;
    /** @} */
    quint64 m_iTimeStep;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationPerformanceMonitor_h */
