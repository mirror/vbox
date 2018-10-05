/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
# include <QLabel>
# include <QTextEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "QITreeView.h"
# include "UIApplianceImportEditorWidget.h"
# include "UIFilePathSelector.h"
# include "UIMessageCenter.h"
# include "UIWizardImportApp.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CAppliance.h"
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


////////////////////////////////////////////////////////////////////////////////
// ImportSortProxyModel

class ImportSortProxyModel: public UIApplianceSortProxyModel
{
public:
    ImportSortProxyModel(QObject *pParent = NULL)
      : UIApplianceSortProxyModel(pParent)
    {
        m_aFilteredList << KVirtualSystemDescriptionType_License;
    }
};

////////////////////////////////////////////////////////////////////////////////
// UIApplianceImportEditorWidget

UIApplianceImportEditorWidget::UIApplianceImportEditorWidget(QWidget *pParent)
    : UIApplianceEditorWidget(pParent)
    , m_pPathSelectorLabel(0)
    , m_pPathSelector(0)
    , m_pImportHDsAsVDI(0)
    , m_pMACComboBoxLabel(0)
    , m_pMACComboBox(0)
    , m_pOptionsLayout(0)
    , m_pAdditionalOptionsLabel(0)
{
    prepareWidgets();
}

void UIApplianceImportEditorWidget::prepareWidgets()
{
    m_pPathSelectorLabel = new QIRichTextLabel(this);
    if (m_pPathSelectorLabel)
    {
        m_pLayout->addWidget(m_pPathSelectorLabel);
    }
    m_pPathSelector = new UIFilePathSelector(this);
    if (m_pPathSelector)
    {
        m_pPathSelector->setResetEnabled(true);
        m_pPathSelector->setDefaultPath(vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
        m_pPathSelector->setPath(vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
        connect(m_pPathSelector, &UIFilePathSelector::pathChanged, this, &UIApplianceImportEditorWidget::sltHandlePathChanged);
        m_pLayout->addWidget(m_pPathSelector);
    }

    m_pOptionsLayout = new QGridLayout;

    m_pMACComboBox = new QComboBox;
    if (m_pMACComboBox)
    {
        m_pMACComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        /* Add into layout: */
        m_pOptionsLayout->addWidget(m_pMACComboBox, 0, 1);
    }

        /* Create format combo-box label: */
    m_pMACComboBoxLabel = new QLabel;
    if (m_pMACComboBoxLabel)
    {
        m_pMACComboBoxLabel->setAlignment(Qt::AlignLeft | Qt::AlignTrailing | Qt::AlignVCenter);
        /* Add into layout: */
        m_pOptionsLayout->addWidget(m_pMACComboBoxLabel, 0, 0);
    }

    if (m_pMACComboBoxLabel)
        m_pMACComboBoxLabel->setBuddy(m_pMACComboBox);

    m_pAdditionalOptionsLabel = new QLabel;
    if (m_pAdditionalOptionsLabel)
    {
        m_pAdditionalOptionsLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
        m_pOptionsLayout->addWidget(m_pAdditionalOptionsLabel, 2, 0, 1, 1);
    }

    m_pImportHDsAsVDI = new QCheckBox;
    {
        m_pOptionsLayout->addWidget(m_pImportHDsAsVDI, 2, 1);
        m_pImportHDsAsVDI->setCheckState(Qt::Checked);
    }


    m_pLayout->addLayout(m_pOptionsLayout);
    populateMACAddressImportPolicies();
    connect(m_pMACComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIApplianceImportEditorWidget::sltHandleMACAddressImportPolicyComboChange);
    retranslateUi();
}

bool UIApplianceImportEditorWidget::setFile(const QString& strFile)
{
    bool fResult = false;
    if (!strFile.isEmpty())
    {
        CProgress progress;
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Create a appliance object */
        m_pAppliance = new CAppliance(vbox.CreateAppliance());
        fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Read the appliance */
            progress = m_pAppliance->Read(strFile);
            fResult = m_pAppliance->isOk();
            if (fResult)
            {
                /* Show some progress, so the user know whats going on */
                msgCenter().showModalProgressDialog(progress, tr("Reading Appliance ..."), ":/progress_reading_appliance_90px.png", this);
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    fResult = false;
                else
                {
                    /* Now we have to interpret that stuff */
                    m_pAppliance->Interpret();
                    fResult = m_pAppliance->isOk();
                    if (fResult)
                    {
                        if (m_pModel)
                            delete m_pModel;

                        QVector<CVirtualSystemDescription> vsds = m_pAppliance->GetVirtualSystemDescriptions();

                        m_pModel = new UIApplianceModel(vsds, m_pTreeViewSettings);

                        ImportSortProxyModel *pProxy = new ImportSortProxyModel(this);
                        pProxy->setSourceModel(m_pModel);
                        pProxy->sort(ApplianceViewSection_Description, Qt::DescendingOrder);

                        UIApplianceDelegate *pDelegate = new UIApplianceDelegate(pProxy, this);

                        /* Set our own model */
                        m_pTreeViewSettings->setModel(pProxy);
                        /* Set our own delegate */
                        m_pTreeViewSettings->setItemDelegate(pDelegate);
                        /* For now we hide the original column. This data is displayed as tooltip
                           also. */
                        m_pTreeViewSettings->setColumnHidden(ApplianceViewSection_OriginalValue, true);
                        m_pTreeViewSettings->expandAll();
                        /* Set model root index and make it current: */
                        m_pTreeViewSettings->setRootIndex(pProxy->mapFromSource(m_pModel->root()));
                        m_pTreeViewSettings->setCurrentIndex(pProxy->mapFromSource(m_pModel->root()));

                        /* Check for warnings & if there are one display them. */
                        bool fWarningsEnabled = false;
                        QVector<QString> warnings = m_pAppliance->GetWarnings();
                        if (warnings.size() > 0)
                        {
                            foreach (const QString& text, warnings)
                                m_pTextEditWarning->append("- " + text);
                            fWarningsEnabled = true;
                        }
                        m_pPaneWarning->setVisible(fWarningsEnabled);
                    }
                }
            }
        }
        if (!fResult)
        {
            if (!m_pAppliance->isOk())
                msgCenter().cannotImportAppliance(*m_pAppliance, this);
            else if (!progress.isNull() && (!progress.isOk() || progress.GetResultCode() != 0))
                msgCenter().cannotImportAppliance(progress, m_pAppliance->GetPath(), this);
            /* Delete the appliance in a case of an error */
            delete m_pAppliance;
            m_pAppliance = NULL;
        }
    }
    /* Make sure we initialize model items with correct base folder path: */
    if (m_pPathSelector)
        sltHandlePathChanged(m_pPathSelector->path());

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
        /* Start the import asynchronously */
        CProgress progress;
        QVector<KImportOptions> options;
        if (m_pMACComboBox)
        {
            MACAddressImportPolicy macPolicy = static_cast<MACAddressImportPolicy>(m_pMACComboBox->currentIndex());
            switch (macPolicy)
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

        if (m_pImportHDsAsVDI->isChecked())
            options.append(KImportOptions_ImportToVDI);
        progress = m_pAppliance->ImportMachines(options);
        bool fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            msgCenter().showModalProgressDialog(progress, tr("Importing Appliance ..."), ":/progress_import_90px.png", this);
            if (progress.GetCanceled())
                return false;
            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotImportAppliance(progress, m_pAppliance->GetPath(), this);
                return false;
            }
            else
                return true;
        }
        if (!fResult)
            msgCenter().cannotImportAppliance(*m_pAppliance, this);
    }
    return false;
}

