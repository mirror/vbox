/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSerial class declaration
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

#ifndef __VBoxVMSettingsSerial_h__
#define __VBoxVMSettingsSerial_h__

#include "VBoxVMSettingsSerial.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxVMSettingsDlg;

class VBoxVMSettingsSerial : public QIWithRetranslateUI<QWidget>,
                             public Ui::VBoxVMSettingsSerial
{
    Q_OBJECT;


public:
    VBoxVMSettingsSerial (QWidget* aParent = NULL);

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);

    static void putBackToMachine();

    static bool revalidate (QString &aWarning, QString &aTitle);

protected:

    void retranslateUi();

private slots:

    void mGbSerialToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);
    void mCbModeActivated (const QString &aText);

private:

    QString pageTitle() const;

    void getFromPort (const CSerialPort &aPort);
    void putBackToPort();
    bool isUserDefined();

    static QTabWidget *mTabWidget;

    CSerialPort mPort;
};

#endif // __VBoxVMSettingsSerial_h__

