/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
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
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIFileManager.h"
#include "UIFileManagerSessionPanel.h"


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

protected:

    void retranslateUi() /* override */;
    void keyPressEvent(QKeyEvent * pEvent) /* override */;
    void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    void sltCreateButtonClick();
    void sltShowHidePassword(bool flag);

private:

    void          prepareWidgets();
    QILineEdit   *m_pUserNameEdit;
    QILineEdit   *m_pPasswordEdit;
    QPushButton  *m_pCreateButton;
    QPushButton  *m_pCloseButton;
    QHBoxLayout  *m_pMainLayout;
    QCheckBox    *m_pShowPasswordCheckBox;
};


/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget implementation.                                                                                   *
*********************************************************************************************************************************/

UIGuestSessionCreateWidget::UIGuestSessionCreateWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameEdit(0)
    , m_pPasswordEdit(0)
    , m_pCreateButton(0)
    , m_pCloseButton(0)
    , m_pMainLayout(0)
    , m_pShowPasswordCheckBox(0)
{
    prepareWidgets();
}

void UIGuestSessionCreateWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;

    m_pUserNameEdit = new QILineEdit;
    if (m_pUserNameEdit)
    {
        m_pMainLayout->addWidget(m_pUserNameEdit, 2);
        m_pUserNameEdit->setPlaceholderText("User Name");
    }

    m_pPasswordEdit = new QILineEdit;
    if (m_pPasswordEdit)
    {
        m_pMainLayout->addWidget(m_pPasswordEdit, 2);
        m_pPasswordEdit->setPlaceholderText(UIFileManager::tr("Password"));
        m_pPasswordEdit->setEchoMode(QLineEdit::Password);
    }

    m_pShowPasswordCheckBox = new QCheckBox;
    if (m_pShowPasswordCheckBox)
    {
        m_pShowPasswordCheckBox->setText(UIFileManager::tr("Show Password"));
        m_pMainLayout->addWidget(m_pShowPasswordCheckBox);
        connect(m_pShowPasswordCheckBox, &QCheckBox::toggled,
                this, &UIGuestSessionCreateWidget::sltShowHidePassword);
    }

    m_pCreateButton = new QPushButton;
    if (m_pCreateButton)
    {
        m_pMainLayout->addWidget(m_pCreateButton);
        connect(m_pCreateButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sltCreateButtonClick);
    }

    m_pCloseButton = new QPushButton;
    if (m_pCloseButton)
    {
        m_pMainLayout->addWidget(m_pCloseButton);
        connect(m_pCloseButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sigCloseSession);
    }
    m_pMainLayout->insertStretch(-1, 1);
    retranslateUi();
}

void UIGuestSessionCreateWidget::sltCreateButtonClick()
{
    if (m_pUserNameEdit && m_pPasswordEdit)
        emit sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
}

void UIGuestSessionCreateWidget::sltShowHidePassword(bool flag)
{
    if (!m_pPasswordEdit)
        return;
    if (flag)
        m_pPasswordEdit->setEchoMode(QLineEdit::Normal);
    else
        m_pPasswordEdit->setEchoMode(QLineEdit::Password);
}

void UIGuestSessionCreateWidget::retranslateUi()
{
    if (m_pUserNameEdit)
    {
        m_pUserNameEdit->setToolTip(UIFileManager::tr("User name to authenticate session creation"));
        m_pUserNameEdit->setPlaceholderText(UIFileManager::tr("User Name"));

    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(UIFileManager::tr("Password to authenticate session creation"));
        m_pPasswordEdit->setPlaceholderText(UIFileManager::tr("Password"));
    }

    if (m_pCreateButton)
        m_pCreateButton->setText(UIFileManager::tr("Create Session"));
    if (m_pCloseButton)
        m_pCloseButton->setText(UIFileManager::tr("Close Session"));
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
    if (m_pCreateButton)
        m_pCreateButton->setEnabled(true);
    if (m_pCloseButton)
        m_pCloseButton->setEnabled(false);
}

void UIGuestSessionCreateWidget::switchSessionCloseMode()
{
    if (m_pUserNameEdit)
        m_pUserNameEdit->setEnabled(false);
    if (m_pPasswordEdit)
        m_pPasswordEdit->setEnabled(false);
    if (m_pCreateButton)
        m_pCreateButton->setEnabled(false);
    if (m_pCloseButton)
        m_pCloseButton->setEnabled(true);
}


/*********************************************************************************************************************************
*   UIFileManagerSessionPanel implementation.                                                                        *
*********************************************************************************************************************************/

UIFileManagerSessionPanel::UIFileManagerSessionPanel(QWidget *pParent /* = 0 */)
    : UIDialogPanel(pParent)
    , m_pSessionCreateWidget(0)
{
    prepare();
}

void UIFileManagerSessionPanel::switchSessionCloseMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCloseMode();
}

void UIFileManagerSessionPanel::switchSessionCreateMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCreateMode();
}

QString UIFileManagerSessionPanel::panelName() const
{
    return "SessionPanel";
}

void UIFileManagerSessionPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pSessionCreateWidget = new UIGuestSessionCreateWidget;
    if (m_pSessionCreateWidget)
        mainLayout()->addWidget(m_pSessionCreateWidget);
}

void UIFileManagerSessionPanel::prepareConnections()
{
    if (m_pSessionCreateWidget)
    {
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCreateSession,
                this, &UIFileManagerSessionPanel::sigCreateSession);
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCloseSession,
                this, &UIFileManagerSessionPanel::sigCloseSession);
    }
}

void UIFileManagerSessionPanel::retranslateUi()
{
    UIDialogPanel::retranslateUi();

}

#include "UIFileManagerSessionPanel.moc"
