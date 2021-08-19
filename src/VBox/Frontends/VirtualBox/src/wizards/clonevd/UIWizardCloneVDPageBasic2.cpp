/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>

/* GUI includes: */
#include "UIWizardDiskEditors.h"
#include "UIWizardCloneVDPageBasic2.h"
#include "UIWizardCloneVD.h"
#include "QIRichTextLabel.h"

/* COM includes: */
#include "CMediumFormat.h"


// UIWizardCloneVDPage2::UIWizardCloneVDPage2()
// {
// }

// qulonglong UIWizardCloneVDPage2::mediumVariant() const
// {
//     /* Initial value: */
//     qulonglong uMediumVariant = (qulonglong)KMediumVariant_Max;

//     /* Exclusive options: */
//     if (m_pDynamicalButton->isChecked())
//         uMediumVariant = (qulonglong)KMediumVariant_Standard;
//     else if (m_pFixedButton->isChecked())
//         uMediumVariant = (qulonglong)KMediumVariant_Fixed;

//     /* Additional options: */
//     if (m_pSplitBox->isChecked())
//         uMediumVariant |= (qulonglong)KMediumVariant_VmdkSplit2G;

//     /* Return options: */
//     return uMediumVariant;
// }

// void UIWizardCloneVDPage2::setMediumVariant(qulonglong uMediumVariant)
// {
//     /* Exclusive options: */
//     if (uMediumVariant & (qulonglong)KMediumVariant_Fixed)
//     {
//         m_pFixedButton->click();
//         m_pFixedButton->setFocus();
//     }
//     else
//     {
//         m_pDynamicalButton->click();
//         m_pDynamicalButton->setFocus();
//     }

//     /* Additional options: */
//     m_pSplitBox->setChecked(uMediumVariant & (qulonglong)KMediumVariant_VmdkSplit2G);
// }

UIWizardCloneVDPageBasic2::UIWizardCloneVDPageBasic2(KDeviceType /*enmDeviceType*/)
    : m_pDescriptionLabel(0)
    , m_pDynamicLabel(0)
    , m_pFixedLabel(0)
    , m_pSplitLabel(0)
    , m_pVariantGroupBox(0)
{
    prepare();
    /* Create widgets: */
    // QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    // {
    //     QVBoxLayout *pVariantLayout = new QVBoxLayout;
    //     {
    //         m_pVariantButtonGroup = new QButtonGroup(this);
    //         {
    //             m_pDynamicalButton = new QRadioButton(this);
    //             if (enmDeviceType == KDeviceType_HardDisk)
    //             {
    //                 m_pDynamicalButton->click();
    //                 m_pDynamicalButton->setFocus();
    //             }
    //             m_pFixedButton = new QRadioButton(this);
    //             if (   enmDeviceType == KDeviceType_DVD
    //                 || enmDeviceType == KDeviceType_Floppy)
    //             {
    //                 m_pFixedButton->click();
    //                 m_pFixedButton->setFocus();
    //             }
    //             m_pVariantButtonGroup->addButton(m_pDynamicalButton, 0);
    //             m_pVariantButtonGroup->addButton(m_pFixedButton, 1);
    //         }
    //         m_pSplitBox = new QCheckBox(this);
    //         pVariantLayout->addWidget(m_pDynamicalButton);
    //         pVariantLayout->addWidget(m_pFixedButton);
    //         pVariantLayout->addWidget(m_pSplitBox);
    //     }
    //     pMainLayout->addWidget(m_pDescriptionLabel);
    //     pMainLayout->addWidget(m_pDynamicLabel);
    //     pMainLayout->addWidget(m_pFixedLabel);
    //     pMainLayout->addWidget(m_pSplitLabel);
    //     pMainLayout->addLayout(pVariantLayout);
    //     pMainLayout->addStretch();
    // }

    // /* Setup connections: */
    // connect(m_pVariantButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
    //         this, &UIWizardCloneVDPageBasic2::completeChanged);
    // connect(m_pSplitBox, &QCheckBox::stateChanged,
    //         this, &UIWizardCloneVDPageBasic2::completeChanged);
}

