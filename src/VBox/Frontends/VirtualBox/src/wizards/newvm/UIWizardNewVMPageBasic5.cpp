/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic5 class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

/* Local includes: */
#include "UIWizardNewVMPageBasic5.h"
#include "UIWizardNewVM.h"
#include "VBoxGlobal.h"
#include "QIRichTextLabel.h"

UIWizardNewVMPage5::UIWizardNewVMPage5()
{
}

UIWizardNewVMPageBasic5::UIWizardNewVMPageBasic5()
{
    /* Create widget: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel1 = new QIRichTextLabel(this);
        m_pSummary = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
        pMainLayout->addWidget(m_pLabel1);
        pMainLayout->addWidget(m_pSummary);
        pMainLayout->addWidget(m_pLabel2);
        pMainLayout->addStretch();
    }
}

void UIWizardNewVMPageBasic5::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Summary"));

    /* Translate widgets: */
    m_pLabel1->setText(UIWizardNewVM::tr("<p>You are going to create a new virtual machine with the following parameters:</p>"));
    m_pLabel2->setText(UIWizardNewVM::tr("<p>If the above is correct press the <b>%1</b> button. Once "
                                         "you press it, a new virtual machine will be created. </p><p>Note "
                                         "that you can alter these and all other setting of the created "
                                         "virtual machine at any time using the <b>Settings</b> dialog "
                                         "accessible through the menu of the main window.</p>")
                                        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::FinishButton)))));

    /* Compose common summary: */
    QString strSummary;
    QString strName = field("name").toString();
    QString strType = field("type").value<CGuestOSType>().isNull() ? QString() : field("type").value<CGuestOSType>().GetDescription();
    QString strRam = QString::number(field("ram").toInt());
    strSummary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td>%2</td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td>%4</td></tr>"
        "<tr><td><nobr>%5: </nobr></td><td>%6 %7</td></tr>"
    )
    .arg(UIWizardNewVM::tr("Name", "summary"), strName)
    .arg(UIWizardNewVM::tr("OS Type", "summary"), strType)
    .arg(UIWizardNewVM::tr("Base Memory", "summary"), strRam, VBoxGlobal::tr("MB", "size suffix MBytes=1024KBytes"));
    /* Add virtual-disk info: */
    if (!field("virtualDiskId").toString().isNull())
    {
        strSummary += QString("<tr><td><nobr>%8: </nobr></td><td><nobr>%9</nobr></td></tr>")
                             .arg(UIWizardNewVM::tr("Start-up Disk", "summary"), field("virtualDiskName").toString());
    }
    m_pSummary->setText("<table cellspacing=0 cellpadding=0>" + strSummary + "</table>");
}

void UIWizardNewVMPageBasic5::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Summary should have focus initially: */
    m_pSummary->setFocus();
}

bool UIWizardNewVMPageBasic5::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to create VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

