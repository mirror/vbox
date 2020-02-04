/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class declaration.
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

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CGuest.h"
#include "CMachine.h"

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QVBoxLayout;
class UISession;
class UIRuntimeInfoWidget;

/** UIInformationRuntime class displays a table including some
  * run time attributes. */
class UIInformationRuntime : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession);

protected:

    void retranslateUi();

private slots:

    /** @name These functions are connected to API events and implement necessary updates on the table.
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

    /** Holds the instance of layout we create. */
    QVBoxLayout *m_pMainLayout;
    UIRuntimeInfoWidget *m_pRuntimeInfoWidget;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h */
