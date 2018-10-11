/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic2 class implementation.
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
# include "UIConverter.h"
# include "UIEmptyFilePathSelector.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageBasic2.h"

/* COM includes: */
# include "CCloudProvider.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage2::UIWizardExportAppPage2()
    : m_pFormatLayout(0)
    , m_pSettingsLayout1(0)
    , m_pSettingsLayout2(0)
    , m_pFormatComboBoxLabel(0)
    , m_pFormatComboBox(0)
    , m_pSettingsWidget(0)
    , m_pFileSelectorLabel(0)
    , m_pFileSelector(0)
    , m_pMACComboBoxLabel(0)
    , m_pMACComboBox(0)
    , m_pAdditionalLabel(0)
    , m_pManifestCheckbox(0)
    , m_pIncludeISOsCheckbox(0)
    , m_pAccountComboBoxLabel(0)
    , m_pAccountComboBox(0)
    , m_pAccountPropertyTable(0)
{
    /* Init Cloud Provider Manager: */
    CVirtualBox comVBox = vboxGlobal().virtualBox();
    m_comCloudProviderManager = comVBox.GetCloudProviderManager();
    AssertMsg(comVBox.isOk() && m_comCloudProviderManager.isNotNull(),
              ("Unable to acquire Cloud Provider Manager"));
}

void UIWizardExportAppPage2::populateFormats()
{
    AssertReturnVoid(m_pFormatComboBox->count() == 0);

    /* Compose hardcoded format list: */
    QStringList formats;
    formats << "ovf-0.9";
    formats << "ovf-1.0";
    formats << "ovf-2.0";
    formats << "opc-1.0";
    /* Add that list to combo: */
    foreach (const QString &strShortName, formats)
    {
        /* Compose empty item, fill it's data: */
        m_pFormatComboBox->addItem(QString());
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, strShortName, FormatData_ShortName);
    }

    /* Iterate through existing providers: */
    foreach (const CCloudProvider &comProvider, m_comCloudProviderManager.GetProviders())
    {
        /* Skip if we have nothing to populate (file missing?): */
        if (comProvider.isNull())
            continue;

        /* Compose empty item, fill it's data: */
        m_pFormatComboBox->addItem(QString());
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, comProvider.GetId(),        FormatData_ID);
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, comProvider.GetName(),      FormatData_Name);
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, comProvider.GetShortName(), FormatData_ShortName);
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, true,                       FormatData_IsItCloudFormat);
    }

    /* Set default: */
    setFormat("ovf-1.0");
}

void UIWizardExportAppPage2::populateMACAddressPolicies()
{
    AssertReturnVoid(m_pMACComboBox->count() == 0);

    /* Apply hardcoded policies list: */
    for (int i = 0; i < (int)MACAddressPolicy_MAX; ++i)
    {
        m_pMACComboBox->addItem(QString::number(i));
        m_pMACComboBox->setItemData(i, i);
    }

    /* Set default: */
    setMACAddressPolicy(MACAddressPolicy_StripAllNonNATMACs);
}

void UIWizardExportAppPage2::populateAccounts()
{
    /* Clear combo initially: */
    m_pAccountComboBox->clear();
    /* Return if no provider chosen: */
    if (providerId().isNull())
        return;

    /* Acquire Cloud Provider: */
    CCloudProvider comCloudProvider = m_comCloudProviderManager.GetProviderById(providerId());
    /* Return if the provider has disappeared: */
    if (comCloudProvider.isNull())
        return;

    /* Iterate through profile names: */
    foreach (const QString &strProfileName, comCloudProvider.GetProfileNames())
    {
        m_pAccountComboBox->addItem(strProfileName);
        m_pAccountComboBox->setItemData(m_pAccountComboBox->count() - 1, strProfileName, AccountData_ProfileName);
    }

    /* Set default: */
    if (m_pAccountComboBox->count() != 0)
        m_pAccountComboBox->setCurrentIndex(0);
}

