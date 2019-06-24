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


UIWizardNewCloudVMPageExpert::UIWizardNewCloudVMPageExpert()
    : m_pCntDestination(0)
    , m_pSettingsCnt(0)
{
    /* Create main layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    if (pMainLayout)
    {
        /* Create destination container: */
        m_pCntDestination = new QGroupBox(this);
        if (m_pCntDestination)
        {
            /* Create destination layout: */
            m_pDestinationLayout = new QGridLayout(m_pCntDestination);
            if (m_pDestinationLayout)
            {
                /* Create destination selector: */
                m_pDestinationComboBox = new QIComboBox(m_pCntDestination);
                if (m_pDestinationComboBox)
                {
                    /* Add into layout: */
                    m_pDestinationLayout->addWidget(m_pDestinationComboBox, 0, 0);
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
                        m_pAccountComboBox = new QIComboBox(m_pCntDestination);
                        if (m_pAccountComboBox)
                        {
                            /* Add into layout: */
                            pSubLayout->addWidget(m_pAccountComboBox);
                        }
                        /* Create account tool-button: */
                        m_pAccountToolButton = new QIToolButton(m_pCntDestination);
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
                    m_pAccountPropertyTable = new QTableWidget(m_pCntDestination);
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
                    m_pAccountImageList = new QListWidget(m_pCntDestination);
                    if (m_pAccountImageList)
                    {
                        const QFontMetrics fm(m_pAccountImageList->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pAccountImageList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        //m_pAccountImageList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pAccountImageList->setAlternatingRowColors(true);

                        /* Add into layout: */
                        m_pCloudContainerLayout->addWidget(m_pAccountImageList, 2, 0);
                    }

                    /* Add into layout: */
                    m_pDestinationLayout->addLayout(m_pCloudContainerLayout, 1, 0);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntDestination);
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

    /* Populate destinations: */
    populateDestinations();

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardNewCloudVMPageExpert::sltHandleDestinationChange);
    connect(m_pDestinationComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageExpert::sltHandleDestinationChange);
    connect(m_pAccountComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountButtonClick);
    connect(m_pAccountImageList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleInstanceListChange);

    /* Register fields: */
    registerField("client", this, "client");
    registerField("vsd", this, "vsd");
    registerField("vsdForm", this, "vsdForm");
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
    /* Translate destination container: */
    m_pCntDestination->setTitle(UIWizardNewCloudVM::tr("Destination"));

    /* Translate received values of Destination combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pDestinationComboBox->count(); ++i)
    {
        m_pDestinationComboBox->setItemText(i, m_pDestinationComboBox->itemData(i, DestinationData_Name).toString());
        m_pDestinationComboBox->setItemData(i, UIWizardNewCloudVM::tr("Create VM for cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate settings container: */
    m_pSettingsCnt->setTitle(UIWizardNewCloudVM::tr("Settings"));

    /* Update tool-tips: */
    updateDestinationComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardNewCloudVMPageExpert::initializePage()
{
    /* If wasn't polished yet: */
    if (!m_fPolished)
    {
        QMetaObject::invokeMethod(this, "sltHandleDestinationChange", Qt::QueuedConnection);
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
    fResult =    client().isNotNull()
              && !imageId().isNull()
              && vsd().isNotNull()
              && vsdForm().isNotNull();

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

    /* Give changed VSD back: */
    if (fResult)
    {
        comForm.GetVirtualSystemDescription();
        fResult = comForm.isOk();
        if (!fResult)
            msgCenter().cannotAcquireVirtualSystemDescriptionFormProperty(comForm);
    }

    /* Try to create cloud VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewCloudVM*>(wizard())->createCloudVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageExpert::sltHandleDestinationChange()
{
    /* Update tool-tip: */
    updateDestinationComboToolTip();

    /* Make image list focused by default: */
    m_pAccountImageList->setFocus();

    /* Refresh required settings: */
    populateAccounts();
    populateAccountProperties();
    populateAccountImages();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
    populateAccountImages();
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
    /* Refresh required settings: */
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}
