/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageExpert class implementation.
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
#include <QHeaderView>
#include <QListWidget>
#include <QTabBar>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIToolButton.h"
#include "UICloudNetworkingStuff.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManager.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageExpert.h"

/* Namespaces: */
using namespace UIWizardNewCloudVMPage1;
using namespace UIWizardNewCloudVMPage2;


UIWizardNewCloudVMPageExpert::UIWizardNewCloudVMPageExpert(bool fFullWizard)
    : m_fFullWizard(fFullWizard)
    , m_pCntLocation(0)
    , m_pProviderComboBox(0)
    , m_pProfileComboBox(0)
    , m_pProfileToolButton(0)
    , m_pCntSource(0)
    , m_pSourceTabBar(0)
    , m_pSourceImageList(0)
    , m_pSettingsCnt(0)
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(0, 0);
        pLayoutMain->setRowStretch(1, 1);

        /* Prepare location container: */
        m_pCntLocation = new QGroupBox(this);
        if (m_pCntLocation)
        {
            /* There is no location container in short wizard form: */
            if (!m_fFullWizard)
                m_pCntLocation->setVisible(false);

            /* Prepare location layout: */
            QVBoxLayout *pLocationLayout = new QVBoxLayout(m_pCntLocation);
            if (pLocationLayout)
            {
                /* Prepare location combo-box: */
                m_pProviderComboBox = new QIComboBox(m_pCntLocation);
                if (m_pProviderComboBox)
                    pLocationLayout->addWidget(m_pProviderComboBox);

                /* Prepare profile layout: */
                QHBoxLayout *pProfileLayout = new QHBoxLayout;
                if (pProfileLayout)
                {
                    pProfileLayout->setContentsMargins(0, 0, 0, 0);
                    pProfileLayout->setSpacing(1);

                    /* Prepare profile combo-box: */
                    m_pProfileComboBox = new QIComboBox(m_pCntLocation);
                    if (m_pProfileComboBox)
                        pProfileLayout->addWidget(m_pProfileComboBox);

                    /* Prepare profile tool-button: */
                    m_pProfileToolButton = new QIToolButton(m_pCntLocation);
                    if (m_pProfileToolButton)
                    {
                        m_pProfileToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                          ":/cloud_profile_manager_disabled_16px.png"));
                        pProfileLayout->addWidget(m_pProfileToolButton);
                    }

                    /* Add into layout: */
                    pLocationLayout->addLayout(pProfileLayout);
                }
            }

            /* Add into layout: */
            pLayoutMain->addWidget(m_pCntLocation, 0, 0);
        }

        /* Prepare source container: */
        m_pCntSource = new QGroupBox(this);
        if (m_pCntSource)
        {
            /* There is no source table in short wizard form: */
            if (!m_fFullWizard)
                m_pCntSource->setVisible(false);

            /* Prepare source layout: */
            QVBoxLayout *pSourceLayout = new QVBoxLayout(m_pCntSource);
            if (pSourceLayout)
            {
                pSourceLayout->setSpacing(0);

                /* Prepare source tab-bar: */
                m_pSourceTabBar = new QTabBar(this);
                if (m_pSourceTabBar)
                {
                    m_pSourceTabBar->addTab(QString());
                    m_pSourceTabBar->addTab(QString());

                    /* Add into layout: */
                    pSourceLayout->addWidget(m_pSourceTabBar);
                }

                /* Prepare source image list: */
                m_pSourceImageList = new QListWidget(m_pCntSource);
                if (m_pSourceImageList)
                {
                    m_pSourceImageList->setSortingEnabled(true);
                    /* Make source image list fit 40/50 symbols
                     * horizontally and 8 lines vertically: */
                    const QFontMetrics fm(m_pSourceImageList->font());
                    const int iFontWidth = fm.width('x');
#ifdef VBOX_WS_MAC
                    const int iTotalWidth = 40 * iFontWidth;
#else
                    const int iTotalWidth = 50 * iFontWidth;
#endif
                    const int iFontHeight = fm.height();
                    const int iTotalHeight = 8 * iFontHeight;
                    m_pSourceImageList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                    m_pSourceImageList->setAlternatingRowColors(true);

                    /* Add into layout: */
                    pSourceLayout->addWidget(m_pSourceImageList);
                }
            }

            /* Add into layout: */
            pLayoutMain->addWidget(m_pCntSource, 1, 0);
        }

        /* Prepare settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Prepare settings layout: */
            QVBoxLayout *pSettingsLayout = new QVBoxLayout(m_pSettingsCnt);
            if (pSettingsLayout)
            {
                /* Prepare form editor widget: */
                m_pFormEditor = new UIFormEditorWidget(m_pSettingsCnt);
                if (m_pFormEditor)
                {
                    /* Make form editor fit fit 40/50 symbols
                     * horizontally and 8 sections vertically: */
                    const QFontMetrics fm(m_pSourceImageList->font());
                    const int iFontWidth = fm.width('x');
#ifdef VBOX_WS_MAC
                    const int iTotalWidth = 40 * iFontWidth;
#else
                    const int iTotalWidth = 50 * iFontWidth;
#endif
                    const int iSectionHeight = m_pFormEditor->verticalHeader()
                                             ? m_pFormEditor->verticalHeader()->defaultSectionSize()
                                             : fm.height();
                    const int iTotalHeight = 8 * iSectionHeight;
                    m_pFormEditor->setMinimumSize(QSize(iTotalWidth, iTotalHeight));

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFormEditor);
                }
            }

            /* Add into layout: */
            pLayoutMain->addWidget(m_pSettingsCnt, 0, 1, 2, 1);
        }
    }

    /* Setup connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileRegistered,
            this, &UIWizardNewCloudVMPageExpert::sltHandleProviderComboChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigCloudProfileChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleProviderComboChange);
    connect(m_pProviderComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageExpert::sltHandleProviderComboChange);
    connect(m_pProfileComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageExpert::sltHandleProfileComboChange);
    connect(m_pProfileToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageExpert::sltHandleProfileButtonClick);
    connect(m_pSourceTabBar, &QTabBar::currentChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleSourceChange);
    connect(m_pSourceImageList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleSourceImageChange);
}

void UIWizardNewCloudVMPageExpert::retranslateUi()
{
    /* Translate location container: */
    m_pCntLocation->setTitle(UIWizardNewCloudVM::tr("Location"));

    /* Translate received values of Location combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pProviderComboBox->count(); ++i)
    {
        m_pProviderComboBox->setItemText(i, m_pProviderComboBox->itemData(i, ProviderData_Name).toString());
        m_pProviderComboBox->setItemData(i, UIWizardNewCloudVM::tr("Create VM for cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate source container: */
    m_pCntSource->setTitle(UIWizardNewCloudVM::tr("Source"));

    /* Translate source tab-bar: */
    m_pSourceTabBar->setTabText(0, UIWizardNewCloudVM::tr("&Boot Volumes"));
    m_pSourceTabBar->setTabText(1, UIWizardNewCloudVM::tr("&Images"));

    /* Translate profile stuff: */
    m_pProfileToolButton->setToolTip(UIWizardNewCloudVM::tr("Open Cloud Profile Manager..."));

    /* Translate settings container: */
    m_pSettingsCnt->setTitle(UIWizardNewCloudVM::tr("Settings"));

    /* Update tool-tips: */
    updateComboToolTip(m_pProviderComboBox);
}

