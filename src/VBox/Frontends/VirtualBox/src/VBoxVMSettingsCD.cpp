/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsCD class implementation
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

#include "VBoxVMSettingsCD.h"
#include "VBoxGlobal.h"
#include "QIWidgetValidator.h"
#include "VBoxMediaManagerDlg.h"

#include <QFileInfo>

VBoxVMSettingsCD::VBoxVMSettingsCD()
    : mValidator (0)
    , mLastSelected (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsCD::setupUi (this);

    /* Setup connections */
    connect (mGbCD, SIGNAL (toggled (bool)), this, SLOT (onGbChange (bool)));
    connect (mRbHostCD, SIGNAL (toggled (bool)), this, SLOT (onRbChange()));
    connect (mRbIsoCD, SIGNAL (toggled (bool)), this, SLOT (onRbChange()));
    connect (mCbIsoCD, SIGNAL (activated (int)), this, SLOT (onCbChange()));
    connect (mTbIsoCD, SIGNAL (clicked()), this, SLOT (showMediaManager()));

    /* Setup iconsets */
    mTbIsoCD->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                            ":/select_file_dis_16px.png"));

    /* Setup dialog */
    mCbIsoCD->setType (VBoxDefs::MediaType_DVD);
    mLastSelected = mRbHostCD;

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsCD::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Read out the host DVD drive list and prepare the combobox */
    CHostDVDDriveVector coll =
        vboxGlobal().virtualBox().GetHost().GetDVDDrives();
    mHostCDs.resize (coll.size());
    mCbHostCD->clear();

    for (int id = 0; id < coll.size(); ++id)
    {
        CHostDVDDrive hostDVD = coll[id];
        /// @todo (r=dmik) set icon?
        QString name = hostDVD.GetName();
        QString description = hostDVD.GetDescription();
        QString fullName = description.isEmpty() ?
            name : QString ("%1 (%2)").arg (description, name);
        mCbHostCD->insertItem (id, fullName);
        mHostCDs [id] = hostDVD;
    }

    CDVDDrive dvd = mMachine.GetDVDDrive();
    switch (dvd.GetState())
    {
        case KDriveState_HostDriveCaptured:
        {
            CHostDVDDrive drv = dvd.GetHostDrive();
            QString name = drv.GetName();
            QString description = drv.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            if (vboxGlobal().virtualBox().GetHost().FindHostDVDDrive (name).isNull())
            {
                /* If the DVD drive is not currently available,
                 * add it to the end of the list with a special mark */
                mCbHostCD->insertItem (mCbHostCD->count(), "* " + fullName);
                mCbHostCD->setCurrentIndex (mCbHostCD->count() - 1);
            }
            else
            {
                /* This will select the correct item from the prepared list */
                mCbHostCD->setCurrentIndex (mCbHostCD->findText (fullName));
            }
            mRbHostCD->setChecked (true);
            mCbPassthrough->setChecked (dvd.GetPassthrough());
            mRbHostCD->setAutoExclusive (true);
            mRbIsoCD->setAutoExclusive (true);
            break;
        }
        case KDriveState_ImageMounted:
        {
            CDVDImage img = dvd.GetImage();
            QString src = img.GetLocation();
            AssertMsg (!src.isNull(), ("Image file must not be null"));
            QFileInfo fi (src);
            mRbIsoCD->setChecked (true);
            mUuidIsoCD = QString(img.GetId());
            mRbHostCD->setAutoExclusive (true);
            mRbIsoCD->setAutoExclusive (true);
            break;
        }
        case KDriveState_NotMounted:
        {
            mGbCD->setChecked (false);
            mRbHostCD->setAutoExclusive (false);
            mRbIsoCD->setAutoExclusive (false);
            break;
        }
        default:
            AssertMsgFailed (("invalid DVD state: %d\n", dvd.GetState()));
    }

    mCbIsoCD->setMachineId (mMachine.GetId());
    mCbIsoCD->setCurrentItem (mUuidIsoCD);
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mCbIsoCD->refresh();

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsCD::putBackTo()
{
    CDVDDrive dvd = mMachine.GetDVDDrive();
    if (!mGbCD->isChecked())
    {
        dvd.SetPassthrough (false);
        dvd.Unmount();
    }
    else if (mRbHostCD->isChecked())
    {
        dvd.SetPassthrough (mCbPassthrough->isChecked());
        int id = mCbHostCD->currentIndex();
        Assert (id >= 0);
        if (id < (int) mHostCDs.count())
            dvd.CaptureHostDrive (mHostCDs [id]);
    }
    else if (mRbIsoCD->isChecked())
    {
        dvd.SetPassthrough (false);
        Assert (!mUuidIsoCD.isNull());
        dvd.MountImage (mUuidIsoCD);
    }
}

