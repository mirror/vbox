/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxNewVMWzd class implementation
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

#include "VBoxNewVMWzd.h"
#include "VBoxUtils.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxNewHDWzd.h"
#include "VBoxMediaManagerDlg.h"

/**
 * Calculates a suitable page step size for the given max value.
 * The returned size is so that there will be no more than 32 pages.
 * The minimum returned page size is 4.
 */
static int calcPageStep (int aMax)
{
    /* Reasonable max. number of page steps is 32 */
    uint page = ((uint) aMax + 31) / 32;
    /* Make it a power of 2 */
    uint p = page, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (page != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int) p2;
}


VBoxNewVMWzd::VBoxNewVMWzd (QWidget *aParent)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxNewVMWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Name and OS page */
    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    mWvalNameAndOS = new QIWidgetValidator (mPageNameAndOS, this);
    connect (mWvalNameAndOS, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mOSTypeSelector, SIGNAL (osTypeChanged()), this, SLOT (onOSTypeChanged()));

    /* Memory page */
    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();
    mSlRAM->setPageStep (calcPageStep (MaxRAM));
    mSlRAM->setSingleStep (mSlRAM->pageStep() / 4);
    mSlRAM->setTickInterval (mSlRAM->pageStep());
    mSlRAM->setRange ((MinRAM / mSlRAM->pageStep()) * mSlRAM->pageStep(), MaxRAM);
    mLeRAM->setFixedWidthByText ("99999");
    mLeRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));

    mWvalMemory = new QIWidgetValidator (mPageMemory, this);
    connect (mWvalMemory, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mWvalMemory, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));
    connect (mSlRAM, SIGNAL (valueChanged (int)),
             this, SLOT (slRAMValueChanged (int)));
    connect (mLeRAM, SIGNAL (textChanged (const QString&)),
             this, SLOT (leRAMTextChanged (const QString&)));

    /* HDD Images page */
    mHDCombo->setType (VBoxDefs::MediaType_HardDisk);
    mHDCombo->repopulate();

    mWvalHDD = new QIWidgetValidator (mPageHDD, this);
    connect (mWvalHDD, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mWvalHDD, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));
    connect (mGbHDA, SIGNAL (toggled (bool)), mWvalHDD, SLOT (revalidate()));
    connect (mHDCombo, SIGNAL (currentIndexChanged (int)),
             mWvalHDD, SLOT (revalidate()));
    connect (mPbNewHD, SIGNAL (clicked()), this, SLOT (showNewHDWizard()));
    connect (mPbExistingHD, SIGNAL (clicked()), this, SLOT (showMediaManager()));

    /* Name and OS page */
    onOSTypeChanged();

    /* Memory page */
    slRAMValueChanged (mSlRAM->value());

    /* Initial revalidation */
    mWvalNameAndOS->revalidate();
    mWvalMemory->revalidate();
    mWvalHDD->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

VBoxNewVMWzd::~VBoxNewVMWzd()
{
    ensureNewHardDiskDeleted();
}

const CMachine& VBoxNewVMWzd::machine() const
{
    return mMachine;
}

void VBoxNewVMWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxNewVMWzd::retranslateUi (this);

    CGuestOSType type = mOSTypeSelector->type();

    mTextRAMBest->setText (
        tr ("The recommended base memory size is <b>%1</b> MB.")
            .arg (type.GetRecommendedRAM()));
    mTextVDIBest->setText (
        tr ("The recommended size of the boot hard disk is <b>%1</b> MB.")
            .arg (type.GetRecommendedHDD()));

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();

    mTxRAMMin->setText (QString ("<qt>%1&nbsp;%2</qt>")
                        .arg (MinRAM).arg (tr ("MB", "megabytes")));
    mTxRAMMax->setText (QString ("<qt>%1&nbsp;%2</qt>")
                        .arg (MaxRAM).arg (tr ("MB", "megabytes")));

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageSummary)
    {
        /* Compose summary */
        QString summary = QString (
            "<tr><td><nobr>%1:&nbsp;</nobr></td><td>%2</td></tr>"
            "<tr><td><nobr>%3:&nbsp;</nobr></td><td>%4</td></tr>"
            "<tr><td><nobr>%5:&nbsp;</nobr></td><td>%6&nbsp;%7</td></tr>")
            .arg (tr ("Name", "summary"), mLeName->text())
            .arg (tr ("OS Type", "summary"), type.GetDescription())
            .arg (tr ("Base Memory", "summary")).arg (mSlRAM->value())
            .arg (tr ("MB", "megabytes"));

        if (mGbHDA->isChecked() && !mHDCombo->id().isNull())
            summary += QString (
                "<tr><td><nobr>%8:&nbsp;</nobr></td><td><nobr>%9</nobr></td></tr>")
                .arg (tr ("Boot Hard Disk", "summary"), mHDCombo->currentText());

        mTeSummary->setText ("<table>" + summary + "</table>");
    }
}

void VBoxNewVMWzd::accept()
{
    /* Try to create the machine when the Finish button is pressed.
     * On failure, the wisard will remain open to give it another try. */
    if (constructMachine())
        QDialog::accept();
}

void VBoxNewVMWzd::showMediaManager()
{
    VBoxMediaManagerDlg dlg (this);
    dlg.setup (VBoxDefs::MediaType_HardDisk, true);

    if (dlg.exec() == QDialog::Accepted)
    {
        QUuid newId = dlg.selectedId();
        if (mHDCombo->id() != newId)
        {
            ensureNewHardDiskDeleted();
            mHDCombo->setCurrentItem (newId);
        }
    }

    mHDCombo->setFocus();
}

