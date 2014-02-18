/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class declaration
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

/* Forward declarations: */
class UIRuntimeMiniToolBar;

/* Fullscreen machine-window implementation: */
class UIMachineWindowFullscreen : public UIMachineWindow
{
    Q_OBJECT;

#ifdef RT_OS_DARWIN
signals:
    /** Mac OS X: Notifies listener about native fullscreen entering. */
    void sigNotifyAboutNativeFullscreenDidEnter();
    /** Mac OS X: Notifies listener about native fullscreen exiting. */
    void sigNotifyAboutNativeFullscreenDidExit();
#endif /* RT_OS_DARWIN */

protected:

    /* Constructor: */
    UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId);

#ifdef Q_WS_MAC
    /** Mac OS X: Handles native notifications @a strNativeNotificationName for fullscreen window. */
    void handleNativeNotification(const QString &strNativeNotificationName);
#endif /* Q_WS_MAC */

private slots:

#ifdef RT_OS_DARWIN
    /** Mac OS X: Toggles native fullscreen mode. */
    void sltToggleNativeFullscreenMode();
#endif /* RT_OS_DARWIN */

    /* Session event-handlers: */
    void sltMachineStateChanged();

    /* Show in necessary mode: */
    void sltShowInNecessaryMode() { showInNecessaryMode(); }

    /* Popup main-menu: */
    void sltPopupMainMenu();

private:

    /* Prepare helpers: */
    void prepareMenu();
    void prepareVisualState();
    void prepareMiniToolbar();

    /* Cleanup helpers: */
    void cleanupMiniToolbar();
    void cleanupVisualState();
    void cleanupMenu();

    /* Show stuff: */
    void placeOnScreen();
    void showInNecessaryMode();

    /* Update stuff: */
    void updateAppearanceOf(int iElement);

    /* Widgets: */
    QMenu *m_pMainMenu;
    UIRuntimeMiniToolBar *m_pMiniToolBar;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowFullscreen_h__

