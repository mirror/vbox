/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageBasic1 class implementation.
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
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageBasic1.h"

/* COM includes: */
#include "CStringArray.h"


/*********************************************************************************************************************************
*   Class UIWizardAddCloudVMPage1 implementation.                                                                                *
*********************************************************************************************************************************/

UIWizardAddCloudVMPage1::UIWizardAddCloudVMPage1()
    : m_fPolished(false)
    , m_pSourceLayout(0)
    , m_pSourceLabel(0)
    , m_pSourceComboBox(0)
    , m_pCloudContainerLayout(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pProfileInstanceLabel(0)
    , m_pProfileInstanceList(0)
{
}

void UIWizardAddCloudVMPage1::populateSources()
{
    /* To be executed just once, so combo should be empty: */
    AssertReturnVoid(m_pSourceComboBox->count() == 0);

    /* Do we have OCI source? */
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
            m_pSourceComboBox->addItem(QString());
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetId(),        SourceData_ID);
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetName(),      SourceData_Name);
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetShortName(), SourceData_ShortName);
            if (m_pSourceComboBox->itemData(m_pSourceComboBox->count() - 1, SourceData_ShortName).toString() == "OCI")
                fOCIPresent = true;
        }
    }
    while (0);

    /* Set default: */
    if (fOCIPresent)
        setSource("OCI");
}

void UIWizardAddCloudVMPage1::populateProfiles()
{
    /* Block signals while updating: */
    m_pProfileComboBox->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (m_pProfileComboBox->currentIndex() != -1)
        strOldData = m_pProfileComboBox->itemData(m_pProfileComboBox->currentIndex(), ProfileData_Name).toString();
    else
    {
        /* Try to fetch "old" profile name from wizard full group name: */
        UIWizardAddCloudVM *pWizard = qobject_cast<UIWizardAddCloudVM*>(wizardImp());
        AssertPtrReturnVoid(pWizard);
        const QString strFullGroupName = pWizard->fullGroupName();
        const QString strProfileName = strFullGroupName.section('/', 2, 2);
        if (!strProfileName.isEmpty())
            strOldData = strProfileName;
    }

    /* Clear combo initially: */
    m_pProfileComboBox->clear();
    /* Clear Cloud Provider: */
    m_comCloudProvider = CCloudProvider();

    /* If provider chosen: */
    if (!sourceId().isNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* (Re)initialize Cloud Provider: */
            m_comCloudProvider = m_comCloudProviderManager.GetProviderById(sourceId());
            if (!m_comCloudProviderManager.isOk())
            {
                msgCenter().cannotFindCloudProvider(m_comCloudProviderManager, sourceId());
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
                m_pProfileComboBox->addItem(strProfileName);
                m_pProfileComboBox->setItemData(m_pProfileComboBox->count() - 1, strProfileName, ProfileData_Name);
            }

            /* Set previous/default item if possible: */
            int iNewIndex = -1;
            if (   iNewIndex == -1
                && !strOldData.isNull())
                iNewIndex = m_pProfileComboBox->findData(strOldData, ProfileData_Name);
            if (   iNewIndex == -1
                && m_pProfileComboBox->count() > 0)
                iNewIndex = 0;
            if (iNewIndex != -1)
                m_pProfileComboBox->setCurrentIndex(iNewIndex);
        }
        while (0);
    }

    /* Unblock signals after update: */
    m_pProfileComboBox->blockSignals(false);
}

void UIWizardAddCloudVMPage1::populateProfile()
{
    /* Clear Cloud Profile: */
    m_comCloudProfile = CCloudProfile();

    /* If both provider and profile chosen: */
    if (m_comCloudProvider.isNotNull() && !profileName().isNull())
    {
        /* Acquire Cloud Profile: */
        m_comCloudProfile = m_comCloudProvider.GetProfileByName(profileName());
        /* Show error message if necessary: */
        if (!m_comCloudProvider.isOk())
            msgCenter().cannotFindCloudProfile(m_comCloudProvider, profileName());
    }
}

void UIWizardAddCloudVMPage1::populateProfileInstances()
{
    /* Block signals while updating: */
    m_pProfileInstanceList->blockSignals(true);

    /* Clear list initially: */
    m_pProfileInstanceList->clear();
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

            /* Gather instance names and ids: */
            CStringArray comNames;
            CStringArray comIDs;

            /* Ask for cloud instances: */
            CProgress comProgress = comCloudClient.ListSourceInstances(comNames, comIDs);
            if (!comCloudClient.isOk())
            {
                msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
                break;
            }

            /* Show "Acquire cloud instances" progress: */
            msgCenter().showModalProgressDialog(comProgress, QString(),
                                                ":/progress_reading_appliance_90px.png", 0, 0);
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
                QListWidgetItem *pItem = new QListWidgetItem(names.at(i), m_pProfileInstanceList);
                if (pItem)
                {
                    pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
                    pItem->setData(Qt::UserRole, ids.at(i));
                }
            }

            /* Choose the 1st one by default if possible: */
            if (m_pProfileInstanceList->count())
                m_pProfileInstanceList->setCurrentRow(0);
        }
        while (0);
    }

    /* Unblock signals after update: */
    m_pProfileInstanceList->blockSignals(false);
}

