/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsAudio class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIMachineSettingsAudio.h"
#include "UIConverter.h"

/* COM includes: */
#include "CAudioAdapter.h"

UIMachineSettingsAudio::UIMachineSettingsAudio()
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsAudio::setupUi (this);
    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
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

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsAudio::getFromCache()
{
    /* Get audio data from cache: */
    const UIDataSettingsMachineAudio &audioData = m_cache.base();

    /* Load audio data to page: */
    mGbAudio->setChecked(audioData.m_fAudioEnabled);
    mCbAudioDriver->setCurrentIndex(mCbAudioDriver->findText(gpConverter->toString(audioData.m_audioDriverType)));
    mCbAudioController->setCurrentIndex(mCbAudioController->findText(gpConverter->toString(audioData.m_audioControllerType)));

    /* Polish page finally: */
    polishPage();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsAudio::putToCache()
{
    /* Prepare audio data: */
    UIDataSettingsMachineAudio audioData = m_cache.base();

    /* Gather audio data: */
    audioData.m_fAudioEnabled = mGbAudio->isChecked();
    audioData.m_audioDriverType = gpConverter->fromString<KAudioDriverType>(mCbAudioDriver->currentText());
    audioData.m_audioControllerType = gpConverter->fromString<KAudioControllerType>(mCbAudioController->currentText());

    /* Cache audio data: */
    m_cache.cacheCurrentData(audioData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if audio data was changed: */
    if (m_cache.wasChanged())
    {
        /* Check if adapter still valid: */
        CAudioAdapter adapter = m_machine.GetAudioAdapter();
        if (!adapter.isNull())
        {
            /* Get audio data from cache: */
            const UIDataSettingsMachineAudio &audioData = m_cache.data();

            /* Store audio data: */
            if (isMachineOffline())
            {
                adapter.SetEnabled(audioData.m_fAudioEnabled);
                adapter.SetAudioDriver(audioData.m_audioDriverType);
                adapter.SetAudioController(audioData.m_audioControllerType);
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsAudio::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbAudio);
    setTabOrder (mGbAudio, mCbAudioDriver);
    setTabOrder (mCbAudioDriver, mCbAudioController);
}

void UIMachineSettingsAudio::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsAudio::retranslateUi (this);
    /* Fill the comboboxes */
    prepareComboboxes();
}

void UIMachineSettingsAudio::prepareComboboxes()
{
    /* Save the current selected value */
    int currentDriver = mCbAudioDriver->currentIndex();
    /* Clear the driver box */
    mCbAudioDriver->clear();
    /* Refill them */
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_Null));
#if defined Q_WS_WIN32
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_WinMM));
# endif
#endif
#if defined Q_OS_SOLARIS
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_SolAudio));
# if defined VBOX_WITH_SOLARIS_OSS
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_OSS));
#endif
#endif
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_OSS));
# ifdef VBOX_WITH_PULSE
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_Pulse));
# endif
#endif
#if defined Q_OS_LINUX
# ifdef VBOX_WITH_ALSA
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_ALSA));
# endif
#endif
#if defined Q_OS_MACX
    mCbAudioDriver->addItem (gpConverter->toString (KAudioDriverType_CoreAudio));
#endif
    /* Set the old value */
    mCbAudioDriver->setCurrentIndex (currentDriver);

    /* Save the current selected value */
    int currentController = mCbAudioController->currentIndex();
    /* Clear the controller box */
    mCbAudioController->clear();
    /* Refill them */
    mCbAudioController->insertItem (mCbAudioController->count(),
        gpConverter->toString (KAudioControllerType_HDA));
    mCbAudioController->insertItem (mCbAudioController->count(),
        gpConverter->toString (KAudioControllerType_AC97));
    mCbAudioController->insertItem (mCbAudioController->count(),
        gpConverter->toString (KAudioControllerType_SB16));
    /* Set the old value */
    mCbAudioController->setCurrentIndex (currentController);
}

void UIMachineSettingsAudio::polishPage()
{
    mGbAudio->setEnabled(isMachineOffline());
    mLbAudioDriver->setEnabled(isMachineOffline());
    mCbAudioDriver->setEnabled(isMachineOffline());
    mLbAudioController->setEnabled(isMachineOffline());
    mCbAudioController->setEnabled(isMachineOffline());
}

