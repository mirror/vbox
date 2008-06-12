/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsAudio class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxVMSettingsAudio.h"
#include "VBoxGlobal.h"

VBoxVMSettingsAudio* VBoxVMSettingsAudio::mSettings = 0;

VBoxVMSettingsAudio::VBoxVMSettingsAudio (QWidget *aParent)
    : QWidget (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsAudio::setupUi (this);

    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_Null));
#if defined Q_WS_WIN32
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_WinMM));
# endif
#elif defined Q_OS_LINUX
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_OSS));
# ifdef VBOX_WITH_ALSA
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_ALSA));
# endif
# ifdef VBOX_WITH_PULSE
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_Pulse));
# endif
#elif defined Q_OS_MACX
    mCbAudioDriver->insertItem (mCbAudioDriver->count(),
        vboxGlobal().toString (KAudioDriverType_CoreAudio));
#endif

    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_AC97));
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_SB16));
}

void VBoxVMSettingsAudio::getFromMachine (const CMachine &aMachine,
                                          QWidget *aPage)
{
    mSettings = new VBoxVMSettingsAudio (aPage);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    mSettings->getFrom (aMachine);
}

void VBoxVMSettingsAudio::putBackToMachine()
{
    mSettings->putBackTo();
}

void VBoxVMSettingsAudio::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    CAudioAdapter audio = aMachine.GetAudioAdapter();
    mGbAudio->setChecked (audio.GetEnabled());
    mCbAudioDriver->setCurrentIndex (mCbAudioDriver->
        findText (vboxGlobal().toString (audio.GetAudioDriver())));
    mCbAudioController->setCurrentIndex (mCbAudioController->
        findText (vboxGlobal().toString (audio.GetAudioController())));
}

void VBoxVMSettingsAudio::putBackTo()
{
    CAudioAdapter audio = mMachine.GetAudioAdapter();
    audio.SetAudioDriver (vboxGlobal().toAudioDriverType (mCbAudioDriver->currentText()));
    audio.SetAudioController (vboxGlobal().toAudioControllerType (mCbAudioController->currentText()));
    audio.SetEnabled (mGbAudio->isChecked());
    AssertWrapperOk (audio);
}

