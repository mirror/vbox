/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsDialogSpecific class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISettingsDialogSpecific_h__
#define __UISettingsDialogSpecific_h__

/* GUI includes: */
#include "UISettingsDialog.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"
#include "CConsole.h"
#include "CMachine.h"

/* Dialog which encapsulate all the specific functionalities of the Global Settings */
class UISettingsDialogGlobal : public UISettingsDialog
{
    Q_OBJECT;

public:

    UISettingsDialogGlobal(QWidget *pParent);
    ~UISettingsDialogGlobal();

protected:

    void loadData();
    void saveData();

    void retranslateUi();

    QString title() const;

private:

    bool isPageAvailable(int iPageId);
};

/* Dialog which encapsulate all the specific functionalities of the Virtual Machine Settings */
class UISettingsDialogMachine : public UISettingsDialog
{
    Q_OBJECT;

public:

    UISettingsDialogMachine(QWidget *pParent, const QString &strMachineId,
                            const QString &strCategory, const QString &strControl);
    ~UISettingsDialogMachine();

protected:

    void loadData();
    void saveData();

    void retranslateUi();

    QString title() const;

    void recorrelate(UISettingsPage *pSettingsPage);

private slots:

    void sltMarkLoaded();
    void sltMarkSaved();
    void sltSessionStateChanged(QString strMachineId, KSessionState sessionState);
    void sltMachineStateChanged(QString strMachineId, KMachineState machineState);
    void sltMachineDataChanged(QString strMachineId);
    void sltCategoryChanged(int cId);
    void sltAllowResetFirstRunFlag();
    void sltSetFirstRunFlag();
    void sltResetFirstRunFlag();

private:

    bool isPageAvailable(int iPageId);
    bool isSettingsChanged();
    void updateDialogType();

    QString m_strMachineId;
    KSessionState m_sessionState;
    KMachineState m_machineState;

    CSession m_session;
    CMachine m_machine;
    CConsole m_console;

    bool m_fAllowResetFirstRunFlag;
    bool m_fResetFirstRunFlag;
};

#endif // __UISettingsDialogSpecific_h__

