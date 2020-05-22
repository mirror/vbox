/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic1 class implementation.
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
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic1.h"

/* COM includes: */
#include "CAppliance.h"
#include "CStringArray.h"


/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPage1 implementation.                                                                                *
*********************************************************************************************************************************/

UIWizardNewCloudVMPage1::UIWizardNewCloudVMPage1()
    : m_fPolished(false)
    , m_pLocationLabel(0)
    , m_pLocationComboBox(0)
    , m_pAccountLabel(0)
    , m_pAccountComboBox(0)
    , m_pAccountToolButton(0)
    , m_pAccountPropertyTable(0)
    , m_pSourceImageLabel(0)
    , m_pSourceTabBar(0)
    , m_pSourceImageList(0)
{
}

void UIWizardNewCloudVMPage1::populateLocations()
{
    /* To be executed just once, so combo should be empty: */
    AssertReturnVoid(m_pLocationComboBox->count() == 0);

    /* Do we have OCI location? */
    bool fOCIPresent = false;

    /* Main API request sequence, can be interrupted after any step: */
    do
    {
        /* Initialize Cloud Provider Manager: */
        CVirtualBox comVBox = uiCommon().virtualBox();
        m_comCloudProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
        {
            msgCenter().cannotAcquireCloudProviderManager(comVBox);
            break;
        }

        /* Acquire existing providers: */
        const QVector<CCloudProvider> providers = m_comCloudProviderManager.GetProviders();
        if (!m_comCloudProviderManager.isOk())
        {
            msgCenter().cannotAcquireCloudProviderManagerParameter(m_comCloudProviderManager);
            break;
        }

        /* Iterate through existing providers: */
        foreach (const CCloudProvider &comProvider, providers)
        {
            /* Skip if we have nothing to populate (file missing?): */
            if (comProvider.isNull())
                continue;

            /* Compose empty item, fill it's data: */
            m_pLocationComboBox->addItem(QString());
            m_pLocationComboBox->setItemData(m_pLocationComboBox->count() - 1, comProvider.GetId(),        LocationData_ID);
            m_pLocationComboBox->setItemData(m_pLocationComboBox->count() - 1, comProvider.GetName(),      LocationData_Name);
            m_pLocationComboBox->setItemData(m_pLocationComboBox->count() - 1, comProvider.GetShortName(), LocationData_ShortName);
            if (m_pLocationComboBox->itemData(m_pLocationComboBox->count() - 1, LocationData_ShortName).toString() == "OCI")
                fOCIPresent = true;
        }
    }
    while (0);

    /* Set default: */
    if (fOCIPresent)
        setLocation("OCI");
}

void UIWizardNewCloudVMPage1::populateAccounts()
{
    /* Block signals while updating: */
    m_pAccountComboBox->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (m_pAccountComboBox->currentIndex() != -1)
        strOldData = m_pAccountComboBox->itemData(m_pAccountComboBox->currentIndex(), AccountData_ProfileName).toString();

    /* Clear combo initially: */
    m_pAccountComboBox->clear();
    /* Clear Cloud Provider: */
    m_comCloudProvider = CCloudProvider();

    /* If provider chosen: */
    if (!locationId().isNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* (Re)initialize Cloud Provider: */
            m_comCloudProvider = m_comCloudProviderManager.GetProviderById(locationId());
            if (!m_comCloudProviderManager.isOk())
            {
                msgCenter().cannotFindCloudProvider(m_comCloudProviderManager, locationId());
                break;
            }

            /* Acquire existing profile names: */
            const QVector<QString> profileNames = m_comCloudProvider.GetProfileNames();
            if (!m_comCloudProvider.isOk())
            {
                msgCenter().cannotAcquireCloudProviderParameter(m_comCloudProvider);
                break;
            }

            /* Iterate through existing profile names: */
            foreach (const QString &strProfileName, profileNames)
            {
                /* Skip if we have nothing to show (wtf happened?): */
                if (strProfileName.isEmpty())
                    continue;

                /* Compose item, fill it's data: */
                m_pAccountComboBox->addItem(strProfileName);
                m_pAccountComboBox->setItemData(m_pAccountComboBox->count() - 1, strProfileName, AccountData_ProfileName);
            }

            /* Set previous/default item if possible: */
            int iNewIndex = -1;
            if (   iNewIndex == -1
                && !strOldData.isNull())
                iNewIndex = m_pAccountComboBox->findData(strOldData, AccountData_ProfileName);
            if (   iNewIndex == -1
                && m_pAccountComboBox->count() > 0)
                iNewIndex = 0;
            if (iNewIndex != -1)
                m_pAccountComboBox->setCurrentIndex(iNewIndex);
        }
        while (0);
    }

    /* Unblock signals after update: */
    m_pAccountComboBox->blockSignals(false);
}

