/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIEmptyFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppDefs.h"
#include "UIWizardExportAppPageBasic2.h"

/* COM includes: */
#include "CCloudClient.h"
#include "CMachine.h"


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage2::UIWizardExportAppPage2(bool fExportToOCIByDefault)
    : m_fExportToOCIByDefault(fExportToOCIByDefault)
    , m_pFormatLayout(0)
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
    , m_pAccountLabel(0)
    , m_pAccountComboBox(0)
    , m_pAccountToolButton(0)
    , m_pAccountPropertyTable(0)
{
}

void UIWizardExportAppPage2::populateFormats()
{
    AssertReturnVoid(m_pFormatComboBox->count() == 0);

    /* Compose hardcoded format list: */
    QStringList formats;
    formats << "ovf-0.9";
    formats << "ovf-1.0";
    formats << "ovf-2.0";
    /* Add that list to combo: */
    foreach (const QString &strShortName, formats)
    {
        /* Compose empty item, fill it's data: */
        m_pFormatComboBox->addItem(QString());
        m_pFormatComboBox->setItemData(m_pFormatComboBox->count() - 1, strShortName, FormatData_ShortName);
    }

    /* Initialize Cloud Provider Manager: */
    bool fOCIPresent = false;
    CVirtualBox comVBox = uiCommon().virtualBox();
    m_comCloudProviderManager = comVBox.GetCloudProviderManager();
    /* Show error message if necessary: */
    if (!comVBox.isOk())
        msgCenter().cannotAcquireCloudProviderManager(comVBox);
    else
    {
        /* Acquire existing providers: */
        const QVector<CCloudProvider> providers = m_comCloudProviderManager.GetProviders();
        /* Show error message if necessary: */
        if (!m_comCloudProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(m_comCloudProviderManager);
        else
        {
            /* Iterate through existing providers: */
            foreach (const CCloudProvider &comProvider, providers)
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
                if (m_pFormatComboBox->itemData(m_pFormatComboBox->count() - 1, FormatData_ShortName).toString() == "OCI")
                    fOCIPresent = true;
            }
        }
    }

    /* Set default: */
    if (m_fExportToOCIByDefault && fOCIPresent)
        setFormat("OCI");
    else
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
    /* Block signals while updating: */
    m_pAccountComboBox->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (m_pAccountComboBox->currentIndex() != -1)
        strOldData = m_pAccountComboBox->itemData(m_pAccountComboBox->currentIndex(), AccountData_ProfileName).toString();

    /* Clear combo initially: */
    m_pAccountComboBox->clear();
    /* Clear Cloud Provider: */
    m_comCloudProvider = CCloudProvider();

    /* If provider chosen: */
    if (!providerId().isNull())
    {
        /* (Re)initialize Cloud Provider: */
        m_comCloudProvider = m_comCloudProviderManager.GetProviderById(providerId());
        /* Show error message if necessary: */
        if (!m_comCloudProviderManager.isOk())
            msgCenter().cannotFindCloudProvider(m_comCloudProviderManager, providerId());
        else
        {
            /* Acquire existing profile names: */
            const QVector<QString> profileNames = m_comCloudProvider.GetProfileNames();
            /* Show error message if necessary: */
            if (!m_comCloudProvider.isOk())
                msgCenter().cannotAcquireCloudProviderParameter(m_comCloudProvider);
            else
            {
                /* Iterate through existing profile names: */
                foreach (const QString &strProfileName, profileNames)
                {
                    /* Skip if we have nothing to show (wtf happened?): */
                    if (strProfileName.isEmpty())
                        continue;

                    /* Compose item, fill it's data: */
                    m_pAccountComboBox->addItem(strProfileName);
                    m_pAccountComboBox->setItemData(m_pAccountComboBox->count() - 1, strProfileName, AccountData_ProfileName);
                }
            }
        }

        /* Set previous/default item if possible: */
        int iNewIndex = -1;
        if (   iNewIndex == -1
            && !strOldData.isNull())
            iNewIndex = m_pAccountComboBox->findData(strOldData, AccountData_ProfileName);
        if (   iNewIndex == -1
            && m_pAccountComboBox->count() > 0)
            iNewIndex = 0;
        if (iNewIndex != -1)
            m_pAccountComboBox->setCurrentIndex(iNewIndex);
    }

    /* Unblock signals after update: */
    m_pAccountComboBox->blockSignals(false);
}

