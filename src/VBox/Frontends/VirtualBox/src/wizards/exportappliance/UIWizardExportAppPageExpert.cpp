/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageExpert class implementation
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

/* Global includes: */
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>

/* Local includes: */
#include "UIWizardExportAppPageExpert.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppDefs.h"
#include "VBoxGlobal.h"
#include "QILabelSeparator.h"
#include "VBoxFilePathSelectorWidget.h"
#include "UIApplianceExportEditorWidget.h"

UIWizardExportAppPageExpert::UIWizardExportAppPageExpert(const QStringList &selectedVMNames)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        pMainLayout->setContentsMargins(8, 6, 8, 6);
        m_pVMSelectorLabel = new QILabelSeparator(this);
        m_pVMSelector = new QListWidget(this);
        {
            m_pVMSelector->setAlternatingRowColors(true);
            m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);
            m_pVMSelectorLabel->setBuddy(m_pVMSelector);
        }
        m_pVMApplianceLabel = new QILabelSeparator(this);
        m_pApplianceWidget = new UIApplianceExportEditorWidget(this);
        {
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            m_pVMApplianceLabel->setBuddy(m_pApplianceWidget);
        }
        m_pTypeCnt = new QGroupBox(this);
        {
            m_pTypeCnt->hide();
            QVBoxLayout *pTypeCntLayout = new QVBoxLayout(m_pTypeCnt);
            {
                m_pTypeLocalFilesystem = new QRadioButton(m_pTypeCnt);
                m_pTypeSunCloud = new QRadioButton(m_pTypeCnt);
                m_pTypeSimpleStorageSystem = new QRadioButton(m_pTypeCnt);
                pTypeCntLayout->addWidget(m_pTypeLocalFilesystem);
                pTypeCntLayout->addWidget(m_pTypeSunCloud);
                pTypeCntLayout->addWidget(m_pTypeSimpleStorageSystem);
            }
        }
        QGridLayout *pSettingsLayout = new QGridLayout;
        {
            m_pUsernameEditor = new QLineEdit(this);
            m_pUsernameLabel = new QLabel(this);
            {
                m_pUsernameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pUsernameLabel->setBuddy(m_pUsernameEditor);
            }
            m_pPasswordEditor = new QLineEdit(this);
            {
                m_pPasswordEditor->setEchoMode(QLineEdit::Password);
            }
            m_pPasswordLabel = new QLabel(this);
            {
                m_pPasswordLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pPasswordLabel->setBuddy(m_pPasswordEditor);
            }
            m_pHostnameEditor = new QLineEdit(this);
            m_pHostnameLabel = new QLabel(this);
            {
                m_pHostnameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pHostnameLabel->setBuddy(m_pHostnameEditor);
            }
            m_pBucketEditor = new QLineEdit(this);
            m_pBucketLabel = new QLabel(this);
            {
                m_pBucketLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pBucketLabel->setBuddy(m_pBucketEditor);
            }
            m_pFileSelector = new VBoxEmptyFileSelector(this);
            {
                m_pFileSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Save);
                m_pFileSelector->setEditable(true);
                m_pFileSelector->setButtonPosition(VBoxEmptyFileSelector::RightPosition);
                m_pFileSelector->setDefaultSaveExt("ova");
            }
            m_pFileSelectorLabel = new QLabel(this);
            {
                m_pFileSelectorLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pFileSelectorLabel->setBuddy(m_pFileSelector);
            }
            pSettingsLayout->addWidget(m_pUsernameLabel, 0, 0);
            pSettingsLayout->addWidget(m_pUsernameEditor, 0, 1);
            pSettingsLayout->addWidget(m_pPasswordLabel, 1, 0);
            pSettingsLayout->addWidget(m_pPasswordEditor, 1, 1);
            pSettingsLayout->addWidget(m_pHostnameLabel, 2, 0);
            pSettingsLayout->addWidget(m_pHostnameEditor, 2, 1);
            pSettingsLayout->addWidget(m_pBucketLabel, 3, 0);
            pSettingsLayout->addWidget(m_pBucketEditor, 3, 1);
            pSettingsLayout->addWidget(m_pFileSelectorLabel, 4, 0);
            pSettingsLayout->addWidget(m_pFileSelector, 4, 1);
        }
        m_pOVF09Checkbox = new QCheckBox(this);
        m_pManifestCheckbox = new QCheckBox(this);
        pMainLayout->addWidget(m_pVMSelectorLabel, 0, 0);
        pMainLayout->addWidget(m_pVMApplianceLabel, 0, 1);
        pMainLayout->addWidget(m_pVMSelector, 1, 0);
        pMainLayout->addWidget(m_pApplianceWidget, 1, 1);
        pMainLayout->addWidget(m_pTypeCnt, 2, 0, 1, 2);
        pMainLayout->addLayout(pSettingsLayout, 3, 0, 1, 2);
        pMainLayout->addWidget(m_pOVF09Checkbox, 4, 0, 1, 2);
        pMainLayout->addWidget(m_pManifestCheckbox, 5, 0, 1, 2);
        populateVMSelectorItems(selectedVMNames);
        chooseDefaultStorageType();
        chooseDefaultSettings();
    }

    /* Setup connections: */
    connect(m_pVMSelector, SIGNAL(itemSelectionChanged()), this, SLOT(sltVMSelectionChangeHandler()));
    connect(m_pTypeLocalFilesystem, SIGNAL(clicked()), this, SLOT(sltStorageTypeChangeHandler()));
    connect(m_pTypeSunCloud, SIGNAL(clicked()), this, SLOT(sltStorageTypeChangeHandler()));
    connect(m_pTypeSimpleStorageSystem, SIGNAL(clicked()), this, SLOT(sltStorageTypeChangeHandler()));
    connect(m_pUsernameEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pPasswordEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pHostnameEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pBucketEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pFileSelector, SIGNAL(pathChanged(const QString &)), this, SIGNAL(completeChanged()));

    /* Register classes: */
    qRegisterMetaType<StorageType>();
    qRegisterMetaType<ExportAppliancePointer>();
    /* Register fields: */
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");
    registerField("storageType", this, "storageType");
    registerField("OVF09Selected", this, "OVF09Selected");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("username", this, "username");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
    registerField("bucket", this, "bucket");
    registerField("path", this, "path");
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardExportAppPageExpert::sltVMSelectionChangeHandler()
{
    /* Call to base-class: */
    refreshApplianceSettingsWidget();
    refreshCurrentSettings();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltStorageTypeChangeHandler()
{
    /* Call to base-class: */
    refreshCurrentSettings();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardExportAppPageExpert::retranslateUi()
{
    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");
    /* Translate widgets: */
    m_pVMSelectorLabel->setText(UIWizardExportApp::tr("Virtual &machines to export"));
    m_pVMApplianceLabel->setText(UIWizardExportApp::tr("Appliance &settings"));
    m_pTypeCnt->setTitle(UIWizardExportApp::tr("&Destination"));
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&Local Filesystem "));
    m_pTypeSunCloud->setText(UIWizardExportApp::tr("Sun &Cloud"));
    m_pTypeSimpleStorageSystem->setText(UIWizardExportApp::tr("&Simple Storage System (S3)"));
    m_pUsernameLabel->setText(UIWizardExportApp::tr("&Username:"));
    m_pPasswordLabel->setText(UIWizardExportApp::tr("&Password:"));
    m_pHostnameLabel->setText(UIWizardExportApp::tr("&Hostname:"));
    m_pBucketLabel->setText(UIWizardExportApp::tr("&Bucket:"));
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a virtual appliance file"));
    m_pFileSelector->setFileFilters(UIWizardExportApp::tr("Open Virtualization Format Archive (%1)").arg("*.ova") + ";;" +
                                    UIWizardExportApp::tr("Open Virtualization Format (%1)").arg("*.ovf"));
    m_pOVF09Checkbox->setToolTip(UIWizardExportApp::tr("Write in legacy OVF 0.9 format for compatibility with other virtualization products."));
    m_pOVF09Checkbox->setText(UIWizardExportApp::tr("&Write legacy OVF 0.9"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("Write &Manifest file"));
}

void UIWizardExportAppPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Call to base-class: */
    refreshApplianceSettingsWidget();
    refreshCurrentSettings();
}

bool UIWizardExportAppPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* There should be at least one vm selected: */
    if (fResult)
        fResult = (m_pVMSelector->selectedItems().size() > 0);

    /* Check storage-type attributes: */
    if (fResult)
    {
        const QString &strFile = m_pFileSelector->path().toLower();
        fResult = VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::OVFFileExts);
        if (fResult)
        {
            StorageType st = storageType();
            switch (st)
            {
                case Filesystem:
                    break;
                case SunCloud:
                    fResult &= !m_pUsernameEditor->text().isEmpty() &&
                               !m_pPasswordEditor->text().isEmpty() &&
                               !m_pBucketEditor->text().isEmpty();
                    break;
                case S3:
                    fResult &= !m_pUsernameEditor->text().isEmpty() &&
                               !m_pPasswordEditor->text().isEmpty() &&
                               !m_pHostnameEditor->text().isEmpty() &&
                               !m_pBucketEditor->text().isEmpty();
                    break;
            }
        }
    }

    /* Return result: */
    return fResult;
}

bool UIWizardExportAppPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to export appliance: */
    fResult = qobject_cast<UIWizardExportApp*>(wizard())->exportAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

