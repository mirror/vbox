/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSettingsDialogSpecific class declaration
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

#ifndef __VBoxSettingsDialogSpecific_h__
#define __VBoxSettingsDialogSpecific_h__

#include "VBoxSettingsDialog.h"
#include "COMDefs.h"

class VBoxGlobalSettings;
class VBoxSettingsPage;

/*
 * Dialog which encapsulate all the specific funtionalities of the
 * Global Settings.
 */
class VBoxGLSettingsDlg : public VBoxSettingsDialog
{
    Q_OBJECT;

public:

    enum GLSettingsPageIds
    {
        GeneralId = 0,
        InputId,
        UpdateId,
        LanguageId,
        USBId
    };

    VBoxGLSettingsDlg (QWidget *aParent);

protected:

    void getFrom();
    void putBackTo();

    void retranslateUi();

    QString dialogTitle() const;

private:

    void updateAvailability();
};

/*
 * Dialog which encapsulate all the specific funtionalities of the
 * Virtual Machine Settings.
 */
class VBoxVMSettingsDlg : public VBoxSettingsDialog
{
    Q_OBJECT;

public:

    enum VMSettingsPageIds
    {
        GeneralId = 0,
        StorageId,
        HDId,
        CDId,
        FDId,
        AudioId,
        NetworkId,
        PortsId,
        SerialId,
        ParallelId,
        USBId,
        SFId,
        VRDPId
    };

    VBoxVMSettingsDlg (QWidget *aParent, const CMachine &aMachine,
                       const QString &aCategory, const QString &aControl);

protected slots:

    void revalidate (QIWidgetValidator *aWval);

protected:

    void getFrom();
    void putBackTo();

    void retranslateUi();

    QString dialogTitle() const;

private slots:

    void onMediaEnumerationDone();
    void resetFirstRunFlag();

private:

    void addItem (const QString &aBigIcon, const QString &aBigIconDisabled, const QString &aSmallIcon, const QString &aSmallIconDisabled, int aId, const QString &aLink, VBoxSettingsPage* aPrefPage = NULL, int aParentId = -1);
    void updateAvailability();
    VBoxSettingsPage* attachValidator (VBoxSettingsPage *aPage);

    CMachine mMachine;
    bool mAllowResetFirstRunFlag;
    bool mResetFirstRunFlag;
};

#endif // __VBoxSettingsDialogSpecific_h__

