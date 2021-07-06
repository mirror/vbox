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
#include <QLineEdit>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"

/*********************************************************************************************************************************
*   UIPasswordLineEdit definition.                                                                                        *
*********************************************************************************************************************************/

class UIPasswordLineEdit : public QLineEdit
{
    Q_OBJECT;

signals:

    void sigTextVisibilityToggled(bool fTextVisible);

public:

    UIPasswordLineEdit(QWidget *pParent = 0);
    void toggleTextVisibility(bool fTextVisible);
    void mark(bool fError, const QString &strErrorToolTip);

protected:

    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void paintEvent(QPaintEvent *pPaintEvent) /* override */;

private slots:

    void sltHandleTextVisibilityChange();

private:

    void prepare();
    void adjustTextVisibilityButtonGeometry();

    QIToolButton *m_pTextVisibilityButton;
    QIcon m_markIcon;
    QLabel *m_pErrorIconLabel;
    QString m_strErrorToolTip;
    /** When true the line edit is marked with some icon to indicate some error. */
    bool m_fMarkForError;
};


/*********************************************************************************************************************************
*   UIPasswordLineEdit implementation.                                                                                           *
*********************************************************************************************************************************/

UIPasswordLineEdit::UIPasswordLineEdit(QWidget *pParent /*= 0 */)
    : QLineEdit(pParent)
    , m_pTextVisibilityButton(0)
    , m_pErrorIconLabel(0)
    , m_fMarkForError(false)
{
    prepare();
}

void UIPasswordLineEdit::toggleTextVisibility(bool fTextVisible)
{
    AssertPtrReturnVoid(m_pTextVisibilityButton);

    if (fTextVisible)
    {
        setEchoMode(QLineEdit::Normal);
        if (m_pTextVisibilityButton)
            m_pTextVisibilityButton->setIcon(UIIconPool::iconSet(":/eye_closed_10px.png"));
    }
    else
    {
        setEchoMode(QLineEdit::Password);
        if (m_pTextVisibilityButton)
            m_pTextVisibilityButton->setIcon(UIIconPool::iconSet(":/eye_10px.png"));
    }
}

void UIPasswordLineEdit::mark(bool fError, const QString &strErrorToolTip)
{
    if (m_fMarkForError == fError &&  m_strErrorToolTip == strErrorToolTip)
        return;
    m_fMarkForError = fError;
    m_strErrorToolTip = strErrorToolTip;
    update();
}

void UIPasswordLineEdit::prepare()
{
    m_markIcon = UIIconPool::iconSet(":/status_error_16px.png");
    /* Prepare text visibility button: */
    m_pTextVisibilityButton = new QIToolButton(this);
    if (m_pTextVisibilityButton)
    {
        m_pTextVisibilityButton->setIconSize(QSize(10, 10));
        m_pTextVisibilityButton->setFocusPolicy(Qt::ClickFocus);
        m_pTextVisibilityButton->setCursor(Qt::ArrowCursor);
        m_pTextVisibilityButton->show();
        connect(m_pTextVisibilityButton, &QToolButton::clicked, this, &UIPasswordLineEdit::sltHandleTextVisibilityChange);
    }
    m_pErrorIconLabel = new QLabel(this);
    toggleTextVisibility(false);
    adjustTextVisibilityButtonGeometry();
}

void UIPasswordLineEdit::adjustTextVisibilityButtonGeometry()
{
    AssertPtrReturnVoid(m_pTextVisibilityButton);

#ifdef VBOX_WS_MAC
    /* Do not forget to update QIToolButton size on macOS, it's FIXED: */
    m_pTextVisibilityButton->setFixedSize(m_pTextVisibilityButton->minimumSizeHint());
    /* Calculate suitable position for a QIToolButton, it's FRAMELESS: */
    const int iWidth = m_pTextVisibilityButton->width();
    const int iMinHeight = qMin(height(), m_pTextVisibilityButton->height());
    const int iMaxHeight = qMax(height(), m_pTextVisibilityButton->height());
    const int iHalfHeightDiff = (iMaxHeight - iMinHeight) / 2;
    m_pTextVisibilityButton->setGeometry(width() - iWidth - iHalfHeightDiff, iHalfHeightDiff, iWidth, iWidth);
#else
    int iFrameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    int iSize = height() - 2 * iFrameWidth;
    m_pTextVisibilityButton->setGeometry(width() - iSize, iFrameWidth, iSize, iSize);
#endif
}

