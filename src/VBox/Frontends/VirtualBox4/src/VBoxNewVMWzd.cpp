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
#include "VBoxDiskImageManagerDlg.h"

/**
 *  Calculates a suitable page step size for the given max value.
 *  The returned size is so that there will be no more than 32 pages.
 *  The minimum returned page size is 4.
 */
static int calcPageStep (int aMax)
{
    /* reasonable max. number of page steps is 32 */
    uint page = ((uint) aMax + 31) / 32;
    /* make it a power of 2 */
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
    connect (mWvalNameAndOS, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mCbOS, SIGNAL (activated (int)), this, SLOT (cbOSActivated (int)));

    /* Memory page */
    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();
    mLeRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));
    mWvalMemory = new QIWidgetValidator (mPageMemory, this);
    connect (mWvalMemory, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mSlRAM, SIGNAL (valueChanged (int)),
             this, SLOT (slRAMValueChanged (int)));
    connect (mLeRAM, SIGNAL (textChanged (const QString&)),
             this, SLOT (leRAMTextChanged (const QString&)));

    /* HDD Images page */
    mMediaCombo->setType (VBoxDefs::HD);
    mMediaCombo->setUseEmptyItem (true);
    mMediaCombo->setFocusPolicy (Qt::StrongFocus);
    connect (mMediaCombo, SIGNAL (activated (int)),
             this, SLOT (currentMediaChanged (int)));
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mMediaCombo->refresh();
    mWvalHDD = new QIWidgetValidator (mPageHDD, this);
    connect (mWvalHDD, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalHDD, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mPbNewHD, SIGNAL (clicked()), this, SLOT (showNewVDIWizard()));
    connect (mPbExistingHD, SIGNAL (clicked()), this, SLOT (showVDIManager()));

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
    return cmachine;
}

void VBoxNewVMWzd::retranslateUi()
{
   /* Translate uic generated strings */
    Ui::VBoxNewVMWzd::retranslateUi (this);

    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());

    mTextRAMBest->setText (
        tr ("The recommended base memory size is <b>%1</b> MB.")
            .arg (type.GetRecommendedRAM()));
    mSlRAM->setValue (type.GetRecommendedRAM());
    mTextVDIBest->setText (
        tr ("The recommended size of the boot hard disk is <b>%1</b> MB.")
            .arg (type.GetRecommendedHDD()));

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();

    mTxRAMMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MinRAM));
    mTxRAMMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MaxRAM));

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageSummary)
    {
        /* compose summary */
        QString summary = QString (tr (
            "<tr><td><nobr>Name:</nobr></td><td>%1</td></tr>"
            "<tr><td><nobr>OS Type:</nobr></td><td>%2<</td></tr>"
            "<tr><td><nobr>Base Memory:</nobr></td><td>%3&nbsp;MB</td></tr>"))
            .arg (mLeName->text())
            .arg (vboxGlobal().vmGuestOSType (mCbOS->currentIndex()).GetDescription())
            .arg (mSlRAM->value());

        if (mMediaCombo->currentIndex())
            summary += QString (tr (
                "<tr><td><nobr>Boot Hard Disk:</nobr></td><td>%4</td></tr>"))
                .arg (mMediaCombo->currentText());

        mTeSummary->setText ("<table cellspacing=0 cellpadding=2>" + summary + "</table>");
    }
}

void VBoxNewVMWzd::accept()
{
    /* Try to create the machine when the Finish button is pressed.
     * On failure, the wisard will remain open to give it another try. */
    if (constructMachine())
        QDialog::accept();
}

void VBoxNewVMWzd::showVDIManager()
{
    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", Qt::WType_Dialog | Qt::WShowModal);
    dlg.setup (VBoxDefs::HD, true);
    QUuid newId = dlg.exec() == VBoxDiskImageManagerDlg::Accepted ?
        dlg.getSelectedUuid() : mMediaCombo->getId();

    if (uuidHD != newId)
    {
        ensureNewHardDiskDeleted();
        uuidHD = newId;
        mMediaCombo->setCurrentItem (uuidHD);
    }
    mMediaCombo->setFocus();

    mWvalHDD->revalidate();
}

void VBoxNewVMWzd::showNewVDIWizard()
{
    VBoxNewHDWzd dlg (this);

    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());

    dlg.setRecommendedFileName (mLeName->text());
    dlg.setRecommendedSize (type.GetRecommendedHDD());

    if (dlg.exec() == QDialog::Accepted)
    {
        ensureNewHardDiskDeleted();
        chd = dlg.hardDisk();
        /* fetch uuid and name/path */
        uuidHD = chd.GetId();
        /* update media combobox */
        VBoxMedia::Status status =
            chd.GetAccessible() == TRUE ? VBoxMedia::Ok :
            chd.isOk() ? VBoxMedia::Inaccessible :
            VBoxMedia::Error;
        vboxGlobal().addMedia (VBoxMedia (CUnknown (chd), VBoxDefs::HD, status));
        mMediaCombo->setCurrentItem (uuidHD);
        mMediaCombo->setFocus();
        /* revalidate */
        mWvalHDD->revalidate();
    }
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
}

