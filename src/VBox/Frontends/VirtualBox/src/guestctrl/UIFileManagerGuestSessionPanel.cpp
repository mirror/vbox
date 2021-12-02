/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include <QHBoxLayout>
#include <QPushButton>

/* GUI includes: */
#include "QILineEdit.h"
#include "UIFileManagerGuestSessionPanel.h"
#include "UIUserNamePasswordEditor.h"

/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget definition.                                                                                   *
*********************************************************************************************************************************/
/** A QWidget extension containing text entry fields for password and username and buttons to
 *  start/stop a guest session. */
class UIGuestSessionCreateWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCreateSession(QString strUserName, QString strPassword);
    void sigCloseSession();

public:

    UIGuestSessionCreateWidget(QWidget *pParent = 0);
    /** Disables certain widget after a guest session has been created. */
    void switchSessionCreateMode();
    /** Makes sure certain widgets are enabled so that a guest session can be created. */
    void switchSessionCloseMode();
    void markForError(bool fMarkForError);

protected:

    void retranslateUi() /* override */;
    void keyPressEvent(QKeyEvent * pEvent) /* override */;
    void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    void sltButtonClick();
    void sltHandleTextChanged(const QString &strText);

private:

    enum ButtonMode
    {
        ButtonMode_Create,
        ButtonMode_Close
    };

    void          prepareWidgets();
    void          updateButton();

    ButtonMode    m_enmButtonMode;
    QILineEdit   *m_pUserNameEdit;
    UIPasswordLineEdit   *m_pPasswordEdit;
    QPushButton  *m_pButton;
    QHBoxLayout  *m_pMainLayout;
    QColor        m_defaultBaseColor;
    QColor        m_errorBaseColor;
    bool          m_fMarkedForError;
};


/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget implementation.                                                                                   *
*********************************************************************************************************************************/

UIGuestSessionCreateWidget::UIGuestSessionCreateWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmButtonMode(ButtonMode_Create)
    , m_pUserNameEdit(0)
    , m_pPasswordEdit(0)
    , m_pButton(0)
    , m_pMainLayout(0)
    , m_fMarkedForError(0)
{
    prepareWidgets();
}

void UIGuestSessionCreateWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;

    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pUserNameEdit = new QILineEdit;
    if (m_pUserNameEdit)
    {
        m_pMainLayout->addWidget(m_pUserNameEdit, 2);
        m_pUserNameEdit->setPlaceholderText(QApplication::translate("UIFileManager", "User Name"));
        m_defaultBaseColor = m_pUserNameEdit->palette().color(QPalette::Base);
        m_errorBaseColor = QColor(m_defaultBaseColor.red(),
                                  0.5 * m_defaultBaseColor.green(),
                                  0.5 * m_defaultBaseColor.blue());
        connect(m_pUserNameEdit, &QILineEdit::textChanged,
                this, &UIGuestSessionCreateWidget::sltHandleTextChanged);
    }

    m_pPasswordEdit = new UIPasswordLineEdit;
    if (m_pPasswordEdit)
    {
        m_pMainLayout->addWidget(m_pPasswordEdit, 2);
        m_pPasswordEdit->setPlaceholderText(QApplication::translate("UIFileManager", "Password"));
        m_pPasswordEdit->setEchoMode(QLineEdit::Password);
        connect(m_pPasswordEdit, &UIPasswordLineEdit::textChanged,
                this, &UIGuestSessionCreateWidget::sltHandleTextChanged);
    }

    m_pButton = new QPushButton;
    if (m_pButton)
    {
        m_pMainLayout->addWidget(m_pButton);
        connect(m_pButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sltButtonClick);
    }


    m_pMainLayout->insertStretch(-1, 1);
    switchSessionCreateMode();
    retranslateUi();
}

void UIGuestSessionCreateWidget::sltButtonClick()
{
    if (m_enmButtonMode == ButtonMode_Create && m_pUserNameEdit && m_pPasswordEdit)
        emit sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
    else if (m_enmButtonMode == ButtonMode_Close)
        emit sigCloseSession();
}

void UIGuestSessionCreateWidget::sltHandleTextChanged(const QString &strText)
{
    Q_UNUSED(strText);
    markForError(false);
}

