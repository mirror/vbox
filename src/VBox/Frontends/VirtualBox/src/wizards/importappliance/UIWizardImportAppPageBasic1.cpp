/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic1 class implementation.
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
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QStackedLayout>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIEmptyFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"

/* COM includes: */
#include "CStringArray.h"
#include "CVirtualSystemDescription.h"


/*********************************************************************************************************************************
*   Class UIWizardImportAppPage1 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardImportAppPage1::UIWizardImportAppPage1(bool fImportFromOCIByDefault)
    : m_fImportFromOCIByDefault(fImportFromOCIByDefault)
    , m_pSourceLayout(0)
    , m_pSourceLabel(0)
    , m_pSourceComboBox(0)
    , m_pStackedLayout(0)
    , m_pLocalContainerLayout(0)
    , m_pFileLabel(0)
    , m_pFileSelector(0)
    , m_pCloudContainerLayout(0)
    , m_pProfileLabel(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pProfileInstanceLabel(0)
    , m_pProfileInstanceList(0)
{
}

void UIWizardImportAppPage1::populateSources()
{
    /* Block signals while updating: */
    m_pSourceComboBox->blockSignals(true);

    /* Clear combo initially: */
    m_pSourceComboBox->clear();

    /* Compose hardcoded sources list, there might be few of list items: */
    QStringList sources;
    sources << "local";
    /* Add that list to combo: */
    foreach (const QString &strShortName, sources)
    {
        /* Compose empty combo item, fill it's data: */
        m_pSourceComboBox->addItem(QString());
        m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, strShortName, SourceData_ShortName);
    }

    /* Initialize Cloud Provider Manager: */
    bool fOCIPresent = false;
    m_comCloudProviderManager = cloudProviderManager(wizardImp());
    if (m_comCloudProviderManager.isNotNull())
    {
        /* Iterate through existing providers: */
        foreach (const CCloudProvider &comProvider, listCloudProviders(wizardImp()))
        {
            /* Skip if we have nothing to populate (file missing?): */
            if (comProvider.isNull())
                continue;

            /* Compose empty item, fill it's data: */
            m_pSourceComboBox->addItem(QString());
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetId(),        SourceData_ID);
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetName(),      SourceData_Name);
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, comProvider.GetShortName(), SourceData_ShortName);
            m_pSourceComboBox->setItemData(m_pSourceComboBox->count() - 1, true,                       SourceData_IsItCloudFormat);
            if (m_pSourceComboBox->itemData(m_pSourceComboBox->count() - 1, SourceData_ShortName).toString() == "OCI")
                fOCIPresent = true;
        }
    }

    /* Set default: */
    if (m_fImportFromOCIByDefault && fOCIPresent)
        setSource("OCI");
    else
        setSource("local");

    /* Unblock signals after update: */
    m_pSourceComboBox->blockSignals(false);
}

