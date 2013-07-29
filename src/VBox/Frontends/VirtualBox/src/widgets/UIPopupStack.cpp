/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupStack class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <QVBoxLayout>
#include <QScrollArea>
#include <QEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>

/* GUI includes: */
#include "UIPopupStack.h"
#include "UIPopupPane.h"
#include "UIMachineWindowNormal.h"

/* Other VBox includes: */
#include <VBox/sup.h>

UIPopupStack::UIPopupStack()
    : m_pScrollArea(0)
    , m_pScrollViewport(0)
    , m_iParentMenuBarHeight(0)
    , m_iParentStatusBarHeight(0)
{
    /* Prepare: */
    prepare();
}

bool UIPopupStack::exists(const QString &strPopupPaneID) const
{
    /* Redirect question to viewport: */
    return m_pScrollViewport->exists(strPopupPaneID);
}

void UIPopupStack::createPopupPane(const QString &strPopupPaneID,
                                   const QString &strMessage, const QString &strDetails,
                                   const QMap<int, QString> &buttonDescriptions,
                                   bool fProposeAutoConfirmation)
{
    /* Redirect request to viewport: */
    m_pScrollViewport->createPopupPane(strPopupPaneID,
                                       strMessage, strDetails,
                                       buttonDescriptions,
                                       fProposeAutoConfirmation);

    /* Propagate width: */
    propagateWidth();
}

void UIPopupStack::updatePopupPane(const QString &strPopupPaneID,
                                   const QString &strMessage, const QString &strDetails)
{
    /* Redirect request to viewport: */
    m_pScrollViewport->updatePopupPane(strPopupPaneID,
                                       strMessage, strDetails);

    /* Propagate width: */
    propagateWidth();
}

void UIPopupStack::recallPopupPane(const QString &strPopupPaneID)
{
    /* Redirect request to viewport: */
    m_pScrollViewport->recallPopupPane(strPopupPaneID);

    /* Propagate width: */
    propagateWidth();
}

void UIPopupStack::setParent(QWidget *pParent)
{
    /* Call to base-class: */
    QWidget::setParent(pParent);
    /* Recalculate parent menu-bar height: */
    m_iParentMenuBarHeight = parentMenuBarHeight(pParent);
    /* Recalculate parent status-bar height: */
    m_iParentStatusBarHeight = parentStatusBarHeight(pParent);
}

void UIPopupStack::setParent(QWidget *pParent, Qt::WindowFlags flags)
{
    /* Call to base-class: */
    QWidget::setParent(pParent, flags);
    /* Recalculate parent menu-bar height: */
    m_iParentMenuBarHeight = parentMenuBarHeight(pParent);
    /* Recalculate parent status-bar height: */
    m_iParentStatusBarHeight = parentStatusBarHeight(pParent);
}

void UIPopupStack::sltAdjustGeometry()
{
    /* Make sure parent is currently set: */
    if (!parent())
        return;

    /* Read parent geometry: */
    QRect geo(parentWidget()->geometry());

    /* Determine origin: */
    bool fIsWindow = isWindow();
    int iX = fIsWindow ? geo.x() : 0;
    int iY = fIsWindow ? geo.y() : 0;
    /* Subtract menu-bar height: */
    iY += m_iParentMenuBarHeight;

    /* Determine size: */
    int iWidth = parentWidget()->width();
    int iHeight = parentWidget()->height();
    /* Subtract menu-bar and status-bar heights: */
    iHeight -= (m_iParentMenuBarHeight + m_iParentStatusBarHeight);
    /* Check if minimum height is even less than current: */
    if (m_pScrollViewport)
    {
        /* Get minimum viewport height: */
        int iMinimumHeight = m_pScrollViewport->minimumSizeHint().height();
        /* Subtract layout margins: */
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        iMinimumHeight += (iTop + iBottom);
        /* Compare minimum and current height: */
        iHeight = qMin(iHeight, iMinimumHeight);
    }

    /* Adjust geometry: */
    setGeometry(iX, iY, iWidth, iHeight);
}

void UIPopupStack::sltPopupPaneRemoved(QString)
{
    /* Move focus to the parent: */
    if (parentWidget())
        parentWidget()->setFocus();
}

void UIPopupStack::prepare()
{
    /* Configure background: */
    setAutoFillBackground(false);
#if defined(Q_WS_WIN) || defined (Q_WS_MAC)
    /* Using Qt API to enable translucent background for the Win/Mac host.
     * - Under x11 host Qt 4.8.3 has it broken wih KDE 4.9 for now: */
    setAttribute(Qt::WA_TranslucentBackground);
#endif /* Q_WS_WIN || Q_WS_MAC */

#ifdef Q_WS_MAC
    /* Do not hide popup-stack
     * and actually the seamless machine-window too
     * due to Qt bug on window deactivation... */
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif /* Q_WS_MAC */

    /* Prepare content: */
    prepareContent();
}

