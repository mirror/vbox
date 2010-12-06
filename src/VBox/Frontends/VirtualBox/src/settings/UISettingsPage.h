/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsPage class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

/* Global includes */
#include <QWidget>
#include <QVariant>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"
#include "VBoxGlobalSettings.h"

/* Forward declarations */
class QIWidgetValidator;

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
    UISettingsDataMachine(const CMachine &machine)
        : m_machine(machine) {}
    CMachine m_machine;
};
Q_DECLARE_METATYPE(UISettingsDataMachine);

/* Settings page base class: */
class UISettingsPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /* Load data to cashe from corresponding external object(s),
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

    /* Returns settings page type: */
    virtual UISettingsPageType type() const;

    /* Validation stuff: */
    virtual void setValidator(QIWidgetValidator *pValidator);
    virtual bool revalidate(QString &strWarningText, QString &strTitle);

    /* Navigation stuff: */
    virtual void setOrderAfter(QWidget *pWidget);

    /* Page 'ID' stuff: */
    int id() const;
    void setId(int cId);

    /* Page 'processed' stuff: */
    bool processed() const;
    void setProcessed(bool fProcessed);

    /* Page 'failed' stuff: */
    bool failed() const;
    void setFailed(bool fFailed);

protected:

    /* Settings page constructor, hidden: */
    UISettingsPage(UISettingsPageType type, QWidget *pParent = 0);

    /* Variables: */
    UISettingsPageType m_type;
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

    /* Fetch data to m_properties & m_settings: */
    void fetchData(const QVariant &data);

    /* Upload m_properties & m_settings to data: */
    void uploadData(QVariant &data) const;

    /* Global settings page constructor, hidden: */
    UISettingsPageGlobal(QWidget *pParent = 0);

    /* Global data source: */
    CSystemProperties m_properties;
    VBoxGlobalSettings m_settings;
};

/* Machine settings page class: */
class UISettingsPageMachine : public UISettingsPage
{
    Q_OBJECT;

protected:

    /* Fetch data to m_machine: */
    void fetchData(const QVariant &data);

    /* Upload m_machine to data: */
    void uploadData(QVariant &data) const;

    /* Machine settings page constructor, hidden: */
    UISettingsPageMachine(QWidget *pParent = 0);

    /* Machine data source: */
    CMachine m_machine;
};

#endif // __UISettingsPage_h__

