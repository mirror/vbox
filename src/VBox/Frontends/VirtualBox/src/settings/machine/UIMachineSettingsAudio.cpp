/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsAudio class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIAudioControllerEditor.h"
#include "UIAudioHostDriverEditor.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIMachineAudioFeaturesEditor.h"
#include "UIMachineSettingsAudio.h"

/* COM includes: */
#include "CAudioAdapter.h"


/** Machine settings: Audio page data structure. */
struct UIDataSettingsMachineAudio
{
    /** Constructs data. */
    UIDataSettingsMachineAudio()
        : m_fAudioEnabled(false)
        , m_audioDriverType(KAudioDriverType_Null)
        , m_audioControllerType(KAudioControllerType_AC97)
        , m_fAudioOutputEnabled(false)
        , m_fAudioInputEnabled(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineAudio &other) const
    {
        return true
               && (m_fAudioEnabled == other.m_fAudioEnabled)
               && (m_audioDriverType == other.m_audioDriverType)
               && (m_audioControllerType == other.m_audioControllerType)
               && (m_fAudioOutputEnabled == other.m_fAudioOutputEnabled)
               && (m_fAudioInputEnabled == other.m_fAudioInputEnabled)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineAudio &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineAudio &other) const { return !equal(other); }

    /** Holds whether the audio is enabled. */
    bool                  m_fAudioEnabled;
    /** Holds the audio driver type. */
    KAudioDriverType      m_audioDriverType;
    /** Holds the audio controller type. */
    KAudioControllerType  m_audioControllerType;
    /** Holds whether the audio output is enabled. */
    bool                  m_fAudioOutputEnabled;
    /** Holds whether the audio input is enabled. */
    bool                  m_fAudioInputEnabled;
};


UIMachineSettingsAudio::UIMachineSettingsAudio()
    : m_pCache(0)
    , m_pCheckBoxAudio(0)
    , m_pWidgetAudioSettings(0)
    , m_pEditorAudioHostDriver(0)
    , m_pEditorAudioController(0)
    , m_pEditorAudioFeatures(0)
{
    prepare();
}

UIMachineSettingsAudio::~UIMachineSettingsAudio()
{
    cleanup();
}

bool UIMachineSettingsAudio::changed() const
{
    return m_pCache ? m_pCache->wasChanged() : false;
}

void UIMachineSettingsAudio::loadToCacheFrom(QVariant &data)
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old data: */
    UIDataSettingsMachineAudio oldAudioData;

    /* Check whether adapter is valid: */
    const CAudioAdapter &comAdapter = m_machine.GetAudioAdapter();
    if (!comAdapter.isNull())
    {
        /* Gather old data: */
        oldAudioData.m_fAudioEnabled = comAdapter.GetEnabled();
        oldAudioData.m_audioDriverType = comAdapter.GetAudioDriver();
        oldAudioData.m_audioControllerType = comAdapter.GetAudioController();
        oldAudioData.m_fAudioOutputEnabled = comAdapter.GetEnabledOut();
        oldAudioData.m_fAudioInputEnabled = comAdapter.GetEnabledIn();
    }

    /* Cache old data: */
    m_pCache->cacheInitialData(oldAudioData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::getFromCache()
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Get old data from cache: */
    const UIDataSettingsMachineAudio &oldAudioData = m_pCache->base();

    /* Load old data from cache: */
    m_pCheckBoxAudio->setChecked(oldAudioData.m_fAudioEnabled);
    if (m_pEditorAudioHostDriver)
        m_pEditorAudioHostDriver->setValue(oldAudioData.m_audioDriverType);
    if (m_pEditorAudioController)
        m_pEditorAudioController->setValue(oldAudioData.m_audioControllerType);
    if (m_pEditorAudioFeatures)
    {
        m_pEditorAudioFeatures->setEnableOutput(oldAudioData.m_fAudioOutputEnabled);
        m_pEditorAudioFeatures->setEnableInput(oldAudioData.m_fAudioInputEnabled);
    }

    /* Polish page finally: */
    polishPage();
}

void UIMachineSettingsAudio::putToCache()
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Prepare new data: */
    UIDataSettingsMachineAudio newAudioData;

    /* Cache new data: */
    if (m_pCheckBoxAudio)
        newAudioData.m_fAudioEnabled = m_pCheckBoxAudio->isChecked();
    if (m_pEditorAudioHostDriver)
        newAudioData.m_audioDriverType = m_pEditorAudioHostDriver->value();
    if (m_pEditorAudioController)
        newAudioData.m_audioControllerType = m_pEditorAudioController->value();
    if (m_pEditorAudioFeatures)
    {
        newAudioData.m_fAudioOutputEnabled = m_pEditorAudioFeatures->outputEnabled();
        newAudioData.m_fAudioInputEnabled = m_pEditorAudioFeatures->inputEnabled();
    }
    m_pCache->cacheCurrentData(newAudioData);
}

void UIMachineSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::retranslateUi()
{
    m_pCheckBoxAudio->setText(tr("Enable &Audio"));
    m_pCheckBoxAudio->setToolTip(tr("When checked, a virtual PCI audio card will be plugged into the virtual machine "
                                    "and will communicate with the host audio system using the specified driver."));

    /* These editors have own labels, but we want them to be properly layouted according to each other: */
    int iMinimumLayoutHint = 0;
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorAudioHostDriver->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorAudioController->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorAudioFeatures->minimumLabelHorizontalHint());
    m_pEditorAudioHostDriver->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorAudioController->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorAudioFeatures->setMinimumLayoutIndent(iMinimumLayoutHint);
}

void UIMachineSettingsAudio::polishPage()
{
    /* Polish audio page availability: */
    m_pCheckBoxAudio->setEnabled(isMachineOffline());
    m_pEditorAudioHostDriver->setEnabled(isMachineOffline() || isMachineSaved());
    m_pEditorAudioController->setEnabled(isMachineOffline());
    m_pEditorAudioFeatures->setEnabled(isMachineInValidMode());
    m_pWidgetAudioSettings->setEnabled(m_pCheckBoxAudio->isChecked());
}

void UIMachineSettingsAudio::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineAudio;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsAudio::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayout = new QGridLayout(this);
    if (pLayout)
    {
        pLayout->setRowStretch(2, 1);

        /* Prepare audio check-box: */
        m_pCheckBoxAudio = new QCheckBox(this);
        if (m_pCheckBoxAudio)
            pLayout->addWidget(m_pCheckBoxAudio, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayout->addItem(pSpacerItem, 1, 0);

        /* Prepare audio settings widget: */
        m_pWidgetAudioSettings = new QWidget(this);
        if (m_pWidgetAudioSettings)
        {
            /* Prepare audio settings widget layout: */
            QVBoxLayout *pLayoutAudioSettings = new QVBoxLayout(m_pWidgetAudioSettings);
            if (pLayoutAudioSettings)
            {
                pLayoutAudioSettings->setContentsMargins(0, 0, 0, 0);

                /* Prepare audio host driver editor: */
                m_pEditorAudioHostDriver = new UIAudioHostDriverEditor(m_pWidgetAudioSettings);
                if (m_pEditorAudioHostDriver)
                    pLayoutAudioSettings->addWidget(m_pEditorAudioHostDriver);

                /* Prepare audio host controller editor: */
                m_pEditorAudioController = new UIAudioControllerEditor(m_pWidgetAudioSettings);
                if (m_pEditorAudioController)
                    pLayoutAudioSettings->addWidget(m_pEditorAudioController);

                /* Prepare audio extended features editor: */
                m_pEditorAudioFeatures = new UIMachineAudioFeaturesEditor(m_pWidgetAudioSettings);
                if (m_pEditorAudioFeatures)
                    pLayoutAudioSettings->addWidget(m_pEditorAudioFeatures);
            }

            pLayout->addWidget(m_pWidgetAudioSettings, 1, 1);
        }
    }
}

void UIMachineSettingsAudio::prepareConnections()
{
    connect(m_pCheckBoxAudio, &QCheckBox::toggled, m_pWidgetAudioSettings, &QWidget::setEnabled);
}

void UIMachineSettingsAudio::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIMachineSettingsAudio::saveData()
{
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save audio settings from cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsMachineAudio &oldAudioData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsMachineAudio &newAudioData = m_pCache->data();

        /* Get audio adapter for further activities: */
        CAudioAdapter comAdapter = m_machine.GetAudioAdapter();
        fSuccess = m_machine.isOk() && comAdapter.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            /* Save whether audio is enabled: */
            if (fSuccess && isMachineOffline() && newAudioData.m_fAudioEnabled != oldAudioData.m_fAudioEnabled)
            {
                comAdapter.SetEnabled(newAudioData.m_fAudioEnabled);
                fSuccess = comAdapter.isOk();
            }
            /* Save audio driver type: */
            if (fSuccess && (isMachineOffline() || isMachineSaved()) && newAudioData.m_audioDriverType != oldAudioData.m_audioDriverType)
            {
                comAdapter.SetAudioDriver(newAudioData.m_audioDriverType);
                fSuccess = comAdapter.isOk();
            }
            /* Save audio controller type: */
            if (fSuccess && isMachineOffline() && newAudioData.m_audioControllerType != oldAudioData.m_audioControllerType)
            {
                comAdapter.SetAudioController(newAudioData.m_audioControllerType);
                fSuccess = comAdapter.isOk();
            }
            /* Save whether audio output is enabled: */
            if (fSuccess && isMachineInValidMode() && newAudioData.m_fAudioOutputEnabled != oldAudioData.m_fAudioOutputEnabled)
            {
                comAdapter.SetEnabledOut(newAudioData.m_fAudioOutputEnabled);
                fSuccess = comAdapter.isOk();
            }
            /* Save whether audio input is enabled: */
            if (fSuccess && isMachineInValidMode() && newAudioData.m_fAudioInputEnabled != oldAudioData.m_fAudioInputEnabled)
            {
                comAdapter.SetEnabledIn(newAudioData.m_fAudioInputEnabled);
                fSuccess = comAdapter.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(comAdapter));
        }
    }
    /* Return result: */
    return fSuccess;
}
