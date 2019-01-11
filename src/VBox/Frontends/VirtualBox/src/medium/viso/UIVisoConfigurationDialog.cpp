/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoConfigurationDialog class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QCheckBox>
#include <QGridLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTextBlock>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QILabel.h"
#include "QILineEdit.h"
#include "UIVisoConfigurationDialog.h"
#include "UIVisoCreator.h"

UIVisoConfigurationDialog::UIVisoConfigurationDialog(const VisoOptions &visoOptions,
                                                     QWidget *pParent /* =0 */)
    : QIDialog(pParent)
    , m_pMainLayout(0)
    , m_pButtonBox(0)
    , m_pVisoNameLineEdit(0)
    , m_pCustomOptionsTextEdit(0)
    , m_visoOptions(visoOptions)
{
    prepareObjects();
    prepareConnections();
}

UIVisoConfigurationDialog::~UIVisoConfigurationDialog()
{
}

const VisoOptions &UIVisoConfigurationDialog::visoOptions() const
{
    return m_visoOptions;
}

void UIVisoConfigurationDialog::accept()
{
    if (m_pVisoNameLineEdit)
        m_visoOptions.m_strVisoName = m_pVisoNameLineEdit->text();

    if (m_pCustomOptionsTextEdit)
    {
        QTextDocument *pDocument = m_pCustomOptionsTextEdit->document();
        if (pDocument)
        {
            for(QTextBlock block = pDocument->begin(); block != pDocument->end()/*.isValid()*/; block = block.next())
                    m_visoOptions.m_customOptions << block.text();
        }
    }

    QIDialog::accept();
}

void UIVisoConfigurationDialog::prepareObjects()
{
    m_pMainLayout = new QGridLayout;

    if (!m_pMainLayout)
        return;

    /* Name edit and and label: */
    QILabel *pVisoNameLabel = new QILabel(UIVisoCreator::tr("VISO Name:"));
    m_pVisoNameLineEdit = new QILineEdit;
    if (pVisoNameLabel && m_pVisoNameLineEdit)
    {
        m_pVisoNameLineEdit->setText(m_visoOptions.m_strVisoName);
        pVisoNameLabel->setBuddy(m_pVisoNameLineEdit);
        m_pMainLayout->addWidget(pVisoNameLabel, 0, 0, 1, 1);
        m_pMainLayout->addWidget(m_pVisoNameLineEdit, 0, 1, 1, 1);
    }

    /* Cutom Viso options stuff: */
    QILabel *pCustomOptionsLabel = new QILabel(UIVisoCreator::tr("Custom VISO options:"));
    m_pCustomOptionsTextEdit = new QTextEdit;
    if (pCustomOptionsLabel && m_pCustomOptionsTextEdit)
    {
        m_pCustomOptionsTextEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        pCustomOptionsLabel->setBuddy(m_pCustomOptionsTextEdit);
        m_pMainLayout->addWidget(pCustomOptionsLabel, 1, 0, 1, 1, Qt::AlignTop);
        m_pMainLayout->addWidget(m_pCustomOptionsTextEdit, 1, 1, 1, 1);
        foreach (const QString &strLine, m_visoOptions.m_customOptions)
            m_pCustomOptionsTextEdit->append(strLine);
    }

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pMainLayout->addWidget(m_pButtonBox, 2, 0, 1, 2);
    }
    setLayout(m_pMainLayout);
}

void UIVisoConfigurationDialog::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoConfigurationDialog::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoConfigurationDialog::accept);
    }
}
