/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITreeView.h"
#include "UIApplianceImportEditorWidget.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardImportApp.h"

/* COM includes: */
#include "CAppliance.h"
#include "CSystemProperties.h"


/** UIApplianceSortProxyModel subclass for Export Appliance wizard. */
class ImportSortProxyModel: public UIApplianceSortProxyModel
{
public:

    /** Constructs proxy model passing @a pParent to the base-class. */
    ImportSortProxyModel(QObject *pParent = 0)
        : UIApplianceSortProxyModel(pParent)
    {
        m_aFilteredList << KVirtualSystemDescriptionType_License;
    }
};


/*********************************************************************************************************************************
*   Class UIApplianceImportEditorWidget implementation.                                                                          *
*********************************************************************************************************************************/

UIApplianceImportEditorWidget::UIApplianceImportEditorWidget(QWidget *pParent)
    : UIApplianceEditorWidget(pParent)
    , m_pLayoutOptions(0)
    , m_pLabelExportingFilePath(0)
    , m_pEditorExportingFilePath(0)
    , m_pLabelMACExportPolicy(0)
    , m_pComboMACExportPolicy(0)
    , m_pLabelAdditionalOptions(0)
    , m_pCheckboxImportHDsAsVDI(0)
{
    prepare();
}

bool UIApplianceImportEditorWidget::setFile(const QString& strFile)
{
    bool fResult = false;
    if (!strFile.isEmpty())
    {
        CProgress comProgress;

        /* Create a appliance object: */
        CVirtualBox comVBox = uiCommon().virtualBox();
        m_pAppliance = new CAppliance(comVBox.CreateAppliance());
        fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Read the appliance: */
            comProgress = m_pAppliance->Read(strFile);
            fResult = m_pAppliance->isOk();
            if (fResult)
            {
                /* Show some progress, so the user know whats going on: */
                msgCenter().showModalProgressDialog(comProgress, tr("Reading Appliance ..."),
                                                    ":/progress_reading_appliance_90px.png", this);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    fResult = false;
                else
                {
                    /* Now we have to interpret that stuff: */
                    m_pAppliance->Interpret();
                    fResult = m_pAppliance->isOk();
                    if (fResult)
                    {
                        /* Cleanup previous stuff: */
                        if (m_pModel)
                            delete m_pModel;

                        /* Prepare model: */
                        QVector<CVirtualSystemDescription> vsds = m_pAppliance->GetVirtualSystemDescriptions();
                        m_pModel = new UIApplianceModel(vsds, m_pTreeViewSettings);
                        if (m_pModel)
                        {
                            /* Create proxy model: */
                            ImportSortProxyModel *pProxy = new ImportSortProxyModel(m_pModel);
                            if (pProxy)
                            {
                                pProxy->setSourceModel(m_pModel);
                                pProxy->sort(ApplianceViewSection_Description, Qt::DescendingOrder);

                                /* Set our own model */
                                m_pTreeViewSettings->setModel(pProxy);
                                /* Set our own delegate */
                                UIApplianceDelegate *pDelegate = new UIApplianceDelegate(pProxy);
                                if (pDelegate)
                                    m_pTreeViewSettings->setItemDelegate(pDelegate);

                                /* For now we hide the original column. This data is displayed as tooltip also. */
                                m_pTreeViewSettings->setColumnHidden(ApplianceViewSection_OriginalValue, true);
                                m_pTreeViewSettings->expandAll();
                                /* Set model root index and make it current: */
                                m_pTreeViewSettings->setRootIndex(pProxy->mapFromSource(m_pModel->root()));
                                m_pTreeViewSettings->setCurrentIndex(pProxy->mapFromSource(m_pModel->root()));
                            }
                        }

                        /* Check for warnings & if there are one display them: */
                        const QVector<QString> warnings = m_pAppliance->GetWarnings();
                        bool fWarningsEnabled = warnings.size() > 0;
                        foreach (const QString &strText, warnings)
                            m_pTextEditWarning->append("- " + strText);
                        m_pPaneWarning->setVisible(fWarningsEnabled);
                    }
                }
            }
        }

        if (!fResult)
        {
            if (!m_pAppliance->isOk())
                msgCenter().cannotImportAppliance(*m_pAppliance, this);
            else if (!comProgress.isNull() && (!comProgress.isOk() || comProgress.GetResultCode() != 0))
                msgCenter().cannotImportAppliance(comProgress, m_pAppliance->GetPath(), this);
            /* Delete the appliance in a case of an error: */
            delete m_pAppliance;
            m_pAppliance = 0;
        }
    }

    /* Make sure we initialize model items with correct base folder path: */
    if (m_pEditorExportingFilePath)
        sltHandlePathChanged(m_pEditorExportingFilePath->path());

    return fResult;
}

void UIApplianceImportEditorWidget::prepareImport()
{
    if (m_pAppliance)
        m_pModel->putBack();
}

