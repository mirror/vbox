/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsAudio class implementation.
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
    , m_pAudioHostDriverLabel(0)
    , m_pAudioControllerLabel(0)
    , m_pLabelAudioExtended(0)
    , m_pContainerAudioSubOptions(0)
    , m_pAudioHostDriverEditor(0)
    , m_pAudioControllerEditor(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsAudio::~UIMachineSettingsAudio()
{
    /* Cleanup: */
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
    m_pAudioHostDriverEditor->setValue(oldAudioData.m_audioDriverType);
    m_pAudioControllerEditor->setValue(oldAudioData.m_audioControllerType);
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
    newAudioData.m_audioDriverType = m_pAudioHostDriverEditor->value();
    newAudioData.m_audioControllerType = m_pAudioControllerEditor->value();
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
    m_pCheckBoxAudio->setWhatsThis(tr("When checked, a virtual PCI audio card"
                                      "will be plugged into the virtual machine and will communicate with"
                                      "the host audio system using the specified driver."));
    m_pCheckBoxAudio->setText(tr("Enable &Audio"));
    m_pAudioHostDriverLabel->setText(tr("Host Audio &Driver:"));
    m_pAudioHostDriverEditor->setWhatsThis(tr("Selects the audio output driver."
                                              "The <b>Null Audio Driver</b> makes the guest see an audio card,"
                                              "however every access to it will be ignored."));
    m_pAudioControllerLabel->setText(tr("Audio &Controller:"));
    m_pAudioControllerEditor->setWhatsThis(tr("Selects the type of the virtual sound"
                                              "card. Depending on this value, VirtualBox will provide different"
                                              "audio hardware to the virtual machine."));
    m_pLabelAudioExtended->setText(tr("Extended Features:"));
    m_pCheckBoxAudioOutput->setWhatsThis(tr("When checked, output to the virtual"
                                            "audio device will reach the host. Otherwise the guest is muted."));
    m_pCheckBoxAudioOutput->setText(tr("Enable Audio &Output"));
    m_pCheckBoxAudioInput->setWhatsThis(tr("When checked, the guest will be able"
                                           "to capture audio input from the host. Otherwise the guest will"
                                           "capture only silence."));
    m_pCheckBoxAudioInput->setText(tr("Enable Audio &Input"));

}

void UIMachineSettingsAudio::polishPage()
{
    /* Polish audio page availability: */
    m_pCheckBoxAudio->setEnabled(isMachineOffline());
    m_pAudioHostDriverLabel->setEnabled(isMachineOffline() || isMachineSaved());
    m_pAudioHostDriverEditor->setEnabled(isMachineOffline() || isMachineSaved());
    m_pAudioControllerLabel->setEnabled(isMachineOffline());
    m_pAudioControllerEditor->setEnabled(isMachineOffline());
    m_pLabelAudioExtended->setEnabled(isMachineInValidMode());
    m_pCheckBoxAudioOutput->setEnabled(isMachineInValidMode());
    m_pCheckBoxAudioInput->setEnabled(isMachineInValidMode());
    m_pContainerAudioSubOptions->setEnabled(m_pCheckBoxAudio->isChecked());
}

void UIMachineSettingsAudio::prepare()
{
    prepareWidgets();

    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineAudio;
    AssertPtrReturnVoid(m_pCache);

    /* Layout created in the .ui file. */
    {
        /* Audio host-driver label & editor created in the .ui file. */
        AssertPtrReturnVoid(m_pAudioHostDriverLabel);
        AssertPtrReturnVoid(m_pAudioHostDriverEditor);
        {
            /* Configure label & editor: */
            m_pAudioHostDriverLabel->setBuddy(m_pAudioHostDriverEditor->focusProxy());
        }

        /* Audio controller label & editor created in the .ui file. */
        AssertPtrReturnVoid(m_pAudioControllerLabel);
        AssertPtrReturnVoid(m_pAudioControllerEditor);
        {
            /* Configure label & editor: */
            m_pAudioControllerLabel->setBuddy(m_pAudioControllerEditor->focusProxy());
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsAudio::prepareWidgets()
{
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIMachineSettingsAudio"));
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setObjectName(QStringLiteral("pMainLayout"));
    QWidget *pContainerAudioOptions = new QWidget();
    pContainerAudioOptions->setObjectName(QStringLiteral("pContainerAudioOptions"));
    QGridLayout *pContainerLayoutAudioOptions = new QGridLayout(pContainerAudioOptions);
    pContainerLayoutAudioOptions->setObjectName(QStringLiteral("pContainerLayoutAudioOptions"));
    pContainerLayoutAudioOptions->setContentsMargins(0, 0, 0, 0);
    m_pCheckBoxAudio = new QCheckBox(pContainerAudioOptions);
    m_pCheckBoxAudio->setObjectName(QStringLiteral("m_pCheckBoxAudio"));
    pContainerLayoutAudioOptions->addWidget(m_pCheckBoxAudio, 0, 0, 1, 2);

    QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    pContainerLayoutAudioOptions->addItem(pSpacerItem, 1, 0, 1, 1);

    m_pContainerAudioSubOptions = new QWidget(pContainerAudioOptions);
    m_pContainerAudioSubOptions->setObjectName(QStringLiteral("m_pContainerAudioSubOptions"));
    QGridLayout *pContainerLayoutAudioSubOptions = new QGridLayout(m_pContainerAudioSubOptions);
    pContainerLayoutAudioSubOptions->setContentsMargins(0, 0, 0, 0);
    pContainerLayoutAudioSubOptions->setObjectName(QStringLiteral("pContainerLayoutAudioSubOptions"));
    pContainerLayoutAudioSubOptions->setContentsMargins(0, 0, 0, 0);
    m_pAudioHostDriverLabel = new QLabel(m_pContainerAudioSubOptions);
    m_pAudioHostDriverLabel->setObjectName(QStringLiteral("m_pAudioHostDriverLabel"));
    m_pAudioHostDriverLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutAudioSubOptions->addWidget(m_pAudioHostDriverLabel, 0, 0, 1, 1);

    m_pAudioHostDriverEditor = new UIAudioHostDriverEditor(m_pContainerAudioSubOptions);
    m_pAudioHostDriverEditor->setObjectName(QStringLiteral("m_pAudioHostDriverEditor"));
    pContainerLayoutAudioSubOptions->addWidget(m_pAudioHostDriverEditor, 0, 1, 1, 1);

    m_pAudioControllerLabel = new QLabel(m_pContainerAudioSubOptions);
    m_pAudioControllerLabel->setObjectName(QStringLiteral("m_pAudioControllerLabel"));
    m_pAudioControllerLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutAudioSubOptions->addWidget(m_pAudioControllerLabel, 1, 0, 1, 1);

    m_pAudioControllerEditor = new UIAudioControllerEditor(m_pContainerAudioSubOptions);
    m_pAudioControllerEditor->setObjectName(QStringLiteral("m_pAudioControllerEditor"));
    pContainerLayoutAudioSubOptions->addWidget(m_pAudioControllerEditor, 1, 1, 1, 1);

    m_pLabelAudioExtended = new QLabel(m_pContainerAudioSubOptions);
    m_pLabelAudioExtended->setObjectName(QStringLiteral("m_pLabelAudioExtended"));
    m_pLabelAudioExtended->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutAudioSubOptions->addWidget(m_pLabelAudioExtended, 2, 0, 1, 1);

    m_pCheckBoxAudioOutput = new QCheckBox(m_pContainerAudioSubOptions);
    m_pCheckBoxAudioOutput->setObjectName(QStringLiteral("m_pCheckBoxAudioOutput"));
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(1);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_pCheckBoxAudioOutput->sizePolicy().hasHeightForWidth());
    m_pCheckBoxAudioOutput->setSizePolicy(sizePolicy);
    pContainerLayoutAudioSubOptions->addWidget(m_pCheckBoxAudioOutput, 2, 1, 1, 1);

    m_pCheckBoxAudioInput = new QCheckBox(m_pContainerAudioSubOptions);
    m_pCheckBoxAudioInput->setObjectName(QStringLiteral("m_pCheckBoxAudioInput"));
    sizePolicy.setHeightForWidth(m_pCheckBoxAudioInput->sizePolicy().hasHeightForWidth());
    m_pCheckBoxAudioInput->setSizePolicy(sizePolicy);
    pContainerLayoutAudioSubOptions->addWidget(m_pCheckBoxAudioInput, 3, 1, 1, 1);

    pContainerLayoutAudioOptions->addWidget(m_pContainerAudioSubOptions, 1, 1, 1, 1);
    pMainLayout->addWidget(pContainerAudioOptions);

    QSpacerItem *pSpacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pMainLayout->addItem(pSpacerItem1);

    QObject::connect(m_pCheckBoxAudio, &QCheckBox::toggled, m_pContainerAudioSubOptions, &QWidget::setEnabled);
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
