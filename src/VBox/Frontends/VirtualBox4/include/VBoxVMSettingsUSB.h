/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsUSB class declaration
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

#ifndef __VBoxVMSettingsUSB_h__
#define __VBoxVMSettingsUSB_h__

#include "VBoxVMSettingsUSB.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxGlobalSettingsDlg;
class VBoxVMSettingsDlg;
class QIWidgetValidator;
class VBoxUSBMenu;

class VBoxVMSettingsUSB : public QIWithRetranslateUI<QWidget>,
                          public Ui::VBoxVMSettingsUSB
{
    Q_OBJECT;

public:

    enum FilterType
    {
        WrongType = 0,
        HostType = 1,
        MachineType = 2
    };

    static void getFrom (QWidget *aPage,
                         VBoxGlobalSettingsDlg *aDlg,
                         const QString &aPath);
    static void getFrom (const CMachine &aMachine,
                         QWidget *aPage,
                         VBoxVMSettingsDlg *aDlg,
                         const QString &aPath);
    static void putBackTo();

protected:

    VBoxVMSettingsUSB (QWidget *aParent,
                       FilterType aType,
                       QWidget *aDlg,
                       const QString &aPath);

   ~VBoxVMSettingsUSB();

    void getFromHost();
    void putBackToHost();

    void getFromMachine (const CMachine &aMachine);
    void putBackToMachine();

    void retranslateUi();

    FilterType mType;

private slots:

    void usbAdapterToggled (bool aOn);
    void currentChanged (QTreeWidgetItem *aItem = 0,
                         QTreeWidgetItem *aPrev = 0);
    void setCurrentText (const QString &aText);
    void newClicked();
    void addClicked();
    void addConfirmed (QAction *aAction);
    void delClicked();
    void mupClicked();
    void mdnClicked();
    void showContextMenu (const QPoint &aPos);

private:

    void addUSBFilter (const CUSBDeviceFilter &aFilter, bool isNew);

    static VBoxVMSettingsUSB *mSettings;

    CMachine mMachine;
    QIWidgetValidator *mValidator;
    QAction *mNewAction;
    QAction *mAddAction;
    QAction *mDelAction;
    QAction *mMupAction;
    QAction *mMdnAction;
    QMenu *mMenu;
    VBoxUSBMenu *mUSBDevicesMenu;
    bool mUSBFilterListModified;
    QList<CUSBDeviceFilter> mFilters;

    QString mUSBFilterName;
};

#endif // __VBoxVMSettingsUSB_h__

