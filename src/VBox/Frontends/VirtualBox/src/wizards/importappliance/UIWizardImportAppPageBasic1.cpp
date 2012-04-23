/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardImportAppPageBasic1 class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QFileInfo>
#include <QVBoxLayout>

/* Local includes: */
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"
#include "UIWizardImportApp.h"
#include "VBoxGlobal.h"
#include "VBoxFilePathSelectorWidget.h"
#include "QIRichTextLabel.h"

UIWizardImportAppPage1::UIWizardImportAppPage1()
{
}

UIWizardImportAppPageBasic1::UIWizardImportAppPageBasic1()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pFileSelector = new VBoxEmptyFileSelector(this);
        {
            m_pFileSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Open);
            m_pFileSelector->setHomeDir(vboxGlobal().documentsPath());
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pFileSelector);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pFileSelector, SIGNAL(pathChanged(const QString&)), this, SIGNAL(completeChanged()));
}

void UIWizardImportAppPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Welcome to the Appliance Import wizard!"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardImportApp::tr("<p>This wizard will guide you through importing an appliance.</p>"
                                            "<p>%1</p><p>VirtualBox currently supports importing appliances "
                                            "saved in the Open Virtualization Format (OVF). To continue, "
                                            "select the file to import below:</p>")
                                           .arg(standardHelpText()));
    m_pFileSelector->setChooseButtonText(UIWizardImportApp::tr("Open appliance..."));
    m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Select an appliance to import"));
    m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));
}

void UIWizardImportAppPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardImportAppPageBasic1::isComplete() const
{
    /* Make sure appliance file has allowed extension and exists: */
    return VBoxGlobal::hasAllowedExtension(m_pFileSelector->path().toLower(), VBoxDefs::OVFFileExts) &&
           QFileInfo(m_pFileSelector->path()).exists();
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