void UIWizardNewCloudVMPageExpert::initializePage()
{
    if (m_fFullWizard)
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
    else
    {
        /* Generate VSD form, asynchronously: */
        QMetaObject::invokeMethod(this, "sltInitShortWizardForm", Qt::QueuedConnection);
    }
}

bool UIWizardNewCloudVMPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    client().isNotNull()
              && vsd().isNotNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Make sure table has own data committed: */
    m_pFormEditor->makeSureEditorDataCommitted();

    /* Check whether we have proper VSD form: */
    CVirtualSystemDescriptionForm comForm = vsdForm();
    /* Give changed VSD back: */
    if (comForm.isNotNull())
    {
        comForm.GetVirtualSystemDescription();
        fResult = comForm.isOk();
        if (!fResult)
            msgCenter().cannotAcquireVirtualSystemDescriptionFormProperty(comForm);
    }

    /* Try to create cloud VM: */
    if (fResult)
    {
        fResult = qobject_cast<UIWizardNewCloudVM*>(wizard())->createCloudVM();

        /* If the final step failed we could try
         * sugest user more valid form this time: */
        if (!fResult)
        {
            setVSDForm(CVirtualSystemDescriptionForm());
            sltInitShortWizardForm();
        }
    }

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageExpert::sltHandleProviderComboChange()
{
    updateProvider();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleProfileComboChange()
{
    updateProfile();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleProfileButtonClick()
{
    if (gpManager)
        gpManager->openCloudProfileManager();
}

void UIWizardNewCloudVMPageExpert::sltHandleSourceChange()
{
    updateSource();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleSourceImageChange()
{
    updateSourceImage();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltInitShortWizardForm()
{
    if (vsdForm().isNull())
        qobject_cast<UIWizardNewCloudVM*>(wizard())->createVSDForm();
    updatePropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::setShortProviderName(const QString &strShortProviderName)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setShortProviderName(strShortProviderName);
}

QString UIWizardNewCloudVMPageExpert::shortProviderName() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->shortProviderName();
}

void UIWizardNewCloudVMPageExpert::setProfileName(const QString &strProfileName)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setProfileName(strProfileName);
}

QString UIWizardNewCloudVMPageExpert::profileName() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->profileName();
}

void UIWizardNewCloudVMPageExpert::setClient(const CCloudClient &comClient)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setClient(comClient);
}

