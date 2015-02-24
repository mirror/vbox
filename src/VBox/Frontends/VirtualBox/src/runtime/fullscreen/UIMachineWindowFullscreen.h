/** @file
 * VBox Qt GUI - UIMachineWindowFullscreen class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineWindowFullscreen_h__
#define __UIMachineWindowFullscreen_h__

/* Local includes: */
#include "UIMachineWindow.h"

#ifndef Q_WS_MAC
/* Forward declarations: */
class UIRuntimeMiniToolBar;
#endif /* !Q_WS_MAC */

/* Fullscreen machine-window implementation: */
class UIMachineWindowFullscreen : public UIMachineWindow
{
    Q_OBJECT;

#ifdef RT_OS_DARWIN
signals:
    /** Mac OS X: Notifies listener about native 'fullscreen' will be entered. */
    void sigNotifyAboutNativeFullscreenWillEnter();
    /** Mac OS X: Notifies listener about native 'fullscreen' entered. */
    void sigNotifyAboutNativeFullscreenDidEnter();
    /** Mac OS X: Notifies listener about native 'fullscreen' will be exited. */
    void sigNotifyAboutNativeFullscreenWillExit();
    /** Mac OS X: Notifies listener about native 'fullscreen' exited. */
    void sigNotifyAboutNativeFullscreenDidExit();
    /** Mac OS X: Notifies listener about native 'fullscreen' fail to enter. */
    void sigNotifyAboutNativeFullscreenFailToEnter();
#endif /* RT_OS_DARWIN */

protected:

    /* Constructor: */
    UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId);

#ifdef Q_WS_MAC
    /** Mac OS X: Handles native notifications @a strNativeNotificationName for 'fullscreen' window. */
    void handleNativeNotification(const QString &strNativeNotificationName);
    /** Mac OS X: Returns whether window is in 'fullscreen' transition. */
    bool isInFullscreenTransition() const { return m_fIsInFullscreenTransition; }
#endif /* Q_WS_MAC */

private slots:

#ifndef Q_WS_MAC
    /* Session event-handlers: */
    void sltMachineStateChanged();
#endif /* !Q_WS_MAC */

#ifdef RT_OS_DARWIN
    /** Mac OS X: Commands @a pMachineWindow to enter native 'fullscreen' mode if possible. */
    void sltEnterNativeFullscreen(UIMachineWindow *pMachineWindow);
    /** Mac OS X: Commands @a pMachineWindow to exit native 'fullscreen' mode if possible. */
    void sltExitNativeFullscreen(UIMachineWindow *pMachineWindow);
#endif /* RT_OS_DARWIN */

    /** Revokes keyboard-focus. */
    void sltRevokeFocus();

private:

    /* Prepare helpers: */
    void prepareVisualState();
#ifndef Q_WS_MAC
    void prepareMiniToolbar();
#endif /* !Q_WS_MAC */

    /* Cleanup helpers: */
#ifndef Q_WS_MAC
    void cleanupMiniToolbar();
#endif /* !Q_WS_MAC */
    void cleanupVisualState();

    /* Show stuff: */
    void placeOnScreen();
    void showInNecessaryMode();

    /** Adjusts machine-view size to correspond current machine-window size. */
    virtual void adjustMachineViewSize();

#ifndef Q_WS_MAC
    /* Update routines: */
    void updateAppearanceOf(int iElement);
#endif /* !Q_WS_MAC */

#ifndef Q_WS_MAC
    /** Holds the mini-toolbar instance. */
    UIRuntimeMiniToolBar *m_pMiniToolBar;
#endif /* !Q_WS_MAC */

#ifdef Q_WS_MAC
    /** Mac OS X: Reflects whether window is in 'fullscreen' transition. */
    bool m_fIsInFullscreenTransition;
    /** Mac OS X: Allows 'fullscreen' API access: */
    friend class UIMachineLogicFullscreen;
#endif /* Q_WS_MAC */

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowFullscreen_h__

