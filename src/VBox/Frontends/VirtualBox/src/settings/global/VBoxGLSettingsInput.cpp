/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsInput class implementation
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

#include "VBoxGLSettingsInput.h"
#include "VBoxGlobalSettings.h"

VBoxGLSettingsInput::VBoxGLSettingsInput()
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsInput::setupUi (this);

    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void VBoxGLSettingsInput::loadToCacheFrom(QVariant &data)
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
void VBoxGLSettingsInput::getFromCache()
{
    /* Fetch from cache: */
    mHeHostKey->setKey(m_cache.m_iHostKey);
    mCbAutoGrab->setChecked(m_cache.m_fAutoCapture);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void VBoxGLSettingsInput::putToCache()
{
    /* Upload to cache: */
    m_cache.m_iHostKey = mHeHostKey->key();
    m_cache.m_fAutoCapture = mCbAutoGrab->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void VBoxGLSettingsInput::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    m_settings.setHostKey(m_cache.m_iHostKey);
    m_settings.setAutoCapture(m_cache.m_fAutoCapture);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void VBoxGLSettingsInput::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mHeHostKey);
    setTabOrder (mHeHostKey, mTbResetHostKey);
    setTabOrder (mTbResetHostKey, mCbAutoGrab);
}

void VBoxGLSettingsInput::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsInput::retranslateUi (this);
}