void UIWizardNewCloudVMPage1::populateAccountProperties()
{
    /* Block signals while updating: */
    m_pAccountPropertyTable->blockSignals(true);

    /* Clear table initially: */
    m_pAccountPropertyTable->clear();
    m_pAccountPropertyTable->setRowCount(0);
    m_pAccountPropertyTable->setColumnCount(0);
    /* Clear Cloud Profile: */
    m_comCloudProfile = CCloudProfile();

    /* If both provider and profile chosen: */
    if (m_comCloudProvider.isNotNull() && !profileName().isNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* Acquire Cloud Profile: */
            m_comCloudProfile = m_comCloudProvider.GetProfileByName(profileName());
            if (!m_comCloudProvider.isOk())
            {
                msgCenter().cannotFindCloudProfile(m_comCloudProvider, profileName());
                break;
            }

            /* Acquire profile properties: */
            QVector<QString> keys;
            QVector<QString> values;
            values = m_comCloudProfile.GetProperties(QString(), keys);
            if (!m_comCloudProfile.isOk())
            {
                msgCenter().cannotAcquireCloudProfileParameter(m_comCloudProfile);
                break;
            }

            /* Configure table: */
            m_pAccountPropertyTable->setRowCount(keys.size());
            m_pAccountPropertyTable->setColumnCount(2);

            /* Push acquired keys/values to data fields: */
            for (int i = 0; i < m_pAccountPropertyTable->rowCount(); ++i)
            {
                /* Create key item: */
                QTableWidgetItem *pItemK = new QTableWidgetItem(keys.at(i));
                if (pItemK)
                {
                    /* Non-editable for sure, but non-selectable? */
                    pItemK->setFlags(pItemK->flags() & ~Qt::ItemIsEditable);
                    pItemK->setFlags(pItemK->flags() & ~Qt::ItemIsSelectable);

                    /* Use non-translated description as tool-tip: */
                    const QString strToolTip = m_comCloudProvider.GetPropertyDescription(keys.at(i));
                    /* Show error message if necessary: */
                    if (!m_comCloudProfile.isOk())
                        msgCenter().cannotAcquireCloudProfileParameter(m_comCloudProfile);
                    else
                        pItemK->setData(Qt::UserRole, strToolTip);

                    /* Insert into table: */
                    m_pAccountPropertyTable->setItem(i, 0, pItemK);
                }

                /* Create value item: */
                QTableWidgetItem *pItemV = new QTableWidgetItem(values.at(i));
                if (pItemV)
                {
                    /* Non-editable for sure, but non-selectable? */
                    pItemV->setFlags(pItemV->flags() & ~Qt::ItemIsEditable);
                    pItemV->setFlags(pItemV->flags() & ~Qt::ItemIsSelectable);

                    /* Use the value as tool-tip, there can be quite long values: */
                    const QString strToolTip = values.at(i);
                    pItemV->setToolTip(strToolTip);

                    /* Insert into table: */
                    m_pAccountPropertyTable->setItem(i, 1, pItemV);
                }
            }

            /* Update table tool-tips: */
            updateAccountPropertyTableToolTips();

            /* Adjust the table: */
            adjustAccountPropertyTable();
        }
        while (0);
    }

    /* Unblock signals after update: */
    m_pAccountPropertyTable->blockSignals(false);
}

