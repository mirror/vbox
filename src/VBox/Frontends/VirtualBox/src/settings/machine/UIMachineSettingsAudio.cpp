/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsAudio class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIConverter.h"
# include "UIMachineSettingsAudio.h"

/* COM includes: */
# include "CAudioAdapter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: Audio page data structure. */
struct UIDataSettingsMachineAudio
{
    /** Constructs data. */
    UIDataSettingsMachineAudio()
        : m_fAudioEnabled(false)
        , m_audioDriverType(KAudioDriverType_Null)
        , m_audioControllerType(KAudioControllerType_AC97)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineAudio &other) const
    {
        return true
               && (m_fAudioEnabled == other.m_fAudioEnabled)
               && (m_audioDriverType == other.m_audioDriverType)
               && (m_audioControllerType == other.m_audioControllerType)
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
};


UIMachineSettingsAudio::UIMachineSettingsAudio()
    : m_pCache(0)
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

    /* Prepare audio data: */
    UIDataSettingsMachineAudio audioData;

    /* Check if adapter is valid: */
    const CAudioAdapter &adapter = m_machine.GetAudioAdapter();
    if (!adapter.isNull())
    {
        /* Gather audio data: */
        audioData.m_fAudioEnabled = adapter.GetEnabled();
        audioData.m_audioDriverType = adapter.GetAudioDriver();
        audioData.m_audioControllerType = adapter.GetAudioController();
    }

    /* Cache audio data: */
    m_pCache->cacheInitialData(audioData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::getFromCache()
{
    /* Get audio data from cache: */
    const UIDataSettingsMachineAudio &audioData = m_pCache->base();

    /* Load audio data to page: */
    m_pCheckBoxAudio->setChecked(audioData.m_fAudioEnabled);
    m_pComboAudioDriver->setCurrentIndex(m_pComboAudioDriver->findData((int)audioData.m_audioDriverType));
    m_pComboAudioController->setCurrentIndex(m_pComboAudioController->findData((int)audioData.m_audioControllerType));

    /* Polish page finally: */
    polishPage();
}

void UIMachineSettingsAudio::putToCache()
{
    /* Prepare audio data: */
    UIDataSettingsMachineAudio audioData = m_pCache->base();

    /* Gather audio data: */
    audioData.m_fAudioEnabled = m_pCheckBoxAudio->isChecked();
    audioData.m_audioDriverType = static_cast<KAudioDriverType>(m_pComboAudioDriver->itemData(m_pComboAudioDriver->currentIndex()).toInt());
    audioData.m_audioControllerType = static_cast<KAudioControllerType>(m_pComboAudioController->itemData(m_pComboAudioController->currentIndex()).toInt());

    /* Cache audio data: */
    m_pCache->cacheCurrentData(audioData);
}

void UIMachineSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in 'offline' mode & audio data was changed: */
    if (isMachineOffline() && m_pCache->wasChanged())
    {
        /* Check if adapter still valid: */
        CAudioAdapter audioAdapter = m_machine.GetAudioAdapter();
        if (!audioAdapter.isNull())
        {
            /* Get audio data from cache: */
            const UIDataSettingsMachineAudio &audioData = m_pCache->data();

            /* Store audio data: */
            audioAdapter.SetEnabled(audioData.m_fAudioEnabled);
            audioAdapter.SetAudioDriver(audioData.m_audioDriverType);
            audioAdapter.SetAudioController(audioData.m_audioControllerType);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::retranslateUi()
{
    /* Translate generated strings: */
    Ui::UIMachineSettingsAudio::retranslateUi(this);

    /* Translate audio-driver combo.
     * Make sure this order corresponds the same in prepare(): */
    int iIndex = -1;
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_Null));
#ifdef Q_OS_WIN
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_WinMM));
# endif
#endif
#ifdef VBOX_WITH_AUDIO_OSS
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_OSS));
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_ALSA));
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_Pulse));
#endif
#ifdef Q_OS_MACX
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_CoreAudio));
#endif

    /* Translate audio-controller combo.
     * Make sure this order corresponds the same in prepare(): */
    iIndex = -1;
    m_pComboAudioController->setItemText(++iIndex, gpConverter->toString(KAudioControllerType_HDA));
    m_pComboAudioController->setItemText(++iIndex, gpConverter->toString(KAudioControllerType_AC97));
    m_pComboAudioController->setItemText(++iIndex, gpConverter->toString(KAudioControllerType_SB16));
}

void UIMachineSettingsAudio::polishPage()
{
    /* Polish audio-page availability: */
    m_pContainerAudioOptions->setEnabled(isMachineOffline());
    m_pContainerAudioSubOptions->setEnabled(m_pCheckBoxAudio->isChecked());
}

void UIMachineSettingsAudio::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsAudio::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineAudio;
    AssertPtrReturnVoid(m_pCache);

    /* Layout created in the .ui file. */
    {
        /* Audio-driver combo-box created in the .ui file. */
        AssertPtrReturnVoid(m_pComboAudioDriver);
        {
            /* Configure combo-box.
             * Make sure this order corresponds the same in retranslateUi(): */
            int iIndex = -1;
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_Null);
#ifdef Q_OS_WIN
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_DirectSound);
# ifdef VBOX_WITH_WINMM
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_WinMM);
# endif
#endif
#ifdef VBOX_WITH_AUDIO_OSS
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_OSS);
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_ALSA);
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_Pulse);
#endif
#ifdef Q_OS_MACX
            m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_CoreAudio);
#endif
        }

        /* Audio-controller combo-box created in the .ui file. */
        AssertPtrReturnVoid(m_pComboAudioController);
        {
            /* Configure combo-box.
             * Make sure this order corresponds the same in retranslateUi(): */
            int iIndex = -1;
            m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_HDA);
            m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_AC97);
            m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_SB16);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsAudio::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