void UIWizardAddCloudVMPage1::updateSourceComboToolTip()
{
    const int iCurrentIndex = m_pSourceComboBox->currentIndex();
    if (iCurrentIndex != -1)
    {
        const QString strCurrentToolTip = m_pSourceComboBox->itemData(iCurrentIndex, Qt::ToolTipRole).toString();
        AssertMsg(!strCurrentToolTip.isEmpty(), ("Data not found!"));
        m_pSourceComboBox->setToolTip(strCurrentToolTip);
    }
}

void UIWizardAddCloudVMPage1::setSource(const QString &strSource)
{
    const int iIndex = m_pSourceComboBox->findData(strSource, SourceData_ShortName);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pSourceComboBox->setCurrentIndex(iIndex);
}

QString UIWizardAddCloudVMPage1::source() const
{
    const int iIndex = m_pSourceComboBox->currentIndex();
    return m_pSourceComboBox->itemData(iIndex, SourceData_ShortName).toString();
}

QUuid UIWizardAddCloudVMPage1::sourceId() const
{
    const int iIndex = m_pSourceComboBox->currentIndex();
    return m_pSourceComboBox->itemData(iIndex, SourceData_ID).toUuid();
}

QString UIWizardAddCloudVMPage1::profileName() const
{
    const int iIndex = m_pProfileComboBox->currentIndex();
    return m_pProfileComboBox->itemData(iIndex, ProfileData_Name).toString();
}

QStringList UIWizardAddCloudVMPage1::instanceIds() const
{
    QStringList result;
    foreach (QListWidgetItem *pItem, m_pProfileInstanceList->selectedItems())
        result << pItem->data(Qt::UserRole).toString();
    return result;
}

void UIWizardAddCloudVMPage1::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardAddCloudVM*>(wizardImp())->setClient(comClient);
}

CCloudClient UIWizardAddCloudVMPage1::client() const
{
    return qobject_cast<UIWizardAddCloudVM*>(wizardImp())->client();
}


/*********************************************************************************************************************************
*   Class UIWizardAddCloudVMPageBasic1 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardAddCloudVMPageBasic1::UIWizardAddCloudVMPageBasic1()
    : m_pLabelMain(0)
    , m_pLabelDescription(0)
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

        /* Create source layout: */
        m_pSourceLayout = new QGridLayout;
        if (m_pSourceLayout)
        {
            m_pSourceLayout->setContentsMargins(0, 0, 0, 0);
            m_pSourceLayout->setColumnStretch(0, 0);
            m_pSourceLayout->setColumnStretch(1, 1);

            /* Create source label: */
            m_pSourceLabel = new QLabel(this);
            if (m_pSourceLabel)
            {
                /* Add into layout: */
                m_pSourceLayout->addWidget(m_pSourceLabel, 0, 0, Qt::AlignRight);
            }
            /* Create source selector: */
            m_pSourceComboBox = new QIComboBox(this);
            if (m_pSourceComboBox)
            {
                m_pSourceLabel->setBuddy(m_pSourceComboBox);

                /* Add into layout: */
                m_pSourceLayout->addWidget(m_pSourceComboBox, 0, 1);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pSourceLayout);
        }

        /* Create description label: */
        m_pLabelDescription = new QIRichTextLabel(this);
        if (m_pLabelDescription)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelDescription);
        }

        /* Create cloud container layout: */
        m_pCloudContainerLayout = new QGridLayout;
        if (m_pCloudContainerLayout)
        {
            m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
            m_pCloudContainerLayout->setColumnStretch(0, 0);
            m_pCloudContainerLayout->setColumnStretch(1, 1);
            m_pCloudContainerLayout->setRowStretch(1, 0);
            m_pCloudContainerLayout->setRowStretch(2, 1);

            /* Create profile label: */
            m_pProfileLabel = new QLabel(this);
            if (m_pProfileLabel)
            {
                /* Add into layout: */
                m_pCloudContainerLayout->addWidget(m_pProfileLabel, 0, 0, Qt::AlignRight);
            }
            /* Create sub-layout: */
            QHBoxLayout *pSubLayout = new QHBoxLayout;
            if (pSubLayout)
            {
                pSubLayout->setContentsMargins(0, 0, 0, 0);
                pSubLayout->setSpacing(1);

                /* Create profile combo-box: */
                m_pProfileComboBox = new QIComboBox(this);
                if (m_pProfileComboBox)
                {
                    m_pProfileLabel->setBuddy(m_pProfileComboBox);

                    /* Add into layout: */
                    pSubLayout->addWidget(m_pProfileComboBox);
                }
                /* Create profile tool-button: */
                m_pProfileToolButton = new QIToolButton(this);
                if (m_pProfileToolButton)
                {
                    m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                      ":/cloud_profile_manager_disabled_16px.png"));

                    /* Add into layout: */
                    pSubLayout->addWidget(m_pProfileToolButton);
                }

                /* Add into layout: */
                m_pCloudContainerLayout->addLayout(pSubLayout, 0, 1);
            }

            /* Create profile instance label: */
            m_pProfileInstanceLabel = new QLabel(this);
            if (m_pProfileInstanceLabel)
            {
                /* Add into layout: */
                m_pCloudContainerLayout->addWidget(m_pProfileInstanceLabel, 1, 0, Qt::AlignRight);
            }
            /* Create profile instances table: */
            m_pProfileInstanceList = new QListWidget(this);
            if (m_pProfileInstanceList)
            {
                m_pProfileInstanceLabel->setBuddy(m_pProfileInstanceLabel);
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
                m_pCloudContainerLayout->addWidget(m_pProfileInstanceList, 1, 1, 2, 1);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pCloudContainerLayout);
        }
    }

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardAddCloudVMPageBasic1::sltHandleSourceChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardAddCloudVMPageBasic1::sltHandleSourceChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardAddCloudVMPageBasic1::sltHandleProfileButtonClick);
    connect(m_pProfileInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardAddCloudVMPageBasic1::completeChanged);

    /* Register fields: */
    registerField("source", this, "source");
    registerField("profileName", this, "profileName");
    registerField("instanceIds", this, "instanceIds");
}

void UIWizardAddCloudVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardAddCloudVM::tr("Source to add from"));

    /* Translate main label: */
    m_pLabelMain->setText(UIWizardAddCloudVM::tr("Please choose the source to add cloud virtual machine from.  This can "
                                                 "be one of known cloud service providers below."));

    /* Translate source label: */
    m_pSourceLabel->setText(UIWizardAddCloudVM::tr("&Source:"));
    /* Translate received values of Source combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pSourceComboBox->count(); ++i)
    {
        m_pSourceComboBox->setItemText(i, m_pSourceComboBox->itemData(i, SourceData_Name).toString());
        m_pSourceComboBox->setItemData(i, UIWizardAddCloudVM::tr("Add VM from cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate description label: */
    m_pLabelDescription->setText(UIWizardAddCloudVM::tr("<p>Please choose one of cloud service profiles you have registered to "
                                                        "add virtual machine from.  Existing instance list will be "
                                                        "updated.  To continue, select at least one instance to add virtual "
                                                        "machine on the basis of it.</p>"));

    /* Translate profile stuff: */
    m_pProfileLabel->setText(UIWizardAddCloudVM::tr("&Profile:"));
    m_pProfileToolButton->setToolTip(UIWizardAddCloudVM::tr("Open Cloud Profile Manager..."));
    m_pProfileInstanceLabel->setText(UIWizardAddCloudVM::tr("&Instances:"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    labels << m_pSourceLabel;
    labels << m_pProfileLabel;
    labels << m_pProfileInstanceLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    m_pSourceLayout->setColumnMinimumWidth(0, iMaxWidth);
    m_pCloudContainerLayout->setColumnMinimumWidth(0, iMaxWidth);

    /* Update tool-tips: */
    updateSourceComboToolTip();
}

void UIWizardAddCloudVMPageBasic1::initializePage()
{
    /* If wasn't polished yet: */
    if (!m_fPolished)
    {
        /* Populate sources: */
        populateSources();
        /* Choose one of them, asynchronously: */
        QMetaObject::invokeMethod(this, "sltHandleSourceChange", Qt::QueuedConnection);
        m_fPolished = true;
    }

    /* Translate page: */
    retranslateUi();
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

    /* Lock finish button: */
    startProcessing();

    /* Try to add cloud VMs: */
    fResult = qobject_cast<UIWizardAddCloudVM*>(wizard())->addCloudVMs();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardAddCloudVMPageBasic1::sltHandleSourceChange()
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

void UIWizardAddCloudVMPageBasic1::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfile();
    populateProfileInstances();
    emit completeChanged();
}

void UIWizardAddCloudVMPageBasic1::sltHandleProfileButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}
