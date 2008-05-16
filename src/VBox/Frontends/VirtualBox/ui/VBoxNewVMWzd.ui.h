/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "New virtual machine" wizard UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/


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

void VBoxNewVMWzd::init()
{
    /* disable help buttons */
    helpButton()->setShown (false);

    /*
     *  fix tab order to get the proper direction
     *  (originally the focus goes Next/Finish -> Back -> Cancel -> page)
     */
    QWidget::setTabOrder (backButton(), nextButton());
    QWidget::setTabOrder (nextButton(), finishButton());
    QWidget::setTabOrder (finishButton(), cancelButton());

    /*
     *  setup connections and set validation for pages
     *  ----------------------------------------------------------------------
     */

    /* setup the label colors for nice scaling */
    VBoxGlobal::adoptLabelPixmap (pmWelcome);
    VBoxGlobal::adoptLabelPixmap (pmNameAndOS);
    VBoxGlobal::adoptLabelPixmap (pmMemory);
    VBoxGlobal::adoptLabelPixmap (pmHDD);
    VBoxGlobal::adoptLabelPixmap (pmSummary);

    /* Name and OS page */

    leName->setValidator (new QRegExpValidator (QRegExp (".+" ), this));

    wvalNameAndOS = new QIWidgetValidator (pageNameAndOS, this);
    connect (wvalNameAndOS, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));

    connect (cbOS, SIGNAL (activated (int)), this, SLOT (cbOS_activated (int)));

    /* Memory page */

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();

    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();

    leRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));

    wvalMemory = new QIWidgetValidator (pageMemory, this);
    connect (wvalMemory, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));

    /* HDD Images page */
    mediaCombo = new VBoxMediaComboBox (grbHDA, "mediaCombo", VBoxDefs::HD, true);
    grbHDALayout->addMultiCellWidget (mediaCombo, 0, 0, 0, 2);
    setTabOrder (mediaCombo, pbNewHD);
    setTabOrder (pbNewHD, pbExistingHD);
    connect (mediaCombo, SIGNAL (activated (int)),
             this, SLOT (currentMediaChanged (int)));
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        mediaCombo->refresh();

    /// @todo (dmik) remove?
    wvalHDD = new QIWidgetValidator (pageHDD, this);
    connect (wvalHDD, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (wvalHDD, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

    /* Summary page */

    teSummary = new QITextEdit (pageSummary);
    teSummary->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);
    teSummary->setFrameShape (QTextEdit::NoFrame);
    teSummary->setReadOnly (TRUE);
    summaryLayout->insertWidget (1, teSummary);

    /* filter out Enter keys in order to direct them to the default dlg button */
    QIKeyFilter *ef = new QIKeyFilter (this, Key_Enter);
    ef->watchOn (teSummary);

    /*
     *  set initial values
     *  ----------------------------------------------------------------------
     */

    /* Name and OS page */

    cbOS->insertStringList (vboxGlobal().vmGuestOSTypeDescriptions());
    cbOS_activated (cbOS->currentItem());

    /* Memory page */

    slRAM->setPageStep (calcPageStep (MaxRAM));
    slRAM->setLineStep (slRAM->pageStep() / 4);
    slRAM->setTickInterval (slRAM->pageStep());
    /* setup the scale so that ticks are at page step boundaries */
    slRAM->setMinValue ((MinRAM / slRAM->pageStep()) * slRAM->pageStep());
    slRAM->setMaxValue (MaxRAM);
    txRAMMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MinRAM));
    txRAMMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MaxRAM));
    /*
     *  initial RAM value is set in cbOS_activated()
     *  limit min/max. size of QLineEdit
     */
    leRAM->setMaximumSize (leRAM->fontMetrics().width ("99999")
                           + leRAM->frameWidth() * 2,
                           leRAM->minimumSizeHint().height());
    leRAM->setMinimumSize (leRAM->maximumSize());
    /* ensure leRAM value and validation is updated */
    slRAM_valueChanged (slRAM->value());

    /* HDD Images page */

    /* Summary page */

    teSummary->setPaper (pageSummary->backgroundBrush());

    /*
     *  update the next button state for pages with validation
     *  (validityChanged() connected to enableNext() will do the job)
     */
    wvalNameAndOS->revalidate();
    wvalMemory->revalidate();
    wvalHDD->revalidate();

    /* the finish button on the Summary page is always enabled */
    setFinishEnabled (pageSummary, true);

    /* setup minimum width for the sizeHint to be calculated correctly */
    int wid = widthSpacer->minimumSize().width();
    txWelcome->setMinimumWidth (wid);
    txNameAndOS->setMinimumWidth (wid);
    textLabel1->setMinimumWidth (wid);
    txRAMBest2->setMinimumWidth (wid);
    textLabel1_3->setMinimumWidth (wid);
    txVDIBest->setMinimumWidth (wid);
    txSummaryHdr->setMinimumWidth (wid);
    txSummaryFtr->setMinimumWidth (wid);
}


void VBoxNewVMWzd::destroy()
{
    ensureNewHardDiskDeleted();
}

void VBoxNewVMWzd::showEvent (QShowEvent *e)
{
    QDialog::showEvent (e);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    layout()->activate();

    /* resize to the miminum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
}

void VBoxNewVMWzd::enableNext (const QIWidgetValidator *wval)
{
    setNextEnabled (wval->widget(), wval->isValid());
}


void VBoxNewVMWzd::revalidate (QIWidgetValidator *wval)
{
    /* do individual validations for pages */

    bool valid = wval->isOtherValid();

    if (wval == wvalHDD)
    {
        if (!chd.isNull() && mediaCombo->getId() != chd.GetId())
            ensureNewHardDiskDeleted();
    }

    wval->setOtherValid( valid );
}


