/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxLicenseViewer class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
# include <QFile>
# include <QPushButton>
# include <QScrollBar>
# include <QTextBrowser>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "VBoxLicenseViewer.h"
# include "UIMessageCenter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


VBoxLicenseViewer::VBoxLicenseViewer(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QDialog>(pParent)
    , m_pLicenseBrowser(0)
    , m_pButtonAgree(0)
    , m_pButtonDisagree(0)
{
#if !(defined(VBOX_WS_WIN) || defined(VBOX_WS_MAC))
    /* Assign application icon: */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));
#endif

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create license browser: */
        m_pLicenseBrowser = new QTextBrowser(this);
        if (m_pLicenseBrowser)
        {
            /* Configure license browser: */
            m_pLicenseBrowser->verticalScrollBar()->installEventFilter(this);
            connect(m_pLicenseBrowser->verticalScrollBar(), &QScrollBar::valueChanged,
                    this, &VBoxLicenseViewer::sltHandleScrollBarMoved);

            /* Add into layout: */
            pMainLayout->addWidget(m_pLicenseBrowser);
        }

        /* Create agree button: */
        /** todo rework buttons to be a part of button-box itself */
        QDialogButtonBox *pDialogButtonBox = new QIDialogButtonBox;
        if (pDialogButtonBox)
        {
            /* Create agree button: */
            m_pButtonAgree = new QPushButton;
            if (m_pButtonAgree)
            {
                /* Configure button: */
                connect(m_pButtonAgree, &QPushButton::clicked, this, &QDialog::accept);

                /* Add into button-box: */
                pDialogButtonBox->addButton(m_pButtonAgree, QDialogButtonBox::AcceptRole);
            }

            /* Create agree button: */
            m_pButtonDisagree = new QPushButton;
            if (m_pButtonDisagree)
            {
                /* Configure button: */
                connect(m_pButtonDisagree, &QPushButton::clicked, this, &QDialog::reject);

                /* Add into button-box: */
                pDialogButtonBox->addButton(m_pButtonDisagree, QDialogButtonBox::RejectRole);
            }
        }

        /* Add into layout: */
        pMainLayout->addWidget(pDialogButtonBox);
    }

    /* Configure self: */
    resize(600, 450);

    /* Apply language settings: */
    retranslateUi();
}

int VBoxLicenseViewer::showLicenseFromString(const QString &strLicenseText)
{
    /* Set license text: */
    m_pLicenseBrowser->setText(strLicenseText);
    return exec();
}

int VBoxLicenseViewer::showLicenseFromFile(const QString &strLicenseFileName)
{
    /* Read license file: */
    QFile file(strLicenseFileName);
    if (file.open(QIODevice::ReadOnly))
        return showLicenseFromString(file.readAll());
    else
    {
        msgCenter().cannotOpenLicenseFile(strLicenseFileName, this);
        return QDialog::Rejected;
    }
}

bool VBoxLicenseViewer::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::Hide:
            if (pObject == m_pLicenseBrowser->verticalScrollBar())
                /* Doesn't work on wm's like ion3 where the window starts maximized: isActiveWindow() */
                sltUnlockButtons();
        default:
            break;
    }

    /* Call to base-class: */
    return QDialog::eventFilter(pObject, pEvent);
}

void VBoxLicenseViewer::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QDialog::showEvent(pEvent);

    /* Enable/disable buttons accordingly: */
    bool fScrollBarHidden =    !m_pLicenseBrowser->verticalScrollBar()->isVisible()
                            && !(windowState() & Qt::WindowMinimized);
    m_pButtonAgree->setEnabled(fScrollBarHidden);
    m_pButtonDisagree->setEnabled(fScrollBarHidden);
}

void VBoxLicenseViewer::retranslateUi()
{
    /* Translate dialog title: */
    setWindowTitle(tr("VirtualBox License"));

    /* Translate buttons: */
    m_pButtonAgree->setText(tr("I &Agree"));
    m_pButtonDisagree->setText(tr("I &Disagree"));
}

int VBoxLicenseViewer::exec()
{
    /* Nothing wrong with that, just hiding slot: */
    return QDialog::exec();
}

void VBoxLicenseViewer::sltHandleScrollBarMoved(int iValue)
{
    if (iValue == m_pLicenseBrowser->verticalScrollBar()->maximum())
        sltUnlockButtons();
}

void VBoxLicenseViewer::sltUnlockButtons()
{
    m_pButtonAgree->setEnabled(true);
    m_pButtonDisagree->setEnabled(true);
}

