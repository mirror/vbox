/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVMPageBasic1 class implementation
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
#include <QLineEdit>
#include <QCheckBox>

/* Local includes: */
#include "UIWizardCloneVMPageBasic1.h"
#include "UIWizardCloneVM.h"
#include "COMDefs.h"
#include "QIRichTextLabel.h"

UIWizardCloneVMPageBasic1::UIWizardCloneVMPageBasic1(const QString &strOriginalName)
    : m_strOriginalName(strOriginalName)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel1 = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
        m_pNameEditor = new QLineEdit(this);
            m_pNameEditor->setText(UIWizardCloneVM::tr("%1 Clone").arg(m_strOriginalName));
        m_pReinitMACsCheckBox = new QCheckBox(this);
    pMainLayout->addWidget(m_pLabel1);
    pMainLayout->addWidget(m_pLabel2);
    pMainLayout->addWidget(m_pNameEditor);
    pMainLayout->addWidget(m_pReinitMACsCheckBox);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pNameEditor, SIGNAL(textChanged(const QString&)), this, SIGNAL(completeChanged()));

    /* Register fields: */
    registerField("cloneName", this, "cloneName");
    registerField("reinitMACs", this, "reinitMACs");
}

void UIWizardCloneVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVM::tr("Welcome to the virtual machine clone wizard"));

    /* Translate widgets: */
    m_pLabel1->setText(UIWizardCloneVM::tr("<p>This wizard will help you to create a clone of your virtual machine.</p>"));
    m_pLabel2->setText(UIWizardCloneVM::tr("<p>Please choose a name for the new virtual machine:</p>"));
    m_pReinitMACsCheckBox->setToolTip(UIWizardCloneVM::tr("When checked a new unique MAC address will be assigned to all configured network cards."));
    m_pReinitMACsCheckBox->setText(UIWizardCloneVM::tr("&Reinitialize the MAC address of all network cards"));

    /* Append page text with common part: */
    QString strCommonPart = QString("<p>%1</p>").arg(standardHelpText());
    m_pLabel1->setText(m_pLabel1->text() + strCommonPart);
}

void UIWizardCloneVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVMPageBasic1::isComplete() const
{
    QString strName = m_pNameEditor->text().trimmed();
    return !strName.isEmpty() && strName != m_strOriginalName;
}

bool UIWizardCloneVMPageBasic1::validatePage()
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

QString UIWizardCloneVMPageBasic1::cloneName() const
{
    return m_pNameEditor->text();
}

void UIWizardCloneVMPageBasic1::setCloneName(const QString &strName)
{
    m_pNameEditor->setText(strName);
}

bool UIWizardCloneVMPageBasic1::isReinitMACsChecked() const
{
    return m_pReinitMACsCheckBox->isChecked();
}