CCloudClient UIWizardNewCloudVMPageExpert::client() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->client();
}

void UIWizardNewCloudVMPageExpert::setVSD(const CVirtualSystemDescription &comDescription)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setVSD(comDescription);
}

CVirtualSystemDescription UIWizardNewCloudVMPageExpert::vsd() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsd();
}

void UIWizardNewCloudVMPageExpert::setVSDForm(const CVirtualSystemDescriptionForm &comForm)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setVSDForm(comForm);
}

CVirtualSystemDescriptionForm UIWizardNewCloudVMPageExpert::vsdForm() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsdForm();
}

void UIWizardNewCloudVMPageExpert::updateProvider()
{
    updateComboToolTip(m_pProviderComboBox);
    setShortProviderName(m_pProviderComboBox->currentData(ProviderData_ShortName).toString());
    CCloudProvider comCloudProvider = cloudProviderByShortName(shortProviderName(), wizard());
    populateProfiles(m_pProfileComboBox, comCloudProvider);
    updateProfile();
}

void UIWizardNewCloudVMPageExpert::updateProfile()
{
    setProfileName(m_pProfileComboBox->currentData(ProfileData_Name).toString());
    setClient(cloudClientByName(shortProviderName(), profileName(), wizard()));
    updateSource();
}

void UIWizardNewCloudVMPageExpert::updateSource()
{
    populateSourceImages(m_pSourceImageList, m_pSourceTabBar, client());
    updateSourceImage();
}

void UIWizardNewCloudVMPageExpert::updateSourceImage()
{
    m_strSourceImageId = currentListWidgetData(m_pSourceImageList);
    updateVSDForm();
}

void UIWizardNewCloudVMPageExpert::updateVSDForm()
{
    setVSD(createVirtualSystemDescription(wizard()));
    populateFormProperties(vsd(), m_pSourceTabBar, m_strSourceImageId);
    qobject_cast<UIWizardNewCloudVM*>(wizard())->createVSDForm();
    updatePropertiesTable();
}

void UIWizardNewCloudVMPageExpert::updatePropertiesTable()
{
    refreshFormPropertiesTable(m_pFormEditor, vsdForm());
}
