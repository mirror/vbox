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
# include <QHeaderView>
# include <QLabel>
# include <QLineEdit>
# include <QStackedWidget>
# include <QTableWidget>
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
    : m_pSettingsWidget(0)
    , m_pFileSelectorLabel(0)
    , m_pFileSelector(0)
    , m_pFormatComboBoxLabel(0)
    , m_pFormatComboBox(0)
    , m_pAdditionalLabel(0)
    , m_pManifestCheckbox(0)
    , m_pProviderComboBoxLabel(0)
    , m_pProviderComboBox(0)
    , m_pProfileComboBoxLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileSettingsTable(0)
{
}

void UIWizardExportAppPage3::populateFormats()
{
    AssertReturnVoid(m_pFormatComboBox->count() == 0);

    /* Apply hardcoded format list: */
    QStringList formats;
    formats << "ovf-0.9";
    formats << "ovf-1.0";
    formats << "ovf-2.0";
    formats << "opc-1.0";
    m_pFormatComboBox->addItems(formats);

    /* Duplicate non-translated names to data fields: */
    for (int i = 0; i < m_pFormatComboBox->count(); ++i)
        m_pFormatComboBox->setItemData(i, m_pFormatComboBox->itemText(i));

    /* Set default: */
    setFormat("ovf-1.0");
}

void UIWizardExportAppPage3::populateProviders()
{
    AssertReturnVoid(m_pProviderComboBox->count() == 0);

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

    /* Set default: */
    setProvider("OCI");
}

void UIWizardExportAppPage3::populateProfiles()
{
    /* Acquire profile list: */
    // Here goes the experiamental list with
    // arbitrary contents for testing purposes.
    QStringList profiles;
    profiles << "Profile 1";
    profiles << "Profile 2";
    profiles << "Profile 3";
    profiles << "Profile 4";
    m_pProfileComboBox->clear();
    m_pProfileComboBox->addItems(profiles);

    /* Duplicate non-translated names to data fields: */
    for (int i = 0; i < m_pProfileComboBox->count(); ++i)
        m_pProfileComboBox->setItemData(i, m_pProfileComboBox->itemText(i));
}

void UIWizardExportAppPage3::populateProfileSettings()
{
    /* Acquire current profile table: */
    // Here goes the experiamental lists with
    // arbitrary contents for testing purposes.
    QStringList keys;
    QStringList values;
    keys << "Key 1";
    keys << "Key 2";
    keys << "Key 3";
    keys << "Key 4";
    values << "Value 1";
    values << "Value 2";
    values << "Value 3";
    values << "Value 4";
    m_pProfileSettingsTable->clear();
    m_pProfileSettingsTable->setRowCount(4);
    m_pProfileSettingsTable->setColumnCount(2);

    /* Push acquired keys/values to data fields: */
    for (int i = 0; i < m_pProfileSettingsTable->rowCount(); ++i)
    {
        QTableWidgetItem *pItem1 = new QTableWidgetItem(keys.at(i));
        pItem1->setFlags(pItem1->flags() & ~Qt::ItemIsEditable);
        QTableWidgetItem *pItem2 = new QTableWidgetItem(values.at(i));
        pItem2->setFlags(pItem2->flags() & ~Qt::ItemIsEditable);
        m_pProfileSettingsTable->setItem(i, 0, pItem1);
        m_pProfileSettingsTable->setItem(i, 1, pItem2);
    }

    /* Adjust the table: */
    adjustProfileSettingsTable();
}

void UIWizardExportAppPage3::updatePageAppearance()
{
    /* Update page appearance according to chosen storage-type: */
    const StorageType enmStorageType = fieldImp("storageType").value<StorageType>();
    m_pSettingsWidget->setCurrentIndex((int)enmStorageType);
}

void UIWizardExportAppPage3::refreshFileSelectorName()
{
    /* If it's one VM only, we use the VM name as file-name: */
    if (fieldImp("machineNames").toStringList().size() == 1)
        m_strFileSelectorName = fieldImp("machineNames").toStringList()[0];
    /* Otherwise => we use the default file-name: */
    else
        m_strFileSelectorName = m_strDefaultApplianceName;

    /* Cascade update for file selector path: */
    refreshFileSelectorPath();
}

void UIWizardExportAppPage3::refreshFileSelectorExtension()
{
    /* If the format is set to OPC: */
    if (fieldImp("format").toString() == "opc-1.0")
    {
        /* We use .tar.gz extension: */
        m_strFileSelectorExt = ".tar.gz";

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(UIWizardExportApp::tr("Oracle Public Cloud Format Archive (%1)").arg("*.tar.gz"));
    }
    /* Otherwise: */
    else
    {
        /* We use the default (.ova) extension: */
        m_strFileSelectorExt = ".ova";

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(UIWizardExportApp::tr("Open Virtualization Format Archive (%1)").arg("*.ova") + ";;" +
                                        UIWizardExportApp::tr("Open Virtualization Format (%1)").arg("*.ovf"));
    }

    /* Cascade update for file selector path: */
    refreshFileSelectorPath();
}

