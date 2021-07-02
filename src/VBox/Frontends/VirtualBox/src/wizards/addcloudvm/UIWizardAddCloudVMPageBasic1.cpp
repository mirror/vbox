/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageBasic1 class implementation.
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
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICloudNetworkingStuff.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageBasic1.h"

/* COM includes: */
#include "CStringArray.h"

/* Namespaces: */
using namespace UIWizardAddCloudVMPage1;


/*********************************************************************************************************************************
*   Namespace UIWizardAddCloudVMPage1 implementation.                                                                            *
*********************************************************************************************************************************/

void UIWizardAddCloudVMPage1::populateProviders(QIComboBox *pCombo)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pCombo);
    /* We need top-level parent as well: */
    QWidget *pParent = pCombo->window();
    AssertPtrReturnVoid(pParent);

    /* Block signals while updating: */
    pCombo->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (pCombo->currentIndex() != -1)
        strOldData = pCombo->itemData(pCombo->currentIndex(), ProviderData_ShortName).toString();
    /* Otherwise "OCI" should be the default one: */
    else
        strOldData = "OCI";

    /* Clear combo initially: */
    pCombo->clear();

    /* Iterate through existing providers: */
    foreach (const CCloudProvider &comProvider, listCloudProviders(pParent))
    {
        /* Skip if we have nothing to populate (file missing?): */
        if (comProvider.isNull())
            continue;
        /* Acquire provider name: */
        QString strProviderName;
        if (!cloudProviderName(comProvider, strProviderName, pParent))
            continue;
        /* Acquire provider short name: */
        QString strProviderShortName;
        if (!cloudProviderShortName(comProvider, strProviderShortName, pParent))
            continue;

        /* Compose empty item, fill the data: */
        pCombo->addItem(QString());
        pCombo->setItemData(pCombo->count() - 1, strProviderName,      ProviderData_Name);
        pCombo->setItemData(pCombo->count() - 1, strProviderShortName, ProviderData_ShortName);
    }

    /* Set previous/default item if possible: */
    int iNewIndex = -1;
    if (   iNewIndex == -1
        && !strOldData.isNull())
        iNewIndex = pCombo->findData(strOldData, ProviderData_ShortName);
    if (   iNewIndex == -1
        && pCombo->count() > 0)
        iNewIndex = 0;
    if (iNewIndex != -1)
        pCombo->setCurrentIndex(iNewIndex);

    /* Unblock signals after update: */
    pCombo->blockSignals(false);
}

void UIWizardAddCloudVMPage1::populateProfiles(QIComboBox *pCombo, const CCloudProvider &comProvider)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pCombo);
    AssertReturnVoid(comProvider.isNotNull());
    /* We need top-level parent as well: */
    QWidget *pParent = pCombo->window();
    AssertPtrReturnVoid(pParent);

    /* Block signals while updating: */
    pCombo->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (pCombo->currentIndex() != -1)
        strOldData = pCombo->itemData(pCombo->currentIndex(), ProfileData_Name).toString();
    else
    {
        /* Try to fetch "old" profile name from wizard full group name: */
        UIWizardAddCloudVM *pWizard = qobject_cast<UIWizardAddCloudVM*>(pParent);
        AssertPtrReturnVoid(pWizard);
        const QString strFullGroupName = pWizard->fullGroupName();
        const QString strProfileName = strFullGroupName.section('/', 2, 2);
        if (!strProfileName.isEmpty())
            strOldData = strProfileName;
    }

    /* Clear combo initially: */
    pCombo->clear();

    /* Iterate through existing profiles: */
    foreach (const CCloudProfile &comProfile, listCloudProfiles(comProvider, pParent))
    {
        /* Skip if we have nothing to populate (wtf happened?): */
        if (comProfile.isNull())
            continue;
        /* Acquire profile name: */
        QString strProfileName;
        if (!cloudProfileName(comProfile, strProfileName, pParent))
            continue;

        /* Compose item, fill the data: */
        pCombo->addItem(strProfileName);
        pCombo->setItemData(pCombo->count() - 1, strProfileName, ProfileData_Name);
    }

    /* Set previous/default item if possible: */
    int iNewIndex = -1;
    if (   iNewIndex == -1
        && !strOldData.isNull())
        iNewIndex = pCombo->findData(strOldData, ProfileData_Name);
    if (   iNewIndex == -1
        && pCombo->count() > 0)
        iNewIndex = 0;
    if (iNewIndex != -1)
        pCombo->setCurrentIndex(iNewIndex);

    /* Unblock signals after update: */
    pCombo->blockSignals(false);
}

