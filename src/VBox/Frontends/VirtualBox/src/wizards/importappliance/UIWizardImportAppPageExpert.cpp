/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageExpert class implementation.
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
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UIApplianceImportEditorWidget.h"
#include "UICommon.h"
#include "UIEmptyFilePathSelector.h"
#include "UIFilePathSelector.h"
#include "UIFormEditorWidget.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageExpert.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardImportAppPageExpert::UIWizardImportAppPageExpert(bool fImportFromOCIByDefault, const QString &strFileName)
    : UIWizardImportAppPage1(fImportFromOCIByDefault)
    , m_strFileName(strFileName)
    , m_pCntSource(0)
    , m_pSettingsCnt(0)
{
    /* Prepare main layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    if (pMainLayout)
    {
        /* Prepare source container: */
        m_pCntSource = new QGroupBox(this);
        if (m_pCntSource)
        {
            /* Prepare source layout: */
            m_pSourceLayout = new QGridLayout(m_pCntSource);
            if (m_pSourceLayout)
            {
                /* Prepare source combo: */
                m_pSourceComboBox = new QIComboBox(m_pCntSource);
                if (m_pSourceComboBox)
                    m_pSourceLayout->addWidget(m_pSourceComboBox, 0, 0);

                /* Prepare settings widget 1: */
                m_pSettingsWidget1 = new QStackedWidget(m_pCntSource);
                if (m_pSettingsWidget1)
                {
                    /* Prepare local container: */
                    QWidget *pContainerLocal = new QWidget(m_pSettingsWidget1);
                    if (pContainerLocal)
                    {
                        /* Prepare local widget layout: */
                        m_pLocalContainerLayout = new QGridLayout(pContainerLocal);
                        if (m_pLocalContainerLayout)
                        {
                            m_pLocalContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pLocalContainerLayout->setRowStretch(1, 1);

                            /* Prepare file-path selector: */
                            m_pFileSelector = new UIEmptyFilePathSelector(pContainerLocal);
                            if (m_pFileSelector)
                            {
                                m_pFileSelector->setHomeDir(uiCommon().documentsPath());
                                m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
                                m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                                m_pFileSelector->setEditable(true);
                                m_pLocalContainerLayout->addWidget(m_pFileSelector, 0, 0);
                            }
                        }

                        /* Add into widget: */
                        m_pSettingsWidget1->addWidget(pContainerLocal);
                    }

                    /* Prepare cloud container: */
                    QWidget *pContainerCloud = new QWidget(m_pSettingsWidget1);
                    if (pContainerCloud)
                    {
                        /* Prepare cloud container layout: */
                        m_pCloudContainerLayout = new QGridLayout(pContainerCloud);
                        if (m_pCloudContainerLayout)
                        {
                            m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pCloudContainerLayout->setRowStretch(1, 1);

                            /* Prepare profile layout: */
                            QHBoxLayout *pLayoutProfile = new QHBoxLayout;
                            if (pLayoutProfile)
                            {
                                pLayoutProfile->setContentsMargins(0, 0, 0, 0);
                                pLayoutProfile->setSpacing(1);

                                /* Prepare profile combo-box: */
                                m_pProfileComboBox = new QIComboBox(pContainerCloud);
                                if (m_pProfileComboBox)
                                    pLayoutProfile->addWidget(m_pProfileComboBox);

                                /* Prepare profile tool-button: */
                                m_pProfileToolButton = new QIToolButton(pContainerCloud);
                                if (m_pProfileToolButton)
                                {
                                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                                      ":/cloud_profile_manager_disabled_16px.png"));
                                    pLayoutProfile->addWidget(m_pProfileToolButton);
                                }

                                /* Add into layout: */
                                m_pCloudContainerLayout->addLayout(pLayoutProfile, 0, 0);
                            }

                            /* Create profile instances table: */
                            m_pProfileInstanceList = new QListWidget(pContainerCloud);
                            if (m_pProfileInstanceList)
                            {
                                const QFontMetrics fm(m_pProfileInstanceList->font());
                                const int iFontWidth = fm.width('x');
                                const int iTotalWidth = 50 * iFontWidth;
                                const int iFontHeight = fm.height();
                                const int iTotalHeight = 4 * iFontHeight;
                                m_pProfileInstanceList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
//                                m_pProfileInstanceList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                                m_pProfileInstanceList->setAlternatingRowColors(true);
                                m_pCloudContainerLayout->addWidget(m_pProfileInstanceList, 1, 0);
                            }
                        }

                        /* Add into widget: */
                        m_pSettingsWidget1->addWidget(pContainerCloud);
                    }

                    /* Add into layout: */
                    m_pSourceLayout->addWidget(m_pSettingsWidget1, 1, 0);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntSource);
        }

        /* Prepare settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Prepare settings layout: */
            QVBoxLayout *pLayoutSettings = new QVBoxLayout(m_pSettingsCnt);
            if (pLayoutSettings)
            {
                /* Prepare settings widget 2: */
                m_pSettingsWidget2 = new QStackedWidget(m_pSettingsCnt);
                if (m_pSettingsWidget2)
                {
                    /* Prepare appliance container: */
                    QWidget *pContainerAppliance = new QWidget(m_pSettingsCnt);
                    if (pContainerAppliance)
                    {
                        /* Prepare appliance layout: */
                        QGridLayout *pLayoutAppliance = new QGridLayout(pContainerAppliance);
                        if (pLayoutAppliance)
                        {
                            /* Prepare appliance widget: */
                            m_pApplianceWidget = new UIApplianceImportEditorWidget(pContainerAppliance);
                            if (m_pApplianceWidget)
                            {
                                m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                                pLayoutAppliance->addWidget(m_pApplianceWidget, 0, 0, 1, 3);
                            }

                            /* Prepare import path label: */
                            m_pLabelImportFilePath = new QLabel(pContainerAppliance);
                            if (m_pLabelImportFilePath)
                            {
                                m_pLabelImportFilePath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                                pLayoutAppliance->addWidget(m_pLabelImportFilePath, 1, 0);
                            }
                            /* Prepare import path selector: */
                            m_pEditorImportFilePath = new UIFilePathSelector(pContainerAppliance);
                            if (m_pEditorImportFilePath)
                            {
                                m_pEditorImportFilePath->setResetEnabled(true);
                                m_pEditorImportFilePath->setDefaultPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
                                m_pEditorImportFilePath->setPath(uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
                                m_pLabelImportFilePath->setBuddy(m_pEditorImportFilePath);
                                pLayoutAppliance->addWidget(m_pEditorImportFilePath, 1, 1, 1, 2);
                            }

                            /* Prepare MAC address policy label: */
                            m_pLabelMACImportPolicy = new QLabel(pContainerAppliance);
                            if (m_pLabelMACImportPolicy)
                            {
                                m_pLabelMACImportPolicy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                                pLayoutAppliance->addWidget(m_pLabelMACImportPolicy, 2, 0);
                            }
                            /* Prepare MAC address policy combo: */
                            m_pComboMACImportPolicy = new QIComboBox(pContainerAppliance);
                            if (m_pComboMACImportPolicy)
                            {
                                m_pComboMACImportPolicy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                                m_pLabelMACImportPolicy->setBuddy(m_pComboMACImportPolicy);
                                pLayoutAppliance->addWidget(m_pComboMACImportPolicy, 2, 1, 1, 2);
                            }

                            /* Prepare additional options label: */
                            m_pLabelAdditionalOptions = new QLabel(pContainerAppliance);
                            if (m_pLabelAdditionalOptions)
                            {
                                m_pLabelAdditionalOptions->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                                pLayoutAppliance->addWidget(m_pLabelAdditionalOptions, 3, 0);
                            }
                            /* Prepare import HDs as VDIs checkbox: */
                            m_pCheckboxImportHDsAsVDI = new QCheckBox(pContainerAppliance);
                            {
                                m_pCheckboxImportHDsAsVDI->setCheckState(Qt::Checked);
                                pLayoutAppliance->addWidget(m_pCheckboxImportHDsAsVDI, 3, 1);
                            }
                        }

                        /* Add into layout: */
                        m_pSettingsWidget2->addWidget(pContainerAppliance);
                    }

                    /* Prepare form editor container: */
                    QWidget *pContainerFormEditor = new QWidget(m_pSettingsCnt);
                    if (pContainerFormEditor)
                    {
                        /* Prepare form editor layout: */
                        QVBoxLayout *pLayoutFormEditor = new QVBoxLayout(pContainerFormEditor);
                        if (pLayoutFormEditor)
                        {
                            /* Prepare form editor widget: */
                            m_pFormEditor = new UIFormEditorWidget(pContainerFormEditor);
                            if (m_pFormEditor)
                                pLayoutFormEditor->addWidget(m_pFormEditor);
                        }

                        /* Add into layout: */
                        m_pSettingsWidget2->addWidget(pContainerFormEditor);
                    }

                    /* Add into layout: */
                    pLayoutSettings->addWidget(m_pSettingsWidget2);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardImportAppPageExpert::sltHandleSourceComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardImportAppPageExpert::sltHandleSourceComboChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardImportAppPageExpert::sltHandleSourceComboChange);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageExpert::sltHandleImportedFileSelectorChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageExpert::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardImportAppPageExpert::sltHandleProfileButtonClick);
    connect(m_pProfileInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardImportAppPageExpert::sltHandleInstanceListChange);
    connect(m_pEditorImportFilePath, &UIFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageExpert::sltHandleImportPathEditorChange);
    connect(m_pComboMACImportPolicy, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageExpert::sltHandleMACImportPolicyComboChange);

    /* Register fields: */
    registerField("isSourceCloudOne", this, "isSourceCloudOne");
    registerField("profile", this, "profile");
    registerField("appliance", this, "appliance");
    registerField("vsdForm", this, "vsdForm");
    registerField("machineId", this, "machineId");
    registerField("macAddressImportPolicy", this, "macAddressImportPolicy");
    registerField("importHDsAsVDI", this, "importHDsAsVDI");
}