void UIWizardExportAppPage3::refreshFileSelectorPath()
{
    /* Compose file selector path: */
    const QString strPath = QDir::toNativeSeparators(QString("%1/%2")
                                                     .arg(vboxGlobal().documentsPath())
                                                     .arg(m_strFileSelectorName + m_strFileSelectorExt));

    /* Assign the path: */
    m_pFileSelector->setPath(strPath);
}

void UIWizardExportAppPage3::refreshManifestCheckBoxAccess()
{
    /* If the format is set to OPC: */
    if (fieldImp("format").toString() == "opc-1.0")
    {
        /* Disable manifest check-box: */
        m_pManifestCheckbox->setChecked(false);
        m_pManifestCheckbox->setEnabled(false);
    }
    /* Otherwise: */
    else
    {
        /* Enable manifest check-box: */
        m_pManifestCheckbox->setChecked(true);
        m_pManifestCheckbox->setEnabled(true);
    }
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

void UIWizardExportAppPage3::adjustProfileSettingsTable()
{
    /* Disable last column stretching temporary: */
    m_pProfileSettingsTable->horizontalHeader()->setStretchLastSection(false);

    /* Resize both columns to contents: */
    m_pProfileSettingsTable->resizeColumnsToContents();
    /* Then acquire full available width: */
    const int iFullWidth = m_pProfileSettingsTable->viewport()->width();
    /* First column should not be less than it's minimum size, last gets the rest: */
    const int iMinimumWidth0 = qMin(m_pProfileSettingsTable->horizontalHeader()->sectionSize(0), iFullWidth / 2);
    m_pProfileSettingsTable->horizontalHeader()->resizeSection(0, iMinimumWidth0);

    /* Enable last column stretching again: */
    m_pProfileSettingsTable->horizontalHeader()->setStretchLastSection(true);
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

QString UIWizardExportAppPage3::profile() const
{
    const int iIndex = m_pProfileComboBox->currentIndex();
    return m_pProfileComboBox->itemData(iIndex).toString();
}

void UIWizardExportAppPage3::setProfile(const QString &strProfile)
{
    const int iIndex = m_pProfileComboBox->findData(strProfile);
    AssertMsg(iIndex != -1, ("Field not found!"));
    m_pProfileComboBox->setCurrentIndex(iIndex);
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
        m_pSettingsWidget = new QStackedWidget;
        if (m_pSettingsWidget)
        {
            /* Create settings pane 1: */
            QWidget *pSettingsPane1 = new QWidget;
            if (pSettingsPane1)
            {
                /* Create settings layout 1: */
                QGridLayout *pSettingsLayout1 = new QGridLayout(pSettingsPane1);
                if (pSettingsLayout1)
                {
                    pSettingsLayout1->setContentsMargins(0, 0, 0, 0);
                    pSettingsLayout1->setColumnStretch(0, 0);
                    pSettingsLayout1->setColumnStretch(1, 1);

                    /* Create file selector: */
                    m_pFileSelector = new UIEmptyFilePathSelector;
                    if (m_pFileSelector)
                    {
                        m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Save);
                        m_pFileSelector->setEditable(true);
                        m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                        m_pFileSelector->setDefaultSaveExt("ova");

                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pFileSelector, 0, 1);
                    }
                    /* Create file selector label: */
                    m_pFileSelectorLabel = new QLabel;
                    if (m_pFileSelectorLabel)
                    {
                        m_pFileSelectorLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                        m_pFileSelectorLabel->setBuddy(m_pFileSelector);

                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pFileSelectorLabel, 0, 0);
                    }

                    /* Create format combo-box: */
                    m_pFormatComboBox = new QComboBox;
                    if (m_pFormatComboBox)
                    {
                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pFormatComboBox, 1, 1);
                    }
                    /* Create format combo-box label: */
                    m_pFormatComboBoxLabel = new QLabel;
                    if (m_pFormatComboBoxLabel)
                    {
                        m_pFormatComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                        m_pFormatComboBoxLabel->setBuddy(m_pFormatComboBox);

                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pFormatComboBoxLabel, 1, 0);
                    }

                    /* Create advanced label: */
                    m_pAdditionalLabel = new QLabel;
                    if (m_pAdditionalLabel)
                    {
                        m_pAdditionalLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pAdditionalLabel, 2, 0);
                    }
                    /* Create manifest check-box: */
                    m_pManifestCheckbox = new QCheckBox;
                    if (m_pManifestCheckbox)
                    {
                        /* Add into layout: */
                        pSettingsLayout1->addWidget(m_pManifestCheckbox, 2, 1);
                    }

                    /* Create placeholder: */
                    QWidget *pPlaceholder = new QWidget;
                    if (pPlaceholder)
                    {
                        /* Add into layout: */
                        pSettingsLayout1->addWidget(pPlaceholder, 3, 0, 1, 2);
                    }
                }

                /* Add into layout: */
                m_pSettingsWidget->addWidget(pSettingsPane1);
            }

            /* Create settings pane 2: */
            QWidget *pSettingsPane2 = new QWidget;;
            if (pSettingsPane2)
            {
                /* Create settings layout 2: */
                QGridLayout *pSettingsLayout2 = new QGridLayout(pSettingsPane2);
                if (pSettingsLayout2)
                {
                    pSettingsLayout2->setContentsMargins(0, 0, 0, 0);
                    pSettingsLayout2->setColumnStretch(0, 0);
                    pSettingsLayout2->setColumnStretch(1, 1);

                    /* Create provider combo-box: */
                    m_pProviderComboBox = new QComboBox;
                    if (m_pProviderComboBox)
                    {
                        /* Add into layout: */
                        pSettingsLayout2->addWidget(m_pProviderComboBox, 0, 1);
                    }
                    /* Create provider label: */
                    m_pProviderComboBoxLabel = new QLabel;
                    if (m_pProviderComboBoxLabel)
                    {
                        m_pProviderComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                        m_pProviderComboBoxLabel->setBuddy(m_pProviderComboBox);

                        /* Add into layout: */
                        pSettingsLayout2->addWidget(m_pProviderComboBoxLabel, 0, 0);
                    }

                    /* Create profile combo-box: */
                    m_pProfileComboBox = new QComboBox;
                    if (m_pProfileComboBox)
                    {
                        /* Add into layout: */
                        pSettingsLayout2->addWidget(m_pProfileComboBox, 1, 1);
                    }
                    /* Create profile label: */
                    m_pProfileComboBoxLabel = new QLabel;
                    if (m_pProfileComboBoxLabel)
                    {
                        m_pProfileComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                        m_pProfileComboBoxLabel->setBuddy(m_pProfileComboBox);

                        /* Add into layout: */
                        pSettingsLayout2->addWidget(m_pProfileComboBoxLabel, 1, 0);
                    }
                    /* Create profile settings table: */
                    m_pProfileSettingsTable = new QTableWidget;
                    if (m_pProfileSettingsTable)
                    {
                        const QFontMetrics fm(m_pProfileSettingsTable->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pProfileSettingsTable->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        m_pProfileSettingsTable->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pProfileSettingsTable->setAlternatingRowColors(true);
                        m_pProfileSettingsTable->horizontalHeader()->setVisible(false);
                        m_pProfileSettingsTable->verticalHeader()->setVisible(false);
                        m_pProfileSettingsTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

                        /* Add into layout: */
                        pSettingsLayout2->addWidget(m_pProfileSettingsTable, 2, 1);
                    }
                }

                /* Add into layout: */
                m_pSettingsWidget->addWidget(pSettingsPane2);
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsWidget);
        }
    }

    /* Populate formats: */
    populateFormats();
    /* Populate providers: */
    populateProviders();
    /* Populate profiles: */
    populateProfiles();
    /* Populate profile settings: */
    populateProfileSettings();

    /* Setup connections: */
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageBasic3::completeChanged);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic3::sltHandleFormatComboChange);
    connect(m_pProviderComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic3::sltHandleProviderComboChange);
    connect(m_pProfileComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic3::sltHandleProfileComboChange);

    /* Register fields: */
    registerField("path", this, "path");
    registerField("format", this, "format");
    registerField("manifestSelected", this, "manifestSelected");
}

