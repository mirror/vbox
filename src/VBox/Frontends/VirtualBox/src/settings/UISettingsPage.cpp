/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsPage class implementation
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

/* Local includes */
#include "UISettingsPage.h"

/* Returns settings page type: */
UISettingsPageType UISettingsPage::type() const
{
    return m_type;
}

/* Validation stuff: */
void UISettingsPage::setValidator(QIWidgetValidator *pValidator)
{
    Q_UNUSED(pValidator);
}

/* Validation stuff: */
bool UISettingsPage::revalidate(QString &strWarningText, QString &strTitle)
{
    Q_UNUSED(strWarningText);
    Q_UNUSED(strTitle);
    return true;
}

/* Navigation stuff: */
void UISettingsPage::setOrderAfter(QWidget *pWidget)
{
    m_pFirstWidget = pWidget;
}

/* Page 'ID' stuff: */
int UISettingsPage::id() const
{
    return m_cId;
}

/* Page 'ID' stuff: */
void UISettingsPage::setId(int cId)
{
    m_cId = cId;
}

/* Page 'processed' stuff: */
bool UISettingsPage::processed() const
{
    return m_fProcessed;
}

/* Page 'processed' stuff: */
void UISettingsPage::setProcessed(bool fProcessed)
{
    m_fProcessed = fProcessed;
}

/* Page 'failed' stuff: */
bool UISettingsPage::failed() const
{
    return m_fFailed;
}

/* Page 'failed' stuff: */
void UISettingsPage::setFailed(bool fFailed)
{
    m_fFailed = fFailed;
}

/* Settings page constructor, hidden: */
UISettingsPage::UISettingsPage(UISettingsPageType type, QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_type(type)
    , m_cId(-1)
    , m_fProcessed(false)
    , m_fFailed(false)
    , m_pFirstWidget(0)
{
}

/* Fetch data to m_properties & m_settings: */
void UISettingsPageGlobal::fetchData(const QVariant &data)
{
    m_properties = data.value<UISettingsDataGlobal>().m_properties;
    m_settings = data.value<UISettingsDataGlobal>().m_settings;
}

/* Upload m_properties & m_settings to data: */
void UISettingsPageGlobal::uploadData(QVariant &data) const
{
    data = QVariant::fromValue(UISettingsDataGlobal(m_properties, m_settings));
}

/* Global settings page constructor, hidden: */
UISettingsPageGlobal::UISettingsPageGlobal(QWidget *pParent)
    : UISettingsPage(UISettingsPageType_Global, pParent)
{
}

/* Fetch data to m_machine: */
void UISettingsPageMachine::fetchData(const QVariant &data)
{
    m_machine = data.value<UISettingsDataMachine>().m_machine;
}

/* Upload m_machine to data: */
void UISettingsPageMachine::uploadData(QVariant &data) const
{
    data = QVariant::fromValue(UISettingsDataMachine(m_machine));
}

/* Machine settings page constructor, hidden: */
UISettingsPageMachine::UISettingsPageMachine(QWidget *pParent)
    : UISettingsPage(UISettingsPageType_Machine, pParent)
{
}

