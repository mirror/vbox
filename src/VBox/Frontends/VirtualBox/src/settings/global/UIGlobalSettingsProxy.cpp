/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class implementation.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
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
#include "UIGlobalProxyFeaturesEditor.h"
#include "UIGlobalSettingsProxy.h"
#include "UIExtraDataManager.h"

/* COM includes: */
#include "CSystemProperties.h"


/** Global settings: Proxy page data structure. */
struct UIDataSettingsGlobalProxy
{
    /** Constructs data. */
    UIDataSettingsGlobalProxy()
        : m_enmProxyMode(KProxyMode_System)
        , m_strProxyHost(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalProxy &other) const
    {
        return    true
               && (m_enmProxyMode == other.m_enmProxyMode)
               && (m_strProxyHost == other.m_strProxyHost)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalProxy &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalProxy &other) const { return !equal(other); }

    /** Holds the proxy mode. */
    KProxyMode  m_enmProxyMode;
    /** Holds the proxy host. */
    QString     m_strProxyHost;
};


/*********************************************************************************************************************************
*   Class UIGlobalSettingsProxy implementation.                                                                                  *
*********************************************************************************************************************************/

UIGlobalSettingsProxy::UIGlobalSettingsProxy()
    : m_pCache(0)
{
    prepare();
}

UIGlobalSettingsProxy::~UIGlobalSettingsProxy()
{
    cleanup();
}

void UIGlobalSettingsProxy::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalProxy oldData;
    oldData.m_enmProxyMode = m_properties.GetProxyMode();
    oldData.m_strProxyHost = m_properties.GetProxyURL();
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsProxy::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalProxy &oldData = m_pCache->base();
    m_pEditorGlobalProxyFeatures->setProxyMode(oldData.m_enmProxyMode);
    m_pEditorGlobalProxyFeatures->setProxyHost(oldData.m_strProxyHost);

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsProxy::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalProxy newData = m_pCache->base();

    /* Cache new data: */
    newData.m_enmProxyMode = m_pEditorGlobalProxyFeatures->proxyMode();
    newData.m_strProxyHost = m_pEditorGlobalProxyFeatures->proxyHost();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsProxy::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsProxy::validate(QList<UIValidationMessage> &messages)
{
    /* Pass if proxy is disabled: */
    if (m_pEditorGlobalProxyFeatures->proxyMode() != KProxyMode_Manual)
        return true;

    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* Check for URL presence: */
    if (m_pEditorGlobalProxyFeatures->proxyHost().trimmed().isEmpty())
    {
        message.second << tr("No proxy URL is currently specified.");
        fPass = false;
    }

    else

    /* Check for URL validness: */
    if (!QUrl(m_pEditorGlobalProxyFeatures->proxyHost().trimmed()).isValid())
    {
        message.second << tr("Invalid proxy URL is currently specified.");
        fPass = true;
    }

    else

    /* Check for password presence: */
    if (!QUrl(m_pEditorGlobalProxyFeatures->proxyHost().trimmed()).password().isEmpty())
    {
        message.second << tr("You have provided a proxy password. "
                             "Please be aware that the password will be saved in plain text. "
                             "You may wish to configure a system-wide proxy instead and not "
                             "store application-specific settings.");
        fPass = true;
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsProxy::retranslateUi()
{
}

void UIGlobalSettingsProxy::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalProxy;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsProxy::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /* Prepare global proxy features editor: */
        m_pEditorGlobalProxyFeatures = new UIGlobalProxyFeaturesEditor(this);
        if (m_pEditorGlobalProxyFeatures)
            pLayout->addWidget(m_pEditorGlobalProxyFeatures);

        /* Add stretch to the end: */
        pLayout->addStretch();
    }
}

void UIGlobalSettingsProxy::prepareConnections()
{
    connect(m_pEditorGlobalProxyFeatures, &UIGlobalProxyFeaturesEditor::sigProxyModeChanged,
            this, &UIGlobalSettingsProxy::revalidate);
    connect(m_pEditorGlobalProxyFeatures, &UIGlobalProxyFeaturesEditor::sigProxyHostChanged,
            this, &UIGlobalSettingsProxy::revalidate);
}

void UIGlobalSettingsProxy::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsProxy::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save settings from cache: */
    if (   fSuccess
        && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalProxy &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalProxy &newData = m_pCache->data();

        /* Save new data from cache: */
        if (   fSuccess
            && newData.m_enmProxyMode != oldData.m_enmProxyMode)
        {
            m_properties.SetProxyMode(newData.m_enmProxyMode);
            fSuccess &= m_properties.isOk();
        }
        if (   fSuccess
            && newData.m_strProxyHost != oldData.m_strProxyHost)
        {
            m_properties.SetProxyURL(newData.m_strProxyHost);
            fSuccess &= m_properties.isOk();
        }

        /* Drop the old extra data setting if still around: */
        if (   fSuccess
            && !gEDataManager->proxySettings().isEmpty())
            /* fSuccess = */ gEDataManager->setProxySettings(QString());
    }
    /* Return result: */
    return fSuccess;
}
