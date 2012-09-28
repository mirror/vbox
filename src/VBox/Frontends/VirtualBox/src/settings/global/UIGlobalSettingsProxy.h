/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsProxy class declaration
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsProxy_h__
#define __UIGlobalSettingsProxy_h__

/* Local includes */
#include "UISettingsPage.h"
#include "UIGlobalSettingsProxy.gen.h"

/* Global settings / Proxy page / Cache: */
struct UISettingsCacheGlobalProxy
{
    UISettingsCacheGlobalProxy() : m_fProxyEnabled(false), m_fAuthEnabled(false) {}
    bool m_fProxyEnabled;
    QString m_strProxyHost;
    QString m_strProxyPort;
    bool m_fAuthEnabled;
    QString m_strAuthLogin;
    QString m_strAuthPassword;
};

/* Global settings / Proxy page: */
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

    /* Validation stuff: */
    void setValidator(QIWidgetValidator *pValidator);

    /* Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Translation stuff: */
    void retranslateUi();

private slots:

    void sltProxyToggled();
    void sltAuthToggled();

private:

    /* Validator: */
    QIWidgetValidator *m_pValidator;

    /* Cache: */
    UISettingsCacheGlobalProxy m_cache;
};

#endif // __UIGlobalSettingsProxy_h__

