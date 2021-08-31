/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageVariant class implementation.
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

/* GUI includes: */
#include "UIWizardDiskEditors.h"
#include "UIWizardCloneVDPageVariant.h"
#include "UIWizardCloneVD.h"
#include "QIRichTextLabel.h"

/* COM includes: */
#include "CMediumFormat.h"

UIWizardCloneVDPageVariant::UIWizardCloneVDPageVariant()
    : m_pDescriptionLabel(0)
    , m_pDynamicLabel(0)
    , m_pFixedLabel(0)
    , m_pSplitLabel(0)
    , m_pVariantGroupBox(0)
{
    prepare();
}

void UIWizardCloneVDPageVariant::prepare()
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
    {
        pMainLayout->addWidget(m_pVariantGroupBox);
        connect(m_pVariantGroupBox, &UIDiskVariantGroupBox::sigMediumVariantChanged,
                this, &UIWizardCloneVDPageVariant::sltMediumVariantChanged);

    }
    retranslateUi();
}


void UIWizardCloneVDPageVariant::retranslateUi()
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

void UIWizardCloneVDPageVariant::initializePage()
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVD>());
    /* Translate page: */
    retranslateUi();

    setWidgetVisibility(wizardWindow<UIWizardCloneVD>()->mediumFormat());
    if (m_pVariantGroupBox)
        wizardWindow<UIWizardCloneVD>()->setMediumVariant(m_pVariantGroupBox->mediumVariant());
}

bool UIWizardCloneVDPageVariant::isComplete() const
{
    AssertReturn(m_pVariantGroupBox, false);
    return m_pVariantGroupBox->isComplete();
}

void UIWizardCloneVDPageVariant::setWidgetVisibility(const CMediumFormat &mediumFormat)
{
    AssertReturnVoid(m_pVariantGroupBox);

    m_pVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(mediumFormat);

    if (m_pDynamicLabel)
        m_pDynamicLabel->setHidden(!m_pVariantGroupBox->isCreateDynamicPossible());
    if (m_pFixedLabel)
        m_pFixedLabel->setHidden(!m_pVariantGroupBox->isCreateFixedPossible());
    if (m_pSplitLabel)
        m_pSplitLabel->setHidden(!m_pVariantGroupBox->isCreateSplitPossible());
}

void UIWizardCloneVDPageVariant::sltMediumVariantChanged(qulonglong uVariant)
{
    if (wizardWindow<UIWizardCloneVD>())
        wizardWindow<UIWizardCloneVD>()->setMediumVariant(uVariant);
}
