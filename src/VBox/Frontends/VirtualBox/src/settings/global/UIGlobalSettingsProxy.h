/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class declaration.
 */

/*
 * Copyright (C) 2011-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsProxy_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsProxy_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"
#include "VBoxUtils.h"

/* Forward declarations: */
class QButtonGroup;
class QLabel;
class QRadioButton;
class QILineEdit;
struct UIDataSettingsGlobalProxy;
typedef UISettingsCache<UIDataSettingsGlobalProxy> UISettingsCacheGlobalProxy;

/** Global settings: Proxy page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsProxy : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs Proxy settings page. */
    UIGlobalSettingsProxy();
    /** Destructs Proxy settings page. */
    ~UIGlobalSettingsProxy();

protected:

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles proxy toggling. */
    void sltHandleProxyToggle();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares wıdgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing proxy data from cache. */
    bool saveData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalProxy *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the button-group instance. */
        QButtonGroup *m_pButtonGroup;
        /** Holds the 'proxy auto' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyAuto;
        /** Holds the 'proxy disabled' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyDisabled;
        /** Holds the 'proxy enabled' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyEnabled;
        /** Holds the settings widget instance. */
        QWidget      *m_pWidgetSettings;
        /** Holds the host label instance. */
        QLabel       *m_pLabelHost;
        /** Holds the host editor instance. */
        QILineEdit   *m_pEditorHost;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsProxy_h */
