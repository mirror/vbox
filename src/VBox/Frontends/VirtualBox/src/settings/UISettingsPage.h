/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsPage class declaration
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

#ifndef __UISettingsPage_h__
#define __UISettingsPage_h__

/* Qt includes: */
#include <QWidget>
#include <QVariant>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UISettingsDefs.h"
#include "VBoxGlobalSettings.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CConsole.h"
#include "CSystemProperties.h"

/* Forward declarations: */
class QIWidgetValidator;
class QShowEvent;

/* Using declarations: */
using namespace UISettingsDefs;

/* Settings page types: */
enum UISettingsPageType
{
    UISettingsPageType_Global,
    UISettingsPageType_Machine
};

/* Global settings data wrapper: */
struct UISettingsDataGlobal
{
    UISettingsDataGlobal() {}
    UISettingsDataGlobal(const CSystemProperties &properties, const VBoxGlobalSettings &settings)
        : m_properties(properties), m_settings(settings) {}
    CSystemProperties m_properties;
    VBoxGlobalSettings m_settings;
};
Q_DECLARE_METATYPE(UISettingsDataGlobal);

/* Machine settings data wrapper: */
struct UISettingsDataMachine
{
    UISettingsDataMachine() {}
    UISettingsDataMachine(const CMachine &machine, const CConsole &console)
        : m_machine(machine), m_console(console) {}
    CMachine m_machine;
    CConsole m_console;
};
Q_DECLARE_METATYPE(UISettingsDataMachine);

/* Settings page base class: */
class UISettingsPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    virtual void loadToCacheFrom(QVariant &data) = 0;
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    virtual void getFromCache() = 0;

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    virtual void putToCache() = 0;
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    virtual void saveFromCacheTo(QVariant &data) = 0;

    /* Validation stuff: */
    virtual void setValidator(QIWidgetValidator* /* pValidator */) {}
    virtual bool revalidate(QString& /* strWarningText */, QString& /* strTitle */) { return true; }

    /* Navigation stuff: */
    QWidget* firstWidget() const { return m_pFirstWidget; }
    virtual void setOrderAfter(QWidget *pWidget) { m_pFirstWidget = pWidget; }

    /* Settings page type stuff: */
    UISettingsPageType pageType() const { return m_pageType; }

    /* Settings dialog type stuff: */
    SettingsDialogType dialogType() const { return m_dialogType; }
    virtual void setDialogType(SettingsDialogType settingsDialogType) { m_dialogType = settingsDialogType; polishPage(); }
    bool isMachineOffline() const { return dialogType() == SettingsDialogType_Offline; }
    bool isMachineSaved() const { return dialogType() == SettingsDialogType_Saved; }
    bool isMachineOnline() const { return dialogType() == SettingsDialogType_Online; }
    bool isMachineInValidMode() const { return isMachineOffline() || isMachineSaved() || isMachineOnline(); }

    /* Page changed: */
    virtual bool changed() const = 0;

    /* Page 'ID' stuff: */
    int id() const { return m_cId; }
    void setId(int cId) { m_cId = cId; }

    /* Page 'processed' stuff: */
    bool processed() const { return m_fProcessed; }
    void setProcessed(bool fProcessed) { m_fProcessed = fProcessed; }

    /* Page 'failed' stuff: */
    bool failed() const { return m_fFailed; }
    void setFailed(bool fFailed) { m_fFailed = fFailed; }

    /* Virtual function to polish page content: */
    virtual void polishPage() {}

protected:

    /* Settings page constructor, hidden: */
    UISettingsPage(UISettingsPageType type);

private:

    /* Private variables: */
    UISettingsPageType m_pageType;
    SettingsDialogType m_dialogType;
    int m_cId;
    bool m_fProcessed;
    bool m_fFailed;
    QWidget *m_pFirstWidget;
};

/* Global settings page class: */
class UISettingsPageGlobal : public UISettingsPage
{
    Q_OBJECT;

protected:

    /* Global settings page constructor, hidden: */
    UISettingsPageGlobal();

    /* Fetch data to m_properties & m_settings: */
    void fetchData(const QVariant &data);

    /* Upload m_properties & m_settings to data: */
    void uploadData(QVariant &data) const;

    /* Page changed: */
    bool changed() const { return false; }

    /* Global data source: */
    CSystemProperties m_properties;
    VBoxGlobalSettings m_settings;
};

/* Machine settings page class: */
class UISettingsPageMachine : public UISettingsPage
{
    Q_OBJECT;

protected:

    /* Machine settings page constructor, hidden: */
    UISettingsPageMachine();

    /* Fetch data to m_machine: */
    void fetchData(const QVariant &data);

    /* Upload m_machine to data: */
    void uploadData(QVariant &data) const;

    /* Machine data source: */
    CMachine m_machine;
    CConsole m_console;
};

#endif // __UISettingsPage_h__

