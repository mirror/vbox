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
    mFileSelector->setEditable (true);
    mFileSelector->setButtonPosition (VBoxEmptyFileSelector::RightPosition);
    mFileSelector->setFileDialogTitle (tr ("Select a file to export into"));
    mFileSelector->setFileFilters (tr ("Open Virtualization Format (%1)").arg ("*.ovf"));
    mFileSelector->setDefaultSaveExt ("ovf");
    setTabOrder (mLeBucket, mFileSelector);

    /* Connect the restore button with the settings widget */
    connect (mBtnRestore, SIGNAL (clicked()),
             mExportSettingsWgt, SLOT (restoreDefaults()));

    /* Validator for the file selector page */
    mWValFileSelector = new QIWidgetValidator (mTargetOptionsPage, this);
    connect (mWValFileSelector, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValFileSelector, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mFileSelector, SIGNAL (pathChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));
    connect (mLeUsername, SIGNAL (textChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));
    connect (mLePassword, SIGNAL (textChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));
    connect (mLeHostname, SIGNAL (textChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));
    connect (mLeBucket, SIGNAL (textChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));

//    mLeUsername->setText (vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_Export_Username));
//    mLeHostname->setText (vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_Export_Hostname));
//    mLeBucket->setText (vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_Export_Bucket));

    mWValFileSelector->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();

    /* Type selection is disabled for now */
    connect (mBtnNext2, SIGNAL (clicked(bool)),
             mBtnNext3, SLOT (click()));
    connect (mBtnBack4, SIGNAL (clicked(bool)),
             mBtnBack3, SLOT (click()));

//    bool ok;
//    StorageType type = StorageType (vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_Export_StorageType).toInt(&ok));
//    if (ok)
//        setCurrentStorageType (type);
}

void VBoxExportApplianceWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxExportApplianceWzd::retranslateUi (this);

    mDefaultApplianceName = tr("Appliance");

    mExportToFileSystemDesc = tr("Please choose a filename to export the OVF in.");
    mExportToSunCloudDesc = tr("Please complete the additionally fields like the username, password and the bucket. Finally you have to provide a filename for the OVF target.");
    mExportToS3Desc = tr("Please complete the additionally fields like the username, password, hostname and the bucket. Finally you have to provide a filename for the OVF target.");

    switch (currentStorageType())
    {
        case Filesystem: mTextTargetOptions->setText (mExportToFileSystemDesc); break;
        case SunCloud: mTextTargetOptions->setText (mExportToSunCloudDesc); break;
        case S3: mTextTargetOptions->setText (mExportToS3Desc); break;
    }
}

void VBoxExportApplianceWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWValVMSelector)
        valid = mVMListWidget->selectedItems().size() > 0;

    if (aWval == mWValFileSelector)
    {
        valid = mFileSelector->path().toLower().endsWith (".ovf");
        if (currentStorageType() == SunCloud)
        {
            valid &= !mLeUsername->text().isEmpty() &&
                !mLePassword->text().isEmpty() &&
                !mLeBucket->text().isEmpty();
        }
        else if (currentStorageType() == S3)
        {
            valid &= !mLeUsername->text().isEmpty() &&
                !mLePassword->text().isEmpty() &&
                !mLeHostname->text().isEmpty() &&
                !mLeBucket->text().isEmpty();
        }
    }

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
    CAppliance *appliance = mExportSettingsWgt->appliance();
    QFileInfo fi (mFileSelector->path());
    QVector<QString> files;
    files << fi.fileName();
    /* We need to know every filename which will be created, so that we can
     * ask the user for confirmation of overwriting. For that we iterating
     * over all virtual systems & fetch all descriptions of the type
     * HardDiskImage. */
    CVirtualSystemDescriptionVector vsds = appliance->GetVirtualSystemDescriptions();
    for (int i=0; i < vsds.size(); ++i)
    {
        QVector<KVirtualSystemDescriptionType> types;
        QVector<QString> refs, origValues, configValues, extraConfigValues;

        vsds[i].GetDescriptionByType (KVirtualSystemDescriptionType_HardDiskImage, types, refs, origValues, configValues, extraConfigValues);
        foreach (const QString &s, origValues)
            files << QString ("%2").arg (s);
    }
    CVFSExplorer explorer = appliance->CreateVFSExplorer(uri());
    CProgress progress = explorer.Update();
    bool fResult = explorer.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on */
        vboxProblem().showModalProgressDialog (progress, tr ("Checking files ..."), this);
        if (progress.GetCanceled())
            return;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            vboxProblem().cannotCheckFiles (progress, this);
            return;
        }
    }
    QVector<QString> exists = explorer.Exists (files);
    /* Check if the file exists already, if yes get confirmation for
     * overwriting from the user. */
    if (!vboxProblem().askForOverridingFiles (exists, this))
        return;
    /* Ok all is confirmed so delete all the files which exists */
    if (!exists.isEmpty())
    {
        CProgress progress1 = explorer.Remove (exists);
        fResult = explorer.isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            vboxProblem().showModalProgressDialog (progress1, tr ("Removing files ..."), this);
            if (progress1.GetCanceled())
                return;
            if (!progress1.isOk() || progress1.GetResultCode() != 0)
            {
                vboxProblem().cannotRemoveFiles (progress1, this);
                return;
            }
        }
    }
    /* Export the VMs, on success we are finished */
    if (exportVMs (*appliance))
    {
//        vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_Export_StorageType, QString::number(currentStorageType()));
//        vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_Export_Username, mLeUsername->text());
//        vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_Export_Hostname, mLeHostname->text());
//        vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_Export_Bucket, mLeBucket->text());
        QIAbstractWizard::accept();
    }
}

