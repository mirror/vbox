/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsGeneral class declaration
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxVMSettingsGeneral_h__
#define __VBoxVMSettingsGeneral_h__

#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsGeneral.gen.h"
#include "COMDefs.h"

class VBoxVMSettingsGeneral : public VBoxSettingsPage,
                              public Ui::VBoxVMSettingsGeneral
{
    Q_OBJECT;

public:

    VBoxVMSettingsGeneral();

    bool is64BitOSTypeSelected() const;

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isWindowsOSTypeSelected() const;
#endif

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aVal);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private:

    void showEvent (QShowEvent *aEvent);

    CMachine mMachine;
    QIWidgetValidator *mValidator;
};

#endif // __VBoxVMSettingsGeneral_h__

