/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic1 class implementation.
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
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QTabBar>
#include <QTableWidget>
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
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic1.h"

/* COM includes: */
#include "CAppliance.h"
#include "CStringArray.h"

/* Namespaces: */
using namespace UIWizardNewCloudVMPage1;


/*********************************************************************************************************************************
*   Namespace UIWizardNewCloudVMPage1 implementation.                                                                            *
*********************************************************************************************************************************/

void UIWizardNewCloudVMPage1::populateProviders(QIComboBox *pCombo)
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

void UIWizardNewCloudVMPage1::populateProfiles(QIComboBox *pCombo, const CCloudProvider &comProvider)
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
        UIWizardNewCloudVM *pWizard = qobject_cast<UIWizardNewCloudVM*>(pParent);
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

void UIWizardNewCloudVMPage1::populateSourceImages(QListWidget *pList, QTabBar *pTabBar, const CCloudClient &comClient)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pList);
    AssertPtrReturnVoid(pTabBar);
    AssertReturnVoid(comClient.isNotNull());
    /* We need top-level parent as well: */
    QWidget *pParent = pList->window();
    AssertPtrReturnVoid(pParent);

    /* Block signals while updating: */
    pList->blockSignals(true);

    /* Clear list initially: */
    pList->clear();

    /* Gather source names and ids, depending on current source tab-bar index: */
    CStringArray comNames;
    CStringArray comIDs;
    bool fResult = false;
    switch (pTabBar->currentIndex())
    {
        /* Ask for cloud images, currently we are interested in Available images only: */
        case 0: fResult = listCloudImages(comClient, comNames, comIDs, pParent); break;
        /* Ask for cloud boot-volumes, currently we are interested in Source boot-volumes only: */
        case 1: fResult = listCloudSourceBootVolumes(comClient, comNames, comIDs, pParent); break;
        default: break;
    }
    if (fResult)
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

void UIWizardNewCloudVMPage1::populateFormProperties(CVirtualSystemDescription comVSD, QTabBar *pTabBar, const QString &strImageId)
{
    /* Sanity check: */
    AssertReturnVoid(comVSD.isNotNull());
    AssertPtrReturnVoid(pTabBar);

    /* Depending on current source tab-bar index: */
    switch (pTabBar->currentIndex())
    {
        /* Add image id to virtual system description: */
        case 0: comVSD.AddDescription(KVirtualSystemDescriptionType_CloudImageId, strImageId, QString()); break;
        /* Add boot-volume id to virtual system description: */
        case 1: comVSD.AddDescription(KVirtualSystemDescriptionType_CloudBootVolumeId, strImageId, QString()); break;
        default: break;
    }
    if (!comVSD.isOk())
        msgCenter().cannotAddVirtualSystemDescriptionValue(comVSD);
}

void UIWizardNewCloudVMPage1::updateComboToolTip(QIComboBox *pCombo)
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

QString UIWizardNewCloudVMPage1::currentListWidgetData(QListWidget *pList)
{
    /* Sanity check: */
    AssertPtrReturn(pList, QString());

    QListWidgetItem *pItem = pList->currentItem();
    return pItem ? pItem->data(Qt::UserRole).toString() : QString();
}



/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPageBasic1 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardNewCloudVMPageBasic1::UIWizardNewCloudVMPageBasic1()
    : m_pLabelMain(0)
    , m_pProviderLayout(0)
    , m_pProviderLabel(0)
    , m_pProviderComboBox(0)
    , m_pLabelDescription(0)
    , m_pOptionsLayout(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pSourceImageLabel(0)
    , m_pSourceTabBar(0)
    , m_pSourceImageList(0)
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

            /* Prepare source image label: */
            m_pSourceImageLabel = new QLabel(this);
            if (m_pSourceImageLabel)
                m_pOptionsLayout->addWidget(m_pSourceImageLabel, 1, 0, Qt::AlignRight);

            /* Prepare source image layout: */
            QVBoxLayout *pSourceImageLayout = new QVBoxLayout;
            if (pSourceImageLayout)
            {
                pSourceImageLayout->setSpacing(0);
                pSourceImageLayout->setContentsMargins(0, 0, 0, 0);

                /* Prepare source tab-bar: */
                m_pSourceTabBar = new QTabBar(this);
                if (m_pSourceTabBar)
                {
                    m_pSourceTabBar->addTab(QString());
                    m_pSourceTabBar->addTab(QString());

                    /* Add into layout: */
                    pSourceImageLayout->addWidget(m_pSourceTabBar);
                }

                /* Prepare source image list: */
                m_pSourceImageList = new QListWidget(this);
                if (m_pSourceImageList)
                {
                    m_pSourceImageLabel->setBuddy(m_pSourceImageList);
                    /* Make source image list fit 50 symbols
                     * horizontally and 8 lines vertically: */
                    const QFontMetrics fm(m_pSourceImageList->font());
                    const int iFontWidth = fm.width('x');
                    const int iTotalWidth = 50 * iFontWidth;
                    const int iFontHeight = fm.height();
                    const int iTotalHeight = 8 * iFontHeight;
                    m_pSourceImageList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                    m_pSourceImageList->setAlternatingRowColors(true);
                    m_pSourceImageList->setSortingEnabled(true);

                    /* Add into layout: */
                    pSourceImageLayout->addWidget(m_pSourceImageList);
                }

                /* Add into layout: */
                m_pOptionsLayout->addLayout(pSourceImageLayout, 1, 1, 2, 1);
            }

            /* Add into layout: */
            pLayoutMain->addLayout(m_pOptionsLayout);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(m_pProviderComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageBasic1::sltHandleProviderComboChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageBasic1::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleProfileButtonClick);
    connect(m_pSourceTabBar, &QTabBar::currentChanged,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleSourceChange);
    connect(m_pSourceImageList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleSourceImageChange);
}

void UIWizardNewCloudVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewCloudVM::tr("Location to create"));

    /* Translate main label: */
    m_pLabelMain->setText(UIWizardNewCloudVM::tr("Please choose the location to create cloud virtual machine in.  This can "
                                                 "be one of known cloud service providers below."));

    /* Translate provider label: */
    m_pProviderLabel->setText(UIWizardNewCloudVM::tr("&Location:"));
    /* Translate received values of Location combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        m_pProviderComboBox->setItemText(i, m_pProviderComboBox->itemData(i, ProviderData_Name).toString());
        m_pProviderComboBox->setItemData(i, UIWizardNewCloudVM::tr("Create VM for cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate description label: */
    m_pLabelDescription->setText(UIWizardNewCloudVM::tr("<p>Please choose one of cloud service profiles you have registered to "
                                                        "create virtual machine for.  Existing images list will be "
                                                        "updated.  To continue, select one of images to create virtual "
                                                        "machine on the basis of it.</p>"));

    /* Translate profile stuff: */
    m_pProfileLabel->setText(UIWizardNewCloudVM::tr("&Profile:"));
    m_pProfileToolButton->setToolTip(UIWizardNewCloudVM::tr("Open Cloud Profile Manager..."));
    m_pSourceImageLabel->setText(UIWizardNewCloudVM::tr("&Source:"));

    /* Translate source tab-bar: */
    m_pSourceTabBar->setTabText(0, UIWizardNewCloudVM::tr("&Images"));
    m_pSourceTabBar->setTabText(1, UIWizardNewCloudVM::tr("&Boot Volumes"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pProviderLabel;
    labels << m_pProfileLabel;
    labels << m_pSourceImageLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pProviderLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pOptionsLayout->setColumnMinimumWidth(0, iMaxWidth);

    /* Update tool-tips: */
    updateComboToolTip(m_pProviderComboBox);
}

void UIWizardNewCloudVMPageBasic1::initializePage()
{
    /* Populate providers: */
    populateProviders(m_pProviderComboBox);
    /* Translate providers: */
    retranslateUi();
    /* Fetch it, asynchronously: */
    QMetaObject::invokeMethod(this, "sltHandleProviderComboChange", Qt::QueuedConnection);
    /* Make image list focused by default: */
    m_pSourceImageList->setFocus();
}

bool UIWizardNewCloudVMPageBasic1::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    client().isNotNull()
              && !m_strSourceImageId.isNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageBasic1::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Populate vsd and form properties: */
    setVSD(createVirtualSystemDescription(wizard()));
    populateFormProperties(vsd(), m_pSourceTabBar, m_strSourceImageId);
    qobject_cast<UIWizardNewCloudVM*>(wizard())->createVSDForm();

    /* And make sure they are not NULL: */
    fResult =    vsd().isNotNull()
              && vsdForm().isNotNull();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageBasic1::sltHandleProviderComboChange()
{
    updateProvider();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::sltHandleProfileComboChange()
{
    updateProfile();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::sltHandleProfileButtonClick()
{
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardNewCloudVMPageBasic1::sltHandleSourceChange()
{
    updateSource();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::sltHandleSourceImageChange()
{
    updateSourceImage();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::setShortProviderName(const QString &strProviderShortName)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setShortProviderName(strProviderShortName);
}

QString UIWizardNewCloudVMPageBasic1::shortProviderName() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->shortProviderName();
}

void UIWizardNewCloudVMPageBasic1::setProfileName(const QString &strProfileName)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setProfileName(strProfileName);
}

QString UIWizardNewCloudVMPageBasic1::profileName() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->profileName();
}

void UIWizardNewCloudVMPageBasic1::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setClient(comClient);
}

CCloudClient UIWizardNewCloudVMPageBasic1::client() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->client();
}

void UIWizardNewCloudVMPageBasic1::setVSD(const CVirtualSystemDescription &comDescription)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setVSD(comDescription);
}

CVirtualSystemDescription UIWizardNewCloudVMPageBasic1::vsd() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsd();
}

CVirtualSystemDescriptionForm UIWizardNewCloudVMPageBasic1::vsdForm() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsdForm();
}

void UIWizardNewCloudVMPageBasic1::updateProvider()
{
    updateComboToolTip(m_pProviderComboBox);
    setShortProviderName(m_pProviderComboBox->currentData(ProviderData_ShortName).toString());
    CCloudProvider comCloudProvider = cloudProviderByShortName(shortProviderName(), wizard());
    populateProfiles(m_pProfileComboBox, comCloudProvider);
    updateProfile();
}

void UIWizardNewCloudVMPageBasic1::updateProfile()
{
    setProfileName(m_pProfileComboBox->currentData(ProfileData_Name).toString());
    setClient(cloudClientByName(shortProviderName(), profileName(), wizard()));
    updateSource();
}

void UIWizardNewCloudVMPageBasic1::updateSource()
{
    populateSourceImages(m_pSourceImageList, m_pSourceTabBar, client());
    updateSourceImage();
}

void UIWizardNewCloudVMPageBasic1::updateSourceImage()
{
    m_strSourceImageId = currentListWidgetData(m_pSourceImageList);
}
