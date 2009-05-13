/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsGeneral class declaration
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

signals:

    void tableChanged();

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void valueChangedRAM (int aVal);
    void textChangedRAM (const QString &aText);

    void moveBootItemUp();
    void moveBootItemDown();
    void onCurrentBootItemChanged (QTreeWidgetItem *aItem,
                                   QTreeWidgetItem *aPrev = 0);

    void stateChangedVirt (int aState);

private:

    void adjustBootOrderTWSize();

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

    CMachine mMachine;
    QIWidgetValidator *mValidator;
};

#endif // __VBoxVMSettingsGeneral_h__

