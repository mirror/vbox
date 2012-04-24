/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsDialogSpecific class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

/* Local includes */
#include "COMDefs.h"
#include "UISettingsDialog.h"

/* Dialog which encapsulate all the specific functionalities of the Global Settings */
class UISettingsDialogGlobal : public UISettingsDialog
{
    Q_OBJECT;

public:

    enum GLSettingsPage
    {
        GLSettingsPage_General = 0,
        GLSettingsPage_Input,
        GLSettingsPage_Update,
        GLSettingsPage_Language,
        GLSettingsPage_Display,
        GLSettingsPage_USB,
        GLSettingsPage_Network,
        GLSettingsPage_Extension,
        GLSettingsPage_Proxy,
        GLSettingsPage_MAX
    };

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

    enum VMSettingsPage
    {
        VMSettingsPage_General = 0,
        VMSettingsPage_System,
        VMSettingsPage_Display,
        VMSettingsPage_Storage,
        VMSettingsPage_Audio,
        VMSettingsPage_Network,
        VMSettingsPage_Ports,
        VMSettingsPage_Serial,
        VMSettingsPage_Parallel,
        VMSettingsPage_USB,
        VMSettingsPage_SF,
        VMSettingsPage_MAX
    };

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
    void sltMachineStateChanged(QString strMachineId, KMachineState machineState);
    void sltMachineDataChanged(QString strMachineId);
    void sltCategoryChanged(int cId);
    void sltAllowResetFirstRunFlag();
    void sltSetFirstRunFlag();
    void sltResetFirstRunFlag();

private:

    bool isPageAvailable(int iPageId);
    bool isSettingsChanged();

    QString m_strMachineId;
    KMachineState m_machineState;

    CSession m_session;
    CMachine m_machine;
    CConsole m_console;

    bool m_fAllowResetFirstRunFlag;
    bool m_fResetFirstRunFlag;
};

#endif // __UISettingsDialogSpecific_h__