void UIWizardImportAppPageExpert::retranslateUi()
{
    /* Translate appliance container: */
    if (m_pCntSource)
        m_pCntSource->setTitle(UIWizardImportApp::tr("Source"));

    /* Translate hardcoded values of Source combo-box: */
    if (m_pSourceComboBox)
    {
        m_pSourceComboBox->setItemText(0, UIWizardImportApp::tr("Local File System"));
        m_pSourceComboBox->setItemData(0, UIWizardImportApp::tr("Import from local file system."), Qt::ToolTipRole);

        /* Translate received values of Source combo-box.
         * We are enumerating starting from 0 for simplicity: */
        for (int i = 0; i < m_pSourceComboBox->count(); ++i)
            if (isSourceCloudOne(i))
            {
                m_pSourceComboBox->setItemText(i, m_pSourceComboBox->itemData(i, SourceData_Name).toString());
                m_pSourceComboBox->setItemData(i, UIWizardImportApp::tr("Import from cloud service provider."), Qt::ToolTipRole);
            }
    }

    /* Translate file selector: */
    if (m_pFileSelector)
    {
        m_pFileSelector->setChooseButtonToolTip(UIWizardImportApp::tr("Choose a virtual appliance file to import..."));
        m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Please choose a virtual appliance file to import"));
        m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));
    }

    /* Translate profile stuff: */
    if (m_pProfileToolButton)
        m_pProfileToolButton->setToolTip(UIWizardImportApp::tr("Open Cloud Profile Manager..."));

    /* Translate settings container: */
    if (m_pSettingsCnt)
        m_pSettingsCnt->setTitle(UIWizardImportApp::tr("Settings"));

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

    /* Update page appearance: */
    updatePageAppearance();

    /* Update tool-tips: */
    updateSourceComboToolTip();
}

