/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageExpert class implementation.
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
#include <QHeaderView>
#include <QListWidget>
#include <QTabBar>
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


UIWizardNewCloudVMPageExpert::UIWizardNewCloudVMPageExpert(bool fFullWizard)
    : UIWizardNewCloudVMPage2(fFullWizard)
    , m_pCntLocation(0)
    , m_pCntSource(0)
    , m_pSettingsCnt(0)
{
    /* Create main layout: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setRowStretch(0, 0);
        pMainLayout->setRowStretch(1, 1);

        /* Create location container: */
        m_pCntLocation = new QGroupBox(this);
        if (m_pCntLocation)
        {
            /* There is no location container in short wizard form: */
            if (!m_fFullWizard)
                m_pCntLocation->setVisible(false);

            /* Create location layout: */
            QVBoxLayout *pLocationLayout = new QVBoxLayout(m_pCntLocation);
            if (pLocationLayout)
            {
                /* Create location combo-box: */
                m_pLocationComboBox = new QIComboBox(m_pCntLocation);
                if (m_pLocationComboBox)
                {
                    /* Add into layout: */
                    pLocationLayout->addWidget(m_pLocationComboBox);
                }

                /* Create account layout: */
                QHBoxLayout *pAccountLayout = new QHBoxLayout;
                if (pAccountLayout)
                {
                    pAccountLayout->setContentsMargins(0, 0, 0, 0);
                    pAccountLayout->setSpacing(1);

                    /* Create account combo-box: */
                    m_pAccountComboBox = new QIComboBox(m_pCntLocation);
                    if (m_pAccountComboBox)
                    {
                        /* Add into layout: */
                        pAccountLayout->addWidget(m_pAccountComboBox);
                    }
                    /* Create account tool-button: */
                    m_pAccountToolButton = new QIToolButton(m_pCntLocation);
                    if (m_pAccountToolButton)
                    {
                        m_pAccountToolButton->setIcon(UIIconPool::iconSet(":/cloud_profile_manager_16px.png",
                                                                          ":/cloud_profile_manager_disabled_16px.png"));

                        /* Add into layout: */
                        pAccountLayout->addWidget(m_pAccountToolButton);
                    }

                    /* Add into layout: */
                    pLocationLayout->addLayout(pAccountLayout);
                }

                /* Create account property table: */
                m_pAccountPropertyTable = new QTableWidget(m_pCntLocation);
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
                    pLocationLayout->addWidget(m_pAccountPropertyTable);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntLocation, 0, 0);
        }

        /* Create source container: */
        m_pCntSource = new QGroupBox(this);
        if (m_pCntSource)
        {
            /* There is no source table in short wizard form: */
            if (!m_fFullWizard)
                m_pCntSource->setVisible(false);

            /* Create source layout: */
            QVBoxLayout *pSourceLayout = new QVBoxLayout(m_pCntSource);
            if (pSourceLayout)
            {
                pSourceLayout->setSpacing(0);

                /* Create source tab-bar: */
                m_pSourceTabBar = new QTabBar(this);
                if (m_pSourceTabBar)
                {
                    m_pSourceTabBar->addTab(QString());
                    m_pSourceTabBar->addTab(QString());
                    connect(m_pSourceTabBar, &QTabBar::currentChanged,
                            this, &UIWizardNewCloudVMPageExpert::sltHandleSourceChange);

                    /* Add into layout: */
                    pSourceLayout->addWidget(m_pSourceTabBar);
                }

                /* Create source image list: */
                m_pSourceImageList = new QListWidget(m_pCntSource);
                if (m_pSourceImageList)
                {
                    const QFontMetrics fm(m_pSourceImageList->font());
                    const int iFontWidth = fm.width('x');
                    const int iTotalWidth = 50 * iFontWidth;
                    const int iFontHeight = fm.height();
                    const int iTotalHeight = 4 * iFontHeight;
                    m_pSourceImageList->setMinimumSize(QSize(iTotalWidth, iTotalHeight));
                    //m_pSourceImageList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                    m_pSourceImageList->setAlternatingRowColors(true);

                    /* Add into layout: */
                    pSourceLayout->addWidget(m_pSourceImageList);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pCntSource, 1, 0);
        }

        /* Create settings container: */
        m_pSettingsCnt = new QGroupBox(this);
        if (m_pSettingsCnt)
        {
            /* Create settings layout: */
            QVBoxLayout *pSettingsLayout = new QVBoxLayout(m_pSettingsCnt);
            if (pSettingsLayout)
            {
                /* Create form editor widget: */
                m_pFormEditor = new UIFormEditorWidget(m_pSettingsCnt);
                if (m_pFormEditor)
                {
                    /* Make form-editor fit 8 sections in height by default: */
                    const int iDefaultSectionHeight = m_pFormEditor->verticalHeader()
                                                    ? m_pFormEditor->verticalHeader()->defaultSectionSize()
                                                    : 0;
                    if (iDefaultSectionHeight > 0)
                        m_pFormEditor->setMinimumHeight(8 * iDefaultSectionHeight);

                    /* Add into layout: */
                    pSettingsLayout->addWidget(m_pFormEditor);
                }
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsCnt, 0, 1, 2, 1);
        }
    }

    /* Setup connections: */
    if (gpManager)
        connect(gpManager, &UIVirtualBoxManager::sigCloudProfileManagerChange,
                this, &UIWizardNewCloudVMPageExpert::sltHandleLocationChange);
    connect(m_pLocationComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
            this, &UIWizardNewCloudVMPageExpert::sltHandleLocationChange);
    connect(m_pAccountComboBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange);
    connect(m_pAccountToolButton, &QIToolButton::clicked,
            this, &UIWizardNewCloudVMPageExpert::sltHandleAccountButtonClick);
    connect(m_pSourceImageList, &QListWidget::currentRowChanged,
            this, &UIWizardNewCloudVMPageExpert::sltHandleInstanceListChange);

    /* Register fields: */
    registerField("location", this, "location");
    registerField("profileName", this, "profileName");
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
    /* Translate location container: */
    m_pCntLocation->setTitle(UIWizardNewCloudVM::tr("Location"));

    /* Translate received values of Location combo-box.
     * We are enumerating starting from 0 for simplicity: */
    for (int i = 0; i < m_pLocationComboBox->count(); ++i)
    {
        m_pLocationComboBox->setItemText(i, m_pLocationComboBox->itemData(i, LocationData_Name).toString());
        m_pLocationComboBox->setItemData(i, UIWizardNewCloudVM::tr("Create VM for cloud service provider."), Qt::ToolTipRole);
    }

    /* Translate source container: */
    m_pCntSource->setTitle(UIWizardNewCloudVM::tr("Source"));

    /* Translate source tab-bar: */
    m_pSourceTabBar->setTabText(0, UIWizardNewCloudVM::tr("&Boot Volumes"));
    m_pSourceTabBar->setTabText(1, UIWizardNewCloudVM::tr("&Images"));

    /* Translate settings container: */
    m_pSettingsCnt->setTitle(UIWizardNewCloudVM::tr("Settings"));

    /* Update tool-tips: */
    updateLocationComboToolTip();
    updateAccountPropertyTableToolTips();
}

