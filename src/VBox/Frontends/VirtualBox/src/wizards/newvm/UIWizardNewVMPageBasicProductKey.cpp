/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicProductKey class implementation.
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
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIIconPool.h"
#include "UIWizardNewVMPageBasicProductKey.h"
#include "UIWizardNewVM.h"

UIWizardNewVMPageProductKey::UIWizardNewVMPageProductKey()
    : m_pProductKeyLineEdit(0)
    , m_pHostnameLabel(0)
{
}

QString UIWizardNewVMPageProductKey::productKey() const
{
    if (!m_pProductKeyLineEdit || !m_pProductKeyLineEdit->hasAcceptableInput())
        return QString();
    return m_pProductKeyLineEdit->text();
}


UIWizardNewVMPageBasicProductKey::UIWizardNewVMPageBasicProductKey()
    : m_pLabel(0)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        QGridLayout *pGridLayout = new QGridLayout;
        {
            m_pHostnameLabel = new QLabel;
            m_pProductKeyLineEdit = new QLineEdit;
            m_pProductKeyLineEdit->setInputMask(">NNNNN-NNNNN-NNNNN-NNNNN-NNNNN;#");
            pGridLayout->addWidget(m_pHostnameLabel, 3, 0, 1, 1, Qt::AlignRight);
            pGridLayout->addWidget(m_pProductKeyLineEdit, 3, 1, 1, 3);
        }
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);
        pMainLayout->addLayout(pGridLayout);
        pMainLayout->addStretch();
    }

    registerField("productKey", this, "productKey");
}

void UIWizardNewVMPageBasicProductKey::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Product Key"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can enter the product key for the guest OS."));
    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(UIWizardNewVM::tr("Hostname:"));
}

void UIWizardNewVMPageBasicProductKey::initializePage()
{
    retranslateUi();
}

bool UIWizardNewVMPageBasicProductKey::isComplete() const
{
    return true;
}
