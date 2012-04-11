/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVMPageBasic2 class implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>

/* Local includes: */
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVM.h"
#include "COMDefs.h"
#include "QIRichTextLabel.h"

UIWizardCloneVMPageBasic2::UIWizardCloneVMPageBasic2(bool fAdditionalInfo)
    : m_fAdditionalInfo(fAdditionalInfo)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pFullCloneRadio = new QRadioButton(this);
            m_pFullCloneRadio->setChecked(true);
        m_pLinkedCloneRadio = new QRadioButton(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pFullCloneRadio);
    pMainLayout->addWidget(m_pLinkedCloneRadio);
    pMainLayout->addStretch();

    /* Create button-group: */
    QButtonGroup *pButtonGroup = new QButtonGroup(this);
    pButtonGroup->addButton(m_pFullCloneRadio);
    pButtonGroup->addButton(m_pLinkedCloneRadio);

    /* Setup connections: */
    connect(pButtonGroup, SIGNAL(buttonClicked(QAbstractButton *)), this, SLOT(buttonClicked(QAbstractButton *)));

    /* Register fields: */
    registerField("linkedClone", this, "linkedClone");
}

void UIWizardCloneVMPageBasic2::buttonClicked(QAbstractButton *pButton)
{
    setFinalPage(pButton != m_pFullCloneRadio);
    /* On older Qt versions the content of the current page isn't updated when
     * using setFinalPage. So switch back and for to simulate it. */
#if QT_VERSION < 0x040700
    wizard()->back();
    wizard()->next();
#endif
}

void UIWizardCloneVMPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVM::tr("Cloning Configuration"));

    /* Translate widgets: */
    m_pFullCloneRadio->setText(UIWizardCloneVM::tr("Full Clone"));
    m_pLinkedCloneRadio->setText(UIWizardCloneVM::tr("Linked Clone"));

    /* Set label text: */
    QString strLabel = UIWizardCloneVM::tr("<p>Please select the type of the clone.</p><p>If you choose <b>Full Clone</b> an exact copy "
                                           "(including all virtual disk images) of the original VM will be created. If you select <b>Linked Clone</b>, "
                                           "a new VM will be created, but the virtual disk images will point to the virtual disk images of original VM.</p>");
    if (m_fAdditionalInfo)
        strLabel += UIWizardCloneVM::tr("<p>Note that a new snapshot within the source VM is created in case you select <b>Linked Clone</b>.</p>");
    m_pLabel->setText(strLabel);
}

void UIWizardCloneVMPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVMPageBasic2::validatePage()
{
    if (isFinalPage())
    {
        /* Try to create the clone: */
        startProcessing();
        bool fResult = qobject_cast<UIWizardCloneVM*>(wizard())->cloneVM();
        endProcessing();
        return fResult;
    }
    else
        return true;
}

int UIWizardCloneVMPageBasic2::nextId() const
{
    return m_pFullCloneRadio->isChecked() && wizard()->page(UIWizardCloneVM::Page3) ? UIWizardCloneVM::Page3 : -1;
}

bool UIWizardCloneVMPageBasic2::isLinkedClone() const
{
    return m_pLinkedCloneRadio->isChecked();
}