void VBoxNewVMWzd::showNewHDWizard()
{
    VBoxNewHDWzd dlg (this);

    dlg.setRecommendedFileName (mLeName->text());
    dlg.setRecommendedSize (mOSTypeSelector->type().GetRecommendedHDD());

    if (dlg.exec() == QDialog::Accepted)
    {
        ensureNewHardDiskDeleted();
        mHardDisk = dlg.hardDisk();
        mHDCombo->setCurrentItem (mHardDisk.GetId());
    }

    mHDCombo->setFocus();
}

void VBoxNewVMWzd::onOSTypeChanged()
{
    slRAMValueChanged (mOSTypeSelector->type().GetRecommendedRAM());
}

void VBoxNewVMWzd::slRAMValueChanged (int aValue)
{
    mLeRAM->setText (QString().setNum (aValue));
}

void VBoxNewVMWzd::leRAMTextChanged (const QString &aText)
{
    mSlRAM->setValue (aText.toInt());
}

void VBoxNewVMWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Get common result of validators */
    bool valid = aWval->isOtherValid();

    /* Do individual validations for pages */
    ulong memorySize = vboxGlobal().virtualBox().GetHost().GetMemorySize();
    ulong summarySize = (ulong)(mSlRAM->value()) +
                        (ulong)(VBoxGlobal::requiredVideoMemory() / _1M);
    if (aWval->widget() == mPageMemory)
    {
        valid = true;
        if (summarySize > 0.75 * memorySize)
            valid = false;
    }
    else if (aWval->widget() == mPageHDD)
    {
        valid = true;
        if (mGbHDA->isChecked() && mHDCombo->id().isNull())
            valid = false;
    }

    aWval->setOtherValid (valid);
}

void VBoxNewVMWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxNewVMWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageWelcome)
        nextButton (page)->setFocus();
    else if (page == mPageNameAndOS)
        mLeName->setFocus();
    else if (page == mPageMemory)
        mSlRAM->setFocus();
    else if (page == mPageHDD)
        mHDCombo->setFocus();
    else if (page == mPageSummary)
        mTeSummary->setFocus();

    if (page == mPageSummary)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxNewVMWzd::showNextPage()
{
    /* Ask user about disk-less machine */
    if (sender() == mBtnNext4 && !mGbHDA->isChecked() &&
        !vboxProblem().confirmHardDisklessMachine (this))
        return;

    /* Switch to the next page */
    QIAbstractWizard::showNextPage();
}


bool VBoxNewVMWzd::constructMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* OS type */
    CGuestOSType type = mOSTypeSelector->type();
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    QString typeId = type.GetId();

    /* Create a machine with the default settings file location */
    if (mMachine.isNull())
    {
        mMachine = vbox.CreateMachine (mLeName->text(), typeId, QString::null, QUuid());
        if (!vbox.isOk())
        {
            vboxProblem().cannotCreateMachine (vbox, this);
            return false;
        }

        /* The FirstRun wizard is to be shown only when we don't attach any hard
         * disk or attach a new (empty) one. Selecting an existing hard disk
         * will cancel the wizard. */
        if (!mGbHDA->isChecked() || !mHardDisk.isNull())
            mMachine.SetExtraData (VBoxDefs::GUI_FirstRun, "yes");
    }

    /* RAM size */
    mMachine.SetMemorySize (mSlRAM->value());

    /* VRAM size - select maximum between recommended and minimum for fullscreen */
    mMachine.SetVRAMSize (qMax (type.GetRecommendedVRAM(),
                                (ULONG) (VBoxGlobal::requiredVideoMemory() / _1M)));

    /* Enabling audio by default */
    mMachine.GetAudioAdapter().SetEnabled (true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2) */
    CUSBController usbController = mMachine.GetUSBController();
    if (!usbController.isNull())
    {
        usbController.SetEnabled (true);
        usbController.SetEnabledEhci (true);
    }

    /* Register the VM prior to attaching hard disks */
    vbox.RegisterMachine (mMachine);
    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateMachine (vbox, mMachine, this);
        return false;
    }

    /* Boot hard disk (IDE Primary Master) */
    if (mGbHDA->isChecked())
    {
        bool success = false;
        QUuid machineId = mMachine.GetId();
        CSession session = vboxGlobal().openSession (machineId);
        if (!session.isNull())
        {
            CMachine m = session.GetMachine();
            m.AttachHardDisk(mHDCombo->id(), QString("IDE"), 0, 0);
            if (m.isOk())
            {
                m.SaveSettings();
                if (m.isOk())
                    success = true;
                else
                    vboxProblem().cannotSaveMachineSettings (m, this);
            }
            else
                vboxProblem().cannotAttachHardDisk (this, m,
                                                    mHDCombo->location(),
                                                    KStorageBus_IDE, 0, 0);
            session.Close();
        }
        if (!success)
        {
            /* Unregister on failure */
            vbox.UnregisterMachine (machineId);
            if (vbox.isOk())
                mMachine.DeleteSettings();
            return false;
        }
    }

    /* Ensure we don't try to delete a newly created hard disk on success */
    mHardDisk.detach();

    return true;
}

void VBoxNewVMWzd::ensureNewHardDiskDeleted()
{
    if (!mHardDisk.isNull())
    {
        /* Remember ID as it may be lost after the deletion */
        QUuid id = mHardDisk.GetId();

        bool success = false;

        CProgress progress = mHardDisk.DeleteStorage();
        if (mHardDisk.isOk())
        {
            vboxProblem().showModalProgressDialog (progress, windowTitle(),
                                                   parentWidget());
            if (progress.isOk() && progress.GetResultCode() == S_OK)
                success = true;
        }

        if (success)
            vboxGlobal().removeMedium (VBoxDefs::MediaType_HardDisk, id);
        else
            vboxProblem().cannotDeleteHardDiskStorage (this, mHardDisk,
                                                       progress);
        mHardDisk.detach();
    }
}

