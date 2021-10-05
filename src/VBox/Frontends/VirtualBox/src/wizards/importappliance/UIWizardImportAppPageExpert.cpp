/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageExpert class implementation.
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
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QStackedLayout>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UIApplianceImportEditorWidget.h"
#include "UICommon.h"
#include "UIEmptyFilePathSelector.h"
#include "UIFilePathSelector.h"
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
                /* Prepare source selector: */
                m_pSourceComboBox = new QIComboBox(m_pCntSource);
                if (m_pSourceComboBox)
                    m_pSourceLayout->addWidget(m_pSourceComboBox, 0, 0);

                /* Prepare stacked layout: */
                m_pStackedLayout = new QStackedLayout;
                if (m_pStackedLayout)
                {
                    /* Prepare local container: */
                    QWidget *pLocalContainer = new QWidget(m_pCntSource);
                    if (pLocalContainer)
                    {
                        /* Prepare local container layout: */
                        m_pLocalContainerLayout = new QGridLayout(pLocalContainer);
                        if (m_pLocalContainerLayout)
                        {
                            m_pLocalContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pLocalContainerLayout->setRowStretch(1, 1);

                            /* Prepare file-path selector: */
                            m_pFileSelector = new UIEmptyFilePathSelector(pLocalContainer);
                            if (m_pFileSelector)
                            {
                                m_pFileSelector->setHomeDir(uiCommon().documentsPath());
                                m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
                                m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                                m_pFileSelector->setEditable(true);
                                m_pLocalContainerLayout->addWidget(m_pFileSelector, 0, 0);
                            }
                        }

                        /* Add into layout: */
                        m_pStackedLayout->addWidget(pLocalContainer);
                    }

                    /* Prepare cloud container: */
                    QWidget *pCloudContainer = new QWidget(m_pCntSource);
                    if (pCloudContainer)
                    {
                        /* Prepare cloud container layout: */
                        m_pCloudContainerLayout = new QGridLayout(pCloudContainer);
                        if (m_pCloudContainerLayout)
                        {
                            m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pCloudContainerLayout->setRowStretch(1, 1);

                            /* Prepare sub-layout: */
                            QHBoxLayout *pSubLayout = new QHBoxLayout;
                            if (pSubLayout)
                            {
                                pSubLayout->setContentsMargins(0, 0, 0, 0);
                                pSubLayout->setSpacing(1);

                                /* Prepare profile combo-box: */
                                m_pProfileComboBox = new QIComboBox(pCloudContainer);
                                if (m_pProfileComboBox)
                                    pSubLayout->addWidget(m_pProfileComboBox);

                                /* Prepare profile tool-button: */
                                m_pProfileToolButton = new QIToolButton(pCloudContainer);
                                if (m_pProfileToolButton)
                                {
                                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                                      ":/cloud_profile_manager_disabled_16px.png"));
                                    pSubLayout->addWidget(m_pProfileToolButton);
                                }

                                /* Add into layout: */
                                m_pCloudContainerLayout->addLayout(pSubLayout, 0, 0);
                            }

                            /* Create profile instances table: */
                            m_pProfileInstanceList = new QListWidget(pCloudContainer);
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

                        /* Add into layout: */
                        m_pStackedLayout->addWidget(pCloudContainer);
                    }

                    /* Add into layout: */
                    m_pSourceLayout->addLayout(m_pStackedLayout, 1, 0);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntSource);
        }

        /* Prepare settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Prepare settings container layout: */
            m_pSettingsCntLayout = new QStackedLayout(m_pSettingsCnt);
            if (m_pSettingsCntLayout)
            {
                /* Prepare appliance widget container: */
                QWidget *pApplianceWidgetCnt = new QWidget(m_pSettingsCnt);
                if (pApplianceWidgetCnt)
                {
                    /* Prepare appliance widget layout: */
                    QGridLayout *pApplianceWidgetLayout = new QGridLayout(pApplianceWidgetCnt);
                    if (pApplianceWidgetLayout)
                    {
                        /* Prepare appliance widget: */
                        m_pApplianceWidget = new UIApplianceImportEditorWidget(pApplianceWidgetCnt);
                        if (m_pApplianceWidget)
                        {
                            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                            m_pApplianceWidget->setFile(strFileName);
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
                    }

                    /* Add into layout: */
                    m_pSettingsCntLayout->addWidget(pApplianceWidgetCnt);
                }

                /* Prepare form editor container: */
                QWidget *pFormEditorCnt = new QWidget(m_pSettingsCnt);
                if (pFormEditorCnt)
                {
                    /* Prepare form editor layout: */
                    QVBoxLayout *pFormEditorLayout = new QVBoxLayout(pFormEditorCnt);
                    if (pFormEditorLayout)
                    {
                        /* Prepare form editor widget: */
                        m_pFormEditor = new UIFormEditorWidget(pFormEditorCnt);
                        if (m_pFormEditor)
                            pFormEditorLayout->addWidget(m_pFormEditor);
                    }

                    /* Add into layout: */
                    m_pSettingsCntLayout->addWidget(pFormEditorCnt);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardImportAppPageExpert::sltHandleSourceChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardImportAppPageExpert::sltHandleSourceChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardImportAppPageExpert::sltHandleSourceChange);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageExpert::sltFilePathChangeHandler);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageExpert::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardImportAppPageExpert::sltHandleProfileButtonClick);
    connect(m_pProfileInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardImportAppPageExpert::sltHandleInstanceListChange);
    connect(m_pEditorImportFilePath, &UIFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageExpert::sltHandlePathChanged);
    connect(m_pComboMACImportPolicy, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageExpert::sltHandleMACImportPolicyChange);

    /* Register classes: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register fields: */
    registerField("isSourceCloudOne", this, "isSourceCloudOne");
    registerField("profile", this, "profile");
    registerField("appliance", this, "appliance");
    registerField("vsdForm", this, "vsdForm");
    registerField("machineId", this, "machineId");
    registerField("applianceWidget", this, "applianceWidget");
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
}