QList<QPair<QString, QString> > UIApplianceImportEditorWidget::licenseAgreements() const
{
    QList<QPair<QString, QString> > list;

    CVirtualSystemDescriptionVector vsds = m_pAppliance->GetVirtualSystemDescriptions();
    for (int i = 0; i < vsds.size(); ++i)
    {
        QVector<QString> strLicense;
        strLicense = vsds[i].GetValuesByType(KVirtualSystemDescriptionType_License,
                                             KVirtualSystemDescriptionValueType_Original);
        if (!strLicense.isEmpty())
        {
            QVector<QString> strName;
            strName = vsds[i].GetValuesByType(KVirtualSystemDescriptionType_Name,
                                              KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString>(strName.first(), strLicense.first());
        }
    }

    return list;
}

void UIApplianceImportEditorWidget::retranslateUi()
{
    UIApplianceEditorWidget::retranslateUi();
    if (m_pPathSelectorLabel)
        m_pPathSelectorLabel->setText(UIWizardImportApp::tr("You can modify the base folder which will host all the virtual machines.\n"
                                                            "Home folders can also be individually (per virtual machine)  modified."));
    if (m_pImportHDsAsVDI)
    {
        m_pImportHDsAsVDI->setText(tr("&Import hard drives as VDI"));
        m_pImportHDsAsVDI->setToolTip(tr("When checked a all the hard drives that belong to this appliance will be imported in VDI format"));
    }

    /* Translate MAC address policy combo-box: */
    m_pMACComboBoxLabel->setText(tr("MAC Address &Policy:"));
    m_pMACComboBox->setItemText(MACAddressImportPolicy_KeepAllMACs,
                                tr("Include all network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressImportPolicy_KeepNATMACs,
                                tr("Include only NAT network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressImportPolicy_StripAllMACs,
                                tr("Generate new MAC addresses for all network adapters"));
    m_pMACComboBox->setItemData(MACAddressImportPolicy_KeepAllMACs,
                                tr("Include all network adapter MAC addresses in exported "
                                                    "during cloning."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressImportPolicy_KeepNATMACs,
                                tr("Include only NAT network adapter MAC addresses "
                                                    "during cloning."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressImportPolicy_StripAllMACs,
                                tr("Generate new MAC addresses for all network adapters "
                                                    "during cloning."), Qt::ToolTipRole);

    m_pAdditionalOptionsLabel->setText(tr("Additional Options:"));

    QList<QWidget*> labels;
    labels << m_pMACComboBoxLabel;
    labels << m_pAdditionalOptionsLabel;

    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pOptionsLayout->setColumnMinimumWidth(0, iMaxWidth);
}

void UIApplianceImportEditorWidget::sltHandlePathChanged(const QString &newPath)
{
    setVirtualSystemBaseFolder(newPath);
}

void UIApplianceImportEditorWidget::populateMACAddressImportPolicies()
{
    AssertReturnVoid(m_pMACComboBox->count() == 0);

    /* Apply hardcoded policies list: */
    for (int i = 0; i < (int)MACAddressImportPolicy_MAX; ++i)
    {
        m_pMACComboBox->addItem(QString::number(i));
        m_pMACComboBox->setItemData(i, i);
    }

    /* Set default: */
    setMACAddressImportPolicy(MACAddressImportPolicy_KeepNATMACs);
}

void UIApplianceImportEditorWidget::setMACAddressImportPolicy(MACAddressImportPolicy enmMACAddressImportPolicy)
{
    const int iIndex = m_pMACComboBox->findData((int)enmMACAddressImportPolicy);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pMACComboBox->setCurrentIndex(iIndex);
}

void UIApplianceImportEditorWidget::sltHandleMACAddressImportPolicyComboChange()
{
    /* Update tool-tip: */
    updateMACAddressImportPolicyComboToolTip();
}

void UIApplianceImportEditorWidget::updateMACAddressImportPolicyComboToolTip()
{
    const int iCurrentIndex = m_pMACComboBox->currentIndex();
    const QString strCurrentToolTip = m_pMACComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
    AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
    m_pMACComboBox->setToolTip(strCurrentToolTip);
}