void UIWizardImportAppPage1::populateProfiles()
{
    /* Block signals while updating: */
    m_pProfileComboBox->blockSignals(true);

    /* Remember current item data to be able to restore it: */
    QString strOldData;
    if (m_pProfileComboBox->currentIndex() != -1)
        strOldData = m_pProfileComboBox->itemData(m_pProfileComboBox->currentIndex(), ProfileData_Name).toString();

    /* Clear combo initially: */
    m_pProfileComboBox->clear();
    /* Clear Cloud Provider: */
    m_comCloudProvider = CCloudProvider();

    /* If provider chosen: */
    if (!sourceId().isNull())
    {
        /* (Re)initialize Cloud Provider: */
        m_comCloudProvider = cloudProviderById(sourceId(), wizardImp());
        if (m_comCloudProvider.isNotNull())
        {
            /* Iterate through existing profile names: */
            foreach (const CCloudProfile &comProfile, listCloudProfiles(m_comCloudProvider, wizardImp()))
            {
                /* Acquire profile name: */
                QString strProfileName;
                if (cloudProfileName(comProfile, strProfileName, wizardImp()))
                {
                    /* Compose item, fill it's data: */
                    m_pProfileComboBox->addItem(strProfileName);
                    m_pProfileComboBox->setItemData(m_pProfileComboBox->count() - 1, strProfileName, ProfileData_Name);
                }
            }
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

    /* Unblock signals after update: */
    m_pProfileComboBox->blockSignals(false);
}

void UIWizardImportAppPage1::populateProfile()
{
    /* Clear Cloud Profile: */
    m_comCloudProfile = CCloudProfile();

    /* If both provider and profile chosen: */
    if (!source().isNull() && !profileName().isNull())
    {
        /* Acquire Cloud Profile: */
        m_comCloudProfile = cloudProfileByName(source(), profileName(), wizardImp());
    }
}

void UIWizardImportAppPage1::populateProfileInstances()
{
    /* Block signals while updating: */
    m_pProfileInstanceList->blockSignals(true);

    /* Clear list initially: */
    m_pProfileInstanceList->clear();
    /* Clear Cloud Client: */
    m_comCloudClient = CCloudClient();

    /* If profile chosen: */
    if (m_comCloudProfile.isNotNull())
    {
        /* Main API request sequence, can be interrupted after any step: */
        do
        {
            /* Acquire Cloud Client: */
            m_comCloudClient = cloudClient(m_comCloudProfile);
            if (m_comCloudClient.isNull())
                break;

            /* Gather VM names, ids and states.
             * Currently we are interested in Running and Stopped VMs only. */
            QString strErrorMessage;
            QMap<QString, QString> instances = listInstances(m_comCloudClient, strErrorMessage, wizardImp());

            /* Push acquired names to list rows: */
            foreach (const QString &strId, instances.keys())
            {
                /* Create list item: */
                QListWidgetItem *pItem = new QListWidgetItem(instances.value(strId), m_pProfileInstanceList);
                if (pItem)
                {
                    pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
                    pItem->setData(Qt::UserRole, strId);
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

void UIWizardImportAppPage1::populateFormProperties()
{
    /* Clear appliance: */
    m_comCloudAppliance = CAppliance();
    /* Clear form properties: */
    m_comVSDForm = CVirtualSystemDescriptionForm();

    /* If client created: */
    if (m_comCloudClient.isNotNull())
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

            /* Remember appliance: */
            m_comCloudAppliance = comAppliance;

            /* Read cloud instance info: */
            CProgress comReadProgress = m_comCloudAppliance.Read(QString("OCI://%1/%2").arg(profileName(), machineId()));
            if (!m_comCloudAppliance.isOk())
            {
                msgCenter().cannotImportAppliance(m_comCloudAppliance);
                break;
            }

            /* Show "Read appliance" progress: */
            msgCenter().showModalProgressDialog(comReadProgress, UIWizardImportApp::tr("Read appliance ..."),
                                                ":/progress_reading_appliance_90px.png", 0, 0);
            if (!comReadProgress.isOk() || comReadProgress.GetResultCode() != 0)
            {
                msgCenter().cannotImportAppliance(comReadProgress, m_comCloudAppliance.GetPath());
                break;
            }

            /* Acquire virtual system description: */
            QVector<CVirtualSystemDescription> descriptions = m_comCloudAppliance.GetVirtualSystemDescriptions();
            if (!m_comCloudAppliance.isOk())
            {
                msgCenter().cannotAcquireVirtualSystemDescription(m_comCloudAppliance);
                break;
            }

            /* Make sure there is at least one virtual system description created: */
            AssertReturnVoid(!descriptions.isEmpty());
            CVirtualSystemDescription comDescription = descriptions.at(0);

            /* Read Cloud Client description form: */
            CVirtualSystemDescriptionForm comForm;
            bool fSuccess = importDescriptionForm(m_comCloudClient, comDescription, comForm, wizardImp());
            if (!fSuccess)
                break;

            /* Remember form: */
            m_comVSDForm = comForm;
        }
        while (0);
    }
}

void UIWizardImportAppPage1::updatePageAppearance()
{
    /* Update page appearance according to chosen source: */
    m_pStackedLayout->setCurrentIndex((int)isSourceCloudOne());
}

void UIWizardImportAppPage1::updateSourceComboToolTip()
{
    const QString strCurrentToolTip = m_pSourceComboBox->currentData(Qt::ToolTipRole).toString();
    m_pSourceComboBox->setToolTip(strCurrentToolTip);
}

void UIWizardImportAppPage1::setSource(const QString &strSource)
{
    const int iIndex = m_pSourceComboBox->findData(strSource, SourceData_ShortName);
    AssertMsg(iIndex != -1, ("Data not found!"));
    m_pSourceComboBox->setCurrentIndex(iIndex);
}

QString UIWizardImportAppPage1::source() const
{
    const int iIndex = m_pSourceComboBox->currentIndex();
    return m_pSourceComboBox->itemData(iIndex, SourceData_ShortName).toString();
}

bool UIWizardImportAppPage1::isSourceCloudOne(int iIndex /* = -1 */) const
{
    if (iIndex == -1)
        iIndex = m_pSourceComboBox->currentIndex();
    return m_pSourceComboBox->itemData(iIndex, SourceData_IsItCloudFormat).toBool();
}

QUuid UIWizardImportAppPage1::sourceId() const
{
    const int iIndex = m_pSourceComboBox->currentIndex();
    return m_pSourceComboBox->itemData(iIndex, SourceData_ID).toUuid();
}

QString UIWizardImportAppPage1::profileName() const
{
    const int iIndex = m_pProfileComboBox->currentIndex();
    return m_pProfileComboBox->itemData(iIndex, ProfileData_Name).toString();
}

QString UIWizardImportAppPage1::machineId() const
{
    QListWidgetItem *pItem = m_pProfileInstanceList->currentItem();
    return pItem ? pItem->data(Qt::UserRole).toString() : QString();
}

CCloudProfile UIWizardImportAppPage1::profile() const
{
    return m_comCloudProfile;
}

CAppliance UIWizardImportAppPage1::cloudAppliance() const
{
    return m_comCloudAppliance;
}

CVirtualSystemDescriptionForm UIWizardImportAppPage1::vsdForm() const
{
    return m_comVSDForm;
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPageBasic1 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardImportAppPageBasic1::UIWizardImportAppPageBasic1(bool fImportFromOCIByDefault)
    : UIWizardImportAppPage1(fImportFromOCIByDefault)
    , m_pLabelMain(0)
    , m_pLabelDescription(0)
{
    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Prepare main label: */
        m_pLabelMain = new QIRichTextLabel(this);
        if (m_pLabelMain)
            pMainLayout->addWidget(m_pLabelMain);

        /* Prepare source layout: */
        m_pSourceLayout = new QGridLayout;
        if (m_pSourceLayout)
        {
            m_pSourceLayout->setContentsMargins(0, 0, 0, 0);
            m_pSourceLayout->setColumnStretch(0, 0);
            m_pSourceLayout->setColumnStretch(1, 1);

            /* Prepare source label: */
            m_pSourceLabel = new QLabel(this);
            if (m_pSourceLabel)
                m_pSourceLayout->addWidget(m_pSourceLabel, 0, 0, Qt::AlignRight);
            /* Prepare source selector: */
            m_pSourceComboBox = new QIComboBox(this);
            if (m_pSourceComboBox)
            {
                m_pSourceLabel->setBuddy(m_pSourceComboBox);
                m_pSourceLayout->addWidget(m_pSourceComboBox, 0, 1);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pSourceLayout);
        }

        /* Prepare description label: */
        m_pLabelDescription = new QIRichTextLabel(this);
        if (m_pLabelDescription)
            pMainLayout->addWidget(m_pLabelDescription);

        /* Prepare stacked layout: */
        m_pStackedLayout = new QStackedLayout;
        if (m_pStackedLayout)
        {
            /* Prepare local container: */
            QWidget *pLocalContainer = new QWidget(this);
            if (pLocalContainer)
            {
                /* Prepare local container layout: */
                m_pLocalContainerLayout = new QGridLayout(pLocalContainer);
                if (m_pLocalContainerLayout)
                {
                    m_pLocalContainerLayout->setContentsMargins(0, 0, 0, 0);
                    m_pLocalContainerLayout->setColumnStretch(0, 0);
                    m_pLocalContainerLayout->setColumnStretch(1, 1);
                    m_pLocalContainerLayout->setRowStretch(1, 1);

                    /* Prepare file label: */
                    m_pFileLabel = new QLabel(pLocalContainer);
                    if (m_pFileLabel)
                        m_pLocalContainerLayout->addWidget(m_pFileLabel, 0, 0, Qt::AlignRight);

                    /* Prepare file-path selector: */
                    m_pFileSelector = new UIEmptyFilePathSelector(pLocalContainer);
                    if (m_pFileSelector)
                    {
                        m_pFileLabel->setBuddy(m_pFileSelector);
                        m_pFileSelector->setHomeDir(uiCommon().documentsPath());
                        m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
                        m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                        m_pFileSelector->setEditable(true);
                        m_pLocalContainerLayout->addWidget(m_pFileSelector, 0, 1);
                    }
                }

                /* Add into layout: */
                m_pStackedLayout->addWidget(pLocalContainer);
            }

            /* Prepare cloud container: */
            QWidget *pCloudContainer = new QWidget(this);
            if (pCloudContainer)
            {
                /* Prepare cloud container layout: */
                m_pCloudContainerLayout = new QGridLayout(pCloudContainer);
                if (m_pCloudContainerLayout)
                {
                    m_pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                    m_pCloudContainerLayout->setColumnStretch(0, 0);
                    m_pCloudContainerLayout->setColumnStretch(1, 1);
                    m_pCloudContainerLayout->setRowStretch(1, 0);
                    m_pCloudContainerLayout->setRowStretch(2, 1);

                    /* Prepare profile label: */
                    m_pProfileLabel = new QLabel(pCloudContainer);
                    if (m_pProfileLabel)
                        m_pCloudContainerLayout->addWidget(m_pProfileLabel, 0, 0, Qt::AlignRight);

                    /* Prepare sub-layout: */
                    QHBoxLayout *pSubLayout = new QHBoxLayout;
                    if (pSubLayout)
                    {
                        pSubLayout->setContentsMargins(0, 0, 0, 0);
                        pSubLayout->setSpacing(1);

                        /* Prepare profile combo-box: */
                        m_pProfileComboBox = new QIComboBox(pCloudContainer);
                        if (m_pProfileComboBox)
                        {
                            m_pProfileLabel->setBuddy(m_pProfileComboBox);
                            pSubLayout->addWidget(m_pProfileComboBox);
                        }

                        /* Prepare profile tool-button: */
                        m_pProfileToolButton = new QIToolButton(pCloudContainer);
                        if (m_pProfileToolButton)
                        {
                            m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                              ":/cloud_profile_manager_disabled_16px.png"));
                            pSubLayout->addWidget(m_pProfileToolButton);
                        }

                        /* Add into layout: */
                        m_pCloudContainerLayout->addLayout(pSubLayout, 0, 1);
                    }

                    /* Prepare profile instance label: */
                    m_pProfileInstanceLabel = new QLabel(pCloudContainer);
                    if (m_pProfileInstanceLabel)
                        m_pCloudContainerLayout->addWidget(m_pProfileInstanceLabel, 1, 0, Qt::AlignRight);

                    /* Prepare profile instances table: */
                    m_pProfileInstanceList = new QListWidget(pCloudContainer);
                    if (m_pProfileInstanceList)
                    {
                        m_pProfileInstanceLabel->setBuddy(m_pProfileInstanceLabel);
                        const QFontMetrics fm(m_pProfileInstanceList->font());
                        const int iFontWidth = fm.width('x');
                        const int iTotalWidth = 50 * iFontWidth;
                        const int iFontHeight = fm.height();
                        const int iTotalHeight = 4 * iFontHeight;
                        m_pProfileInstanceList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
//                        m_pProfileInstanceList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                        m_pProfileInstanceList->setAlternatingRowColors(true);
                        m_pCloudContainerLayout->addWidget(m_pProfileInstanceList, 1, 1, 2, 1);
                    }
                }

                /* Add into layout: */
                m_pStackedLayout->addWidget(pCloudContainer);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pStackedLayout);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardImportAppPageBasic1::sltHandleSourceChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardImportAppPageBasic1::sltHandleSourceChange);
    connect(m_pSourceComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardImportAppPageBasic1::sltHandleSourceChange);
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged,
            this, &UIWizardImportAppPageBasic1::completeChanged);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardImportAppPageBasic1::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardImportAppPageBasic1::sltHandleProfileButtonClick);
    connect(m_pProfileInstanceList, &QListWidget::currentRowChanged,
            this, &UIWizardImportAppPageBasic1::completeChanged);

    /* Register fields: */
    registerField("isSourceCloudOne", this, "isSourceCloudOne");
    registerField("profile", this, "profile");
    registerField("cloudAppliance", this, "cloudAppliance");
    registerField("vsdForm", this, "vsdForm");
    registerField("machineId", this, "machineId");
}

void UIWizardImportAppPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance to import"));

    /* Translate main label: */
    if (m_pLabelMain)
        m_pLabelMain->setText(UIWizardImportApp::tr("Please choose the source to import appliance from.  This can be a "
                                                    "local file system to import OVF archive or one of known cloud "
                                                    "service providers to import cloud VM from."));

    /* Translate source label: */
    if (m_pSourceLabel)
        m_pSourceLabel->setText(UIWizardImportApp::tr("&Source:"));
    if (m_pSourceComboBox)
    {
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
    }

    /* Translate local stuff: */
    if (m_pFileLabel)
        m_pFileLabel->setText(UIWizardImportApp::tr("&File:"));
    if (m_pFileSelector)
    {
        m_pFileSelector->setChooseButtonToolTip(UIWizardImportApp::tr("Choose a virtual appliance file to import..."));
        m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Please choose a virtual appliance file to import"));
        m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));
    }

    /* Translate profile stuff: */
    if (m_pProfileLabel)
        m_pProfileLabel->setText(UIWizardImportApp::tr("&Profile:"));
    if (m_pProfileToolButton)
        m_pProfileToolButton->setToolTip(UIWizardImportApp::tr("Open Cloud Profile Manager..."));
    if (m_pProfileInstanceLabel)
        m_pProfileInstanceLabel->setText(UIWizardImportApp::tr("&Machines:"));

    /* Adjust label widths: */
    QList<QWidget*> labels;
    if (m_pFileLabel)
        labels << m_pFileLabel;
    if (m_pSourceLabel)
        labels << m_pSourceLabel;
    if (m_pProfileLabel)
        labels << m_pProfileLabel;
    if (m_pProfileInstanceLabel)
        labels << m_pProfileInstanceLabel;
    int iMaxWidth = 0;
    foreach (QWidget *pLabel, labels)
        iMaxWidth = qMax(iMaxWidth, pLabel->minimumSizeHint().width());
    if (m_pSourceLayout)
        m_pSourceLayout->setColumnMinimumWidth(0, iMaxWidth);
    if (m_pLocalContainerLayout)
    {
        m_pLocalContainerLayout->setColumnMinimumWidth(0, iMaxWidth);
        m_pCloudContainerLayout->setColumnMinimumWidth(0, iMaxWidth);
    }

    /* Update page appearance: */
    updatePageAppearance();

    /* Update tool-tips: */
    updateSourceComboToolTip();
}

