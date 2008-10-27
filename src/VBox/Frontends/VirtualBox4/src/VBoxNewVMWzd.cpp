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
    connect (mCbOS, SIGNAL (activated (int)), this, SLOT (cbOSActivated (int)));

    /* Memory page */
    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();
    mLeRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));
    mWvalMemory = new QIWidgetValidator (mPageMemory, this);
    connect (mWvalMemory, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
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
    connect (mPbNewHD, SIGNAL (clicked()), this, SLOT (showNewHardDiskWizard()));
    connect (mPbExistingHD, SIGNAL (clicked()), this, SLOT (showHardDiskManager()));

    /* Summary page */


    /* Name and OS page */
    mCbOS->addItems (vboxGlobal().vmGuestOSTypeDescriptions());
    cbOSActivated (mCbOS->currentIndex());

    /* Memory page */
    mSlRAM->setPageStep (calcPageStep (MaxRAM));
    mSlRAM->setSingleStep (mSlRAM->pageStep() / 4);
    mSlRAM->setTickInterval (mSlRAM->pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    mSlRAM->setRange ((MinRAM / mSlRAM->pageStep()) * mSlRAM->pageStep(),
                      MaxRAM);
    /* Initial RAM value is set in cbOSActivated()
     * limit min/max. size of QLineEdit */
    mLeRAM->setFixedWidthByText ("99999");
    /* Ensure mLeRAM value and validation is updated */
    slRAMValueChanged (mSlRAM->value());

    /* HDD Images page */

    /* Summary page */
    /* Update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
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

    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());

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
            .arg (tr ("OS Type", "summary"),
                  vboxGlobal().vmGuestOSType (mCbOS->currentIndex()).GetDescription())
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

void VBoxNewVMWzd::showHardDiskManager()
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

void VBoxNewVMWzd::showNewHardDiskWizard()
{
    VBoxNewHDWzd dlg (this);

    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());
    dlg.setRecommendedFileName (mLeName->text());
    dlg.setRecommendedSize (type.GetRecommendedHDD());

    if (dlg.exec() == QDialog::Accepted)
    {
        ensureNewHardDiskDeleted();
        mHardDisk = dlg.hardDisk();
        mHDCombo->setCurrentItem (mHardDisk.GetId());
    }

    mHDCombo->setFocus();
}

void VBoxNewVMWzd::slRAMValueChanged (int aValue)
{
    mLeRAM->setText (QString().setNum (aValue));
}

void VBoxNewVMWzd::leRAMTextChanged (const QString &aText)
{
    mSlRAM->setValue (aText.toInt());
}

void VBoxNewVMWzd::cbOSActivated (int aItem)
{
    CGuestOSType type = vboxGlobal().vmGuestOSType (aItem);
    mPmOS->setPixmap (vboxGlobal().vmGuestOSTypeIcon (type.GetId()));
    mSlRAM->setValue (type.GetRecommendedRAM());
}

void VBoxNewVMWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Get common result of validators */
    bool valid = aWval->isOtherValid();

    /* Do individual validations for pages */
    if (aWval->widget() == mPageHDD)
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

    /* Create a machine with the default settings file location */
    if (mMachine.isNull())
    {
        mMachine = vbox.CreateMachine (QString(), mLeName->text(), QUuid());
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

    /* OS type */
    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    QString typeId = type.GetId();
    mMachine.SetOSTypeId (typeId);

    if (typeId == "os2warp3"  ||
        typeId == "os2warp4"  ||
        typeId == "os2warp45" ||
        typeId == "ecs")
        mMachine.SetHWVirtExEnabled (KTSBool_True);

    /* RAM size */
    mMachine.SetMemorySize (mSlRAM->value());

    /* Add one network adapter (NAT) by default */
    {
        CNetworkAdapter cadapter = mMachine.GetNetworkAdapter (0);
#ifdef VBOX_WITH_E1000
        /* Default to e1k on solaris */
        if (typeId == "solaris" ||
            typeId == "opensolaris")
            cadapter.SetAdapterType (KNetworkAdapterType_I82540EM);
#endif /* VBOX_WITH_E1000 */
        cadapter.SetEnabled (true);
        cadapter.AttachToNAT();
        cadapter.SetMACAddress (QString::null);
        cadapter.SetCableConnected (true);
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
            m.AttachHardDisk2 (mHDCombo->id(), KStorageBus_IDE, 0, 0);
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

