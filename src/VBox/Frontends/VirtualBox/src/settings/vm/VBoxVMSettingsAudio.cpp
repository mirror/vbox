/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsAudio class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxVMSettingsAudio.h"
#include "VBoxGlobal.h"

VBoxVMSettingsAudio::VBoxVMSettingsAudio()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsAudio::setupUi (this);
    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void VBoxVMSettingsAudio::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Fill internal variables with corresponding values: */
    const CAudioAdapter &audio = m_machine.GetAudioAdapter();
    m_cache.m_fAudioEnabled = audio.GetEnabled();
    m_cache.m_audioDriverType = audio.GetAudioDriver();
    m_cache.m_audioControllerType = audio.GetAudioController();

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void VBoxVMSettingsAudio::getFromCache()
{
    /* Apply internal variables data to QWidget(s): */
    mGbAudio->setChecked(m_cache.m_fAudioEnabled);
    mCbAudioDriver->setCurrentIndex(mCbAudioDriver->findText(vboxGlobal().toString(m_cache.m_audioDriverType)));
    mCbAudioController->setCurrentIndex(mCbAudioController->findText(vboxGlobal().toString(m_cache.m_audioControllerType)));
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void VBoxVMSettingsAudio::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    m_cache.m_fAudioEnabled = mGbAudio->isChecked();
    m_cache.m_audioDriverType = vboxGlobal().toAudioDriverType(mCbAudioDriver->currentText());
    m_cache.m_audioControllerType = vboxGlobal().toAudioControllerType(mCbAudioController->currentText());
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void VBoxVMSettingsAudio::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Gather corresponding values from internal variables: */
    CAudioAdapter &audio = m_machine.GetAudioAdapter();
    audio.SetEnabled(m_cache.m_fAudioEnabled);
    audio.SetAudioDriver(m_cache.m_audioDriverType);
    audio.SetAudioController(m_cache.m_audioControllerType);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void VBoxVMSettingsAudio::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbAudio);
    setTabOrder (mGbAudio, mCbAudioDriver);
    setTabOrder (mCbAudioDriver, mCbAudioController);
}

void VBoxVMSettingsAudio::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsAudio::retranslateUi (this);
    /* Fill the comboboxes */
    prepareComboboxes();
}

void VBoxVMSettingsAudio::prepareComboboxes()
{
    /* Save the current selected value */
    int currentDriver = mCbAudioDriver->currentIndex();
    /* Clear the driver box */
    mCbAudioDriver->clear();
    /* Refill them */
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_Null));
#if defined Q_WS_WIN32
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_WinMM));
# endif
#endif
#if defined Q_OS_SOLARIS
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_SolAudio));
# if defined VBOX_WITH_SOLARIS_OSS
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_OSS));
#endif
#endif
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_OSS));
# ifdef VBOX_WITH_PULSE
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_Pulse));
# endif
#endif
#if defined Q_OS_LINUX
# ifdef VBOX_WITH_ALSA
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_ALSA));
# endif
#endif
#if defined Q_OS_MACX
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_CoreAudio));
#endif
    /* Set the old value */
    mCbAudioDriver->setCurrentIndex (currentDriver);

    /* Save the current selected value */
    int currentController = mCbAudioController->currentIndex();
    /* Clear the controller box */
    mCbAudioController->clear();
    /* Refill them */
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_HDA));
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_AC97));
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_SB16));
    /* Set the old value */
    mCbAudioController->setCurrentIndex (currentController);
}

