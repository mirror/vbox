/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupCenter class implementation
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

/* GUI includes: */
#include "UIPopupCenter.h"
#include "UIPopupPane.h"

/* Other VBox includes: */
#include <VBox/sup.h>

/* static */
UIPopupCenter* UIPopupCenter::m_spInstance = 0;
UIPopupCenter* UIPopupCenter::instance() { return m_spInstance; }

/* static */
void UIPopupCenter::create()
{
    /* Make sure instance is NOT created yet: */
    if (m_spInstance)
        return;

    /* Create instance: */
    new UIPopupCenter;
}

/* static */
void UIPopupCenter::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!m_spInstance)
        return;

    /* Destroy instance: */
    delete m_spInstance;
}

UIPopupCenter::UIPopupCenter()
{
    /* Assign instance: */
    m_spInstance = this;
}

UIPopupCenter::~UIPopupCenter()
{
    /* Unassign instance: */
    m_spInstance = 0;
}

void UIPopupCenter::message(QWidget *pParent, const QString &strId,
                            const QString &strMessage, const QString &strDetails,
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                            const QString &strButtonText1 /* = QString() */,
                            const QString &strButtonText2 /* = QString() */,
                            const QString &strButtonText3 /* = QString() */)
{
    showPopupPane(pParent, strId,
                  strMessage, strDetails,
                  iButton1, iButton2, iButton3,
                  strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::error(QWidget *pParent, const QString &strId,
                          const QString &strMessage, const QString &strDetails)
{
    message(pParent, strId,
            strMessage, strDetails,
            AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape);
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strId,
                          const QString &strMessage)
{
    error(pParent, strId,
          strMessage, QString());
}

void UIPopupCenter::question(QWidget *pParent, const QString &strId,
                             const QString &strMessage,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             const QString &strButtonText3 /*= QString()*/)
{
    message(pParent, strId,
            strMessage, QString(),
            iButton1, iButton2, iButton3,
            strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::questionBinary(QWidget *pParent, const QString &strId,
                                   const QString &strMessage,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/)
{
    question(pParent, strId,
             strMessage,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             0 /* third button */,
             strOkButtonText,
             strCancelButtonText,
             QString() /* third button */);
}

void UIPopupCenter::questionTrinary(QWidget *pParent, const QString &strId,
                                    const QString &strMessage,
                                    const QString &strChoice1ButtonText /*= QString()*/,
                                    const QString &strChoice2ButtonText /*= QString()*/,
                                    const QString &strCancelButtonText /*= QString()*/)
{
    question(pParent, strId,
             strMessage,
             AlertButton_Choice1,
             AlertButton_Choice2 | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strChoice1ButtonText,
             strChoice2ButtonText,
             strCancelButtonText);
}

void UIPopupCenter::showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                                 const QString &strMessage, const QString &strDetails,
                                 int iButton1, int iButton2, int iButton3,
                                 const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3)
{
    /* Looking for the corresponding popup-stack: */
    QString strPopupStackID = pParent->metaObject()->className();
    UIPopupStack *pPopupStack = 0;
    /* Is there already popup-stack with the same ID? */
    if (m_stacks.contains(strPopupStackID))
    {
        /* Get existing one: */
        pPopupStack = m_stacks[strPopupStackID];
    }
    /* There is no popup-stack with the same ID? */
    else
    {
        /* Create new one: */
        pPopupStack = m_stacks[strPopupStackID] = new UIPopupStack(pParent);
        /* Attach popup-stack connections: */
        connect(pPopupStack, SIGNAL(sigPopupPaneDone(QString, int)), this, SIGNAL(sigPopupPaneDone(QString, int)));
        connect(pPopupStack, SIGNAL(sigRemove()), this, SLOT(sltRemovePopupStack()));
    }
    /* Show popup-stack: */
    pPopupStack->show();

    /* Choose at least one button to be the 'default' and 'escape': */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape;
    /* Compose button description map: */
    QMap<int, QString> buttonDescriptions;
    if (iButton1 != 0 && !buttonDescriptions.contains(iButton1))
        buttonDescriptions[iButton1] = strButtonText1;
    if (iButton2 != 0 && !buttonDescriptions.contains(iButton2))
        buttonDescriptions[iButton2] = strButtonText2;
    if (iButton3 != 0 && !buttonDescriptions.contains(iButton3))
        buttonDescriptions[iButton3] = strButtonText3;
    /* Update corresponding popup-pane: */
    pPopupStack->updatePopupPane(strPopupPaneID, strMessage, strDetails, buttonDescriptions);
}

void UIPopupCenter::sltRemovePopupStack()
{
    /* Make sure the sender is the popup-stack: */
    UIPopupStack *pPopupStack = qobject_cast<UIPopupStack*>(sender());
    if (!pPopupStack)
    {
        AssertMsgFailed(("Should be called by popup-stack only!"));
        return;
    }

    /* Make sure the popup-stack still exists: */
    const QString strPopupStackID(m_stacks.key(pPopupStack, QString()));
    if (strPopupStackID.isNull())
    {
        AssertMsgFailed(("Popup-stack already destroyed!"));
        return;
    }

    /* Cleanup the popup-stack: */
    m_stacks.remove(strPopupStackID);
    delete pPopupStack;
}

void UIPopupCenter::remindAboutMouseIntegration(bool fSupportsAbsolute, QWidget *pParent)
{
    if (fSupportsAbsolute)
    {
        alert(pParent, QString("remindAboutMouseIntegration"),
              tr("<p>The Virtual Machine reports that the guest OS supports <b>mouse pointer integration</b>. "
                 "This means that you do not need to <i>capture</i> the mouse pointer to be able to use it in your guest OS -- "
                 "all mouse actions you perform when the mouse pointer is over the Virtual Machine's display "
                 "are directly sent to the guest OS. If the mouse is currently captured, it will be automatically uncaptured.</p>"
                 "<p>The mouse icon on the status bar will look like&nbsp;<img src=:/mouse_seamless_16px.png/>&nbsp;to inform you "
                 "that mouse pointer integration is supported by the guest OS and is currently turned on.</p>"
                 "<p><b>Note</b>: Some applications may behave incorrectly in mouse pointer integration mode. "
                 "You can always disable it for the current session (and enable it again) "
                 "by selecting the corresponding action from the menu bar.</p>"));
    }
    else
    {
        alert(pParent, QString("remindAboutMouseIntegration"),
              tr("<p>The Virtual Machine reports that the guest OS does not support <b>mouse pointer integration</b> "
                 "in the current video mode. You need to capture the mouse (by clicking over the VM display "
                 "or pressing the host key) in order to use the mouse inside the guest OS.</p>"));
    }
}


/* Qt includes: */
#include <QVBoxLayout>
#include <QEvent>
#include <QMainWindow>
#include <QStatusBar>

UIPopupStack::UIPopupStack(QWidget *pParent)
    : QWidget(pParent)
    , m_fPolished(false)
    , m_iStackLayoutMargin(0)
    , m_iStackLayoutSpacing(0)
    , m_iParentStatusBarHeight(parentStatusBarHeight(pParent))
{
    /* Prepare: */
    prepare();
}

void UIPopupStack::updatePopupPane(const QString &strPopupPaneID,
                                   const QString &strMessage, const QString &strDetails,
                                   const QMap<int, QString> &buttonDescriptions)
{
    /* Looking for the corresponding popup-pane: */
    UIPopupPane *pPopupPane = 0;
    /* Is there already popup-pane with the same ID? */
    if (m_panes.contains(strPopupPaneID))
    {
        /* Get existing one: */
        pPopupPane = m_panes[strPopupPaneID];
        /* And update message and details: */
        pPopupPane->setMessage(strMessage);
        pPopupPane->setDetails(strDetails);
    }
    /* There is no popup-pane with the same ID? */
    else
    {
        /* Create new one: */
        pPopupPane = m_panes[strPopupPaneID] = new UIPopupPane(this,
                                                               strMessage, strDetails,
                                                               buttonDescriptions);
        pPopupPane->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        /* Attach popup-pane connection: */
        connect(pPopupPane, SIGNAL(sigSizeHintChanged()), this, SLOT(sltAdjustGeometry()));
        connect(pPopupPane, SIGNAL(sigDone(int)), this, SLOT(sltPopupPaneDone(int)));
        /* Show popup-pane: */
        pPopupPane->show();
    }

    /* Propagate desired width: */
    setDesiredWidth(parentWidget()->width());
    /* Adjust geometry: */
    sltAdjustGeometry();
}

void UIPopupStack::sltAdjustGeometry()
{
    /* Get this attributes: */
    const int iWidth = parentWidget()->width();
    const int iHeight = minimumHeightHint();

    /* Move according parent: */
    move(0, parentWidget()->height() - iHeight - m_iParentStatusBarHeight);
    /* Resize according parent: */
    resize(iWidth, iHeight);

    /* Layout content: */
    layoutContent();
}

void UIPopupStack::sltPopupPaneDone(int iButtonCode)
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
    emit sigPopupPaneDone(strPopupPaneID, iButtonCode);

    /* Cleanup the popup-pane: */
    m_panes.remove(strPopupPaneID);
    delete pPopupPane;

    /* Make sure this stack still contains popup-panes: */
    if (!m_panes.isEmpty())
        return;

    /* Notify listeners about popup-stack: */
    emit sigRemove();
}

void UIPopupStack::prepare()
{
    /* Install event-filter to parent: */
    parent()->installEventFilter(this);
}

int UIPopupStack::minimumWidthHint()
{
    /* Prepare width hint: */
    int iWidthHint = 0;

    /* Take into account all the panes: */
    foreach (UIPopupPane *pPane, m_panes)
        iWidthHint = qMax(iWidthHint, pPane->minimumWidthHint());

    /* And two margins finally: */
    iWidthHint += 2 * m_iStackLayoutMargin;

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
        iHeightHint += (m_panes.size() - 1) * m_iStackLayoutSpacing;

    /* And two margins finally: */
    iHeightHint += 2 * m_iStackLayoutMargin;

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
        pPane->setDesiredWidth(iWidth - 2 * m_iStackLayoutMargin);
}

void UIPopupStack::layoutContent()
{
    /* Get attributes: */
    const int iWidth = width();
    const int iHeight = height();
    int iX = m_iStackLayoutMargin;
    int iY = iHeight - m_iStackLayoutMargin;

    /* Layout every pane we have: */
    foreach (UIPopupPane *pPane, m_panes)
    {
        /* Get pane attributes: */
        const int iPaneWidth = iWidth - 2 * m_iStackLayoutMargin;
        const int iPaneHeight = pPane->minimumHeightHint();
        /* Adjust geometry for the pane: */
        pPane->move(iX, iY - iPaneHeight);
        pPane->resize(iPaneWidth, iPaneHeight);
        pPane->layoutContent();
        /* Increment placeholder: */
        iY -= (iPaneHeight + m_iStackLayoutSpacing);
    }
}

bool UIPopupStack::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Make sure its parent event came: */
    if (pWatched != parent())
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

void UIPopupStack::showEvent(QShowEvent *pEvent)
{
    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIPopupStack::polishEvent(QShowEvent*)
{
    /* Propagate desired width: */
    setDesiredWidth(parentWidget()->width());
    /* Adjust geometry: */
    sltAdjustGeometry();
}

/* static */
int UIPopupStack::parentStatusBarHeight(QWidget *pParent)
{
    /* Check if passed parent is QMainWindow and contains status-bar: */
    if (QMainWindow *pParentWindow = qobject_cast<QMainWindow*>(pParent))
        if (pParentWindow->statusBar())
            return pParentWindow->statusBar()->height();
    /* Zero by default: */
    return 0;
}

