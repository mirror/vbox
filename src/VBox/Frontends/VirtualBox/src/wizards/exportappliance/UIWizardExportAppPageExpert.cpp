/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageExpert class implementation.
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIApplianceExportEditorWidget.h"
#include "UIConverter.h"
#include "UIEmptyFilePathSelector.h"
#include "UIFormEditorWidget.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageBasic1.h"
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportAppPageBasic3.h"
#include "UIWizardExportAppPageExpert.h"

/* Namespaces: */
using namespace UIWizardExportAppPage1;
using namespace UIWizardExportAppPage2;
using namespace UIWizardExportAppPage3;


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageExpert implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageExpert::UIWizardExportAppPageExpert(const QStringList &selectedVMNames, bool fExportToOCIByDefault)
    : m_selectedVMNames(selectedVMNames)
    , m_fExportToOCIByDefault(fExportToOCIByDefault)
    , m_pSelectorCnt(0)
    , m_pVMSelector(0)
    , m_pSettingsCnt(0)
    , m_pFormatLayout(0)
    , m_pFormatComboBoxLabel(0)
    , m_pFormatComboBox(0)
    , m_pSettingsWidget1(0)
    , m_pSettingsLayout1(0)
    , m_pFileSelectorLabel(0)
    , m_pFileSelector(0)
    , m_pMACComboBoxLabel(0)
    , m_pMACComboBox(0)
    , m_pAdditionalLabel(0)
    , m_pManifestCheckbox(0)
    , m_pIncludeISOsCheckbox(0)
    , m_pSettingsLayout2(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pExportModeLabel(0)
    , m_pExportModeButtonGroup(0)
    , m_pApplianceCnt(0)
    , m_pSettingsWidget2(0)
    , m_pApplianceWidget(0)
    , m_pFormEditor(0)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setRowStretch(0, 1);

        /* Create VM selector container: */
        m_pSelectorCnt = new QGroupBox(this);
        if (m_pSelectorCnt)
        {
            /* Create VM selector container layout: */
            QVBoxLayout *pSelectorCntLayout = new QVBoxLayout(m_pSelectorCnt);
            if (pSelectorCntLayout)
            {
                /* Create VM selector: */
                m_pVMSelector = new QListWidget(m_pSelectorCnt);
                if (m_pVMSelector)
                {
                    m_pVMSelector->setAlternatingRowColors(true);
                    m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);
                    pSelectorCntLayout->addWidget(m_pVMSelector);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSelectorCnt, 0, 0);
        }

        /* Create settings widget container: */
        m_pSettingsCnt = new QGroupBox(this);
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
                    m_pFormatComboBox = new QIComboBox(m_pSettingsCnt);
                    if (m_pFormatComboBox)
                        m_pFormatLayout->addWidget(m_pFormatComboBox, 0, 1);
                    /* Create format combo-box label: */
                    m_pFormatComboBoxLabel = new QLabel(m_pSettingsCnt);
                    if (m_pFormatComboBoxLabel)
                    {
                        m_pFormatComboBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                        m_pFormatComboBoxLabel->setBuddy(m_pFormatComboBox);
                        m_pFormatLayout->addWidget(m_pFormatComboBoxLabel, 0, 0);
                    }

                    /* Add into layout: */
                    pSettingsCntLayout->addLayout(m_pFormatLayout);
                }

                /* Create settings widget: */
                m_pSettingsWidget1 = new QStackedWidget;
                if (m_pSettingsWidget1)
                {
                    /* Create settings pane 1: */
                    QWidget *pSettingsPane1 = new QWidget(m_pSettingsWidget1);
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
                            m_pMACComboBox = new QIComboBox;
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
                        m_pSettingsWidget1->addWidget(pSettingsPane1);
                    }

                    /* Create settings pane 2: */
                    QWidget *pSettingsPane2 = new QWidget(m_pSettingsWidget1);
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

                            /* Create profile label: */
                            m_pProfileLabel = new QLabel(pSettingsPane2);
                            if (m_pProfileLabel)
                            {
                                m_pProfileLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                                m_pSettingsLayout2->addWidget(m_pProfileLabel, 0, 0);
                            }
                            /* Create sub-layout: */
                            QHBoxLayout *pSubLayout = new QHBoxLayout;
                            if (pSubLayout)
                            {
                                pSubLayout->setContentsMargins(0, 0, 0, 0);
                                pSubLayout->setSpacing(1);

                                /* Create profile combo-box: */
                                m_pProfileComboBox = new QIComboBox(pSettingsPane2);
                                if (m_pProfileComboBox)
                                {
                                    m_pProfileLabel->setBuddy(m_pProfileComboBox);
                                    pSubLayout->addWidget(m_pProfileComboBox);
                                }
                                /* Create profile tool-button: */
                                m_pProfileToolButton = new QIToolButton(pSettingsPane2);
                                if (m_pProfileToolButton)
                                {
                                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                                      ":/cloud_profile_manager_disabled_16px.png"));
                                    pSubLayout->addWidget(m_pProfileToolButton);
                                }

                                /* Add into layout: */
                                m_pSettingsLayout2->addLayout(pSubLayout, 0, 1);
                            }

                            /* Create profile label: */
                            m_pExportModeLabel = new QLabel(pSettingsPane2);
                            if (m_pExportModeLabel)
                            {
                                m_pExportModeLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                                m_pSettingsLayout2->addWidget(m_pExportModeLabel, 1, 0);
                            }

                            /* Create button-group: */
                            m_pExportModeButtonGroup = new QButtonGroup(pSettingsPane2);
                            if (m_pExportModeButtonGroup)
                            {
                                /* Create Do Not Ask button: */
                                m_exportModeButtons[CloudExportMode_DoNotAsk] = new QRadioButton(pSettingsPane2);
                                if (m_exportModeButtons.value(CloudExportMode_DoNotAsk))
                                {
                                    m_pExportModeButtonGroup->addButton(m_exportModeButtons.value(CloudExportMode_DoNotAsk));
                                    m_pSettingsLayout2->addWidget(m_exportModeButtons.value(CloudExportMode_DoNotAsk), 1, 1);
                                }
                                /* Create Ask Then Export button: */
                                m_exportModeButtons[CloudExportMode_AskThenExport] = new QRadioButton(pSettingsPane2);
                                if (m_exportModeButtons.value(CloudExportMode_AskThenExport))
                                {
                                    m_pExportModeButtonGroup->addButton(m_exportModeButtons.value(CloudExportMode_AskThenExport));
                                    m_pSettingsLayout2->addWidget(m_exportModeButtons.value(CloudExportMode_AskThenExport), 2, 1);
                                }
                                /* Create Export Then Ask button: */
                                m_exportModeButtons[CloudExportMode_ExportThenAsk] = new QRadioButton(pSettingsPane2);
                                if (m_exportModeButtons.value(CloudExportMode_ExportThenAsk))
                                {
                                    m_pExportModeButtonGroup->addButton(m_exportModeButtons.value(CloudExportMode_ExportThenAsk));
                                    m_pSettingsLayout2->addWidget(m_exportModeButtons.value(CloudExportMode_ExportThenAsk), 3, 1);
                                }
                            }
                        }

                        /* Add into layout: */
                        m_pSettingsWidget1->addWidget(pSettingsPane2);
                    }

                    /* Add into layout: */
                    pSettingsCntLayout->addWidget(m_pSettingsWidget1);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt, 1, 0, 1, 2);
        }

        /* Create appliance widget container: */
        m_pApplianceCnt = new QGroupBox(this);
        if (m_pApplianceCnt)
        {
            /* Create appliance widget container layout: */
            QVBoxLayout *pApplianceCntLayout = new QVBoxLayout(m_pApplianceCnt);
            if (pApplianceCntLayout)
            {
                /* Create settings widget 2: */
                m_pSettingsWidget2 = new QStackedWidget;
                if (m_pSettingsWidget2)
                {
                    /* Create appliance widget container: */
                    QWidget *pApplianceWidgetCnt = new QWidget(this);
                    if (pApplianceWidgetCnt)
                    {
                        /* Create appliance widget layout: */
                        QVBoxLayout *pApplianceWidgetLayout = new QVBoxLayout(pApplianceWidgetCnt);
                        if (pApplianceWidgetLayout)
                        {
                            pApplianceWidgetLayout->setContentsMargins(0, 0, 0, 0);

                            /* Create appliance widget: */
                            m_pApplianceWidget = new UIApplianceExportEditorWidget(pApplianceWidgetCnt);
                            if (m_pApplianceWidget)
                            {
                                m_pApplianceWidget->setMinimumHeight(250);
                                m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                                pApplianceWidgetLayout->addWidget(m_pApplianceWidget);
                            }
                        }

                        /* Add into layout: */
                        m_pSettingsWidget2->addWidget(pApplianceWidgetCnt);
                    }

                    /* Create form editor container: */
                    QWidget *pFormEditorCnt = new QWidget(this);
                    if (pFormEditorCnt)
                    {
                        /* Create form editor layout: */
                        QVBoxLayout *pFormEditorLayout = new QVBoxLayout(pFormEditorCnt);
                        if (pFormEditorLayout)
                        {
                            pFormEditorLayout->setContentsMargins(0, 0, 0, 0);

                            /* Create form editor widget: */
                            m_pFormEditor = new UIFormEditorWidget(pFormEditorCnt);
                            if (m_pFormEditor)
                                pFormEditorLayout->addWidget(m_pFormEditor);
                        }

                        /* Add into layout: */
                        m_pSettingsWidget2->addWidget(pFormEditorCnt);
                    }

                    /* Add into layout: */
                    pApplianceCntLayout->addWidget(m_pSettingsWidget2);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pApplianceCnt, 0, 1);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);
    connect(m_pVMSelector, &QListWidget::itemSelectionChanged,
            this, &UIWizardExportAppPageExpert::sltHandleVMItemSelectionChanged);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged,
            this, &UIWizardExportAppPageExpert::sltHandleFileSelectorChange);
    connect(m_pFormatComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleFormatComboChange);
    connect(m_pMACComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleMACAddressExportPolicyComboChange);
    connect(m_pManifestCheckbox, &QCheckBox::stateChanged,
            this, &UIWizardExportAppPageExpert::sltHandleManifestCheckBoxChange);
    connect(m_pIncludeISOsCheckbox, &QCheckBox::stateChanged,
            this, &UIWizardExportAppPageExpert::sltHandleIncludeISOsCheckBoxChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardExportAppPageExpert::sltHandleProfileComboChange);
    connect(m_pExportModeButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*, bool)>(&QButtonGroup::buttonToggled),
            this, &UIWizardExportAppPageExpert::sltHandleRadioButtonToggled);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardExportAppPageExpert::sltHandleProfileButtonClick);
}

