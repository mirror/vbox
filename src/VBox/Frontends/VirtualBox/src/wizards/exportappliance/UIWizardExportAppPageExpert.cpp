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
# include <QHeaderView>
# include <QLabel>
# include <QLineEdit>
# include <QListWidget>
# include <QRadioButton>
# include <QStackedWidget>
# include <QTableWidget>
# include <QVBoxLayout>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIApplianceExportEditorWidget.h"
# include "UIConverter.h"
# include "UIEmptyFilePathSelector.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageExpert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageExpert implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageExpert::UIWizardExportAppPageExpert(const QStringList &selectedVMNames)
    : m_pSelectorCnt(0)
    , m_pApplianceCnt(0)
    , m_pSettingsCnt(0)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setRowStretch(0, 1);

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

        /* Create settings widget container: */
        m_pSettingsCnt = new QGroupBox;
        if (m_pSettingsCnt)
        {
            /* Create settings widget container layout: */
            QVBoxLayout *pSettingsCntLayout = new QVBoxLayout(m_pSettingsCnt);
            if (pSettingsCntLayout)
            {
#ifdef VBOX_WS_MAC
                pSettingsCntLayout->setSpacing(5);
#endif

                /* Create format layout: */
                m_pFormatLayout = new QGridLayout;
                if (m_pFormatLayout)
                {
                    m_pFormatLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
                    m_pFormatLayout->setSpacing(10);
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
                    pSettingsCntLayout->addLayout(m_pFormatLayout);
                }

                /* Create settings widget: */
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
                            m_pSettingsLayout1->setSpacing(10);
#endif
                            m_pSettingsLayout1->setContentsMargins(0, 0, 0, 0);
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
                                m_pFileSelectorLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
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
                                m_pMACComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                                m_pMACComboBoxLabel->setBuddy(m_pMACComboBox);

                                /* Add into layout: */
                                m_pSettingsLayout1->addWidget(m_pMACComboBoxLabel, 1, 0);
                            }

                            /* Create advanced label: */
                            m_pAdditionalLabel = new QLabel;
                            if (m_pAdditionalLabel)
                            {
                                m_pAdditionalLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

                                /* Add into layout: */
                                m_pSettingsLayout1->addWidget(m_pAdditionalLabel, 2, 0);
                            }
                            /* Create manifest check-box editor: */
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
                            m_pSettingsLayout2->setSpacing(10);
#endif
                            m_pSettingsLayout2->setContentsMargins(0, 0, 0, 0);
                            m_pSettingsLayout2->setColumnStretch(0, 0);
                            m_pSettingsLayout2->setColumnStretch(1, 1);

                            /* Create account combo-box: */
                            m_pAccountComboBox = new QComboBox;
                            if (m_pAccountComboBox)
                            {
                                /* Add into layout: */
                                m_pSettingsLayout2->addWidget(m_pAccountComboBox, 0, 1);
                            }
                            /* Create account label: */
                            m_pAccountComboBoxLabel = new QLabel;
                            if (m_pAccountComboBoxLabel)
                            {
                                m_pAccountComboBoxLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                                m_pAccountComboBoxLabel->setBuddy(m_pAccountComboBox);

                                /* Add into layout: */
                                m_pSettingsLayout2->addWidget(m_pAccountComboBoxLabel, 0, 0);
                            }
                            /* Create account property table: */
                            m_pAccountPropertyTable = new QTableWidget;
                            if (m_pAccountPropertyTable)
                            {
                                const QFontMetrics fm(m_pAccountPropertyTable->font());
                                const int iFontWidth = fm.width('x');
                                const int iTotalWidth = 50 * iFontWidth;
                                const int iFontHeight = fm.height();
                                const int iTotalHeight = 4 * iFontHeight;
                                m_pAccountPropertyTable->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                                m_pAccountPropertyTable->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
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
                    pSettingsCntLayout->addWidget(m_pSettingsWidget);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt, 1, 0, 1, 2);
        }
    }

    /* Populate VM selector items: */
    populateVMSelectorItems(selectedVMNames);
    /* Populate formats: */
    populateFormats();
    /* Populate MAC address policies: */
    populateMACAddressPolicies();
    /* Populate accounts: */
    populateAccounts();
    /* Populate account properties: */
    populateAccountProperties();

    /* Setup connections: */
    connect(m_pVMSelector, &QListWidget::itemSelectionChanged,      this, &UIWizardExportAppPageExpert::sltVMSelectionChangeHandler);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardExportAppPageExpert::completeChanged);
    connect(m_pFormatComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);
    connect(m_pMACComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleMACAddressPolicyComboChange);
    connect(m_pAccountComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleAccountComboChange);

    /* Register classes: */
    qRegisterMetaType<ExportAppliancePointer>();

    /* Register fields: */
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");
    registerField("format", this, "format");
    registerField("path", this, "path");
    registerField("macAddressPolicy", this, "macAddressPolicy");
    registerField("manifestSelected", this, "manifestSelected");
    registerField("includeISOsSelected", this, "includeISOsSelected");
    registerField("providerShortName", this, "providerShortName");
    registerField("profileName", this, "profileName");
    registerField("profile", this, "profile");
    registerField("applianceWidget", this, "applianceWidget");
}

bool UIWizardExportAppPageExpert::event(QEvent *pEvent)
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

