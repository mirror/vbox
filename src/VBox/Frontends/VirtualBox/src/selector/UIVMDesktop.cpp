/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMDesktop class implementation.
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
# include <QLabel>
# include <QStackedWidget>
# include <QToolButton>
# ifdef VBOX_WS_MAC
#  include <QTimer>
# endif /* VBOX_WS_MAC */

/* GUI includes */
# include "UIBar.h"
# include "UIIconPool.h"
# include "UISpacerWidgets.h"
# include "UISpecialControls.h"
# include "UIVMDesktop.h"
# include "UIVMItem.h"
# include "UIToolBar.h"
# include "UISnapshotPane.h"
# include "VBoxUtils.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <QStackedLayout>


#ifdef VBOX_WS_MAC
static const int gsLeftMargin = 5;
static const int gsTopMargin = 5;
static const int gsRightMargin = 5;
static const int gsBottomMargin = 5;
#else /* VBOX_WS_MAC */
static const int gsLeftMargin = 0;
static const int gsTopMargin = 5;
static const int gsRightMargin = 5;
static const int gsBottomMargin = 5;
#endif /* !VBOX_WS_MAC */

/* Container to store VM desktop panes. */
class UIVMDesktopPrivate : public QIWithRetranslateUI<QStackedWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVMDesktopPrivate(QWidget *pParent, QAction *pRefreshAction);

    /* API: Pane text setters stuff: */
    void setText(const QString &strText);
    void setError(const QString &strError);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helpers: Prepare stuff: */
    void prepareTextPane();
    void prepareErrorPane();

    /* Text pane stuff: */
    QRichTextBrowser *m_pText;

    /* Error pane stuff: */
    QWidget *m_pErrBox;
    QLabel *m_pErrLabel;
    QTextBrowser *m_pErrText;
    QToolButton *m_pRefreshButton;
    QAction *m_pRefreshAction;
};

UIVMDesktopPrivate::UIVMDesktopPrivate(QWidget *pParent, QAction *pRefreshAction)
    : QIWithRetranslateUI<QStackedWidget>(pParent)
    , m_pText(0)
    , m_pErrBox(0), m_pErrLabel(0), m_pErrText(0)
    , m_pRefreshButton(0), m_pRefreshAction(pRefreshAction)
{
    /* Make sure refresh action was passed: */
    AssertMsg(m_pRefreshAction, ("Refresh action was NOT passed!"));

    /* Translate finally: */
    retranslateUi();
}

void UIVMDesktopPrivate::setText(const QString &strText)
{
    /* Prepare text pane if necessary: */
    prepareTextPane();

    /* Assign corresponding text: */
    m_pText->setText(strText);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pText));
}

void UIVMDesktopPrivate::setError(const QString &strError)
{
    /* Prepare error pane if necessary: */
    prepareErrorPane();

    /* Assign corresponding text: */
    m_pErrText->setText(strError);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pErrBox));
}

void UIVMDesktopPrivate::retranslateUi()
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

void UIVMDesktopPrivate::prepareTextPane()
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

void UIVMDesktopPrivate::prepareErrorPane()
{
    if (m_pErrBox)
        return;

    /* Create error pane: */
    m_pErrBox = new QWidget;

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(m_pErrBox);
    pMainLayout->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);
    pMainLayout->setSpacing(10);

    /* Create error label: */
    m_pErrLabel = new QLabel(m_pErrBox);
    m_pErrLabel->setWordWrap(true);
    m_pErrLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pMainLayout->addWidget(m_pErrLabel);

    /* Create error text browser: */
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

enum
{
    Dtls = 0,
    Snap
};