void UIWizardExportAppPage2::populateAccountProperties()
{
    /* Clear table initially: */
    m_pAccountPropertyTable->clear();
    /* Return if no provider chosen: */
    if (providerId().isNull())
        return;

    /* Acquire Cloud Provider: */
    CCloudProvider comCloudProvider = m_comCloudProviderManager.GetProviderById(providerId());
    /* Return if the provider has disappeared: */
    if (comCloudProvider.isNull())
        return;

    /* Acquire Cloud Profile: */
    m_comCloudProfile = comCloudProvider.GetProfileByName(profileName());
    /* Return if the profile has disappeared: */
    if (m_comCloudProfile.isNull())
        return;

    /* Acquire properties: */
    QVector<QString> keys;
    QVector<QString> values;
    values = m_comCloudProfile.GetProperties(QString(), keys);

    /* Configure table: */
    m_pAccountPropertyTable->setRowCount(keys.size());
    m_pAccountPropertyTable->setColumnCount(2);

    /* Push acquired keys/values to data fields: */
    for (int i = 0; i < m_pAccountPropertyTable->rowCount(); ++i)
    {
        /* Create key item: */
        QTableWidgetItem *pItemK = new QTableWidgetItem(keys.at(i));
        if (pItemK)
        {
            /* Non-editable for sure, but non-selectable? */
            pItemK->setFlags(pItemK->flags() & ~Qt::ItemIsEditable);
            pItemK->setFlags(pItemK->flags() & ~Qt::ItemIsSelectable);

            /* Use non-translated description as tool-tip: */
            const QString strToolTip = m_comCloudProfile.GetPropertyDescription(keys.at(i));
            pItemK->setData(Qt::UserRole, strToolTip);

            /* Insert into table: */
            m_pAccountPropertyTable->setItem(i, 0, pItemK);
        }

        /* Create value item: */
        QTableWidgetItem *pItemV = new QTableWidgetItem(values.at(i));
        if (pItemV)
        {
            /* Non-editable for sure, but non-selectable? */
            pItemV->setFlags(pItemV->flags() & ~Qt::ItemIsEditable);
            pItemV->setFlags(pItemV->flags() & ~Qt::ItemIsSelectable);

            /* Use the value as tool-tip, there can be quite long values: */
            const QString strToolTip = values.at(i);
            pItemV->setToolTip(strToolTip);

            /* Insert into table: */
            m_pAccountPropertyTable->setItem(i, 1, pItemV);
        }
    }

    /* Update table tool-tips: */
    updateAccountPropertyTableToolTips();

    /* Adjust the table: */
    adjustAccountPropertyTable();
}

void UIWizardExportAppPage2::updatePageAppearance()
{
    /* Update page appearance according to chosen storage-type: */
    m_pSettingsWidget->setCurrentIndex((int)isFormatCloudOne());
}

void UIWizardExportAppPage2::refreshFileSelectorName()
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

