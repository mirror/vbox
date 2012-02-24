/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIExportApplianceWzd class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QDir>

/* Local includes */
#include "UIExportApplianceWzd.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

class VMListWidgetItems : public QListWidgetItem
{
public:

    VMListWidgetItems(QPixmap &pixIcon, QString &strText, QString strUuid, bool fInSaveState, QListWidget *pParent)
        : QListWidgetItem(pixIcon, strText, pParent)
        , m_strUuid(strUuid)
        , m_fInSaveState(fInSaveState)
    {
    }

    /* Sort like in the VM selector of the main window */
    bool operator<(const QListWidgetItem &other) const
    {
        return text().toLower() < other.text().toLower();
    }

    QString uuid() { return m_strUuid; }
    bool isInSaveState() { return m_fInSaveState; }

private:
    QString m_strUuid;
    bool m_fInSaveState;
};

UIExportApplianceWzd::UIExportApplianceWzd(QWidget *pParent, const QStringList &selectedVMNames) : QIWizard(pParent)
{
    /* Create & add pages */
    addPage(new UIExportApplianceWzdPage1);
    addPage(new UIExportApplianceWzdPage2);
    addPage(new UIExportApplianceWzdPage3);
    addPage(new UIExportApplianceWzdPage4);

    /* Set 'selectedVMNames' field for wizard page 1: */
    setField("selectedVMNames", selectedVMNames);

    /* Initial translate */
    retranslateUi();

    /* Initial translate all pages */
    retranslateAllPages();

#ifndef Q_WS_MAC
    /* Assign watermark */
    assignWatermark(":/vmw_ovf_export.png");
#else /* Q_WS_MAC */
    /* Assign background image */
    assignBackground(":/vmw_ovf_export_bg.png");
#endif /* Q_WS_MAC */

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio(UIWizardType_ExportAppliance);

    /* Setup connections */
    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(sltCurrentIdChanged(int)));
    AssertMsg(!field("applianceWidget").value<ExportAppliancePointer>().isNull(),
              ("Appliance Widget Pointer is not set!\n"));
    connect(this, SIGNAL(customButtonClicked(int)),
            field("applianceWidget").value<ExportAppliancePointer>(), SLOT(restoreDefaults()));
}

void UIExportApplianceWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("Appliance Export Wizard"));

    /* Extra buttons */
    setButtonText(QWizard::CustomButton1, tr("Restore Defaults"));
    setButtonText(QWizard::FinishButton, tr("Export"));
}

void UIExportApplianceWzd::sltCurrentIdChanged(int iId)
{
    setOption(QWizard::HaveCustomButton1, iId == 1 /* Page #2 */);
}

UIExportApplianceWzdPage1::UIExportApplianceWzdPage1()
{
    /* Decorate page */
    Ui::UIExportApplianceWzdPage1::setupUi(this);

    /* Register 'selectedVMNames', 'machineNames', 'machineIDs' fields! */
    registerField("selectedVMNames", this, "selectedVMNames");
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");

    /* Configure 'VM Selector' settings */
    m_pVMSelector->setAlternatingRowColors(true);
    m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);

    /* Configure 'VM Selector' connections */
    connect(m_pVMSelector, SIGNAL(itemSelectionChanged()), this, SLOT(sltSelectedVMChanged()));

    /* Fill 'VM Selector' */
    populateVMSelectorItems();
}

void UIExportApplianceWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIExportApplianceWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the Appliance Export Wizard!"));

    m_pPage1Text1->setText(tr("<p>This wizard will guide you through the process of "
                              "exporting an appliance.</p><p>%1</p><p>Please select "
                              "the virtual machines that should be added to the "
                              "appliance. You can select more than one. Please note "
                              "that these machines have to be turned off before they "
                              "can be exported.</p>")
                           .arg(standardHelpText()));
}

void UIExportApplianceWzdPage1::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Choose initially selected items (if passed) */
    for (int i = 0; i < m_selectedVMNames.size(); ++i)
    {
        QList<QListWidgetItem*> list = m_pVMSelector->findItems(m_selectedVMNames[i], Qt::MatchExactly);
        if (list.size() > 0)
        {
            if (m_pVMSelector->selectedItems().isEmpty())
                m_pVMSelector->setCurrentItem(list.first());
            else
                list.first()->setSelected(true);
        }
    }
}

void UIExportApplianceWzdPage1::cleanupPage()
{
    /* Do NOT call superclass method, it will clean default (initially set) field - 'selectedVMNames'! */
}

