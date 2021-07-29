/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageFileType class implementation.
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
#include <QRadioButton>

/* GUI includes: */
#include "UIConverter.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVDPageFileType.h"
#include "UIWizardNewVD.h"
#include "QIRichTextLabel.h"

UIWizardNewVDPageFileType::UIWizardNewVDPageFileType()
    : m_pLabel(0)
    , m_pFormatButtonGroup(0)
{
    prepare();
}

void UIWizardNewVDPageFileType::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    m_pFormatButtonGroup = new UIDiskFormatsGroupBox(false, 0);
    pMainLayout->addWidget(m_pFormatButtonGroup, false);

    pMainLayout->addStretch();
    connect(m_pFormatButtonGroup, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
            this, &UIWizardNewVDPageFileType::sltMediumFormatChanged);
    retranslateUi();
}

void UIWizardNewVDPageFileType::sltMediumFormatChanged()
{
    AssertReturnVoid(m_pFormatButtonGroup);
    newVDWizardPropertySet(MediumFormat, m_pFormatButtonGroup->mediumFormat());
}

void UIWizardNewVDPageFileType::retranslateUi()
{
    setTitle(UIWizardNewVD::tr("Virtual Hard disk file type"));

    m_pLabel->setText(UIWizardNewVD::tr("Please choose the type of file that you would like to use "
                                        "for the new virtual hard disk. If you do not need to use it "
                                        "with other virtualization software you can leave this setting unchanged."));
}

void UIWizardNewVDPageFileType::initializePage()
{
    retranslateUi();
    if (m_pFormatButtonGroup)
        newVDWizardPropertySet(MediumFormat, m_pFormatButtonGroup->mediumFormat());
}

bool UIWizardNewVDPageFileType::isComplete() const
{
    /* Make sure medium format is correct: */
    //return !mediumFormat().isNull();
    return true;
}
