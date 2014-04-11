/** @file
 * VBox Qt GUI - UIExtraDataManager class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIExtraDataManager_h___
#define ___UIExtraDataManager_h___

/* Qt includes: */
#include <QObject>
#include <QMap>

/* COM includes: */
#include "CEventListener.h"

/* Forward declarations: */
class UIExtraDataEventHandler;

/* Type definitions: */
typedef QMap<QString, QString> ExtraDataMap;

/** Singleton QObject extension
  * providing GUI with corresponding extra-data values,
  * and notifying it whenever any of those values changed. */
class UIExtraDataManager : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about GUI language change. */
    void sigLanguageChange(QString strLanguage);

    /** Notifies about Selector UI keyboard shortcut change. */
    void sigSelectorUIShortcutChange();
    /** Notifies about Runtime UI keyboard shortcut change. */
    void sigRuntimeUIShortcutChange();

    /** Notifies about HID LED sync state change. */
    void sigHIDLedsSyncStateChange(bool fEnabled);

#ifdef RT_OS_DARWIN
    /** Mac OS X: Notifies about 'presentation mode' status change. */
    void sigPresentationModeChange(bool fEnabled);
    /** Mac OS X: Notifies about 'dock icon' appearance change. */
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

public:

    /** Static Extra-data Manager instance/constructor. */
    static UIExtraDataManager* instance();
    /** Static Extra-data Manager destructor. */
    static void destroy();

private:

    /** Extra-data Manager constructor. */
    UIExtraDataManager();
    /** Extra-data Manager destructor. */
    ~UIExtraDataManager();

    /** Prepare Extra-data Manager. */
    void prepare();
    /** Prepare Main event-listener. */
    void prepareMainEventListener();
    /** Prepare extra-data event-handler. */
    void prepareExtraDataEventHandler();
    /** Prepare global extra-data map. */
    void prepareGlobalExtraDataMap();

    /** Cleanup Extra-data Manager. */
    void cleanup();
    /** Cleanup Main event-listener. */
    void cleanupMainEventListener();
    // /** Cleanup extra-data event-handler. */
    // void cleanupExtraDataEventHandler();
    // /** Cleanup extra-data map. */
    // void cleanupExtraDataMap();

    /** Hot-load machine extra-data map. */
    void hotloadMachineExtraDataMap(const QString &strID) const;

    /** Determines whether feature corresponding to passed @a strKey is allowed.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    bool isFeatureAllowed(const QString &strKey, const QString &strID = QString()) const;

    /** Returns extra-data value corresponding to passed @a strKey as QString.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QString extraDataString(const QString &strKey, const QString &strID = QString()) const;

    /** Returns extra-data value corresponding to passed @a strKey as QStringList.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QStringList extraDataStringList(const QString &strKey, const QString &strID = QString()) const;

    /** Singleton Extra-data Manager instance. */
    static UIExtraDataManager *m_pInstance;

    /** Main event-listener instance. */
    CEventListener m_listener;
    /** Extra-data event-handler instance. */
    UIExtraDataEventHandler *m_pHandler;

    /** Extra-data map. */
    mutable QMap<QString, ExtraDataMap> m_data;
};

/** Singleton Extra-data Manager 'official' name. */
#define gEDataManager UIExtraDataManager::instance()

#endif /* !___UIExtraDataManager_h___ */
