/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsGeneral class implementation
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

#include "VBoxVMSettingsGeneral.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

#include <QDir>

VBoxVMSettingsGeneral::VBoxVMSettingsGeneral()
    : mValidator (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsGeneral::setupUi (this);

    /* Setup validators */
    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    /* Shared Clipboard mode */
    mCbClipboard->addItem (""); /* KClipboardMode_Disabled */
    mCbClipboard->addItem (""); /* KClipboardMode_HostToGuest */
    mCbClipboard->addItem (""); /* KClipboardMode_GuestToHost */
    mCbClipboard->addItem (""); /* KClipboardMode_Bidirectional */

    /* Applying language settings */
    retranslateUi();
}

bool VBoxVMSettingsGeneral::is64BitOSTypeSelected() const
{
    return mOSTypeSelector->type().GetIs64Bit();
}

void VBoxVMSettingsGeneral::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Name */
    mLeName->setText (aMachine.GetName());

    /* OS type */
    mOSTypeSelector->setType (vboxGlobal().vmGuestOSType (aMachine.GetOSTypeId()));

    /* Remember mediums mounted at runtime */
    QString saveRtimeImages = mMachine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
    mCbSaveMounted->setChecked (saveRtimeImages != "no");

    /* Snapshot folder */
    mPsSnapshot->setPath (aMachine.GetSnapshotFolder());
    mPsSnapshot->setHomeDir (QFileInfo (mMachine.GetSettingsFilePath()).absolutePath());

    /* Shared clipboard mode */
    mCbClipboard->setCurrentIndex (aMachine.GetClipboardMode());

    /* Description */
    mTeDescription->setPlainText (aMachine.GetDescription());

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsGeneral::putBackTo()
{
    /* Name */
    mMachine.SetName (mLeName->text());

    /* OS type */
    AssertMsg (!mOSTypeSelector->type().isNull(), ("mOSTypeSelector must return non-null type"));
    mMachine.SetOSTypeId (mOSTypeSelector->type().GetId());

    /* Remember mediums mounted at runtime */
    mMachine.SetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime,
                           mCbSaveMounted->isChecked() ? "yes" : "no");

    /* Saved state folder */
    if (mPsSnapshot->isModified())
    {
        mMachine.SetSnapshotFolder (mPsSnapshot->path());
        if (!mMachine.isOk())
            vboxProblem().cannotSetSnapshotFolder (mMachine,
                    QDir::toNativeSeparators (mPsSnapshot->path()));
    }

    /* Shared clipboard mode */
    mMachine.SetClipboardMode ((KClipboardMode) mCbClipboard->currentIndex());

    /* Description (set empty to null to avoid an empty <Description> node
     * in the settings file) */
    mMachine.SetDescription (mTeDescription->toPlainText().isEmpty() ?
                             QString::null : mTeDescription->toPlainText());
}

void VBoxVMSettingsGeneral::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mOSTypeSelector, SIGNAL (osTypeChanged()), mValidator, SLOT (revalidate()));
}

void VBoxVMSettingsGeneral::setOrderAfter (QWidget *aWidget)
{
    /* Setup Tab order */
    setTabOrder (aWidget, mTwGeneral->focusProxy());
    setTabOrder (mTwGeneral->focusProxy(), mLeName);
    setTabOrder (mLeName, mOSTypeSelector);

    setTabOrder (mOSTypeSelector, mPsSnapshot);
    setTabOrder (mPsSnapshot, mCbClipboard);
    setTabOrder (mCbClipboard, mCbSaveMounted);

    setTabOrder (mCbSaveMounted, mTeDescription);
}

void VBoxVMSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsGeneral::retranslateUi (this);

    /* Path selector */
    mPsSnapshot->setWhatsThis (tr ("Displays the path where snapshots of this "
                                   "virtual machine will be stored. Note that "
                                   "snapshots can take quite a lot of disk "
                                   "space."));

    /* Shared Clipboard mode */
    mCbClipboard->setItemText (0, vboxGlobal().toString (KClipboardMode_Disabled));
    mCbClipboard->setItemText (1, vboxGlobal().toString (KClipboardMode_HostToGuest));
    mCbClipboard->setItemText (2, vboxGlobal().toString (KClipboardMode_GuestToHost));
    mCbClipboard->setItemText (3, vboxGlobal().toString (KClipboardMode_Bidirectional));
}

void VBoxVMSettingsGeneral::showEvent (QShowEvent *aEvent)
{
    QCoreApplication::sendPostedEvents();
    VBoxSettingsPage::showEvent (aEvent);
}

