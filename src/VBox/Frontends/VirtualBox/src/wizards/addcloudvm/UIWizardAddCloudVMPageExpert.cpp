/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageExpert class implementation.
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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QTableWidget>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UICloudNetworkingStuff.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageExpert.h"

/* Namespaces: */
using namespace UIWizardAddCloudVMPage1;


UIWizardAddCloudVMPageExpert::UIWizardAddCloudVMPageExpert()
    : m_pCntProvider(0)
    , m_pProviderLayout(0)
    , m_pProviderLabel(0)
    , m_pProviderComboBox(0)
    , m_pOptionsLayout(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pSourceInstanceLabel(0)
    , m_pSourceInstanceList(0)
{
    /* Prepare main layout: */
    QHBoxLayout *pLayoutMain = new QHBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare provider container: */
        m_pCntProvider = new QGroupBox(this);
        if (m_pCntProvider)
        {
            /* Prepare provider layout: */
            m_pProviderLayout = new QGridLayout(m_pCntProvider);
            if (m_pProviderLayout)
            {
                /* Prepare provider selector: */
                m_pProviderComboBox = new QIComboBox(m_pCntProvider);
                if (m_pProviderComboBox)
                    m_pProviderLayout->addWidget(m_pProviderComboBox, 0, 0);

                /* Prepare options layout: */
                m_pOptionsLayout = new QGridLayout;
                if (m_pOptionsLayout)
                {
                    m_pOptionsLayout->setContentsMargins(0, 0, 0, 0);
                    m_pOptionsLayout->setRowStretch(1, 1);

                    /* Prepare sub-layout: */
                    QHBoxLayout *pSubLayout = new QHBoxLayout;
                    if (pSubLayout)
                    {
                        pSubLayout->setContentsMargins(0, 0, 0, 0);
                        pSubLayout->setSpacing(1);

                        /* Prepare profile combo-box: */
                        m_pProfileComboBox = new QIComboBox(m_pCntProvider);
                        if (m_pProfileComboBox)
                            pSubLayout->addWidget(m_pProfileComboBox);

                        /* Prepare profile tool-button: */
                        m_pProfileToolButton = new QIToolButton(m_pCntProvider);
                        if (m_pProfileToolButton)
                        {
                            m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                              ":/cloud_profile_manager_disabled_16px.png"));
                            pSubLayout->addWidget(m_pProfileToolButton);
                        }

                        /* Add into layout: */
                        m_pOptionsLayout->addLayout(pSubLayout, 0, 0);
                    }

                    /* Prepare source instances table: */
                    m_pSourceInstanceList = new QListWidget(m_pCntProvider);
                    if (m_pSourceInstanceList)
                    {
                        /* Make source image list fit 50 symbols
                         * horizontally and 8 lines vertically: */
                        const QFontMetrics fm(m_pSourceInstanceList->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 8 * iFontHeight;
                        m_pSourceInstanceList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                        m_pSourceInstanceList->setAlternatingRowColors(true);
                        m_pSourceInstanceList->setSelectionMode(QAbstractItemView::ExtendedSelection);

                        /* Add into layout: */
                        m_pOptionsLayout->addWidget(m_pSourceInstanceList, 1, 0);
                    }

                    /* Add into layout: */
                    m_pProviderLayout->addLayout(m_pOptionsLayout, 1, 0);
                }
            }

            /* Add into layout: */
            pLayoutMain->addWidget(m_pCntProvider);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardAddCloudVMPageExpert::sltHandleProviderComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardAddCloudVMPageExpert::sltHandleProviderComboChange);
    connect(m_pProviderComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardAddCloudVMPageExpert::sltHandleProviderComboChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardAddCloudVMPageExpert::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardAddCloudVMPageExpert::sltHandleProfileButtonClick);
    connect(m_pSourceInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardAddCloudVMPageExpert::completeChanged);
}

void UIWizardAddCloudVMPageExpert::retranslateUi()
{
    /* Translate source container: */
    m_pCntProvider->setTitle(UIWizardAddCloudVM::tr("Source"));

    /* Translate profile stuff: */
    m_pProfileToolButton->setToolTip(UIWizardAddCloudVM::tr("Open Cloud Profile Manager..."));

    /* Translate received values of Source combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        m_pProviderComboBox->setItemText(i, m_pProviderComboBox->itemData(i, ProviderData_Name).toString());
        m_pProviderComboBox->setItemData(i, UIWizardAddCloudVM::tr("Add VM from cloud service provider."), Qt::ToolTipRole);
    }

    /* Update tool-tips: */
    updateComboToolTip(m_pProviderComboBox);
}

void UIWizardAddCloudVMPageExpert::initializePage()
{
    /* Populate providers: */
    populateProviders(m_pProviderComboBox);
    /* Translate providers: */
    retranslateUi();
    /* Fetch it, asynchronously: */
    QMetaObject::invokeMethod(this, "sltHandleProviderComboChange", Qt::QueuedConnection);
    /* Make image list focused by default: */
    m_pSourceInstanceList->setFocus();
}

bool UIWizardAddCloudVMPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Make sure client is not NULL and
     * at least one instance is selected: */
    fResult =    client().isNotNull()
              && !instanceIds().isEmpty();

    /* Return result: */
    return fResult;
}

bool UIWizardAddCloudVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Try to add cloud VMs: */
    fResult = qobject_cast<UIWizardAddCloudVM*>(wizard())->addCloudVMs();

    /* Return result: */
    return fResult;
}

void UIWizardAddCloudVMPageExpert::sltHandleProviderComboChange()
{
    updateProvider();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleProfileComboChange()
{
    updateProfile();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::sltHandleProfileButtonClick()
{
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardAddCloudVMPageExpert::sltHandleSourceInstanceChange()
{
    updateSourceInstance();
    emit completeChanged();
}

void UIWizardAddCloudVMPageExpert::setShortProviderName(const QString &strShortProviderName)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setShortProviderName(strShortProviderName);
}

QString UIWizardAddCloudVMPageExpert::shortProviderName() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->shortProviderName();
}

void UIWizardAddCloudVMPageExpert::setProfileName(const QString &strProfileName)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setProfileName(strProfileName);
}

QString UIWizardAddCloudVMPageExpert::profileName() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->profileName();
}

void UIWizardAddCloudVMPageExpert::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setClient(comClient);
}

CCloudClient UIWizardAddCloudVMPageExpert::client() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->client();
}

void UIWizardAddCloudVMPageExpert::setInstanceIds(const QStringList &instanceIds)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setInstanceIds(instanceIds);
}

QStringList UIWizardAddCloudVMPageExpert::instanceIds() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->instanceIds();
}

void UIWizardAddCloudVMPageExpert::updateProvider()
{
    updateComboToolTip(m_pProviderComboBox);
    setShortProviderName(m_pProviderComboBox->currentData(ProviderData_ShortName).toString());
    CCloudProvider comCloudProvider = cloudProviderByShortName(shortProviderName(), wizard());
    populateProfiles(m_pProfileComboBox, comCloudProvider);
    updateProfile();
}

void UIWizardAddCloudVMPageExpert::updateProfile()
{
    setProfileName(m_pProfileComboBox->currentData(ProfileData_Name).toString());
    setClient(cloudClientByName(shortProviderName(), profileName(), wizard()));
    populateProfileInstances(m_pSourceInstanceList, client());
    updateSourceInstance();
}

void UIWizardAddCloudVMPageExpert::updateSourceInstance()
{
    setInstanceIds(currentListWidgetData(m_pSourceInstanceList));
}