void UIPasswordLineEdit::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QLineEdit::resizeEvent(pEvent);
    adjustTextVisibilityButtonGeometry();
}

void UIPasswordLineEdit::paintEvent(QPaintEvent *pPaintEvent)
{
    QLineEdit::paintEvent(pPaintEvent);
    if (m_fMarkForError)
    {
        const int iIconMargin = 0.5 * QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
        int iIconSize = height() - 2 * iIconMargin;
        if (!m_pErrorIconLabel)
            m_pErrorIconLabel = new QLabel(this);
        m_pErrorIconLabel->setPixmap(m_markIcon.pixmap(windowHandle(), QSize(iIconSize, iIconSize)));
        int iIconX = width() - iIconSize - iIconMargin;
        if (m_pTextVisibilityButton)
            iIconX -= m_pTextVisibilityButton->width() - iIconMargin;
        m_pErrorIconLabel->move(iIconX, iIconMargin);
        m_pErrorIconLabel->setToolTip(m_strErrorToolTip);
        m_pErrorIconLabel->show();
    }
    else
    {
        if (m_pErrorIconLabel)
            m_pErrorIconLabel->hide();
    }
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


/*********************************************************************************************************************************
*   UIUserNamePasswordEditor implementation.                                                                                     *
*********************************************************************************************************************************/

UIUserNamePasswordEditor::UIUserNamePasswordEditor(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameLineEdit(0)
    , m_pPasswordLineEdit(0)
    , m_pPasswordRepeatLineEdit(0)
    , m_pUserNameLabel(0)
    , m_pPasswordLabel(0)
    , m_pPasswordRepeatLabel(0)
    , m_fShowPlaceholderText(true)
    , m_fLabelsVisible(true)
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
    if (m_pUserNameLineEdit)
        m_pUserNameLineEdit->mark(!fComplete, UIUserNamePasswordEditor::tr("Invalid username"));
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
        m_pPasswordLineEdit->mark(!fPasswordOK, m_strPasswordError);
        m_pPasswordRepeatLineEdit->mark(!fPasswordOK, m_strPasswordError);
    }
    return fPasswordOK;
}

bool UIUserNamePasswordEditor::isComplete()
{
    bool fUserNameField = isUserNameComplete();
    bool fPasswordField = isPasswordComplete();
    return fUserNameField && fPasswordField;
}

void UIUserNamePasswordEditor::setPlaceholderTextEnabled(bool fEnabled)
{
    if (m_fShowPlaceholderText == fEnabled)
        return;
    m_fShowPlaceholderText = fEnabled;
    retranslateUi();
}

void UIUserNamePasswordEditor::setLabelsVisible(bool fVisible)
{
    if (m_fLabelsVisible == fVisible)
        return;
    m_fLabelsVisible = fVisible;
    m_pUserNameLabel->setVisible(fVisible);
    m_pPasswordLabel->setVisible(fVisible);
    m_pPasswordRepeatLabel->setVisible(fVisible);

}