void UIWizardImportAppPageExpert::initializePage()
{
    /* Populate sources: */
    populateSources();
    /* Populate profiles: */
    populateProfiles();
    /* Populate profile: */
    populateProfile();
    /* Populate profile instances: */
    populateProfileInstances();
    /* Populate form properties: */
    populateFormProperties();
    refreshFormPropertiesTable();

    /* Populate MAC address import combo: */
    populateMACAddressImportPolicies();

    /* Translate page: */
    retranslateUi();

    /* If we have file name passed,
     * check if specified file contains valid appliance: */
    if (   !m_strFileName.isEmpty()
        && !qobject_cast<UIWizardImportApp*>(wizard())->setFile(m_strFileName))
    {
        wizard()->reject();
        return;
    }

    /* Acquire appliance: */
    CAppliance comAppliance = qobject_cast<UIWizardImportApp*>(wizard())->localAppliance();
    if (comAppliance.isNotNull())
    {
        /* Initialize appliance widget: */
        m_pApplianceWidget->setAppliance(comAppliance);
        /* Make sure we initialize appliance widget model with correct base folder path: */
        sltHandleImportPathEditorChange();
    }
}

bool UIWizardImportAppPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check whether there was cloud source selected: */
    if (isSourceCloudOne())
        fResult =    m_comCloudAppliance.isNotNull()
                  && m_comVSDForm.isNotNull();
    else
        fResult = qobject_cast<UIWizardImportApp*>(wizard())->isValid();

    /* Return result: */
    return fResult;
}

