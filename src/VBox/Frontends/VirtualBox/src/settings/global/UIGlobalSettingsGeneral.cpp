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


/** Global settings: General page data structure. */
struct UIDataSettingsGlobalGeneral
{
    /** Constructs data. */
    UIDataSettingsGlobalGeneral()
        : m_strDefaultMachineFolder(QString())
        , m_strVRDEAuthLibrary(QString())
        , m_fHostScreenSaverDisabled(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalGeneral &other) const
    {
        return true
               && (m_strDefaultMachineFolder == other.m_strDefaultMachineFolder)
               && (m_strVRDEAuthLibrary == other.m_strVRDEAuthLibrary)
               && (m_fHostScreenSaverDisabled == other.m_fHostScreenSaverDisabled)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalGeneral &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalGeneral &other) const { return !equal(other); }

    /** Holds the default machine folder path. */
    QString m_strDefaultMachineFolder;
    /** Holds the VRDE authentication library name. */
    QString m_strVRDEAuthLibrary;
    /** Holds whether host screen-saver should be disabled. */
    bool m_fHostScreenSaverDisabled;
};


UIGlobalSettingsGeneral::UIGlobalSettingsGeneral()
    : m_pCache(new UISettingsCacheGlobalGeneral)
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

UIGlobalSettingsGeneral::~UIGlobalSettingsGeneral()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old data: */
    UIDataSettingsGlobalGeneral oldData;

    /* Gather old data: */
    oldData.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    oldData.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
    oldData.m_fHostScreenSaverDisabled = m_settings.hostScreenSaverDisabled();

    /* Cache old data: */
    m_pCache->cacheInitialData(oldData);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::getFromCache()
{
    /* Get old data from cache: */
    const UIDataSettingsGlobalGeneral &oldData = m_pCache->base();

    /* Load old data from cache: */
    m_pSelectorMachineFolder->setPath(oldData.m_strDefaultMachineFolder);
    m_pSelectorVRDPLibName->setPath(oldData.m_strVRDEAuthLibrary);
    m_pCheckBoxHostScreenSaver->setChecked(oldData.m_fHostScreenSaverDisabled);
}

void UIGlobalSettingsGeneral::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalGeneral newData = m_pCache->base();

    /* Gather new data: */
    newData.m_strDefaultMachineFolder = m_pSelectorMachineFolder->path();
    newData.m_strVRDEAuthLibrary = m_pSelectorVRDPLibName->path();
    newData.m_fHostScreenSaverDisabled = m_pCheckBoxHostScreenSaver->isChecked();

    /* Cache new data: */
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save new data from cache: */
    if (m_pCache->wasChanged())
    {
        if (   m_properties.isOk()
            && m_pCache->data().m_strDefaultMachineFolder != m_pCache->base().m_strDefaultMachineFolder)
            m_properties.SetDefaultMachineFolder(m_pCache->data().m_strDefaultMachineFolder);
        if (   m_properties.isOk()
            && m_pCache->data().m_strVRDEAuthLibrary != m_pCache->base().m_strVRDEAuthLibrary)
            m_properties.SetVRDEAuthLibrary(m_pCache->data().m_strVRDEAuthLibrary);
        if (m_pCache->data().m_fHostScreenSaverDisabled != m_pCache->base().m_fHostScreenSaverDisabled)
            m_settings.setHostScreenSaverDisabled(m_pCache->data().m_fHostScreenSaverDisabled);
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