void UIWizardExportAppPage2::populateAccountProperties()
{
    /* Clear table initially: */
    m_pAccountPropertyTable->clear();
    m_pAccountPropertyTable->setRowCount(0);
    m_pAccountPropertyTable->setColumnCount(0);
    /* Clear Cloud Profile: */
    m_comCloudProfile = CCloudProfile();

    /* If both provider and profile chosen: */
    if (!m_comCloudProvider.isNull() && !profileName().isNull())
    {
        /* Acquire Cloud Profile: */
        m_comCloudProfile = m_comCloudProvider.GetProfileByName(profileName());
        /* Show error message if necessary: */
        if (!m_comCloudProvider.isOk())
            msgCenter().cannotFindCloudProfile(m_comCloudProvider, profileName());
        else
        {
            /* Acquire properties: */
            QVector<QString> keys;
            QVector<QString> values;
            values = m_comCloudProfile.GetProperties(QString(), keys);
            /* Show error message if necessary: */
            if (!m_comCloudProfile.isOk())
                msgCenter().cannotAcquireCloudProfileParameter(m_comCloudProfile);
            else
            {
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
                        const QString strToolTip = m_comCloudProvider.GetPropertyDescription(keys.at(i));
                        /* Show error message if necessary: */
                        if (!m_comCloudProfile.isOk())
                            msgCenter().cannotAcquireCloudProfileParameter(m_comCloudProfile);
                        else
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
        }
    }
}

void UIWizardExportAppPage2::populateFormProperties()
{
    /* Clear appliance: */
    m_comAppliance = CAppliance();
    /* Clear description: */
    m_comVSD = CVirtualSystemDescription();
    /* Clear description form: */
    m_comVSDExportForm = CVirtualSystemDescriptionForm();

    /* If profile chosen: */
    if (m_comCloudProfile.isNotNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* Perform cloud export procedure for first uuid only: */
            const QList<QUuid> uuids = fieldImp("machineIDs").value<QList<QUuid> >();
            AssertReturnVoid(!uuids.isEmpty());
            const QUuid uMachineId = uuids.first();

            /* Get the machine with the uMachineId: */
            CVirtualBox comVBox = uiCommon().virtualBox();
            CMachine comMachine = comVBox.FindMachine(uMachineId.toString());
            if (!comVBox.isOk())
            {
                msgCenter().cannotFindMachineById(comVBox, uMachineId);
                break;
            }

            /* Create appliance: */
            CAppliance comAppliance = comVBox.CreateAppliance();
            if (!comVBox.isOk())
            {
                msgCenter().cannotCreateAppliance(comVBox);
                break;
            }

            /* Remember appliance: */
            m_comAppliance = comAppliance;

            /* Add the export virtual system description to our appliance object: */
            CVirtualSystemDescription comVSD = comMachine.ExportTo(m_comAppliance, qobject_cast<UIWizardExportApp*>(wizardImp())->uri());
            if (!comMachine.isOk())
            {
                msgCenter().cannotExportAppliance(comMachine, m_comAppliance.GetPath(), thisImp());
                break;
            }

            /* Remember description: */
            m_comVSD = comVSD;

            /* Create Cloud Client: */
            CCloudClient comCloudClient = m_comCloudProfile.CreateCloudClient();
            if (!m_comCloudProfile.isOk())
            {
                msgCenter().cannotCreateCloudClient(m_comCloudProfile);
                break;
            }

            /* Read Cloud Client Export description form: */
            CVirtualSystemDescriptionForm comExportForm;
            CProgress comExportDescriptionFormProgress = comCloudClient.GetExportLaunchDescriptionForm(m_comVSD, comExportForm);
            if (!comCloudClient.isOk())
            {
                msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
                break;
            }

            /* Show "Acquire export form" progress: */
            msgCenter().showModalProgressDialog(comExportDescriptionFormProgress, UIWizardExportApp::tr("Acquire export form..."),
                                                ":/progress_refresh_90px.png", 0, 0);
            if (!comExportDescriptionFormProgress.isOk() || comExportDescriptionFormProgress.GetResultCode() != 0)
            {
                msgCenter().cannotAcquireCloudClientParameter(comExportDescriptionFormProgress);
                break;
            }

            /* Remember description form: */
            m_comVSDExportForm = comExportForm;
        }
        while (0);
    }
}