void UIUserNamePasswordEditor::retranslateUi()
{
    QString strPassword = UIUserNamePasswordEditor::tr("Pass&word");
    QString strRepeatPassword = UIUserNamePasswordEditor::tr("&Repeat Password");
    QString strUsername = UIUserNamePasswordEditor::tr("U&sername");
    if (m_pUserNameLabel)
    {
        m_pUserNameLabel->setText(QString("%1%2").arg(strUsername).arg(":"));
        m_pUserNameLabel->setToolTip(UIUserNamePasswordEditor::tr("Type the user name which will be used in attended install:"));

    }
    if (m_pPasswordLabel)
    {
        m_pPasswordLabel->setText(QString("%1%2").arg(strPassword).arg(":"));
        m_pPasswordLabel->setToolTip(UIUserNamePasswordEditor::tr("Type the password for the user name"));

    }
    if (m_pPasswordRepeatLabel)
    {
        m_pPasswordRepeatLabel->setText(QString("%1%2").arg(strRepeatPassword).arg(":"));
        m_pPasswordRepeatLabel->setToolTip(UIUserNamePasswordEditor::tr("Retype the password:"));
    }

    if (m_fShowPlaceholderText)
    {
        if(m_pUserNameLineEdit)
            m_pUserNameLineEdit->setPlaceholderText(strUsername.remove('&'));
        if (m_pPasswordLineEdit)
            m_pPasswordLineEdit->setPlaceholderText(strPassword.remove('&'));
        if (m_pPasswordRepeatLineEdit)
            m_pPasswordRepeatLineEdit->setPlaceholderText(strRepeatPassword.remove('&'));
    }
    else
    {
        if(m_pUserNameLineEdit)
            m_pUserNameLineEdit->setPlaceholderText(QString());
        if (m_pPasswordLineEdit)
            m_pPasswordLineEdit->setPlaceholderText(QString());
        if (m_pPasswordRepeatLineEdit)
            m_pPasswordRepeatLineEdit->setPlaceholderText(QString());
    }
    m_strPasswordError = UIUserNamePasswordEditor::tr("Invalid password pair");
}

template <class T>
void UIUserNamePasswordEditor::addLineEdit(int &iRow, QLabel *&pLabel, T *&pLineEdit, QGridLayout *pLayout)
{
    if (!pLayout || pLabel || pLineEdit)
        return;
    pLabel = new QLabel;
    if (!pLabel)
        return;
    pLabel->setAlignment(Qt::AlignRight);
    pLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    pLayout->addWidget(pLabel, iRow, 0, 1, 1);

    pLineEdit = new T;
    if (!pLineEdit)
        return;
    pLayout->addWidget(pLineEdit, iRow, 1, 1, 3);

    pLabel->setBuddy(pLineEdit);
    ++iRow;
    return;
}

void UIUserNamePasswordEditor::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout;
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    pMainLayout->setColumnStretch(0, 0);
    pMainLayout->setColumnStretch(1, 1);
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);
    int iRow = 0;
    addLineEdit<QILineEdit>(iRow, m_pUserNameLabel, m_pUserNameLineEdit, pMainLayout);
    addLineEdit<UIPasswordLineEdit>(iRow, m_pPasswordLabel, m_pPasswordLineEdit, pMainLayout);
    addLineEdit<UIPasswordLineEdit>(iRow, m_pPasswordRepeatLabel, m_pPasswordRepeatLineEdit, pMainLayout);

    connect(m_pPasswordLineEdit, &UIPasswordLineEdit::sigTextVisibilityToggled,
            this, &UIUserNamePasswordEditor::sltHandlePasswordVisibility);
    connect(m_pPasswordRepeatLineEdit, &UIPasswordLineEdit::sigTextVisibilityToggled,
            this, &UIUserNamePasswordEditor::sltHandlePasswordVisibility);
    connect(m_pPasswordLineEdit, &UIPasswordLineEdit::textChanged,
            this, &UIUserNamePasswordEditor::sltPasswordChanged);
    connect(m_pUserNameLineEdit, &QILineEdit::textChanged,
            this, &UIUserNamePasswordEditor::sltUserNameChanged);

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

void UIUserNamePasswordEditor::sltUserNameChanged()
{
    if (isUserNameComplete())
        emit sigUserNameChanged(m_pUserNameLineEdit->text());
}

void UIUserNamePasswordEditor::sltPasswordChanged()
{
    if (isPasswordComplete())
        emit sigPasswordChanged(m_pPasswordLineEdit->text());
}


#include "UIUserNamePasswordEditor.moc"
