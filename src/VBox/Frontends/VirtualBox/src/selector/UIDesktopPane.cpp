/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopPane class implementation.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
# include <QAction>
# include <QLabel>
# include <QStackedWidget>
# include <QToolButton>
# include <QVBoxLayout>

/* GUI includes */
# include "QIWithRetranslateUI.h"
# include "UIDesktopPane.h"
# include "VBoxUtils.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QStackedWidget subclass representing container which have two panes:
  * 1. Text pane reflecting base information about VirtualBox,
  * 2. Error pane reflecting information about currently chosen
  *    inaccessible VM and allowing to operate over it. */
class UIDesktopPanePrivate : public QIWithRetranslateUI<QStackedWidget>
{
    Q_OBJECT;

public:

    /** Constructs private desktop pane passing @a pParent to the base-class.
      * @param  pRefreshAction  Brings the refresh action reference. */
    UIDesktopPanePrivate(QWidget *pParent, QAction *pRefreshAction);

    /** Assigns @a strText and switches to text pane. */
    void setText(const QString &strText);
    /** Assigns @a strError and switches to error pane. */
    void setError(const QString &strError);

protected:

    /** Handles translation event. */
    void retranslateUi();

    /** Prepares text pane. */
    void prepareTextPane();
    /** Prepares error pane. */
    void prepareErrorPane();

private:

    /** Holds the text pane instance. */
    QRichTextBrowser *m_pText;

    /** Holds the error pane container. */
    QWidget *m_pErrBox;
    /** Holds the error label instance. */
    QLabel *m_pErrLabel;
    /** Holds the error text-browser instance. */
    QTextBrowser *m_pErrText;
    /** Holds the VM refresh button instance. */
    QToolButton *m_pRefreshButton;
    /** Holds the VM refresh action reference. */
    QAction *m_pRefreshAction;
};


/*********************************************************************************************************************************
*   Class UIDesktopPanePrivate implementation.                                                                                   *
*********************************************************************************************************************************/

UIDesktopPanePrivate::UIDesktopPanePrivate(QWidget *pParent, QAction *pRefreshAction)
    : QIWithRetranslateUI<QStackedWidget>(pParent)
    , m_pText(0)
    , m_pErrBox(0), m_pErrLabel(0), m_pErrText(0)
    , m_pRefreshButton(0), m_pRefreshAction(pRefreshAction)
{
    /* Translate finally: */
    retranslateUi();
}

void UIDesktopPanePrivate::setText(const QString &strText)
{
    /* Prepare text pane if necessary: */
    prepareTextPane();

    /* Assign corresponding text: */
    m_pText->setText(strText);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pText));
}

void UIDesktopPanePrivate::setError(const QString &strError)
{
    /* Prepare error pane if necessary: */
    prepareErrorPane();

    /* Assign corresponding text: */
    m_pErrText->setText(strError);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pErrBox));
}

void UIDesktopPanePrivate::retranslateUi()
{
    /* Translate error-label text: */
    if (m_pErrLabel)
        m_pErrLabel->setText(QApplication::translate("UIDetailsPagePrivate",
                                 "The selected virtual machine is <i>inaccessible</i>. "
                                 "Please inspect the error message shown below and press the "
                                 "<b>Refresh</b> button if you want to repeat the accessibility check:"));

    /* Translate refresh button & action text: */
    if (m_pRefreshAction && m_pRefreshButton)
    {
        m_pRefreshButton->setText(m_pRefreshAction->text());
        m_pRefreshButton->setIcon(m_pRefreshAction->icon());
        m_pRefreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
}

void UIDesktopPanePrivate::prepareTextPane()
{
    if (m_pText)
        return;

    /* Create text pane: */
    m_pText = new QRichTextBrowser(this);
    m_pText->setFocusPolicy(Qt::StrongFocus);
    m_pText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    /* Make text pane transparent: */
    m_pText->setFrameShape(QFrame::NoFrame);
    m_pText->viewport()->setAutoFillBackground(false);
    m_pText->setOpenLinks(false);

    /* Add into the stack: */
    addWidget(m_pText);

    /* Retranslate finally: */
    retranslateUi();
}

void UIDesktopPanePrivate::prepareErrorPane()
{
    if (m_pErrBox)
        return;

    /* Create error pane: */
    m_pErrBox = new QWidget;

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(m_pErrBox);
#if   defined(VBOX_WS_MAC)
    pMainLayout->setContentsMargins(4, 5, 5, 5);
#elif defined(VBOX_WS_WIN)
    pMainLayout->setContentsMargins(3, 5, 5, 0);
#elif defined(VBOX_WS_X11)
    pMainLayout->setContentsMargins(0, 5, 5, 5);
#endif
    pMainLayout->setSpacing(10);

    /* Create error label: */
    m_pErrLabel = new QLabel(m_pErrBox);
    m_pErrLabel->setWordWrap(true);
    m_pErrLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pMainLayout->addWidget(m_pErrLabel);

    /* Create error text-browser: */
    m_pErrText = new QTextBrowser(m_pErrBox);
    m_pErrText->setFocusPolicy(Qt::StrongFocus);
    m_pErrText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    pMainLayout->addWidget(m_pErrText);

    /* If refresh action was set: */
    if (m_pRefreshAction)
    {
        /* Create refresh button: */
        m_pRefreshButton = new QToolButton(m_pErrBox);
        m_pRefreshButton->setFocusPolicy(Qt::StrongFocus);

        /* Create refresh button layout: */
        QHBoxLayout *pButtonLayout = new QHBoxLayout;
        pMainLayout->addLayout(pButtonLayout);
        pButtonLayout->addStretch();
        pButtonLayout->addWidget(m_pRefreshButton);

        /* Connect refresh button: */
        connect(m_pRefreshButton, SIGNAL(clicked()), m_pRefreshAction, SIGNAL(triggered()));
    }

    pMainLayout->addStretch();

    /* Add into the stack: */
    addWidget(m_pErrBox);

    /* Retranslate finally: */
    retranslateUi();
}


/*********************************************************************************************************************************
*   Class UIDesktopPane implementation.                                                                                          *
*********************************************************************************************************************************/

UIDesktopPane::UIDesktopPane(QAction *pRefreshAction /* = 0 */, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
{
    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    /* Create desktop pane: */
    m_pDesktopPrivate = new UIDesktopPanePrivate(this, pRefreshAction);

    /* Add it to the layout: */
    pMainLayout->addWidget(m_pDesktopPrivate);
}

void UIDesktopPane::updateDetailsText(const QString &strText)
{
    m_pDesktopPrivate->setText(strText);
}

void UIDesktopPane::updateDetailsError(const QString &strError)
{
    m_pDesktopPrivate->setError(strError);
}

#include "UIDesktopPane.moc"

