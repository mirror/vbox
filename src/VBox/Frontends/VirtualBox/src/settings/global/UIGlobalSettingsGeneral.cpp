/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsGeneral class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QDir>
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UICommon.h"
#include "UIErrorString.h"
#include "UIExtraDataManager.h"
#include "UIFilePathSelector.h"
#include "UIGlobalSettingsGeneral.h"


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
    : m_pCache(0)
    , m_pLabelMachineFolder(0)
    , m_pSelectorMachineFolder(0)
    , m_pLabelVRDPLibName(0)
    , m_pSelectorVRDPLibName(0)
    , m_pLabelHostScreenSaver(0)
    , m_pCheckBoxHostScreenSaver(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsGeneral::~UIGlobalSettingsGeneral()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old general data: */
    UIDataSettingsGlobalGeneral oldGeneralData;

    /* Gather old general data: */
    oldGeneralData.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    oldGeneralData.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
    oldGeneralData.m_fHostScreenSaverDisabled = gEDataManager->hostScreenSaverDisabled();

    /* Cache old general data: */
    m_pCache->cacheInitialData(oldGeneralData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::getFromCache()
{
    /* Get old general data from the cache: */
    const UIDataSettingsGlobalGeneral &oldGeneralData = m_pCache->base();

    /* Load old general data from the cache: */
    m_pSelectorMachineFolder->setPath(oldGeneralData.m_strDefaultMachineFolder);
    m_pSelectorVRDPLibName->setPath(oldGeneralData.m_strVRDEAuthLibrary);
    m_pCheckBoxHostScreenSaver->setChecked(oldGeneralData.m_fHostScreenSaverDisabled);
}

void UIGlobalSettingsGeneral::putToCache()
{
    /* Prepare new general data: */
    UIDataSettingsGlobalGeneral newGeneralData = m_pCache->base();

    /* Gather new general data: */
    newGeneralData.m_strDefaultMachineFolder = m_pSelectorMachineFolder->path();
    newGeneralData.m_strVRDEAuthLibrary = m_pSelectorVRDPLibName->path();
    newGeneralData.m_fHostScreenSaverDisabled = m_pCheckBoxHostScreenSaver->isChecked();

    /* Cache new general data: */
    m_pCache->cacheCurrentData(newGeneralData);
}

void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update general data and failing state: */
    setFailed(!saveGeneralData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::retranslateUi()
{
    m_pLabelMachineFolder->setText(tr("Default &Machine Folder:"));
    m_pSelectorMachineFolder->setWhatsThis(tr("Holds the path to the default virtual machine folder. This folder is used, "
                                              "if not explicitly specified otherwise, when creating new virtual machines."));
    m_pLabelVRDPLibName->setText(tr("V&RDP Authentication Library:"));
    m_pSelectorVRDPLibName->setWhatsThis(tr("Holds the path to the library that provides authentication for Remote Display (VRDP) clients."));
    m_pLabelHostScreenSaver->setText(tr("Host Screensaver:"));
    m_pCheckBoxHostScreenSaver->setWhatsThis(tr("When checked, the host screensaver will be disabled whenever a virtual machine is running."));
    m_pCheckBoxHostScreenSaver->setText(tr("&Disable When Running Virtual Machines"));
}

void UIGlobalSettingsGeneral::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalGeneral;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsGeneral::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setContentsMargins(0, 0, 0, 0);
        pLayoutMain->setColumnStretch(1, 1);
        pLayoutMain->setRowStretch(3, 1);

        /* Prepare machine folder label: */
        m_pLabelMachineFolder = new QLabel(this);
        if (m_pLabelMachineFolder)
        {
            m_pLabelMachineFolder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMachineFolder, 0, 0);
        }
        /* Prepare machine folder selector: */
        m_pSelectorMachineFolder = new UIFilePathSelector(this);
        if (m_pSelectorMachineFolder)
        {
            if (m_pLabelMachineFolder)
                m_pLabelMachineFolder->setBuddy(m_pSelectorMachineFolder);
            m_pSelectorMachineFolder->setHomeDir(uiCommon().homeFolder());

            pLayoutMain->addWidget(m_pSelectorMachineFolder, 0, 1, 1, 2);
        }

        /* Prepare VRDP library name label: */
        m_pLabelVRDPLibName = new QLabel(this);
        if (m_pLabelVRDPLibName)
        {
            m_pLabelVRDPLibName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelVRDPLibName, 1, 0);
        }
        /* Prepare VRDP library name selector: */
        m_pSelectorVRDPLibName = new UIFilePathSelector(this);
        if (m_pSelectorVRDPLibName)
        {
            if (m_pLabelVRDPLibName)
                m_pLabelVRDPLibName->setBuddy(m_pSelectorVRDPLibName);
            m_pSelectorVRDPLibName->setHomeDir(uiCommon().homeFolder());
            m_pSelectorVRDPLibName->setMode(UIFilePathSelector::Mode_File_Open);

            pLayoutMain->addWidget(m_pSelectorVRDPLibName, 1, 1, 1, 2);
        }

        /* Prepare screen-saver label: */
        m_pLabelHostScreenSaver = new QLabel(this);
        if (m_pLabelHostScreenSaver)
        {
            m_pLabelHostScreenSaver->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLabelHostScreenSaver->hide();
            pLayoutMain->addWidget(m_pLabelHostScreenSaver, 2, 0);
        }
        /* Prepare screen-saver check-box: */
        m_pCheckBoxHostScreenSaver = new QCheckBox(this);
        if (m_pCheckBoxHostScreenSaver)
        {
            if (m_pLabelHostScreenSaver)
                m_pLabelHostScreenSaver->setBuddy(m_pCheckBoxHostScreenSaver);
            m_pCheckBoxHostScreenSaver->hide();
            pLayoutMain->addWidget(m_pCheckBoxHostScreenSaver, 2, 1);
        }
    }
}

void UIGlobalSettingsGeneral::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsGeneral::saveGeneralData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save general settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old general data from the cache: */
        const UIDataSettingsGlobalGeneral &oldGeneralData = m_pCache->base();
        /* Get new general data from the cache: */
        const UIDataSettingsGlobalGeneral &newGeneralData = m_pCache->data();

        /* Save default machine folder: */
        if (   fSuccess
            && newGeneralData.m_strDefaultMachineFolder != oldGeneralData.m_strDefaultMachineFolder)
        {
            m_properties.SetDefaultMachineFolder(newGeneralData.m_strDefaultMachineFolder);
            fSuccess = m_properties.isOk();
        }
        /* Save VRDE auth library: */
        if (   fSuccess
            && newGeneralData.m_strVRDEAuthLibrary != oldGeneralData.m_strVRDEAuthLibrary)
        {
            m_properties.SetVRDEAuthLibrary(newGeneralData.m_strVRDEAuthLibrary);
            fSuccess = m_properties.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_properties));

        /* Save new general data from the cache: */
        if (newGeneralData.m_fHostScreenSaverDisabled != oldGeneralData.m_fHostScreenSaverDisabled)
            gEDataManager->setHostScreenSaverDisabled(newGeneralData.m_fHostScreenSaverDisabled);
    }
    /* Return result: */
    return fSuccess;
}
