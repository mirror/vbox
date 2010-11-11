/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsInput class implementation
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
#include "UIGlobalSettingsInput.h"
#include "VBoxGlobalSettings.h"

/* Input page constructor: */
UIGlobalSettingsInput::UIGlobalSettingsInput()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsInput::setupUi (this);

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    m_cache.m_iHostKey = m_settings.hostKey();
    m_cache.m_fAutoCapture = m_settings.autoCapture();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::getFromCache()
{
    /* Fetch from cache: */
    m_pHostKeyEditor->setKey(m_cache.m_iHostKey);
    m_pEnableAutoGrabCheckbox->setChecked(m_cache.m_fAutoCapture);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::putToCache()
{
    /* Upload to cache: */
    m_cache.m_iHostKey = m_pHostKeyEditor->key();
    m_cache.m_fAutoCapture = m_pEnableAutoGrabCheckbox->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    m_settings.setHostKey(m_cache.m_iHostKey);
    m_settings.setAutoCapture(m_cache.m_fAutoCapture);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Navigation stuff: */
void UIGlobalSettingsInput::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pHostKeyEditor);
    setTabOrder(m_pHostKeyEditor, m_pResetHostKeyButton);
    setTabOrder(m_pResetHostKeyButton, m_pEnableAutoGrabCheckbox);
}

/* Translation stuff: */
void UIGlobalSettingsInput::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsInput::retranslateUi (this);
}

