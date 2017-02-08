/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsGeneral class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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

/* Qt includes: */
# include <QDir>

/* GUI includes: */
# include "UIGlobalSettingsGeneral.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGlobalSettingsGeneral::UIGlobalSettingsGeneral()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsGeneral::setupUi(this);

    /* Hide checkbox for now: */
    m_pLabelHostScreenSaver->hide();
    m_pCheckBoxHostScreenSaver->hide();

    /* Setup widgets: */
    m_pSelectorMachineFolder->setHomeDir(vboxGlobal().homeFolder());
    m_pSelectorVRDPLibName->setHomeDir(vboxGlobal().homeFolder());
    m_pSelectorVRDPLibName->setMode(UIFilePathSelector::Mode_File_Open);

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare old data: */
    UIDataSettingsGlobalGeneral oldData;

    /* Gather old data: */
    oldData.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    oldData.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
    oldData.m_fHostScreenSaverDisabled = m_settings.hostScreenSaverDisabled();

    /* Cache old data: */
    m_cache.cacheInitialData(oldData);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::getFromCache()
{
    /* Get old data from cache: */
    const UIDataSettingsGlobalGeneral &oldData = m_cache.base();

    /* Load old data from cache: */
    m_pSelectorMachineFolder->setPath(oldData.m_strDefaultMachineFolder);
    m_pSelectorVRDPLibName->setPath(oldData.m_strVRDEAuthLibrary);
    m_pCheckBoxHostScreenSaver->setChecked(oldData.m_fHostScreenSaverDisabled);
}

void UIGlobalSettingsGeneral::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalGeneral newData = m_cache.base();

    /* Gather new data: */
    newData.m_strDefaultMachineFolder = m_pSelectorMachineFolder->path();
    newData.m_strVRDEAuthLibrary = m_pSelectorVRDPLibName->path();
    newData.m_fHostScreenSaverDisabled = m_pCheckBoxHostScreenSaver->isChecked();

    /* Cache new data: */
    m_cache.cacheCurrentData(newData);
}

void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save new data from cache: */
    if (m_cache.wasChanged())
    {
        if (   m_properties.isOk()
            && m_cache.data().m_strDefaultMachineFolder != m_cache.base().m_strDefaultMachineFolder)
            m_properties.SetDefaultMachineFolder(m_cache.data().m_strDefaultMachineFolder);
        if (   m_properties.isOk()
            && m_cache.data().m_strVRDEAuthLibrary != m_cache.base().m_strVRDEAuthLibrary)
            m_properties.SetVRDEAuthLibrary(m_cache.data().m_strVRDEAuthLibrary);
        if (m_cache.data().m_fHostScreenSaverDisabled != m_cache.base().m_fHostScreenSaverDisabled)
            m_settings.setHostScreenSaverDisabled(m_cache.data().m_fHostScreenSaverDisabled);
    }

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation: */
    setTabOrder(pWidget, m_pSelectorMachineFolder);
    setTabOrder(m_pSelectorMachineFolder, m_pSelectorVRDPLibName);
    setTabOrder(m_pSelectorVRDPLibName, m_pCheckBoxHostScreenSaver);
}

void UIGlobalSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsGeneral::retranslateUi(this);
}

