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
#include "UIPopupStack.h"
#include "QIMessageBox.h"
#include "VBoxGlobal.h"

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
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/,
                            const QString &strButtonText1 /*= QString()*/,
                            const QString &strButtonText2 /*= QString()*/,
                            bool fProposeAutoConfirmation /*= false*/)
{
    showPopupPane(pParent, strId,
                  strMessage, strDetails,
                  iButton1, iButton2,
                  strButtonText1, strButtonText2,
                  fProposeAutoConfirmation);
}

void UIPopupCenter::error(QWidget *pParent, const QString &strId,
                          const QString &strMessage, const QString &strDetails,
                          bool fProposeAutoConfirmation /*= false*/)
{
    message(pParent, strId,
            strMessage, strDetails,
            AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape /* 1st button */,
            0 /* 2nd button */,
            QString() /* 1st button text */,
            QString() /* 2nd button text */,
            fProposeAutoConfirmation);
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strId,
                          const QString &strMessage,
                          bool fProposeAutoConfirmation /*= false*/)
{
    error(pParent, strId,
          strMessage, QString(),
          fProposeAutoConfirmation);
}

void UIPopupCenter::question(QWidget *pParent, const QString &strId,
                             const QString &strMessage,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             bool fProposeAutoConfirmation /*= false*/)
{
    message(pParent, strId,
            strMessage, QString(),
            iButton1, iButton2,
            strButtonText1, strButtonText2,
            fProposeAutoConfirmation);
}

void UIPopupCenter::questionBinary(QWidget *pParent, const QString &strId,
                                   const QString &strMessage,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/,
                                   bool fProposeAutoConfirmation /*= false*/)
{
    question(pParent, strId,
             strMessage,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strOkButtonText,
             strCancelButtonText,
             fProposeAutoConfirmation);
}

void UIPopupCenter::showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                                  const QString &strMessage, const QString &strDetails,
                                  int iButton1, int iButton2,
                                  const QString &strButtonText1, const QString &strButtonText2,
                                  bool fProposeAutoConfirmation)
{
    /* Make sure at least one button is valid: */
    if (iButton1 == 0 && iButton2 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape;

    /* Check if popup-pane was auto-confirmed before: */
    if (fProposeAutoConfirmation)
    {
        QStringList confirmedPopupList = vboxGlobal().virtualBox().GetExtraData(GUI_SuppressMessages).split(',');
        if (confirmedPopupList.contains(strPopupPaneID))
        {
            int iResultCode = AlertOption_AutoConfirmed;
            if (iButton1 & AlertButtonOption_Default)
                iResultCode |= (iButton1 & AlertButtonMask);
            else if (iButton2 & AlertButtonOption_Default)
                iResultCode |= (iButton2 & AlertButtonMask);
            emit sigPopupPaneDone(strPopupPaneID, iResultCode);
            return;
        }
    }

    /* Make sure parent is always set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

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
        connect(pPopupStack, SIGNAL(sigPopupPaneDone(QString, int)), this, SLOT(sltPopupPaneDone(QString, int)));
        connect(pPopupStack, SIGNAL(sigRemove()), this, SLOT(sltRemovePopupStack()));
        /* Show popup-stack: */
        pPopupStack->show();
    }

    /* Compose button description map: */
    QMap<int, QString> buttonDescriptions;
    if (iButton1 != 0 && !buttonDescriptions.contains(iButton1))
        buttonDescriptions[iButton1] = strButtonText1;
    if (iButton2 != 0 && !buttonDescriptions.contains(iButton2))
        buttonDescriptions[iButton2] = strButtonText2;
    /* Update corresponding popup-pane: */
    pPopupStack->updatePopupPane(strPopupPaneID, strMessage, strDetails, buttonDescriptions, fProposeAutoConfirmation);
}

void UIPopupCenter::sltPopupPaneDone(QString strPopupPaneID, int iResultCode)
{
    /* Was the result auto-confirmated? */
    if (iResultCode & AlertOption_AutoConfirmed)
    {
        /* Remember auto-confirmation fact: */
        QStringList confirmedPopupList = vboxGlobal().virtualBox().GetExtraData(GUI_SuppressMessages).split(',');
        confirmedPopupList << strPopupPaneID;
        vboxGlobal().virtualBox().SetExtraData(GUI_SuppressMessages, confirmedPopupList.join(","));
    }

    /* Notify listeners: */
    emit sigPopupPaneDone(strPopupPaneID, iResultCode);
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

void UIPopupCenter::remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute)
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
                 "by selecting the corresponding action from the menu bar.</p>"),
              true);
    }
    else
    {
        alert(pParent, QString("remindAboutMouseIntegration"),
              tr("<p>The Virtual Machine reports that the guest OS does not support <b>mouse pointer integration</b> "
                 "in the current video mode. You need to capture the mouse (by clicking over the VM display "
                 "or pressing the host key) in order to use the mouse inside the guest OS.</p>"),
              true);
    }
}