void UIWizardExportAppPage2::refreshFileSelectorExtension()
{
    /* If format is cloud one: */
    if (isFormatCloudOne())
    {
        /* We use no extension: */
        m_strFileSelectorExt.clear();

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(QString());
    }
    /* Else if the format is set to OPC: */
    else if (fieldImp("format").toString() == "opc-1.0")
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

void UIWizardExportAppPage2::refreshFileSelectorPath()
{
    /* Compose file selector path: */
    const QString strPath = QDir::toNativeSeparators(QString("%1/%2")
                                                     .arg(vboxGlobal().documentsPath())
                                                     .arg(m_strFileSelectorName + m_strFileSelectorExt));

    /* Assign the path: */
    m_pFileSelector->setPath(strPath);
}

void UIWizardExportAppPage2::refreshManifestCheckBoxAccess()
{
    /* If format is cloud one or OPC: */
    if (   isFormatCloudOne()
        || fieldImp("format").toString() == "opc-1.0")
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

void UIWizardExportAppPage2::refreshIncludeISOsCheckBoxAccess()
{
    /* If format is cloud one or OPC: */
    if (   isFormatCloudOne()
        || fieldImp("format").toString() == "opc-1.0")
    {
        /* Disable include ISO check-box: */
        m_pIncludeISOsCheckbox->setChecked(false);
        m_pIncludeISOsCheckbox->setEnabled(false);
    }
    /* Otherwise: */
    else
    {
        /* Enable include ISO check-box: */
        m_pIncludeISOsCheckbox->setEnabled(true);
    }
}

void UIWizardExportAppPage2::updateFormatComboToolTip()
{
    const int iCurrentIndex = m_pFormatComboBox->currentIndex();
    const QString strCurrentToolTip = m_pFormatComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pFormatComboBox->setToolTip(strCurrentToolTip);
}

void UIWizardExportAppPage2::updateMACAddressPolicyComboToolTip()
{
    const int iCurrentIndex = m_pMACComboBox->currentIndex();
    const QString strCurrentToolTip = m_pMACComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pMACComboBox->setToolTip(strCurrentToolTip);
}

void UIWizardExportAppPage2::updateAccountPropertyTableToolTips()
{
    /* Iterate through all the key items: */
    for (int i = 0; i < m_pAccountPropertyTable->rowCount(); ++i)
    {
        /* Acquire current key item: */
        QTableWidgetItem *pItemK = m_pAccountPropertyTable->item(i, 0);
        if (pItemK)
        {
            const QString strToolTip = pItemK->data(Qt::UserRole).toString();
            pItemK->setToolTip(QApplication::translate("UIWizardExportAppPageBasic2", strToolTip.toUtf8().constData()));
        }
    }
}

void UIWizardExportAppPage2::adjustAccountPropertyTable()
{
    /* Disable last column stretching temporary: */
    m_pAccountPropertyTable->horizontalHeader()->setStretchLastSection(false);

    /* Resize both columns to contents: */
    m_pAccountPropertyTable->resizeColumnsToContents();
    /* Then acquire full available width: */
    const int iFullWidth = m_pAccountPropertyTable->viewport()->width();
    /* First column should not be less than it's minimum size, last gets the rest: */
    const int iMinimumWidth0 = qMin(m_pAccountPropertyTable->horizontalHeader()->sectionSize(0), iFullWidth / 2);
    m_pAccountPropertyTable->horizontalHeader()->resizeSection(0, iMinimumWidth0);

    /* Enable last column stretching again: */
    m_pAccountPropertyTable->horizontalHeader()->setStretchLastSection(true);
}

void UIWizardExportAppPage2::setFormat(const QString &strFormat)
{
    const int iIndex = m_pFormatComboBox->findData(strFormat, FormatData_ShortName);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pFormatComboBox->setCurrentIndex(iIndex);
}

QString UIWizardExportAppPage2::format() const
{
    const int iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex, FormatData_ShortName).toString();
}

bool UIWizardExportAppPage2::isFormatCloudOne(int iIndex /* = -1 */) const
{
    if (iIndex == -1)
        iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex, FormatData_IsItCloudFormat).toBool();
}

void UIWizardExportAppPage2::setPath(const QString &strPath)
{
    m_pFileSelector->setPath(strPath);
}

QString UIWizardExportAppPage2::path() const
{
    return m_pFileSelector->path();
}

void UIWizardExportAppPage2::setMACAddressPolicy(MACAddressPolicy enmMACAddressPolicy)
{
    const int iIndex = m_pMACComboBox->findData((int)enmMACAddressPolicy);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pMACComboBox->setCurrentIndex(iIndex);
}

MACAddressPolicy UIWizardExportAppPage2::macAddressPolicy() const
{
    const int iIndex = m_pMACComboBox->currentIndex();
    return (MACAddressPolicy)m_pMACComboBox->itemData(iIndex).toInt();
}

void UIWizardExportAppPage2::setManifestSelected(bool fChecked)
{
    m_pManifestCheckbox->setChecked(fChecked);
}

bool UIWizardExportAppPage2::isManifestSelected() const
{
    return m_pManifestCheckbox->isChecked();
}

void UIWizardExportAppPage2::setIncludeISOsSelected(bool fChecked)
{
    m_pIncludeISOsCheckbox->setChecked(fChecked);
}

bool UIWizardExportAppPage2::isIncludeISOsSelected() const
{
    return m_pIncludeISOsCheckbox->isChecked();
}

void UIWizardExportAppPage2::setProviderById(const QString &strId)
{
    const int iIndex = m_pFormatComboBox->findData(strId, FormatData_ID);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pFormatComboBox->setCurrentIndex(iIndex);
}

QString UIWizardExportAppPage2::providerId() const
{
    const int iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex, FormatData_ID).toString();
}

QString UIWizardExportAppPage2::providerShortName() const
{
    const int iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex, FormatData_ShortName).toString();
}

QString UIWizardExportAppPage2::profileName() const
{
    const int iIndex = m_pAccountComboBox->currentIndex();
    return m_pAccountComboBox->itemData(iIndex, AccountData_ProfileName).toString();
}