void VBoxNewVMWzd::showPage (QWidget *page)
{
    if (page == pageSummary)
    {
        if (!mediaCombo->currentItem())
        {
            if (!vboxProblem().confirmHardDisklessMachine (this))
                return;
        }

        /* compose summary */
        QString summary = QString (tr (
            "<tr><td>Name:</td><td>%1</td></tr>"
            "<tr><td>OS Type:</td><td>%2</td></tr>"
            "<tr><td>Base Memory:</td><td>%3&nbsp;MB</td></tr>"))
            .arg (leName->text())
            .arg (vboxGlobal().vmGuestOSType (cbOS->currentItem()).GetDescription())
            .arg (slRAM->value());

        if (mediaCombo->currentItem())
            summary += QString (tr (
                "<tr><td>Boot Hard Disk:</td><td>%4</td></tr>"))
                .arg (mediaCombo->currentText());

        teSummary->setText ("<table>" + summary + "</table>");

        /* set Finish to default */
        finishButton()->setDefault( true );
    }
    else
    {
        /* always set Next to default */
        nextButton()->setDefault( true );
    }

    QWizard::showPage (page);

    /*
     *  fix focus on the last page. when we go to the last page
     *  having the Next in focus the focus goes to the Cancel
     *  button because when the Next hides Finish is not yet shown.
     */
    if (page == pageSummary && focusWidget() == cancelButton())
        finishButton()->setFocus();

    /* setup focus for individual pages */
    if (page == pageNameAndOS)
        leName->setFocus();
    else if (page == pageMemory)
        slRAM->setFocus();
    else if (page == pageHDD)
        mediaCombo->setFocus();
    else if (page == pageSummary)
        teSummary->setFocus();

    page->layout()->activate();
}

void VBoxNewVMWzd::accept()
{
    /*
     *  Try to create the machine when the Finish button is pressed.
     *  On failure, the wisard will remain open to give it another try.
     */
    if (constructMachine())
        QWizard::accept();
}

bool VBoxNewVMWzd::constructMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* create a machine with the default settings file location */
    if (cmachine.isNull())
    {
        cmachine = vbox.CreateMachine (QString(), leName->text(), QUuid());
        if (!vbox.isOk())
        {
            vboxProblem().cannotCreateMachine (vbox, this);
            return false;
        }
        if (uuidHD.isNull() || !chd.isNull())
            cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, "yes");
    }

    /* name is set in CreateMachine() */

    /* OS type */
    CGuestOSType type = vboxGlobal().vmGuestOSType (cbOS->currentItem());
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    cmachine.SetOSTypeId (type.GetId());

    if (type.GetId() == "os2warp3" ||
        type.GetId() == "os2warp4" ||
        type.GetId() == "os2warp45")
        cmachine.SetHWVirtExEnabled (KTSBool_True);

    /* RAM size */
    cmachine.SetMemorySize (slRAM->value());

    /* add one network adapter (NAT) by default */
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

    /* register the VM prior to attaching hard disks */
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

CMachine VBoxNewVMWzd::machine()
{
    return cmachine;
}

void VBoxNewVMWzd::showVDIManager()
{
    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", WType_Dialog | WShowModal);
    dlg.setup (VBoxDefs::HD, true);
    QUuid newId = dlg.exec() == VBoxDiskImageManagerDlg::Accepted ?
        dlg.getSelectedUuid() : mediaCombo->getId();

    if (uuidHD != newId)
    {
        ensureNewHardDiskDeleted();
        uuidHD = newId;
        mediaCombo->setCurrentItem (uuidHD);
    }
    mediaCombo->setFocus();
    /* revailidate */
    wvalHDD->revalidate();
}

void VBoxNewVMWzd::showNewVDIWizard()
{
    VBoxNewHDWzd dlg (this, "VBoxNewHDWzd");

    CGuestOSType type = vboxGlobal().vmGuestOSType (cbOS->currentItem());

    dlg.setRecommendedFileName (leName->text());
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
        mediaCombo->setCurrentItem (uuidHD);
        mediaCombo->setFocus();
        /* revailidate */
        wvalHDD->revalidate();
    }
}

void VBoxNewVMWzd::slRAM_valueChanged (int val)
{
    leRAM->setText (QString().setNum (val));
}


void VBoxNewVMWzd::leRAM_textChanged (const QString &text)
{
    slRAM->setValue (text.toInt());
}

void VBoxNewVMWzd::cbOS_activated (int item)
{
    CGuestOSType type = vboxGlobal().vmGuestOSType (item);
    pmOS->setPixmap (vboxGlobal().vmGuestOSTypeIcon (type.GetId()));
    txRAMBest->setText (QString::null);
    txRAMBest2->setText (
        tr ("The recommended base memory size is <b>%1</b> MB.")
            .arg (type.GetRecommendedRAM()));
    slRAM->setValue (type.GetRecommendedRAM());
    txVDIBest->setText (
        tr ("The recommended size of the boot hard disk is <b>%1</b> MB.")
            .arg (type.GetRecommendedHDD()));
}

void VBoxNewVMWzd::currentMediaChanged (int)
{
    uuidHD = mediaCombo->getId();
    /* revailidate */
    wvalHDD->revalidate();
}
