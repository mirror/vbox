/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class declaration.
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

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
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
#include "UIMainEventListener.h"


/* Forward declarations: */
class QVBoxLayout;
class QLabel;
class UIChart;
class UISession;
class UIRuntimeInfoWidget;

/** UIInformationRuntime class displays some high level performance metric of the guest system.
  * The values are read in certain periods and cached in the GUI side. Currently we draw some line charts
  * and pie charts (where applicable) alongside with some text. Additionally it displays a table including some
  * run time attributes. */
class UIInformationRuntime : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession);
    ~UIInformationRuntime();

protected:
    void retranslateUi();

private slots:

    /** @name These functions are connected to API events and implement necessary updates.
      * @{ */
        void sltGuestAdditionsStateChange();
        void sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);
        void sltVRDEChange();
        void sltClipboardChange(KClipboardMode enmMode);
        void sltDnDModeChange(KDnDMode enmMode);
    /** @} */

private:

    void prepareObjects();

    CMachine m_machine;
    CConsole m_console;
    CGuest m_comGuest;

    CPerformanceCollector m_performanceMonitor;
    CMachineDebugger      m_machineDebugger;
    /** Holds the instance of layout we create. */
    QVBoxLayout *m_pMainLayout;
    UIRuntimeInfoWidget *m_pRuntimeInfoWidget;

    QVector<QString> m_nameList;
    QVector<CUnknown> m_objectList;

    QMap<QString,QLabel*>  m_infoLabels;
    ComObjPtr<UIMainEventListenerImpl> m_pQtGuestListener;

    /** @name These metric names are used for map keys to identify metrics.
      * @{ */
        QString m_strCPUMetricName;
        QString m_strRAMMetricName;
        QString m_strDiskMetricName;
        QString m_strNetworkMetricName;
        QString m_strDiskIOMetricName;
        QString m_strVMExitMetricName;
    /** @} */

    /** The following string is used while querrying CMachineDebugger. */
    QString m_strQueryString;
    quint64 m_iTimeStep;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h */
