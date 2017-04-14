/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupCenter class implementation.
 */

/*
 * Copyright (C) 2013-2016 Oracle Corporation
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

/* GUI includes: */
# include "UIPopupCenter.h"
# include "UIPopupStack.h"
# include "UIMachineWindow.h"
# include "QIMessageBox.h"
# include "VBoxGlobal.h"
# include "UIHostComboEditor.h"
# include "UIExtraDataManager.h"

/* Other VBox includes: */
# include <VBox/sup.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


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
    AssertPtrReturnVoid(pParent);

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
    AssertPtrReturnVoid(pParent);

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
    AssertPtrReturnVoid(pParent);

    /* Composing corresponding popup-stack ID: */
    const QString strPopupStackID(popupStackID(pParent));

    /* Looking for current popup-stack type, create if it doesn't exists: */
    UIPopupStackType &stackType = m_stackTypes[strPopupStackID];

    /* Make sure stack-type has changed: */
    if (stackType == newStackType)
        return;

    /* Remember new stack type: */
    LogRelFlow(("UIPopupCenter::setPopupStackType: Changing type of popup-stack with ID = '%s' from '%s' to '%s'.\n",
                strPopupStackID.toLatin1().constData(),
                stackType == UIPopupStackType_Separate ? "separate window" : "embedded widget",
                newStackType == UIPopupStackType_Separate ? "separate window" : "embedded widget"));
    stackType = newStackType;
}

void UIPopupCenter::setPopupStackOrientation(QWidget *pParent, UIPopupStackOrientation newStackOrientation)
{
    /* Make sure parent is set! */
    AssertPtrReturnVoid(pParent);

    /* Composing corresponding popup-stack ID: */
    const QString strPopupStackID(popupStackID(pParent));

    /* Looking for current popup-stack orientation, create if it doesn't exists: */
    UIPopupStackOrientation &stackOrientation = m_stackOrientations[strPopupStackID];

    /* Make sure stack-orientation has changed: */
    if (stackOrientation == newStackOrientation)
        return;

    /* Remember new stack orientation: */
    LogRelFlow(("UIPopupCenter::setPopupStackType: Changing orientation of popup-stack with ID = '%s' from '%s' to '%s'.\n",
                strPopupStackID.toLatin1().constData(),
                stackOrientation == UIPopupStackOrientation_Top ? "top oriented" : "bottom oriented",
                newStackOrientation == UIPopupStackOrientation_Top ? "top oriented" : "bottom oriented"));
    stackOrientation = newStackOrientation;

    /* Update orientation for popup-stack if it currently exists: */
    if (m_stacks.contains(strPopupStackID))
        m_stacks[strPopupStackID]->setOrientation(stackOrientation);
}

void UIPopupCenter::message(QWidget *pParent, const QString &strPopupPaneID,
                            const QString &strMessage, const QString &strDetails,
                            const QString &strButtonText1 /* = QString()*/,
                            const QString &strButtonText2 /* = QString()*/,
                            bool fProposeAutoConfirmation /* = false*/)
{
    showPopupPane(pParent, strPopupPaneID,
                  strMessage, strDetails,
                  strButtonText1, strButtonText2,
                  fProposeAutoConfirmation);
}

void UIPopupCenter::popup(QWidget *pParent, const QString &strPopupPaneID,
                          const QString &strMessage)
{
    message(pParent, strPopupPaneID, strMessage, QString());
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strPopupPaneID,
                          const QString &strMessage,
                          bool fProposeAutoConfirmation /* = false*/)
{
    message(pParent, strPopupPaneID, strMessage, QString(),
            QApplication::translate("UIMessageCenter", "Close") /* 1st button text */,
            QString() /* 2nd button text */,
            fProposeAutoConfirmation);
}

void UIPopupCenter::question(QWidget *pParent, const QString &strPopupPaneID,
                             const QString &strMessage,
                             const QString &strButtonText1 /* = QString()*/,
                             const QString &strButtonText2 /* = QString()*/,
                             bool fProposeAutoConfirmation /* = false*/)
{
    message(pParent, strPopupPaneID, strMessage, QString(),
            strButtonText1, strButtonText2,
            fProposeAutoConfirmation);
}

void UIPopupCenter::recall(QWidget *pParent, const QString &strPopupPaneID)
{
    hidePopupPane(pParent, strPopupPaneID);
}

