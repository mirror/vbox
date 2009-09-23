/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMFirstRunWzd class implementation
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

#include "VBoxVMFirstRunWzd.h"
#include "VBoxGlobal.h"
#include "VBoxMediaManagerDlg.h"

VBoxVMFirstRunWzd::VBoxVMFirstRunWzd (const CMachine &aMachine, QWidget *aParent)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
    , mMachine (aMachine)
{
    /* Apply UI decorations */
    Ui::VBoxVMFirstRunWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Hide unnecessary text labels */
    CMediumAttachmentVector vec = mMachine.GetMediumAttachments();
    if (vec.size() != 0)
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
    mCbImage->setMachineId (mMachine.GetId());
    mTbVmm->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
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
    connect (mTbVmm, SIGNAL (clicked()), this, SLOT (openMediaManager()));
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
        QString summary = QString (
            "<table>"
            "<tr><td>%1:&nbsp;</td><td>%2</td></tr>"
            "<tr><td>%3:&nbsp;</td><td>%4</td></tr>"
            "</table>"
        )
            .arg (tr ("Type", "summary"), type)
            .arg (tr ("Source", "summary"), source);

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
            CMedium hostDrive = mHostDVDs [mCbHost->currentIndex()];
            if (!hostDrive.isNull())
                mMachine.MountMedium ("IDE Controller", 1, 0, hostDrive.GetId());
        }
        else if (mRbImage->isChecked())
            mMachine.MountMedium ("IDE Controller", 1, 0, mCbImage->id());
    }
    /* Floppy Media selected */
    else if (mRbFdType->isChecked())
    {
        if (mRbHost->isChecked())
        {
            CMedium hostDrive = mHostFloppys [mCbHost->currentIndex()];
            if (!hostDrive.isNull())
                mMachine.MountMedium ("IDE Controller", 1, 0, hostDrive.GetId());
        }
        else if (mRbImage->isChecked())
            mMachine.MountMedium ("IDE Controller", 1, 0, mCbImage->id());
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
        CMediumVector coll =
            vboxGlobal().virtualBox().GetHost().GetDVDDrives();
        mHostDVDs.resize (coll.size());

        for (int id = 0; id < coll.size(); ++id)
        {
            CMedium hostDVD = coll[id];
            QString name = hostDVD.GetName();
            QString description = hostDVD.GetDescription();
            QString fullName = description.isEmpty() ?
                name : QString ("%1 (%2)").arg (description, name);
            mCbHost->insertItem (id, fullName);
            mHostDVDs [id] = hostDVD;
        }

        /* Switch media images type to DVD */
        mCbImage->setType (VBoxDefs::MediumType_DVD);
    }
    /* Floppy media type selected */
    else if (sender() == mRbFdType)
    {
        /* Search for the host floppy-drives */
        CMediumVector coll =
            vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
        mHostFloppys.resize (coll.size());

        for (int id = 0; id < coll.size(); ++id)
        {
            CMedium hostFloppy = coll[id];
            QString name = hostFloppy.GetName();
            QString description = hostFloppy.GetDescription();
            QString fullName = description.isEmpty() ?
                name : QString ("%1 (%2)").arg (description, name);
            mCbHost->insertItem (id, fullName);
            mHostFloppys [id] = hostFloppy;
        }

        /* Switch media images type to FD */
        mCbImage->setType (VBoxDefs::MediumType_Floppy);
    }

    /* Repopulate the media list */
    mCbImage->repopulate();

    /* Revalidate updated page */
    mWvalType->revalidate();
}

void VBoxVMFirstRunWzd::mediaSourceChanged()
{
    mCbHost->setEnabled (sender() == mRbHost);
    mCbImage->setEnabled (sender() == mRbImage);
    mTbVmm->setEnabled (sender() == mRbImage);

    /* Revalidate updated page */
    mWvalType->revalidate();
}

void VBoxVMFirstRunWzd::openMediaManager()
{
    VBoxDefs::MediumType type =
        mRbCdType->isChecked() ? VBoxDefs::MediumType_DVD :
        mRbFdType->isChecked() ? VBoxDefs::MediumType_Floppy :
        VBoxDefs::MediumType_Invalid;

    AssertReturnVoid (type != VBoxDefs::MediumType_Invalid);

    VBoxMediaManagerDlg dlg (this);
    dlg.setup (type, true /* aDoSelect */);
    if (dlg.exec() == QDialog::Accepted)
    {
        mCbImage->setCurrentItem (dlg.selectedId());

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

