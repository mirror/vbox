/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageExpert class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QVBoxLayout>
# include <QGridLayout>
# include <QListWidget>
# include <QGroupBox>
# include <QRadioButton>
# include <QLineEdit>
# include <QLabel>
# include <QCheckBox>
# include <QGroupBox>

/* GUI includes: */
# include "UIWizardExportAppPageExpert.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "VBoxGlobal.h"
# include "UIEmptyFilePathSelector.h"
# include "UIApplianceExportEditorWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageExpert implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageExpert::UIWizardExportAppPageExpert(const QStringList &selectedVMNames)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        /* Create VM selector container: */
        m_pSelectorCnt = new QGroupBox(this);
        {
            /* Create VM selector container layout: */
            QVBoxLayout *pSelectorCntLayout = new QVBoxLayout(m_pSelectorCnt);
            {
                /* Create VM selector: */
                m_pVMSelector = new QListWidget(m_pSelectorCnt);
                {
                    m_pVMSelector->setAlternatingRowColors(true);
                    m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);
                }

                /* Add into layout: */
                pSelectorCntLayout->addWidget(m_pVMSelector);
            }
        }

        /* Create appliance widget container: */
        m_pApplianceCnt = new QGroupBox(this);
        {
            /* Create appliance widget container layout: */
            QVBoxLayout *pApplianceCntLayout = new QVBoxLayout(m_pApplianceCnt);
            {
                /* Create appliance widget: */
                m_pApplianceWidget = new UIApplianceExportEditorWidget(m_pApplianceCnt);
                {
                    m_pApplianceWidget->setMinimumHeight(250);
                    m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                }

                /* Add into layout: */
                pApplianceCntLayout->addWidget(m_pApplianceWidget);
            }
        }

        /* Create storage type container: */
        m_pTypeCnt = new QGroupBox(this);
        {
            /* Hide it for now: */
            m_pTypeCnt->hide();

            /* Create storage type container layout: */
            QVBoxLayout *pTypeCntLayout = new QVBoxLayout(m_pTypeCnt);
            {
                /* Create Local Filesystem radio-button: */
                m_pTypeLocalFilesystem = new QRadioButton(m_pTypeCnt);
                /* Create SunCloud radio-button: */
                m_pTypeSunCloud = new QRadioButton(m_pTypeCnt);
                /* Create Simple Storage System radio-button: */
                m_pTypeSimpleStorageSystem = new QRadioButton(m_pTypeCnt);

                /* Add into layout: */
                pTypeCntLayout->addWidget(m_pTypeLocalFilesystem);
                pTypeCntLayout->addWidget(m_pTypeSunCloud);
                pTypeCntLayout->addWidget(m_pTypeSimpleStorageSystem);
            }
        }

        /* Create settings widget container: */
        m_pSettingsCnt = new QGroupBox(this);
        {
            /* Create settings widget container layout: */
            QGridLayout *pSettingsLayout = new QGridLayout(m_pSettingsCnt);
            {
                /* Create username editor: */
                m_pUsernameEditor = new QLineEdit(m_pSettingsCnt);
                /* Create username label: */
                m_pUsernameLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pUsernameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pUsernameLabel->setBuddy(m_pUsernameEditor);
                }

                /* Create password editor: */
                m_pPasswordEditor = new QLineEdit(m_pSettingsCnt);
                {
                    m_pPasswordEditor->setEchoMode(QLineEdit::Password);
                }
                /* Create password label: */
                m_pPasswordLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pPasswordLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pPasswordLabel->setBuddy(m_pPasswordEditor);
                }

                /* Create hostname editor: */
                m_pHostnameEditor = new QLineEdit(m_pSettingsCnt);
                /* Create hostname label: */
                m_pHostnameLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pHostnameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pHostnameLabel->setBuddy(m_pHostnameEditor);
                }

                /* Create bucket editor: */
                m_pBucketEditor = new QLineEdit(m_pSettingsCnt);
                /* Create bucket label: */
                m_pBucketLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pBucketLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pBucketLabel->setBuddy(m_pBucketEditor);
                }

                /* Create file selector: */
                m_pFileSelector = new UIEmptyFilePathSelector(m_pSettingsCnt);
                {
                    m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Save);
                    m_pFileSelector->setEditable(true);
                    m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                    m_pFileSelector->setDefaultSaveExt("ova");
                }
                /* Create file selector label: */
                m_pFileSelectorLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pFileSelectorLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pFileSelectorLabel->setBuddy(m_pFileSelector);
                }

                /* Create format combo-box editor: */
                m_pFormatComboBox = new QComboBox(m_pSettingsCnt);
                {
                    const QString strFormatOVF09("ovf-0.9");
                    const QString strFormatOVF10("ovf-1.0");
                    const QString strFormatOVF20("ovf-2.0");
                    const QString strFormatOPC10("opc-1.0");
                    m_pFormatComboBox->addItem(strFormatOVF09, strFormatOVF09);
                    m_pFormatComboBox->addItem(strFormatOVF10, strFormatOVF10);
                    m_pFormatComboBox->addItem(strFormatOVF20, strFormatOVF20);
                    m_pFormatComboBox->addItem(strFormatOPC10, strFormatOPC10);
                    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);
                }
                /* Create format combo-box label: */
                m_pFormatComboBoxLabel = new QLabel(m_pSettingsCnt);
                {
                    m_pFormatComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pFormatComboBoxLabel->setBuddy(m_pFormatComboBox);
                }

                /* Create manifest check-box editor: */
                m_pManifestCheckbox = new QCheckBox(m_pSettingsCnt);

                /* Add into layout: */
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
                pSettingsLayout->addWidget(m_pFormatComboBoxLabel, 5, 0);
                pSettingsLayout->addWidget(m_pFormatComboBox, 5, 1);
                pSettingsLayout->addWidget(m_pManifestCheckbox, 6, 0, 1, 2);
            }
        }

        /* Add into layout: */
        pMainLayout->addWidget(m_pSelectorCnt, 0, 0);
        pMainLayout->addWidget(m_pApplianceCnt, 0, 1);
        pMainLayout->addWidget(m_pTypeCnt, 1, 0, 1, 2);
        pMainLayout->addWidget(m_pSettingsCnt, 2, 0, 1, 2);

        /* Populate VM selector items: */
        populateVMSelectorItems(selectedVMNames);
        /* Choose default storage type: */
        chooseDefaultStorageType();
        /* Choose default settings: */
        chooseDefaultSettings();
    }

    /* Setup connections: */
    connect(m_pVMSelector, &QListWidget::itemSelectionChanged,      this, &UIWizardExportAppPageExpert::sltVMSelectionChangeHandler);
    connect(m_pTypeLocalFilesystem, &QRadioButton::clicked,         this, &UIWizardExportAppPageExpert::sltStorageTypeChangeHandler);
    connect(m_pTypeSunCloud, &QRadioButton::clicked,                this, &UIWizardExportAppPageExpert::sltStorageTypeChangeHandler);
    connect(m_pTypeSimpleStorageSystem, &QRadioButton::clicked,     this, &UIWizardExportAppPageExpert::sltStorageTypeChangeHandler);
    connect(m_pUsernameEditor, &QLineEdit::textChanged,             this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pPasswordEditor, &QLineEdit::textChanged,             this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pHostnameEditor, &QLineEdit::textChanged,             this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pBucketEditor, &QLineEdit::textChanged,               this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageExpert::completeChanged);

    /* Register classes: */
    qRegisterMetaType<StorageType>();
    qRegisterMetaType<ExportAppliancePointer>();

    /* Register fields: */
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");
    registerField("storageType", this, "storageType");
    registerField("format", this, "format");
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
    refreshCurrentSettings();
    refreshApplianceSettingsWidget();

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