void VBoxExportApplianceWzd::showNextPage()
{
    /* We propose a filename the first time the second page is displayed */
    if (sender() == mBtnNext1)
    {
        prepareSettingsWidget();
    }
    else if (sender() == mBtnNext3)
    {
        storageTypeChanged();
        if (mFileSelector->path().isEmpty())
        {
            /* Set the default filename */
            QString name = mDefaultApplianceName;
            /* If it is one VM only, we use the VM name as file name */
            if (mVMListWidget->selectedItems().size() == 1)
                name = mVMListWidget->selectedItems().first()->text();

            name += ".ovf";

            if (currentStorageType() == Filesystem)
                name = QDir::toNativeSeparators (QString ("%1/%2").arg (vboxGlobal().documentsPath())
                                                 .arg (name));
            mFileSelector->setPath (name);
            mWValFileSelector->revalidate();
        }
        mExportSettingsWgt->prepareExport();
    }

    QIAbstractWizard::showNextPage();
}

void VBoxExportApplianceWzd::onPageShow()
{
    QWidget *page = mPageStack->currentWidget();

    if (page == mTargetOptionsPage)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxExportApplianceWzd::addListViewVMItems (const QString& aSelectName)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector vec = vbox.GetMachines();
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
            uuid = m->GetId();
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

bool VBoxExportApplianceWzd::prepareSettingsWidget()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CAppliance *appliance = mExportSettingsWgt->init();
    bool fResult = appliance->isOk();
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
                CVirtualSystemDescription vsd = m.Export (*appliance);
                fResult = m.isOk();
                if (!fResult)
                {
                    vboxProblem().cannotExportAppliance (m, appliance, this);
                    return false;
                }
                /* Now add some new fields the user may change */
                vsd.AddDescription (KVirtualSystemDescriptionType_Product, "", "");
                vsd.AddDescription (KVirtualSystemDescriptionType_ProductUrl, "", "");
                vsd.AddDescription (KVirtualSystemDescriptionType_Vendor, "", "");
                vsd.AddDescription (KVirtualSystemDescriptionType_VendorUrl, "", "");
                vsd.AddDescription (KVirtualSystemDescriptionType_Version, "", "");
                vsd.AddDescription (KVirtualSystemDescriptionType_License, "", "");
            }
            else
                break;
        }
        /* Make sure the settings widget get the new descriptions */
        mExportSettingsWgt->populate();
    }
    if (!fResult)
        vboxProblem().cannotExportAppliance (appliance, this);
    return fResult;
}

bool VBoxExportApplianceWzd::exportVMs (CAppliance &aAppliance)
{
    /* Write the appliance */
    QString version = mSelectOVF09->isChecked() ? "ovf-0.9" : "ovf-1.0";
    CProgress progress = aAppliance.Write (version, uri());
    bool fResult = aAppliance.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on */
        vboxProblem().showModalProgressDialog (progress, tr ("Exporting Appliance ..."), this);
        if (progress.GetCanceled())
            return false;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            vboxProblem().cannotExportAppliance (progress, &aAppliance, this);
            return false;
        }
        else
            return true;
    }
    if (!fResult)
        vboxProblem().cannotExportAppliance (&aAppliance, this);
    return false;
}