void UIWizardNewCloudVMPageExpert::initializePage()
{
    /* If wasn't polished yet: */
    if (!UIWizardNewCloudVMPage1::m_fPolished || !UIWizardNewCloudVMPage2::m_fPolished)
    {
        if (m_fFullWizard)
        {
            /* Populate locations: */
            populateLocations();
            /* Choose one of them, asynchronously: */
            QMetaObject::invokeMethod(this, "sltHandleLocationChange", Qt::QueuedConnection);
        }
        else
        {
            /* Generate VSD form, asynchronously: */
            QMetaObject::invokeMethod(this, "sltInitShortWizardForm", Qt::QueuedConnection);
        }
        UIWizardNewCloudVMPage1::m_fPolished = true;
        UIWizardNewCloudVMPage2::m_fPolished = true;
    }

    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewCloudVMPageExpert::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    UIWizardNewCloudVMPage1::client().isNotNull()
              && UIWizardNewCloudVMPage1::vsd().isNotNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Make sure table has own data committed: */
    m_pFormEditor->makeSureEditorDataCommitted();

    /* Check whether we have proper VSD form: */
    CVirtualSystemDescriptionForm comForm = UIWizardNewCloudVMPage1::vsdForm();
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
            sltInitShortWizardForm();
    }

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageExpert::sltHandleLocationChange()
{
    /* Update tool-tip: */
    updateLocationComboToolTip();

    /* Make image list focused by default: */
    m_pSourceImageList->setFocus();

    /* Refresh required settings: */
    populateAccounts();
    populateAccountProperties();
    populateSourceImages();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleAccountComboChange()
{
    /* Refresh required settings: */
    populateAccountProperties();
    populateSourceImages();
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

void UIWizardNewCloudVMPageExpert::sltHandleSourceChange()
{
    /* Refresh required settings: */
    populateSourceImages();
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltHandleInstanceListChange()
{
    /* Refresh required settings: */
    populateFormProperties();
    refreshFormPropertiesTable();
    emit completeChanged();
}

void UIWizardNewCloudVMPageExpert::sltInitShortWizardForm()
{
    /* Create Virtual System Description Form: */
    qobject_cast<UIWizardNewCloudVM*>(wizardImp())->createVSDForm();

    /* Refresh form properties table: */
    refreshFormPropertiesTable();
    emit completeChanged();
}
