/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic1 class implementation.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#include <QLabel>
#include <QStackedLayout>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIRichTextLabel.h"
#include "VBoxGlobal.h"
#include "UIEmptyFilePathSelector.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"


/*********************************************************************************************************************************
*   Namespace ImportSourceTypeConverter implementation.                                                                          *
*********************************************************************************************************************************/

QString ImportSourceTypeConverter::toString(ImportSourceType enmType)
{
    switch (enmType)
    {
        case ImportSourceType_Local: return QApplication::translate("UIWizardImportApp", "Local File System");
        case ImportSourceType_Cloud: return QApplication::translate("UIWizardImportApp", "Cloud Content Provider");
        default: break;
    }
    return QString();
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPage1 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardImportAppPage1::UIWizardImportAppPage1()
    : m_pSourceLabel(0)
    , m_pSourceSelector(0)
    , m_pStackedLayout(0)
    , m_pFileSelector(0)
{
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPageBasic1 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardImportAppPageBasic1::UIWizardImportAppPageBasic1()
    : m_pLabel(0)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create source selector layout: */
        QHBoxLayout *pSourceSelectorLayout = new QHBoxLayout;
        if (pSourceSelectorLayout)
        {
            /* Create source label: */
            m_pSourceLabel = new QLabel(this);
            if (m_pSourceLabel)
            {
                m_pSourceLabel->hide();
                m_pSourceLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
                m_pSourceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

                /* Add into layout: */
                pSourceSelectorLayout->addWidget(m_pSourceLabel);
            }

            /* Create source selector: */
            m_pSourceSelector = new QIComboBox(this);
            if (m_pSourceSelector)
            {
                m_pSourceLabel->setBuddy(m_pSourceSelector);
                m_pSourceSelector->hide();
                m_pSourceSelector->addItem(QString(), QVariant::fromValue(ImportSourceType_Local));
                m_pSourceSelector->addItem(QString(), QVariant::fromValue(ImportSourceType_Cloud));
                connect(m_pSourceSelector, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated),
                        this, static_cast<void(UIWizardImportAppPageBasic1::*)(int)>(&UIWizardImportAppPageBasic1::sltHandleSourceChange));

                /* Add into layout: */
                pSourceSelectorLayout->addWidget(m_pSourceSelector);
            }

            /* Add into layout: */
            pMainLayout->addLayout(pSourceSelectorLayout);
        }

        /* Create stacked layout: */
        m_pStackedLayout = new QStackedLayout;
        if (m_pStackedLayout)
        {
            /* Create local container: */
            QWidget *pLocalContainer = new QWidget(this);
            if (pLocalContainer)
            {
                /* Create local container layout: */
                QVBoxLayout *pLocalContainerLayout = new QVBoxLayout(pLocalContainer);
                if (pLocalContainerLayout)
                {
                    pLocalContainerLayout->setContentsMargins(0, 0, 0, 0);
                    pLocalContainerLayout->setSpacing(0);

                    /* Create file-path selector: */
                    m_pFileSelector = new UIEmptyFilePathSelector(this);
                    if (m_pFileSelector)
                    {
                        m_pFileSelector->setHomeDir(vboxGlobal().documentsPath());
                        m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
                        m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
                        m_pFileSelector->setEditable(true);

                        /* Add into layout: */
                        pLocalContainerLayout->addWidget(m_pFileSelector);
                    }

                    /* Add stretch: */
                    pLocalContainerLayout->addStretch();
                }

                /* Add into layout: */
                m_stackedLayoutIndexMap[ImportSourceType_Local] = m_pStackedLayout->addWidget(pLocalContainer);
            }

            /* Create cloud container: */
            QWidget *pCloudContainer = new QWidget(this);
            if (pCloudContainer)
            {
                /* Create cloud container layout: */
                QVBoxLayout *pCloudContainerLayout = new QVBoxLayout(pCloudContainer);
                if (pCloudContainerLayout)
                {
                    pCloudContainerLayout->setContentsMargins(0, 0, 0, 0);
                    pCloudContainerLayout->setSpacing(0);

                    /* Add stretch: */
                    pCloudContainerLayout->addStretch();
                }

                /* Add into layout: */
                m_stackedLayoutIndexMap[ImportSourceType_Cloud] = m_pStackedLayout->addWidget(pCloudContainer);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pStackedLayout);
        }

        /* Add vertical stretch finally: */
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardImportAppPageBasic1::completeChanged);
}

void UIWizardImportAppPageBasic1::sltHandleSourceChange(int iIndex)
{
    m_pStackedLayout->setCurrentIndex(m_stackedLayoutIndexMap.value(m_pSourceSelector->itemData(iIndex).value<ImportSourceType>()));
}

void UIWizardImportAppPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance to import"));

    /* Translate label: */
    m_pLabel->setText(UIWizardImportApp::tr("<p>VirtualBox currently supports importing appliances "
                                            "saved in the Open Virtualization Format (OVF). "
                                            "To continue, select the file to import below.</p>"));

    /* Translate source selector: */
    m_pSourceLabel->setText(tr("Source:"));
    for (int i = 0; i < m_pSourceSelector->count(); ++i)
        m_pSourceSelector->setItemText(i, ImportSourceTypeConverter::toString(m_pSourceSelector->itemData(i).value<ImportSourceType>()));

    /* Translate file selector: */
    m_pFileSelector->setChooseButtonToolTip(UIWizardImportApp::tr("Choose a virtual appliance file to import..."));
    m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Please choose a virtual appliance file to import"));
    m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));
}

void UIWizardImportAppPageBasic1::initializePage()
{
    retranslateUi();
}

bool UIWizardImportAppPageBasic1::isComplete() const
{
    /* Make sure appliance file has allowed extension and exists: */
    return    VBoxGlobal::hasAllowedExtension(m_pFileSelector->path().toLower(), OVFFileExts)
           && QFile::exists(m_pFileSelector->path());
}

bool UIWizardImportAppPageBasic1::validatePage()
{
    /* Get import appliance widget: */
    ImportAppliancePointer pImportApplianceWidget = field("applianceWidget").value<ImportAppliancePointer>();
    AssertMsg(!pImportApplianceWidget.isNull(), ("Appliance Widget is not set!\n"));

    /* If file name was changed: */
    if (m_pFileSelector->isModified())
    {
        /* Check if set file contains valid appliance: */
        if (!pImportApplianceWidget->setFile(m_pFileSelector->path()))
            return false;
        /* Reset the modified bit afterwards: */
        m_pFileSelector->resetModified();
    }

    /* If we have a valid ovf proceed to the appliance settings page: */
    return pImportApplianceWidget->isValid();
}