QString VBoxExportApplianceWzd::uri() const
{
    if (currentStorageType() == Filesystem)
        return mFileSelector->path();
    else if (currentStorageType() == SunCloud)
    {
        QString uri ("SunCloud://");
        if (!mLeUsername->text().isEmpty())
            uri = QString ("%1%2").arg (uri).arg (mLeUsername->text());
        if (!mLePassword->text().isEmpty())
            uri = QString ("%1:%2").arg (uri).arg (mLePassword->text());
        if (!mLeUsername->text().isEmpty() ||
            !mLePassword->text().isEmpty())
            uri = QString ("%1@").arg (uri);
        uri = QString ("%1%2/%3/%4").arg (uri).arg ("object.storage.network.com").arg (mLeBucket->text()).arg (mFileSelector->path());
        return uri;
    }
    else if (currentStorageType() == S3)
    {
        QString uri ("S3://");
        if (!mLeUsername->text().isEmpty())
            uri = QString ("%1%2").arg (uri).arg (mLeUsername->text());
        if (!mLePassword->text().isEmpty())
            uri = QString ("%1:%2").arg (uri).arg (mLePassword->text());
        if (!mLeUsername->text().isEmpty() ||
            !mLePassword->text().isEmpty())
            uri = QString ("%1@").arg (uri);
        uri = QString ("%1%2/%3/%4").arg (uri).arg (mLeHostname->text()).arg (mLeBucket->text()).arg (mFileSelector->path());
        return uri;
    }
    return "";
}

VBoxExportApplianceWzd::StorageType VBoxExportApplianceWzd::currentStorageType() const
{
    if (mRBtnLocalFileSystem->isChecked())
        return Filesystem;
    else if (mRBtnSunCloud->isChecked())
        return SunCloud;
    else
        return S3;
}

void VBoxExportApplianceWzd::storageTypeChanged()
{
    StorageType type = currentStorageType();
    switch (type)
    {
        case Filesystem:
        {
            mTextTargetOptions->setText (mExportToFileSystemDesc);
            mLblUsername->setVisible (false);
            mLeUsername->setVisible (false);
            mLblPassword->setVisible (false);
            mLePassword->setVisible (false);
            mLblHostname->setVisible (false);
            mLeHostname->setVisible (false);
            mLblBucket->setVisible (false);
            mLeBucket->setVisible (false);
            mSelectOVF09->setVisible (true);
            mFileSelector->setChooserVisible (true);
            mFileSelector->setFocus();
            break;
        };
        case SunCloud:
        {
            mTextTargetOptions->setText (mExportToSunCloudDesc);
            mLblUsername->setVisible (true);
            mLeUsername->setVisible (true);
            mLblPassword->setVisible (true);
            mLePassword->setVisible (true);
            mLblHostname->setVisible (false);
            mLeHostname->setVisible (false);
            mLblBucket->setVisible (true);
            mLeBucket->setVisible (true);
            mSelectOVF09->setVisible (false);
            mSelectOVF09->setChecked (false);
            mFileSelector->setChooserVisible (false);
            mLeUsername->setFocus();
            break;
        };
        case S3:
        {
            mTextTargetOptions->setText (mExportToS3Desc);
            mLblUsername->setVisible (true);
            mLeUsername->setVisible (true);
            mLblPassword->setVisible (true);
            mLePassword->setVisible (true);
            mLblHostname->setVisible (true);
            mLeHostname->setVisible (true);
            mLblBucket->setVisible (true);
            mLeBucket->setVisible (true);
            mSelectOVF09->setVisible (true);
            mFileSelector->setChooserVisible (false);
            mLeUsername->setFocus();
            break;
        };
    }

    if (!mFileSelector->path().isEmpty())
    {
        QFileInfo fi (mFileSelector->path());
        QString name = fi.fileName();
        if (type == Filesystem)
            name = QDir::toNativeSeparators (QString ("%1/%2").arg (vboxGlobal().documentsPath())
                                             .arg (name));
        mFileSelector->setPath (name);
    }
}

void VBoxExportApplianceWzd::setCurrentStorageType (VBoxExportApplianceWzd::StorageType aType)
{
    switch (aType)
    {
        case Filesystem: mRBtnLocalFileSystem->setChecked(true); mRBtnLocalFileSystem->setFocus(); break;
        case SunCloud: mRBtnSunCloud->setChecked(true); mRBtnSunCloud->setFocus(); break;
        case S3: mRBtnS3->setChecked(true); mRBtnS3->setFocus(); break;
    }
}