void VBoxNewVMWzd::currentMediaChanged (int /* aItem */)
{
    uuidHD = mMediaCombo->getId();
    mWvalHDD->revalidate();
}

void VBoxNewVMWzd::revalidate (QIWidgetValidator *aWval)
{
    /* do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWvalHDD)
    {
        if (!chd.isNull() &&
            mMediaCombo->currentIndex() != mMediaCombo->count() - 1)
            ensureNewHardDiskDeleted();
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
        mMediaCombo->setFocus();
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
    if (sender() == mBtnNext4 &&
        !mMediaCombo->currentIndex() &&
        !vboxProblem().confirmHardDisklessMachine (this))
        return;

    /* Switch to the next page */
    QIAbstractWizard::showNextPage();
}


bool VBoxNewVMWzd::constructMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* create a machine with the default settings file location */
    if (cmachine.isNull())
    {
        cmachine = vbox.CreateMachine (QString(), mLeName->text(), QUuid());
        if (!vbox.isOk())
        {
            vboxProblem().cannotCreateMachine (vbox, this);
            return false;
        }
        if (uuidHD.isNull() || !chd.isNull())
            cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, "yes");
    }

    /* OS type */
    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOS->currentIndex());
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    cmachine.SetOSTypeId (type.GetId());

    if (type.GetId() == "os2warp3" ||
        type.GetId() == "os2warp4" ||
        type.GetId() == "os2warp45")
        cmachine.SetHWVirtExEnabled (KTSBool_True);

    /* RAM size */
    cmachine.SetMemorySize (mSlRAM->value());

    /* Add one network adapter (NAT) by default */
    {
        CNetworkAdapter cadapter = cmachine.GetNetworkAdapter (0);
#ifdef VBOX_WITH_E1000
        /* Default to e1k on solaris */
        if (type.GetId() == "solaris" ||
            type.GetId() == "opensolaris")
            cadapter.SetAdapterType (KNetworkAdapterType_I82540EM);
#endif /* VBOX_WITH_E1000 */
        cadapter.SetEnabled (true);
        cadapter.AttachToNAT();
        cadapter.SetMACAddress (QString::null);
        cadapter.SetCableConnected (true);
    }

    /* Register the VM prior to attaching hard disks */
    vbox.RegisterMachine (cmachine);
    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateMachine (vbox, cmachine, this);
        return false;
    }

    /* Boot hard disk (Primary Master) */
    if (!uuidHD.isNull())
    {
        bool ok = false;
        QUuid id = cmachine.GetId();
        CSession session = vboxGlobal().openSession (id);
        if (!session.isNull())
        {
            CMachine m = session.GetMachine();
            m.AttachHardDisk (uuidHD, KStorageBus_IDE, 0, 0);
            if (m.isOk())
            {
                m.SaveSettings();
                if (m.isOk())
                    ok = true;
                else
                    vboxProblem().cannotSaveMachineSettings (m, this);
            }
            else
                vboxProblem().cannotAttachHardDisk (this, m, uuidHD,
                                                    KStorageBus_IDE, 0, 0);
            session.Close();
        }
        if (!ok)
        {
            /* unregister on failure */
            vbox.UnregisterMachine (id);
            if (vbox.isOk())
                cmachine.DeleteSettings();
            return false;
        }
    }

    /* ensure we don't delete a newly created hard disk on success */
    chd.detach();

    return true;
}

void VBoxNewVMWzd::ensureNewHardDiskDeleted()
{
    if (!chd.isNull())
    {
        QUuid hdId = chd.GetId();
        CVirtualBox vbox = vboxGlobal().virtualBox();
        vbox.UnregisterHardDisk (chd.GetId());
        if (!vbox.isOk())
            vboxProblem().cannotUnregisterMedia (this, vbox, VBoxDefs::HD,
                                                 chd.GetLocation());
        else
        {
            CVirtualDiskImage vdi = CUnknown (chd);
            if (!vdi.isNull())
            {
                vdi.DeleteImage();
                if (!vdi.isOk())
                    vboxProblem().cannotDeleteHardDiskImage (this, vdi);
            }
        }
        chd.detach();
        vboxGlobal().removeMedia (VBoxDefs::HD, hdId);
    }
}