bool UIApplianceImportEditorWidget::import()
{
    if (m_pAppliance)
    {
        /* Configure appliance importing: */
        QVector<KImportOptions> options;
        if (m_pComboMACExportPolicy)
        {
            const MACAddressImportPolicy enmPolicy = m_pComboMACExportPolicy->currentData().value<MACAddressImportPolicy>();
            switch (enmPolicy)
            {
                case MACAddressImportPolicy_KeepAllMACs:
                    options.append(KImportOptions_KeepAllMACs);
                    break;
                case MACAddressImportPolicy_KeepNATMACs:
                    options.append(KImportOptions_KeepNATMACs);
                    break;
                default:
                    break;
            }
        }
        if (m_pCheckboxImportHDsAsVDI->isChecked())
            options.append(KImportOptions_ImportToVDI);

        /* Import appliance: */
        UINotificationProgressApplianceImport *pNotification = new UINotificationProgressApplianceImport(*m_pAppliance,
                                                                                                         options);
        gpNotificationCenter->append(pNotification);

        /* Positive: */
        return true;
    }
    return false;
}

QList<QPair<QString, QString> > UIApplianceImportEditorWidget::licenseAgreements() const
{
    QList<QPair<QString, QString> > list;

    foreach (CVirtualSystemDescription comVsd, m_pAppliance->GetVirtualSystemDescriptions())
    {
        QVector<QString> strLicense;
        strLicense = comVsd.GetValuesByType(KVirtualSystemDescriptionType_License,
                                            KVirtualSystemDescriptionValueType_Original);
        if (!strLicense.isEmpty())
        {
            QVector<QString> strName;
            strName = comVsd.GetValuesByType(KVirtualSystemDescriptionType_Name,
                                             KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString>(strName.first(), strLicense.first());
        }
    }

    return list;
}

void UIApplianceImportEditorWidget::retranslateUi()
{
    /* Call to base-class: */
    UIApplianceEditorWidget::retranslateUi();

    /* Translate path selector label: */
    if (m_pLabelExportingFilePath)
        m_pLabelExportingFilePath->setText(tr("&Machine Base Folder:"));

    /* Translate check-box: */
    if (m_pCheckboxImportHDsAsVDI)
    {
        m_pCheckboxImportHDsAsVDI->setText(tr("&Import hard drives as VDI"));
        m_pCheckboxImportHDsAsVDI->setToolTip(tr("Import all the hard drives that belong to this appliance in VDI format."));
    }

    /* Translate MAC address policy combo-box: */
    m_pLabelMACExportPolicy->setText(tr("MAC Address &Policy:"));
    for (int i = 0; i < m_pComboMACExportPolicy->count(); ++i)
    {
        const MACAddressImportPolicy enmPolicy = m_pComboMACExportPolicy->itemData(i).value<MACAddressImportPolicy>();
        switch (enmPolicy)
        {
            case MACAddressImportPolicy_KeepAllMACs:
            {
                m_pComboMACExportPolicy->setItemText(i, tr("Include all network adapter MAC addresses"));
                m_pComboMACExportPolicy->setItemData(i, tr("Include all network adapter MAC addresses during importing."), Qt::ToolTipRole);
                break;
            }
            case MACAddressImportPolicy_KeepNATMACs:
            {
                m_pComboMACExportPolicy->setItemText(i, tr("Include only NAT network adapter MAC addresses"));
                m_pComboMACExportPolicy->setItemData(i, tr("Include only NAT network adapter MAC addresses during importing."), Qt::ToolTipRole);
                break;
            }
            case MACAddressImportPolicy_StripAllMACs:
            {
                m_pComboMACExportPolicy->setItemText(i, tr("Generate new MAC addresses for all network adapters"));
                m_pComboMACExportPolicy->setItemData(i, tr("Generate new MAC addresses for all network adapters during importing."), Qt::ToolTipRole);
                break;
            }
            default:
                break;
        }
    }

    m_pLabelAdditionalOptions->setText(tr("Additional Options:"));

#if 0 /* this may be needed if contents became dinamical to avoid label jumping */
    QList<QWidget*> labels;
    labels << m_pLabelMACExportPolicy;
    labels << m_pLabelAdditionalOptions;

    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pLayoutOptions->setColumnMinimumWidth(0, iMaxWidth);
#endif /* this may be needed if contents became dinamical to avoid label jumping */
}

void UIApplianceImportEditorWidget::sltHandlePathChanged(const QString &strNewPath)
{
    setVirtualSystemBaseFolder(strNewPath);
}

void UIApplianceImportEditorWidget::sltHandleMACAddressImportPolicyChange()
{
    /* Update tool-tip: */
    const QString strCurrentToolTip = m_pComboMACExportPolicy->currentData(Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pComboMACExportPolicy->setToolTip(strCurrentToolTip);
}

void UIApplianceImportEditorWidget::prepare()
{
    /* Prepare options layout: */
    m_pLayoutOptions = new QGridLayout;
    if (m_pLayoutOptions)
    {
        m_pLayoutOptions->setColumnStretch(0, 0);
        m_pLayoutOptions->setColumnStretch(1, 1);

        /* Prepare path selector label: */
        m_pLabelExportingFilePath = new QLabel(this);
        if (m_pLabelExportingFilePath)
        {
            m_pLabelExportingFilePath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayoutOptions->addWidget(m_pLabelExportingFilePath, 0, 0);
        }
        /* Prepare path selector editor: */
        m_pEditorExportingFilePath = new UIFilePathSelector(this);
        if (m_pEditorExportingFilePath)
        {
            m_pEditorExportingFilePath->setResetEnabled(true);
            m_pEditorExportingFilePath->setDefaultPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
            m_pEditorExportingFilePath->setPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
            m_pLabelExportingFilePath->setBuddy(m_pEditorExportingFilePath);
            m_pLayoutOptions->addWidget(m_pEditorExportingFilePath, 0, 1, 1, 2);
        }

        /* Prepare MAC address policy label: */
        m_pLabelMACExportPolicy = new QLabel(this);
        if (m_pLabelMACExportPolicy)
        {
            m_pLabelMACExportPolicy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayoutOptions->addWidget(m_pLabelMACExportPolicy, 1, 0);
        }
        /* Prepare MAC address policy combo: */
        m_pComboMACExportPolicy = new QComboBox(this);
        if (m_pComboMACExportPolicy)
        {
            m_pComboMACExportPolicy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            m_pLabelMACExportPolicy->setBuddy(m_pComboMACExportPolicy);
            m_pLayoutOptions->addWidget(m_pComboMACExportPolicy, 1, 1, 1, 2);
        }

        /* Prepare additional options label: */
        m_pLabelAdditionalOptions = new QLabel(this);
        if (m_pLabelAdditionalOptions)
        {
            m_pLabelAdditionalOptions->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayoutOptions->addWidget(m_pLabelAdditionalOptions, 2, 0);
        }
        /* Prepare import HDs as VDIs checkbox: */
        m_pCheckboxImportHDsAsVDI = new QCheckBox(this);
        {
            m_pCheckboxImportHDsAsVDI->setCheckState(Qt::Checked);
            m_pLayoutOptions->addWidget(m_pCheckboxImportHDsAsVDI, 2, 1);
        }

        /* Add into layout: */
        m_pLayout->addLayout(m_pLayoutOptions);
    }

    /* Populate MAC address import combo: */
    populateMACAddressImportPolicies();

    /* And connect signals afterwards: */
    connect(m_pEditorExportingFilePath, &UIFilePathSelector::pathChanged,
            this, &UIApplianceImportEditorWidget::sltHandlePathChanged);
    connect(m_pComboMACExportPolicy, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIApplianceImportEditorWidget::sltHandleMACAddressImportPolicyChange);

    /* Apply language settings: */
    retranslateUi();
}

void UIApplianceImportEditorWidget::populateMACAddressImportPolicies()
{
    AssertReturnVoid(m_pComboMACExportPolicy->count() == 0);

    /* Map known import options to known MAC address import policies: */
    QMap<KImportOptions, MACAddressImportPolicy> knownOptions;
    knownOptions[KImportOptions_KeepAllMACs] = MACAddressImportPolicy_KeepAllMACs;
    knownOptions[KImportOptions_KeepNATMACs] = MACAddressImportPolicy_KeepNATMACs;

    /* Load currently supported import options: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KImportOptions> supportedOptions = comProperties.GetSupportedImportOptions();

    /* Check which of supported options/policies are known: */
    QList<MACAddressImportPolicy> supportedPolicies;
    foreach (const KImportOptions &enmOption, supportedOptions)
        if (knownOptions.contains(enmOption))
            supportedPolicies << knownOptions.value(enmOption);

    /* Add supported policies first: */
    foreach (const MACAddressImportPolicy &enmPolicy, supportedPolicies)
        m_pComboMACExportPolicy->addItem(QString(), QVariant::fromValue(enmPolicy));

    /* Add hardcoded policy finally: */
    m_pComboMACExportPolicy->addItem(QString(), QVariant::fromValue(MACAddressImportPolicy_StripAllMACs));

    /* Set default: */
    if (supportedPolicies.contains(MACAddressImportPolicy_KeepNATMACs))
        setMACAddressImportPolicy(MACAddressImportPolicy_KeepNATMACs);
    else
        setMACAddressImportPolicy(MACAddressImportPolicy_StripAllMACs);
}

void UIApplianceImportEditorWidget::setMACAddressImportPolicy(MACAddressImportPolicy enmMACAddressImportPolicy)
{
    const int iIndex = m_pComboMACExportPolicy->findData(enmMACAddressImportPolicy);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pComboMACExportPolicy->setCurrentIndex(iIndex);
}
