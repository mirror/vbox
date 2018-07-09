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
# include <QCheckBox>
# include <QGridLayout>
# include <QGroupBox>
# include <QLabel>
# include <QLineEdit>
# include <QListWidget>
# include <QRadioButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIApplianceExportEditorWidget.h"
# include "UIEmptyFilePathSelector.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageExpert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageExpert implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageExpert::UIWizardExportAppPageExpert(const QStringList &selectedVMNames)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        /* Create VM selector container: */
        m_pSelectorCnt = new QGroupBox;
        if (m_pSelectorCnt)
        {
            /* Create VM selector container layout: */
            QVBoxLayout *pSelectorCntLayout = new QVBoxLayout(m_pSelectorCnt);
            if (pSelectorCntLayout)
            {
                /* Create VM selector: */
                m_pVMSelector = new QListWidget;
                if (m_pVMSelector)
                {
                    m_pVMSelector->setAlternatingRowColors(true);
                    m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);

                    /* Add into layout: */
                    pSelectorCntLayout->addWidget(m_pVMSelector);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSelectorCnt, 0, 0);
        }

        /* Create appliance widget container: */
        m_pApplianceCnt = new QGroupBox;
        if (m_pApplianceCnt)
        {
            /* Create appliance widget container layout: */
            QVBoxLayout *pApplianceCntLayout = new QVBoxLayout(m_pApplianceCnt);
            if (pApplianceCntLayout)
            {
                /* Create appliance widget: */
                m_pApplianceWidget = new UIApplianceExportEditorWidget;
                if (m_pApplianceWidget)
                {
                    m_pApplianceWidget->setMinimumHeight(250);
                    m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

                    /* Add into layout: */
                    pApplianceCntLayout->addWidget(m_pApplianceWidget);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pApplianceCnt, 0, 1);
        }

        /* Create storage type container: */
        m_pTypeCnt = new QGroupBox;
        if (m_pTypeCnt)
        {
            /* Hide it for now: */
            m_pTypeCnt->hide();

            /* Create storage type container layout: */
            QVBoxLayout *pTypeCntLayout = new QVBoxLayout(m_pTypeCnt);
            if (pTypeCntLayout)
            {
                /* Create Local LocalFilesystem radio-button: */
                m_pTypeLocalFilesystem = new QRadioButton;
                if (m_pTypeLocalFilesystem)
                {
                    /* Add into layout: */
                    pTypeCntLayout->addWidget(m_pTypeLocalFilesystem);
                }
                /* Create CloudProvider radio-button: */
                m_pTypeCloudServiceProvider = new QRadioButton;
                if (m_pTypeCloudServiceProvider)
                {
                    /* Add into layout: */
                    pTypeCntLayout->addWidget(m_pTypeCloudServiceProvider);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pTypeCnt, 1, 0, 1, 2);
        }

        /* Create settings widget container: */
        m_pSettingsCnt = new QGroupBox;
        if (m_pSettingsCnt)
        {
            /* Create settings widget container layout: */
            QGridLayout *pSettingsLayout = new QGridLayout(m_pSettingsCnt);
            if (pSettingsLayout)
            {
                /* Create file selector: */
                m_pFileSelector = new UIEmptyFilePathSelector;
                if (m_pFileSelector)
                {
                    m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Save);
                    m_pFileSelector->setEditable(true);
                    m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                    m_pFileSelector->setDefaultSaveExt("ova");

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFileSelector, 4, 1);
                }
                /* Create file selector label: */
                m_pFileSelectorLabel = new QLabel;
                if (m_pFileSelectorLabel)
                {
                    m_pFileSelectorLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pFileSelectorLabel->setBuddy(m_pFileSelector);

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFileSelectorLabel, 4, 0);
                }

                /* Create format combo-box editor: */
                m_pFormatComboBox = new QComboBox;
                if (m_pFormatComboBox)
                {
                    const QString strFormatOVF09("ovf-0.9");
                    const QString strFormatOVF10("ovf-1.0");
                    const QString strFormatOVF20("ovf-2.0");
                    const QString strFormatOPC10("opc-1.0");
                    m_pFormatComboBox->addItem(strFormatOVF09, strFormatOVF09);
                    m_pFormatComboBox->addItem(strFormatOVF10, strFormatOVF10);
                    m_pFormatComboBox->addItem(strFormatOVF20, strFormatOVF20);
                    m_pFormatComboBox->addItem(strFormatOPC10, strFormatOPC10);

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFormatComboBox, 5, 1);
                }
                /* Create format combo-box label: */
                m_pFormatComboBoxLabel = new QLabel;
                if (m_pFormatComboBoxLabel)
                {
                    m_pFormatComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                    m_pFormatComboBoxLabel->setBuddy(m_pFormatComboBox);

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFormatComboBoxLabel, 5, 0);
                }

                /* Create advanced label: */
                m_pAdditionalLabel = new QLabel;
                if (m_pAdditionalLabel)
                {
                    m_pAdditionalLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pAdditionalLabel, 6, 0);
                }
                /* Create manifest check-box editor: */
                m_pManifestCheckbox = new QCheckBox;
                if (m_pManifestCheckbox)
                {
                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pManifestCheckbox, 6, 1);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt, 2, 0, 1, 2);
        }
    }

    /* Populate VM selector items: */
    populateVMSelectorItems(selectedVMNames);
    /* Choose default storage type: */
    chooseDefaultStorageType();
    /* Choose default settings: */
    chooseDefaultSettings();

    /* Setup connections: */
    connect(m_pVMSelector, &QListWidget::itemSelectionChanged,      this, &UIWizardExportAppPageExpert::sltVMSelectionChangeHandler);
    connect(m_pTypeLocalFilesystem, &QRadioButton::clicked,         this, &UIWizardExportAppPageExpert::sltStorageTypeChangeHandler);
    connect(m_pTypeCloudServiceProvider, &QRadioButton::clicked,    this, &UIWizardExportAppPageExpert::sltStorageTypeChangeHandler);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);

    /* Register classes: */
    qRegisterMetaType<StorageType>();
    qRegisterMetaType<ExportAppliancePointer>();

    /* Register fields: */
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");
    registerField("storageType", this, "storageType");
    registerField("path", this, "path");
    registerField("format", this, "format");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("applianceWidget", this, "applianceWidget");
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
    m_pTypeLocalFilesystem->setText(UIWizardExportApp::tr("&Local Filesystem"));
    m_pTypeCloudServiceProvider->setText(UIWizardExportApp::tr("&Cloud Service Provider"));
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
    updateProviderComboToolTip();
}

void UIWizardExportAppPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Refresh current settings: */
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
        const bool fOVF =    field("format").toString() == "ovf-0.9"
                          || field("format").toString() == "ovf-1.0"
                          || field("format").toString() == "ovf-2.0";
        const bool fOPC =    field("format").toString() == "opc-1.0";
        fResult =    (   fOVF
                      && VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
                  || (   fOPC
                      && VBoxGlobal::hasAllowedExtension(strFile, OPCFileExts));
        if (fResult)
        {
            const StorageType enmStorageType = storageType();
            switch (enmStorageType)
            {
                case LocalFilesystem:
                    break;
                case CloudProvider:
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

void UIWizardExportAppPageExpert::sltVMSelectionChangeHandler()
{
    /* Refresh current settings: */
    refreshCurrentSettings();
    refreshApplianceSettingsWidget();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltStorageTypeChangeHandler()
{
    /* Refresh current settings: */
    refreshCurrentSettings();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleFormatComboChange()
{
    /* Refresh current settings: */
    refreshCurrentSettings();
    updateFormatComboToolTip();
}
