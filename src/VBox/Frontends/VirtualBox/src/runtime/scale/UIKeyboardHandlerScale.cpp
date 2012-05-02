/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIKeyboardHandlerScale class implementation
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
#include <QKeyEvent>
#include <QTimer>
#include <QWidget>

/* Local includes */
#include "UIKeyboardHandlerScale.h"
#include "UIMachineWindow.h"
#include "UIMachineShortcuts.h"

/* Fullscreen keyboard-handler constructor: */
UIKeyboardHandlerScale::UIKeyboardHandlerScale(UIMachineLogic* pMachineLogic)
    : UIKeyboardHandler(pMachineLogic)
{
}

/* Fullscreen keyboard-handler destructor: */
UIKeyboardHandlerScale::~UIKeyboardHandlerScale()
{
}

/* Event handler for prepared listener(s): */
bool UIKeyboardHandlerScale::eventFilter(QObject *pWatchedObject, QEvent *pEvent)
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
            /* We don't want this on the Mac, cause there the menu bar isn't within the window
             * and popping up a menu there looks really ugly. */
            case QEvent::KeyPress:
            {
                /* Get key-event: */
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);
                /* Process Host+Home for menu popup: */
                if (isHostKeyPressed() && pKeyEvent->key() == gMS->keySequence(UIMachineShortcuts::PopupMenuShortcut))
                {
                    /* Post request to show popup-menu: */
                    QTimer::singleShot(0, m_windows[uScreenId], SLOT(sltPopupMainMenu()));
                    /* Filter-out this event: */
                    return true;
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

