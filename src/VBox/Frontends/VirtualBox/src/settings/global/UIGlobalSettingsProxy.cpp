/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsProxy class implementation
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

/* Global includes */
#include <QRegExpValidator>

/* Local includes */
#include "QIWidgetValidator.h"
#include "UIGlobalSettingsProxy.h"
#include "VBoxUtils.h"

/* General page constructor: */
UIGlobalSettingsProxy::UIGlobalSettingsProxy()
    : m_pValidator(0)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsProxy::setupUi(this);

    /* Setup widgets: */
    m_pPortEditor->setFixedWidthByText(QString().fill('0', 6));
    m_pHostEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pHostEditor));
    m_pPortEditor->setValidator(new QRegExpValidator(QRegExp("\\d+"), m_pPortEditor));
    m_pLoginEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pLoginEditor));
    m_pPasswordEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pPasswordEditor));

    /* Setup connections: */
    connect(m_pProxyCheckbox, SIGNAL(stateChanged(int)), this, SLOT(sltProxyToggled()));
    connect(m_pAuthCheckbox, SIGNAL(stateChanged(int)), this, SLOT(sltAuthToggled()));

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
    m_cache.m_fAuthEnabled = proxyManager.authEnabled();
    m_cache.m_strAuthLogin = proxyManager.authLogin();
    m_cache.m_strAuthPassword = proxyManager.authPassword();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsProxy::getFromCache()
{
    /* Fetch from cache: */
    m_pProxyCheckbox->setChecked(m_cache.m_fProxyEnabled);
    m_pHostEditor->setText(m_cache.m_strProxyHost);
    m_pPortEditor->setText(m_cache.m_strProxyPort);
    m_pAuthCheckbox->setChecked(m_cache.m_fAuthEnabled);
    m_pLoginEditor->setText(m_cache.m_strAuthLogin);
    m_pPasswordEditor->setText(m_cache.m_strAuthPassword);
    sltProxyToggled();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsProxy::putToCache()
{
    /* Upload to cache: */
    m_cache.m_fProxyEnabled = m_pProxyCheckbox->isChecked();
    m_cache.m_strProxyHost = m_pHostEditor->text();
    m_cache.m_strProxyPort = m_pPortEditor->text();
    m_cache.m_fAuthEnabled = m_pAuthCheckbox->isChecked();
    m_cache.m_strAuthLogin = m_pLoginEditor->text();
    m_cache.m_strAuthPassword = m_pPasswordEditor->text();
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
    proxyManager.setAuthEnabled(m_cache.m_fAuthEnabled);
    proxyManager.setAuthLogin(m_cache.m_strAuthLogin);
    proxyManager.setAuthPassword(m_cache.m_strAuthPassword);
    m_settings.setProxySettings(proxyManager.toString());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Validation stuff: */
void UIGlobalSettingsProxy::setValidator(QIWidgetValidator *pValidator)
{
    m_pValidator = pValidator;
}

/* Navigation stuff: */
void UIGlobalSettingsProxy::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pProxyCheckbox);
    setTabOrder(m_pProxyCheckbox, m_pHostEditor);
    setTabOrder(m_pHostEditor, m_pPortEditor);
    setTabOrder(m_pPortEditor, m_pAuthCheckbox);
    setTabOrder(m_pAuthCheckbox, m_pLoginEditor);
    setTabOrder(m_pLoginEditor, m_pPasswordEditor);
}

/* Translation stuff: */
void UIGlobalSettingsProxy::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsProxy::retranslateUi(this);
}

void UIGlobalSettingsProxy::sltProxyToggled()
{
    /* Update widgets availability: */
    m_pHostLabel->setEnabled(m_pProxyCheckbox->isChecked());
    m_pHostEditor->setEnabled(m_pProxyCheckbox->isChecked());
    m_pPortLabel->setEnabled(m_pProxyCheckbox->isChecked());
    m_pPortEditor->setEnabled(m_pProxyCheckbox->isChecked());
    m_pAuthCheckbox->setEnabled(m_pProxyCheckbox->isChecked());

    /* Update auth widgets also: */
    sltAuthToggled();
}

void UIGlobalSettingsProxy::sltAuthToggled()
{
    /* Update widgets availability: */
    m_pLoginLabel->setEnabled(m_pProxyCheckbox->isChecked() && m_pAuthCheckbox->isChecked());
    m_pLoginEditor->setEnabled(m_pProxyCheckbox->isChecked() && m_pAuthCheckbox->isChecked());
    m_pPasswordLabel->setEnabled(m_pProxyCheckbox->isChecked() && m_pAuthCheckbox->isChecked());
    m_pPasswordEditor->setEnabled(m_pProxyCheckbox->isChecked() && m_pAuthCheckbox->isChecked());

    /* Revalidate if possible: */
    if (m_pValidator)
        m_pValidator->revalidate();
}