void UIPopupStack::prepareContent()
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(0, 0, 0, 0);
        /* Create scroll-area: */
        m_pScrollArea = new QScrollArea;
        {
            /* Configure scroll-area: */
            m_pScrollArea->setCursor(Qt::ArrowCursor);
            m_pScrollArea->setWidgetResizable(true);
            m_pScrollArea->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
            m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            //m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            QPalette pal = m_pScrollArea->palette();
            pal.setColor(QPalette::Window, QColor(Qt::transparent));
            m_pScrollArea->setPalette(pal);
            /* Create scroll-viewport: */
            m_pScrollViewport = new UIPopupStackViewport;
            {
                /* Configure scroll-viewport: */
                m_pScrollViewport->setCursor(Qt::ArrowCursor);
                /* Connect scroll-viewport: */
                connect(this, SIGNAL(sigProposeStackViewportWidth(int)),
                        m_pScrollViewport, SLOT(sltHandleProposalForWidth(int)));
                connect(m_pScrollViewport, SIGNAL(sigSizeHintChanged()),
                        this, SLOT(sltAdjustGeometry()));
                connect(m_pScrollViewport, SIGNAL(sigPopupPaneDone(QString, int)),
                        this, SIGNAL(sigPopupPaneDone(QString, int)));
                connect(m_pScrollViewport, SIGNAL(sigPopupPaneRemoved(QString)),
                        this, SLOT(sltPopupPaneRemoved(QString)));
                connect(m_pScrollViewport, SIGNAL(sigRemove()),
                        this, SIGNAL(sigRemove()));
            }
            /* Assign scroll-viewport to scroll-area: */
            m_pScrollArea->setWidget(m_pScrollViewport);
        }
        /* Add scroll-area to layout: */
        m_pMainLayout->addWidget(m_pScrollArea);
    }
}

bool UIPopupStack::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Call to base-class if that is not parent event: */
    if (!parent() || pWatched != parent())
        return QWidget::eventFilter(pWatched, pEvent);

    /* Handle parent geometry events: */
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            /* Propagate stack width: */
            propagateWidth();
            /* Adjust geometry: */
            sltAdjustGeometry();
            break;
        }
        case QEvent::Move:
        {
            /* Adjust geometry: */
            sltAdjustGeometry();
            break;
        }
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pWatched, pEvent);
}

void UIPopupStack::showEvent(QShowEvent*)
{
    /* Propagate width: */
    propagateWidth();
    /* Adjust geometry: */
    sltAdjustGeometry();
}

void UIPopupStack::propagateWidth()
{
    /* Make sure parent is currently set: */
    if (!parent())
        return;

    /* Get parent width: */
    int iWidth = parentWidget()->width();
    /* Subtract left/right layout margins: */
    if (m_pMainLayout)
    {
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        iWidth -= (iLeft + iRight);
    }
    /* Subtract scroll-area frame-width: */
    if (m_pScrollArea)
    {
        iWidth -= 2 * m_pScrollArea->frameWidth();
    }

    /* Propose resulting width to viewport: */
    emit sigProposeStackViewportWidth(iWidth);
}

/* static */
int UIPopupStack::parentMenuBarHeight(QWidget *pParent)
{
    /* Menu-bar can exist only on QMainWindow sub-class: */
    if (QMainWindow *pMainWindow = qobject_cast<QMainWindow*>(pParent))
    {
        /* Search for existing menu-bar child: */
        if (QMenuBar *pMenuBar = pMainWindow->findChild<QMenuBar*>())
            return pMenuBar->height();
    }
    /* Zero by default: */
    return 0;
}

/* static */
int UIPopupStack::parentStatusBarHeight(QWidget *pParent)
{
    /* Status-bar can exist only on QMainWindow sub-class: */
    if (QMainWindow *pMainWindow = qobject_cast<QMainWindow*>(pParent))
    {
        /* Search for existing status-bar child: */
        if (QStatusBar *pStatusBar = pMainWindow->findChild<QStatusBar*>())
            return pStatusBar->height();
    }
    /* Zero by default: */
    return 0;
}


UIPopupStackViewport::UIPopupStackViewport()
    : m_iLayoutMargin(1)
    , m_iLayoutSpacing(1)
{
}

bool UIPopupStackViewport::exists(const QString &strPopupPaneID) const
{
    /* Is there already popup-pane with the same ID? */
    return m_panes.contains(strPopupPaneID);
}

void UIPopupStackViewport::createPopupPane(const QString &strPopupPaneID,
                                           const QString &strMessage, const QString &strDetails,
                                           const QMap<int, QString> &buttonDescriptions,
                                           bool fProposeAutoConfirmation)
{
    /* Make sure there is no such popup-pane already: */
    if (m_panes.contains(strPopupPaneID))
    {
        AssertMsgFailed(("Popup-pane already exists!"));
        return;
    }

    /* Create new popup-pane: */
    UIPopupPane *pPopupPane = m_panes[strPopupPaneID] = new UIPopupPane(this,
                                                                        strMessage, strDetails,
                                                                        buttonDescriptions,
                                                                        fProposeAutoConfirmation);

    /* Attach popup-pane connection: */
    connect(this, SIGNAL(sigProposePopupPaneWidth(int)), pPopupPane, SLOT(sltHandleProposalForWidth(int)));
    connect(pPopupPane, SIGNAL(sigSizeHintChanged()), this, SLOT(sltAdjustGeometry()));
    connect(pPopupPane, SIGNAL(sigDone(int)), this, SLOT(sltPopupPaneDone(int)));

    /* Show popup-pane: */
    pPopupPane->show();
}

