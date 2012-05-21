/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardImportAppPageExpert class implementation
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
#include "UIWizardImportAppPageExpert.h"
#include "UIWizardImportApp.h"
#include "VBoxGlobal.h"
#include "QILabelSeparator.h"
#include "VBoxFilePathSelectorWidget.h"
#include "UIApplianceImportEditorWidget.h"

UIWizardImportAppPageExpert::UIWizardImportAppPageExpert(const QString &strFileName)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        pMainLayout->setContentsMargins(8, 6, 8, 6);
        m_pVMApplianceLabel = new QILabelSeparator(this);
        m_pFileSelector = new VBoxEmptyFileSelector(this);
        {
            m_pFileSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Open);
            m_pFileSelector->setHomeDir(vboxGlobal().documentsPath());
        }
        m_pApplianceWidget = new UIApplianceImportEditorWidget(this);
        {
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            m_pApplianceWidget->setFile(strFileName);
        }
        pMainLayout->addWidget(m_pVMApplianceLabel);
        pMainLayout->addWidget(m_pFileSelector);
        pMainLayout->addWidget(m_pApplianceWidget);
    }

    /* Setup connections: */
    connect(m_pFileSelector, SIGNAL(pathChanged(const QString&)), this, SLOT(sltFilePathChangeHandler()));

    /* Register classes: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register fields: */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardImportAppPageExpert::sltFilePathChangeHandler()
{
    /* Check if set file contains valid appliance: */
    if (m_pApplianceWidget->setFile(m_pFileSelector->path()))
    {
        /* Reset the modified bit if file was correctly set: */
        m_pFileSelector->resetModified();
    }

    emit completeChanged();
}

void UIWizardImportAppPageExpert::retranslateUi()
{
    /* Translate widgets: */
    m_pVMApplianceLabel->setText(UIWizardImportApp::tr("Appliance to import"));
    m_pFileSelector->setChooseButtonText(UIWizardImportApp::tr("Open appliance..."));
    m_pFileSelector->setFileDialogTitle(UIWizardImportApp::tr("Select an appliance to import"));
    m_pFileSelector->setFileFilters(UIWizardImportApp::tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));
}

void UIWizardImportAppPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardImportAppPageExpert::isComplete() const
{
    /* Make sure appliance file has allowed extension and exists and appliance widget is valid: */
    return VBoxGlobal::hasAllowedExtension(m_pFileSelector->path().toLower(), VBoxDefs::OVFFileExts) &&
           QFileInfo(m_pFileSelector->path()).exists() &&
           m_pApplianceWidget->isValid();
}

bool UIWizardImportAppPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to import appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardImportApp*>(wizard())->importAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

