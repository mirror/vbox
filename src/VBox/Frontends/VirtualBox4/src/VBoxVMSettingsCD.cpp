/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsCD class implementation
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

#include "VBoxVMSettingsCD.h"
#include "VBoxVMSettingsDlg.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxDiskImageManagerDlg.h"

/* Qt includes */
#include <QFileInfo>

VBoxVMSettingsCD* VBoxVMSettingsCD::mSettings = 0;

VBoxVMSettingsCD::VBoxVMSettingsCD (QWidget *aParent,
                                    VBoxVMSettingsDlg *aDlg,
                                    const QString &aPath)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mLastSelected (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsCD::setupUi (this);

    /* Setup validation */
    mValidator = new QIWidgetValidator (aPath, aParent, this);
    connect (mValidator, SIGNAL (validityChanged (const QIWidgetValidator*)),
             aDlg, SLOT (enableOk (const QIWidgetValidator*)));
    connect (mValidator, SIGNAL (isValidRequested (QIWidgetValidator*)),
             aDlg, SLOT (revalidate (QIWidgetValidator*)));

    /* Setup connections */
    connect (mGbCD, SIGNAL (toggled (bool)), this, SLOT (onMediaChanged()));
    connect (mCbIsoCD, SIGNAL (activated (int)), this, SLOT (onMediaChanged()));
    connect (mTbIsoCD, SIGNAL (clicked()), this, SLOT (showImageManager()));
    connect (mRbHostCD, SIGNAL (toggled (bool)), mValidator, SLOT (revalidate()));
    connect (mRbIsoCD, SIGNAL (toggled (bool)), mValidator, SLOT (revalidate()));

    /* Setup iconsets */
    mTbIsoCD->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                            ":/select_file_dis_16px.png"));

    /* Setup dialog */
    mCbIsoCD->setType (VBoxDefs::CD);

    mLastSelected = mRbHostCD;
    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsCD::getFromMachine (const CMachine &aMachine,
                                       QWidget *aPage,
                                       VBoxVMSettingsDlg *aDlg,
                                       const QString &aPath)
{
    mSettings = new VBoxVMSettingsCD (aPage, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    connect (mSettings, SIGNAL (cdChanged()), aDlg, SLOT (resetFirstRunFlag()));
    mSettings->getFrom (aMachine);
}

void VBoxVMSettingsCD::putBackToMachine()
{
    mSettings->putBackTo();
}

bool VBoxVMSettingsCD::revalidate (QString &aWarning)
{
    return mSettings->validate (aWarning);
}

void VBoxVMSettingsCD::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Read out the host DVD drive list and prepare the combobox */
    CHostDVDDriveCollection coll =
        vboxGlobal().virtualBox().GetHost().GetDVDDrives();
    mHostCDs.resize (coll.GetCount());
    mCbHostCD->clear();
    int id = 0;
    CHostDVDDriveEnumerator en = coll.Enumerate();
    while (en.HasMore())
    {
        CHostDVDDrive hostDVD = en.GetNext();
        /// @todo (r=dmik) set icon?
        QString name = hostDVD.GetName();
        QString description = hostDVD.GetDescription();
        QString fullName = description.isEmpty() ?
            name : QString ("%1 (%2)").arg (description, name);
        mCbHostCD->insertItem (id, fullName);
        mHostCDs [id] = hostDVD;
        ++ id;
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
            if (coll.FindByName (name).isNull())
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
            break;
        }
        case KDriveState_ImageMounted:
        {
            CDVDImage img = dvd.GetImage();
            QString src = img.GetFilePath();
            AssertMsg (!src.isNull(), ("Image file must not be null"));
            QFileInfo fi (src);
            mRbIsoCD->setChecked (true);
            mUuidIsoCD = QUuid (img.GetId());
            break;
        }
        case KDriveState_NotMounted:
        {
            mGbCD->setChecked (false);
            break;
        }
        default:
            AssertMsgFailed (("invalid DVD state: %d\n", dvd.GetState()));
    }

    mCbIsoCD->setCurrentItem (mUuidIsoCD);
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mCbIsoCD->refresh();

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

bool VBoxVMSettingsCD::validate (QString &aWarning)
{
    /* Toggle auto-exclusiveness on/off to let the buttons be
     * unchecked both in case of group-box is not checked and
     * exclusively checked in case of group-box is checked. */
    mRbHostCD->setAutoExclusive (mGbCD->isChecked());
    mRbIsoCD->setAutoExclusive (mGbCD->isChecked());

    /* Toggle both buttons off when the group box unchecked. */
    if (!mGbCD->isChecked())
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

    /* Check the 'host' group. */
    mCbHostCD->setEnabled (mRbHostCD->isChecked());
    mCbPassthrough->setEnabled (mRbHostCD->isChecked());

    /* Check the 'iso' group. */
    mCbIsoCD->setEnabled (mRbIsoCD->isChecked());
    mTbIsoCD->setEnabled (mRbIsoCD->isChecked());

    /* Compose the error string. */
    if (mRbHostCD->isChecked() && mCbHostCD->currentText().isNull())
        aWarning = tr ("Host CD/DVD drive is not selected");
    else if (mRbIsoCD->isChecked() && mUuidIsoCD.isNull())
        aWarning = tr ("CD/DVD image file is not selected");

    return aWarning.isNull();
}


void VBoxVMSettingsCD::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsCD::retranslateUi (this);
}


void VBoxVMSettingsCD::onMediaChanged()
{
    mUuidIsoCD = mGbCD->isChecked() ? mCbIsoCD->getId() : QUuid();
    mValidator->revalidate();
    emit cdChanged();
}

void VBoxVMSettingsCD::showImageManager()
{
    QUuid oldId = mUuidIsoCD;
    VBoxDiskImageManagerDlg dlg (this);
    QUuid machineId = mMachine.GetId();
    dlg.setup (VBoxDefs::CD, true, &machineId, true /* aRefresh */,
               mMachine, QUuid(), mCbIsoCD->getId(), QUuid());

    QUuid newId = dlg.exec() == QDialog::Accepted ?
                  dlg.selectedUuid() : mCbIsoCD->getId();
    if (oldId != newId)
    {
        mUuidIsoCD = newId;
        mCbIsoCD->setCurrentItem (mUuidIsoCD);
        emit cdChanged();
    }

    mCbIsoCD->setFocus();
    mValidator->revalidate();
}