bool UIExportApplianceWzdPage1::isComplete() const
{
    /* There should be at least one vm selected! */
    return m_pVMSelector->selectedItems().size() > 0;
}

bool UIExportApplianceWzdPage1::validatePage()
{
    /* Ask user machines which are in save state currently. */
    QStringList savedMachines;
    QList<QListWidgetItem*> pItems = m_pVMSelector->selectedItems();
    for (int i=0; i < pItems.size(); ++i)
    {
        if (static_cast<VMListWidgetItems*>(pItems.at(i))->isInSaveState())
            savedMachines << pItems.at(i)->text();
    }

    if (!savedMachines.isEmpty())
        return msgCenter().confirmExportMachinesInSaveState(savedMachines, this);

    return true;
}

int UIExportApplianceWzdPage1::nextId() const
{
    /* Skip next (3rd, storage-type) page for now! */
    return wizard()->page(QIWizardPage::nextId())->nextId();
}

void UIExportApplianceWzdPage1::sltSelectedVMChanged()
{
    /* Clear lists initially */
    m_MachineNames.clear();
    m_MachineIDs.clear();
    /* Iterate over all the selected items */
    foreach (QListWidgetItem *item, m_pVMSelector->selectedItems())
    {
        m_MachineNames << item->text();
        m_MachineIDs << static_cast<VMListWidgetItems*>(item)->uuid();
    }
    /* Revalidate page */
    emit completeChanged();
}

void UIExportApplianceWzdPage1::populateVMSelectorItems()
{
    /* Add all VM items into 'VM Selector' */
    foreach (const CMachine &m, vboxGlobal().virtualBox().GetMachines())
    {
        QPixmap pixIcon;
        QString strName;
        QString strUuid;
        bool fInSaveState = false;
        bool fEnabled = false;
        if (m.GetAccessible())
        {
            pixIcon = vboxGlobal().vmGuestOSTypeIcon(m.GetOSTypeId()).scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            strName = m.GetName();
            strUuid = m.GetId();
            fEnabled = m.GetSessionState() == KSessionState_Unlocked;
            fInSaveState = m.GetState() == KMachineState_Saved;
        }
        else
        {
            QString settingsFile = m.GetSettingsFilePath();
            QFileInfo fi(settingsFile);
            strName = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxDefs::VBoxFileExts) ? fi.completeBaseName() : fi.fileName();
            pixIcon = QPixmap(":/os_other.png").scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        QListWidgetItem *item = new VMListWidgetItems(pixIcon, strName, strUuid, fInSaveState, m_pVMSelector);
        if (!fEnabled)
            item->setFlags(0);
        m_pVMSelector->addItem(item);
    }
    m_pVMSelector->sortItems();
}

UIExportApplianceWzdPage2::UIExportApplianceWzdPage2()
{
    /* Decorate page */
    Ui::UIExportApplianceWzdPage2::setupUi(this);

    /* Register StorageType class */
    qRegisterMetaType<StorageType>();

    /* Register 'storageType' field! */
    registerField("storageType", this, "storageType");

    /* Setup connections */
    connect (m_pTypeLocalFilesystem, SIGNAL(clicked()), this, SLOT(sltStorageTypeChanged()));
    connect (m_pTypeSunCloud, SIGNAL(clicked()), this, SLOT(sltStorageTypeChanged()));
    connect (m_pTypeSimpleStorageSystem, SIGNAL(clicked()), this, SLOT(sltStorageTypeChanged()));

#if 0
    /* Load storage-type from GUI extra data */
    bool ok;
    StorageType storageType =
        StorageType(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_Export_StorageType).toInt(&ok));
    if (ok)
    {
        switch (storageType)
        {
            case Filesystem:
                m_pTypeLocalFilesystem->setChecked(true);
                m_pTypeLocalFilesystem->setFocus();
                break;
            case SunCloud:
                m_pTypeSunCloud->setChecked(true);
                m_pTypeSunCloud->setFocus();
                break;
            case S3:
                m_pTypeSimpleStorageSystem->setChecked(true);
                m_pTypeSimpleStorageSystem->setFocus();
                break;
        }
    }
#else
    /* Just select first of types */
    m_pTypeLocalFilesystem->click();
#endif
}

void UIExportApplianceWzdPage2::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIExportApplianceWzdPage2::retranslateUi(this);

    /* Wizard page 3 title */
    setTitle(tr("Appliance Export Settings"));
}

void UIExportApplianceWzdPage2::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Revert to initial choice */
    m_pTypeLocalFilesystem->click();
}

