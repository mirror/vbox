/* $Id$ */
/** @file
 * VBox Qt GUI - UIUserNamePasswordEditor class implementation.
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
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"

UIPasswordLineEdit::UIPasswordLineEdit(QWidget *pParent /*= 0 */)
    :QLineEdit(pParent)
    , m_pTextVisibilityButton(0)
{
    prepare();
}

void UIPasswordLineEdit::toggleTextVisibility(bool fTextVisible)
{
    if (fTextVisible)
    {
        setEchoMode(QLineEdit::Normal);
        if(m_pTextVisibilityButton)
            m_pTextVisibilityButton->setIcon(UIIconPool::iconSet(":/eye_closed_10px.png"));
        return;
    }
    setEchoMode(QLineEdit::Password);
    if(m_pTextVisibilityButton)
        m_pTextVisibilityButton->setIcon(UIIconPool::iconSet(":/eye_10px.png"));
}

void UIPasswordLineEdit::prepare()
{
    m_pTextVisibilityButton = new QToolButton(this);
    m_pTextVisibilityButton->setIconSize(QSize(10, 10));
    m_pTextVisibilityButton->setFocusPolicy(Qt::ClickFocus);
    m_pTextVisibilityButton->setAutoRaise(true);
    m_pTextVisibilityButton->setCursor(Qt::ArrowCursor);
    m_pTextVisibilityButton->show();
    connect(m_pTextVisibilityButton, &QToolButton::clicked, this, &UIPasswordLineEdit::sltHandleTextVisibilityChange);
    toggleTextVisibility(false);
}

void UIPasswordLineEdit::paintEvent(QPaintEvent *pevent)
{
    QLineEdit::paintEvent(pevent);
    int iFrameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    int iSize = height() - 2 * iFrameWidth;
    m_pTextVisibilityButton->setGeometry(width() - iSize, iFrameWidth, iSize, iSize);

}

void UIPasswordLineEdit::sltHandleTextVisibilityChange()
{
    bool fTextVisible = false;
    if (echoMode() == QLineEdit::Normal)
        fTextVisible = false;
    else
        fTextVisible = true;
    toggleTextVisibility(fTextVisible);
    emit sigTextVisibilityToggled(fTextVisible);
}

