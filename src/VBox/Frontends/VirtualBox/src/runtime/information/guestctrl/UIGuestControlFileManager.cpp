/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileManager class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QSplitter>
# include <QHBoxLayout>
# include <QVBoxLayout>

/* GUI includes: */
# include "QILabel.h"
# include "QILineEdit.h"
# include "QIToolButton.h"
# include "QIWithRetranslateUI.h"
# include "UIGuestControlFileManager.h"
# include "UIVMInformationDialog.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

class UIGuestSessionCreateWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIGuestSessionCreateWidget(QWidget *pParent = 0);

protected:

    void retranslateUi();

private:
    void         prepareWidgets();
    QILineEdit   *m_pUserNameEdit;
    QILineEdit   *m_pPasswordEdit;

    QILabel      *m_pUserNameLabel;
    QILabel      *m_pPasswordLabel;
    QIToolButton *m_pCreateButton;

    QHBoxLayout *m_pMainLayout;

};

UIGuestSessionCreateWidget::UIGuestSessionCreateWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameEdit(0)
    , m_pPasswordEdit(0)
    , m_pUserNameLabel(0)
    , m_pPasswordLabel(0)
    , m_pCreateButton(0)
    , m_pMainLayout(0)
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
        m_pMainLayout->addWidget(m_pUserNameEdit);
    }
    m_pUserNameLabel = new QILabel;
    if (m_pUserNameLabel)
    {
        m_pMainLayout->addWidget(m_pUserNameLabel);
    }
    m_pPasswordEdit = new QILineEdit;
    if (m_pPasswordEdit)
    {
        m_pMainLayout->addWidget(m_pPasswordEdit);
    }

    m_pPasswordLabel = new QILabel;
    if (m_pPasswordLabel)
    {
        m_pMainLayout->addWidget(m_pPasswordLabel);
    }

    m_pCreateButton = new QIToolButton;
    if (m_pCreateButton)
    {
        m_pMainLayout->addWidget(m_pCreateButton);
    }
    retranslateUi();
}

void UIGuestSessionCreateWidget::retranslateUi()
{
    if (m_pUserNameEdit)
    {
        m_pUserNameEdit->setToolTip(UIVMInformationDialog::tr("User name to authenticate session creation"));
    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(UIVMInformationDialog::tr("Password to authenticate session creation"));
    }
    if (m_pUserNameLabel)
    {
        m_pUserNameLabel->setToolTip(UIVMInformationDialog::tr("User name to authenticate session creation"));
        m_pUserNameLabel->setText(UIVMInformationDialog::tr("User name"));
    }
    if (m_pPasswordLabel)
    {
        m_pPasswordLabel->setToolTip(UIVMInformationDialog::tr("Password to authenticate session creation"));
        m_pPasswordLabel->setText(UIVMInformationDialog::tr("Password"));
    }
    if(m_pCreateButton)
    {
        m_pCreateButton->setText(UIVMInformationDialog::tr("Create Session"));
    }
}

UIGuestControlFileManager::UIGuestControlFileManager(QWidget *pParent, const CGuest &comGuest)
    : QWidget(pParent)
    , m_comGuest(comGuest)
    , m_pMainLayout(0)
    , m_pSplitter(0)
    , m_pSessionCreateWidget(0)
{
    prepareObjects();
    prepareConnections();
}

void UIGuestControlFileManager::prepareObjects()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);

    m_pSplitter = new QSplitter;

    if (!m_pSplitter)
        return;

    m_pSessionCreateWidget = new UIGuestSessionCreateWidget();
    if (m_pSessionCreateWidget)
    {
        m_pSplitter->addWidget(m_pSessionCreateWidget);
    }

    m_pSplitter->setOrientation(Qt::Vertical);
    m_pMainLayout->addWidget(m_pSplitter);


    // m_pSplitter->setStretchFactor(0, 9);
    // m_pSplitter->setStretchFactor(1, 4);
}

void UIGuestControlFileManager::prepareConnections()
{

}

#include "UIGuestControlFileManager.moc"
