/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVM.h"
#include "QIRichTextLabel.h"
#include "UIWizardCloneVMEditors.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardCloneVMPageBasic2::UIWizardCloneVMPageBasic2(bool fAdditionalInfo)
    : m_pLabel(0)
    , m_fAdditionalInfo(fAdditionalInfo)
    , m_pCloneTypeGroupBox(0)
{
    prepare();
}

void UIWizardCloneVMPageBasic2::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pLabel = new QIRichTextLabel(this);
    if (m_pLabel)
        pMainLayout->addWidget(m_pLabel);

    m_pCloneTypeGroupBox = new UICloneVMCloneTypeGroupBox;
    if (m_pCloneTypeGroupBox)
    {
        m_pCloneTypeGroupBox->setFlat(true);
        pMainLayout->addWidget(m_pCloneTypeGroupBox);
        connect(m_pCloneTypeGroupBox, &UICloneVMCloneTypeGroupBox::sigFullCloneSelected,
                this, &UIWizardCloneVMPageBasic2::sltCloneTypeChanged);
    }

    pMainLayout->addStretch();
}

void UIWizardCloneVMPageBasic2::sltCloneTypeChanged(bool fIsFullClone)
{
    UIWizardCloneVM *pWizard = qobject_cast<UIWizardCloneVM*>(wizard());
    AssertReturnVoid(pWizard);
    pWizard->setCloneModePageVisible(fIsFullClone);
}

void UIWizardCloneVMPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVM::tr("Clone type"));

    /* Translate widgets: */
    QString strLabel = UIWizardCloneVM::tr("<p>Please choose the type of clone you wish to create.</p>"
                                           "<p>If you choose <b>Full clone</b>, "
                                           "an exact copy (including all virtual hard disk files) "
                                           "of the original virtual machine will be created.</p>"
                                           "<p>If you choose <b>Linked clone</b>, "
                                           "a new machine will be created, but the virtual hard disk files "
                                           "will be tied to the virtual hard disk files of original machine "
                                           "and you will not be able to move the new virtual machine "
                                           "to a different computer without moving the original as well.</p>");
    if (m_fAdditionalInfo)
        strLabel += UIWizardCloneVM::tr("<p>If you create a <b>Linked clone</b> then a new snapshot will be created "
                                        "in the original virtual machine as part of the cloning process.</p>");
    if (m_pLabel)
        m_pLabel->setText(strLabel);
}

void UIWizardCloneVMPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVMPageBasic2::validatePage()
{
    UIWizardCloneVM *pWizard = qobject_cast<UIWizardCloneVM*>(wizard());
    AssertReturn(pWizard, false);

    /* This page could be final: */
    if (!pWizard->isCloneModePageVisible())
    {
        /* Initial result: */
        bool fResult = true;

        /* Trying to clone VM: */
        fResult = pWizard->cloneVM();

        /* Return result: */
        return fResult;
    }
    else
        return true;
}