void UIExportApplianceWzdPage2::sltStorageTypeChanged()
{
    /* Update selected storage-type */
    if (m_pTypeLocalFilesystem->isChecked())
        m_StorageType = Filesystem;
    else if (m_pTypeSunCloud->isChecked())
        m_StorageType = SunCloud;
    else if (m_pTypeSimpleStorageSystem->isChecked())
        m_StorageType = S3;
    else
        m_StorageType = Filesystem;
    /* Revalidate page */
    emit completeChanged();
}

UIExportApplianceWzdPage3::UIExportApplianceWzdPage3()
{
    /* Decorate page */
    Ui::UIExportApplianceWzdPage3::setupUi(this);

    /* Configure the file selector */
    m_pFileSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Save);
    m_pFileSelector->setEditable(true);
    m_pFileSelector->setButtonPosition(VBoxEmptyFileSelector::RightPosition);
    m_pFileSelector->setDefaultSaveExt("ova");

    /* Complete validators for the file selector page */
    connect(m_pLeUsername, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLePassword, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeHostname, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeBucket, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pFileSelector, SIGNAL(pathChanged(const QString &)), this, SIGNAL(completeChanged()));

    /* Register 'target' fields! */
    registerField("OVF09Selected", this, "OVF09Selected");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("username", this, "username");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
    registerField("bucket", this, "bucket");
    registerField("path", this, "path");

#if 0
    /* Load default attributes from GUI extra data */
    m_pLeUsername->setText(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_Export_Username));
    m_pLeHostname->setText(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_Export_Hostname));
    m_pLeBucket->setText(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_Export_Bucket));
#endif
}

void UIExportApplianceWzdPage3::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIExportApplianceWzdPage3::retranslateUi(this);

    /* Wizard page 4 title */
    setTitle(tr("Appliance Export Settings"));

    /* Setup defaults */
    m_strDefaultApplianceName = tr("Appliance");

    /* Translate the file selector */
    m_pFileSelector->setFileDialogTitle(tr("Select a file to export into"));
    m_pFileSelector->setFileFilters(tr("Open Virtualization Format Archive (%1)").arg("*.ova")  + ";;" + tr("Open Virtualization Format (%1)").arg("*.ovf"));
}

void UIExportApplianceWzdPage3::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* Setup components for chosen storage-type */
    StorageType storageType = field("storageType").value<StorageType>();
    switch (storageType)
    {
        case Filesystem:
        {
            m_pPage4Text1->setText(tr("Please choose a filename to export the "
                                      "OVF/OVA to. If you use an <i>ova</i> file name "
                                      "extension, then all the files will be "
                                      "combined into one Open Virtualization "
                                      "Format Archive. If you use an <i>ovf</i> "
                                      "extension, several files will be written "
                                      "separately. Other extensions are not "
                                      "allowed."));
            m_pTxUsername->setVisible(false);
            m_pLeUsername->setVisible(false);
            m_pTxPassword->setVisible(false);
            m_pLePassword->setVisible(false);
            m_pTxHostname->setVisible(false);
            m_pLeHostname->setVisible(false);
            m_pTxBucket->setVisible(false);
            m_pLeBucket->setVisible(false);
            m_pSelectOVF09->setVisible(true);
            m_pFileSelector->setChooserVisible(true);
            m_pFileSelector->setFocus();
            break;
        }
        case SunCloud:
        {
            m_pPage4Text1->setText(tr("Please complete the additional fields like the username, password "
                                      "and the bucket, and provide a filename for the OVF target."));
            m_pTxUsername->setVisible(true);
            m_pLeUsername->setVisible(true);
            m_pTxPassword->setVisible(true);
            m_pLePassword->setVisible(true);
            m_pTxHostname->setVisible(false);
            m_pLeHostname->setVisible(false);
            m_pTxBucket->setVisible(true);
            m_pLeBucket->setVisible(true);
            m_pSelectOVF09->setVisible(false);
            m_pSelectOVF09->setChecked(false);
            m_pFileSelector->setChooserVisible(false);
            m_pLeUsername->setFocus();
            break;
        }
        case S3:
        {
            m_pPage4Text1->setText(tr("Please complete the additional fields like the username, password, "
                                      "hostname and the bucket, and provide a filename for the OVF target."));
            m_pTxUsername->setVisible(true);
            m_pLeUsername->setVisible(true);
            m_pTxPassword->setVisible(true);
            m_pLePassword->setVisible(true);
            m_pTxHostname->setVisible(true);
            m_pLeHostname->setVisible(true);
            m_pTxBucket->setVisible(true);
            m_pLeBucket->setVisible(true);
            m_pSelectOVF09->setVisible(true);
            m_pFileSelector->setChooserVisible(false);
            m_pLeUsername->setFocus();
            break;
        }
    }

    if (!m_pFileSelector->path().isEmpty())
    {
        QFileInfo fi(m_pFileSelector->path());
        QString strName = fi.fileName();
        if (storageType == Filesystem)
            strName = QDir::toNativeSeparators(QString("%1/%2").arg(vboxGlobal().documentsPath()).arg(strName));
        m_pFileSelector->setPath(strName);
    }

    if (m_pFileSelector->path().isEmpty())
    {
        /* Set the default filename */
        QString strName = m_strDefaultApplianceName;
        /* If it is one VM only, we use the VM name as file name */
        if (field("machineNames").toStringList().size() == 1)
            strName = field("machineNames").toStringList()[0];

        strName += ".ova";

        if (storageType == Filesystem)
            strName = QDir::toNativeSeparators(QString("%1/%2").arg(vboxGlobal().documentsPath()).arg(strName));
        m_pFileSelector->setPath(strName);
    }
}

