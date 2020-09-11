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
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UICommon.h"
#include "UIDefaultMachineFolderEditor.h"
#include "UIErrorString.h"
#include "UIGlobalSettingsGeneral.h"
#include "UIVRDEAuthLibraryEditor.h"


/** Global settings: General page data structure. */
struct UIDataSettingsGlobalGeneral
{
    /** Constructs data. */
    UIDataSettingsGlobalGeneral()
        : m_strDefaultMachineFolder(QString())
        , m_strVRDEAuthLibrary(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalGeneral &other) const
    {
        return true
               && (m_strDefaultMachineFolder == other.m_strDefaultMachineFolder)
               && (m_strVRDEAuthLibrary == other.m_strVRDEAuthLibrary)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalGeneral &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalGeneral &other) const { return !equal(other); }

    /** Holds the 'default machine folder' path. */
    QString m_strDefaultMachineFolder;
    /** Holds the 'VRDE auth library' name. */
    QString m_strVRDEAuthLibrary;
};


UIGlobalSettingsGeneral::UIGlobalSettingsGeneral()
    : m_pCache(0)
    , m_pLabelDefaultMachineFolder(0)
    , m_pEditorDefaultMachineFolder(0)
    , m_pLabelVRDEAuthLibrary(0)
    , m_pEditorVRDEAuthLibrary(0)
{
    prepare();
}

UIGlobalSettingsGeneral::~UIGlobalSettingsGeneral()
{
    cleanup();
}

void UIGlobalSettingsGeneral::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalGeneral oldData;
    oldData.m_strDefaultMachineFolder = m_properties.GetDefaultMachineFolder();
    oldData.m_strVRDEAuthLibrary = m_properties.GetVRDEAuthLibrary();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalGeneral &oldData = m_pCache->base();
    m_pEditorDefaultMachineFolder->setValue(oldData.m_strDefaultMachineFolder);
    m_pEditorVRDEAuthLibrary->setValue(oldData.m_strVRDEAuthLibrary);
}

void UIGlobalSettingsGeneral::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalGeneral newData = m_pCache->base();

    /* Cache new data: */
    newData.m_strDefaultMachineFolder = m_pEditorDefaultMachineFolder->value();
    newData.m_strVRDEAuthLibrary = m_pEditorVRDEAuthLibrary->value();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsGeneral::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveGeneralData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsGeneral::retranslateUi()
{
    m_pLabelDefaultMachineFolder->setText(UIDefaultMachineFolderEditor::tr("Default &Machine Folder:"));
    m_pLabelVRDEAuthLibrary->setText(UIVRDEAuthLibraryEditor::tr("V&RDP Authentication Library:"));
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
        pLayoutMain->setColumnStretch(1, 1);
        pLayoutMain->setRowStretch(3, 1);

        /* Prepare 'default machine folder' label: */
        m_pLabelDefaultMachineFolder = new QLabel(this);
        if (m_pLabelDefaultMachineFolder)
        {
            m_pLabelDefaultMachineFolder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelDefaultMachineFolder, 0, 0);
        }
        /* Prepare 'default machine folder' editor: */
        m_pEditorDefaultMachineFolder = new UIDefaultMachineFolderEditor(this);
        if (m_pEditorDefaultMachineFolder)
        {
            if (m_pLabelDefaultMachineFolder)
                m_pLabelDefaultMachineFolder->setBuddy(m_pEditorDefaultMachineFolder);
            pLayoutMain->addWidget(m_pEditorDefaultMachineFolder, 0, 1, 1, 2);
        }

        /* Prepare 'VRDE auth library' label: */
        m_pLabelVRDEAuthLibrary = new QLabel(this);
        if (m_pLabelVRDEAuthLibrary)
        {
            m_pLabelVRDEAuthLibrary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelVRDEAuthLibrary, 1, 0);
        }
        /* Prepare 'VRDE auth library' editor: */
        m_pEditorVRDEAuthLibrary = new UIVRDEAuthLibraryEditor(this);
        if (m_pEditorVRDEAuthLibrary)
        {
            if (m_pLabelVRDEAuthLibrary)
                m_pLabelVRDEAuthLibrary->setBuddy(m_pEditorVRDEAuthLibrary);
            pLayoutMain->addWidget(m_pEditorVRDEAuthLibrary, 1, 1, 1, 2);
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
    /* Save settings from cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalGeneral &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalGeneral &newData = m_pCache->data();

        /* Save 'default machine folder': */
        if (   fSuccess
            && newData.m_strDefaultMachineFolder != oldData.m_strDefaultMachineFolder)
        {
            m_properties.SetDefaultMachineFolder(newData.m_strDefaultMachineFolder);
            fSuccess = m_properties.isOk();
        }
        /* Save 'VRDE auth library': */
        if (   fSuccess
            && newData.m_strVRDEAuthLibrary != oldData.m_strVRDEAuthLibrary)
        {
            m_properties.SetVRDEAuthLibrary(newData.m_strVRDEAuthLibrary);
            fSuccess = m_properties.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_properties));
    }
    /* Return result: */
    return fSuccess;
}
