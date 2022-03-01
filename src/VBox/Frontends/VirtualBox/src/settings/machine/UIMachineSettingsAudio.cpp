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
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIAudioControllerEditor.h"
#include "UIAudioHostDriverEditor.h"
#include "UIConverter.h"
#include "UIErrorString.h"
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
    , m_pLayoutAudioSettings(0)
    , m_pEditorAudioHostDriver(0)
    , m_pEditorAudioController(0)
    , m_pLabelAudioExtended(0)
    , m_pCheckBoxAudioOutput(0)
    , m_pCheckBoxAudioInput(0)
{
    prepare();
}

UIMachineSettingsAudio::~UIMachineSettingsAudio()
{
    cleanup();
}

bool UIMachineSettingsAudio::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsAudio::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old audio data: */
    UIDataSettingsMachineAudio oldAudioData;

    /* Check whether adapter is valid: */
    const CAudioAdapter &comAdapter = m_machine.GetAudioAdapter();
    if (!comAdapter.isNull())
    {
        /* Gather old audio data: */
        oldAudioData.m_fAudioEnabled = comAdapter.GetEnabled();
        oldAudioData.m_audioDriverType = comAdapter.GetAudioDriver();
        oldAudioData.m_audioControllerType = comAdapter.GetAudioController();
        oldAudioData.m_fAudioOutputEnabled = comAdapter.GetEnabledOut();
        oldAudioData.m_fAudioInputEnabled = comAdapter.GetEnabledIn();
    }

    /* Cache old audio data: */
    m_pCache->cacheInitialData(oldAudioData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::getFromCache()
{
    /* Get old audio data from the cache: */
    const UIDataSettingsMachineAudio &oldAudioData = m_pCache->base();

    /* Load old audio data from the cache: */
    m_pCheckBoxAudio->setChecked(oldAudioData.m_fAudioEnabled);
    m_pEditorAudioHostDriver->setValue(oldAudioData.m_audioDriverType);
    m_pEditorAudioController->setValue(oldAudioData.m_audioControllerType);
    m_pCheckBoxAudioOutput->setChecked(oldAudioData.m_fAudioOutputEnabled);
    m_pCheckBoxAudioInput->setChecked(oldAudioData.m_fAudioInputEnabled);

    /* Polish page finally: */
    polishPage();
}

void UIMachineSettingsAudio::putToCache()
{
    /* Prepare new audio data: */
    UIDataSettingsMachineAudio newAudioData;

    /* Gather new audio data: */
    newAudioData.m_fAudioEnabled = m_pCheckBoxAudio->isChecked();
    newAudioData.m_audioDriverType = m_pEditorAudioHostDriver->value();
    newAudioData.m_audioControllerType = m_pEditorAudioController->value();
    newAudioData.m_fAudioOutputEnabled = m_pCheckBoxAudioOutput->isChecked();
    newAudioData.m_fAudioInputEnabled = m_pCheckBoxAudioInput->isChecked();

    /* Cache new audio data: */
    m_pCache->cacheCurrentData(newAudioData);
}

void UIMachineSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update audio data and failing state: */
    setFailed(!saveAudioData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::retranslateUi()
{
    m_pCheckBoxAudio->setText(tr("Enable &Audio"));
    m_pCheckBoxAudio->setToolTip(tr("When checked, a virtual PCI audio card will be plugged into the virtual machine "
                                    "and will communicate with the host audio system using the specified driver."));
    m_pLabelAudioExtended->setText(tr("Extended Features:"));
    m_pCheckBoxAudioOutput->setText(tr("Enable Audio &Output"));
    m_pCheckBoxAudioOutput->setToolTip(tr("When checked, output to the virtual audio device will reach the host. "
                                          "Otherwise the guest is muted."));
    m_pCheckBoxAudioInput->setText(tr("Enable Audio &Input"));
    m_pCheckBoxAudioInput->setToolTip(tr("When checked, the guest will be able to capture audio input from the host. "
                                         "Otherwise the guest will capture only silence."));

    /* These editors have own labels, but we want them to be properly layouted according to each other: */
    int iMinimumLayoutHint = 0;
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorAudioHostDriver->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorAudioController->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pLabelAudioExtended->minimumSizeHint().width());
    m_pEditorAudioHostDriver->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorAudioController->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pLayoutAudioSettings->setColumnMinimumWidth(0, iMinimumLayoutHint);
}

void UIMachineSettingsAudio::polishPage()
{
    /* Polish audio page availability: */
    m_pCheckBoxAudio->setEnabled(isMachineOffline());
    m_pEditorAudioHostDriver->setEnabled(isMachineOffline() || isMachineSaved());
    m_pEditorAudioController->setEnabled(isMachineOffline());
    m_pLabelAudioExtended->setEnabled(isMachineInValidMode());
    m_pCheckBoxAudioOutput->setEnabled(isMachineInValidMode());
    m_pCheckBoxAudioInput->setEnabled(isMachineInValidMode());
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
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(2, 1);

        /* Prepare audio check-box: */
        m_pCheckBoxAudio = new QCheckBox(this);
        if (m_pCheckBoxAudio)
            pLayoutMain->addWidget(m_pCheckBoxAudio, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayoutMain->addItem(pSpacerItem, 1, 0);

        /* Prepare audio settings widget: */
        m_pWidgetAudioSettings = new QWidget(this);
        if (m_pWidgetAudioSettings)
        {
            /* Prepare audio settings widget layout: */
            m_pLayoutAudioSettings = new QGridLayout(m_pWidgetAudioSettings);
            if (m_pLayoutAudioSettings)
            {
                m_pLayoutAudioSettings->setContentsMargins(0, 0, 0, 0);
                m_pLayoutAudioSettings->setColumnStretch(1, 1);

                /* Prepare audio host driver editor: */
                m_pEditorAudioHostDriver = new UIAudioHostDriverEditor(m_pWidgetAudioSettings);
                if (m_pEditorAudioHostDriver)
                    m_pLayoutAudioSettings->addWidget(m_pEditorAudioHostDriver, 0, 0, 1, 3);

                /* Prepare audio host controller editor: */
                m_pEditorAudioController = new UIAudioControllerEditor(m_pWidgetAudioSettings);
                if (m_pEditorAudioController)
                    m_pLayoutAudioSettings->addWidget(m_pEditorAudioController, 1, 0, 1, 3);

                /* Prepare audio extended label: */
                m_pLabelAudioExtended = new QLabel(m_pWidgetAudioSettings);
                if (m_pLabelAudioExtended)
                {
                    m_pLabelAudioExtended->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    m_pLayoutAudioSettings->addWidget(m_pLabelAudioExtended, 2, 0);
                }
                /* Prepare audio output check-box: */
                m_pCheckBoxAudioOutput = new QCheckBox(m_pWidgetAudioSettings);
                if (m_pCheckBoxAudioOutput)
                    m_pLayoutAudioSettings->addWidget(m_pCheckBoxAudioOutput, 2, 1);
                /* Prepare audio input check-box: */
                m_pCheckBoxAudioInput = new QCheckBox(m_pWidgetAudioSettings);
                if (m_pCheckBoxAudioInput)
                    m_pLayoutAudioSettings->addWidget(m_pCheckBoxAudioInput, 3, 1);
            }

            pLayoutMain->addWidget(m_pWidgetAudioSettings, 1, 1);
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

bool UIMachineSettingsAudio::saveAudioData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save audio settings from the cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Get old audio data from the cache: */
        const UIDataSettingsMachineAudio &oldAudioData = m_pCache->base();
        /* Get new audio data from the cache: */
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
