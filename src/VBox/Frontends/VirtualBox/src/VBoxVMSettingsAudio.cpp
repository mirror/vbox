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

VBoxVMSettingsAudio::VBoxVMSettingsAudio()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsAudio::setupUi (this);
    /* Applying language settings */
    retranslateUi();
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
#elif defined Q_OS_SOLARIS
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_SolAudio));
#elif defined Q_OS_LINUX
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_OSS));
# ifdef VBOX_WITH_ALSA
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_ALSA));
# endif
# ifdef VBOX_WITH_PULSE
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_Pulse));
# endif
#elif defined Q_OS_MACX
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_CoreAudio));
#elif defined Q_OS_FREEBSD
    mCbAudioDriver->addItem (vboxGlobal().toString (KAudioDriverType_OSS));
#endif
    /* Set the old value */
    mCbAudioDriver->setCurrentIndex (currentDriver);

    /* Save the current selected value */
    int currentController = mCbAudioController->currentIndex();
    /* Clear the controller box */
    mCbAudioController->clear();
    /* Refill them */
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_AC97));
    mCbAudioController->insertItem (mCbAudioController->count(),
        vboxGlobal().toString (KAudioControllerType_SB16));
    /* Set the old value */
    mCbAudioController->setCurrentIndex (currentController);
}