UIWizardExportApp *UIWizardExportAppPageExpert::wizard() const
{
    return qobject_cast<UIWizardExportApp*>(UINativeWizardPage::wizard());
}

void UIWizardExportAppPageExpert::retranslateUi()
{
    /* Translate objects: */
    m_strDefaultApplianceName = UIWizardExportApp::tr("Appliance");
    refreshFileSelectorName(m_strFileSelectorName, wizard()->machineNames(), m_strDefaultApplianceName, wizard()->isFormatCloudOne());
    refreshFileSelectorPath(m_pFileSelector, m_strFileSelectorName, m_strFileSelectorExt, wizard()->isFormatCloudOne());

    /* Translate group-boxes: */
    m_pSelectorCnt->setTitle(UIWizardExportApp::tr("Virtual &machines to export"));
    m_pApplianceCnt->setTitle(UIWizardExportApp::tr("Virtual &system settings"));
    m_pSettingsCnt->setTitle(UIWizardExportApp::tr("Appliance settings"));

    /* Translate File selector: */
    m_pFileSelectorLabel->setText(UIWizardExportApp::tr("&File:"));
    m_pFileSelector->setChooseButtonToolTip(UIWizardExportApp::tr("Choose a file to export the virtual appliance to..."));
    m_pFileSelector->setFileDialogTitle(UIWizardExportApp::tr("Please choose a file to export the virtual appliance to"));

    /* Translate hard-coded values of Format combo-box: */
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
        if (isFormatCloudOne(m_pFormatComboBox, i))
        {
            m_pFormatComboBox->setItemText(i, m_pFormatComboBox->itemData(i, FormatData_Name).toString());
            m_pFormatComboBox->setItemData(i, UIWizardExportApp::tr("Export to cloud service provider."), Qt::ToolTipRole);
        }

    /* Translate MAC address policy combo-box: */
    m_pMACComboBoxLabel->setText(UIWizardExportApp::tr("MAC Address &Policy:"));
    for (int i = 0; i < m_pMACComboBox->count(); ++i)
    {
        const MACAddressExportPolicy enmPolicy = m_pMACComboBox->itemData(i).value<MACAddressExportPolicy>();
        switch (enmPolicy)
        {
            case MACAddressExportPolicy_KeepAllMACs:
            {
                m_pMACComboBox->setItemText(i, UIWizardExportApp::tr("Include all network adapter MAC addresses"));
                m_pMACComboBox->setItemData(i, UIWizardExportApp::tr("Include all network adapter MAC addresses in exported "
                                                                     "appliance archive."), Qt::ToolTipRole);
                break;
            }
            case MACAddressExportPolicy_StripAllNonNATMACs:
            {
                m_pMACComboBox->setItemText(i, UIWizardExportApp::tr("Include only NAT network adapter MAC addresses"));
                m_pMACComboBox->setItemData(i, UIWizardExportApp::tr("Include only NAT network adapter MAC addresses in exported "
                                                                     "appliance archive."), Qt::ToolTipRole);
                break;
            }
            case MACAddressExportPolicy_StripAllMACs:
            {
                m_pMACComboBox->setItemText(i, UIWizardExportApp::tr("Strip all network adapter MAC addresses"));
                m_pMACComboBox->setItemData(i, UIWizardExportApp::tr("Strip all network adapter MAC addresses from exported "
                                                                     "appliance archive."), Qt::ToolTipRole);
                break;
            }
            default:
                break;
        }
    }

    /* Translate addtional stuff: */
    m_pAdditionalLabel->setText(UIWizardExportApp::tr("Additionally:"));
    m_pManifestCheckbox->setToolTip(UIWizardExportApp::tr("Create a Manifest file for automatic data integrity checks on import."));
    m_pManifestCheckbox->setText(UIWizardExportApp::tr("&Write Manifest file"));
    m_pIncludeISOsCheckbox->setToolTip(UIWizardExportApp::tr("Include ISO image files into exported VM archive."));
    m_pIncludeISOsCheckbox->setText(UIWizardExportApp::tr("&Include ISO image files"));

    /* Translate profile stuff: */
    m_pProfileLabel->setText(UIWizardExportApp::tr("&Profile:"));
    m_pProfileToolButton->setToolTip(UIWizardExportApp::tr("Open Cloud Profile Manager..."));

    /* Translate option label: */
    m_pExportModeLabel->setText(UIWizardExportApp::tr("Machine Creation:"));
    m_exportModeButtons.value(CloudExportMode_DoNotAsk)->setText(UIWizardExportApp::tr("Do not ask me about it, leave custom &image for future usage"));
    m_exportModeButtons.value(CloudExportMode_AskThenExport)->setText(UIWizardExportApp::tr("Ask me about it &before exporting disk as custom image"));
    m_exportModeButtons.value(CloudExportMode_ExportThenAsk)->setText(UIWizardExportApp::tr("Ask me about it &after exporting disk as custom image"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pFormatComboBoxLabel;
    labels << m_pFileSelectorLabel;
    labels << m_pMACComboBoxLabel;
    labels << m_pAdditionalLabel;
    labels << m_pProfileLabel;
    labels << m_pExportModeLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pFormatLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pSettingsLayout1->setColumnMinimumWidth(0, iMaxWidth);
    m_pSettingsLayout2->setColumnMinimumWidth(0, iMaxWidth);

    /* Update tool-tips: */
    updateFormatComboToolTip(m_pFormatComboBox);
    updateMACAddressExportPolicyComboToolTip(m_pMACComboBox);
}

void UIWizardExportAppPageExpert::initializePage()
{
    /* Populate VM items: */
    populateVMItems(m_pVMSelector, m_selectedVMNames);
    /* Populate formats: */
    populateFormats(m_pFormatComboBox, m_fExportToOCIByDefault);
    /* Populate MAC address policies: */
    populateMACAddressPolicies(m_pMACComboBox);
    /* Translate page: */
    retranslateUi();

    /* Fetch it, asynchronously: */
    QMetaObject::invokeMethod(this, "sltHandleFormatComboChange", Qt::QueuedConnection);
}

bool UIWizardExportAppPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* There should be at least one vm selected: */
    if (fResult)
        fResult = wizard()->machineNames().size() > 0;

    /* Check appliance settings: */
    if (fResult)
    {
        const bool fOVF =    wizard()->format() == "ovf-0.9"
                          || wizard()->format() == "ovf-1.0"
                          || wizard()->format() == "ovf-2.0";
        const bool fCSP =    wizard()->isFormatCloudOne();

        fResult =    (   fOVF
                      && UICommon::hasAllowedExtension(wizard()->path().toLower(), OVFFileExts))
                  || (   fCSP
                      && wizard()->cloudAppliance().isNotNull()
                      && wizard()->cloudClient().isNotNull()
                      && wizard()->vsd().isNotNull()
                      && wizard()->vsdExportForm().isNotNull());
    }

    /* Return result: */
    return fResult;
}

bool UIWizardExportAppPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Check whether there was cloud target selected: */
    if (wizard()->isFormatCloudOne())
    {
        /* Make sure table has own data committed: */
        m_pFormEditor->makeSureEditorDataCommitted();

        /* Check whether we have proper VSD form: */
        CVirtualSystemDescriptionForm comForm = wizard()->vsdExportForm();
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
    /* Otherwise if there was local target selected: */
    else
    {
        /* Ask user about machines which are in Saved state currently: */
        QStringList savedMachines;
        refreshSavedMachines(savedMachines, m_pVMSelector);
        if (!savedMachines.isEmpty())
            fResult = msgCenter().confirmExportMachinesInSaveState(savedMachines, this);

        /* Prepare export: */
        if (fResult)
        {
            m_pApplianceWidget->prepareExport();
            wizard()->setLocalAppliance(*m_pApplianceWidget->appliance());
        }
    }

    /* Try to export appliance: */
    if (fResult)
        fResult = wizard()->exportAppliance();

    /* Return result: */
    return fResult;
}

void UIWizardExportAppPageExpert::sltHandleVMItemSelectionChanged()
{
    updateMachines();
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleFormatComboChange()
{
    updateFormat();
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleFileSelectorChange()
{
    /* Skip empty paths: */
    if (m_pFileSelector->path().isEmpty())
        return;

    m_strFileSelectorName = QFileInfo(m_pFileSelector->path()).completeBaseName();
    wizard()->setPath(m_pFileSelector->path());
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleMACAddressExportPolicyComboChange()
{
    updateMACAddressExportPolicyComboToolTip(m_pMACComboBox);
    wizard()->setMACAddressExportPolicy(m_pMACComboBox->currentData().value<MACAddressExportPolicy>());
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleManifestCheckBoxChange()
{
    wizard()->setManifestSelected(m_pManifestCheckbox->isChecked());
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleIncludeISOsCheckBoxChange()
{
    wizard()->setIncludeISOsSelected(m_pIncludeISOsCheckbox->isChecked());
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleProfileComboChange()
{
    updateProfile();
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleRadioButtonToggled(QAbstractButton *pButton, bool fToggled)
{
    /* Handle checked buttons only: */
    if (!fToggled)
        return;

    /* Update cloud export mode field value: */
    wizard()->setCloudExportMode(m_exportModeButtons.key(pButton));
    emit completeChanged();
}

void UIWizardExportAppPageExpert::sltHandleProfileButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardExportAppPageExpert::updateMachines()
{
    /* Update wizard fields: */
    wizard()->setMachineNames(machineNames(m_pVMSelector));
    wizard()->setMachineIDs(machineIDs(m_pVMSelector));

    /* Refresh required settings: */
    refreshFileSelectorName(m_strFileSelectorName, wizard()->machineNames(), m_strDefaultApplianceName, wizard()->isFormatCloudOne());
    refreshFileSelectorPath(m_pFileSelector, m_strFileSelectorName, m_strFileSelectorExt, wizard()->isFormatCloudOne());

    /* Update cloud stuff: */
    updateCloudStuff();
}

void UIWizardExportAppPageExpert::updateFormat()
{
    /* Update combo tool-tip: */
    updateFormatComboToolTip(m_pFormatComboBox);

    /* Update wizard fields: */
    wizard()->setFormat(format(m_pFormatComboBox));
    wizard()->setFormatCloudOne(isFormatCloudOne(m_pFormatComboBox));

    /* Refresh settings widget state: */
    UIWizardExportAppPage2::refreshStackedWidget(m_pSettingsWidget1, wizard()->isFormatCloudOne());
    UIWizardExportAppPage3::refreshStackedWidget(m_pSettingsWidget2, wizard()->isFormatCloudOne());

    /* Update export settings: */
    refreshFileSelectorExtension(m_strFileSelectorExt, m_pFileSelector, wizard()->isFormatCloudOne());
    refreshFileSelectorPath(m_pFileSelector, m_strFileSelectorName, m_strFileSelectorExt, wizard()->isFormatCloudOne());
    refreshManifestCheckBoxAccess(m_pManifestCheckbox, wizard()->isFormatCloudOne());
    refreshIncludeISOsCheckBoxAccess(m_pIncludeISOsCheckbox, wizard()->isFormatCloudOne());
    refreshProfileCombo(m_pProfileComboBox, wizard()->format(), wizard()->isFormatCloudOne());
    refreshCloudExportMode(m_exportModeButtons, wizard()->isFormatCloudOne());

    /* Update profile: */
    updateProfile();
}

void UIWizardExportAppPageExpert::updateProfile()
{
    /* Update wizard fields: */
    wizard()->setProfileName(profileName(m_pProfileComboBox));

    /* Update export settings: */
    refreshCloudProfile(m_comCloudProfile,
                        wizard()->format(),
                        wizard()->profileName(),
                        wizard()->isFormatCloudOne());

    /* Update cloud stuff: */
    updateCloudStuff();
}

void UIWizardExportAppPageExpert::updateCloudStuff()
{
    /* Create appliance, client, VSD and VSD export form: */
    CAppliance comAppliance;
    CCloudClient comClient;
    CVirtualSystemDescription comDescription;
    CVirtualSystemDescriptionForm comForm;
    refreshCloudStuff(comAppliance,
                      comClient,
                      comDescription,
                      comForm,
                      m_comCloudProfile,
                      wizard()->machineIDs(),
                      wizard()->uri(),
                      wizard()->cloudExportMode());
    wizard()->setCloudAppliance(comAppliance);
    wizard()->setCloudClient(comClient);
    wizard()->setVsd(comDescription);
    wizard()->setVsdExportForm(comForm);

    /* Refresh corresponding widgets: */
    refreshApplianceSettingsWidget(m_pApplianceWidget, wizard()->machineIDs(), wizard()->uri(), wizard()->isFormatCloudOne());
    refreshFormPropertiesTable(m_pFormEditor, wizard()->vsdExportForm(), wizard()->isFormatCloudOne());
}
