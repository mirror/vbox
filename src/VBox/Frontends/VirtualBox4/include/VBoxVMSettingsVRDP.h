/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsVRDP class declaration
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

#ifndef __VBoxVMSettingsVRDP_h__
#define __VBoxVMSettingsVRDP_h__

#include "VBoxVMSettingsVRDP.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxVMSettingsDlg;
class QIWidgetValidator;

class VBoxVMSettingsVRDP : public QIWithRetranslateUI<QWidget>,
                           public Ui::VBoxVMSettingsVRDP
{
    Q_OBJECT;

public:

    VBoxVMSettingsVRDP (QWidget *aParent, VBoxVMSettingsDlg *aDlg,
                        const QString &aPath);

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);
    static void putBackToMachine();

    void getFrom (const CMachine &aMachine);
    void putBackTo();

protected:

    void retranslateUi();

private:

    static VBoxVMSettingsVRDP *mSettings;

    CMachine mMachine;
    QIWidgetValidator *mValidator;
};

#endif // __VBoxVMSettingsVRDP_h__