void UIWizardImportAppPageBasic1::initializePage()
{
    /* Populate sources: */
    populateSources();
    /* Populate profiles: */
    populateProfiles();
    /* Populate profile: */
    populateProfile();
    /* Populate profile instances: */
    populateProfileInstances();

    /* Translate page: */
    retranslateUi();
}

bool UIWizardImportAppPageBasic1::isComplete() const
{
    bool fResult = true;

    /* Check appliance settings: */
    if (fResult)
    {
        const bool fOVF = !isSourceCloudOne();
        const bool fCSP = !fOVF;

        fResult =    (   fOVF
                      && UICommon::hasAllowedExtension(m_pFileSelector->path().toLower(), OVFFileExts)
                      && QFile::exists(m_pFileSelector->path()))
                  || (   fCSP
                      && !m_comCloudProfile.isNull()
                      && !m_comCloudClient.isNull()
                      && !machineId().isNull());
    }

    return fResult;
}

bool UIWizardImportAppPageBasic1::validatePage()
{
    if (isSourceCloudOne())
    {
        /* Create appliance & populate form properties: */
        populateFormProperties();
        /* And make sure they are not null: */
        return m_comCloudAppliance.isNotNull() && m_comVSDForm.isNotNull();
    }
    else
    {
        /* If file name was changed: */
        if (m_pFileSelector->isModified())
        {
            /* Check if specified file contains valid appliance: */
            if (   !QFile::exists(m_pFileSelector->path())
                || !qobject_cast<UIWizardImportApp*>(wizard())->setFile(m_pFileSelector->path()))
                return false;
            /* Reset the modified bit afterwards: */
            m_pFileSelector->resetModified();
        }

        /* If we have a valid ovf proceed to the appliance settings page: */
        return qobject_cast<UIWizardImportApp*>(wizard())->isValid();
    }
}