bool UIExportApplianceWzdPage3::isComplete() const
{
    const QString &strFile = m_pFileSelector->path().toLower();
    bool bComplete = VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::OVFFileExts);
    StorageType storageType = field("storageType").value<StorageType>();
    switch (storageType)
    {
        case Filesystem:
            break;
        case SunCloud:
            bComplete &= !m_pLeUsername->text().isEmpty() && !m_pLePassword->text().isEmpty() &&
                         !m_pLeBucket->text().isEmpty();
            break;
        case S3:
            bComplete &= !m_pLeUsername->text().isEmpty() && !m_pLePassword->text().isEmpty() &&
                         !m_pLeHostname->text().isEmpty() && !m_pLeBucket->text().isEmpty();
            break;
    }
    return bComplete;
}

UIExportApplianceWzdPage4::UIExportApplianceWzdPage4()
{
    /* Decorate page */
    Ui::UIExportApplianceWzdPage4::setupUi(this);

    /* Register ExportAppliancePointer class */
    qRegisterMetaType<ExportAppliancePointer>();

    /* Register 'applianceWidget' field! */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIExportApplianceWzdPage4::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIExportApplianceWzdPage4::retranslateUi(this);

    /* Wizard page 2 title */
    setTitle(tr("Appliance Export Settings"));
}

void UIExportApplianceWzdPage4::initializePage()
{
    /* Fill and translate */
    retranslateUi();

    /* We propose a filename the first time the second page is displayed */
    prepareSettingsWidget();
}

void UIExportApplianceWzdPage4::cleanupPage()
{
    /* Do NOT call superclass method, it will clean default (initially set) field - 'applianceWidget'! */
}

bool UIExportApplianceWzdPage4::validatePage()
{
    startProcessing();
    bool fResult = exportAppliance();
    endProcessing();
    return fResult;
}

bool UIExportApplianceWzdPage4::prepareSettingsWidget()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CAppliance *appliance = m_pApplianceWidget->init();
    bool fResult = appliance->isOk();
    if (fResult)
    {
        /* Iterate over all the selected machine ids */
        QStringList uuids = field("machineIDs").toStringList();
        foreach (const QString &uuid, uuids)
        {
            /* Get the machine with the uuid */
            CMachine m = vbox.FindMachine(uuid);
            fResult = m.isOk();
            if (fResult)
            {
                /* Add the export description to our appliance object */
                CVirtualSystemDescription vsd = m.Export(*appliance, uri());
                fResult = m.isOk();
                if (!fResult)
                {
                    msgCenter().cannotExportAppliance(m, appliance, this);
                    return false;
                }
                /* Now add some new fields the user may change */
                vsd.AddDescription(KVirtualSystemDescriptionType_Product, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_ProductUrl, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_Vendor, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_VendorUrl, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_Version, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_License, "", "");
            }
            else
                break;
        }
        /* Make sure the settings widget get the new descriptions */
        m_pApplianceWidget->populate();
    }
    if (!fResult)
        msgCenter().cannotExportAppliance(appliance, this);
    return fResult;
}

