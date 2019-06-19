/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageExpert class implementation.
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
#include <QGroupBox>
#include <QHeaderView>
#include <QListWidget>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageExpert.h"

/* COM includes: */
#include "CVirtualSystemDescription.h"


UIWizardNewCloudVMPageExpert::UIWizardNewCloudVMPageExpert(bool fImportFromOCIByDefault)
    : UIWizardNewCloudVMPage1(fImportFromOCIByDefault)
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

                /* Create cloud container layout: */
                m_pCloudContainerLayout = new QGridLayout;
                if (m_pCloudContainerLayout)
                {
                    m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                    m_pCloudContainerLayout->setRowStretch(3, 1);

                    /* Create sub-layout: */
                    QHBoxLayout *pSubLayout = new QHBoxLayout;
                    if (pSubLayout)
                    {
                        pSubLayout->setContentsMargins(0, 0, 0, 0);
                        pSubLayout->setSpacing(1);

                        /* Create account combo-box: */
                        m_pAccountComboBox = new QIComboBox(m_pCntSource);
                        if (m_pAccountComboBox)
                        {
                            /* Add into layout: */
                            pSubLayout->addWidget(m_pAccountComboBox);
                        }
                        /* Create account tool-button: */
                        m_pAccountToolButton = new QIToolButton(m_pCntSource);
                        if (m_pAccountToolButton)
                        {
                            m_pAccountToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                              ":/cloud_profile_manager_disabled_16px.png"));

                            /* Add into layout: */
                            pSubLayout->addWidget(m_pAccountToolButton);
                        }

                        /* Add into layout: */
                        m_pCloudContainerLayout->addLayout(pSubLayout, 0, 0);
                    }

                    /* Create profile property table: */
                    m_pAccountPropertyTable = new QTableWidget(m_pCntSource);
                    if (m_pAccountPropertyTable)
                    {
                        const QFontMetrics fm(m_pAccountPropertyTable->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pAccountPropertyTable->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        //m_pAccountPropertyTable->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pAccountPropertyTable->setAlternatingRowColors(true);
                        m_pAccountPropertyTable->horizontalHeader()->setVisible(false);
                        m_pAccountPropertyTable->verticalHeader()->setVisible(false);
                        m_pAccountPropertyTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

                        /* Add into layout: */
                        m_pCloudContainerLayout->addWidget(m_pAccountPropertyTable, 1, 0);
                    }

                    /* Create profile instances table: */
                    m_pAccountInstanceList = new QListWidget(m_pCntSource);
                    if (m_pAccountInstanceList)
                    {
                        const QFontMetrics fm(m_pAccountInstanceList->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pAccountInstanceList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        //m_pAccountInstanceList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pAccountInstanceList->setAlternatingRowColors(true);

                        /* Add into layout: */
                        m_pCloudContainerLayout->addWidget(m_pAccountInstanceList, 2, 0);
                    }

                    /* Add into layout: */
                    m_pSourceLayout->addLayout(m_pCloudContainerLayout, 1, 0);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntSource);
        }

        /* Create settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Create form editor layout: */
            QVBoxLayout *pFormEditorLayout = new QVBoxLayout(m_pSettingsCnt);
            if (pFormEditorLayout)
            {
                /* Create form editor widget: */
                m_pFormEditor = new UIFormEditorWidget(m_pSettingsCnt);
                if (m_pFormEditor)
                {
                    /* Add into layout: */
                    pFormEditorLayout->addWidget(m_pFormEditor);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt);
        }
    }

    /* Populate sources: */
    populateSources();

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardNewCloudVMPageExpert::sltHandleSourceChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageExpert::sltHandleSourceChange);
    connect(m_pAccountComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountButtonClick);
    connect(m_pAccountInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleInstanceListChange);

    /* Register fields: */
    registerField("source", this, "source");
    registerField("profile", this, "profile");
    registerField("appliance", this, "appliance");
    registerField("vsdForm", this, "vsdForm");
    registerField("machineId", this, "machineId");
}

bool UIWizardNewCloudVMPageExpert::event(QEvent *pEvent)
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

void UIWizardNewCloudVMPageExpert::retranslateUi()
{
    /* Translate appliance container: */
    m_pCntSource->setTitle(UIWizardNewCloudVM::tr("Source"));

    /* Translate received values of Source combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pSourceComboBox->count(); ++i)
    {
        m_pSourceComboBox->setItemText(i, m_pSourceComboBox->itemData(i, SourceData_Name).toString());
        m_pSourceComboBox->setItemData(i, UIWizardNewCloudVM::tr("Import from cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate settings container: */
    m_pSettingsCnt->setTitle(UIWizardNewCloudVM::tr("Settings"));

    /* Update tool-tips: */
    updateSourceComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardNewCloudVMPageExpert::initializePage()
{
    /* If wasn't polished yet: */
    if (!m_fPolished)
    {
        QMetaObject::invokeMethod(this, "sltHandleSourceChange", Qt::QueuedConnection);
        m_fPolished = true;
    }

    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewCloudVMPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    !m_comCloudProfile.isNull()
              && !m_comCloudClient.isNull()
              && !machineId().isNull()
              && !m_comAppliance.isNull()
              && !m_comVSDForm.isNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

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

    /* Try to import appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewCloudVM*>(wizard())->importAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageExpert::sltHandleSourceChange()
{
    /* Update tool-tip: */
    updateSourceComboToolTip();

    /* Refresh required settings: */
    populateAccounts();
    populateAccountProperties();
    populateAccountInstances();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
    populateAccountInstances();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleAccountButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardNewCloudVMPageExpert::sltHandleInstanceListChange()
{
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}
