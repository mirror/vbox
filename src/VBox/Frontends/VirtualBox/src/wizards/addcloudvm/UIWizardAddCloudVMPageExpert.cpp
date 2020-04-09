/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageExpert class implementation.
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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QTableWidget>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageExpert.h"


UIWizardAddCloudVMPageExpert::UIWizardAddCloudVMPageExpert()
    : m_pCntSource(0)
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
                        m_pAccountInstanceList->setSelectionMode(QAbstractItemView::ExtendedSelection);

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
    }

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardAddCloudVMPageExpert::sltHandleSourceChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardAddCloudVMPageExpert::sltHandleSourceChange);
    connect(m_pAccountComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardAddCloudVMPageExpert::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardAddCloudVMPageExpert::sltHandleAccountButtonClick);
    connect(m_pAccountInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardAddCloudVMPageExpert::completeChanged);

    /* Register fields: */
    registerField("instanceIds", this, "instanceIds");
}

bool UIWizardAddCloudVMPageExpert::event(QEvent *pEvent)
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

void UIWizardAddCloudVMPageExpert::retranslateUi()
{
    /* Translate source container: */
    m_pCntSource->setTitle(UIWizardAddCloudVM::tr("Source"));

    /* Translate received values of Source combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pSourceComboBox->count(); ++i)
    {
        m_pSourceComboBox->setItemText(i, m_pSourceComboBox->itemData(i, SourceData_Name).toString());
        m_pSourceComboBox->setItemData(i, UIWizardAddCloudVM::tr("Add VM from cloud service provider."), Qt::ToolTipRole);
    }

    /* Update tool-tips: */
    updateSourceComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardAddCloudVMPageExpert::initializePage()
{
    /* If wasn't polished yet: */
    if (!UIWizardAddCloudVMPage1::m_fPolished)
    {
        /* Populate sources: */
        populateSources();
        /* Choose one of them, asynchronously: */
        QMetaObject::invokeMethod(this, "sltHandleSourceChange", Qt::QueuedConnection);
        UIWizardAddCloudVMPage1::m_fPolished = true;
    }

    /* Translate page: */
    retranslateUi();
}

bool UIWizardAddCloudVMPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Make sure client is not NULL and
     * at least one instance is selected: */
    fResult =   UIWizardAddCloudVMPage1::client().isNotNull()
            && !UIWizardAddCloudVMPage1::instanceIds().isEmpty();

    /* Return result: */
    return fResult;
}

bool UIWizardAddCloudVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to add cloud VMs: */
    fResult = qobject_cast<UIWizardAddCloudVM*>(wizard())->addCloudVMs();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardAddCloudVMPageExpert::sltHandleSourceChange()
{
    /* Update tool-tip: */
    updateSourceComboToolTip();

    /* Make instance list focused by default: */
    m_pAccountInstanceList->setFocus();

    /* Refresh required settings: */
    populateAccounts();
    populateAccountProperties();
    populateAccountInstances();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
    populateAccountInstances();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleAccountButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}