UIUserNamePasswordEditor::UIUserNamePasswordEditor(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameLineEdit(0)
    , m_pPasswordLineEdit(0)
    , m_pPasswordRepeatLineEdit(0)
    , m_pUserNameLabel(0)
    , m_pPasswordLabel(0)
    , m_pPasswordRepeatLabel(0)
{
    prepare();
}

QString UIUserNamePasswordEditor::userName() const
{
    if (m_pUserNameLineEdit)
        return m_pUserNameLineEdit->text();
    return QString();
}

void UIUserNamePasswordEditor::setUserName(const QString &strUserName)
{
    if (m_pUserNameLineEdit)
        return m_pUserNameLineEdit->setText(strUserName);
}

QString UIUserNamePasswordEditor::password() const
{
    if (m_pPasswordLineEdit)
        return m_pPasswordLineEdit->text();
    return QString();
}

void UIUserNamePasswordEditor::setPassword(const QString &strPassword)
{
    if (m_pPasswordLineEdit)
        m_pPasswordLineEdit->setText(strPassword);
    if (m_pPasswordRepeatLineEdit)
        m_pPasswordRepeatLineEdit->setText(strPassword);
}

bool UIUserNamePasswordEditor::isUserNameComplete()
{
    bool fComplete = (m_pUserNameLineEdit && !m_pUserNameLineEdit->text().isEmpty());
    markLineEdit(m_pUserNameLineEdit, !fComplete);
    return fComplete;
}

bool UIUserNamePasswordEditor::isPasswordComplete()
{
    bool fPasswordOK = true;
    if (m_pPasswordLineEdit && m_pPasswordRepeatLineEdit)
    {
        if (m_pPasswordLineEdit->text() != m_pPasswordRepeatLineEdit->text())
            fPasswordOK = false;
        if (m_pPasswordLineEdit->text().isEmpty())
            fPasswordOK = false;
    }
    markLineEdit(m_pPasswordLineEdit, !fPasswordOK);
    markLineEdit(m_pPasswordRepeatLineEdit, !fPasswordOK);
    return fPasswordOK;
}

bool UIUserNamePasswordEditor::isComplete()
{
    bool fUserNameField = isUserNameComplete();
    bool fPasswordField = isPasswordComplete();
    return fUserNameField && fPasswordField;
}

void UIUserNamePasswordEditor::retranslateUi()
{
    if (m_pUserNameLabel)
    {
        m_pUserNameLabel->setText(UIWizardNewVM::tr("User Name:"));
        m_pUserNameLabel->setToolTip(UIWizardNewVM::tr("Type the user name which will be used in attended install:"));

    }
    if (m_pPasswordLabel)
    {
        m_pPasswordLabel->setText(UIWizardNewVM::tr("Password:"));
        m_pPasswordLabel->setToolTip(UIWizardNewVM::tr("Type the password for the user name"));

    }
    if (m_pPasswordRepeatLabel)
    {
        m_pPasswordRepeatLabel->setText(UIWizardNewVM::tr("Repeat Password:"));
        m_pPasswordRepeatLabel->setToolTip(UIWizardNewVM::tr("Retype the password:"));
    }
}

template <class T>
void UIUserNamePasswordEditor::addLineEdit(int &iRow, QLabel *&pLabel, T *&pLineEdit, QGridLayout *pLayout)
{
    if (!pLayout || pLabel || pLineEdit)
        return;
    pLabel = new QLabel;
    if (!pLabel)
        return;
    pLayout->addWidget(pLabel, iRow, 0, 1, 1, Qt::AlignRight);

    pLineEdit = new T;
    if (!pLineEdit)
        return;
    pLayout->addWidget(pLineEdit, iRow, 1, 1, 3);

    pLabel->setBuddy(pLineEdit);
    ++iRow;
    connect(pLineEdit, &T::textChanged, this, &UIUserNamePasswordEditor::sltSomeTextChanged);
    return;
}

void UIUserNamePasswordEditor::markLineEdit(QLineEdit *pLineEdit, bool fError)
{
    if (!pLineEdit)
        return;
    QPalette palette = pLineEdit->palette();
    if (fError)
        palette.setColor(QPalette::Base, QColor(255, 180, 180));
    else
        palette.setColor(QPalette::Base, m_orginalLineEditBaseColor);
    pLineEdit->setPalette(palette);
}

void UIUserNamePasswordEditor::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout;
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);
    int iRow = 0;
    addLineEdit<QLineEdit>(iRow, m_pUserNameLabel, m_pUserNameLineEdit, pMainLayout);
    addLineEdit<UIPasswordLineEdit>(iRow, m_pPasswordLabel, m_pPasswordLineEdit, pMainLayout);
    addLineEdit<UIPasswordLineEdit>(iRow, m_pPasswordRepeatLabel, m_pPasswordRepeatLineEdit, pMainLayout);

    connect(m_pPasswordLineEdit, &UIPasswordLineEdit::sigTextVisibilityToggled,
            this, &UIUserNamePasswordEditor::sltHandlePasswordVisibility);
    connect(m_pPasswordRepeatLineEdit, &UIPasswordLineEdit::sigTextVisibilityToggled,
            this, &UIUserNamePasswordEditor::sltHandlePasswordVisibility);
    /* Cache the original back color of the line edit to restore it correctly: */
    if (m_pUserNameLineEdit)
        m_orginalLineEditBaseColor = m_pUserNameLineEdit->palette().color(QPalette::Base);

    retranslateUi();
}

void UIUserNamePasswordEditor::sltHandlePasswordVisibility(bool fPasswordVisible)
{
    if (m_pPasswordLineEdit)
        m_pPasswordLineEdit->toggleTextVisibility(fPasswordVisible);
    if (m_pPasswordRepeatLineEdit)
        m_pPasswordRepeatLineEdit->toggleTextVisibility(fPasswordVisible);
}

void UIUserNamePasswordEditor::sltSomeTextChanged()
{
    /* Check text lines edits and mark them accordingly: */
    isComplete();
    emit sigSomeTextChanged();
}