void VBoxVMSettingsCD::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsCD::revalidate (QString &aWarning, QString &)
{
    if (mRbHostCD->isChecked() && mCbHostCD->currentText().isNull())
        aWarning = tr ("Host CD/DVD drive is not selected");
    else if (mRbIsoCD->isChecked() && mUuidIsoCD.isNull())
        aWarning = tr ("CD/DVD image file is not selected");

    return aWarning.isNull();
}

void VBoxVMSettingsCD::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbCD);
    setTabOrder (mGbCD, mRbHostCD);
    setTabOrder (mRbHostCD, mCbHostCD);
    setTabOrder (mCbHostCD, mCbPassthrough);
    setTabOrder (mCbPassthrough, mRbIsoCD);
    setTabOrder (mRbIsoCD, mCbIsoCD);
    setTabOrder (mCbIsoCD, mTbIsoCD);
}

void VBoxVMSettingsCD::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsCD::retranslateUi (this);
}

void VBoxVMSettingsCD::onGbChange (bool aSwitchedOn)
{
    /* Toggle auto-exclusiveness on/off to let the buttons be
     * unchecked both in case of group-box is not checked and
     * exclusively checked in case of group-box is checked. */
    mRbHostCD->setAutoExclusive (aSwitchedOn);
    mRbIsoCD->setAutoExclusive (aSwitchedOn);

    /* Toggle both buttons off when the group box unchecked. */
    if (!aSwitchedOn)
    {
        mLastSelected = mRbIsoCD->isChecked() ? mRbIsoCD : mRbHostCD;

        mRbHostCD->blockSignals (true);
        mRbIsoCD->blockSignals (true);
        mRbHostCD->setChecked (false);
        mRbIsoCD->setChecked (false);
        mRbHostCD->blockSignals (false);
        mRbIsoCD->blockSignals (false);
    }
    /* Toggle last checked button on when the group box checked. */
    else if (!mRbHostCD->isChecked() && !mRbIsoCD->isChecked())
    {
        mLastSelected->blockSignals (true);
        mLastSelected->setChecked (true);
        mLastSelected->blockSignals (false);
    }

    mMountCDChild->setEnabled (aSwitchedOn);

    onRbChange();
}

void VBoxVMSettingsCD::onRbChange()
{
    /* Check the 'host' group. */
    mCbHostCD->setEnabled (mRbHostCD->isChecked());
    mCbPassthrough->setEnabled (mRbHostCD->isChecked());

    /* Check the 'iso' group. */
    mCbIsoCD->setEnabled (mRbIsoCD->isChecked());
    mTbIsoCD->setEnabled (mRbIsoCD->isChecked());

    onCbChange();
}

void VBoxVMSettingsCD::onCbChange()
{
    mUuidIsoCD = mGbCD->isChecked() ? mCbIsoCD->id() : QString::null;
    emit cdChanged();
    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsCD::showMediaManager()
{
    QString oldId = mUuidIsoCD;
    VBoxMediaManagerDlg dlg (this);

    dlg.setup (VBoxDefs::MediaType_DVD, true /* aDoSelect */,
               false /* aRefresh */, mMachine, mCbIsoCD->id());

    QString newId = dlg.exec() == QDialog::Accepted ?
                  dlg.selectedId() : mCbIsoCD->id();
    if (oldId != newId)
    {
        mUuidIsoCD = newId;
        mCbIsoCD->setCurrentItem (mUuidIsoCD);
        emit cdChanged();
    }

    mCbIsoCD->setFocus();
    if (mValidator)
        mValidator->revalidate();
}

