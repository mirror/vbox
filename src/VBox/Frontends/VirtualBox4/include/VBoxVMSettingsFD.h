/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsFD class declaration
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

#ifndef __VBoxVMSettingsFD_h__
#define __VBoxVMSettingsFD_h__

#include "VBoxVMSettingsFD.gen.h"
#include "COMDefs.h"

class VBoxVMSettingsDlg;
class QIWidgetValidator;

class VBoxVMSettingsFD : public QWidget,
                         public Ui::VBoxVMSettingsFD
{
    Q_OBJECT

public:

    VBoxVMSettingsFD (QWidget *aParent, VBoxVMSettingsDlg *aDlg,
                      const QString &aPath);

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);
    static void putBackToMachine();
    static bool revalidate (QString &aWarning);

    void getFrom (const CMachine &aMachine);
    void putBackTo();
    bool validate (QString &aWarning);

signals:

    void fdChanged();

private slots:

    void onMediaChanged();
    void showImageManager();

private:

    static VBoxVMSettingsFD *mSettings;

    CMachine mMachine;
    QIWidgetValidator *mValidator;
    QVector<CHostFloppyDrive> mHostFDs;
    QUuid mUuidIsoFD;
    QRadioButton *mLastSelected;
};

#endif // __VBoxVMSettingsFD_h__

