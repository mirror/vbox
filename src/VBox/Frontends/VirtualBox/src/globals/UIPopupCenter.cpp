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
#include "UIMachineWindow.h"
#include "QIMessageBox.h"
#include "VBoxGlobal.h"
#include "UIHostComboEditor.h"

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
    /* Prepare instance: */
    m_spInstance->prepare();
}

/* static */
void UIPopupCenter::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!m_spInstance)
        return;

    /* Cleanup instance: */
    m_spInstance->cleanup();
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

void UIPopupCenter::prepare()
{
}

void UIPopupCenter::cleanup()
{
    /* Make sure all the popup-stack types destroyed: */
    foreach (const QString &strPopupStackTypeID, m_stackTypes.keys())
        m_stackTypes.remove(strPopupStackTypeID);
    /* Make sure all the popup-stacks destroyed: */
    foreach (const QString &strPopupStackID, m_stacks.keys())
    {
        delete m_stacks[strPopupStackID];
        m_stacks.remove(strPopupStackID);
    }
}

void UIPopupCenter::showPopupStack(QWidget *pParent)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Make sure corresponding popup-stack *exists*: */
    const QString strPopupStackID(popupStackID(pParent));
    if (!m_stacks.contains(strPopupStackID))
        return;

    /* Assign stack with passed parent: */
    UIPopupStack *pPopupStack = m_stacks[strPopupStackID];
    assignPopupStackParent(pPopupStack, pParent, m_stackTypes[strPopupStackID]);
    pPopupStack->show();
}

void UIPopupCenter::hidePopupStack(QWidget *pParent)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Make sure corresponding popup-stack *exists*: */
    const QString strPopupStackID(popupStackID(pParent));
    if (!m_stacks.contains(strPopupStackID))
        return;

    /* Unassign stack with passed parent: */
    UIPopupStack *pPopupStack = m_stacks[strPopupStackID];
    pPopupStack->hide();
    unassignPopupStackParent(pPopupStack, pParent);
}

void UIPopupCenter::setPopupStackType(QWidget *pParent, UIPopupStackType newStackType)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Composing corresponding popup-stack: */
    const QString strPopupStackID(popupStackID(pParent));

    /* Looking for current popup-stack type, create if it doesn't exists: */
    UIPopupStackType &stackType = m_stackTypes[strPopupStackID];

    /* Make sure stack-type has changed: */
    if (stackType == newStackType)
        return;

    /* Remember new stack type: */
    LogRelFlow(("UIPopupCenter::setPopupStackType: Changing type of popup-stack with ID = '%s' from '%s' to '%s'.\n",
                strPopupStackID.toAscii().constData(),
                stackType == UIPopupStackType_Separate ? "separate window" : "embedded widget",
                newStackType == UIPopupStackType_Separate ? "separate window" : "embedded widget"));
    stackType = newStackType;
}

void UIPopupCenter::message(QWidget *pParent, const QString &strPopupPaneID,
                            const QString &strMessage, const QString &strDetails,
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/,
                            const QString &strButtonText1 /*= QString()*/,
                            const QString &strButtonText2 /*= QString()*/,
                            bool fProposeAutoConfirmation /*= false*/)
{
    showPopupPane(pParent, strPopupPaneID,
                  strMessage, strDetails,
                  iButton1, iButton2,
                  strButtonText1, strButtonText2,
                  fProposeAutoConfirmation);
}

void UIPopupCenter::error(QWidget *pParent, const QString &strPopupPaneID,
                          const QString &strMessage, const QString &strDetails,
                          bool fProposeAutoConfirmation /*= false*/)
{
    message(pParent, strPopupPaneID,
            strMessage, strDetails,
            AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape /* 1st button */,
            0 /* 2nd button */,
            QApplication::translate("UIMessageCenter", "Close") /* 1st button text */,
            QString() /* 2nd button text */,
            fProposeAutoConfirmation);
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strPopupPaneID,
                          const QString &strMessage,
                          bool fProposeAutoConfirmation /*= false*/)
{
    error(pParent, strPopupPaneID,
          strMessage, QString(),
          fProposeAutoConfirmation);
}

void UIPopupCenter::question(QWidget *pParent, const QString &strPopupPaneID,
                             const QString &strMessage,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             bool fProposeAutoConfirmation /*= false*/)
{
    message(pParent, strPopupPaneID,
            strMessage, QString(),
            iButton1, iButton2,
            strButtonText1, strButtonText2,
            fProposeAutoConfirmation);
}

