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
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "VBoxGlobal.h"
#include "UIEmptyFilePathSelector.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"


/*********************************************************************************************************************************
*   Class UIWizardImportAppPage1 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardImportAppPage1::UIWizardImportAppPage1()
    : m_pFileSelector(0)
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

        /* Create file-path selector: */
        m_pFileSelector = new UIEmptyFilePathSelector(this);
        if (m_pFileSelector)
        {
            m_pFileSelector->setHomeDir(vboxGlobal().documentsPath());
            m_pFileSelector->setMode(UIEmptyFilePathSelector::Mode_File_Open);
            m_pFileSelector->setButtonPosition(UIEmptyFilePathSelector::RightPosition);
            m_pFileSelector->setEditable(true);

            /* Add into layout: */
            pMainLayout->addWidget(m_pFileSelector);
        }

        /* Add vertical stretch finally: */
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pFileSelector, &UIEmptyFilePathSelector::pathChanged, this, &UIWizardImportAppPageBasic1::completeChanged);
}

void UIWizardImportAppPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance to import"));

    /* Translate label: */
    m_pLabel->setText(UIWizardImportApp::tr("<p>VirtualBox currently supports importing appliances "
                                            "saved in the Open Virtualization Format (OVF). "
                                            "To continue, select the file to import below.</p>"));

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
