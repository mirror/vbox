/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsFD class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "VBoxVMSettingsFD.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxMediaManagerDlg.h"

#include <QFileInfo>

VBoxVMSettingsFD::VBoxVMSettingsFD()
    : mValidator (0)
    , mLastSelected (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsFD::setupUi (this);

    /* Setup connections */
    connect (mGbFD, SIGNAL (toggled (bool)), this, SLOT (onGbChange (bool)));
    connect (mRbHostFD, SIGNAL (toggled (bool)), this, SLOT (onRbChange()));
    connect (mRbIsoFD, SIGNAL (toggled (bool)), this, SLOT (onRbChange()));
    connect (mCbIsoFD, SIGNAL (activated (int)), this, SLOT (onCbChange()));
    connect (mTbIsoFD, SIGNAL (clicked()), this, SLOT (showMediaManager()));

    /* Setup iconsets */
    mTbIsoFD->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                            ":/select_file_dis_16px.png"));

    /* Setup dialog */
    mCbIsoFD->setType (VBoxDefs::MediaType_Floppy);

    mLastSelected = mRbHostFD;
    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsFD::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Read out the host floppy drive list and prepare the combobox */
    CHostFloppyDriveVector coll =
        vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
    mHostFDs.resize (coll.size());
    mCbHostFD->clear();

    for (int id = 0; id < coll.size(); ++id)
    {
        CHostFloppyDrive hostFloppy = coll[id];
        /** @todo set icon? */
        QString name = hostFloppy.GetName();
        QString description = hostFloppy.GetDescription();
        QString fullName = description.isEmpty() ?
            name :
            QString ("%1 (%2)").arg (description, name);
        mCbHostFD->insertItem (id, fullName);
        mHostFDs [id] = hostFloppy;
    }

    CFloppyDrive floppy = aMachine.GetFloppyDrive();
    switch (floppy.GetState())
    {
        case KDriveState_HostDriveCaptured:
        {
            CHostFloppyDrive drv = floppy.GetHostDrive();
            QString name = drv.GetName();
            QString description = drv.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            if (vboxGlobal().virtualBox().GetHost().FindHostFloppyDrive (name).isNull())
            {
                /* If the floppy drive is not currently available, add it to
                 * the end of the list with a special mark */
                mCbHostFD->insertItem (mCbHostFD->count(), "* " + fullName);
                mCbHostFD->setCurrentIndex (mCbHostFD->count() - 1);
            }
            else
            {
                /* This will select the correct item from the prepared list. */
                mCbHostFD->setCurrentIndex (mCbHostFD->findText (fullName));
            }
            mRbHostFD->setChecked (true);
            mRbHostFD->setAutoExclusive (true);
            mRbIsoFD->setAutoExclusive (true);
            break;
        }
        case KDriveState_ImageMounted:
        {
            CFloppyImage img = floppy.GetImage();
            QString src = img.GetLocation();
            AssertMsg (!src.isNull(), ("Image file must not be null"));
            QFileInfo fi (src);
            mRbIsoFD->setChecked (true);
            mUuidIsoFD = QString (img.GetId());
            mRbHostFD->setAutoExclusive (true);
            mRbIsoFD->setAutoExclusive (true);
            break;
        }
        case KDriveState_NotMounted:
        {
            mGbFD->setChecked (false);
            mRbHostFD->setAutoExclusive (false);
            mRbIsoFD->setAutoExclusive (false);
            break;
        }
        default:
            AssertMsgFailed (("invalid floppy state: %d\n", floppy.GetState()));
    }

    mCbIsoFD->setMachineId (mMachine.GetId());
    mCbIsoFD->setCurrentItem (mUuidIsoFD);
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mCbIsoFD->refresh();

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsFD::putBackTo()
{
    CFloppyDrive floppy = mMachine.GetFloppyDrive();
    if (!mGbFD->isChecked())
    {
        floppy.Unmount();
    }
    else if (mRbHostFD->isChecked())
    {
        int id = mCbHostFD->currentIndex();
        Assert (id >= 0);
        if (id < (int) mHostFDs.count())
            floppy.CaptureHostDrive (mHostFDs [id]);
    }
    else if (mRbIsoFD->isChecked())
    {
        Assert (!mUuidIsoFD.isNull());
        floppy.MountImage (mUuidIsoFD);
    }
}

void VBoxVMSettingsFD::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsFD::revalidate (QString &aWarning, QString &)
{
    if (mRbHostFD->isChecked() && mCbHostFD->currentText().isNull())
        aWarning = tr ("Host floppy drive is not selected");
    else if (mRbIsoFD->isChecked() && mUuidIsoFD.isNull())
        aWarning = tr ("Floppy image file is not selected");

    return aWarning.isNull();
}

void VBoxVMSettingsFD::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbFD);
    setTabOrder (mGbFD, mRbHostFD);
    setTabOrder (mRbHostFD, mCbHostFD);
    setTabOrder (mCbHostFD, mRbIsoFD);
    setTabOrder (mRbIsoFD, mCbIsoFD);
    setTabOrder (mCbIsoFD, mTbIsoFD);
}

void VBoxVMSettingsFD::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsFD::retranslateUi (this);
}

void VBoxVMSettingsFD::onGbChange (bool aSwitchedOn)
{
    /* Toggle auto-exclusiveness on/off to let the buttons be unchecked both in
     * case of group-box is not checked and exclusively checked in case of
     * group-box is checked. */
    mRbHostFD->setAutoExclusive (aSwitchedOn);
    mRbIsoFD->setAutoExclusive (aSwitchedOn);

    /* Toggle both buttons off when the group box unchecked. */
    if (!aSwitchedOn)
    {
        mLastSelected = mRbIsoFD->isChecked() ? mRbIsoFD : mRbHostFD;

        mRbHostFD->blockSignals (true);
        mRbIsoFD->blockSignals (true);
        mRbHostFD->setChecked (false);
        mRbIsoFD->setChecked (false);
        mRbHostFD->blockSignals (false);
        mRbIsoFD->blockSignals (false);
    }
    /* Toggle last checked button on when the group box checked. */
    else if (!mRbHostFD->isChecked() && !mRbIsoFD->isChecked())
    {
        mLastSelected->blockSignals (true);
        mLastSelected->setChecked (true);
        mLastSelected->blockSignals (false);
    }

    mMountFDChild->setEnabled (aSwitchedOn);

    onRbChange();
}

void VBoxVMSettingsFD::onRbChange()
{
    /* Check the 'host' group. */
    mCbHostFD->setEnabled (mRbHostFD->isChecked());

    /* Check the 'iso' group. */
    mCbIsoFD->setEnabled (mRbIsoFD->isChecked());
    mTbIsoFD->setEnabled (mRbIsoFD->isChecked());

    onCbChange();
}

void VBoxVMSettingsFD::onCbChange()
{
    mUuidIsoFD = mGbFD->isChecked() ? mCbIsoFD->id() : QString::null;
    emit fdChanged();
    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsFD::showMediaManager()
{
    QString oldId = mUuidIsoFD;
    VBoxMediaManagerDlg dlg (this);

    dlg.setup (VBoxDefs::MediaType_Floppy, true /* aDoSelect */,
               false /* aRefresh */, mMachine, mCbIsoFD->id());

    QString newId = dlg.exec() == QDialog::Accepted ?
                  dlg.selectedId() : mCbIsoFD->id();
    if (oldId != newId)
    {
        mUuidIsoFD = newId;
        mCbIsoFD->setCurrentItem (mUuidIsoFD);
        emit fdChanged();
    }

    mCbIsoFD->setFocus();
    if (mValidator)
        mValidator->revalidate();
}

