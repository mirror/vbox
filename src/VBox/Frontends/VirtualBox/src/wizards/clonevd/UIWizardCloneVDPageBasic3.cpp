/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic3 class implementation
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
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>

/* Local includes: */
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardCloneVD.h"
#include "COMDefs.h"
#include "QIRichTextLabel.h"

UIWizardCloneVDPage3::UIWizardCloneVDPage3()
{
}

qulonglong UIWizardCloneVDPage3::mediumVariant() const
{
    /* Initial value: */
    qulonglong uMediumVariant = (qulonglong)KMediumVariant_Max;

    /* Exclusive options: */
    if (m_pDynamicalButton->isChecked())
        uMediumVariant = (qulonglong)KMediumVariant_Standard;
    else if (m_pFixedButton->isChecked())
        uMediumVariant = (qulonglong)KMediumVariant_Fixed;

    /* Additional options: */
    if (m_pSplitBox->isChecked())
        uMediumVariant |= (qulonglong)KMediumVariant_VmdkSplit2G;

    /* Return options: */
    return uMediumVariant;
}

void UIWizardCloneVDPage3::setMediumVariant(qulonglong uMediumVariant)
{
    /* Exclusive options: */
    if (uMediumVariant & (qulonglong)KMediumVariant_Fixed)
    {
        m_pFixedButton->click();
        m_pFixedButton->setFocus();
    }
    else
    {
        m_pDynamicalButton->click();
        m_pDynamicalButton->setFocus();
    }

    /* Additional options: */
    m_pSplitBox->setChecked(uMediumVariant & (qulonglong)KMediumVariant_VmdkSplit2G);
}

UIWizardCloneVDPageBasic3::UIWizardCloneVDPageBasic3()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pDescriptionLabel = new QIRichTextLabel(this);
        m_pDynamicLabel = new QIRichTextLabel(this);
        m_pFixedLabel = new QIRichTextLabel(this);
        m_pSplitLabel = new QIRichTextLabel(this);
        m_pVariantCnt = new QGroupBox(this);
        {
            m_pVariantCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pVariantCntLayout = new QVBoxLayout(m_pVariantCnt);
            {
                m_pVariantButtonGroup = new QButtonGroup(m_pVariantCnt);
                {
                    m_pDynamicalButton = new QRadioButton(m_pVariantCnt);
                    {
                        m_pDynamicalButton->click();
                        m_pDynamicalButton->setFocus();
                    }
                    m_pFixedButton = new QRadioButton(m_pVariantCnt);
                    m_pVariantButtonGroup->addButton(m_pDynamicalButton, 0);
                    m_pVariantButtonGroup->addButton(m_pFixedButton, 1);
                }
                m_pSplitBox = new QCheckBox(m_pVariantCnt);
                pVariantCntLayout->addWidget(m_pDynamicalButton);
                pVariantCntLayout->addWidget(m_pFixedButton);
                pVariantCntLayout->addWidget(m_pSplitBox);
            }
        }
        pMainLayout->addWidget(m_pDescriptionLabel);
        pMainLayout->addWidget(m_pDynamicLabel);
        pMainLayout->addWidget(m_pFixedLabel);
        pMainLayout->addWidget(m_pSplitLabel);
        pMainLayout->addWidget(m_pVariantCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pVariantButtonGroup, SIGNAL(buttonClicked(QAbstractButton *)), this, SIGNAL(completeChanged()));
    connect(m_pSplitBox, SIGNAL(stateChanged(int)), this, SIGNAL(completeChanged()));

    /* Register fields: */
    registerField("mediumVariant", this, "mediumVariant");
}

void UIWizardCloneVDPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Virtual disk storage details"));

    /* Translate widgets: */
    m_pDescriptionLabel->setText(UIWizardCloneVD::tr("Please choose whether the new virtual disk file should be "
                                                     "allocated as it is used or if it should be created fully allocated."));
    m_pDynamicLabel->setText(UIWizardCloneVD::tr("<p>A <b>dynamically allocated</b> virtual disk file will only use space on "
                                                 "your physical hard disk as it fills up (up to a <b>fixed maximum size</b>), "
                                                 "although it will not shrink again automatically when space on it is freed.</p>"));
    m_pFixedLabel->setText(UIWizardCloneVD::tr("<p>A <b>fixed size</b> virtual disk file may take longer to create on some "
                                               "systems but is often faster to use.</p>"));
    m_pSplitLabel->setText(UIWizardCloneVD::tr("<p>You can also choose to <b>split</b> the virtual disk into several files "
                                               "of up to two gigabytes each. This is mainly useful if you wish to store the "
                                               "virtual machine on removable USB devices or old systems, some of which cannot "
                                               "handle very large files."));

    m_pVariantCnt->setTitle(UIWizardCloneVD::tr("Storage details"));
    m_pDynamicalButton->setText(UIWizardCloneVD::tr("&Dynamically allocated"));
    m_pFixedButton->setText(UIWizardCloneVD::tr("&Fixed size"));
    m_pSplitBox->setText(UIWizardCloneVD::tr("&Split into files of less than 2GB"));
}

void UIWizardCloneVDPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Setup visibility: */
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    ULONG uCapabilities = mediumFormat.GetCapabilities();
    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    m_pDynamicLabel->setHidden(!fIsCreateDynamicPossible);
    m_pDynamicalButton->setHidden(!fIsCreateDynamicPossible);
    m_pFixedLabel->setHidden(!fIsCreateFixedPossible);
    m_pFixedButton->setHidden(!fIsCreateFixedPossible);
    m_pSplitLabel->setHidden(!fIsCreateSplitPossible);
    m_pSplitBox->setHidden(!fIsCreateSplitPossible);
}

bool UIWizardCloneVDPageBasic3::isComplete() const
{
    /* Make sure medium variant is correct: */
    return mediumVariant() != (qulonglong)KMediumVariant_Max;
}

