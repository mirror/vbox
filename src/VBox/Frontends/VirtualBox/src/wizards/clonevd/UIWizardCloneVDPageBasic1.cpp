/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic1 class implementation
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
#include <QHBoxLayout>
#include <QGroupBox>

/* Local includes: */
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVD.h"
#include "UIIconPool.h"
#include "QIRichTextLabel.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"

UIWizardCloneVDPageBasic1::UIWizardCloneVDPageBasic1(const CMedium &sourceVirtualDisk)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pSourceDiskContainer = new QGroupBox(this);
            m_pSourceDiskContainer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pSourceDiskLayout = new QHBoxLayout(m_pSourceDiskContainer);
                m_pSourceDiskSelector = new VBoxMediaComboBox(m_pSourceDiskContainer);
                    m_pSourceDiskSelector->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                    m_pSourceDiskSelector->setType(VBoxDefs::MediumType_HardDisk);
                    m_pSourceDiskSelector->setCurrentItem(sourceVirtualDisk.GetId());
                    m_pSourceDiskSelector->repopulate();
                m_pOpenSourceDiskButton = new QIToolButton(m_pSourceDiskContainer);
                    m_pOpenSourceDiskButton->setAutoRaise(true);
                    m_pOpenSourceDiskButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
            pSourceDiskLayout->addWidget(m_pSourceDiskSelector);
            pSourceDiskLayout->addWidget(m_pOpenSourceDiskButton);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pSourceDiskContainer);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pSourceDiskSelector, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pOpenSourceDiskButton, SIGNAL(clicked()), this, SLOT(sltHandleOpenSourceDiskClick()));

    /* Register CMedium class: */
    qRegisterMetaType<CMedium>();
    /* Register 'sourceVirtualDisk' field: */
    registerField("sourceVirtualDisk", this, "sourceVirtualDisk");
}

void UIWizardCloneVDPageBasic1::sltHandleOpenSourceDiskClick()
{
    /* Get source virtual-disk using file-open dialog: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(VBoxDefs::MediumType_HardDisk, this);
    if (!strMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pSourceDiskSelector->setCurrentItem(strMediumId);
        /* Focus on virtual-disk combo: */
        m_pSourceDiskSelector->setFocus();
    }
}

void UIWizardCloneVDPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Welcome to the virtual disk copying wizard"));

    /* Translate widgets: */
    m_pSourceDiskContainer->setTitle(UIWizardCloneVD::tr("Virtual disk to copy"));
    m_pOpenSourceDiskButton->setToolTip(UIWizardCloneVD::tr("Choose a virtual hard disk file..."));
    m_pLabel->setText(UIWizardCloneVD::tr("<p>This wizard will help you to copy a virtual disk.</p>"));
    m_pLabel->setText(m_pLabel->text() + QString("<p>%1</p>").arg(standardHelpText()));
    m_pLabel->setText(m_pLabel->text() + UIWizardCloneVD::tr("Please select the virtual disk which you would like "
                                                             "to copy if it is not already selected. You can either "
                                                             "choose one from the list or use the folder icon "
                                                             "beside the list to select a virtual disk file."));
}

void UIWizardCloneVDPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVDPageBasic1::isComplete() const
{
    /* Check what source virtual-disk feats the rules: */
    return !sourceVirtualDisk().isNull();
}

CMedium UIWizardCloneVDPageBasic1::sourceVirtualDisk() const
{
    return vboxGlobal().findMedium(m_pSourceDiskSelector->id()).medium();
}

void UIWizardCloneVDPageBasic1::setSourceVirtualDisk(const CMedium &sourceVirtualDisk)
{
    m_pSourceDiskSelector->setCurrentItem(sourceVirtualDisk.GetId());
}