void UIWizardAddCloudVMPage1::populateProfileInstances(QListWidget *pList, const CCloudClient &comClient)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pList);
    AssertReturnVoid(comClient.isNotNull());
    /* We need top-level parent as well: */
    QWidget *pParent = pList->window();
    AssertPtrReturnVoid(pParent);

    /* Block signals while updating: */
    pList->blockSignals(true);

    /* Clear list initially: */
    pList->clear();

    /* Gather instance names and ids: */
    CStringArray comNames;
    CStringArray comIDs;
    if (listCloudSourceInstances(comClient, comNames, comIDs, pParent))
    {
        /* Push acquired names to list rows: */
        const QVector<QString> names = comNames.GetValues();
        const QVector<QString> ids = comIDs.GetValues();
        for (int i = 0; i < names.size(); ++i)
        {
            /* Create list item: */
            QListWidgetItem *pItem = new QListWidgetItem(names.at(i), pList);
            if (pItem)
            {
                pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
                pItem->setData(Qt::UserRole, ids.at(i));
            }
        }
    }

    /* Choose the 1st one by default if possible: */
    if (pList->count())
        pList->setCurrentRow(0);

    /* Unblock signals after update: */
    pList->blockSignals(false);
}

void UIWizardAddCloudVMPage1::updateComboToolTip(QIComboBox *pCombo)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pCombo);

    const int iCurrentIndex = pCombo->currentIndex();
    if (iCurrentIndex != -1)
    {
        const QString strCurrentToolTip = pCombo->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
        AssertMsg(!strCurrentToolTip.isEmpty(), ("Tool-tip data not found!\n"));
        pCombo->setToolTip(strCurrentToolTip);
    }
}

QStringList UIWizardAddCloudVMPage1::currentListWidgetData(QListWidget *pList)
{
    QStringList result;
    foreach (QListWidgetItem *pItem, pList->selectedItems())
        result << pItem->data(Qt::UserRole).toString();
    return result;
}


/*********************************************************************************************************************************
*   Class UIWizardAddCloudVMPageBasic1 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardAddCloudVMPageBasic1::UIWizardAddCloudVMPageBasic1()
    : m_pLabelMain(0)
    , m_pProviderLayout(0)
    , m_pProviderLabel(0)
    , m_pProviderComboBox(0)
    , m_pLabelDescription(0)
    , m_pOptionsLayout(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pSourceInstanceLabel(0)
    , m_pSourceInstanceList(0)
{
    /* Prepare main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare main label: */
        m_pLabelMain = new QIRichTextLabel(this);
        if (m_pLabelMain)
            pLayoutMain->addWidget(m_pLabelMain);

        /* Prepare provider layout: */
        m_pProviderLayout = new QGridLayout;
        if (m_pProviderLayout)
        {
            m_pProviderLayout->setContentsMargins(0, 0, 0, 0);
            m_pProviderLayout->setColumnStretch(0, 0);
            m_pProviderLayout->setColumnStretch(1, 1);

            /* Prepare provider label: */
            m_pProviderLabel = new QLabel(this);
            if (m_pProviderLabel)
                m_pProviderLayout->addWidget(m_pProviderLabel, 0, 0, Qt::AlignRight);
            /* Prepare provider combo-box: */
            m_pProviderComboBox = new QIComboBox(this);
            if (m_pProviderComboBox)
            {
                m_pProviderLabel->setBuddy(m_pProviderComboBox);
                m_pProviderLayout->addWidget(m_pProviderComboBox, 0, 1);
            }

            /* Add into layout: */
            pLayoutMain->addLayout(m_pProviderLayout);
        }

        /* Prepare description label: */
        m_pLabelDescription = new QIRichTextLabel(this);
        if (m_pLabelDescription)
            pLayoutMain->addWidget(m_pLabelDescription);

        /* Prepare options layout: */
        m_pOptionsLayout = new QGridLayout;
        if (m_pOptionsLayout)
        {
            m_pOptionsLayout->setContentsMargins(0, 0, 0, 0);
            m_pOptionsLayout->setColumnStretch(0, 0);
            m_pOptionsLayout->setColumnStretch(1, 1);
            m_pOptionsLayout->setRowStretch(1, 0);
            m_pOptionsLayout->setRowStretch(2, 1);

            /* Prepare profile label: */
            m_pProfileLabel = new QLabel(this);
            if (m_pProfileLabel)
                m_pOptionsLayout->addWidget(m_pProfileLabel, 0, 0, Qt::AlignRight);

            /* Prepare profile layout: */
            QHBoxLayout *pProfileLayout = new QHBoxLayout;
            if (pProfileLayout)
            {
                pProfileLayout->setContentsMargins(0, 0, 0, 0);
                pProfileLayout->setSpacing(1);

                /* Prepare profile combo-box: */
                m_pProfileComboBox = new QIComboBox(this);
                if (m_pProfileComboBox)
                {
                    m_pProfileLabel->setBuddy(m_pProfileComboBox);
                    pProfileLayout->addWidget(m_pProfileComboBox);
                }

                /* Prepare profile tool-button: */
                m_pProfileToolButton = new QIToolButton(this);
                if (m_pProfileToolButton)
                {
                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                      ":/cloud_profile_manager_disabled_16px.png"));
                    pProfileLayout->addWidget(m_pProfileToolButton);
                }

                /* Add into layout: */
                m_pOptionsLayout->addLayout(pProfileLayout, 0, 1);
            }

            /* Prepare source instance label: */
            m_pSourceInstanceLabel = new QLabel(this);
            if (m_pSourceInstanceLabel)
                m_pOptionsLayout->addWidget(m_pSourceInstanceLabel, 1, 0, Qt::AlignRight);

            /* Prepare source instances table: */
            m_pSourceInstanceList = new QListWidget(this);
            if (m_pSourceInstanceList)
            {
                m_pSourceInstanceLabel->setBuddy(m_pSourceInstanceLabel);
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
                m_pOptionsLayout->addWidget(m_pSourceInstanceList, 1, 1, 2, 1);
            }

            /* Add into layout: */
            pLayoutMain->addLayout(m_pOptionsLayout);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(m_pProviderComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProfileButtonClick);
    connect(m_pSourceInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardAddCloudVMPageBasic1::sltHandleSourceInstanceChange);
}

void UIWizardAddCloudVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardAddCloudVM::tr("Source to add from"));

    /* Translate main label: */
    m_pLabelMain->setText(UIWizardAddCloudVM::tr("Please choose the source to add cloud virtual machine from.  This can "
                                                 "be one of known cloud service providers below."));

    /* Translate provider label: */
    m_pProviderLabel->setText(UIWizardAddCloudVM::tr("&Source:"));
    /* Translate received values of Source combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        m_pProviderComboBox->setItemText(i, m_pProviderComboBox->itemData(i, ProviderData_Name).toString());
        m_pProviderComboBox->setItemData(i, UIWizardAddCloudVM::tr("Add VM from cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate description label: */
    m_pLabelDescription->setText(UIWizardAddCloudVM::tr("<p>Please choose one of cloud service profiles you have registered to "
                                                        "add virtual machine from.  Existing instance list will be "
                                                        "updated.  To continue, select at least one instance to add virtual "
                                                        "machine on the basis of it.</p>"));

    /* Translate profile stuff: */
    m_pProfileLabel->setText(UIWizardAddCloudVM::tr("&Profile:"));
    m_pProfileToolButton->setToolTip(UIWizardAddCloudVM::tr("Open Cloud Profile Manager..."));
    m_pSourceInstanceLabel->setText(UIWizardAddCloudVM::tr("&Instances:"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pProviderLabel;
    labels << m_pProfileLabel;
    labels << m_pSourceInstanceLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pProviderLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pOptionsLayout->setColumnMinimumWidth(0, iMaxWidth);

    /* Update tool-tips: */
    updateComboToolTip(m_pProviderComboBox);
}

void UIWizardAddCloudVMPageBasic1::initializePage()
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

bool UIWizardAddCloudVMPageBasic1::isComplete() const
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

bool UIWizardAddCloudVMPageBasic1::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Try to add cloud VMs: */
    fResult = qobject_cast<UIWizardAddCloudVM*>(wizard())->addCloudVMs();

    /* Return result: */
    return fResult;
}

void UIWizardAddCloudVMPageBasic1::sltHandleProviderComboChange()
{
    updateProvider();
    emit completeChanged();
}

void UIWizardAddCloudVMPageBasic1::sltHandleProfileComboChange()
{
    updateProfile();
    emit completeChanged();
}

void UIWizardAddCloudVMPageBasic1::sltHandleProfileButtonClick()
{
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardAddCloudVMPageBasic1::sltHandleSourceInstanceChange()
{
    updateSourceInstance();
    emit completeChanged();
}

void UIWizardAddCloudVMPageBasic1::setShortProviderName(const QString &strShortProviderName)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setShortProviderName(strShortProviderName);
}

QString UIWizardAddCloudVMPageBasic1::shortProviderName() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->shortProviderName();
}

void UIWizardAddCloudVMPageBasic1::setProfileName(const QString &strProfileName)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setProfileName(strProfileName);
}

QString UIWizardAddCloudVMPageBasic1::profileName() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->profileName();
}

void UIWizardAddCloudVMPageBasic1::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setClient(comClient);
}

CCloudClient UIWizardAddCloudVMPageBasic1::client() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->client();
}

void UIWizardAddCloudVMPageBasic1::setInstanceIds(const QStringList &instanceIds)
{
    qobject_cast<UIWizardAddCloudVM*>(wizard())->setInstanceIds(instanceIds);
}

QStringList UIWizardAddCloudVMPageBasic1::instanceIds() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizard())->instanceIds();
}

void UIWizardAddCloudVMPageBasic1::updateProvider()
{
    updateComboToolTip(m_pProviderComboBox);
    setShortProviderName(m_pProviderComboBox->currentData(ProviderData_ShortName).toString());
    CCloudProvider comCloudProvider = cloudProviderByShortName(shortProviderName(), wizard());
    populateProfiles(m_pProfileComboBox, comCloudProvider);
    updateProfile();
}

void UIWizardAddCloudVMPageBasic1::updateProfile()
{
    setProfileName(m_pProfileComboBox->currentData(ProfileData_Name).toString());
    setClient(cloudClientByName(shortProviderName(), profileName(), wizard()));
    populateProfileInstances(m_pSourceInstanceList, client());
    updateSourceInstance();
}

void UIWizardAddCloudVMPageBasic1::updateSourceInstance()
{
    setInstanceIds(currentListWidgetData(m_pSourceInstanceList));
}
