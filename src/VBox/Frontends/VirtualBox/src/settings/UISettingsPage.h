/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsPage class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISettingsPage_h___
#define ___UISettingsPage_h___

/* Qt includes: */
#include <QVariant>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UISettingsDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CMachine.h"
#include "CSystemProperties.h"

/* Forward declarations: */
class QShowEvent;
class QString;
class QVariant;
class QWidget;
class UIPageValidator;

/* Using declarations: */
using namespace UISettingsDefs;


/** Global settings data wrapper. */
struct UISettingsDataGlobal
{
    /** Constructs NULL global settings data struct. */
    UISettingsDataGlobal() {}
    /** Constructs global settings data struct on the basis of @a comProperties. */
    UISettingsDataGlobal(const CSystemProperties &comProperties)
        : m_properties(comProperties) {}
    /** Holds the global VirtualBox properties. */
    CSystemProperties m_properties;
};
Q_DECLARE_METATYPE(UISettingsDataGlobal);


/** Machine settings data wrapper. */
struct UISettingsDataMachine
{
    /** Constructs NULL machine settings data struct. */
    UISettingsDataMachine() {}
    /** Constructs machine settings data struct on the basis of @a comMachine and @a comConsole. */
    UISettingsDataMachine(const CMachine &comMachine, const CConsole &comConsole)
        : m_machine(comMachine), m_console(comConsole) {}
    /** Holds the machine reference. */
    CMachine m_machine;
    /** Holds the console reference. */
    CConsole m_console;
};
Q_DECLARE_METATYPE(UISettingsDataMachine);


/** Validation message. */
typedef QPair<QString, QStringList> UIValidationMessage;


/** QWidget subclass used as settings page interface. */
class SHARED_LIBRARY_STUFF UISettingsPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about particular operation progress change.
      * @param iOperations  holds the number of operations CProgress have,
      * @param strOperation holds the description of the current CProgress operation,
      * @param iOperation   holds the index of the current CProgress operation,
      * @param iPercent     holds the percentage of the current CProgress operation. */
    void sigOperationProgressChange(ulong iOperations, QString strOperation,
                                    ulong iOperation, ulong iPercent);

    /** Notifies listeners about particular COM error.
      * @param strErrorInfo holds the details of the error happened. */
    void sigOperationProgressError(QString strErrorInfo);

public:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) = 0;
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() = 0;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() = 0;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) = 0;

    /** Notifies listeners about particular COM error.
      * @param  strErrorInfo  Brings the details of the error happened. */
    void notifyOperationProgressError(const QString &strErrorInfo);

    /** Defines @a pValidator. */
    void setValidator(UIPageValidator *pValidator);
    /** Defines whether @a fIsValidatorBlocked which means not used at all. */
    void setValidatorBlocked(bool fIsValidatorBlocked) { m_fIsValidatorBlocked = fIsValidatorBlocked; }
    /** Performs page validation composing a list of @a messages. */
    virtual bool validate(QList<UIValidationMessage> &messages) { Q_UNUSED(messages); return true; }

    /** Returns first navigation widget. */
    QWidget *firstWidget() const { return m_pFirstWidget; }
    /** Defines the first navigation widget for TAB-order. */
    virtual void setOrderAfter(QWidget *pWidget) { m_pFirstWidget = pWidget; }

    /** Defines @a enmConfigurationAccessLevel. */
    virtual void setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel);
    /** Returns configuration access level. */
    ConfigurationAccessLevel configurationAccessLevel() const { return m_enmConfigurationAccessLevel; }
    /** Returns whether configuration access level is Full. */
    bool isMachineOffline() const { return configurationAccessLevel() == ConfigurationAccessLevel_Full; }
    /** Returns whether configuration access level corresponds to machine in Powered Off state. */
    bool isMachinePoweredOff() const { return configurationAccessLevel() == ConfigurationAccessLevel_Partial_PoweredOff; }
    /** Returns whether configuration access level corresponds to machine in Saved state. */
    bool isMachineSaved() const { return configurationAccessLevel() == ConfigurationAccessLevel_Partial_Saved; }
    /** Returns whether configuration access level corresponds to machine in one of Running states. */
    bool isMachineOnline() const { return configurationAccessLevel() == ConfigurationAccessLevel_Partial_Running; }
    /** Returns whether configuration access level corresponds to machine in one of allowed states. */
    bool isMachineInValidMode() const { return isMachineOffline() || isMachinePoweredOff() || isMachineSaved() || isMachineOnline(); }

    /** Returns whether the page content was changed. */
    virtual bool changed() const = 0;

    /** Defines page @a cId. */
    void setId(int cId) { m_cId = cId; }
    /** Returns page ID. */
    int id() const { return m_cId; }

    /** Returns page internal name. */
    virtual QString internalName() const = 0;

    /** Returns page warning pixmap. */
    virtual QPixmap warningPixmap() const = 0;

    /** Defines whether page is @a fProcessed. */
    void setProcessed(bool fProcessed) { m_fProcessed = fProcessed; }
    /** Returns whether page is processed. */
    bool processed() const { return m_fProcessed; }

    /** Defines whether page processing is @a fFailed. */
    void setFailed(bool fFailed) { m_fFailed = fFailed; }
    /** Returns whether page processing is failed. */
    bool failed() const { return m_fFailed; }

    /** Performs page polishing. */
    virtual void polishPage() {}

