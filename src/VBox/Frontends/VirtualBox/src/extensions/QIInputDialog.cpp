/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIInputDialog class implementation.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIInputDialog.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QIInputDialog::QIInputDialog(QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = 0 */)
    : QDialog(pParent, enmFlags)
    , m_fDefaultLabelTextRedefined(false)
    , m_pLabel(0)
    , m_pTextValueEditor(0)
    , m_pButtonBox(0)
{
    /* Prepare: */
    prepare();
}

QString QIInputDialog::labelText() const
{
    return m_pLabel ? m_pLabel->text() : QString();
}

void QIInputDialog::resetLabelText()
{
    m_fDefaultLabelTextRedefined = false;
    retranslateUi();
}

void QIInputDialog::setLabelText(const QString &strText)
{
    m_fDefaultLabelTextRedefined = true;
    if (m_pLabel)
        m_pLabel->setText(strText);
}

QString QIInputDialog::textValue() const
{
    return m_pTextValueEditor ? m_pTextValueEditor->text() : QString();
}

void QIInputDialog::setTextValue(const QString &strText)
{
    if (m_pTextValueEditor)
        m_pTextValueEditor->setText(strText);
}

void QIInputDialog::retranslateUi()
{
    if (m_pLabel && !m_fDefaultLabelTextRedefined)
        m_pLabel->setText(tr("Name:"));
}

void QIInputDialog::sltTextChanged()
{
    if (m_pButtonBox)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(!textValue().isEmpty());
}

void QIInputDialog::prepare()
{
    /* Do not count that window as important for application,
     * it will NOT be taken into account when other
     * top-level windows will be closed: */
    setAttribute(Qt::WA_QuitOnClose, false);

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);

        /* Create text value editor: */
        m_pTextValueEditor = new QLineEdit(this);
        if (m_pTextValueEditor)
        {
            connect(m_pTextValueEditor, &QLineEdit::textChanged, this, &QIInputDialog::sltTextChanged);
            pMainLayout->addWidget(m_pTextValueEditor);
        }

        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
        if (m_pButtonBox)
        {
            connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &QIInputDialog::accept);
            connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &QIInputDialog::reject);
            pMainLayout->addWidget(m_pButtonBox);
        }
    }

    /* Apply language settings: */
    retranslateUi();

    /* Initialize editors: */
    sltTextChanged();
}
