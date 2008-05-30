/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMFirstRunWzd class implementation
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

#include "VBoxVMFirstRunWzd.h"
#include "VBoxGlobal.h"
#include "VBoxDiskImageManagerDlg.h"

VBoxVMFirstRunWzd::VBoxVMFirstRunWzd (const CMachine &aMachine, QWidget *aParent)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
    , mMachine (aMachine)
{
    /* Apply UI decorations */
    Ui::VBoxVMFirstRunWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Hide unnecessary text labels */
    CHardDiskAttachmentEnumerator en = mMachine.GetHardDiskAttachments().Enumerate();
    if (en.HasMore())
    {
        mTextWelcome2->setHidden (true);
        mTextType2->setHidden (true);
        mTextSource2->setHidden (true);
        mTextSummaryHdr2->setHidden (true);
        mTextSummaryFtr2->setHidden (true);
    }
    else
    {
        mTextWelcome1->setHidden (true);
        mTextType1->setHidden (true);
        mTextSource1->setHidden (true);
        mTextSummaryHdr1->setHidden (true);
        mTextSummaryFtr1->setHidden (true);
    }

    /* Media page */
    mTbVdm->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                          ":/select_file_dis_16px.png"));
    mWvalType = new QIWidgetValidator (mPageMedia, this);
    connect (mWvalType, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalType, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mRbCdType, SIGNAL (clicked()), this, SLOT (mediaTypeChanged()));
    connect (mRbFdType, SIGNAL (clicked()), this, SLOT (mediaTypeChanged()));
    connect (mRbHost, SIGNAL (clicked()), this, SLOT (mediaSourceChanged()));
    connect (mRbImage, SIGNAL (clicked()), this, SLOT (mediaSourceChanged()));
    connect (mTbVdm, SIGNAL (clicked()), this, SLOT (openVdm()));
    mRbCdType->animateClick();
    mRbHost->animateClick();

    /* Summary page */
    /* Update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    mWvalType->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

void VBoxVMFirstRunWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMFirstRunWzd::retranslateUi (this);

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageSummary)
    {
        /* compose summary */
        QString type =
            mRbCdType->isChecked() ? tr ("CD/DVD-ROM Device") :
            mRbFdType->isChecked() ? tr ("Floppy Device") :
            QString::null;
        QString source =
            mRbHost->isChecked() ? tr ("Host Drive %1").arg (mCbHost->currentText()) :
            mRbImage->isChecked() ? mCbImage->currentText() : QString::null;
        QString summary = QString (tr (
            "<table> cellspacing=0 cellpadding=2"
            "<tr><td>Type:</td><td>%1</td></tr>"
            "<tr><td>Source:</td><td>%2</td></tr>"
            "</table>"
        ))
            .arg (type)
            .arg (source);

        mTeSummary->setText (summary);
    }
}

void VBoxVMFirstRunWzd::accept()
{
    /* CD/DVD Media selected */
    if (mRbCdType->isChecked())
    {
        if (mRbHost->isChecked())
        {
            CHostDVDDrive hostDrive = mHostDVDs [mCbHost->currentIndex()];
            if (!hostDrive.isNull())
            {
                CDVDDrive virtualDrive = mMachine.GetDVDDrive();
                virtualDrive.CaptureHostDrive (hostDrive);
            }
        }
        else if (mRbImage->isChecked())
        {
            CDVDDrive virtualDrive = mMachine.GetDVDDrive();
            virtualDrive.MountImage (mCbImage->getId());
        }
    }
    /* Floppy Media selected */
    else if (mRbFdType->isChecked())
    {
        if (mRbHost->isChecked())
        {
            CHostFloppyDrive hostDrive = mHostFloppys [mCbHost->currentIndex()];
            if (!hostDrive.isNull())
            {
                CFloppyDrive virtualDrive = mMachine.GetFloppyDrive();
                virtualDrive.CaptureHostDrive (hostDrive);
            }
        }
        else if (mRbImage->isChecked())
        {
            CFloppyDrive virtualDrive = mMachine.GetFloppyDrive();
            virtualDrive.MountImage (mCbImage->getId());
        }
    }

    QIAbstractWizard::accept();
}

void VBoxVMFirstRunWzd::revalidate (QIWidgetValidator *aWval)
{
    /* do individual validations for pages */
    QWidget *pg = aWval->widget();
    bool valid = aWval->isOtherValid();

    if (pg == mPageMedia)
    {
        valid = (mRbHost->isChecked() && !mCbHost->currentText().isEmpty()) ||
                (mRbImage->isChecked() && !mCbImage->currentText().isEmpty());
    }

    aWval->setOtherValid (valid);
}

void VBoxVMFirstRunWzd::mediaTypeChanged()
{
    /* CD/DVD Media type selected */
    mCbHost->clear();
    if (sender() == mRbCdType)
    {
        /* Search for the host dvd-drives */
        CHostDVDDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetDVDDrives();
        mHostDVDs.resize (coll.GetCount());
        int id = 0;
        CHostDVDDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostDVDDrive hostDVD = en.GetNext();
            QString name = hostDVD.GetName();
            QString description = hostDVD.GetDescription();
            QString fullName = description.isEmpty() ?
                name : QString ("%1 (%2)").arg (description, name);
            mCbHost->insertItem (id, fullName);
            mHostDVDs [id] = hostDVD;
            ++ id;
        }

        /* Switch media images type to CD */
        mCbImage->setType (VBoxDefs::CD);
    }
    /* Floppy media type selected */
    else if (sender() == mRbFdType)
    {
        /* Search for the host floppy-drives */
        CHostFloppyDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
        mHostFloppys.resize (coll.GetCount());
        int id = 0;
        CHostFloppyDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostFloppyDrive hostFloppy = en.GetNext();
            QString name = hostFloppy.GetName();
            QString description = hostFloppy.GetDescription();
            QString fullName = description.isEmpty() ?
                name : QString ("%1 (%2)").arg (description, name);
            mCbHost->insertItem (id, fullName);
            mHostFloppys [id] = hostFloppy;
            ++ id;
        }

        /* Switch media images type to FD */
        mCbImage->setType (VBoxDefs::FD);
    }
    /* Update media images list */
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mCbImage->refresh();

    /* Revalidate updated page */
    mWvalType->revalidate();
}

void VBoxVMFirstRunWzd::mediaSourceChanged()
{
    mCbHost->setEnabled (sender() == mRbHost);
    mCbImage->setEnabled (sender() == mRbImage);
    mTbVdm->setEnabled (sender() == mRbImage);

    /* Revalidate updated page */
    mWvalType->revalidate();
}

void VBoxVMFirstRunWzd::openVdm()
{
    VBoxDiskImageManagerDlg vdm (this);
    QUuid machineId = mMachine.GetId();
    VBoxDefs::DiskType type = mRbCdType->isChecked() ? VBoxDefs::CD :
        mRbFdType->isChecked() ? VBoxDefs::FD : VBoxDefs::InvalidType;
    vdm.setup (type, true, &machineId);
    if (vdm.exec() == QDialog::Accepted)
    {
        mCbImage->setCurrentItem (vdm.selectedUuid());

        /* Revalidate updated page */
        mWvalType->revalidate();
    }
}

void VBoxVMFirstRunWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxVMFirstRunWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageWelcome)
        nextButton (page)->setFocus();
    else if (page == mPageMedia)
        mRbCdType->isChecked() ? mRbCdType->setFocus() :
                                 mRbFdType->setFocus();
    else if (page == mPageSummary)
        mTeSummary->setFocus();

    if (page == mPageSummary)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

