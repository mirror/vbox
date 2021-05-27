/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class implementation.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIExtraDataManager.h"
#include "UIGlobalSettingsUpdate.h"
#include "UIUpdateSettingsEditor.h"


/** Global settings: Update page data structure. */
struct UIDataSettingsGlobalUpdate
{
    /** Constructs data. */
    UIDataSettingsGlobalUpdate()
        : m_guiUpdateData(VBoxUpdateData())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalUpdate &other) const
    {
        return    true
               && (m_guiUpdateData == other.m_guiUpdateData)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalUpdate &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalUpdate &other) const { return !equal(other); }

    /** Holds VBox update data. */
    VBoxUpdateData  m_guiUpdateData;
};


/*********************************************************************************************************************************
*   Class UIGlobalSettingsUpdate implementation.                                                                                 *
*********************************************************************************************************************************/

UIGlobalSettingsUpdate::UIGlobalSettingsUpdate()
    : m_pCache(0)
    , m_pEditorUpdateSettings(0)
{
    prepare();
}

UIGlobalSettingsUpdate::~UIGlobalSettingsUpdate()
{
    cleanup();
}

void UIGlobalSettingsUpdate::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalUpdate oldData;
    oldData.m_guiUpdateData = gEDataManager->applicationUpdateData();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalUpdate &oldData = m_pCache->base();
    m_pEditorUpdateSettings->setValue(oldData.m_guiUpdateData);
}

void UIGlobalSettingsUpdate::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalUpdate newData = m_pCache->base();

    /* Cache new data: */
    newData.m_guiUpdateData = m_pEditorUpdateSettings->value();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsUpdate::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
}

void UIGlobalSettingsUpdate::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalUpdate;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsUpdate::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare update settings editor: */
        m_pEditorUpdateSettings = new UIUpdateSettingsEditor(this);
        if (m_pEditorUpdateSettings)
            pLayoutMain->addWidget(m_pEditorUpdateSettings);
    }
}

void UIGlobalSettingsUpdate::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsUpdate::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save update settings from cache: */
    if (   fSuccess
        && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalUpdate &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalUpdate &newData = m_pCache->data();

        /* Save new data from cache: */
        if (   fSuccess
            && newData != oldData)
            /* fSuccess = */ gEDataManager->setApplicationUpdateData(newData.m_guiUpdateData.data());
    }
    /* Return result: */
    return fSuccess;
}