CCloudProfile UIWizardExportAppPage2::profile() const
{
    return m_comCloudProfile;
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic2 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic2::UIWizardExportAppPageBasic2()
    : m_pLabelFormat(0)
    , m_pLabelSettings(0)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create format label: */
        m_pLabelFormat = new QIRichTextLabel;
        if (m_pLabelFormat)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelFormat);
        }

        /* Create format layout: */
        m_pFormatLayout = new QGridLayout;
        if (m_pFormatLayout)
        {
#ifdef VBOX_WS_MAC
            m_pFormatLayout->setContentsMargins(0, 10, 0, 10);
            m_pFormatLayout->setSpacing(10);
#else
            m_pFormatLayout->setContentsMargins(0, qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                                0, qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
#endif
            m_pFormatLayout->setColumnStretch(0, 0);
            m_pFormatLayout->setColumnStretch(1, 1);

            /* Create format combo-box: */
            m_pFormatComboBox = new QComboBox;
            if (m_pFormatComboBox)
            {
                /* Add into layout: */
                m_pFormatLayout->addWidget(m_pFormatComboBox, 0, 1);
            }
            /* Create format combo-box label: */
            m_pFormatComboBoxLabel = new QLabel;
            if (m_pFormatComboBoxLabel)
            {
                m_pFormatComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                m_pFormatComboBoxLabel->setBuddy(m_pFormatComboBox);

                /* Add into layout: */
                m_pFormatLayout->addWidget(m_pFormatComboBoxLabel, 0, 0);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pFormatLayout);
        }

        /* Create settings label: */
        m_pLabelSettings = new QIRichTextLabel;
        if (m_pLabelSettings)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelSettings);
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
                m_pSettingsLayout1 = new QGridLayout(pSettingsPane1);
                if (m_pSettingsLayout1)
                {
#ifdef VBOX_WS_MAC
                    m_pSettingsLayout1->setContentsMargins(0, 10, 0, 10);
                    m_pSettingsLayout1->setSpacing(10);
#else
                    m_pSettingsLayout1->setContentsMargins(0, qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                                           0, qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
#endif
                    m_pSettingsLayout1->setColumnStretch(0, 0);
                    m_pSettingsLayout1->setColumnStretch(1, 1);

                    /* Create file selector: */
                    m_pFileSelector = new UIEmptyFilePathSelector;
                    if (m_pFileSelector)
                    {
                        m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Save);
                        m_pFileSelector->setEditable(true);
                        m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                        m_pFileSelector->setDefaultSaveExt("ova");

                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pFileSelector, 0, 1, 1, 2);
                    }
                    /* Create file selector label: */
                    m_pFileSelectorLabel = new QLabel;
                    if (m_pFileSelectorLabel)
                    {
                        m_pFileSelectorLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                        m_pFileSelectorLabel->setBuddy(m_pFileSelector);

                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pFileSelectorLabel, 0, 0);
                    }

                    /* Create MAC policy combo-box: */
                    m_pMACComboBox = new QComboBox;
                    if (m_pMACComboBox)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pMACComboBox, 1, 1, 1, 2);
                    }
                    /* Create format combo-box label: */
                    m_pMACComboBoxLabel = new QLabel;
                    if (m_pMACComboBoxLabel)
                    {
                        m_pMACComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                        m_pMACComboBoxLabel->setBuddy(m_pMACComboBox);

                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pMACComboBoxLabel, 1, 0);
                    }

                    /* Create advanced label: */
                    m_pAdditionalLabel = new QLabel;
                    if (m_pAdditionalLabel)
                    {
                        m_pAdditionalLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pAdditionalLabel, 2, 0);
                    }
                    /* Create manifest check-box: */
                    m_pManifestCheckbox = new QCheckBox;
                    if (m_pManifestCheckbox)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pManifestCheckbox, 2, 1);
                    }
                    /* Create include ISOs check-box: */
                    m_pIncludeISOsCheckbox = new QCheckBox;
                    if (m_pIncludeISOsCheckbox)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(m_pIncludeISOsCheckbox, 3, 1);
                    }

                    /* Create placeholder: */
                    QWidget *pPlaceholder = new QWidget;
                    if (pPlaceholder)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout1->addWidget(pPlaceholder, 4, 0, 1, 3);
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
                m_pSettingsLayout2 = new QGridLayout(pSettingsPane2);
                if (m_pSettingsLayout2)
                {
#ifdef VBOX_WS_MAC
                    m_pSettingsLayout2->setContentsMargins(0, 10, 0, 10);
                    m_pSettingsLayout2->setSpacing(10);

#else
                    m_pSettingsLayout2->setContentsMargins(0, qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                                           0, qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
#endif
                    m_pSettingsLayout2->setColumnStretch(0, 0);
                    m_pSettingsLayout2->setColumnStretch(1, 1);

                    /* Create provider combo-box: */
                    m_pAccountComboBox = new QComboBox;
                    if (m_pAccountComboBox)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pAccountComboBox, 0, 1);
                    }
                    /* Create provider label: */
                    m_pAccountComboBoxLabel = new QLabel;
                    if (m_pAccountComboBoxLabel)
                    {
                        m_pAccountComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                        m_pAccountComboBoxLabel->setBuddy(m_pAccountComboBox);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pAccountComboBoxLabel, 0, 0);
                    }
                    /* Create profile property table: */
                    m_pAccountPropertyTable = new QTableWidget;
                    if (m_pAccountPropertyTable)
                    {
                        const QFontMetrics fm(m_pAccountPropertyTable->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pAccountPropertyTable->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
//                        m_pAccountPropertyTable->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pAccountPropertyTable->setAlternatingRowColors(true);
                        m_pAccountPropertyTable->horizontalHeader()->setVisible(false);
                        m_pAccountPropertyTable->verticalHeader()->setVisible(false);
                        m_pAccountPropertyTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pAccountPropertyTable, 1, 1);
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
    /* Populate MAC address policies: */
    populateMACAddressPolicies();
    /* Populate providers: */
    populateAccounts();
    /* Populate profile properties: */
    populateAccountProperties();

    /* Setup connections: */
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageBasic2::completeChanged);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleFormatComboChange);
    connect(m_pMACComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleMACAddressPolicyComboChange);
    connect(m_pAccountComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleAccountComboChange);

    /* Register fields: */
    registerField("format", this, "format");
    registerField("isFormatCloudOne", this, "isFormatCloudOne");
    registerField("path", this, "path");
    registerField("macAddressPolicy", this, "macAddressPolicy");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("includeISOsSelected", this, "includeISOsSelected");
    registerField("providerShortName", this, "providerShortName");
    registerField("profileName", this, "profileName");
    registerField("profile", this, "profile");
}

