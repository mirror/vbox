/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic2 class implementation
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
#include "UIWizardFirstRunPageBasic2.h"
#include "UIWizardFirstRun.h"
#include "COMDefs.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"
#include "QIRichTextLabel.h"

UIWizardFirstRunPage2::UIWizardFirstRunPage2(bool fBootHardDiskWasSet)
    : m_fBootHardDiskWasSet(fBootHardDiskWasSet)
{
}

void UIWizardFirstRunPage2::onOpenMediumWithFileOpenDialog()
{
    /* Get opened vboxMedium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(m_pMediaSelector->type(), thisImp());
    /* Update medium-combo if necessary: */
    if (!strMediumId.isNull())
        m_pMediaSelector->setCurrentItem(strMediumId);
}

QString UIWizardFirstRunPage2::id() const
{
    return m_pMediaSelector->id();
}

void UIWizardFirstRunPage2::setId(const QString &strId)
{
    m_pMediaSelector->setCurrentItem(strId);
}

UIWizardFirstRunPageBasic2::UIWizardFirstRunPageBasic2(const QString &strMachineId, bool fBootHardDiskWasSet)
    : UIWizardFirstRunPage2(fBootHardDiskWasSet)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
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
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pSourceCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pMediaSelector, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pSelectMediaButton, SIGNAL(clicked()), this, SLOT(sltOpenMediumWithFileOpenDialog()));

    /* Register fields: */
    registerField("source", this, "source");
    registerField("id", this, "id");
}

void UIWizardFirstRunPageBasic2::sltOpenMediumWithFileOpenDialog()
{
    /* Call to base-class: */
    onOpenMediumWithFileOpenDialog();
}

void UIWizardFirstRunPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardFirstRun::tr("Select Installation Media"));

    /* Translate widgets: */
    if (m_fBootHardDiskWasSet)
        m_pLabel->setText(UIWizardFirstRun::tr("<p>Select the media which contains the setup program "
                                               "of the operating system you want to install. This media must be bootable, "
                                               "otherwise the setup program will not be able to start.</p>"));
    else
        m_pLabel->setText(UIWizardFirstRun::tr("<p>Select the media that contains the operating system "
                                               "you want to work with. This media must be bootable, "
                                               "otherwise the operating system will not be able to start.</p>"));
    m_pSourceCnt->setTitle(UIWizardFirstRun::tr("Media Source"));
    m_pSelectMediaButton->setToolTip(VBoxGlobal::tr("Choose a virtual CD/DVD disk file"));
}

void UIWizardFirstRunPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardFirstRunPageBasic2::isComplete() const
{
    /* Make sure valid medium chosen: */
    return !vboxGlobal().findMedium(id()).isNull();
}

QString UIWizardFirstRunPageBasic2::source() const
{
    return m_pMediaSelector->currentText();
}