void UIWizardCloneVDPageBasic2::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    m_pDescriptionLabel = new QIRichTextLabel(this);
    if (m_pDescriptionLabel)
        pMainLayout->addWidget(m_pDescriptionLabel);
    m_pDynamicLabel = new QIRichTextLabel(this);
    if (m_pDynamicLabel)
        pMainLayout->addWidget(m_pDynamicLabel);
    m_pFixedLabel = new QIRichTextLabel(this);
    if (m_pFixedLabel)
        pMainLayout->addWidget(m_pFixedLabel);
    m_pSplitLabel = new QIRichTextLabel(this);
    if (m_pSplitLabel)
        pMainLayout->addWidget(m_pSplitLabel);
    m_pVariantGroupBox = new UIDiskVariantGroupBox(false /* expert mode */, 0);
    if (m_pVariantGroupBox)
        pMainLayout->addWidget(m_pVariantGroupBox);
    retranslateUi();
}


void UIWizardCloneVDPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Storage on physical hard disk"));

    /* Translate widgets: */
    m_pDescriptionLabel->setText(UIWizardCloneVD::tr("Please choose whether the new virtual disk image file should grow as it is used "
                                                     "(dynamically allocated) or if it should be created at its maximum size (fixed size)."));
    m_pDynamicLabel->setText(UIWizardCloneVD::tr("<p>A <b>dynamically allocated</b> disk image file will only use space "
                                                 "on your physical hard disk as it fills up (up to a maximum <b>fixed size</b>), "
                                                 "although it will not shrink again automatically when space on it is freed.</p>"));
    m_pFixedLabel->setText(UIWizardCloneVD::tr("<p>A <b>fixed size</b> disk image file may take longer to create on some "
                                               "systems but is often faster to use.</p>"));
    m_pSplitLabel->setText(UIWizardCloneVD::tr("<p>You can also choose to <b>split</b> the disk image file into several files "
                                               "of up to two gigabytes each. This is mainly useful if you wish to store the "
                                               "virtual machine on removable USB devices or old systems, some of which cannot "
                                               "handle very large files."));
}

void UIWizardCloneVDPageBasic2::initializePage()
{
    AssertReturnVoid(cloneWizard());
    /* Translate page: */
    retranslateUi();

    setWidgetVisibility(cloneWizard()->mediumFormat());
    if (m_pVariantGroupBox)
        cloneWizard()->setMediumVariant(m_pVariantGroupBox->mediumVariant());
    // /* Setup visibility: */
    // CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    // ULONG uCapabilities = 0;
    // QVector<KMediumFormatCapabilities> capabilities;
    // capabilities = mediumFormat.GetCapabilities();
    // for (int i = 0; i < capabilities.size(); i++)
    //     uCapabilities |= capabilities[i];

    // bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    // bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    // bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    // m_pDynamicLabel->setHidden(!fIsCreateDynamicPossible);
    // m_pDynamicalButton->setHidden(!fIsCreateDynamicPossible);
    // m_pFixedLabel->setHidden(!fIsCreateFixedPossible);
    // m_pFixedButton->setHidden(!fIsCreateFixedPossible);
    // m_pSplitLabel->setHidden(!fIsCreateSplitPossible);
    // m_pSplitBox->setHidden(!fIsCreateSplitPossible);
}

bool UIWizardCloneVDPageBasic2::isComplete() const
{
    return true;
    // /* Make sure medium variant is correct: */
    // return mediumVariant() != (qulonglong)KMediumVariant_Max;
}

UIWizardCloneVD *UIWizardCloneVDPageBasic2::cloneWizard() const
{
    return qobject_cast<UIWizardCloneVD*>(wizard());
}

void UIWizardCloneVDPageBasic2::setWidgetVisibility(const CMediumFormat &mediumFormat)
{
    AssertReturnVoid(m_pVariantGroupBox);

    m_pVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(mediumFormat, true /* hide disabled widgets*/);

    if (m_pDynamicLabel)
        m_pDynamicLabel->setHidden(!m_pVariantGroupBox->isCreateDynamicPossible());
    if (m_pFixedLabel)
        m_pFixedLabel->setHidden(!m_pVariantGroupBox->isCreateFixedPossible());
    if (m_pSplitLabel)
        m_pSplitLabel->setHidden(!m_pVariantGroupBox->isCreateSplitPossible());
}