void UIGuestSessionCreateWidget::retranslateUi()
{
    if (m_pUserNameEdit)
    {
        m_pUserNameEdit->setToolTip(QApplication::translate("UIFileManager", "User name to authenticate session creation"));
        m_pUserNameEdit->setPlaceholderText(QApplication::translate("UIFileManager", "User Name"));

    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(QApplication::translate("UIFileManager", "Password to authenticate session creation"));
        m_pPasswordEdit->setPlaceholderText(QApplication::translate("UIFileManager", "Password"));
    }

    if (m_pButton)
    {
        if (m_enmButtonMode == ButtonMode_Create)
            m_pButton->setText(QApplication::translate("UIFileManager", "Create Session"));
        else
            m_pButton->setText(QApplication::translate("UIFileManager", "Close Session"));
    }
}

void UIGuestSessionCreateWidget::keyPressEvent(QKeyEvent * pEvent)
{
    /* Emit sigCreateSession upon enter press: */
    if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
    {
        if ((m_pUserNameEdit && m_pUserNameEdit->hasFocus()) ||
            (m_pPasswordEdit && m_pPasswordEdit->hasFocus()))
            sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
    }
    QWidget::keyPressEvent(pEvent);
}

void UIGuestSessionCreateWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
    if (m_pUserNameEdit)
        m_pUserNameEdit->setFocus();
}

void UIGuestSessionCreateWidget::switchSessionCreateMode()
{
    if (m_pUserNameEdit)
        m_pUserNameEdit->setEnabled(true);
    if (m_pPasswordEdit)
        m_pPasswordEdit->setEnabled(true);
    m_enmButtonMode = ButtonMode_Create;
    retranslateUi();
}

void UIGuestSessionCreateWidget::switchSessionCloseMode()
{
    if (m_pUserNameEdit)
        m_pUserNameEdit->setEnabled(false);
    if (m_pPasswordEdit)
        m_pPasswordEdit->setEnabled(false);
    m_enmButtonMode = ButtonMode_Close;
    retranslateUi();
}

void UIGuestSessionCreateWidget::markForError(bool fMarkForError)
{
    if (m_fMarkedForError == fMarkForError)
        return;
    m_fMarkedForError = fMarkForError;

        if (m_pUserNameEdit)
        {
            QPalette mPalette = m_pUserNameEdit->palette();
            if (m_fMarkedForError)
                mPalette.setColor(QPalette::Base, m_errorBaseColor);
            else
                mPalette.setColor(QPalette::Base, m_defaultBaseColor);
            m_pUserNameEdit->setPalette(mPalette);
        }
        if (m_pPasswordEdit)
        {
            QPalette mPalette = m_pPasswordEdit->palette();
            if (m_fMarkedForError)
                mPalette.setColor(QPalette::Base, m_errorBaseColor);
            else
                mPalette.setColor(QPalette::Base, m_defaultBaseColor);
            m_pPasswordEdit->setPalette(mPalette);
        }
}


/*********************************************************************************************************************************
*   UIFileManagerGuestSessionPanel implementation.                                                                        *
*********************************************************************************************************************************/

UIFileManagerGuestSessionPanel::UIFileManagerGuestSessionPanel(QWidget *pParent /* = 0 */)
    : UIDialogPanel(pParent)
    , m_pSessionCreateWidget(0)
{
    prepare();
}

void UIFileManagerGuestSessionPanel::switchSessionCloseMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCloseMode();
}

void UIFileManagerGuestSessionPanel::switchSessionCreateMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCreateMode();
}

QString UIFileManagerGuestSessionPanel::panelName() const
{
    return "GuestSessionPanel";
}

void UIFileManagerGuestSessionPanel::markForError(bool fMarkForError)
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->markForError(fMarkForError);
}

void UIFileManagerGuestSessionPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pSessionCreateWidget = new UIGuestSessionCreateWidget;
    if (m_pSessionCreateWidget)
        mainLayout()->addWidget(m_pSessionCreateWidget);
    mainLayout()->setSpacing(0);
    mainLayout()->setContentsMargins(0, 0, 0, 0);
}

void UIFileManagerGuestSessionPanel::prepareConnections()
{
    if (m_pSessionCreateWidget)
    {
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCreateSession,
                this, &UIFileManagerGuestSessionPanel::sigCreateSession);
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCloseSession,
                this, &UIFileManagerGuestSessionPanel::sigCloseSession);
    }
}

void UIFileManagerGuestSessionPanel::retranslateUi()
{
    UIDialogPanel::retranslateUi();

}

void UIFileManagerGuestSessionPanel::showEvent(QShowEvent *pEvent)
{
    markForError(false);
    UIDialogPanel::showEvent(pEvent);
}

#include "UIFileManagerGuestSessionPanel.moc"
