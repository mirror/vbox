/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic1 class implementation
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
#include "UIWizardFirstRunPageBasic1.h"
#include "UIWizardFirstRun.h"
#include "QIRichTextLabel.h"

UIWizardFirstRunPage1::UIWizardFirstRunPage1(bool fBootHardDiskWasSet)
    : m_fBootHardDiskWasSet(fBootHardDiskWasSet)
{
}

UIWizardFirstRunPageBasic1::UIWizardFirstRunPageBasic1(bool fBootHardDiskWasSet)
    : UIWizardFirstRunPage1(fBootHardDiskWasSet)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addStretch();
    }
}

void UIWizardFirstRunPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardFirstRun::tr("Welcome to the First Run wizard!"));

    /* Translate widgets: */
    if (m_fBootHardDiskWasSet)
        m_pLabel->setText(UIWizardFirstRun::tr("<p>You have started a newly created virtual machine for the "
                                               "first time. This wizard will help you to perform the steps "
                                               "necessary for installing an operating system of your choice "
                                               "onto this virtual machine.</p><p>%1</p>")
                                              .arg(standardHelpText()));
    else
        m_pLabel->setText(UIWizardFirstRun::tr("<p>You have started a newly created virtual machine for the "
                                               "first time. This wizard will help you to perform the steps "
                                               "necessary for booting an operating system of your choice on "
                                               "the virtual machine.</p><p>Note that you will not be able to "
                                               "install an operating system into this virtual machine right "
                                               "now because you did not attach any hard disk to it. If this "
                                               "is not what you want, you can cancel the execution of this "
                                               "wizard, select <b>Settings</b> from the <b>Machine</b> menu "
                                               "of the main VirtualBox window to access the settings dialog "
                                               "of this machine and change the hard disk configuration.</p>"
                                               "<p>%1</p>")
                                              .arg(standardHelpText()));
}

void UIWizardFirstRunPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

