/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class declaration
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

#ifndef __UIMachineWindowSeamless_h__
#define __UIMachineWindowSeamless_h__

/* Local includes: */
#include "UIMachineWindow.h"

/* Forward declarations: */
class VBoxMiniToolBar;

/* Seamless machine-window implementation: */
class UIMachineWindowSeamless : public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Constructor: */
    UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId);

private slots:

#ifndef Q_WS_MAC
    /* Session event-handlers: */
    void sltMachineStateChanged();
#endif /* !Q_WS_MAC */

    /* Show in necessary mode: */
    void sltShowInNecessaryMode() { showInNecessaryMode(); }

    /* Popup main menu: */
    void sltPopupMainMenu();

#ifndef RT_OS_DARWIN
    /* Current Qt on MAC has something broken in moc generation,
     * so we have to use RT_OS_DARWIN instead of Q_WS_MAC here. */
    /* Update mini tool-bar mask: */
    void sltUpdateMiniToolBarMask();
#endif /* !RT_OS_DARWIN */

private:

    /* Prepare helpers: */
    void prepareMenu();
    void prepareVisualState();
#ifndef Q_WS_MAC
    void prepareMiniToolbar();
#endif /* !Q_WS_MAC */

    /* Cleanup helpers: */
#ifndef Q_WS_MAC
    void cleanupMiniToolbar();
#endif /* !Q_WS_MAC */
    void cleanupVisualState();
    void cleanupMenu();

    /* Show stuff: */
    void placeOnScreen();
    void showInNecessaryMode();

#ifndef Q_WS_MAC
    /* Update routines: */
    void updateAppearanceOf(int iElement);
#endif /* !Q_WS_MAC */

#ifdef Q_WS_MAC
    /* Event handlers: */
    bool event(QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Helpers: */
    void setMask(const QRegion &region);

    /* Widgets: */
    QMenu *m_pMainMenu;
#ifndef Q_WS_MAC
    VBoxMiniToolBar *m_pMiniToolBar;
#endif /* !Q_WS_MAC */

    /* Variables: */
#ifdef Q_WS_WIN
    QRegion m_prevRegion;
#endif /* Q_WS_WIN */

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowSeamless_h__

