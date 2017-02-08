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


/** Global settings: Proxy page cache structure. */
struct UISettingsCacheGlobalProxy
{
    /** Constructs data. */
    UISettingsCacheGlobalProxy()
        : m_enmProxyState(UIProxyManager::ProxyState_Auto)
        , m_strProxyHost(QString())
        , m_strProxyPort(QString())
    {}

    /** Holds the proxy state. */
    UIProxyManager::ProxyState m_enmProxyState;
    /** Holds the proxy host. */
    QString m_strProxyHost;
    /** Holds the proxy port. */
    QString m_strProxyPort;
};


/** Global settings: Proxy page. */
class UIGlobalSettingsProxy : public UISettingsPageGlobal, public Ui::UIGlobalSettingsProxy
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsProxy();

protected:

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* API: Validation stuff: */
    bool validate(QList<UIValidationMessage> &messages);

    /* Helper: Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Helper: Translation stuff: */
    void retranslateUi();

private slots:

    /* Handler: Proxy-checkbox stuff: */
    void sltProxyToggled();

private:

    /* Cache: */
    UISettingsCacheGlobalProxy m_cache;
};

#endif /* !___UIGlobalSettingsProxy_h___ */

