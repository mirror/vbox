//Added by qt3to4:
#include <QShowEvent>
/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "First Run Wizard" wizard UI include (Qt Designer)
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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

void VBoxVMFirstRunWzd::init()
{
    /* initial wizard setup
     * --------------------------------------------------------------------- */

    /* disable help buttons */
    helpButton()->setShown (false);

    /* fix tab order to get the proper direction
     * (originally the focus goes Next/Finish -> Back -> Cancel -> page) */
    setTabOrder (backButton(), nextButton());
    setTabOrder (nextButton(), finishButton());
    setTabOrder (finishButton(), cancelButton());

    /* setup the label clolors for nice scaling */
    VBoxGlobal::adoptLabelPixmap (pmWelcome);
    VBoxGlobal::adoptLabelPixmap (pmType);
    VBoxGlobal::adoptLabelPixmap (pmSummary);

    /* media page */
    cbImage = new VBoxMediaComboBox (bgSource, "cbImage", VBoxDefs::CD);
#warning port me
//    ltVdm->insertWidget (0, cbImage);
    tbVdm->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                            "select_file_dis_16px.png"));
    setTabOrder (cbImage, tbVdm);

    /* summary page */
    teSummary = new QITextEdit (pageSummary);
    teSummary->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Minimum);
    teSummary->setFrameShape (Q3TextEdit::NoFrame);
    teSummary->setReadOnly (TRUE);
    teSummary->setPaper (pageSummary->backgroundBrush());