void UIWizardNewCloudVMPage1::populateSourceImages()
{
    /* Block signals while updating: */
    m_pSourceImageList->blockSignals(true);

    /* Clear list initially: */
    m_pSourceImageList->clear();
    /* Clear Cloud Client: */
    setClient(CCloudClient());

    /* If profile chosen: */
    if (m_comCloudProfile.isNotNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* Acquire Cloud Client: */
            CCloudClient comCloudClient = m_comCloudProfile.CreateCloudClient();
            if (!m_comCloudProfile.isOk())
            {
                msgCenter().cannotCreateCloudClient(m_comCloudProfile);
                break;
            }

            /* Remember Cloud Client: */
            setClient(comCloudClient);

            /* Gather source names and ids, depending on current source tab-bar index: */
            CStringArray comNames;
            CStringArray comIDs;
            CProgress comProgress;
            switch (m_pSourceTabBar->currentIndex())
            {
                case 0:
                {
                    /* Ask for cloud boot-volumes, currently we are interested in Source boot-volumes only: */
                    comProgress = comCloudClient.ListSourceBootVolumes(comNames, comIDs);
                    break;
                }
                case 1:
                {
                    /* Ask for cloud images, currently we are interested in Available images only: */
                    const QVector<KCloudImageState> cloudImageStates  = QVector<KCloudImageState>()
                                                                     << KCloudImageState_Available;
                    comProgress = comCloudClient.ListImages(cloudImageStates, comNames, comIDs);
                    break;
                }
                default:
                    break;
            }
            if (!comCloudClient.isOk())
            {
                msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
                break;
            }

            /* Show "Acquire cloud images" progress: */
            msgCenter().showModalProgressDialog(comProgress, QString(), ":/progress_reading_appliance_90px.png", 0, 0);
            if (comProgress.GetCanceled())
            {
                wizardImp()->reject();
                break;
            }
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            {
                msgCenter().cannotAcquireCloudClientParameter(comProgress);
                break;
            }

            /* Push acquired names to list rows: */
            const QVector<QString> names = comNames.GetValues();
            const QVector<QString> ids = comIDs.GetValues();
            for (int i = 0; i < names.size(); ++i)
            {
                /* Create list item: */
                QListWidgetItem *pItem = new QListWidgetItem(names.at(i), m_pSourceImageList);
                if (pItem)
                {
                    pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
                    pItem->setData(Qt::UserRole, ids.at(i));
                }
            }

            /* Choose the 1st one by default if possible: */
            if (m_pSourceImageList->count())
                m_pSourceImageList->setCurrentRow(0);
        }
        while (0);
    }

    /* Unblock signals after update: */
    m_pSourceImageList->blockSignals(false);
}

void UIWizardNewCloudVMPage1::populateFormProperties()
{
    /* Clear description & form properties: */
    setVSD(CVirtualSystemDescription());
    setVSDForm(CVirtualSystemDescriptionForm());

    /* If client created: */
    CCloudClient comCloudClient = client();
    if (comCloudClient.isNotNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* Create appliance: */
            CVirtualBox comVBox = uiCommon().virtualBox();
            CAppliance comAppliance = comVBox.CreateAppliance();
            if (!comVBox.isOk())
            {
                msgCenter().cannotCreateAppliance(comVBox);
                break;
            }

            /* Create virtual system description: */
            comAppliance.CreateVirtualSystemDescriptions(1);
            if (!comAppliance.isOk())
            {
                msgCenter().cannotCreateVirtualSystemDescription(comAppliance);
                break;
            }

            /* Acquire virtual system description: */
            QVector<CVirtualSystemDescription> descriptions = comAppliance.GetVirtualSystemDescriptions();
            if (!comAppliance.isOk())
            {
                msgCenter().cannotAcquireVirtualSystemDescription(comAppliance);
                break;
            }

            /* Make sure there is at least one virtual system description created: */
            AssertReturnVoid(!descriptions.isEmpty());
            CVirtualSystemDescription comVSD = descriptions.at(0);

            /* Remember Virtual System Description: */
            setVSD(comVSD);

            /* Depending on current source tab-bar index: */
            switch (m_pSourceTabBar->currentIndex())
            {
                case 0:
                {
                    /* Add boot-volume id to virtual system description: */
                    comVSD.AddDescription(KVirtualSystemDescriptionType_CloudBootVolumeId, imageId(), QString());
                    break;
                }
                case 1:
                {
                    /* Add image id to virtual system description: */
                    comVSD.AddDescription(KVirtualSystemDescriptionType_CloudImageId, imageId(), QString());
                    break;
                }
                default:
                    break;
            }
            if (!comVSD.isOk())
            {
                msgCenter().cannotAddVirtualSystemDescriptionValue(comVSD);
                break;
            }

            /* Create Virtual System Description Form: */
            qobject_cast<UIWizardNewCloudVM*>(wizardImp())->createVSDForm();
        }
        while (0);
    }
}