void UIWizardExportAppPage2::updatePageAppearance()
{
    /* Update page appearance according to chosen format: */
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
    /* Save old extension to compare afterwards: */
    const QString strOldExtension = m_strFileSelectorExt;

    /* If format is cloud one: */
    if (isFormatCloudOne())
    {
        /* We use no extension: */
        m_strFileSelectorExt.clear();

        /* Update file chooser accordingly: */
        m_pFileSelector->setFileFilters(QString());
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

    /* Cascade update for file selector path if necessary: */
    if (m_strFileSelectorExt != strOldExtension)
        refreshFileSelectorPath();
}

void UIWizardExportAppPage2::refreshFileSelectorPath()
{
    /* If format is cloud one: */
    if (isFormatCloudOne())
    {
        /* Clear file selector path: */
        m_pFileSelector->setPath(QString());
    }
    else
    {
        /* Compose file selector path: */
        const QString strPath = QDir::toNativeSeparators(QString("%1/%2")
                                                         .arg(uiCommon().documentsPath())
                                                         .arg(m_strFileSelectorName + m_strFileSelectorExt));
        m_pFileSelector->setPath(strPath);
    }
}

void UIWizardExportAppPage2::refreshManifestCheckBoxAccess()
{
    /* If format is cloud one: */
    if (isFormatCloudOne())
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
    /* If format is cloud one: */
    if (isFormatCloudOne())
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

void UIWizardExportAppPage2::setProviderById(const QUuid &uId)
{
    const int iIndex = m_pFormatComboBox->findData(uId, FormatData_ID);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pFormatComboBox->setCurrentIndex(iIndex);
}

QUuid UIWizardExportAppPage2::providerId() const
{
    const int iIndex = m_pFormatComboBox->currentIndex();
    return m_pFormatComboBox->itemData(iIndex, FormatData_ID).toUuid();
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

CAppliance UIWizardExportAppPage2::appliance() const
{
    return m_comAppliance;
}

CVirtualSystemDescription UIWizardExportAppPage2::vsd() const
{
    return m_comVSD;
}

CVirtualSystemDescriptionForm UIWizardExportAppPage2::vsdExportForm() const
{
    return m_comVSDExportForm;
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic2 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic2::UIWizardExportAppPageBasic2(bool fExportToOCIByDefault)
    : UIWizardExportAppPage2(fExportToOCIByDefault)
    , m_pLabelFormat(0)
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
            QWidget *pSettingsPane2 = new QWidget;
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

                    /* Create account label: */
                    m_pAccountLabel = new QLabel;
                    if (m_pAccountLabel)
                    {
                        m_pAccountLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pAccountLabel, 0, 0);
                    }
                    /* Create sub-layout: */
                    QHBoxLayout *pSubLayout = new QHBoxLayout;
                    if (pSubLayout)
                    {
                        pSubLayout->setContentsMargins(0, 0, 0, 0);
                        pSubLayout->setSpacing(1);

                        /* Create account combo-box: */
                        m_pAccountComboBox = new QComboBox;
                        if (m_pAccountComboBox)
                        {
                            m_pAccountLabel->setBuddy(m_pAccountComboBox);

                            /* Add into layout: */
                            pSubLayout->addWidget(m_pAccountComboBox);
                        }
                        /* Create account tool-button: */
                        m_pAccountToolButton = new QIToolButton;
                        if (m_pAccountToolButton)
                        {
                            m_pAccountToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                              ":/cloud_profile_manager_disabled_16px.png"));

                            /* Add into layout: */
                            pSubLayout->addWidget(m_pAccountToolButton);
                        }

                        /* Add into layout: */
                        m_pSettingsLayout2->addLayout(pSubLayout, 0, 1);
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

                    /* Create account label: */
                    m_pMachineLabel = new QLabel;
                    if (m_pMachineLabel)
                    {
                        m_pMachineLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pMachineLabel, 2, 0);
                    }
                    /* Create Ask Then Export button: */
                    m_pRadioAskThenExport = new QRadioButton;
                    if (m_pRadioAskThenExport)
                    {
                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pRadioAskThenExport, 3, 1);
                    }
                    /* Create Export Then Ask button: */
                    m_pRadioExportThenAsk = new QRadioButton;
                    if (m_pRadioExportThenAsk)
                    {
                        m_pRadioExportThenAsk->setEnabled(false);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pRadioExportThenAsk, 2, 1);
                    }
                    /* Create Do Not Ask button: */
                    m_pRadioDoNotAsk = new QRadioButton;
                    if (m_pRadioDoNotAsk)
                    {
                        m_pRadioDoNotAsk->setEnabled(false);

                        /* Add into layout: */
                        m_pSettingsLayout2->addWidget(m_pRadioDoNotAsk, 4, 1);
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
    /* Populate accounts: */
    populateAccounts();
    /* Populate account properties: */
    populateAccountProperties();

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardExportAppPageBasic2::sltHandleFormatComboChange);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged,
            this, &UIWizardExportAppPageBasic2::sltHandleFileSelectorChange);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleFormatComboChange);
    connect(m_pMACComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleMACAddressPolicyComboChange);
    connect(m_pAccountComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageBasic2::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardExportAppPageBasic2::sltHandleAccountButtonClick);

    /* Register fields: */
    registerField("format", this, "format");
    registerField("isFormatCloudOne", this, "isFormatCloudOne");
    registerField("path", this, "path");
    registerField("macAddressPolicy", this, "macAddressPolicy");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("includeISOsSelected", this, "includeISOsSelected");
    registerField("providerShortName", this, "providerShortName");
    registerField("appliance", this, "appliance");
    registerField("vsd", this, "vsd");
    registerField("vsdExportForm", this, "vsdExportForm");
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
                               "<p>The <b>Oracle Cloud Infrastructure</b> format supports exporting to remote cloud servers only. "
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
    m_pFormatComboBox->setItemData(0, UIWizardExportApp::tr("Write in legacy OVF 0.9 format for compatibility "
                                                            "with other virtualization products."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(1, UIWizardExportApp::tr("Write in standard OVF 1.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(2, UIWizardExportApp::tr("Write in new OVF 2.0 format."), Qt::ToolTipRole);
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
                                UIWizardExportApp::tr("Include all network adapter MAC addresses in exported appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllNonNATMACs,
                                UIWizardExportApp::tr("Include only NAT network adapter MAC addresses in exported appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllMACs,
                                UIWizardExportApp::tr("Strip all network adapter MAC addresses from exported appliance archive."), Qt::ToolTipRole);

    /* Translate addtional stuff: */
    m_pAdditionalLabel->setText(UIWizardExportApp::tr("Additionally:"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("&Write Manifest file"));
    m_pIncludeISOsCheckbox->setToolTip(UIWizardExportApp::tr("Include ISO image files in exported VM archive."));
    m_pIncludeISOsCheckbox->setText(UIWizardExportApp::tr("&Include ISO image files"));

    /* Translate Account label: */
    m_pAccountLabel->setText(UIWizardExportApp::tr("&Account:"));

    /* Translate option label: */
    m_pMachineLabel->setText(UIWizardExportApp::tr("Machine Creation:"));
    m_pRadioExportThenAsk->setText(UIWizardExportApp::tr("Ask me about it &after exporting disk as custom image"));
    m_pRadioAskThenExport->setText(UIWizardExportApp::tr("Ask me about it &before exporting disk as custom image"));
    m_pRadioDoNotAsk->setText(UIWizardExportApp::tr("Do &not ask me about it, leave custom image for future usage"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pFormatComboBoxLabel;
    labels << m_pFileSelectorLabel;
    labels << m_pMACComboBoxLabel;
    labels << m_pAdditionalLabel;
    labels << m_pAccountLabel;
    labels << m_pMachineLabel;
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
    // refreshFileSelectorName(); already called from retranslateUi();
    /* Refresh file selector extension: */
    refreshFileSelectorExtension();
    /* Refresh manifest check-box access: */
    refreshManifestCheckBoxAccess();
    /* Refresh include ISOs check-box access: */
    refreshIncludeISOsCheckBoxAccess();

    /* Choose default cloud export option: */
    m_pRadioAskThenExport->setChecked(true);
}

bool UIWizardExportAppPageBasic2::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check whether there was cloud target selected: */
    if (isFormatCloudOne())
        fResult = m_comCloudProfile.isNotNull();
    else
        fResult = UICommon::hasAllowedExtension(path().toLower(), OVFFileExts);

    /* Return result: */
    return fResult;
}

bool UIWizardExportAppPageBasic2::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Check whether there was cloud target selected: */
    if (isFormatCloudOne())
    {
        /* Create appliance and populate form properties: */
        populateFormProperties();
        /* Which are required to continue to the next page: */
        fResult =    field("appliance").value<CAppliance>().isNotNull()
                  && field("vsd").value<CVirtualSystemDescription>().isNotNull()
                  && field("vsdExportForm").value<CVirtualSystemDescriptionForm>().isNotNull();
    }

    /* Return result: */
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
                                     "machines to. Make sure profile settings reflected in the underlying table are valid. "
                                     "They will be used to establish network connection required to upload your virtual machine "
                                     "files to a remote cloud facility.</p>"));
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
    emit completeChanged();
}

void UIWizardExportAppPageBasic2::sltHandleFileSelectorChange()
{
    /* Remember changed name, except empty one: */
    if (!m_pFileSelector->path().isEmpty())
        m_strFileSelectorName = QFileInfo(m_pFileSelector->path()).completeBaseName();

    /* Refresh required settings: */
    emit completeChanged();
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

void UIWizardExportAppPageBasic2::sltHandleAccountButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}
