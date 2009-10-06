/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMFirstRunWzd class implementation
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

/* Local includes */
#include "VBoxVMFirstRunWzd.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxMediaManagerDlg.h"
#include "QIWidgetValidator.h"

VBoxVMFirstRunWzd::VBoxVMFirstRunWzd (const CMachine &aMachine, QWidget *aParent)
    : QIWithRetranslateUI <QIAbstractWizard> (aParent)
    , mValidator (0)
    , mMachine (aMachine)
{
    /* Apply UI decorations */
    Ui::VBoxVMFirstRunWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Hide unnecessary text labels */
    CMediumAttachment hda = mMachine.GetMediumAttachment ("IDE Controller", 0, 0);
    mTextWelcome1->setHidden (hda.isNull());
    mTextType1->setHidden (hda.isNull());
    mTextSource1->setHidden (hda.isNull());
    mTextSummaryHdr1->setHidden (hda.isNull());
    mTextSummaryFtr1->setHidden (hda.isNull());
    mTextWelcome2->setHidden (!hda.isNull());
    mTextType2->setHidden (!hda.isNull());
    mTextSource2->setHidden (!hda.isNull());
    mTextSummaryHdr2->setHidden (!hda.isNull());
    mTextSummaryFtr2->setHidden (!hda.isNull());

    /* Media page */
    mCbMedia->setMachineId (mMachine.GetId());
    mTbVmm->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png", ":/select_file_dis_16px.png"));
    mValidator = new QIWidgetValidator (mPageMedia, this);
    connect (mValidator, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mValidator, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mRbCdType, SIGNAL (clicked()), this, SLOT (mediaTypeChanged()));
    connect (mRbFdType, SIGNAL (clicked()), this, SLOT (mediaTypeChanged()));
    connect (mTbVmm, SIGNAL (clicked()), this, SLOT (openMediaManager()));
    mRbCdType->animateClick();

    /* Revalidate updated page */
    mValidator->revalidate();

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
        /* Compose Summary */
        QString type = mRbCdType->isChecked() ? tr ("CD/DVD-ROM Device") : tr ("Floppy Device");
        QString source = mCbMedia->currentText();
        QString summary = QString ("<table>"
                                   "<tr><td>%1:&nbsp;</td><td>%2</td></tr>"
                                   "<tr><td>%3:&nbsp;</td><td>%4</td></tr>"
                                   "</table>")
                                   .arg (tr ("Type", "summary"), type)
                                   .arg (tr ("Source", "summary"), source);
        mTeSummary->setText (summary);
    }
}

void VBoxVMFirstRunWzd::accept()
{
    /* Composing default controller name */
    QString ctrName (mRbCdType->isChecked() ? "IDE Controller" : "Floppy Controller");
    LONG ctrPort = mRbCdType->isChecked() ? 1 : 0;
    LONG ctrDevice = 0;
    /* Default should present */
    CStorageController ctr = mMachine.GetStorageControllerByName (ctrName);
    Assert (!ctr.isNull());
    /* Mount medium to the predefined port/device */
    mMachine.MountMedium (ctrName, ctrPort, ctrDevice, mCbMedia->id());
    if (mMachine.isOk())
        QIAbstractWizard::accept();
    else
        vboxProblem().cannotMountMedium (this, mMachine, vboxGlobal().findMedium (mCbMedia->id()));
}

void VBoxVMFirstRunWzd::revalidate (QIWidgetValidator *aValidator)
{
    /* Do individual validations for pages */
    QWidget *pg = aValidator->widget();
    bool valid = aValidator->isOtherValid();

    /* Allow to go to Summary only if non-null medium selected */
    if (pg == mPageMedia)
        valid = !vboxGlobal().findMedium (mCbMedia->id()).isNull();

    aValidator->setOtherValid (valid);
}

void VBoxVMFirstRunWzd::mediaTypeChanged()
{
    /* CD/DVD media type selected */
    if (sender() == mRbCdType)
    {
        /* Switch media combo-box type to CD/DVD */
        mCbMedia->setType (VBoxDefs::MediumType_DVD);
    }
    /* Floppy media type selected */
    else if (sender() == mRbFdType)
    {
        /* Switch media combo-box type to Floppy */
        mCbMedia->setType (VBoxDefs::MediumType_Floppy);
    }

    /* Update the media combo-box */
    mCbMedia->repopulate();

    /* Revalidate updated page */
    mValidator->revalidate();
}

void VBoxVMFirstRunWzd::openMediaManager()
{
    /* Create & open VMM */
    VBoxMediaManagerDlg dlg (this);
    dlg.setup (mCbMedia->type(), true /* aDoSelect */);
    if (dlg.exec() == QDialog::Accepted)
        mCbMedia->setCurrentItem (dlg.selectedId());

    /* Revalidate updated page */
    mValidator->revalidate();
}

void VBoxVMFirstRunWzd::enableNext (const QIWidgetValidator *aValidator)
{
    nextButton (aValidator->widget())->setEnabled (aValidator->isValid());
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

