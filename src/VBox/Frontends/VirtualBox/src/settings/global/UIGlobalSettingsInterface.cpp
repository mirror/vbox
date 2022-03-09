/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInterface class implementation.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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
#include "UIColorThemeEditor.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSettingsInterface.h"

/** Global settings: User Interface page data structure. */
struct UIDataSettingsGlobalInterface
{
    /** Constructs data. */
    UIDataSettingsGlobalInterface()
        : m_enmColorTheme(UIColorThemeType_Auto)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalInterface &other) const
    {
        return    true
               && (m_enmColorTheme == other.m_enmColorTheme)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalInterface &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalInterface &other) const { return !equal(other); }

    /** Holds the color-theme. */
    UIColorThemeType  m_enmColorTheme;
};


/*********************************************************************************************************************************
*   Class UIGlobalSettingsInterface implementation.                                                                              *
*********************************************************************************************************************************/

UIGlobalSettingsInterface::UIGlobalSettingsInterface()
    : m_pCache(0)
    , m_pEditorColorTheme(0)
{
    prepare();
}

UIGlobalSettingsInterface::~UIGlobalSettingsInterface()
{
    cleanup();
}

void UIGlobalSettingsInterface::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalInterface oldData;
    oldData.m_enmColorTheme = gEDataManager->colorTheme();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsInterface::getFromCache()
{
    /* Load old data from the cache: */
    const UIDataSettingsGlobalInterface &oldData = m_pCache->base();
    m_pEditorColorTheme->setValue(oldData.m_enmColorTheme);

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsInterface::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalInterface newData;

    /* Cache new data: */
    newData.m_enmColorTheme = m_pEditorColorTheme->value();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsInterface::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsInterface::retranslateUi()
{
}

void UIGlobalSettingsInterface::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalInterface;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsInterface::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(1, 1);

        /* Prepare color-theme editor: */
        m_pEditorColorTheme = new UIColorThemeEditor(this);
        if (m_pEditorColorTheme)
            pLayoutMain->addWidget(m_pEditorColorTheme, 0, 0);
    }
}

void UIGlobalSettingsInterface::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsInterface::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save settings from cache: */
    if (   fSuccess
        && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalInterface &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalInterface &newData = m_pCache->data();

        /* Save 'color-theme': */
        if (   fSuccess
            && newData.m_enmColorTheme != oldData.m_enmColorTheme)
            /* fSuccess = */ gEDataManager->setColorTheme(newData.m_enmColorTheme);
    }
    /* Return result: */
    return fSuccess;
}
