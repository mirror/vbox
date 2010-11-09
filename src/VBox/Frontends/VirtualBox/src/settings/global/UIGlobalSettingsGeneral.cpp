/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsGeneral class implementation
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

/* Global includes */
#include <QDir>

/* Local includes */
#include "UIGlobalSettingsGeneral.h"
#include "VBoxGlobal.h"

UIGlobalSettingsGeneral::UIGlobalSettingsGeneral()
{
    /* Apply UI decorations */
    Ui::UIGlobalSettingsGeneral::setupUi (this);

#ifndef VBOX_GUI_WITH_SYSTRAY
    mCbCheckTrayIcon->hide();
    mWtSpacer1->hide();
#endif /* !VBOX_GUI_WITH_SYSTRAY */
#ifndef Q_WS_MAC
    mCbCheckPresentationMode->hide();
    mWtSpacer2->hide();
#endif /* !Q_WS_MAC */
//#ifndef Q_WS_WIN /* Checkbox hidden for now! */
    mCbDisableHostScreenSaver->hide();
    mWtSpacer3->hide();
//#endif /* !Q_WS_WIN */

    if (mCbCheckTrayIcon->isHidden() &&
        mCbCheckPresentationMode->isHidden() &&
        mCbDisableHostScreenSaver->isHidden())
        mLnSeparator2->hide();

    mPsMach->setHomeDir (vboxGlobal().virtualBox().GetHomeFolder());
    mPsVRDP->setHomeDir (vboxGlobal().virtualBox().GetHomeFolder());
    mPsVRDP->setMode (VBoxFilePathSelectorWidget::Mode_File_Open);

    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    m_cache.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    m_cache.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
    m_cache.m_fTrayIconEnabled = m_settings.trayIconEnabled();
#ifdef Q_WS_MAC
    m_cache.m_fPresentationModeEnabled = m_settings.presentationModeEnabled();
#endif /* Q_WS_MAC */
    m_cache.m_fHostScreenSaverDisables = m_settings.hostScreenSaverDisabled();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsGeneral::getFromCache()
{
    /* Fetch from cache: */
    mPsMach->setPath(m_cache.m_strDefaultMachineFolder);
    mPsVRDP->setPath(m_cache.m_strVRDEAuthLibrary);
    mCbCheckTrayIcon->setChecked(m_cache.m_fTrayIconEnabled);
#ifdef Q_WS_MAC
    mCbCheckPresentationMode->setChecked(m_cache.m_fPresentationModeEnabled);
#endif /* Q_WS_MAC */
    mCbDisableHostScreenSaver->setChecked(m_cache.m_fHostScreenSaverDisables);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsGeneral::putToCache()
{
    /* Upload to cache: */
    m_cache.m_strDefaultMachineFolder = mPsMach->path();
    m_cache.m_strVRDEAuthLibrary = mPsVRDP->path();
    m_cache.m_fTrayIconEnabled = mCbCheckTrayIcon->isChecked();
#ifdef Q_WS_MAC
    m_cache.m_fPresentationModeEnabled = mCbCheckPresentationMode->isChecked();
#endif /* Q_WS_MAC */
    m_cache.m_fHostScreenSaverDisables = mCbDisableHostScreenSaver->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    if (m_properties.isOk() && mPsMach->isModified())
        m_properties.SetDefaultMachineFolder(m_cache.m_strDefaultMachineFolder);
    if (m_properties.isOk() && mPsVRDP->isModified())
        m_properties.SetVRDEAuthLibrary(m_cache.m_strVRDEAuthLibrary);
    m_settings.setTrayIconEnabled(m_cache.m_fTrayIconEnabled);
#ifdef Q_WS_MAC
    m_settings.setPresentationModeEnabled(m_cache.m_fPresentationModeEnabled);
#endif /* Q_WS_MAC */
    m_settings.setHostScreenSaverDisabled(m_cache.m_fHostScreenSaverDisables);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mPsMach);
    setTabOrder (mPsMach, mPsVRDP);
    setTabOrder (mPsVRDP, mCbCheckTrayIcon);
    setTabOrder (mCbCheckTrayIcon, mCbCheckPresentationMode);
    setTabOrder (mCbCheckPresentationMode, mCbDisableHostScreenSaver);
}

void UIGlobalSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIGlobalSettingsGeneral::retranslateUi (this);

    mPsMach->setWhatsThis (tr ("Displays the path to the default virtual "
                               "machine folder. This folder is used, if not "
                               "explicitly specified otherwise, when creating "
                               "new virtual machines."));
    mPsVRDP->setWhatsThis (tr ("Displays the path to the library that "
                               "provides authentication for Remote Display "
                               "(VRDP) clients."));
}

