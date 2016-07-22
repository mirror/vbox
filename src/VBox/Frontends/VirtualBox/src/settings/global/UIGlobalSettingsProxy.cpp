/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class implementation.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
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
    QButtonGroup *pButtonGroup = new QButtonGroup(this);
    pButtonGroup->addButton(m_pRadioProxyAuto);
    pButtonGroup->addButton(m_pRadioProxyDisabled);
    pButtonGroup->addButton(m_pRadioProxyEnabled);
    m_pPortEditor->setFixedWidthByText(QString().fill('0', 6));
    m_pHostEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pHostEditor));
    m_pPortEditor->setValidator(new QRegExpValidator(QRegExp("\\d+"), m_pPortEditor));

    /* Setup connections: */
    connect(pButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(sltProxyToggled()));
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
    m_cache.m_enmProxyState = proxyManager.proxyState();
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
    switch (m_cache.m_enmProxyState)
    {
        case UIProxyManager::ProxyState_Auto:     m_pRadioProxyAuto->setChecked(true); break;
        case UIProxyManager::ProxyState_Disabled: m_pRadioProxyDisabled->setChecked(true); break;
        case UIProxyManager::ProxyState_Enabled:  m_pRadioProxyEnabled->setChecked(true); break;
    }
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
    m_cache.m_enmProxyState = m_pRadioProxyEnabled->isChecked()  ? UIProxyManager::ProxyState_Enabled :
                              m_pRadioProxyDisabled->isChecked() ? UIProxyManager::ProxyState_Disabled :
                                                                   UIProxyManager::ProxyState_Auto;
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
    proxyManager.setProxyState(m_cache.m_enmProxyState);
    proxyManager.setProxyHost(m_cache.m_strProxyHost);
    proxyManager.setProxyPort(m_cache.m_strProxyPort);
    m_settings.setProxySettings(proxyManager.toString());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsProxy::validate(QList<UIValidationMessage> &messages)
{
    /* Pass if proxy is disabled: */
    if (!m_pRadioProxyEnabled->isChecked())
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
    setTabOrder(pWidget, m_pRadioProxyAuto);
    setTabOrder(m_pRadioProxyAuto, m_pRadioProxyDisabled);
    setTabOrder(m_pRadioProxyDisabled, m_pRadioProxyEnabled);
    setTabOrder(m_pRadioProxyEnabled, m_pHostEditor);
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
    m_pContainerProxy->setEnabled(m_pRadioProxyEnabled->isChecked());

    /* Revalidate: */
    revalidate();
}

