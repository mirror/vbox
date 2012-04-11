/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic2 class implementation
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
#include <QRadioButton>
#include <QCheckBox>
#include <QGroupBox>

/* Local includes: */
#include "UIWizardNewVDPageBasic2.h"
#include "UIWizardNewVD.h"
#include "COMDefs.h"
#include "QIRichTextLabel.h"

UIWizardNewVDPageBasic2::UIWizardNewVDPageBasic2()
    : m_pDynamicalButton(0), m_pFixedButton(0), m_pSplitBox(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pDescriptionLabel = new QIRichTextLabel(this);
        m_pDynamicLabel = new QIRichTextLabel(this);
        m_pFixedLabel = new QIRichTextLabel(this);
        m_pSplitLabel = new QIRichTextLabel(this);
        m_pVariantContainer = new QGroupBox(this);
            m_pVariantContainer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pVariantsLayout = new QVBoxLayout(m_pVariantContainer);
                m_pDynamicalButton = new QRadioButton(m_pVariantContainer);
                    m_pDynamicalButton->click();
                    m_pDynamicalButton->setFocus();
                m_pFixedButton = new QRadioButton(m_pVariantContainer);
                m_pSplitBox = new QCheckBox(m_pVariantContainer);
            pVariantsLayout->addWidget(m_pDynamicalButton);
            pVariantsLayout->addWidget(m_pFixedButton);
            pVariantsLayout->addWidget(m_pSplitBox);
    pMainLayout->addWidget(m_pDescriptionLabel);
    pMainLayout->addWidget(m_pDynamicLabel);
    pMainLayout->addWidget(m_pFixedLabel);
    pMainLayout->addWidget(m_pSplitLabel);
    pMainLayout->addWidget(m_pVariantContainer);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pDynamicalButton, SIGNAL(clicked(bool)), this, SIGNAL(completeChanged()));
    connect(m_pFixedButton, SIGNAL(clicked(bool)), this, SIGNAL(completeChanged()));
    connect(m_pSplitBox, SIGNAL(stateChanged(int)), this, SIGNAL(completeChanged()));

    /* Register 'mediumVariant' field: */
    registerField("mediumVariant", this, "mediumVariant");
}

void UIWizardNewVDPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Virtual disk storage details"));

    /* Translate widgets: */
    m_pDescriptionLabel->setText(UIWizardNewVD::tr("Please choose whether the new virtual disk file should be "
                                                   "allocated as it is used or if it should be created fully allocated."));
    m_pDynamicLabel->setText(UIWizardNewVD::tr("<p>A <b>dynamically allocated</b> virtual disk file will only use space on "
                                               "your physical hard disk as it fills up (up to a <b>fixed maximum size</b>), "
                                               "although it will not shrink again automatically when space on it is freed.</p>"));
    m_pFixedLabel->setText(UIWizardNewVD::tr("<p>A <b>fixed size</b> virtual disk file may take longer to create on some "
                                             "systems but is often faster to use.</p>"));
    m_pSplitLabel->setText(UIWizardNewVD::tr("<p>You can also choose to <b>split</b> the virtual disk into several files "
                                             "of up to two gigabytes each. This is mainly useful if you wish to store the "
                                             "virtual machine on removable USB devices or old systems, some of which cannot "
                                             "handle very large files."));
    m_pVariantContainer->setTitle(UIWizardNewVD::tr("Storage details"));

    /* Translate buttons: */
    m_pDynamicalButton->setText(UIWizardNewVD::tr("&Dynamically allocated"));
    m_pFixedButton->setText(UIWizardNewVD::tr("&Fixed size"));
    m_pSplitBox->setText(UIWizardNewVD::tr("&Split into files of less than 2GB"));
}

void UIWizardNewVDPageBasic2::initializePage()
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

bool UIWizardNewVDPageBasic2::isComplete() const
{
    return mediumVariant() != (qulonglong)KMediumVariant_Max;
}

qulonglong UIWizardNewVDPageBasic2::mediumVariant() const
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

void UIWizardNewVDPageBasic2::setMediumVariant(qulonglong uMediumVariant)
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