bool UIWizardImportAppPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Check whether there was cloud source selected: */
    if (fieldImp("isSourceCloudOne").toBool())
    {
        /* Make sure table has own data committed: */
        m_pFormEditor->makeSureEditorDataCommitted();

        /* Check whether we have proper VSD form: */
        CVirtualSystemDescriptionForm comForm = fieldImp("vsdForm").value<CVirtualSystemDescriptionForm>();
        fResult = comForm.isNotNull();

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

void UIWizardImportAppPageExpert::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardImportAppPage1::updatePageAppearance();
    UIWizardImportAppPage2::updatePageAppearance();

    /* Update page appearance according to chosen storage-type: */
    if (isSourceCloudOne())
        m_pProfileInstanceList->setFocus();
    else
        m_pFileSelector->setFocus();
}

void UIWizardImportAppPageExpert::sltHandleSourceComboChange()
{
    /* Update combo tool-tip: */
    updateSourceComboToolTip();

    /* Refresh required settings: */
    updatePageAppearance();
    populateProfiles();
    populateProfile();
    populateProfileInstances();
    populateFormProperties();
    refreshFormPropertiesTable();

    /* Notify about changes: */
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandleImportedFileSelectorChange()
{
    /* Update local stuff (only if something changed): */
    if (m_pFileSelector->isModified())
    {
        /* Create local appliance: */
        qobject_cast<UIWizardImportApp*>(wizard())->setFile(m_pFileSelector->path());
        m_pFileSelector->resetModified();
    }

    /* Acquire appliance: */
    CAppliance comAppliance = qobject_cast<UIWizardImportApp*>(wizard())->localAppliance();
    /* Initialize appliance widget: */
    m_pApplianceWidget->setAppliance(comAppliance);
    /* Make sure we initialize appliance widget model with correct base folder path: */
    sltHandleImportPathEditorChange();

    /* Notify about changes: */
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfile();
    populateProfileInstances();
    populateFormProperties();
    refreshFormPropertiesTable();

    /* Notify about changes: */
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandleProfileButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardImportAppPageExpert::sltHandleInstanceListChange()
{
    populateFormProperties();
    refreshFormPropertiesTable();

    /* Notify about changes: */
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandleImportPathEditorChange()
{
    AssertPtrReturnVoid(m_pApplianceWidget);
    AssertPtrReturnVoid(m_pEditorImportFilePath);
    m_pApplianceWidget->setVirtualSystemBaseFolder(m_pEditorImportFilePath->path());
}

void UIWizardImportAppPageExpert::sltHandleMACImportPolicyComboChange()
{
    updateMACImportPolicyComboToolTip();
}