bool UIWizardExportAppPageBasic2::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::Show:
        case QEvent::Resize:
        {
            /* Adjust profile property table: */
            adjustAccountPropertyTable();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return UIWizardPage::event(pEvent);
}

void UIWizardExportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Appliance settings"));

    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");

    /* Translate format label: */
    m_pLabelFormat->setText(UIWizardExportApp::
                            tr("<p>Please choose a format to export the virtual appliance to.</p>"
                               "<p>The <b>Open Virtualization Format</b> supports only <b>ovf</b> or <b>ova</b> extensions. "
                               "If you use the <b>ovf</b> extension, several files will be written separately. "
                               "If you use the <b>ova</b> extension, all the files will be combined into one Open "
                               "Virtualization Format archive.</p>"
                               "<p>The <b>Oracle Public Cloud Format</b> supports only the <b>tar.gz</b> extension. "
                               "Each virtual disk file will be written separately.</p>"
                               "<p>The <b>Cloud Service Provider Format</b> supports exporting to remote cloud services only. "
                               "Main virtual disk of each selected machine will be uploaded to remote server.</p>"));

    /* Translate file selector: */
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setChooseButtonToolTip(UIWizardExportApp::tr("Choose a file to export the virtual appliance to..."));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a file to export the virtual appliance to"));

    /* Translate hardcoded values of Format combo-box: */
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
    /* Translate received values of Format combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pFormatComboBox->count(); ++i)
        if (isFormatCloudOne(i))
        {
            m_pFormatComboBox->setItemText(i, m_pFormatComboBox->itemData(i, FormatData_Name).toString());
            m_pFormatComboBox->setItemData(i, UIWizardExportApp::tr("Export to cloud service provider."), Qt::ToolTipRole);
        }

    /* Translate MAC address policy combo-box: */
    m_pMACComboBoxLabel->setText(UIWizardExportApp::tr("MAC Address &Policy:"));
    m_pMACComboBox->setItemText(MACAddressPolicy_KeepAllMACs,
                                UIWizardExportApp::tr("Include all network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressPolicy_StripAllNonNATMACs,
                                UIWizardExportApp::tr("Include only NAT network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressPolicy_StripAllMACs,
                                UIWizardExportApp::tr("Strip all network adapter MAC addresses"));
    m_pMACComboBox->setItemData(MACAddressPolicy_KeepAllMACs,
                                UIWizardExportApp::tr("Include all network adapter MAC addresses in exported "
                                                      "appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllNonNATMACs,
                                UIWizardExportApp::tr("Include only NAT network adapter MAC addresses in "
                                                      "exported appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllMACs,
                                UIWizardExportApp::tr("Strip all network adapter MAC addresses from exported "
                                                      "appliance archive."), Qt::ToolTipRole);

    /* Translate addtional stuff: */
    m_pAdditionalLabel->setText(UIWizardExportApp::tr("Additionally:"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("&Write Manifest file"));
    m_pIncludeISOsCheckbox->setToolTip(UIWizardExportApp::tr("Include ISO image files in exported VM archive."));
    m_pIncludeISOsCheckbox->setText(UIWizardExportApp::tr("&Include ISO image files"));

    /* Translate Account combo-box: */
    m_pAccountComboBoxLabel->setText(UIWizardExportApp::tr("&Account:"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pFormatComboBoxLabel;
    labels << m_pFileSelectorLabel;
    labels << m_pMACComboBoxLabel;
    labels << m_pAdditionalLabel;
    labels << m_pAccountComboBoxLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pFormatLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pSettingsLayout1->setColumnMinimumWidth(0, iMaxWidth);
    m_pSettingsLayout2->setColumnMinimumWidth(0, iMaxWidth);

    /* Refresh file selector name: */
    refreshFileSelectorName();

    /* Update page appearance: */
    updatePageAppearance();

    /* Update tool-tips: */
    updateFormatComboToolTip();
    updateMACAddressPolicyComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardExportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Refresh file selector name: */
    // refreshFileSelectorName(); alreeady called from retranslateUi();
    /* Refresh file selector extension: */
    refreshFileSelectorExtension();
    /* Refresh manifest check-box access: */
    refreshManifestCheckBoxAccess();
    /* Refresh include ISOs check-box access: */
    refreshIncludeISOsCheckBoxAccess();
}

bool UIWizardExportAppPageBasic2::isComplete() const
{
    bool fResult = true;

    /* Check appliance settings: */
    if (fResult)
    {
        const QString &strFile = field("path").toString().toLower();

        const bool fOVF =    field("format").toString() == "ovf-0.9"
                          || field("format").toString() == "ovf-1.0"
                          || field("format").toString() == "ovf-2.0";
        const bool fOPC =    field("format").toString() == "opc-1.0";
        const bool fCSP =    isFormatCloudOne();

        fResult =    (   fOVF
                      && VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
                  || (   fOPC
                      && VBoxGlobal::hasAllowedExtension(strFile, OPCFileExts))
                  || fCSP;
    }

    return fResult;
}

void UIWizardExportAppPageBasic2::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardExportAppPage2::updatePageAppearance();

    /* Update page appearance according to chosen storage-type: */
    if (isFormatCloudOne())
    {
        m_pLabelSettings->setText(UIWizardExportApp::
                                  tr("<p>Please choose one of cloud service accounts you have registered to export virtual "
                                     "machines to.  Make sure profile settings reflected in the underlying table are valid.  "
                                     "They will be used to establish network connection required to upload your virtual machine "
                                     "files to a remote cloud facility."));
        m_pAccountComboBox->setFocus();
    }
    else
    {
        m_pLabelSettings->setText(UIWizardExportApp::
                                  tr("<p>Please choose a filename to export the virtual appliance to. Besides that you can "
                                     "specify a certain amount of options which affects the size and content of resulting "
                                     "archive.</p>"));
        m_pFileSelector->setFocus();
    }
}

void UIWizardExportAppPageBasic2::sltHandleFormatComboChange()
{
    /* Update tool-tip: */
    updateFormatComboToolTip();

    /* Refresh required settings: */
    updatePageAppearance();
    refreshFileSelectorExtension();
    refreshManifestCheckBoxAccess();
    refreshIncludeISOsCheckBoxAccess();
    populateAccounts();
    populateAccountProperties();
}

void UIWizardExportAppPageBasic2::sltHandleMACAddressPolicyComboChange()
{
    /* Update tool-tip: */
    updateMACAddressPolicyComboToolTip();
}

void UIWizardExportAppPageBasic2::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
}
