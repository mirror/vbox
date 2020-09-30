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

                        /* Create profile combo-box: */
                        m_pProfileComboBox = new QIComboBox(m_pCntSource);
                        if (m_pProfileComboBox)
                        {
                            /* Add into layout: */
                            pSubLayout->addWidget(m_pProfileComboBox);
                        }
                        /* Create profile tool-button: */
                        m_pProfileToolButton = new QIToolButton(m_pCntSource);
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
                    m_pProfileInstanceList = new QListWidget(m_pCntSource);
                    if (m_pProfileInstanceList)
                    {
                        const QFontMetrics fm(m_pProfileInstanceList->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pProfileInstanceList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        //m_pProfileInstanceList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pProfileInstanceList->setAlternatingRowColors(true);
                        m_pProfileInstanceList->setSelectionMode(QAbstractItemView::ExtendedSelection);

                        /* Add into layout: */
                        m_pCloudContainerLayout->addWidget(m_pProfileInstanceList, 1, 0);
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
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardAddCloudVMPageExpert::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardAddCloudVMPageExpert::sltHandleProfileButtonClick);
    connect(m_pProfileInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardAddCloudVMPageExpert::completeChanged);

    /* Register fields: */
    registerField("source", this, "source");
    registerField("profileName", this, "profileName");
    registerField("instanceIds", this, "instanceIds");
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
    m_pProfileInstanceList->setFocus();

    /* Refresh required settings: */
    populateProfiles();
    populateProfile();
    populateProfileInstances();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfile();
    populateProfileInstances();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleProfileButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}
