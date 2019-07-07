/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic2 class implementation.
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
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIMessageCenter.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic2.h"


/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPage2 implementation.                                                                                *
*********************************************************************************************************************************/

UIWizardNewCloudVMPage2::UIWizardNewCloudVMPage2(bool fFullWizard)
    : m_fFullWizard(fFullWizard)
    , m_fPolished(false)
{
}

void UIWizardNewCloudVMPage2::refreshFormPropertiesTable()
{
    /* Acquire VSD form: */
    CVirtualSystemDescriptionForm comForm = vsdForm();
    /* Make sure the properties table get the new description form: */
    if (comForm.isNotNull())
        m_pFormEditor->setVirtualSystemDescriptionForm(comForm);
}

CVirtualSystemDescriptionForm UIWizardNewCloudVMPage2::vsdForm() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizardImp())->vsdForm();
}


/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPageBasic2 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardNewCloudVMPageBasic2::UIWizardNewCloudVMPageBasic2(bool fFullWizard)
    : UIWizardNewCloudVMPage2(fFullWizard)
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

        /* Create form editor widget: */
        m_pFormEditor = new UIFormEditorWidget(this);
        if (m_pFormEditor)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pFormEditor);
        }
    }
}

void UIWizardNewCloudVMPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewCloudVM::tr("Cloud Virtual Machine settings"));

    /* Translate description label: */
    m_pLabel->setText(UIWizardNewCloudVM::tr("These are the the suggested settings of the cloud VM creation procedure, they are "
                                             "influencing the resulting cloud VM instance.  You can change many of the "
                                             "properties shown by double-clicking on the items and disable others using the "
                                             "check boxes below."));
}

void UIWizardNewCloudVMPageBasic2::initializePage()
{
    /* If wasn't polished yet: */
    if (!m_fPolished)
    {
        if (!m_fFullWizard)
        {
            /* Generate VSD form, asynchronously: */
            QMetaObject::invokeMethod(this, "sltInitShortWizardForm", Qt::QueuedConnection);
        }
        m_fPolished = true;
    }

    /* Refresh form properties: */
    refreshFormPropertiesTable();

    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewCloudVMPageBasic2::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Check whether we have proper VSD form: */
    CVirtualSystemDescriptionForm comForm = vsdForm();
    fResult = comForm.isNotNull();
    Assert(fResult);

    /* Give changed VSD back: */
    if (fResult)
    {
        comForm.GetVirtualSystemDescription();
        fResult = comForm.isOk();
        if (!fResult)
            msgCenter().cannotAcquireVirtualSystemDescriptionFormProperty(comForm);
    }

    /* Try to create cloud VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewCloudVM*>(wizard())->createCloudVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVMPageBasic2::sltInitShortWizardForm()
{
    /* Create Virtual System Description Form: */
    qobject_cast<UIWizardNewCloudVM*>(wizardImp())->createVSDForm();

    /* Refresh form properties table: */
    refreshFormPropertiesTable();
}