void UIWizardExportAppPageExpert::sltHandleFormatComboChange()
{
    refreshCurrentSettings();
    updateFormatComboToolTip();
}

void UIWizardExportAppPageExpert::retranslateUi()
{
    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");

    /* Translate widgets: */
    m_pSelectorCnt->setTitle(UIWizardExportApp::tr("Virtual &machines to export"));
    m_pApplianceCnt->setTitle(UIWizardExportApp::tr("Appliance &settings"));
    m_pTypeCnt->setTitle(UIWizardExportApp::tr("&Destination"));
    m_pSettingsCnt->setTitle(UIWizardExportApp::tr("&Storage settings"));
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&Local Filesystem "));
    m_pTypeSunCloud->setText(UIWizardExportApp::tr("Sun &Cloud"));
    m_pTypeSimpleStorageSystem->setText(UIWizardExportApp::tr("&Simple Storage System (S3)"));
    m_pUsernameLabel->setText(UIWizardExportApp::tr("&Username:"));
    m_pPasswordLabel->setText(UIWizardExportApp::tr("&Password:"));
    m_pHostnameLabel->setText(UIWizardExportApp::tr("&Hostname:"));
    m_pBucketLabel->setText(UIWizardExportApp::tr("&Bucket:"));
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setChooseButtonToolTip(tr("Choose a file to export the virtual appliance to..."));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a file to export the virtual appliance to"));
    m_pFormatComboBoxLabel->setText(UIWizardExportApp::tr("F&ormat:"));
    m_pFormatComboBox->setItemText(0, UIWizardExportApp::tr("Open Virtualization Format 0.9"));
    m_pFormatComboBox->setItemText(1, UIWizardExportApp::tr("Open Virtualization Format 1.0"));
    m_pFormatComboBox->setItemText(2, UIWizardExportApp::tr("Open Virtualization Format 2.0"));
    m_pFormatComboBox->setItemText(3, UIWizardExportApp::tr("Oracle Public Cloud Format 1.0"));
    m_pFormatComboBox->setItemData(0, UIWizardExportApp::tr("Write in legacy OVF 0.9 format for compatibility "
                                                            "with other virtualization products."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(1, UIWizardExportApp::tr("Write in standard OVF 1.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(2, UIWizardExportApp::tr("Write in new OVF 2.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(3, UIWizardExportApp::tr("Write in Oracle Public Cloud 1.0 format."), Qt::ToolTipRole);
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("Write &Manifest file"));

    /* Refresh current settings: */
    refreshCurrentSettings();
    updateFormatComboToolTip();
}

void UIWizardExportAppPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Call to base-class: */
    refreshCurrentSettings();
    refreshApplianceSettingsWidget();
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
        bool fOVF =    field("format").toString() == "ovf-0.9"
                    || field("format").toString() == "ovf-1.0"
                    || field("format").toString() == "ovf-2.0";
        bool fOPC =    field("format").toString() == "opc-1.0";
        fResult =    (   fOVF
                      && VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
                  || (   fOPC
                      && VBoxGlobal::hasAllowedExtension(strFile, OPCFileExts));
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