void UIPopupCenter::questionBinary(QWidget *pParent, const QString &strPopupPaneID,
                                   const QString &strMessage,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/,
                                   bool fProposeAutoConfirmation /*= false*/)
{
    question(pParent, strPopupPaneID,
             strMessage,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strOkButtonText,
             strCancelButtonText,
             fProposeAutoConfirmation);
}

void UIPopupCenter::recall(QWidget *pParent, const QString &strPopupPaneID)
{
    hidePopupPane(pParent, strPopupPaneID);
}

void UIPopupCenter::showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                                  const QString &strMessage, const QString &strDetails,
                                  int iButton1, int iButton2,
                                  QString strButtonText1, QString strButtonText2,
                                  bool fProposeAutoConfirmation)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Make sure at least one button is valid: */
    if (iButton1 == 0 && iButton2 == 0)
    {
        iButton1 = AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape;
        strButtonText1 = QApplication::translate("UIMessageCenter", "Close");
    }

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

    /* Looking for corresponding popup-stack: */
    const QString strPopupStackID(popupStackID(pParent));
    UIPopupStack *pPopupStack = 0;
    /* If there is already popup-stack with such ID: */
    if (m_stacks.contains(strPopupStackID))
    {
        /* Just get existing one: */
        pPopupStack = m_stacks[strPopupStackID];
    }
    /* If there is no popup-stack with such ID: */
    else
    {
        /* Create new one: */
        pPopupStack = m_stacks[strPopupStackID] = new UIPopupStack(strPopupStackID);
        /* Attach popup-stack connections: */
        connect(pPopupStack, SIGNAL(sigPopupPaneDone(QString, int)), this, SLOT(sltPopupPaneDone(QString, int)));
        connect(pPopupStack, SIGNAL(sigRemove(QString)), this, SLOT(sltRemovePopupStack(QString)));
        /* Show popup-stack: */
        showPopupStack(pParent);
    }

    /* If there is already popup-pane with such ID: */
    if (pPopupStack->exists(strPopupPaneID))
    {
        /* Just update existing one: */
        pPopupStack->updatePopupPane(strPopupPaneID, strMessage, strDetails);
    }
    /* If there is no popup-pane with such ID: */
    else
    {
        /* Compose button description map: */
        QMap<int, QString> buttonDescriptions;
        if (iButton1 != 0 && !buttonDescriptions.contains(iButton1))
            buttonDescriptions[iButton1] = strButtonText1;
        if (iButton2 != 0 && !buttonDescriptions.contains(iButton2))
            buttonDescriptions[iButton2] = strButtonText2;
        /* Create new one: */
        pPopupStack->createPopupPane(strPopupPaneID, strMessage, strDetails,
                                     buttonDescriptions, fProposeAutoConfirmation);
    }
}

void UIPopupCenter::hidePopupPane(QWidget *pParent, const QString &strPopupPaneID)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Make sure corresponding popup-stack *exists*: */
    const QString strPopupStackID(popupStackID(pParent));
    if (!m_stacks.contains(strPopupStackID))
        return;

    /* Make sure corresponding popup-pane *exists*: */
    UIPopupStack *pPopupStack = m_stacks[strPopupStackID];
    if (!pPopupStack->exists(strPopupPaneID))
        return;

    /* Recall corresponding popup-pane: */
    pPopupStack->recallPopupPane(strPopupPaneID);
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

void UIPopupCenter::sltRemovePopupStack(QString strPopupStackID)
{
    /* Make sure corresponding popup-stack *exists*: */
    if (!m_stacks.contains(strPopupStackID))
    {
        AssertMsgFailed(("Popup-stack already destroyed!"));
        return;
    }

    /* Delete popup-stack asyncronously.
     * To avoid issues with events which already posted: */
    UIPopupStack *pPopupStack = m_stacks[strPopupStackID];
    m_stacks.remove(strPopupStackID);
    pPopupStack->deleteLater();
}

/* static */
QString UIPopupCenter::popupStackID(QWidget *pParent)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return QString();

    /* Special handling for Runtime UI: */
    if (qobject_cast<UIMachineWindow*>(pParent))
        return QString("UIMachineWindow");

    /* Common handling for other cases: */
    return pParent->metaObject()->className();
}

/* static */
void UIPopupCenter::assignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent, UIPopupStackType stackType)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Assign event-filter: */
    pParent->installEventFilter(pPopupStack);

    /* Assign parent depending on passed *stack* type: */
    switch (stackType)
    {
        case UIPopupStackType_Embedded:
        {
            pPopupStack->setParent(pParent);
            break;
        }
        case UIPopupStackType_Separate:
        {
            pPopupStack->setParent(pParent, Qt::Tool | Qt::FramelessWindowHint);
            break;
        }
        default: break;
    }
}