void UIWizardNewCloudVMPage1::updateLocationComboToolTip()
{
    const int iCurrentIndex = m_pLocationComboBox->currentIndex();
    if (iCurrentIndex != -1)
    {
        const QString strCurrentToolTip = m_pLocationComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
        AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
        m_pLocationComboBox->setToolTip(strCurrentToolTip);
    }
}

void UIWizardNewCloudVMPage1::updateAccountPropertyTableToolTips()
{
    /* Iterate through all the key items: */
    for (int i = 0; i < m_pAccountPropertyTable->rowCount(); ++i)
    {
        /* Acquire current key item: */
        QTableWidgetItem *pItemK = m_pAccountPropertyTable->item(i, 0);
        if (pItemK)
        {
            const QString strToolTip = pItemK->data(Qt::UserRole).toString();
            pItemK->setToolTip(QApplication::translate("UIWizardNewCloudVMPageBasic1", strToolTip.toUtf8().constData()));
        }
    }
}

void UIWizardNewCloudVMPage1::adjustAccountPropertyTable()
{
    /* Disable last column stretching temporary: */
    m_pAccountPropertyTable->horizontalHeader()->setStretchLastSection(false);

    /* Resize both columns to contents: */
    m_pAccountPropertyTable->resizeColumnsToContents();
    /* Then acquire full available width: */
    const int iFullWidth = m_pAccountPropertyTable->viewport()->width();
    /* First column should not be less than it's minimum size, last gets the rest: */
    const int iMinimumWidth0 = qMin(m_pAccountPropertyTable->horizontalHeader()->sectionSize(0), iFullWidth / 2);
    m_pAccountPropertyTable->horizontalHeader()->resizeSection(0, iMinimumWidth0);

    /* Enable last column stretching again: */
    m_pAccountPropertyTable->horizontalHeader()->setStretchLastSection(true);
}

void UIWizardNewCloudVMPage1::setLocation(const QString &strLocation)
{
    const int iIndex = m_pLocationComboBox->findData(strLocation, LocationData_ShortName);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pLocationComboBox->setCurrentIndex(iIndex);
}

QString UIWizardNewCloudVMPage1::location() const
{
    const int iIndex = m_pLocationComboBox->currentIndex();
    return m_pLocationComboBox->itemData(iIndex, LocationData_ShortName).toString();
}

QUuid UIWizardNewCloudVMPage1::locationId() const
{
    const int iIndex = m_pLocationComboBox->currentIndex();
    return m_pLocationComboBox->itemData(iIndex, LocationData_ID).toUuid();
}

QString UIWizardNewCloudVMPage1::profileName() const
{
    const int iIndex = m_pAccountComboBox->currentIndex();
    return m_pAccountComboBox->itemData(iIndex, AccountData_ProfileName).toString();
}

QString UIWizardNewCloudVMPage1::imageId() const
{
    QListWidgetItem *pItem = m_pSourceImageList->currentItem();
    return pItem ? pItem->data(Qt::UserRole).toString() : QString();
}

void UIWizardNewCloudVMPage1::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardNewCloudVM*>(wizardImp())->setClient(comClient);
}

CCloudClient UIWizardNewCloudVMPage1::client() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizardImp())->client();
}

void UIWizardNewCloudVMPage1::setVSD(const CVirtualSystemDescription &comDescription)
{
    qobject_cast<UIWizardNewCloudVM*>(wizardImp())->setVSD(comDescription);
}

CVirtualSystemDescription UIWizardNewCloudVMPage1::vsd() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizardImp())->vsd();
}

void UIWizardNewCloudVMPage1::setVSDForm(const CVirtualSystemDescriptionForm &comForm)
{
    qobject_cast<UIWizardNewCloudVM*>(wizardImp())->setVSDForm(comForm);
}

CVirtualSystemDescriptionForm UIWizardNewCloudVMPage1::vsdForm() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizardImp())->vsdForm();
}