bool UIWizardExportAppPageBasic3::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::Show:
        case QEvent::Resize:
        {
            /* Adjust profile settings table: */
            adjustProfileSettingsTable();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return UIWizardPage::event(pEvent);
}

void UIWizardExportAppPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Storage settings"));

    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");

    /* Translate file selector: */
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
    m_pProviderComboBoxLabel->setText(UIWizardExportApp::tr("&Provider:"));
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        if (m_pProviderComboBox->itemText(i) == "OCI")
        {
            m_pProviderComboBox->setItemText(i, UIWizardExportApp::tr("Oracle Cloud Infrastructure"));
            m_pProviderComboBox->setItemData(i, UIWizardExportApp::tr("Write to Oracle Cloud Infrastructure"), Qt::ToolTipRole);
        }
        else
        {
            m_pProviderComboBox->setItemData(i, UIWizardExportApp::tr("Write to %1").arg(m_pProviderComboBox->itemText(i)), Qt::ToolTipRole);
        }
    }

    /* Translate Profile combo-box: */
    m_pProfileComboBoxLabel->setText(UIWizardExportApp::tr("P&rofile:"));

    /* Refresh file selector name: */
    refreshFileSelectorName();

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

    /* Refresh file selector name: */
    // refreshFileSelectorName(); alreeady called from retranslateUi();
    /* Refresh file selector extension: */
    refreshFileSelectorExtension();
    /* Refresh manifest check-box access: */
    refreshManifestCheckBoxAccess();
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
    /* Update tool-tip: */
    updateFormatComboToolTip();

    /* Refresh required settings: */
    refreshFileSelectorExtension();
    refreshManifestCheckBoxAccess();
}

void UIWizardExportAppPageBasic3::sltHandleProviderComboChange()
{
    /* Update tool-tip: */
    updateProviderComboToolTip();

    /* Refresh required settings: */
    populateProfiles();
}

void UIWizardExportAppPageBasic3::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfileSettings();
}
