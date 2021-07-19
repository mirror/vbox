/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostnameDomainNameEditor class implementation.
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
#include "UIHostnameDomainNameEditor.h"
#include "UIWizardNewVM.h"



UIHostnameDomainNameEditor::UIHostnameDomainNameEditor(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pHostnameLineEdit(0)
    , m_pDomainNameLineEdit(0)
    , m_pHostnameLabel(0)
    , m_pDomainNameLabel(0)
{
    prepare();
}

QString UIHostnameDomainNameEditor::hostname() const
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->text();
    return QString();
}

bool UIHostnameDomainNameEditor::isComplete() const
{
    return m_pHostnameLineEdit && m_pHostnameLineEdit->hasAcceptableInput() &&
        m_pDomainNameLineEdit && m_pDomainNameLineEdit->hasAcceptableInput();
}

void UIHostnameDomainNameEditor::mark()
{
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->mark(!m_pHostnameLineEdit->hasAcceptableInput(),
                                  tr("Hostname should be at least 2 character lomg. Allowed characters are alphanumerics, \"-\" and \".\""));
    if (m_pDomainNameLineEdit)
        m_pDomainNameLineEdit->mark(!m_pDomainNameLineEdit->hasAcceptableInput(),
                                  tr("Domain name should be at least 2 character lomg. Allowed characters are alphanumerics, \"-\" and \".\""));
}

void UIHostnameDomainNameEditor::setHostname(const QString &strHostname)
{
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setText(strHostname);
}

QString UIHostnameDomainNameEditor::domainName() const
{
    if (m_pDomainNameLineEdit)
        return m_pDomainNameLineEdit->text();
    return QString();
}

void UIHostnameDomainNameEditor::setDomainName(const QString &strDomain)
{
    if (m_pDomainNameLineEdit)
        m_pDomainNameLineEdit->setText(strDomain);
}

QString UIHostnameDomainNameEditor::hostnameDomainName() const
{
    if (m_pHostnameLineEdit && m_pDomainNameLineEdit)
        return QString("%1.%2").arg(m_pHostnameLineEdit->text()).arg(m_pDomainNameLineEdit->text());
    return QString();
}

void UIHostnameDomainNameEditor::retranslateUi()
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
    if (m_pDomainNameLabel)
    {
        m_pDomainNameLabel->setText(tr("&Domain Name"));
        m_pDomainNameLabel->setToolTip(strDomainTooltip);
    }
    m_pDomainNameLineEdit->setToolTip(strDomainTooltip);
}

void UIHostnameDomainNameEditor::addLineEdit(int &iRow, QLabel *&pLabel, QILineEdit *&pLineEdit, QGridLayout *pLayout)
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
    pLabel->setBuddy(pLineEdit);
    ++iRow;
    return;
}

void UIHostnameDomainNameEditor::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout;
    pMainLayout->setColumnStretch(0, 0);
    pMainLayout->setColumnStretch(1, 1);
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);
    int iRow = 0;
    addLineEdit(iRow, m_pHostnameLabel, m_pHostnameLineEdit, pMainLayout);
    addLineEdit(iRow, m_pDomainNameLabel, m_pDomainNameLineEdit, pMainLayout);

    // QRegularExpression hostNameRegex("^[a-zA-Z0-9-.]{2,}\\.[a-zA-Z0-9-.]+$");^[a-zA-Z0-9-.]{2,}\.[a-zA-Z0-9-.]+$
    /* Host name and domain should be strings of minimum length of 2 and composed of alpha numerics, '-', and '.': */
    m_pHostnameLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[a-zA-Z0-9-.]{2,}"), this));
    m_pDomainNameLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[a-zA-Z0-9-.]{2,}"), this));

    connect(m_pHostnameLineEdit, &QILineEdit::textChanged,
            this, &UIHostnameDomainNameEditor::sltHostnameChanged);
    connect(m_pDomainNameLineEdit, &QILineEdit::textChanged,
            this, &UIHostnameDomainNameEditor::sltDomainChanged);


    retranslateUi();
}

void UIHostnameDomainNameEditor::sltHostnameChanged()
{
    m_pHostnameLineEdit->mark(!m_pHostnameLineEdit->hasAcceptableInput(),
                              tr("Hostname should be a string of length 2. Allowed characters are alphanumerics, '-', and '.'" ));
    emit sigHostnameDomainNameChanged(hostnameDomainName());
}

void UIHostnameDomainNameEditor::sltDomainChanged()
{
    m_pDomainNameLineEdit->mark(!m_pDomainNameLineEdit->hasAcceptableInput(),
                              tr("Domain name should be a string of length 2. Allowed characters are alphanumerics, '-', and '.'" ));

    emit sigHostnameDomainNameChanged(hostnameDomainName());
}
