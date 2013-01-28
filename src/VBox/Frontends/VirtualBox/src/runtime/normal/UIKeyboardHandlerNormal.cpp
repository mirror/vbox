/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIKeyboardHandlerNormal class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QMainWindow>
#include <QMenuBar>
#include <QKeyEvent>

/* Local includes */
#include "UIKeyboardHandlerNormal.h"
#include "UIMachineWindow.h"
#include "UIShortcutPool.h"

/* Fullscreen keyboard-handler constructor: */
UIKeyboardHandlerNormal::UIKeyboardHandlerNormal(UIMachineLogic* pMachineLogic)
    : UIKeyboardHandler(pMachineLogic)
{
}

/* Fullscreen keyboard-handler destructor: */
UIKeyboardHandlerNormal::~UIKeyboardHandlerNormal()
{
}

/* Event handler for prepared listener(s): */
bool UIKeyboardHandlerNormal::eventFilter(QObject *pWatchedObject, QEvent *pEvent)
{
    /* Check if pWatchedObject object is view: */
    if (UIMachineView *pWatchedView = isItListenedView(pWatchedObject))
    {
        /* Get corresponding screen index: */
        ulong uScreenId = m_views.key(pWatchedView);
        NOREF(uScreenId);
        /* Handle view events: */
        switch (pEvent->type())
        {
#ifndef Q_WS_MAC
            /* We don't want this on the Mac, cause there the menu-bar isn't within the
             * window and popping up a menu there looks really ugly. */
            case QEvent::KeyPress:
            {
                /* Get key-event: */
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);
                /* Process Host+Home as menu-bar activator: */
                if (isHostKeyPressed() && pKeyEvent->key() == gShortcutPool->shortcut(GUI_Input_MachineShortcuts, QString("PopupMenu")).sequence())
                {
                    /* Trying to get menu-bar: */
                    QMenuBar *pMenuBar = qobject_cast<QMainWindow*>(m_windows[uScreenId])->menuBar();
                    /* If menu-bar is present and have actions: */
                    if (pMenuBar && !pMenuBar->actions().isEmpty())
                    {
                        /* If 'active' action is NOT chosen: */
                        if (!pMenuBar->activeAction())
                            /* Set first menu-bar action as 'active': */
                            pMenuBar->setActiveAction(pMenuBar->actions()[0]);
                        /* If 'active' action is chosen: */
                        if (pMenuBar->activeAction())
                        {
                            /* Activate 'active' menu-bar action: */
                            pMenuBar->activeAction()->activate(QAction::Trigger);
#ifdef Q_WS_WIN
                            /* Windows host needs separate 'focus set'
                             * to let menubar operate while popped up: */
                            pMenuBar->setFocus();
#endif /* Q_WS_WIN */
                            /* Filter-out this event: */
                            return true;
                        }
                    }
                }
                break;
            }
#endif /* !Q_WS_MAC */
            default:
                break;
        }
    }

    /* Else just propagate to base-class: */
    return UIKeyboardHandler::eventFilter(pWatchedObject, pEvent);
}

