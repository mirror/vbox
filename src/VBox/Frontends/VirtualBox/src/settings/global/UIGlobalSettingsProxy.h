/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class declaration.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGlobalSettingsProxy_h___
#define ___UIGlobalSettingsProxy_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsProxy.gen.h"
#include "VBoxUtils.h"


/** Global settings: Proxy page data structure. */
struct UIDataSettingsGlobalProxy
{
    /** Constructs data. */
    UIDataSettingsGlobalProxy()
        : m_enmProxyState(UIProxyManager::ProxyState_Auto)
        , m_strProxyHost(QString())
        , m_strProxyPort(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalProxy &other) const
    {
        return true
               && (m_enmProxyState == other.m_enmProxyState)
               && (m_strProxyHost == other.m_strProxyHost)
               && (m_strProxyPort == other.m_strProxyPort)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalProxy &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalProxy &other) const { return !equal(other); }

    /** Holds the proxy state. */
    UIProxyManager::ProxyState m_enmProxyState;
    /** Holds the proxy host. */
    QString m_strProxyHost;
    /** Holds the proxy port. */
    QString m_strProxyPort;
};
typedef UISettingsCache<UIDataSettingsGlobalProxy> UISettingsCacheGlobalProxy;


/** Global settings: Proxy page. */
class UIGlobalSettingsProxy : public UISettingsPageGlobal, public Ui::UIGlobalSettingsProxy
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsProxy();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /* Handler: Proxy-checkbox stuff: */
    void sltProxyToggled();

private:

    /* Cache: */
    UISettingsCacheGlobalProxy m_cache;
};

#endif /* !___UIGlobalSettingsProxy_h___ */

