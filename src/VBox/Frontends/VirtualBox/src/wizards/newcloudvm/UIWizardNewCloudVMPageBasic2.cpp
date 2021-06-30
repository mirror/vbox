/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic2 class implementation.
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
#include <QHeaderView>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIMessageCenter.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic2.h"

/* Namespaces: */
using namespace UIWizardNewCloudVMPage2;


/*********************************************************************************************************************************
*   Namespace UIWizardNewCloudVMPage2 implementation.                                                                            *
*********************************************************************************************************************************/

void UIWizardNewCloudVMPage2::refreshFormPropertiesTable(UIFormEditorWidgetPointer pFormEditor,
                                                         const CVirtualSystemDescriptionForm &comForm)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pFormEditor.data());
    AssertReturnVoid(comForm.isNotNull());

    /* Make sure the properties table get the new description form: */
    pFormEditor->setVirtualSystemDescriptionForm(comForm);
}


/*********************************************************************************************************************************
*   Class UIWizardNewCloudVMPageBasic2 implementation.                                                                           *
*********************************************************************************************************************************/

UIWizardNewCloudVMPageBasic2::UIWizardNewCloudVMPageBasic2()
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
            /* Make form-editor fit 8 sections in height by default: */
            const int iDefaultSectionHeight = m_pFormEditor->verticalHeader()
                                            ? m_pFormEditor->verticalHeader()->defaultSectionSize()
                                            : 0;
            if (iDefaultSectionHeight > 0)
                m_pFormEditor->setMinimumHeight(8 * iDefaultSectionHeight);

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
    /* Generate VSD form, asynchronously: */
    QMetaObject::invokeMethod(this, "sltInitShortWizardForm", Qt::QueuedConnection);
}

bool UIWizardNewCloudVMPageBasic2::isComplete() const
{
    /* Initial result: */
    bool fResult = true;

    /* Check cloud settings: */
    fResult =    client().isNotNull()
              && vsd().isNotNull();

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVMPageBasic2::validatePage()
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

void UIWizardNewCloudVMPageBasic2::sltInitShortWizardForm()
{
    /* Create Virtual System Description Form: */
    if (vsdForm().isNull())
        qobject_cast<UIWizardNewCloudVM*>(wizard())->createVSDForm();

    /* Refresh form properties table: */
    refreshFormPropertiesTable(m_pFormEditor, vsdForm());
    emit completeChanged();
}

CCloudClient UIWizardNewCloudVMPageBasic2::client() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->client();
}

CVirtualSystemDescription UIWizardNewCloudVMPageBasic2::vsd() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsd();
}

void UIWizardNewCloudVMPageBasic2::setVSDForm(const CVirtualSystemDescriptionForm &comForm)
{
    qobject_cast<UIWizardNewCloudVM*>(wizard())->setVSDForm(comForm);
}

CVirtualSystemDescriptionForm UIWizardNewCloudVMPageBasic2::vsdForm() const
{
    return qobject_cast<UIWizardNewCloudVM*>(wizard())->vsdForm();
}