bool UIExportApplianceWzdPage4::exportAppliance()
{
    /* Fetch all settings from the appliance editor. */
    m_pApplianceWidget->prepareExport();
    /* Get the appliance. */
    CAppliance *appliance = m_pApplianceWidget->appliance();
    /* We need to know every filename which will be created, so that we can
     * ask the user for confirmation of overwriting. For that we iterating
     * over all virtual systems & fetch all descriptions of the type
     * HardDiskImage. Also add the manifest file to the check. In the ova
     * case only the target file itself get checked. */
    QFileInfo fi(field("path").toString());
    QVector<QString> files;
    files << fi.fileName();
    if (fi.suffix().toLower() == "ovf")
    {
        if (field("manifestSelected").toBool())
            files << fi.baseName() + ".mf";
        CVirtualSystemDescriptionVector vsds = appliance->GetVirtualSystemDescriptions();
        for (int i = 0; i < vsds.size(); ++ i)
        {
            QVector<KVirtualSystemDescriptionType> types;
            QVector<QString> refs, origValues, configValues, extraConfigValues;
            vsds[i].GetDescriptionByType(KVirtualSystemDescriptionType_HardDiskImage, types,
                                         refs, origValues, configValues, extraConfigValues);
            foreach (const QString &s, origValues)
                files << QString("%2").arg(s);
        }
    }
    CVFSExplorer explorer = appliance->CreateVFSExplorer(uri(false /* fWithFile */));
    CProgress progress = explorer.Update();
    bool fResult = explorer.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on */
        msgCenter().showModalProgressDialog(progress, tr("Checking files ..."), "", this);
        if (progress.GetCanceled())
            return false;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            msgCenter().cannotCheckFiles(progress, this);
            return false;
        }
    }
    QVector<QString> exists = explorer.Exists(files);
    /* Check if the file exists already, if yes get confirmation for overwriting from the user. */
    if (!msgCenter().askForOverridingFiles(exists, this))
        return false;
    /* Ok all is confirmed so delete all the files which exists */
    if (!exists.isEmpty())
    {
        CProgress progress1 = explorer.Remove(exists);
        fResult = explorer.isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            msgCenter().showModalProgressDialog(progress1, tr("Removing files ..."), "", this);
            if (progress1.GetCanceled())
                return false;
            if (!progress1.isOk() || progress1.GetResultCode() != 0)
            {
                msgCenter().cannotRemoveFiles(progress1, this);
                return false;
            }
        }
    }

    /* Export the VMs, on success we are finished */
    if (exportVMs(*appliance))
    {
#if 0
        /* Save attributes to GUI extra data */
        StorageType storageType = field("storageType").value<StorageType>();
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_StorageType, QString::number(storageType));
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Username, m_pLeUsername->text());
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Hostname, m_pLeHostname->text());
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Bucket, m_pLeBucket->text());
#endif
        return true;
    }
    return false;
}

bool UIExportApplianceWzdPage4::exportVMs(CAppliance &appliance)
{
    /* Write the appliance */
    const QString version = field("OVF09Selected").toBool() ? "ovf-0.9" : "ovf-1.0";
    CProgress progress = appliance.Write(version, field("manifestSelected").toBool() /* fManifest */, uri());
    bool fResult = appliance.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on */
        msgCenter().showModalProgressDialog(progress, tr("Exporting Appliance ..."), ":/progress_export_90px.png", this, true);
        if (progress.GetCanceled())
            return false;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            msgCenter().cannotExportAppliance(progress, &appliance, this);
            return false;
        }
        else
            return true;
    }
    if (!fResult)
        msgCenter().cannotExportAppliance(&appliance, this);
    return false;
}

QString UIExportApplianceWzdPage4::uri(bool fWithFile) const
{
    StorageType type = field("storageType").value<StorageType>();

    QString path = field("path").toString();
    if (!fWithFile)
    {
        QFileInfo fi(path);
        path = fi.path();
    }
    switch (type)
    {
        case Filesystem:
        {
            return path;
        }
        case SunCloud:
        {
            QString uri("SunCloud://");
            if (!field("username").toString().isEmpty())
                uri = QString("%1%2").arg(uri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                uri = QString("%1:%2").arg(uri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("username").toString().isEmpty())
                uri = QString("%1@").arg(uri);
            uri = QString("%1%2/%3/%4").arg(uri).arg("object.storage.network.com").arg(field("bucket").toString()).arg(path);
            return uri;
        }
        case S3:
        {
            QString uri("S3://");
            if (!field("username").toString().isEmpty())
                uri = QString("%1%2").arg(uri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                uri = QString("%1:%2").arg(uri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("password").toString().isEmpty())
                uri = QString("%1@").arg(uri);
            uri = QString("%1%2/%3/%4").arg(uri).arg(field("hostname").toString()).arg(field("bucket").toString()).arg(path);
            return uri;
        }
    }
    return QString();
}

