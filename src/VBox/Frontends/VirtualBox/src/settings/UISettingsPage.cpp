/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsPage class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

/* Settings page constructor, hidden: */
UISettingsPage::UISettingsPage(UISettingsPageType pageType)
    : m_pageType(pageType)
    , m_dialogType(SettingsDialogType_Wrong)
    , m_cId(-1)
    , m_fProcessed(false)
    , m_fFailed(false)
    , m_pFirstWidget(0)
{
}

/* Global settings page constructor, hidden: */
UISettingsPageGlobal::UISettingsPageGlobal()
    : UISettingsPage(UISettingsPageType_Global)
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

/* Machine settings page constructor, hidden: */
UISettingsPageMachine::UISettingsPageMachine()
    : UISettingsPage(UISettingsPageType_Machine)
{
}

/* Fetch data to m_machine & m_console: */
void UISettingsPageMachine::fetchData(const QVariant &data)
{
    m_machine = data.value<UISettingsDataMachine>().m_machine;
    m_console = data.value<UISettingsDataMachine>().m_console;
}

/* Upload m_machine & m_console to data: */
void UISettingsPageMachine::uploadData(QVariant &data) const
{
    data = QVariant::fromValue(UISettingsDataMachine(m_machine, m_console));
}