UIVMDesktop::UIVMDesktop(UIToolBar *pToolBar, QAction *pRefreshAction, QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
{
    /* Create container: */
    QWidget *pContainer = new QWidget;
    {
        /* Create layout: */
        QHBoxLayout *pLayout = new QHBoxLayout(pContainer);
        {
            /* Configure layout: */
            pLayout->setContentsMargins(0, 0, 0, 0);
            /* Create segmented-button: */
            m_pHeaderBtn = new UITexturedSegmentedButton(pContainer, 2);
            {
                /* Configure segmented-button: */
                m_pHeaderBtn->setIcon(Dtls, UIIconPool::iconSet(":/vm_settings_16px.png",
                                                                ":/vm_settings_disabled_16px.png"));
                m_pHeaderBtn->setIcon(Snap, UIIconPool::iconSet(":/snapshot_take_16px.png",
                                                                ":/snapshot_take_disabled_16px.png"));
                /* Add segmented-buttons into layout: */
                pLayout->addWidget(m_pHeaderBtn);
            }
        }
    }

#ifdef VBOX_WS_MAC
    /* Cocoa stuff should be async...
     * Do not ask me why but otherwise
     * it conflicts with native handlers. */
    QTimer::singleShot(0, this, SLOT(sltInit()));
#else /* !VBOX_WS_MAC */
    sltInit();
#endif /* !VBOX_WS_MAC */

    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    /* The header to select the different pages.
     * Has different styles on the different platforms. */
    if (pToolBar)
    {
        pToolBar->addWidget(new UIHorizontalSpacerWidget(this));
        pToolBar->addWidget(pContainer);
        QWidget *pSpace = new QWidget(this);
        /* We need a little bit more space for the beta label. */
        if (vboxGlobal().isBeta())
            pSpace->setFixedSize(28, 1);
        else
            pSpace->setFixedSize(10, 1);
        pToolBar->addWidget(pSpace);
#ifdef VBOX_WS_MAC
        pToolBar->updateLayout();
#endif /* VBOX_WS_MAC */
    }
    else
    {
        UIBar *pBar = new UIBar(this);
        pBar->setContentWidget(pContainer);
        pMainLayout->addWidget(pBar);
    }

    /* Create desktop pane: */
    m_pDesktopPrivate = new UIVMDesktopPrivate(this, pRefreshAction);

    /* Create snapshot pane: */
    m_pSnapshotsPane = new UISnapshotPane(this);
    m_pSnapshotsPane->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);

    /* Add the pages: */
    m_pStackedLayout = new QStackedLayout(pMainLayout);
    m_pStackedLayout->addWidget(m_pDesktopPrivate);
    m_pStackedLayout->addWidget(m_pSnapshotsPane);

    /* Connect the header buttons with the stack layout: */
    connect(m_pHeaderBtn, SIGNAL(clicked(int)), m_pStackedLayout, SLOT(setCurrentIndex(int)));
    connect(m_pStackedLayout, SIGNAL(currentChanged(int)), this, SIGNAL(sigCurrentChanged(int)));

    /* Translate finally: */
    retranslateUi();
}

int UIVMDesktop::widgetIndex() const
{
    return m_pStackedLayout->currentIndex();
}

void UIVMDesktop::updateDetailsText(const QString &strText)
{
    m_pDesktopPrivate->setText(strText);
}

void UIVMDesktop::updateDetailsError(const QString &strError)
{
    m_pDesktopPrivate->setError(strError);
}

void UIVMDesktop::updateSnapshots(UIVMItem *pVMItem, const CMachine& machine)
{
    /* Update the snapshots header name: */
    QString name = tr("&Snapshots");
    if (pVMItem)
    {
        ULONG count = pVMItem->snapshotCount();
        if (count)
            name += QString(" (%1)").arg(count);
    }
    m_pHeaderBtn->setTitle(Snap, name);

    /* Refresh the snapshots widget: */
    if (!machine.isNull())
    {
        m_pHeaderBtn->setEnabled(Snap, true);
        m_pSnapshotsPane->setMachine(machine);
    }
    else
        lockSnapshots();
}

void UIVMDesktop::lockSnapshots()
{
    m_pHeaderBtn->animateClick(Dtls);
    m_pHeaderBtn->setEnabled(Snap, false);
}

void UIVMDesktop::sltInit()
{
    m_pHeaderBtn->animateClick(0);
}

void UIVMDesktop::retranslateUi()
{
    m_pHeaderBtn->setTitle(Dtls, tr("&Details"));
}

#include "UIVMDesktop.moc"