#warning port me
//    ltSummary->insertWidget (2, teSummary);

    /* setup connections and set validation for pages
     * --------------------------------------------------------------------- */

    /* media page */
    wvalType = new QIWidgetValidator (pageType, this);
    connect (wvalType, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (wvalType, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

    /* filter out Enter keys in order to direct them to the default dlg button */
    QIKeyFilter *ef = new QIKeyFilter (this, Qt::Key_Enter);
    ef->watchOn (teSummary);

    /* set initial values
     * --------------------------------------------------------------------- */

    /* the finish button on the Summary page is always enabled */
    setFinishEnabled (pageSummary, true);

    /* setup minimum width for the sizeHint to be calculated correctly */
#warning port me
    int wid = 1;
//    int wid = widthSpacer->minimumSize().width();
    txWelcome->setMinimumWidth (wid);
    txType->setMinimumWidth (wid);
    txSource->setMinimumWidth (wid);
    txSummaryHdr->setMinimumWidth (wid);
    txSummaryFtr->setMinimumWidth (wid);
    txWelcomeHD->setMinimumWidth (wid);
    txTypeHD->setMinimumWidth (wid);
    txSourceHD->setMinimumWidth (wid);
    txSummaryHdrHD->setMinimumWidth (wid);
    txSummaryFtrHD->setMinimumWidth (wid);

    /* media page */
    rbCdType->animateClick();
    rbHost->animateClick();
}


void VBoxVMFirstRunWzd::setup (const CMachine &aMachine)
{
    machine = aMachine;

    CHardDiskAttachmentEnumerator en = machine.GetHardDiskAttachments().Enumerate();
    if (en.HasMore())
    {
        txWelcomeHD->setHidden (true);
        txTypeHD->setHidden (true);
        txSourceHD->setHidden (true);
        txSummaryHdrHD->setHidden (true);
        txSummaryFtrHD->setHidden (true);
    }
    else
    {
        txWelcome->setHidden (true);
        txType->setHidden (true);
        txSource->setHidden (true);
        txSummaryHdr->setHidden (true);
        txSummaryFtr->setHidden (true);
    }
}


void VBoxVMFirstRunWzd::showEvent (QShowEvent *aEvent)
{
    QDialog::showEvent (aEvent);

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


void VBoxVMFirstRunWzd::showPage (QWidget *aPage)
{
    if (aPage == pageSummary)
    {
        QString type =
            rbCdType->isChecked() ? tr ("CD/DVD-ROM Device") :
            rbFdType->isChecked() ? tr ("Floppy Device") :
            QString::null;
        QString source =
            rbHost->isChecked() ? tr ("Host Drive %1").arg (cbHost->currentText()) :
            rbImage->isChecked() ? cbImage->currentText() : QString::null;
        QString summary =
            QString (tr ("<table><tr><td>Type:</td><td>%1</td></tr>"
                         "<tr><td>Source:</td><td>%2</td></tr></table>"))
                         .arg (type).arg (source);
        teSummary->setText (summary);
        /* set Finish to default */
        finishButton()->setDefault (true);
    }
    else
    {
        /* always set Next to default */
        nextButton()->setDefault (true);
    }

    Q3Wizard::showPage (aPage);

    /* fix focus on the last page. when we go to the last page
     * having the Next in focus the focus goes to the Cancel
     * button because when the Next hides Finish is not yet shown. */
    if (aPage == pageSummary && focusWidget() == cancelButton())
        finishButton()->setFocus();

    /* setup focus for individual pages */
    if (aPage == pageType)
        bgType->setFocus();
    else if (aPage == pageSummary)
        teSummary->setFocus();

    aPage->layout()->activate();
}


void VBoxVMFirstRunWzd::accept()
{
    /* CD/DVD Media selected */
    if (rbCdType->isChecked())
    {
        if (rbHost->isChecked())
        {
            CHostDVDDrive hostDrive = hostDVDs [cbHost->currentItem()];
            if (!hostDrive.isNull())
            {
                CDVDDrive virtualDrive = machine.GetDVDDrive();
                virtualDrive.CaptureHostDrive (hostDrive);
            }
        }
        else if (rbImage->isChecked())
        {
            CDVDDrive virtualDrive = machine.GetDVDDrive();
            virtualDrive.MountImage (cbImage->getId());
        }
    }
    /* Floppy Media selected */
    else if (rbFdType->isChecked())
    {
        if (rbHost->isChecked())
        {
            CHostFloppyDrive hostDrive = hostFloppys [cbHost->currentItem()];
            if (!hostDrive.isNull())
            {
                CFloppyDrive virtualDrive = machine.GetFloppyDrive();
                virtualDrive.CaptureHostDrive (hostDrive);
            }
        }
        else if (rbImage->isChecked())
        {
            CFloppyDrive virtualDrive = machine.GetFloppyDrive();
            virtualDrive.MountImage (cbImage->getId());
        }
    }

    Q3Wizard::accept();
}


void VBoxVMFirstRunWzd::enableNext (const QIWidgetValidator *aWval)
{
    setNextEnabled (aWval->widget(), aWval->isValid());
}


void VBoxVMFirstRunWzd::revalidate (QIWidgetValidator *aWval)
{
    /* do individual validations for pages */
    QWidget *pg = aWval->widget();
    bool valid = aWval->isOtherValid();

    if (pg == pageType)
    {
        valid = (rbHost->isChecked() && !cbHost->currentText().isEmpty()) ||
                (rbImage->isChecked() && !cbImage->currentText().isEmpty());
    }

    aWval->setOtherValid (valid);
}


void VBoxVMFirstRunWzd::mediaTypeChanged()
{
    /* CD/DVD Media type selected */
    cbHost->clear();
    if (sender() == rbCdType)
    {
        /* Search for the host dvd-drives */
        CHostDVDDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetDVDDrives();
        hostDVDs.resize (coll.GetCount());
        int id = 0;
        CHostDVDDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostDVDDrive hostDVD = en.GetNext();
            QString name = hostDVD.GetName();
            QString description = hostDVD.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            cbHost->insertItem (fullName, id);
            hostDVDs [id] = hostDVD;
            ++ id;
        }

        /* Switch media images type to CD */
        cbImage->setType (VBoxDefs::CD);
    }
    /* Floppy Media type selected */
    else if (sender() == rbFdType)
    {
        /* Search for the host floppy-drives */
        CHostFloppyDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
        hostFloppys.resize (coll.GetCount());
        int id = 0;
        CHostFloppyDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostFloppyDrive hostFloppy = en.GetNext();
            QString name = hostFloppy.GetName();
            QString description = hostFloppy.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            cbHost->insertItem (fullName, id);
            hostFloppys [id] = hostFloppy;
            ++ id;
        }

        /* Switch media images type to FD */
        cbImage->setType (VBoxDefs::FD);
    }
    /* Update media images list */
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
        cbImage->refresh();

    /* Revalidate updated page */
    wvalType->revalidate();
}


void VBoxVMFirstRunWzd::mediaSourceChanged()
{
    cbHost->setEnabled (sender() == rbHost);
    cbImage->setEnabled (sender() == rbImage);
    tbVdm->setEnabled (sender() == rbImage);

    /* Revalidate updated page */
    wvalType->revalidate();
}


void VBoxVMFirstRunWzd::openVdm()
{
    VBoxDiskImageManagerDlg vdm (this, "VBoxDiskImageManagerDlg",
                                 Qt::WType_Dialog | Qt::WShowModal);
    QUuid machineId = machine.GetId();
    VBoxDefs::DiskType type = rbCdType->isChecked() ? VBoxDefs::CD :
        rbFdType->isChecked() ? VBoxDefs::FD : VBoxDefs::InvalidType;
    vdm.setup (type, true, &machineId);
    if (vdm.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        cbImage->setCurrentItem (vdm.getSelectedUuid());

        /* Revalidate updated page */
        wvalType->revalidate();
    }
}
