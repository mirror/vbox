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
#include <QEvent>
#include <QMainWindow>
#include <QStatusBar>

/* GUI includes: */
#include "UIPopupStack.h"
#include "UIPopupPane.h"
#include "UIMachineWindowNormal.h"

/* Other VBox includes: */
#include <VBox/sup.h>

UIPopupStack::UIPopupStack()
    : m_iLayoutMargin(1), m_iLayoutSpacing(1)
{
    /* Make sure cursor is *always* valid: */
    setCursor(Qt::ArrowCursor);

#if defined(Q_WS_WIN) || defined (Q_WS_MAC)
    /* Using Qt API to enable translucent background for the Win/Mac host.
     * - Under x11 host Qt 4.8.3 has it broken wih KDE 4.9 for now: */
    setAttribute(Qt::WA_TranslucentBackground);
#endif /* Q_WS_WIN || Q_WS_MAC */
}

bool UIPopupStack::exists(const QString &strPopupPaneID) const
{
    /* Is there already popup-pane with the same ID? */
    return m_panes.contains(strPopupPaneID);
}

void UIPopupStack::createPopupPane(const QString &strPopupPaneID,
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
    connect(pPopupPane, SIGNAL(sigSizeHintChanged()), this, SLOT(sltAdjustGeometry()));
    connect(pPopupPane, SIGNAL(sigDone(int)), this, SLOT(sltPopupPaneDone(int)));
    /* Show popup-pane: */
    pPopupPane->show();

    /* Adjust geometry only if parent is currently set: */
    if (parent())
    {
        /* Propagate desired width: */
        setDesiredWidth(parentWidget()->width());
        /* Adjust geometry: */
        sltAdjustGeometry();
    }
}

void UIPopupStack::updatePopupPane(const QString &strPopupPaneID,
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

    /* Adjust geometry only if parent is currently set: */
    if (parent())
    {
        /* Propagate desired width: */
        setDesiredWidth(parentWidget()->width());
        /* Adjust geometry: */
        sltAdjustGeometry();
    }
}

void UIPopupStack::setParent(QWidget *pParent)
{
    /* Call to base-class: */
    QWidget::setParent(pParent);
    /* Recalculate parent status-bar height: */
    m_iParentStatusBarHeight = parentStatusBarHeight(pParent);
}

void UIPopupStack::setParent(QWidget *pParent, Qt::WindowFlags flags)
{
    /* Call to base-class: */
    QWidget::setParent(pParent, flags);
    /* Recalculate parent status-bar height: */
    m_iParentStatusBarHeight = parentStatusBarHeight(pParent);
}

void UIPopupStack::sltAdjustGeometry()
{
    /* Adjust geometry only if parent is currently set: */
    if (!parent())
        return;

    /* Get this attributes: */
    bool fIsWindow = isWindow();
    const int iX = fIsWindow ? parentWidget()->x() : 0;
    const int iY = fIsWindow ? parentWidget()->y() : 0;
    const int iWidth = parentWidget()->width();
    const int iHeight = minimumHeightHint();

    /* Move/resize according parent: */
    setGeometry(iX, iY, iWidth, iHeight);

    /* Layout content: */
    layoutContent();
}

void UIPopupStack::sltPopupPaneDone(int iResultCode)
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

    /* Notify listeners about popup-pane: */
    emit sigPopupPaneDone(strPopupPaneID, iResultCode);

    /* Cleanup the popup-pane: */
    m_panes.remove(strPopupPaneID);
    delete pPopupPane;

    /* Layout content: */
    layoutContent();

    /* Make sure this stack still contains popup-panes: */
    if (!m_panes.isEmpty())
        return;

    /* Notify listeners about popup-stack: */
    emit sigRemove();
}

int UIPopupStack::minimumWidthHint()
{
    /* Prepare width hint: */
    int iWidthHint = 0;

    /* Take into account all the panes: */
    foreach (UIPopupPane *pPane, m_panes)
        iWidthHint = qMax(iWidthHint, pPane->minimumWidthHint());

    /* And two margins finally: */
    iWidthHint += 2 * m_iLayoutMargin;

    /* Return width hint: */
    return iWidthHint;
}

int UIPopupStack::minimumHeightHint()
{
    /* Prepare height hint: */
    int iHeightHint = 0;

    /* Take into account all the panes: */
    foreach (UIPopupPane *pPane, m_panes)
        iHeightHint += pPane->minimumHeightHint();

    /* Take into account all the spacings, if any: */
    if (!m_panes.isEmpty())
        iHeightHint += (m_panes.size() - 1) * m_iLayoutSpacing;

    /* And two margins finally: */
    iHeightHint += 2 * m_iLayoutMargin;

    /* Return height hint: */
    return iHeightHint;
}

QSize UIPopupStack::minimumSizeHint()
{
    /* Wrap reimplemented getters: */
    return QSize(minimumWidthHint(), minimumHeightHint());
}

void UIPopupStack::setDesiredWidth(int iWidth)
{
    /* Propagate desired width to all the popup-panes we have: */
    foreach (UIPopupPane *pPane, m_panes)
        pPane->setDesiredWidth(iWidth - 2 * m_iLayoutMargin);
}

void UIPopupStack::layoutContent()
{
    /* Get attributes: */
    const int iWidth = width();
    const int iHeight = height();
    int iX = m_iLayoutMargin;
    int iY = iHeight - m_iLayoutMargin;

    /* Layout every pane we have: */
    foreach (UIPopupPane *pPane, m_panes)
    {
        /* Get pane attributes: */
        const int iPaneWidth = iWidth - 2 * m_iLayoutMargin;
        const int iPaneHeight = pPane->minimumHeightHint();
        /* Adjust geometry for the pane: */
        pPane->move(iX, iY - iPaneHeight);
        pPane->resize(iPaneWidth, iPaneHeight);
        pPane->layoutContent();
        /* Increment placeholder: */
        iY -= (iPaneHeight + m_iLayoutSpacing);
    }
}

bool UIPopupStack::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Make sure its parent event came: */
    if (!parent() || pWatched != parent())
        return false;

    /* Make sure its resize event came: */
    if (pEvent->type() != QEvent::Resize)
        return false;

    /* Propagate desired width: */
    setDesiredWidth(parentWidget()->width());
    /* Adjust geometry: */
    sltAdjustGeometry();

    /* Do not filter anything: */
    return false;
}

void UIPopupStack::showEvent(QShowEvent*)
{
    /* Adjust geometry only if parent is currently set: */
    if (parent())
    {
        /* Propagate desired width: */
        setDesiredWidth(parentWidget()->width());
        /* Adjust geometry: */
        sltAdjustGeometry();
    }
}

/* static */
int UIPopupStack::parentStatusBarHeight(QWidget *pParent)
{
    /* Status-bar can exist only on QMainWindow sub-class: */
    if (QMainWindow *pMainWindow = qobject_cast<QMainWindow*>(pParent))
    {
        /* Status-bar can exist only:
         * 1. on non-machine-window
         * 2. on machine-window in normal mode: */
        if (!qobject_cast<UIMachineWindow*>(pMainWindow) ||
            qobject_cast<UIMachineWindowNormal*>(pMainWindow))
            return pMainWindow->statusBar()->height();
    }
    /* Zero by default: */
    return 0;
}