/* static */
void UIPopupCenter::unassignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent)
{
    /* Make sure parent is set! */
    AssertMsg(pParent, ("Parent is NULL!"));
    if (!pParent)
        return;

    /* Unassign parent: */
    pPopupStack->setParent(0);

    /* Unassign event-filter: */
    pParent->removeEventFilter(pPopupStack);
}

void UIPopupCenter::cannotSendACPIToMachine(QWidget *pParent)
{
    alert(pParent, "cannotSendACPIToMachine",
          QApplication::translate("UIMessageCenter", "You are trying to shut down the guest with the ACPI power button. "
                                                     "This is currently not possible because the guest does not support software shutdown."));
}

void UIPopupCenter::remindAboutAutoCapture(QWidget *pParent)
{
    alert(pParent, "remindAboutAutoCapture",
          QApplication::translate("UIMessageCenter", "<p>You have the <b>Auto capture keyboard</b> option turned on. "
                                                     "This will cause the Virtual Machine to automatically <b>capture</b> "
                                                     "the keyboard every time the VM window is activated and make it "
                                                     "unavailable to other applications running on your host machine: "
                                                     "when the keyboard is captured, all keystrokes (including system ones "
                                                     "like Alt-Tab) will be directed to the VM.</p>"
                                                     "<p>You can press the <b>host key</b> at any time to <b>uncapture</b> the "
                                                     "keyboard and mouse (if it is captured) and return them to normal "
                                                     "operation. The currently assigned host key is shown on the status bar "
                                                     "at the bottom of the Virtual Machine window, next to the&nbsp;"
                                                     "<img src=:/hostkey_16px.png/>&nbsp;icon. This icon, together "
                                                     "with the mouse icon placed nearby, indicate the current keyboard "
                                                     "and mouse capture state.</p>") +
          QApplication::translate("UIMessageCenter", "<p>The host key is currently defined as <b>%1</b>.</p>",
                                                     "additional message box paragraph")
                                                     .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
          true);
}

void UIPopupCenter::remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute)
{
    if (fSupportsAbsolute)
    {
        alert(pParent, "remindAboutMouseIntegration",
              QApplication::translate("UIMessageCenter", "<p>The Virtual Machine reports that the guest OS supports <b>mouse pointer integration</b>. "
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
        alert(pParent, "remindAboutMouseIntegration",
              QApplication::translate("UIMessageCenter", "<p>The Virtual Machine reports that the guest OS does not support <b>mouse pointer integration</b> "
                                                         "in the current video mode. You need to capture the mouse (by clicking over the VM display "
                                                         "or pressing the host key) in order to use the mouse inside the guest OS.</p>"),
              true);
    }
}

void UIPopupCenter::remindAboutPausedVMInput(QWidget *pParent)
{
    alert(pParent, "remindAboutPausedVMInput",
          QApplication::translate("UIMessageCenter", "<p>The Virtual Machine is currently in the <b>Paused</b> state and not able to see any keyboard or mouse input. "
                                                     "If you want to continue to work inside the VM, you need to resume it by selecting the corresponding action "
                                                     "from the menu bar.</p>"),
          true);
}

void UIPopupCenter::forgetAboutPausedVMInput(QWidget *pParent)
{
    recall(pParent, "remindAboutPausedVMInput");
}

void UIPopupCenter::remindAboutWrongColorDepth(QWidget *pParent, ulong uRealBPP, ulong uWantedBPP)
{
    alert(pParent, "remindAboutWrongColorDepth",
          QApplication::translate("UIMessageCenter", "<p>The virtual machine window is optimized to work in <b>%1&nbsp;bit</b> color mode "
                                                     "but the virtual display is currently set to <b>%2&nbsp;bit</b>.</p>"
                                                     "<p>Please open the display properties dialog of the guest OS and select a <b>%3&nbsp;bit</b> color mode, "
                                                     "if it is available, for best possible performance of the virtual video subsystem.</p>"
                                                     "<p><b>Note</b>. Some operating systems, like OS/2, may actually work in 32&nbsp;bit mode "
                                                     "but report it as 24&nbsp;bit (16 million colors). You may try to select a different color "
                                                     "mode to see if this message disappears or you can simply disable the message now "
                                                     "if you are sure the required color mode (%4&nbsp;bit) is not available in the guest OS.</p>")
                                                     .arg(uWantedBPP).arg(uRealBPP).arg(uWantedBPP).arg(uWantedBPP),
          true);
}

void UIPopupCenter::forgetAboutWrongColorDepth(QWidget *pParent)
{
    recall(pParent, "remindAboutWrongColorDepth");
}