bool UIWizardImportAppPageExpert::isComplete() const
{
    bool fResult = true;

    /* Check appliance settings: */
    if (fResult)
    {
        const bool fOVF = !isSourceCloudOne();
        const bool fCSP = !fOVF;

        fResult =    (   fOVF
                      && UICommon::hasAllowedExtension(m_pFileSelector->path().toLower(), OVFFileExts)
                      && QFile::exists(m_pFileSelector->path())
                      && m_pApplianceWidget->isValid())
                  || (   fCSP
                      && !m_comCloudProfile.isNull()
                      && !m_comCloudClient.isNull()
                      && !machineId().isNull()
                      && !m_comCloudAppliance.isNull()
                      && !m_comVSDForm.isNull());
    }

    return fResult;
}

bool UIWizardImportAppPageExpert::validatePage()
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

void UIWizardImportAppPageExpert::sltHandleSourceChange()
{
    /* Update tool-tip: */
    updateSourceComboToolTip();

    /* Refresh required settings: */
    updatePageAppearance();
    populateProfiles();
    populateProfile();
    populateProfileInstances();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltFilePathChangeHandler()
{
    /* Check if set file contains valid appliance: */
    if (   QFile::exists(m_pFileSelector->path())
        && m_pApplianceWidget->setFile(m_pFileSelector->path()))
    {
        /* Reset the modified bit if file was correctly set: */
        m_pFileSelector->resetModified();
    }

    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfile();
    populateProfileInstances();
    populateFormProperties();
    refreshFormPropertiesTable();
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
    emit completeChanged();
}

void UIWizardImportAppPageExpert::sltHandlePathChanged(const QString &strNewPath)
{
    m_pApplianceWidget->setVirtualSystemBaseFolder(strNewPath);
}

void UIWizardImportAppPageExpert::sltHandleMACImportPolicyChange()
{
    updateMACImportPolicyComboToolTip();
}