void UIPopupCenter::showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                                  const QString &strMessage, const QString &strDetails,
                                  QString strButtonText1 /* = QString()*/, QString strButtonText2 /* = QString()*/,
                                  bool fProposeAutoConfirmation /* = false*/)
{
    /* Make sure parent is set! */
    AssertPtrReturnVoid(pParent);

    /* Prepare buttons: */
    int iButton1 = 0;
    int iButton2 = 0;
    /* Make sure single button is properly configured: */
    if (!strButtonText1.isEmpty() && strButtonText2.isEmpty())
        iButton1 = AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape;
    else if (strButtonText1.isEmpty() && !strButtonText2.isEmpty())
        iButton2 = AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape;
    /* Make sure buttons are unique if set both: */
    else if (!strButtonText1.isEmpty() && !strButtonText2.isEmpty())
    {
        iButton1 = AlertButton_Ok | AlertButtonOption_Default;
        iButton2 = AlertButton_Cancel | AlertButtonOption_Escape;
        /* If user made a mistake in button names, we will fix that: */
        if (strButtonText1 == strButtonText2)
        {
            strButtonText1 = QApplication::translate("UIMessageCenter", "Ok");
            strButtonText1 = QApplication::translate("UIMessageCenter", "Cancel");
        }
    }

    /* Check if popup-pane was auto-confirmed before: */
    if ((iButton1 || iButton2) && fProposeAutoConfirmation)
    {
        const QStringList confirmedPopupList = gEDataManager->suppressedMessages();
        if (   confirmedPopupList.contains(strPopupPaneID)
            || confirmedPopupList.contains("allPopupPanes")
            || confirmedPopupList.contains("all") )
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
        pPopupStack = m_stacks[strPopupStackID] = new UIPopupStack(strPopupStackID, m_stackOrientations[strPopupStackID]);
        /* Attach popup-stack connections: */
        connect(pPopupStack, SIGNAL(sigPopupPaneDone(QString, int)), this, SLOT(sltPopupPaneDone(QString, int)));
        connect(pPopupStack, SIGNAL(sigRemove(QString)), this, SLOT(sltRemovePopupStack(QString)));
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
        if (iButton1 != 0)
            buttonDescriptions[iButton1] = strButtonText1;
        if (iButton2 != 0)
            buttonDescriptions[iButton2] = strButtonText2;
        if (fProposeAutoConfirmation)
            buttonDescriptions[AlertButton_Cancel | AlertOption_AutoConfirmed] = QString();
        /* Create new one: */
        pPopupStack->createPopupPane(strPopupPaneID, strMessage, strDetails, buttonDescriptions);
    }

    /* Show popup-stack: */
    showPopupStack(pParent);
}

void UIPopupCenter::hidePopupPane(QWidget *pParent, const QString &strPopupPaneID)
{
    /* Make sure parent is set! */
    AssertPtrReturnVoid(pParent);

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
    /* Remember auto-confirmation fact (if necessary): */
    if (iResultCode & AlertOption_AutoConfirmed)
        gEDataManager->setSuppressedMessages(gEDataManager->suppressedMessages() << strPopupPaneID);

    /* Notify listeners: */
    emit sigPopupPaneDone(strPopupPaneID, iResultCode);
}

void UIPopupCenter::sltRemovePopupStack(QString strPopupStackID)
{
    /* Make sure corresponding popup-stack *exists*: */
    if (!m_stacks.contains(strPopupStackID))
    {
        AssertMsgFailed(("Popup-stack already destroyed!\n"));
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
    AssertPtrReturn(pParent, QString());

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
    AssertPtrReturnVoid(pParent);

    /* Assign event-filter: */
    pParent->window()->installEventFilter(pPopupStack);

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
    AssertPtrReturnVoid(pParent);

    /* Unassign parent: */
    pPopupStack->setParent(0);

    /* Unassign event-filter: */
    pParent->window()->removeEventFilter(pPopupStack);
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
                                                     .arg(UIHostCombo::toReadableString(gEDataManager->hostKeyCombination())),
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
          QApplication::translate("UIMessageCenter", "<p>The virtual screen is currently set to a <b>%1&nbsp;bit</b> color mode. For better "
                                                     "performance please change this to <b>%2&nbsp;bit</b>. This can usually be done from the"
                                                     " <b>Display</b> section of the guest operating system's Control Panel or System Settings.</p>")
                                                     .arg(uRealBPP).arg(uWantedBPP),
          true);
}

void UIPopupCenter::forgetAboutWrongColorDepth(QWidget *pParent)
{
    recall(pParent, "remindAboutWrongColorDepth");
}

