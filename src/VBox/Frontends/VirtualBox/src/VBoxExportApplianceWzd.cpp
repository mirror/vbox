/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxExportAppliance class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "VBoxExportApplianceWzd.h"
#include "VBoxGlobal.h"
#include "QIWidgetValidator.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QDir>

class VMListWidgetItems: public QListWidgetItem
{
public:
    VMListWidgetItems (QPixmap &aIcon, QString &aText, QListWidget *aParent)
      : QListWidgetItem (aIcon, aText, aParent) {}

    /* Sort like in the VM selector of the main window */
    bool operator< (const QListWidgetItem &aOther) const
    {
        return text().toLower() < aOther.text().toLower();
    }
};

////////////////////////////////////////////////////////////////////////////////
// VBoxExportApplianceWzd

VBoxExportApplianceWzd::VBoxExportApplianceWzd (QWidget *aParent /* = NULL */, const QString& aSelectName /* = QString::null */)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxExportApplianceWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Configure the VM selector widget */
    mVMListWidget->setAlternatingRowColors (true);
    mVMListWidget->setSelectionMode (QAbstractItemView::ExtendedSelection);

    /* Validator for the VM selector page */
    mWValVMSelector = new QIWidgetValidator (mVMSelectPage, this);
    connect (mWValVMSelector, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValVMSelector, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mVMListWidget, SIGNAL (itemSelectionChanged()),
             mWValVMSelector, SLOT (revalidate()));

    /* Fill the VM selector list */
    addListViewVMItems (aSelectName);
    mWValVMSelector->revalidate();

    /* Configure the file selector */
    mFileSelector->setMode (VBoxFilePathSelectorWidget::Mode_File_Save);
    mFileSelector->setResetEnabled (false);
    mFileSelector->setFileDialogTitle (tr ("Select a file to export into"));
    mFileSelector->setFileFilters (tr ("Open Virtualization Format (%1)").arg ("*.ovf"));
    mFileSelector->setDefaultSaveExt ("ovf");
#ifdef Q_WS_MAC
    /* Editable boxes are uncommon on the Mac */
    mFileSelector->setEditable (false);
#endif /* Q_WS_MAC */

    /* Validator for the file selector page */
    mWValFileSelector = new QIWidgetValidator (mFileSelectPage, this);
    connect (mWValFileSelector, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValFileSelector, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mFileSelector, SIGNAL (pathChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));

    mWValFileSelector->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

void VBoxExportApplianceWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxExportApplianceWzd::retranslateUi (this);

    mDefaultApplianceName = tr("Appliance");
}

void VBoxExportApplianceWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWValVMSelector)
        valid = mVMListWidget->selectedItems().size() > 0;

    if (aWval == mWValFileSelector)
        valid = mFileSelector->path().toLower().endsWith (".ovf");

    aWval->setOtherValid (valid);
}

void VBoxExportApplianceWzd::enableNext (const QIWidgetValidator *aWval)
{
    if (aWval == mWValFileSelector)
        finishButton()->setEnabled (aWval->isValid());
    else
        nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxExportApplianceWzd::accept()
{
    /* Check if the file exists already, if yes get confirmation for
     * overwriting from the user. */
    if (!vboxProblem().askForOverridingFileIfExists (mFileSelector->path(), this))
        return;
    /* Export the VMs, on success we are finished */
    if (exportVMs())
        QIAbstractWizard::accept();
}

void VBoxExportApplianceWzd::showNextPage()
{
    /* We propose a filename the first time the second page is displayed */
    if (sender() == mBtnNext1)
        if (mFileSelector->path().isEmpty())
        {
            /* Set the default filename */
            QString name = mDefaultApplianceName;
            /* If it is one VM only, we use the VM name as file name */
            if (mVMListWidget->selectedItems().size() == 1)
                name = mVMListWidget->selectedItems().first()->text();

            mFileSelector->setPath (QDir::toNativeSeparators (QString ("%1/%2.ovf").arg (vboxGlobal().documentsPath())
                                                                                   .arg (name)));
            mWValFileSelector->revalidate();
        }

    QIAbstractWizard::showNextPage();
}

void VBoxExportApplianceWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mFileSelectPage)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxExportApplianceWzd::addListViewVMItems (const QString& aSelectName)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector vec = vbox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
    {
        QPixmap icon;
        QString name;
        QString uuid;
        bool enabled;
        if (m->GetAccessible())
        {
            icon = vboxGlobal().vmGuestOSTypeIcon (m->GetOSTypeId()).scaled (16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            name = m->GetName();
            uuid = m->GetId().toString();
            enabled = m->GetSessionState() == KSessionState_Closed;
        }
        else
        {
            QString settingsFile = m->GetSettingsFilePath();
            QFileInfo fi (settingsFile);
            name = fi.completeSuffix().toLower() == "xml" ?
                   fi.completeBaseName() : fi.fileName();
            icon = QPixmap (":/os_other.png").scaled (16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            enabled = false;
        }
        QListWidgetItem *item = new VMListWidgetItems (icon, name, mVMListWidget);
        item->setData (Qt::UserRole, uuid);
        if (!enabled)
            item->setFlags (0);
        mVMListWidget->addItem (item);
    }
    mVMListWidget->sortItems();

    /* Make sure aSelectName is initial selected in the list */
    QList<QListWidgetItem *> list = mVMListWidget->findItems (aSelectName, Qt::MatchExactly);
    if (list.size() > 0)
        mVMListWidget->setCurrentItem (list.first());
}

bool VBoxExportApplianceWzd::exportVMs()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Create a appliance object */
    CAppliance appliance = vbox.CreateAppliance();
    bool fResult = appliance.isOk();
    if (fResult)
    {
        /* Iterate over all selected items */
        QList<QListWidgetItem *> list = mVMListWidget->selectedItems();
        foreach (const QListWidgetItem* item, list)
        {
            /* The VM uuid can be fetched by the UserRole */
            QString uuid = item->data (Qt::UserRole).toString();
            /* Get the machine with the uuid */
            CMachine m = vbox.GetMachine (uuid);
            fResult = m.isOk();
            if (fResult)
            {
                /* Add the export description to our appliance object */
                m.Export (appliance);
                fResult = m.isOk();
                if (!fResult)
                    break;
            }
            else
                break;
        }
        /* Proceed if there where no errors */
        if (fResult)
        {
            /* Write the appliance */
            CProgress progress = appliance.Write (mFileSelector->path());
            fResult = appliance.isOk();
            if (fResult)
            {
                /* Show some progress, so the user know whats going on */
                vboxProblem().showModalProgressDialog (progress, tr ("Exporting Appliance ..."), this);
                if (!progress.isOk() || progress.GetResultCode() != 0)
                {
                    vboxProblem().cannotExportAppliance (progress, &appliance, this);
                    return false;
                }
                else
                    return true;
            }
        }
    }
    if (!fResult)
        vboxProblem().cannotExportAppliance (&appliance, this);

    return false;
}

