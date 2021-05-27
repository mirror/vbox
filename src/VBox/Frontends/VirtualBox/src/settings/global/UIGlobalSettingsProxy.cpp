/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class implementation.
 */

/*
 * Copyright (C) 2011-2021 Oracle Corporation
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
#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>
#include <QRegExpValidator>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIWidgetValidator.h"
#include "UIGlobalSettingsProxy.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UICommon.h"
#include "VBoxUtils.h"

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
    , m_pButtonGroup(0)
    , m_pRadioButtonProxyAuto(0)
    , m_pRadioButtonProxyDisabled(0)
    , m_pRadioButtonProxyEnabled(0)
    , m_pWidgetSettings(0)
    , m_pLabelHost(0)
    , m_pEditorHost(0)
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
    switch (oldData.m_enmProxyMode)
    {
        case KProxyMode_System:  m_pRadioButtonProxyAuto->setChecked(true); break;
        case KProxyMode_NoProxy: m_pRadioButtonProxyDisabled->setChecked(true); break;
        case KProxyMode_Manual:  m_pRadioButtonProxyEnabled->setChecked(true); break;
        case KProxyMode_Max:     break; /* (compiler warnings) */
    }
    m_pEditorHost->setText(oldData.m_strProxyHost);
    sltHandleProxyToggle();

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsProxy::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalProxy newData = m_pCache->base();

    /* Cache new data: */
    newData.m_enmProxyMode = m_pRadioButtonProxyEnabled->isChecked()
                           ? KProxyMode_Manual : m_pRadioButtonProxyDisabled->isChecked()
                           ? KProxyMode_NoProxy : KProxyMode_System;
    newData.m_strProxyHost = m_pEditorHost->text();
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
    if (!m_pRadioButtonProxyEnabled->isChecked())
        return true;

    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* Check for URL presence: */
    if (m_pEditorHost->text().trimmed().isEmpty())
    {
        message.second << tr("No proxy URL is currently specified.");
        fPass = false;
    }

    else

    /* Check for URL validness: */
    if (!QUrl(m_pEditorHost->text().trimmed()).isValid())
    {
        message.second << tr("Invalid proxy URL is currently specified.");
        fPass = true;
    }

    else

    /* Check for password presence: */
    if (!QUrl(m_pEditorHost->text().trimmed()).password().isEmpty())
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
    m_pRadioButtonProxyAuto->setWhatsThis(tr("When chosen, VirtualBox will try to auto-detect host proxy settings for tasks like "
                                       "downloading Guest Additions from the network or checking for updates."));
    m_pRadioButtonProxyAuto->setText(tr("&Auto-detect Host Proxy Settings"));
    m_pRadioButtonProxyDisabled->setWhatsThis(tr("When chosen, VirtualBox will use direct Internet connection for tasks like downloading "
                                           "Guest Additions from the network or checking for updates."));
    m_pRadioButtonProxyDisabled->setText(tr("&Direct Connection to the Internet"));
    m_pRadioButtonProxyEnabled->setWhatsThis(tr("When chosen, VirtualBox will use the proxy settings supplied for tasks like downloading "
                                          "Guest Additions from the network or checking for updates."));
    m_pRadioButtonProxyEnabled->setText(tr("&Manual Proxy Configuration"));
    m_pLabelHost->setText(tr("&URL:"));

    /* Translate proxy URL editor: */
    m_pEditorHost->setWhatsThis(tr("Holds the proxy URL. "
                                   "The format is: "
                                   "<table cellspacing=0 style='white-space:pre'>"
                                   "<tr><td>[{type}://][{userid}[:{password}]@]{server}[:{port}]</td></tr>"
                                   "<tr><td>http://username:password@proxy.host.com:port</td></tr>"
                                   "</table>"));
}

void UIGlobalSettingsProxy::sltHandleProxyToggle()
{
    /* Update widgets availability: */
    m_pWidgetSettings->setEnabled(m_pRadioButtonProxyEnabled->isChecked());

    /* Revalidate: */
    revalidate();
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
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(4, 1);

        /* Prepare button-group: */
        m_pButtonGroup = new QButtonGroup(this);
        if (m_pButtonGroup)
        {
            /* Prepare 'proxy auto' button: */
            m_pRadioButtonProxyAuto = new QRadioButton(this);
            if (m_pRadioButtonProxyAuto)
            {
                m_pButtonGroup->addButton(m_pRadioButtonProxyAuto);
                pLayoutMain->addWidget(m_pRadioButtonProxyAuto, 0, 0, 1, 2);
            }
            /* Prepare 'proxy disabled' button: */
            m_pRadioButtonProxyDisabled = new QRadioButton(this);
            if (m_pRadioButtonProxyDisabled)
            {
                m_pButtonGroup->addButton(m_pRadioButtonProxyDisabled);
                pLayoutMain->addWidget(m_pRadioButtonProxyDisabled, 1, 0, 1, 2);
            }
            /* Prepare 'proxy enabled' button: */
            m_pRadioButtonProxyEnabled = new QRadioButton(this);
            if (m_pRadioButtonProxyEnabled)
            {
                m_pButtonGroup->addButton(m_pRadioButtonProxyEnabled);
                pLayoutMain->addWidget(m_pRadioButtonProxyEnabled, 2, 0, 1, 2);
            }
        }

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayoutMain->addItem(pSpacerItem, 3, 0);

        /* Prepare settings widget: */
        m_pWidgetSettings = new QWidget(this);
        if (m_pWidgetSettings)
        {
            /* Prepare settings layout widget: */
            QHBoxLayout *pLayoutSettings = new QHBoxLayout(m_pWidgetSettings);
            if (pLayoutSettings)
            {
                pLayoutSettings->setContentsMargins(0, 0, 0, 0);

                /* Prepare host label: */
                m_pLabelHost = new QLabel(m_pWidgetSettings);
                if (m_pLabelHost)
                {
                    m_pLabelHost->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelHost);
                }
                /* Prepare host editor: */
                m_pEditorHost = new QILineEdit(m_pWidgetSettings);
                if (m_pEditorHost)
                {
                    if (m_pLabelHost)
                        m_pLabelHost->setBuddy(m_pEditorHost);
                    m_pEditorHost->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pEditorHost));

                    pLayoutSettings->addWidget(m_pEditorHost);
                }
            }

            pLayoutMain->addWidget(m_pWidgetSettings, 3, 1);
        }
    }
}

void UIGlobalSettingsProxy::prepareConnections()
{
    connect(m_pButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, &UIGlobalSettingsProxy::sltHandleProxyToggle);
    connect(m_pEditorHost, &QILineEdit::textEdited, this, &UIGlobalSettingsProxy::revalidate);
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
        //const UIDataSettingsGlobalProxy &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalProxy &newData = m_pCache->data();

        /* Save new data from cache: */
        if (fSuccess)
        {
            m_properties.SetProxyMode(newData.m_enmProxyMode);
            fSuccess &= m_properties.isOk();
        }
        if (fSuccess)
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
