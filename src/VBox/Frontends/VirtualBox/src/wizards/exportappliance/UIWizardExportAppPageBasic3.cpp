/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class implementation.
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
# include <QComboBox>
# include <QDir>
# include <QGridLayout>
# include <QLabel>
# include <QLineEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "VBoxGlobal.h"
# include "UIEmptyFilePathSelector.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageBasic3.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage3 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage3::UIWizardExportAppPage3()
{
}

void UIWizardExportAppPage3::populateProviders()
{
    /* Acquire provider list: */
    // Here goes the experiamental list with
    // arbitrary contents for testing purposes.
    QStringList providers;
    providers << "OCI";
    providers << "Amazon";
    providers << "Google";
    providers << "Microsoft";
    m_pProviderComboBox->addItems(providers);

    /* Duplicate non-translated names to data fields: */
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
        m_pProviderComboBox->setItemData(i, m_pProviderComboBox->itemText(i));
}

void UIWizardExportAppPage3::chooseDefaultSettings()
{
    /* Choose defaults: */
    setFormat("ovf-1.0");
    setProvider("OCI");
}

void UIWizardExportAppPage3::updatePageAppearance()
{
    /* Update page appearance according to chosen storage-type: */
    const StorageType enmStorageType = fieldImp("storageType").value<StorageType>();
    switch (enmStorageType)
    {
        case LocalFilesystem:
        {
            m_pFileSelectorLabel->setVisible(true);
            m_pFileSelector->setVisible(true);
            m_pFormatComboBoxLabel->setVisible(true);
            m_pFormatComboBox->setVisible(true);
            m_pAdditionalLabel->setVisible(true);
            m_pManifestCheckbox->setVisible(true);
            m_pProviderComboBoxLabel->setVisible(false);
            m_pProviderComboBox->setVisible(false);
            break;
        }
        case CloudProvider:
        {
            m_pFileSelectorLabel->setVisible(false);
            m_pFileSelector->setVisible(false);
            m_pFormatComboBoxLabel->setVisible(false);
            m_pFormatComboBox->setVisible(false);
            m_pAdditionalLabel->setVisible(false);
            m_pManifestCheckbox->setVisible(false);
            m_pProviderComboBoxLabel->setVisible(true);
            m_pProviderComboBox->setVisible(true);
            break;
        }
    }
}

void UIWizardExportAppPage3::refreshCurrentSettings()
{
    /* Compose file-name: */
    QString strName;

    /* If it's one VM only, we use the VM name as file-name: */
    if (fieldImp("machineNames").toStringList().size() == 1)
        strName = fieldImp("machineNames").toStringList()[0];
    /* Otherwise => we use the default file-name: */
    else
        strName = m_strDefaultApplianceName;

    /* If the format is set to OPC: */
    if (fieldImp("format").toString() == "opc-1.0")
    {
        /* We use .tar.gz extension: */
        strName += ".tar.gz";

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(UIWizardExportApp::tr("Oracle Public Cloud Format Archive (%1)").arg("*.tar.gz"));

        /* Disable manifest file creation: */
        m_pManifestCheckbox->setChecked(false);
        m_pManifestCheckbox->setEnabled(false);
    }
    /* Otherwise: */
    else
    {
        /* We use the default (.ova) extension: */
        strName += ".ova";

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(UIWizardExportApp::tr("Open Virtualization Format Archive (%1)").arg("*.ova") + ";;" +
                                        UIWizardExportApp::tr("Open Virtualization Format (%1)").arg("*.ovf"));

        /* Enable manifest file creation: */
        m_pManifestCheckbox->setEnabled(true);
    }

    /* Copose file-path for 'LocalFilesystem' storage case: */
    const StorageType enmStorageType = fieldImp("storageType").value<StorageType>();
    if (enmStorageType == LocalFilesystem)
        strName = QDir::toNativeSeparators(QString("%1/%2").arg(vboxGlobal().documentsPath()).arg(strName));

    /* Assign file-path: */
    m_pFileSelector->setPath(strName);
}

void UIWizardExportAppPage3::updateFormatComboToolTip()
{
    const int iCurrentIndex = m_pFormatComboBox->currentIndex();
    const QString strCurrentToolTip = m_pFormatComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pFormatComboBox->setToolTip(strCurrentToolTip);
}

void UIWizardExportAppPage3::updateProviderComboToolTip()
{
    const int iCurrentIndex = m_pProviderComboBox->currentIndex();
    const QString strCurrentToolTip = m_pProviderComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pProviderComboBox->setToolTip(strCurrentToolTip);
}

QString UIWizardExportAppPage3::path() const
{
    return m_pFileSelector->path();
}

void UIWizardExportAppPage3::setPath(const QString &strPath)
{
    m_pFileSelector->setPath(strPath);
}

QString UIWizardExportAppPage3::format() const
{
    const int iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex).toString();
}

void UIWizardExportAppPage3::setFormat(const QString &strFormat)
{
    const int iIndex = m_pFormatComboBox->findData(strFormat);
    AssertMsg(iIndex != -1, ("Field not found!"));
    m_pFormatComboBox->setCurrentIndex(iIndex);
}

bool UIWizardExportAppPage3::isManifestSelected() const
{
    return m_pManifestCheckbox->isChecked();
}

