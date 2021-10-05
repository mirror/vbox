/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QStackedLayout>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIApplianceUnverifiedCertificateViewer.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIMessageCenter.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic2.h"

/* COM includes: */
#include "CAppliance.h"
#include "CCertificate.h"
#include "CSystemProperties.h"
#include "CVirtualSystemDescriptionForm.h"


/*********************************************************************************************************************************
*   Class UIWizardImportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardImportAppPage2::UIWizardImportAppPage2()
    : m_pSettingsCntLayout(0)
    , m_pLabelImportFilePath(0)
    , m_pEditorImportFilePath(0)
    , m_pLabelMACImportPolicy(0)
    , m_pComboMACImportPolicy(0)
    , m_pLabelAdditionalOptions(0)
    , m_pCheckboxImportHDsAsVDI(0)
{
}

void UIWizardImportAppPage2::populateMACAddressImportPolicies()
{
    /* Map known import options to known MAC address import policies: */
    QMap<KImportOptions, MACAddressImportPolicy> knownOptions;
    knownOptions[KImportOptions_KeepAllMACs] = MACAddressImportPolicy_KeepAllMACs;
    knownOptions[KImportOptions_KeepNATMACs] = MACAddressImportPolicy_KeepNATMACs;

    /* Load currently supported import options: */
    const CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KImportOptions> supportedOptions = comProperties.GetSupportedImportOptions();

    /* Check which of supported options/policies are known: */
    QList<MACAddressImportPolicy> supportedPolicies;
    foreach (const KImportOptions &enmOption, supportedOptions)
        if (knownOptions.contains(enmOption))
            supportedPolicies << knownOptions.value(enmOption);

    /* Block signals while updating: */
    m_pComboMACImportPolicy->blockSignals(true);

    /* Cleanup combo: */
    m_pComboMACImportPolicy->clear();

    /* Add supported policies first: */
    foreach (const MACAddressImportPolicy &enmPolicy, supportedPolicies)
        m_pComboMACImportPolicy->addItem(QString(), QVariant::fromValue(enmPolicy));

    /* Add hardcoded policy finally: */
    m_pComboMACImportPolicy->addItem(QString(), QVariant::fromValue(MACAddressImportPolicy_StripAllMACs));

    /* Set default: */
    if (supportedPolicies.contains(MACAddressImportPolicy_KeepNATMACs))
        setMACAddressImportPolicy(MACAddressImportPolicy_KeepNATMACs);
    else
        setMACAddressImportPolicy(MACAddressImportPolicy_StripAllMACs);

    /* Unblock signals after update: */
    m_pComboMACImportPolicy->blockSignals(false);
}

void UIWizardImportAppPage2::updatePageAppearance()
{
    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = fieldImp("isSourceCloudOne").toBool();
    /* Update page appearance according to chosen source: */
    m_pSettingsCntLayout->setCurrentIndex((int)fIsSourceCloudOne);
}

void UIWizardImportAppPage2::updateMACImportPolicyComboToolTip()
{
    const QString strCurrentToolTip = m_pComboMACImportPolicy->currentData(Qt::ToolTipRole).toString();
    m_pComboMACImportPolicy->setToolTip(strCurrentToolTip);
}

void UIWizardImportAppPage2::refreshFormPropertiesTable()
{
    /* Acquire VSD form: */
    CVirtualSystemDescriptionForm comForm = fieldImp("vsdForm").value<CVirtualSystemDescriptionForm>();
    /* Make sure the properties table get the new description form: */
    if (comForm.isNotNull())
        m_pFormEditor->setVirtualSystemDescriptionForm(comForm);
}

MACAddressImportPolicy UIWizardImportAppPage2::macAddressImportPolicy() const
{
    return m_pComboMACImportPolicy->currentData().value<MACAddressImportPolicy>();
}

void UIWizardImportAppPage2::setMACAddressImportPolicy(MACAddressImportPolicy enmMACAddressImportPolicy)
{
    const int iIndex = m_pComboMACImportPolicy->findData(enmMACAddressImportPolicy);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pComboMACImportPolicy->setCurrentIndex(iIndex);
}