void UIPopupStackViewport::updatePopupPane(const QString &strPopupPaneID,
                                           const QString &strMessage, const QString &strDetails)
{
    /* Make sure there is such popup-pane already: */
    if (!m_panes.contains(strPopupPaneID))
    {
        AssertMsgFailed(("Popup-pane doesn't exists!"));
        return;
    }

    /* Get existing popup-pane: */
    UIPopupPane *pPopupPane = m_panes[strPopupPaneID];

    /* Update message and details: */
    pPopupPane->setMessage(strMessage);
    pPopupPane->setDetails(strDetails);
}

void UIPopupStackViewport::recallPopupPane(const QString &strPopupPaneID)
{
    /* Make sure there is such popup-pane already: */
    if (!m_panes.contains(strPopupPaneID))
    {
        AssertMsgFailed(("Popup-pane doesn't exists!"));
        return;
    }

    /* Get existing popup-pane: */
    UIPopupPane *pPopupPane = m_panes[strPopupPaneID];

    /* Recall popup-pane: */
    pPopupPane->recall();
}

void UIPopupStackViewport::sltHandleProposalForWidth(int iWidth)
{
    /* Subtract layout margins: */
    iWidth -= 2 * m_iLayoutMargin;

    /* Propagate resulting width to popups: */
    emit sigProposePopupPaneWidth(iWidth);
}

void UIPopupStackViewport::sltAdjustGeometry()
{
    /* Update size-hint: */
    updateSizeHint();

    /* Layout content: */
    layoutContent();

    /* Notify parent popup-stack: */
    emit sigSizeHintChanged();
}

void UIPopupStackViewport::sltPopupPaneDone(int iResultCode)
{
    /* Make sure the sender is the popup-pane: */
    UIPopupPane *pPopupPane = qobject_cast<UIPopupPane*>(sender());
    if (!pPopupPane)
    {
        AssertMsgFailed(("Should be called by popup-pane only!"));
        return;
    }

    /* Make sure the popup-pane still exists: */
    const QString strPopupPaneID(m_panes.key(pPopupPane, QString()));
    if (strPopupPaneID.isNull())
    {
        AssertMsgFailed(("Popup-pane already destroyed!"));
        return;
    }

    /* Notify listeners about popup-pane removal: */
    emit sigPopupPaneDone(strPopupPaneID, iResultCode);

    /* Cleanup the popup-pane: */
    m_panes.remove(strPopupPaneID);
    delete pPopupPane;

    /* Notify listeners about popup-pane removed: */
    emit sigPopupPaneRemoved(strPopupPaneID);

    /* Adjust geometry: */
    sltAdjustGeometry();

    /* Make sure this stack still contains popup-panes: */
    if (!m_panes.isEmpty())
        return;

    /* Notify listeners about popup-stack: */
    emit sigRemove();
}

void UIPopupStackViewport::updateSizeHint()
{
    /* Calculate minimum width-hint: */
    int iMinimumWidthHint = 0;
    {
        /* Take into account all the panes: */
        foreach (UIPopupPane *pPane, m_panes)
            iMinimumWidthHint = qMax(iMinimumWidthHint, pPane->minimumSizeHint().width());

        /* And two margins finally: */
        iMinimumWidthHint += 2 * m_iLayoutMargin;
    }

    /* Calculate minimum height-hint: */
    int iMinimumHeightHint = 0;
    {
        /* Take into account all the panes: */
        foreach (UIPopupPane *pPane, m_panes)
            iMinimumHeightHint += pPane->minimumSizeHint().height();

        /* Take into account all the spacings, if any: */
        if (!m_panes.isEmpty())
            iMinimumHeightHint += (m_panes.size() - 1) * m_iLayoutSpacing;

        /* And two margins finally: */
        iMinimumHeightHint += 2 * m_iLayoutMargin;
    }

    /* Compose minimum size-hint: */
    m_minimumSizeHint = QSize(iMinimumWidthHint, iMinimumHeightHint);
}

void UIPopupStackViewport::layoutContent()
{
    /* Get attributes: */
    int iX = m_iLayoutMargin;
    int iY = m_iLayoutMargin;

    /* Layout every pane we have: */
    foreach (UIPopupPane *pPane, m_panes)
    {
        /* Get pane attributes: */
        QSize paneSize = pPane->minimumSizeHint();
        const int iPaneWidth = paneSize.width();
        const int iPaneHeight = paneSize.height();
        /* Adjust geometry for the pane: */
        pPane->setGeometry(iX, iY, iPaneWidth, iPaneHeight);
        pPane->layoutContent();
        /* Increment placeholder: */
        iY += (iPaneHeight + m_iLayoutSpacing);
    }
}

