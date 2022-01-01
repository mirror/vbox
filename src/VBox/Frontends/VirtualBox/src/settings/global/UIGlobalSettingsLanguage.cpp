/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsLanguage class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include "UIGlobalSettingsLanguage.h"
#include "UILanguageSettingsEditor.h"


/** Global settings: Language page data structure. */
struct UIDataSettingsGlobalLanguage
{
    /** Constructs data. */
    UIDataSettingsGlobalLanguage()
        : m_strLanguageId(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalLanguage &other) const
    {
        return    true
               && (m_strLanguageId == other.m_strLanguageId)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalLanguage &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalLanguage &other) const { return !equal(other); }

    /** Holds the current language id. */
    QString m_strLanguageId;
};


/*********************************************************************************************************************************
*   Class UIGlobalSettingsLanguage implementation.                                                                               *
*********************************************************************************************************************************/

UIGlobalSettingsLanguage::UIGlobalSettingsLanguage()
    : m_pCache(0)
    , m_pEditorLanguageSettings(0)
{
    prepare();
}

UIGlobalSettingsLanguage::~UIGlobalSettingsLanguage()
{
    cleanup();
}

void UIGlobalSettingsLanguage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalLanguage oldData;
    oldData.m_strLanguageId = gEDataManager->languageId();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsLanguage::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalLanguage &oldData = m_pCache->base();
    m_pEditorLanguageSettings->setValue(oldData.m_strLanguageId);
}

void UIGlobalSettingsLanguage::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalLanguage newData = m_pCache->base();

    /* Cache new data: */
    newData.m_strLanguageId = m_pEditorLanguageSettings->value();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsLanguage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsLanguage::retranslateUi()
{
}

void UIGlobalSettingsLanguage::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalLanguage;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsLanguage::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare language settings editor: */
        m_pEditorLanguageSettings = new UILanguageSettingsEditor(this);
        if (m_pEditorLanguageSettings)
            pLayoutMain->addWidget(m_pEditorLanguageSettings);
    }
}

void UIGlobalSettingsLanguage::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsLanguage::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save settings from cache: */
    if (   fSuccess
        && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalLanguage &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalLanguage &newData = m_pCache->data();

        /* Save new data from cache: */
        if (   fSuccess
            && newData.m_strLanguageId != oldData.m_strLanguageId)
            /* fSuccess = */ gEDataManager->setLanguageId(newData.m_strLanguageId);
    }
    /* Return result: */
    return fSuccess;
}