/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPageBasic1 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardNewCloudVMPageBasic1::UIWizardNewCloudVMPageBasic1()
    : m_pLabelMain(0)
    , m_pLocationLayout(0)
    , m_pLabelDescription(0)
    , m_pOptionsLayout(0)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create main label: */
        m_pLabelMain = new QIRichTextLabel(this);
        if (m_pLabelMain)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelMain);
        }

        /* Create location layout: */
        m_pLocationLayout = new QGridLayout;
        if (m_pLocationLayout)
        {
            m_pLocationLayout->setContentsMargins(0, 0, 0, 0);
            m_pLocationLayout->setColumnStretch(0, 0);
            m_pLocationLayout->setColumnStretch(1, 1);

            /* Create location label: */
            m_pLocationLabel = new QLabel(this);
            if (m_pLocationLabel)
            {
                /* Add into layout: */
                m_pLocationLayout->addWidget(m_pLocationLabel, 0, 0, Qt::AlignRight);
            }
            /* Create location combo-box: */
            m_pLocationComboBox = new QIComboBox(this);
            if (m_pLocationComboBox)
            {
                m_pLocationLabel->setBuddy(m_pLocationComboBox);

                /* Add into layout: */
                m_pLocationLayout->addWidget(m_pLocationComboBox, 0, 1);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pLocationLayout);
        }

        /* Create description label: */
        m_pLabelDescription = new QIRichTextLabel(this);
        if (m_pLabelDescription)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelDescription);
        }

        /* Create options layout: */
        m_pOptionsLayout = new QGridLayout;
        if (m_pOptionsLayout)
        {
            m_pOptionsLayout->setContentsMargins(0, 0, 0, 0);
            m_pOptionsLayout->setColumnStretch(0, 0);
            m_pOptionsLayout->setColumnStretch(1, 1);
            m_pOptionsLayout->setRowStretch(2, 0);
            m_pOptionsLayout->setRowStretch(3, 1);

            /* Create account label: */
            m_pAccountLabel = new QLabel(this);
            if (m_pAccountLabel)
            {
                /* Add into layout: */
                m_pOptionsLayout->addWidget(m_pAccountLabel, 0, 0, Qt::AlignRight);
            }
            /* Create account layout: */
            QHBoxLayout *pAccountLayout = new QHBoxLayout;
            if (pAccountLayout)
            {
                pAccountLayout->setContentsMargins(0, 0, 0, 0);
                pAccountLayout->setSpacing(1);

                /* Create account combo-box: */
                m_pAccountComboBox = new QIComboBox(this);
                if (m_pAccountComboBox)
                {
                    m_pAccountLabel->setBuddy(m_pAccountComboBox);

                    /* Add into layout: */
                    pAccountLayout->addWidget(m_pAccountComboBox);
                }
                /* Create account tool-button: */
                m_pAccountToolButton = new QIToolButton(this);
                if (m_pAccountToolButton)
                {
                    m_pAccountToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                      ":/cloud_profile_manager_disabled_16px.png"));

                    /* Add into layout: */
                    pAccountLayout->addWidget(m_pAccountToolButton);
                }

                /* Add into layout: */
                m_pOptionsLayout->addLayout(pAccountLayout, 0, 1);
            }

            /* Create account property table: */
            m_pAccountPropertyTable = new QTableWidget(this);
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
                m_pOptionsLayout->addWidget(m_pAccountPropertyTable, 1, 1);
            }

            /* Create source image label: */
            m_pSourceImageLabel = new QLabel(this);
            if (m_pSourceImageLabel)
            {
                /* Add into layout: */
                m_pOptionsLayout->addWidget(m_pSourceImageLabel, 2, 0, Qt::AlignRight);
            }
            /* Create source image layout: */
            QVBoxLayout *pSourceImageLayout = new QVBoxLayout;
            if (pSourceImageLayout)
            {
                pSourceImageLayout->setSpacing(0);
                pSourceImageLayout->setContentsMargins(0, 0, 0, 0);

                /* Create source tab-bar: */
                m_pSourceTabBar = new QTabBar(this);
                if (m_pSourceTabBar)
                {
                    m_pSourceTabBar->addTab(QString());
                    m_pSourceTabBar->addTab(QString());
                    connect(m_pSourceTabBar, &QTabBar::currentChanged,
                            this, &UIWizardNewCloudVMPageBasic1::sltHandleSourceChange);

                    /* Add into layout: */
                    pSourceImageLayout->addWidget(m_pSourceTabBar);
                }

                /* Create source image list: */
                m_pSourceImageList = new QListWidget(this);
                if (m_pSourceImageList)
                {
                    m_pSourceImageLabel->setBuddy(m_pSourceImageList);
                    const QFontMetrics fm(m_pSourceImageList->font());
                    const int iFontWidth = fm.width('x');
                    const int iTotalWidth = 50 * iFontWidth;
                    const int iFontHeight = fm.height();
                    const int iTotalHeight = 4 * iFontHeight;
                    m_pSourceImageList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                    //m_pSourceImageList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                    m_pSourceImageList->setAlternatingRowColors(true);

                    /* Add into layout: */
                    pSourceImageLayout->addWidget(m_pSourceImageList);
                }

                /* Add into layout: */
                m_pOptionsLayout->addLayout(pSourceImageLayout, 2, 1, 2, 1);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pOptionsLayout);
        }
    }

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardNewCloudVMPageBasic1::sltHandleLocationChange);
    connect(m_pLocationComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageBasic1::sltHandleLocationChange);
    connect(m_pAccountComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageBasic1::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageBasic1::sltHandleAccountButtonClick);
    connect(m_pSourceImageList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageBasic1::completeChanged);

    /* Register fields: */
    registerField("location", this, "location");
    registerField("profileName", this, "profileName");
}