bool UIWizardImportAppPage2::importHDsAsVDI() const
{
    return m_pCheckboxImportHDsAsVDI->isChecked();
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPageBasic2 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardImportAppPageBasic2::UIWizardImportAppPageBasic2(const QString &strFileName)
    : m_strFileName(strFileName)
    , m_enmCertText(kCertText_Uninitialized)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Prepare label: */
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);

        /* Prepare settings container layout: */
        m_pSettingsCntLayout = new QStackedLayout;
        if (m_pSettingsCntLayout)
        {
            /* Prepare appliance widget container: */
            QWidget *pApplianceWidgetCnt = new QWidget(this);
            if (pApplianceWidgetCnt)
            {
                /* Prepare appliance widget layout: */
                QGridLayout *pApplianceWidgetLayout = new QGridLayout(pApplianceWidgetCnt);
                if (pApplianceWidgetLayout)
                {
                    pApplianceWidgetLayout->setContentsMargins(0, 0, 0, 0);
                    pApplianceWidgetLayout->setColumnStretch(0, 0);
                    pApplianceWidgetLayout->setColumnStretch(1, 1);

                    /* Prepare appliance widget: */
                    m_pApplianceWidget = new UIApplianceImportEditorWidget(pApplianceWidgetCnt);
                    if (m_pApplianceWidget)
                    {
                        m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                        pApplianceWidgetLayout->addWidget(m_pApplianceWidget, 0, 0, 1, 3);
                    }

                    /* Prepare path selector label: */
                    m_pLabelImportFilePath = new QLabel(pApplianceWidgetCnt);
                    if (m_pLabelImportFilePath)
                    {
                        m_pLabelImportFilePath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pApplianceWidgetLayout->addWidget(m_pLabelImportFilePath, 1, 0);
                    }
                    /* Prepare path selector editor: */
                    m_pEditorImportFilePath = new UIFilePathSelector(pApplianceWidgetCnt);
                    if (m_pEditorImportFilePath)
                    {
                        m_pEditorImportFilePath->setResetEnabled(true);
                        m_pEditorImportFilePath->setDefaultPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
                        m_pEditorImportFilePath->setPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
                        m_pLabelImportFilePath->setBuddy(m_pEditorImportFilePath);
                        pApplianceWidgetLayout->addWidget(m_pEditorImportFilePath, 1, 1, 1, 2);
                    }

                    /* Prepare MAC address policy label: */
                    m_pLabelMACImportPolicy = new QLabel(pApplianceWidgetCnt);
                    if (m_pLabelMACImportPolicy)
                    {
                        m_pLabelMACImportPolicy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pApplianceWidgetLayout->addWidget(m_pLabelMACImportPolicy, 2, 0);
                    }
                    /* Prepare MAC address policy combo: */
                    m_pComboMACImportPolicy = new QComboBox(pApplianceWidgetCnt);
                    if (m_pComboMACImportPolicy)
                    {
                        m_pComboMACImportPolicy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                        m_pLabelMACImportPolicy->setBuddy(m_pComboMACImportPolicy);
                        pApplianceWidgetLayout->addWidget(m_pComboMACImportPolicy, 2, 1, 1, 2);
                    }

                    /* Prepare additional options label: */
                    m_pLabelAdditionalOptions = new QLabel(pApplianceWidgetCnt);
                    if (m_pLabelAdditionalOptions)
                    {
                        m_pLabelAdditionalOptions->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pApplianceWidgetLayout->addWidget(m_pLabelAdditionalOptions, 3, 0);
                    }
                    /* Prepare import HDs as VDIs checkbox: */
                    m_pCheckboxImportHDsAsVDI = new QCheckBox(pApplianceWidgetCnt);
                    {
                        m_pCheckboxImportHDsAsVDI->setCheckState(Qt::Checked);
                        pApplianceWidgetLayout->addWidget(m_pCheckboxImportHDsAsVDI, 3, 1);
                    }

                    /* Prepare certificate label: */
                    m_pCertLabel = new QLabel(pApplianceWidgetCnt);
                    if (m_pCertLabel)
                        pApplianceWidgetLayout->addWidget(m_pCertLabel, 4, 0, 1, 3);
                }

                /* Add into layout: */
                m_pSettingsCntLayout->addWidget(pApplianceWidgetCnt);
            }

            /* Prepare form editor container: */
            QWidget *pFormEditorCnt = new QWidget(this);
            if (pFormEditorCnt)
            {
                /* Prepare form editor layout: */
                QVBoxLayout *pFormEditorLayout = new QVBoxLayout(pFormEditorCnt);
                if (pFormEditorLayout)
                {
                    pFormEditorLayout->setContentsMargins(0, 0, 0, 0);

                    /* Prepare form editor widget: */
                    m_pFormEditor = new UIFormEditorWidget(pFormEditorCnt);
                    if (m_pFormEditor)
                        pFormEditorLayout->addWidget(m_pFormEditor);
                }

                /* Add into layout: */
                m_pSettingsCntLayout->addWidget(pFormEditorCnt);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pSettingsCntLayout);
        }
    }

    /* Setup connections: */
    connect(m_pEditorImportFilePath, &UIFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageBasic2::sltHandlePathChanged);
    connect(m_pComboMACImportPolicy, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageBasic2::sltHandleMACImportPolicyChange);

    /* Register classes: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register fields: */
    registerField("applianceWidget", this, "applianceWidget");
    registerField("macAddressImportPolicy", this, "macAddressImportPolicy");
    registerField("importHDsAsVDI", this, "importHDsAsVDI");
}

void UIWizardImportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance settings"));

    /* Translate path selector label: */
    if (m_pLabelImportFilePath)
        m_pLabelImportFilePath->setText(tr("&Machine Base Folder:"));

    /* Translate MAC address policy combo-box: */
    if (m_pLabelMACImportPolicy)
    {
        m_pLabelMACImportPolicy->setText(tr("MAC Address &Policy:"));
        for (int i = 0; i < m_pComboMACImportPolicy->count(); ++i)
        {
            const MACAddressImportPolicy enmPolicy = m_pComboMACImportPolicy->itemData(i).value<MACAddressImportPolicy>();
            switch (enmPolicy)
            {
                case MACAddressImportPolicy_KeepAllMACs:
                {
                    m_pComboMACImportPolicy->setItemText(i, tr("Include all network adapter MAC addresses"));
                    m_pComboMACImportPolicy->setItemData(i, tr("Include all network adapter MAC addresses during importing."), Qt::ToolTipRole);
                    break;
                }
                case MACAddressImportPolicy_KeepNATMACs:
                {
                    m_pComboMACImportPolicy->setItemText(i, tr("Include only NAT network adapter MAC addresses"));
                    m_pComboMACImportPolicy->setItemData(i, tr("Include only NAT network adapter MAC addresses during importing."), Qt::ToolTipRole);
                    break;
                }
                case MACAddressImportPolicy_StripAllMACs:
                {
                    m_pComboMACImportPolicy->setItemText(i, tr("Generate new MAC addresses for all network adapters"));
                    m_pComboMACImportPolicy->setItemData(i, tr("Generate new MAC addresses for all network adapters during importing."), Qt::ToolTipRole);
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* Translate additional options label: */
    if (m_pLabelAdditionalOptions)
        m_pLabelAdditionalOptions->setText(tr("Additional Options:"));
    /* Translate additional option check-box: */
    if (m_pCheckboxImportHDsAsVDI)
    {
        m_pCheckboxImportHDsAsVDI->setText(tr("&Import hard drives as VDI"));
        m_pCheckboxImportHDsAsVDI->setToolTip(tr("Import all the hard drives that belong to this appliance in VDI format."));
    }

    /* Translate the certificate label: */
    if (m_pCertLabel)
    {
        switch (m_enmCertText)
        {
            case kCertText_Unsigned:
                m_pCertLabel->setText(UIWizardImportApp::tr("Appliance is not signed"));
                break;
            case kCertText_IssuedTrusted:
                m_pCertLabel->setText(UIWizardImportApp::tr("Appliance signed by %1 (trusted)").arg(m_strSignedBy));
                break;
            case kCertText_IssuedExpired:
                m_pCertLabel->setText(UIWizardImportApp::tr("Appliance signed by %1 (expired!)").arg(m_strSignedBy));
                break;
            case kCertText_IssuedUnverified:
                m_pCertLabel->setText(UIWizardImportApp::tr("Unverified signature by %1!").arg(m_strSignedBy));
                break;
            case kCertText_SelfSignedTrusted:
                m_pCertLabel->setText(UIWizardImportApp::tr("Self signed by %1 (trusted)").arg(m_strSignedBy));
                break;
            case kCertText_SelfSignedExpired:
                m_pCertLabel->setText(UIWizardImportApp::tr("Self signed by %1 (expired!)").arg(m_strSignedBy));
                break;
            case kCertText_SelfSignedUnverified:
                m_pCertLabel->setText(UIWizardImportApp::tr("Unverified self signed signature by %1!").arg(m_strSignedBy));
                break;
            default:
                AssertFailed();
                RT_FALL_THRU();
            case kCertText_Uninitialized:
                m_pCertLabel->setText("<uninitialized page>");
                break;
        }
    }

    /* Update page appearance: */
    updatePageAppearance();
}

void UIWizardImportAppPageBasic2::initializePage()
{
    /* Update widget visibility: */
    updatePageAppearance();

    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = field("isSourceCloudOne").toBool();
    if (fIsSourceCloudOne)
    {
        /* Refresh form properties table: */
        refreshFormPropertiesTable();
    }
    else
    {
        /* Populate MAC address import combo: */
        populateMACAddressImportPolicies();

        /* If we have file name passed,
         * check if specified file contains valid appliance: */
        if (   !m_strFileName.isEmpty()
            && !qobject_cast<UIWizardImportApp*>(wizard())->setFile(m_strFileName))
        {
            wizard()->reject();
            return;
        }

        /* Acquire appliance: */
        CAppliance comAppliance = qobject_cast<UIWizardImportApp*>(wizard())->appliance();

        /* Initialize appliance widget: */
        m_pApplianceWidget->setAppliance(comAppliance);
        /* Make sure we initialize appliance widget model with correct base folder path: */
        if (m_pEditorImportFilePath)
            sltHandlePathChanged(m_pEditorImportFilePath->path());

        /* Acquire certificate: */
        CCertificate comCertificate = comAppliance.GetCertificate();
        if (comCertificate.isNull())
            m_enmCertText = kCertText_Unsigned;
        else
        {
            /* Pick a 'signed-by' name: */
            m_strSignedBy = comCertificate.GetFriendlyName();

            /* If trusted, just select the right message: */
            if (comCertificate.GetTrusted())
            {
                if (comCertificate.GetSelfSigned())
                    m_enmCertText = !comCertificate.GetExpired() ? kCertText_SelfSignedTrusted : kCertText_SelfSignedExpired;
                else
                    m_enmCertText = !comCertificate.GetExpired() ? kCertText_IssuedTrusted     : kCertText_IssuedExpired;
            }
            else
            {
                /* Not trusted!  Must ask the user whether to continue in this case: */
                m_enmCertText = comCertificate.GetSelfSigned() ? kCertText_SelfSignedUnverified : kCertText_IssuedUnverified;

                /* Translate page early: */
                retranslateUi();

                /* Instantiate the dialog: */
                QPointer<UIApplianceUnverifiedCertificateViewer> pDialog =
                    new UIApplianceUnverifiedCertificateViewer(this, comCertificate);

                /* Show viewer in modal mode: */
                const int iResultCode = pDialog->exec();

                /* Leave if viewer destroyed prematurely: */
                if (!pDialog)
                    return;
                /* Delete viewer finally: */
                delete pDialog;

                /* Dismiss the entire import-appliance wizard if user rejects certificate: */
                if (iResultCode == QDialog::Rejected)
                    wizard()->reject();
            }
        }
    }

    /* Translate page: */
    retranslateUi();
}

void UIWizardImportAppPageBasic2::cleanupPage()
{
    /* Rollback settings: */
    m_pApplianceWidget->restoreDefaults();
    /* Call to base-class: */
    UIWizardPage::cleanupPage();
}

bool UIWizardImportAppPageBasic2::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = fieldImp("isSourceCloudOne").toBool();
    if (fIsSourceCloudOne)
    {
        /* Make sure table has own data committed: */
        m_pFormEditor->makeSureEditorDataCommitted();

        /* Check whether we have proper VSD form: */
        CVirtualSystemDescriptionForm comForm = fieldImp("vsdForm").value<CVirtualSystemDescriptionForm>();
        fResult = comForm.isNotNull();
        Assert(fResult);

        /* Give changed VSD back to appliance: */
        if (fResult)
        {
            comForm.GetVirtualSystemDescription();
            fResult = comForm.isOk();
            if (!fResult)
                msgCenter().cannotAcquireVirtualSystemDescriptionFormProperty(comForm);
        }
    }
    else
    {
        /* Make sure widget has own data committed: */
        m_pApplianceWidget->prepareImport();
    }

    /* Try to import appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardImportApp*>(wizard())->importAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardImportAppPageBasic2::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardImportAppPage2::updatePageAppearance();

    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = field("isSourceCloudOne").toBool();
    if (fIsSourceCloudOne)
    {
        m_pLabel->setText(UIWizardImportApp::tr("These are the the suggested settings of the cloud VM import "
                                                "procedure, they are influencing the resulting local VM instance. "
                                                "You can change many of the properties shown by double-clicking "
                                                "on the items and disable others using the check boxes below."));
        m_pFormEditor->setFocus();
    }
    else
    {
        m_pLabel->setText(UIWizardImportApp::tr("These are the virtual machines contained in the appliance "
                                                "and the suggested settings of the imported VirtualBox machines. "
                                                "You can change many of the properties shown by double-clicking "
                                                "on the items and disable others using the check boxes below."));
        m_pApplianceWidget->setFocus();
    }
}

void UIWizardImportAppPageBasic2::sltHandlePathChanged(const QString &strNewPath)
{
    m_pApplianceWidget->setVirtualSystemBaseFolder(strNewPath);
}

void UIWizardImportAppPageBasic2::sltHandleMACImportPolicyChange()
{
    updateMACImportPolicyComboToolTip();
}
