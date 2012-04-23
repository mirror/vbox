/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic3 class implementation
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
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
#include "UIWizardFirstRunPageBasic3.h"
#include "UIWizardFirstRun.h"
#include "QIRichTextLabel.h"

UIWizardFirstRunPage3::UIWizardFirstRunPage3(bool fBootHardDiskWasSet)
    : m_fBootHardDiskWasSet(fBootHardDiskWasSet)
{
}

UIWizardFirstRunPageBasic3::UIWizardFirstRunPageBasic3(bool fBootHardDiskWasSet)
    : UIWizardFirstRunPage3(fBootHardDiskWasSet)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel1 = new QIRichTextLabel(this);
        m_pSummaryText = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
        pMainLayout->addWidget(m_pLabel1);
        pMainLayout->addWidget(m_pSummaryText);
        pMainLayout->addWidget(m_pLabel2);
        pMainLayout->addStretch();
    }
}

void UIWizardFirstRunPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(tr("Summary"));

    /* Translate widgets: */
    if (m_fBootHardDiskWasSet)
    {
        m_pLabel1->setText(tr("<p>You have selected the following media to boot from:</p>"));
        m_pLabel2->setText(tr("<p>If the above is correct, press the <b>Finish</b> button. "
                              "Once you press it, the selected media will be temporarily mounted on the virtual machine "
                              "and the machine will start execution.</p><p>"
                              "Please note that when you close the virtual machine, the specified media will be "
                              "automatically unmounted and the boot device will be set back to the first hard disk."
                              "</p><p>Depending on the type of the setup program, you may need to manually "
                              "unmount (eject) the media after the setup program reboots the virtual machine, "
                              "to prevent the installation process from starting again. "
                              "You can do this by selecting the corresponding <b>Unmount...</b> action "
                              "in the <b>Devices</b> menu.</p>"));
    }
    else
    {
        m_pLabel1->setText(tr("<p>You have selected the following media to boot an operating system from:</p>"));
        m_pLabel2->setText(tr("<p>If the above is correct, press the <b>Finish</b> button. "
                              "Once you press it, the selected media will be mounted on the virtual machine "
                              "and the machine will start execution.</p>"));
    }

    /* Compose summary: */
    QString strSummary;
    QString strDescription = tr("CD/DVD-ROM Device");
    QString strSource = field("source").toString();
    strSummary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td><nobr>%2</nobr></td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td><nobr>%4</nobr></td></tr>"
    )
    .arg(tr("Type", "summary"), strDescription)
    .arg(tr("Source", "summary"), strSource);
    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + strSummary + "</table>");
}

void UIWizardFirstRunPageBasic3::initializePage()
{
    /* Reranslate page: */
    retranslateUi();

    /* Summary should initially have focus: */
    m_pSummaryText->setFocus();
}

bool UIWizardFirstRunPageBasic3::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to insert chosen medium: */
    if (fResult)
        fResult = qobject_cast<UIWizardFirstRun*>(wizard())->insertMedium();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