public slots:

    /** Performs validation. */
    void revalidate();

protected:

    /** Constructs settings page. */
    UISettingsPage();

private:

    /** Holds the configuration access level. */
    ConfigurationAccessLevel  m_enmConfigurationAccessLevel;

    /** Holds the page ID. */
    int  m_cId;

    /** Holds the first TAB-orer widget reference. */
    QWidget         *m_pFirstWidget;
    /** Holds the page validator. */
    UIPageValidator *m_pValidator;

    /** Holds whether page validation is blocked. */
    bool  m_fIsValidatorBlocked : 1;
    /** Holds whether page is processed. */
    bool  m_fProcessed : 1;
    /** Holds whether page processing is failed. */
    bool  m_fFailed : 1;
};


/** UISettingsPage extension used as Global Preferences page interface. */
class SHARED_LIBRARY_STUFF UISettingsPageGlobal : public UISettingsPage
{
    Q_OBJECT;

protected:

    /** Constructs global preferences page. */
    UISettingsPageGlobal();

    /** Returns internal page ID. */
    GlobalSettingsPageType internalID() const;

    /** Returns page internal name. */
    virtual QString internalName() const /* override */;

    /** Returns page warning pixmap. */
    virtual QPixmap warningPixmap() const /* override */;

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */ { return false; }

    /** Fetches data to m_properties & m_settings. */
    void fetchData(const QVariant &data);
    /** Uploads m_properties & m_settings to data. */
    void uploadData(QVariant &data) const;

    /** Holds the source of global preferences. */
    CSystemProperties m_properties;
};


/** UISettingsPage extension used as Machine Settings page interface. */
class SHARED_LIBRARY_STUFF UISettingsPageMachine : public UISettingsPage
{
    Q_OBJECT;

protected:

    /** Constructs machine settings page. */
    UISettingsPageMachine();

    /** Returns internal page ID. */
    MachineSettingsPageType internalID() const;

    /** Returns page internal name. */
    virtual QString internalName() const /* override */;

    /** Returns page warning pixmap. */
    virtual QPixmap warningPixmap() const /* override */;

    /** Fetches data to m_machine & m_console. */
    void fetchData(const QVariant &data);
    /** Uploads m_machine & m_console to data. */
    void uploadData(QVariant &data) const;

    /** Holds the source of machine settings. */
    CMachine m_machine;
    /** Holds the source of console settings. */
    CConsole m_console;
};


#endif /* !___UISettingsPage_h___ */
