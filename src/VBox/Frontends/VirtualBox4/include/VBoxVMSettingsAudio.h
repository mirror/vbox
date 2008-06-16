/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsAudio class declaration
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

#ifndef __VBoxVMSettingsAudio_h__
#define __VBoxVMSettingsAudio_h__

#include "VBoxVMSettingsAudio.gen.h"
#include "COMDefs.h"

class VBoxVMSettingsAudio : public QWidget, public Ui::VBoxVMSettingsAudio
{
    Q_OBJECT;

public:

    VBoxVMSettingsAudio (QWidget *aParent);

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage);
    static void putBackToMachine();

    void getFrom (const CMachine &aMachine);
    void putBackTo();

private:

    static VBoxVMSettingsAudio *mSettings;

    CMachine mMachine;
};

#endif // __VBoxVMSettingsAudio_h__

