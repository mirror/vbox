/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsParallel class declaration
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

#ifndef __VBoxVMSettingsParallel_h__
#define __VBoxVMSettingsParallel_h__

#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsParallel.gen.h"
#include "COMDefs.h"

class VBoxVMSettingsParallel : public QIWithRetranslateUI<QWidget>,
                               public Ui::VBoxVMSettingsParallel
{
    Q_OBJECT;

public:

    VBoxVMSettingsParallel();

    void getFromPort (const CParallelPort &aPort);
    void putBackToPort();

    void setValidator (QIWidgetValidator *aVal);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbParallelToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);

private:

    QIWidgetValidator *mValidator;
    CParallelPort mPort;
};

class VBoxVMSettingsParallelPage : public VBoxSettingsPage
{
    Q_OBJECT;

public:

    VBoxVMSettingsParallelPage();

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void retranslateUi();

private:

    QIWidgetValidator *mValidator;
    QTabWidget *mTabWidget;
};

#endif // __VBoxVMSettingsParallel_h__

