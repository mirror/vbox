/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsAudio class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
# include "UIMachineSettingsAudio.h"
# include "UIConverter.h"

/* COM includes: */
# include "CAudioAdapter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineSettingsAudio::UIMachineSettingsAudio()
{
    /* Prepare: */
    prepare();
}

void UIMachineSettingsAudio::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

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
    m_cache.cacheInitialData(audioData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::getFromCache()
{
    /* Get audio data from cache: */
    const UIDataSettingsMachineAudio &audioData = m_cache.base();

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
    UIDataSettingsMachineAudio audioData = m_cache.base();

    /* Gather audio data: */
    audioData.m_fAudioEnabled = m_pCheckBoxAudio->isChecked();
    audioData.m_audioDriverType = static_cast<KAudioDriverType>(m_pComboAudioDriver->itemData(m_pComboAudioDriver->currentIndex()).toInt());
    audioData.m_audioControllerType = static_cast<KAudioControllerType>(m_pComboAudioController->itemData(m_pComboAudioController->currentIndex()).toInt());

    /* Cache audio data: */
    m_cache.cacheCurrentData(audioData);
}

void UIMachineSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in 'offline' mode & audio data was changed: */
    if (isMachineOffline() && m_cache.wasChanged())
    {
        /* Check if adapter still valid: */
        CAudioAdapter audioAdapter = m_machine.GetAudioAdapter();
        if (!audioAdapter.isNull())
        {
            /* Get audio data from cache: */
            const UIDataSettingsMachineAudio &audioData = m_cache.data();

            /* Store audio data: */
            audioAdapter.SetEnabled(audioData.m_fAudioEnabled);
            audioAdapter.SetAudioDriver(audioData.m_audioDriverType);
            audioAdapter.SetAudioController(audioData.m_audioControllerType);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::setOrderAfter(QWidget *pWidget)
{
    /* Audio-page order: */
    setTabOrder(pWidget, m_pCheckBoxAudio);
    setTabOrder(m_pCheckBoxAudio, m_pComboAudioDriver);
    setTabOrder(m_pComboAudioDriver, m_pComboAudioController);
}

void UIMachineSettingsAudio::retranslateUi()
{
    /* Translate generated strings: */
    Ui::UIMachineSettingsAudio::retranslateUi(this);


    /* Translate audio-driver combo.
     * Make sure this order corresponds the same in prepareComboboxes(): */
    int iIndex = -1;

    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_Null));

#ifdef Q_OS_WIN
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    m_pComboAudioDriver->setItemText(++iIndex, gpConverter->toString(KAudioDriverType_WinMM));
# endif /* VBOX_WITH_WINMM */
#endif /* Q_OS_WIN */

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
#endif /* Q_OS_MACX */


    /* Translate audio-controller combo.
     * Make sure this order corresponds the same in prepareComboboxes(): */
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

    /* Prepare combo-boxes: */
    prepareComboboxes();

    /* Translate finally: */
    retranslateUi();
}

void UIMachineSettingsAudio::prepareComboboxes()
{
    /* Prepare audio-driver combo.
     * Make sure this order corresponds the same in retranslateUi(): */
    int iIndex = -1;

    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_Null);

#ifdef Q_OS_WIN
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_DirectSound);
# ifdef VBOX_WITH_WINMM
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_WinMM);
# endif /* VBOX_WITH_WINMM */
#endif /* Q_OS_WIN */
#ifdef VBOX_WITH_AUDIO_OSS
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_OSS);
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_ALSA);
#endif /* VBOX_WITH_AUDIO_ALSA */
#ifdef VBOX_WITH_AUDIO_PULSE
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_Pulse);
#endif /* VBOX_WITH_AUDIO_PULSE */
#ifdef Q_OS_MACX
    m_pComboAudioDriver->insertItem(++iIndex, "", KAudioDriverType_CoreAudio);
#endif /* Q_OS_MACX */

    /* Prepare audio-controller combo.
     * Make sure this order corresponds the same in retranslateUi(): */
    iIndex = -1;

    m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_HDA);
    m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_AC97);
    m_pComboAudioController->insertItem(++iIndex, "", KAudioControllerType_SB16);
}

