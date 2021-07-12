/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNameDomainEditor class implementation.
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
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIHostNameDomainEditor.h"
#include "UIWizardNewVM.h"



UIHostNameDomainEditor::UIHostNameDomainEditor(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pHostnameLineEdit(0)
    , m_pDomainLineEdit(0)
    , m_pHostnameLabel(0)
    , m_pDomainLabel(0)
{
    prepare();
}

QString UIHostNameDomainEditor::hostname() const
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->text();
    return QString();
}

bool UIHostNameDomainEditor::isComplete() const
{
    return m_pHostnameLineEdit && m_pHostnameLineEdit->hasAcceptableInput() &&
        m_pDomainLineEdit && m_pDomainLineEdit->hasAcceptableInput();
}

void UIHostNameDomainEditor::setHostname(const QString &strHostname)
{
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setText(strHostname);
}

QString UIHostNameDomainEditor::domain() const
{
    if (m_pDomainLineEdit)
        return m_pDomainLineEdit->text();
    return QString();
}

void UIHostNameDomainEditor::setDomain(const QString &strDomain)
{
    if (m_pDomainLineEdit)
        m_pDomainLineEdit->setText(strDomain);
}

QString UIHostNameDomainEditor::hostnameDomain() const
{
    if (m_pHostnameLineEdit && m_pDomainLineEdit)
        return QString("%1.%2").arg(m_pHostnameLineEdit->text()).arg(m_pDomainLineEdit->text());
    return QString();
}

void UIHostNameDomainEditor::retranslateUi()
{
    QString strHostnameTooltip(tr("Type the hostname which will be used in attended install:"));
    QString strDomainTooltip(tr("The domain name"));
    if (m_pHostnameLabel)
    {
        m_pHostnameLabel->setText(tr("Hostna&me:"));
        m_pHostnameLabel->setToolTip(strHostnameTooltip);
    }
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setToolTip(strHostnameTooltip);
    if (m_pDomainLabel)
    {
        m_pDomainLabel->setText(tr("&Domain"));
        m_pDomainLabel->setToolTip(strDomainTooltip);
    }
    m_pDomainLineEdit->setToolTip(strDomainTooltip);
}

void UIHostNameDomainEditor::addLineEdit(int &iRow, QLabel *&pLabel, QILineEdit *&pLineEdit, QGridLayout *pLayout)
{
    AssertReturnVoid(pLayout);
    if (pLabel || pLineEdit)
        return;

    pLabel = new QLabel;
    AssertReturnVoid(pLabel);
    pLabel->setAlignment(Qt::AlignRight);
    pLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    pLayout->addWidget(pLabel, iRow, 0, 1, 1);

    pLineEdit = new QILineEdit;
    AssertReturnVoid(pLineEdit);

    pLayout->addWidget(pLineEdit, iRow, 1, 1, 3);
    // QRegularExpression hostNameRegex("^[a-zA-Z0-9-.]{2,}\\.[a-zA-Z0-9-.]+$");^[a-zA-Z0-9-.]{2,}\.[a-zA-Z0-9-.]+$
    /* Host name and domain should be strings of minimum length of 2 and composed of alpha numerics, '-', and '.': */
    m_pHostnameLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[a-zA-Z0-9-.]{2,}"), this));

    pLabel->setBuddy(pLineEdit);
    ++iRow;
    return;
}

void UIHostNameDomainEditor::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout;
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    pMainLayout->setColumnStretch(0, 0);
    pMainLayout->setColumnStretch(1, 1);
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);
    int iRow = 0;
    addLineEdit(iRow, m_pHostnameLabel, m_pHostnameLineEdit, pMainLayout);
    addLineEdit(iRow, m_pDomainLabel, m_pDomainLineEdit, pMainLayout);

    connect(m_pHostnameLineEdit, &QILineEdit::textChanged,
            this, &UIHostNameDomainEditor::sltHostnameChanged);
    connect(m_pDomainLineEdit, &QILineEdit::textChanged,
            this, &UIHostNameDomainEditor::sltDomainChanged);


    retranslateUi();
}

void UIHostNameDomainEditor::sltHostnameChanged()
{
    m_pHostnameLineEdit->mark(!m_pHostnameLineEdit->hasAcceptableInput(),
                              tr("Hostname should be a string of length 2. Allowed characters are alphanumerics, '-', and '.'" ));
    emit sigHostnameDomainChanged(hostnameDomain());
}

void UIHostNameDomainEditor::sltDomainChanged()
{
    m_pDomainLineEdit->mark(!m_pDomainLineEdit->hasAcceptableInput(),
                              tr("Domain name should be a string of length 2. Allowed characters are alphanumerics, '-', and '.'" ));

    emit sigHostnameDomainChanged(hostnameDomain());
}
