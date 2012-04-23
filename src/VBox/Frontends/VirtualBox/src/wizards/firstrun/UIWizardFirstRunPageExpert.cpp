/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageExpert class implementation
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
#include <QHBoxLayout>
#include <QGroupBox>

/* Local includes: */
#include "UIWizardFirstRunPageExpert.h"
#include "UIWizardFirstRun.h"
#include "VBoxGlobal.h"
#include "UIIconPool.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"

UIWizardFirstRunPageExpert::UIWizardFirstRunPageExpert(const QString &strMachineId, bool fBootHardDiskWasSet)
    : UIWizardFirstRunPage1(fBootHardDiskWasSet)
    , UIWizardFirstRunPage2(fBootHardDiskWasSet)
    , UIWizardFirstRunPage3(fBootHardDiskWasSet)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        pMainLayout->setContentsMargins(8, 6, 8, 6);
        m_pSourceCnt = new QGroupBox(this);
        {
            m_pSourceCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pSourceCntLayout = new QHBoxLayout(m_pSourceCnt);
            {
                m_pMediaSelector = new VBoxMediaComboBox(m_pSourceCnt);
                {
                    m_pMediaSelector->setMachineId(strMachineId);
                    m_pMediaSelector->setType(VBoxDefs::MediumType_DVD);
                    m_pMediaSelector->repopulate();
                }
                m_pSelectMediaButton = new QIToolButton(m_pSourceCnt);
                {
                    m_pSelectMediaButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
                    m_pSelectMediaButton->setAutoRaise(true);
                }
                pSourceCntLayout->addWidget(m_pMediaSelector);
                pSourceCntLayout->addWidget(m_pSelectMediaButton);
            }
        }
        pMainLayout->addWidget(m_pSourceCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pMediaSelector, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pSelectMediaButton, SIGNAL(clicked()), this, SLOT(sltOpenMediumWithFileOpenDialog()));

    /* Register fields: */
    registerField("id", this, "id");
}

void UIWizardFirstRunPageExpert::sltOpenMediumWithFileOpenDialog()
{
    /* Call to base-class: */
    onOpenMediumWithFileOpenDialog();
}

void UIWizardFirstRunPageExpert::retranslateUi()
{
    /* Translate widgets: */
    m_pSourceCnt->setTitle(UIWizardFirstRun::tr("Media Source"));
    m_pSelectMediaButton->setToolTip(VBoxGlobal::tr("Choose a virtual CD/DVD disk file"));
}

void UIWizardFirstRunPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardFirstRunPageExpert::isComplete() const
{
    /* Make sure valid medium chosen: */
    return !vboxGlobal().findMedium(id()).isNull();
}

bool UIWizardFirstRunPageExpert::validatePage()
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