void UIWizardImportAppPageBasic1::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardImportAppPage1::updatePageAppearance();

    /* Update page appearance according to chosen storage-type: */
    if (isSourceCloudOne())
    {
        m_pLabelDescription->setText(UIWizardImportApp::
                                     tr("<p>Please choose one of cloud service profiles you have registered to import virtual "
                                        "machine from.  Corresponding machines list will be updated.  To continue, "
                                        "select one of machines to import below.</p>"));
        m_pProfileInstanceList->setFocus();
    }
    else
    {
        m_pLabelDescription->setText(UIWizardImportApp::
                                     tr("<p>Please choose a file to import the virtual appliance from.  VirtualBox currently "
                                        "supports importing appliances saved in the Open Virtualization Format (OVF).  "
                                        "To continue, select the file to import below.</p>"));
        m_pFileSelector->setFocus();
    }
}

void UIWizardImportAppPageBasic1::sltHandleSourceChange()
{
    /* Update tool-tip: */
    updateSourceComboToolTip();

    /* Refresh required settings: */
    updatePageAppearance();
    populateProfiles();
    populateProfile();
    populateProfileInstances();
    emit completeChanged();
}

void UIWizardImportAppPageBasic1::sltHandleProfileComboChange()
{
    /* Refresh required settings: */
    populateProfile();
    populateProfileInstances();
    emit completeChanged();
}

void UIWizardImportAppPageBasic1::sltHandleProfileButtonClick()
{
    /* Open Cloud Profile Manager: */
    if (gpManager)
        gpManager->openCloudProfileManager();
}
