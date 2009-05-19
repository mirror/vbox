/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSystem class declaration
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#ifndef __VBoxVMSettingsSystem_h__
#define __VBoxVMSettingsSystem_h__

#include "COMDefs.h"
#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsSystem.gen.h"

class VBoxVMSettingsSystem : public VBoxSettingsPage,
                             public Ui::VBoxVMSettingsSystem
{
    Q_OBJECT;

public:

    VBoxVMSettingsSystem();

    bool isHWVirtExEnabled() const;

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

    void valueChangedCPU (int aVal);
    void textChangedCPU (const QString &aText);

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    void adjustBootOrderTWSize();

    CMachine mMachine;
    QIWidgetValidator *mValidator;
};

#endif // __VBoxVMSettingsSystem_h__