void UIWizardExportAppPage3::setManifestSelected(bool fChecked)
{
    m_pManifestCheckbox->setChecked(fChecked);
}

QString UIWizardExportAppPage3::provider() const
{
    const int iIndex = m_pProviderComboBox->currentIndex();
    return m_pProviderComboBox->itemData(iIndex).toString();
}

void UIWizardExportAppPage3::setProvider(const QString &strProvider)
{
    const int iIndex = m_pProviderComboBox->findData(strProvider);
    AssertMsg(iIndex != -1, ("Field not found!"));
    m_pProviderComboBox->setCurrentIndex(iIndex);
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic3 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic3::UIWizardExportAppPageBasic3()
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel;
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create settings layout: */
        QGridLayout *pSettingsLayout = new QGridLayout;
        if (pSettingsLayout)
        {
            pSettingsLayout->setColumnStretch(0, 0);
            pSettingsLayout->setColumnStretch(1, 1);

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

            /* Create format combo-box: */
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
            /* Create manifest check-box: */
            m_pManifestCheckbox = new QCheckBox;
            if (m_pManifestCheckbox)
            {
                /* Add into layout: */
                pSettingsLayout->addWidget(m_pManifestCheckbox, 6, 1);
            }

            /* Create provider combo-box: */
            m_pProviderComboBox = new QComboBox;
            if (m_pProviderComboBox)
            {
                /* Add into layout: */
                pSettingsLayout->addWidget(m_pProviderComboBox, 7, 1);
            }
            /* Create provider label: */
            m_pProviderComboBoxLabel = new QLabel;
            if (m_pProviderComboBoxLabel)
            {
                m_pProviderComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                m_pProviderComboBoxLabel->setBuddy(m_pProviderComboBox);

                /* Add into layout: */
                pSettingsLayout->addWidget(m_pProviderComboBoxLabel, 7, 0);
            }

            /* Add into layout: */
            pMainLayout->addLayout(pSettingsLayout);
        }

        /* Add vertical stretch: */
        pMainLayout->addStretch();
    }

    /* Populate providers: */
    populateProviders();
    /* Choose default settings: */
    chooseDefaultSettings();

    /* Setup connections: */
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageBasic3::completeChanged);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic3::sltHandleFormatComboChange);
    connect(m_pProviderComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic3::sltHandleProviderComboChange);

    /* Register fields: */
    registerField("path", this, "path");
    registerField("format", this, "format");
    registerField("manifestSelected", this, "manifestSelected");
}

void UIWizardExportAppPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Storage settings"));

    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");

    /* Translate File selector: */
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setChooseButtonToolTip(tr("Choose a file to export the virtual appliance to..."));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a file to export the virtual appliance to"));

    /* Translate Format combo-box: */
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

    /* Translate addtional stuff: */
    m_pAdditionalLabel->setText(UIWizardExportApp::tr("Additionally:"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("Write &Manifest file"));

    /* Translate Provider combo-box: */
    m_pProviderComboBoxLabel->setText(UIWizardExportApp::tr("&Cloud Service Provider:"));
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        if (m_pProviderComboBox->itemText(i) == "OCI")
        {
            m_pProviderComboBox->setItemText(i, UIWizardExportApp::tr("Oracle Cloud Infrastructure"));
            m_pProviderComboBox->setItemData(i, UIWizardExportApp::tr("Write to Oracle Cloud Infrastructure"), Qt::ToolTipRole);
        }
    }

    /* Refresh current settings: */
    refreshCurrentSettings();

    /* Update page appearance: */
    updatePageAppearance();

    /* Update tool-tips: */
    updateFormatComboToolTip();
    updateProviderComboToolTip();
}

void UIWizardExportAppPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardExportAppPageBasic3::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

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
            const StorageType enmStorageType = field("storageType").value<StorageType>();
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

void UIWizardExportAppPageBasic3::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardExportAppPage3::updatePageAppearance();

    /* Update page appearance according to chosen storage-type: */
    const StorageType enmStorageType = field("storageType").value<StorageType>();
    switch (enmStorageType)
    {
        case LocalFilesystem:
        {
            m_pLabel->setText(tr("<p>Please choose a filename to export the virtual appliance to.</p>"
                                 "<p>The <b>Open Virtualization Format</b> supports only "
                                 "<b>ovf</b> or <b>ova</b> extensions. "
                                 "<br>If you use the <b>ovf</b> extension, "
                                 "several files will be written separately."
                                 "<br>If you use the <b>ova</b> extension, "
                                 "all the files will be combined into one Open Virtualization Format archive.</p>"
                                 "<p>The <b>Oracle Public Cloud Format</b> supports only the <b>tar.gz</b> extension."
                                 "<br>Each virtual disk file will be written separately.</p>"));
            m_pFileSelector->setFocus();
            break;
        }
        case CloudProvider:
        {
            m_pLabel->setText(tr("Please complete the additional fields and provide a filename for "
                                 "the OVF target."));
            m_pProviderComboBox->setFocus();
            break;
        }
    }
}

void UIWizardExportAppPageBasic3::sltHandleFormatComboChange()
{
    refreshCurrentSettings();
    updateFormatComboToolTip();
}

void UIWizardExportAppPageBasic3::sltHandleProviderComboChange()
{
    refreshCurrentSettings();
    updateProviderComboToolTip();
}
