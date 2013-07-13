/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsGeneral class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

/* General page constructor: */
UIGlobalSettingsGeneral::UIGlobalSettingsGeneral()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsGeneral::setupUi(this);

#ifndef Q_WS_MAC
    m_pPresentationModeLabel->hide();
    m_pPresentationModeCheckbox->hide();
#endif /* !Q_WS_MAC */
    /* Hide checkbox for now: */
    m_pHostScreenSaverLabel->hide();
    m_pHostScreenSaverCheckbox->hide();

    /* Setup widgets: */
    m_pMachineFolderSelector->setHomeDir(vboxGlobal().virtualBox().GetHomeFolder());
    m_pVRDPLibNameSelector->setHomeDir(vboxGlobal().virtualBox().GetHomeFolder());
    m_pVRDPLibNameSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Open);

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    m_cache.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    m_cache.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
#ifdef Q_WS_MAC
    m_cache.m_fPresentationModeEnabled = m_settings.presentationModeEnabled();
#endif /* Q_WS_MAC */
    m_cache.m_fHostScreenSaverDisabled = m_settings.hostScreenSaverDisabled();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsGeneral::getFromCache()
{
    /* Fetch from cache: */
    m_pMachineFolderSelector->setPath(m_cache.m_strDefaultMachineFolder);
    m_pVRDPLibNameSelector->setPath(m_cache.m_strVRDEAuthLibrary);
#ifdef Q_WS_MAC
    m_pPresentationModeCheckbox->setChecked(m_cache.m_fPresentationModeEnabled);
#endif /* Q_WS_MAC */
    m_pHostScreenSaverCheckbox->setChecked(m_cache.m_fHostScreenSaverDisabled);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsGeneral::putToCache()
{
    /* Upload to cache: */
    m_cache.m_strDefaultMachineFolder = m_pMachineFolderSelector->path();
    m_cache.m_strVRDEAuthLibrary = m_pVRDPLibNameSelector->path();
#ifdef Q_WS_MAC
    m_cache.m_fPresentationModeEnabled = m_pPresentationModeCheckbox->isChecked();
#endif /* Q_WS_MAC */
    m_cache.m_fHostScreenSaverDisabled = m_pHostScreenSaverCheckbox->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    if (m_properties.isOk() && m_pMachineFolderSelector->isModified())
        m_properties.SetDefaultMachineFolder(m_cache.m_strDefaultMachineFolder);
    if (m_properties.isOk() && m_pVRDPLibNameSelector->isModified())
        m_properties.SetVRDEAuthLibrary(m_cache.m_strVRDEAuthLibrary);
#ifdef Q_WS_MAC
    m_settings.setPresentationModeEnabled(m_cache.m_fPresentationModeEnabled);
#endif /* Q_WS_MAC */
    m_settings.setHostScreenSaverDisabled(m_cache.m_fHostScreenSaverDisabled);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Navigation stuff: */
void UIGlobalSettingsGeneral::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pMachineFolderSelector);
    setTabOrder(m_pMachineFolderSelector, m_pVRDPLibNameSelector);
    setTabOrder(m_pVRDPLibNameSelector, m_pPresentationModeCheckbox);
    setTabOrder(m_pPresentationModeCheckbox, m_pHostScreenSaverCheckbox);
}

/* Translation stuff: */
void UIGlobalSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsGeneral::retranslateUi(this);
}

