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
#include "UICommon.h"
#include "UIApplianceImportEditorWidget.h"
#include "UIEmptyFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageExpert.h"


UIWizardImportAppPageExpert::UIWizardImportAppPageExpert(bool fImportFromOCIByDefault, const QString &strFileName)
    : UIWizardImportAppPage1(fImportFromOCIByDefault)
    , m_pCntSource(0)
    , m_pSettingsCnt(0)
{
    /* Create main layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    if (pMainLayout)
    {
        /* Create source container: */
        m_pCntSource = new QGroupBox(this);
        if (m_pCntSource)
        {
            /* Create source layout: */
            m_pSourceLayout = new QGridLayout(m_pCntSource);
            if (m_pSourceLayout)
            {
                /* Create source selector: */
                m_pSourceComboBox = new QIComboBox(m_pCntSource);
                if (m_pSourceComboBox)
                {
                    /* Add into layout: */
                    m_pSourceLayout->addWidget(m_pSourceComboBox, 0, 0);
                }

                /* Create stacked layout: */
                m_pStackedLayout = new QStackedLayout;
                if (m_pStackedLayout)
                {
                    /* Create local container: */
                    QWidget *pLocalContainer = new QWidget(m_pCntSource);
                    if (pLocalContainer)
                    {
                        /* Create local container layout: */
                        m_pLocalContainerLayout = new QGridLayout(pLocalContainer);
                        if (m_pLocalContainerLayout)
                        {
                            m_pLocalContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pLocalContainerLayout->setRowStretch(1, 1);

                            /* Create file-path selector: */
                            m_pFileSelector = new UIEmptyFilePathSelector(pLocalContainer);
                            if (m_pFileSelector)
                            {
                                m_pFileSelector->setHomeDir(uiCommon().documentsPath());
                                m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
                                m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                                m_pFileSelector->setEditable(true);

                                /* Add into layout: */
                                m_pLocalContainerLayout->addWidget(m_pFileSelector, 0, 0);
                            }
                        }

                        /* Add into layout: */
                        m_pStackedLayout->addWidget(pLocalContainer);
                    }

                    /* Create cloud container: */
                    QWidget *pCloudContainer = new QWidget(m_pCntSource);
                    if (pCloudContainer)
                    {
                        /* Create cloud container layout: */
                        m_pCloudContainerLayout = new QGridLayout(pCloudContainer);
                        if (m_pCloudContainerLayout)
                        {
                            m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                            m_pCloudContainerLayout->setRowStretch(1, 1);

                            /* Create sub-layout: */
                            QHBoxLayout *pSubLayout = new QHBoxLayout;
                            if (pSubLayout)
                            {
                                pSubLayout->setContentsMargins(0, 0, 0, 0);
                                pSubLayout->setSpacing(1);

                                /* Create profile combo-box: */
                                m_pProfileComboBox = new QIComboBox(pCloudContainer);
                                if (m_pProfileComboBox)
                                {
                                    /* Add into layout: */
                                    pSubLayout->addWidget(m_pProfileComboBox);
                                }

                                /* Create profile tool-button: */
                                m_pProfileToolButton = new QIToolButton(pCloudContainer);
                                if (m_pProfileToolButton)
                                {
                                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                                      ":/cloud_profile_manager_disabled_16px.png"));

                                    /* Add into layout: */
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

                                /* Add into layout: */
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

        /* Create settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Create settings container layout: */
            m_pSettingsCntLayout = new QStackedLayout(m_pSettingsCnt);
            if (m_pSettingsCntLayout)
            {
                /* Create appliance widget container: */
                QWidget *pApplianceWidgetCnt = new QWidget(m_pSettingsCnt);
                if (pApplianceWidgetCnt)
                {
                    /* Create appliance widget layout: */
                    QVBoxLayout *pApplianceWidgetLayout = new QVBoxLayout(pApplianceWidgetCnt);
                    if (pApplianceWidgetLayout)
                    {
                        /* Create appliance widget: */
                        m_pApplianceWidget = new UIApplianceImportEditorWidget(pApplianceWidgetCnt);
                        if (m_pApplianceWidget)
                        {
                            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                            m_pApplianceWidget->setFile(strFileName);

                            /* Add into layout: */
                            pApplianceWidgetLayout->addWidget(m_pApplianceWidget);
                        }
                    }

                    /* Add into layout: */
                    m_pSettingsCntLayout->addWidget(pApplianceWidgetCnt);
                }

                /* Create form editor container: */
                QWidget *pFormEditorCnt = new QWidget(m_pSettingsCnt);
                if (pFormEditorCnt)
                {
                    /* Create form editor layout: */
                    QVBoxLayout *pFormEditorLayout = new QVBoxLayout(pFormEditorCnt);
                    if (pFormEditorLayout)
                    {
                        /* Create form editor widget: */
                        m_pFormEditor = new UIFormEditorWidget(pFormEditorCnt);
                        if (m_pFormEditor)
                        {
                            /* Add into layout: */
                            pFormEditorLayout->addWidget(m_pFormEditor);
                        }
                    }

                    /* Add into layout: */
                    m_pSettingsCntLayout->addWidget(pFormEditorCnt);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt);
        }
    }

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

    /* Register classes: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register fields: */
    registerField("source", this, "source");
    registerField("isSourceCloudOne", this, "isSourceCloudOne");
    registerField("profile", this, "profile");
    registerField("appliance", this, "appliance");
    registerField("vsdForm", this, "vsdForm");
    registerField("machineId", this, "machineId");
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardImportAppPageExpert::retranslateUi()
{
    /* Translate appliance container: */
    m_pCntSource->setTitle(UIWizardImportApp::tr("Source"));

    /* Translate hardcoded values of Source combo-box: */
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

    /* Translate file selector: */
    m_pFileSelector->setChooseButtonToolTip(UIWizardImportApp::tr("Choose a virtual appliance file to import..."));
    m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Please choose a virtual appliance file to import"));
    m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));

    /* Translate profile stuff: */
    m_pProfileToolButton->setToolTip(UIWizardImportApp::tr("Open Cloud Profile Manager..."));

    /* Translate settings container: */
    m_pSettingsCnt->setTitle(UIWizardImportApp::tr("Settings"));

    /* Update page appearance: */
    updatePageAppearance();

    /* Update tool-tips: */
    updateSourceComboToolTip();
}

void UIWizardImportAppPageExpert::initializePage()
{
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
                      && !m_comAppliance.isNull()
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