void UIWizardExportAppPageExpert::retranslateUi()
{
    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");

    /* Translate group-boxes: */
    m_pSelectorCnt->setTitle(UIWizardExportApp::tr("Virtual &machines to export"));
    m_pApplianceCnt->setTitle(UIWizardExportApp::tr("Virtual &system settings"));
    m_pSettingsCnt->setTitle(UIWizardExportApp::tr("Appliance settings"));

    /* Translate File selector: */
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setChooseButtonToolTip(UIWizardExportApp::tr("Choose a file to export the virtual appliance to..."));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a file to export the virtual appliance to"));

    /* Translate Format combo-box: */
    m_pFormatComboBoxLabel->setText(UIWizardExportApp::tr("F&ormat:"));
    m_pFormatComboBox->setItemText(0, UIWizardExportApp::tr("Open Virtualization Format 0.9"));
    m_pFormatComboBox->setItemText(1, UIWizardExportApp::tr("Open Virtualization Format 1.0"));
    m_pFormatComboBox->setItemText(2, UIWizardExportApp::tr("Open Virtualization Format 2.0"));
    m_pFormatComboBox->setItemText(3, UIWizardExportApp::tr("Oracle Public Cloud Format 1.0"));
    m_pFormatComboBox->setItemText(4, UIWizardExportApp::tr("Cloud Service Provider"));
    m_pFormatComboBox->setItemData(0, UIWizardExportApp::tr("Write in legacy OVF 0.9 format for compatibility "
                                                            "with other virtualization products."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(1, UIWizardExportApp::tr("Write in standard OVF 1.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(2, UIWizardExportApp::tr("Write in new OVF 2.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(3, UIWizardExportApp::tr("Write in Oracle Public Cloud 1.0 format."), Qt::ToolTipRole);
    m_pFormatComboBox->setItemData(4, UIWizardExportApp::tr("Export to Cloud Service Provider."), Qt::ToolTipRole);

    /* Translate MAC address policy combo-box: */
    m_pMACComboBoxLabel->setText(UIWizardExportApp::tr("MAC Address &Policy:"));
    m_pMACComboBox->setItemText(MACAddressPolicy_KeepAllMACs,
                                UIWizardExportApp::tr("Include all network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressPolicy_StripAllNonNATMACs,
                                UIWizardExportApp::tr("Include only NAT network adapter MAC addresses"));
    m_pMACComboBox->setItemText(MACAddressPolicy_StripAllMACs,
                                UIWizardExportApp::tr("Strip all network adapter MAC addresses"));
    m_pMACComboBox->setItemData(MACAddressPolicy_KeepAllMACs,
                                UIWizardExportApp::tr("Include all network adapter MAC addresses to exported "
                                                      "appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllNonNATMACs,
                                UIWizardExportApp::tr("Include only NAT network adapter MAC addresses to "
                                                      "exported appliance archive."), Qt::ToolTipRole);
    m_pMACComboBox->setItemData(MACAddressPolicy_StripAllMACs,
                                UIWizardExportApp::tr("Strip all network adapter MAC addresses from exported "
                                                      "appliance archive."), Qt::ToolTipRole);

    /* Translate addtional stuff: */
    m_pAdditionalLabel->setText(UIWizardExportApp::tr("Additionally:"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("&Write Manifest file"));
    m_pIncludeISOsCheckbox->setToolTip(UIWizardExportApp::tr("Include ISO image files into exported VM archive."));
    m_pIncludeISOsCheckbox->setText(UIWizardExportApp::tr("&Include ISO image files"));

    /* Translate Account combo-box: */
    m_pAccountComboBoxLabel->setText(UIWizardExportApp::tr("&Account:"));
    for (int i = 0; i < m_pAccountComboBox->count(); ++i)
    {
        m_pAccountComboBox->setItemText(i, UIWizardExportApp::tr("%1: %2", "provider: profile")
            .arg(m_pAccountComboBox->itemData(i, ProviderName).toString())
            .arg(m_pAccountComboBox->itemData(i, ProfileName).toString()));
    }

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

    /* Update tool-tips: */
    updateFormatComboToolTip();
    updateMACAddressPolicyComboToolTip();
}

void UIWizardExportAppPageExpert::initializePage()
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

    /* Refresh appliance settings: */
    refreshApplianceSettingsWidget();
}

bool UIWizardExportAppPageExpert::isComplete() const
{
    bool fResult = true;

    /* There should be at least one vm selected: */
    if (fResult)
        fResult = (m_pVMSelector->selectedItems().size() > 0);

    /* Check appliance settings: */
    if (fResult)
    {
        const QString &strFile = field("path").toString().toLower();

        const bool fOVF =    field("format").toString() == "ovf-0.9"
                          || field("format").toString() == "ovf-1.0"
                          || field("format").toString() == "ovf-2.0";
        const bool fOPC =    field("format").toString() == "opc-1.0";
        const bool fCSP =    field("format").toString() == "csp-1.0";

        fResult =    (   fOVF
                      && VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
                  || (   fOPC
                      && VBoxGlobal::hasAllowedExtension(strFile, OPCFileExts))
                  || fCSP;
    }

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
    /* Refresh required settings: */
    refreshFileSelectorName();
    refreshApplianceSettingsWidget();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleFormatComboChange()
{
    /* Update tool-tip: */
    updateFormatComboToolTip();

    /* Refresh required settings: */
    updatePageAppearance();
    refreshFileSelectorExtension();
    refreshManifestCheckBoxAccess();
    refreshIncludeISOsCheckBoxAccess();
    refreshApplianceSettingsWidget();
}

void UIWizardExportAppPageExpert::sltHandleMACAddressPolicyComboChange()
{
    /* Update tool-tip: */
    updateMACAddressPolicyComboToolTip();
}

void UIWizardExportAppPageExpert::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
}
