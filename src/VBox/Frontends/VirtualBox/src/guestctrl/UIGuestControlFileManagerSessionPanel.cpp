/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
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
# include <QCheckBox>
# include <QHBoxLayout>
# include <QMenu>
# include <QPushButton>
# include <QSpinBox>
# include <QTextEdit>

/* GUI includes: */
# include "QILineEdit.h"
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerSessionPanel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget definition.                                                                                   *
*********************************************************************************************************************************/

class UIGuestSessionCreateWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCreateSession(QString strUserName, QString strPassword);
    void sigCloseSession();

public:

    UIGuestSessionCreateWidget(QWidget *pParent = 0);
    void switchSessionCreateMode();
    void switchSessionCloseMode();

protected:

    void retranslateUi();
    void keyPressEvent(QKeyEvent * pEvent);

private slots:

    void sltCreateButtonClick();
    void sltShowHidePassword(bool flag);

private:
    void         prepareWidgets();
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
        m_pPasswordEdit->setPlaceholderText("Password");
        m_pPasswordEdit->setEchoMode(QLineEdit::Password);
    }

    m_pShowPasswordCheckBox = new QCheckBox;
    if (m_pShowPasswordCheckBox)
    {
        m_pShowPasswordCheckBox->setText("Show Password");
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
        m_pUserNameEdit->setToolTip(QApplication::translate("UIGuestControlFileManager", "User name to authenticate session creation"));
        m_pUserNameEdit->setPlaceholderText(QApplication::translate("UIGuestControlFileManager", "User Name"));

    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(QApplication::translate("UIGuestControlFileManager", "Password to authenticate session creation"));
        m_pPasswordEdit->setPlaceholderText(QApplication::translate("UIGuestControlFileManager", "Password"));
    }

    if (m_pCreateButton)
        m_pCreateButton->setText(QApplication::translate("UIGuestControlFileManager", "Create Session"));
    if (m_pCloseButton)
        m_pCloseButton->setText(QApplication::translate("UIGuestControlFileManager", "Close Session"));
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
*   UIGuestControlFileManagerSessionPanel implementation.                                                                        *
*********************************************************************************************************************************/

UIGuestControlFileManagerSessionPanel::UIGuestControlFileManagerSessionPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pSessionCreateWidget(0)
{
    prepare();
}

void UIGuestControlFileManagerSessionPanel::switchSessionCloseMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCloseMode();
}

void UIGuestControlFileManagerSessionPanel::switchSessionCreateMode()
{
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCreateMode();
}

QString UIGuestControlFileManagerSessionPanel::panelName() const
{
    return "SessionPanel";
}

void UIGuestControlFileManagerSessionPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pSessionCreateWidget = new UIGuestSessionCreateWidget;
    if (m_pSessionCreateWidget)
        mainLayout()->addWidget(m_pSessionCreateWidget);
}

void UIGuestControlFileManagerSessionPanel::prepareConnections()
{
    if (m_pSessionCreateWidget)
    {
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCreateSession,
                this, &UIGuestControlFileManagerSessionPanel::sigCreateSession);
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCloseSession,
                this, &UIGuestControlFileManagerSessionPanel::sigCloseSession);
    }
}

void UIGuestControlFileManagerSessionPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();

}

#include "UIGuestControlFileManagerSessionPanel.moc"