bool UIWizardNewCloudVMPageBasic1::event(QEvent *pEvent)
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

void UIWizardNewCloudVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewCloudVM::tr("Location to create"));

    /* Translate main label: */
    m_pLabelMain->setText(UIWizardNewCloudVM::tr("Please choose the location to create cloud virtual machine in.  This can "
                                                 "be one of known cloud service providers below."));

    /* Translate location label: */
    m_pLocationLabel->setText(UIWizardNewCloudVM::tr("&Location:"));
    /* Translate received values of Location combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pLocationComboBox->count(); ++i)
    {
        m_pLocationComboBox->setItemText(i, m_pLocationComboBox->itemData(i, LocationData_Name).toString());
        m_pLocationComboBox->setItemData(i, UIWizardNewCloudVM::tr("Create VM for cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate description label: */
    m_pLabelDescription->setText(UIWizardNewCloudVM::tr("<p>Please choose one of cloud service accounts you have registered to "
                                                        "create virtual machine for.  Existing images list will be "
                                                        "updated.  To continue, select one of images to create virtual "
                                                        "machine on the basis of it.</p>"));

    /* Translate cloud stuff: */
    m_pAccountLabel->setText(UIWizardNewCloudVM::tr("&Account:"));
    m_pSourceImageLabel->setText(UIWizardNewCloudVM::tr("&Source:"));

    /* Translate source tab-bar: */
    m_pSourceTabBar->setTabText(0, UIWizardNewCloudVM::tr("&Boot Volumes"));
    m_pSourceTabBar->setTabText(1, UIWizardNewCloudVM::tr("&Images"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pLocationLabel;
    labels << m_pAccountLabel;
    labels << m_pSourceImageLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pLocationLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pOptionsLayout->setColumnMinimumWidth(0, iMaxWidth);

    /* Update tool-tips: */
    updateLocationComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardNewCloudVMPageBasic1::initializePage()
{
    /* If wasn't polished yet: */
    if (!m_fPolished)
    {
        /* Populate locations: */
        populateLocations();
        /* Choose one of them, asynchronously: */
        QMetaObject::invokeMethod(this, "sltHandleLocationChange", Qt::QueuedConnection);
        m_fPolished = true;
    }

    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewCloudVMPageBasic1::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    client().isNotNull()
              && !imageId().isNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageBasic1::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Populate vsd and form properties: */
    populateFormProperties();
    /* And make sure they are not NULL: */
    fResult =    vsd().isNotNull()
              && vsdForm().isNotNull();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageBasic1::sltHandleLocationChange()
{
    /* Update tool-tip: */
    updateLocationComboToolTip();

    /* Make image list focused by default: */
    m_pSourceImageList->setFocus();

    /* Refresh required settings: */
    populateAccounts();
    populateAccountProperties();
    populateSourceImages();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
    populateSourceImages();
    emit completeChanged();
}

void UIWizardNewCloudVMPageBasic1::sltHandleAccountButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardNewCloudVMPageBasic1::sltHandleSourceChange()
{
    /* Refresh required settings: */
    populateSourceImages();
    emit completeChanged();
}
