/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class implementation.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Global includes */
# include <QRegExpValidator>

/* Local includes */
# include "QIWidgetValidator.h"
# include "UIGlobalSettingsProxy.h"
# include "VBoxUtils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* General page constructor: */
UIGlobalSettingsProxy::UIGlobalSettingsProxy()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsProxy::setupUi(this);

    /* Setup widgets: */
    m_pPortEditor->setFixedWidthByText(QString().fill('0', 6));
    m_pHostEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pHostEditor));
    m_pPortEditor->setValidator(new QRegExpValidator(QRegExp("\\d+"), m_pPortEditor));

    /* Setup connections: */
    connect(m_pCheckboxProxy, SIGNAL(toggled(bool)), this, SLOT(sltProxyToggled()));
    connect(m_pHostEditor, SIGNAL(textEdited(const QString&)), this, SLOT(revalidate()));
    connect(m_pPortEditor, SIGNAL(textEdited(const QString&)), this, SLOT(revalidate()));

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsProxy::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    UIProxyManager proxyManager(m_settings.proxySettings());
    m_cache.m_fProxyEnabled = proxyManager.proxyEnabled();
    m_cache.m_strProxyHost = proxyManager.proxyHost();
    m_cache.m_strProxyPort = proxyManager.proxyPort();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsProxy::getFromCache()
{
    /* Fetch from cache: */
    m_pCheckboxProxy->setChecked(m_cache.m_fProxyEnabled);
    m_pHostEditor->setText(m_cache.m_strProxyHost);
    m_pPortEditor->setText(m_cache.m_strProxyPort);
    sltProxyToggled();

    /* Revalidate: */
    revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsProxy::putToCache()
{
    /* Upload to cache: */
    m_cache.m_fProxyEnabled = m_pCheckboxProxy->isChecked();
    m_cache.m_strProxyHost = m_pHostEditor->text();
    m_cache.m_strProxyPort = m_pPortEditor->text();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsProxy::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    UIProxyManager proxyManager;
    proxyManager.setProxyEnabled(m_cache.m_fProxyEnabled);
    proxyManager.setProxyHost(m_cache.m_strProxyHost);
    proxyManager.setProxyPort(m_cache.m_strProxyPort);
    m_settings.setProxySettings(proxyManager.toString());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsProxy::validate(QList<UIValidationMessage> &messages)
{
    /* Pass if proxy is disabled: */
    if (!m_pCheckboxProxy->isChecked())
        return true;

    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* Check for host value: */
    if (m_pHostEditor->text().trimmed().isEmpty())
    {
        message.second << tr("No proxy host is currently specified.");
        fPass = false;
    }

    /* Check for port value: */
    if (m_pPortEditor->text().trimmed().isEmpty())
    {
        message.second << tr("No proxy port is currently specified.");
        fPass = false;
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsProxy::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation: */
    setTabOrder(pWidget, m_pCheckboxProxy);
    setTabOrder(m_pCheckboxProxy, m_pHostEditor);
    setTabOrder(m_pHostEditor, m_pPortEditor);
}

void UIGlobalSettingsProxy::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsProxy::retranslateUi(this);
}

void UIGlobalSettingsProxy::sltProxyToggled()
{
    /* Update widgets availability: */
    m_pContainerProxy->setEnabled(m_pCheckboxProxy->isChecked());

    /* Revalidate: */
    revalidate();
}

