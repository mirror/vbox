/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineWindowSeamless class declaration.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineWindowSeamless_h___
#define ___UIMachineWindowSeamless_h___

/* GUI includes: */
#include "UIMachineWindow.h"

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
/* Forward declarations: */
class UIMiniToolBar;
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

/** UIMachineWindow reimplementation,
  * providing GUI with machine-window for the seamless mode. */
class UIMachineWindowSeamless : public UIMachineWindow
{
    Q_OBJECT;

protected:

    /** Constructor, passes @a pMachineLogic and @a uScreenId to the UIMachineWindow constructor. */
    UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId);

private slots:

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /** Handles machine state change event. */
    void sltMachineStateChanged();

    /** Revokes window activation. */
    void sltRevokeWindowActivation();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

private:

    /** Prepare visual-state routine. */
    void prepareVisualState();
#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /** Prepare mini-toolbar routine. */
    void prepareMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /** Cleanup mini-toolbar routine. */
    void cleanupMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */
    /** Cleanup visual-state routine. */
    void cleanupVisualState();

    /** Updates geometry according to visual-state. */
    void placeOnScreen();
    /** Updates visibility according to visual-state. */
    void showInNecessaryMode();

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /** Common update routine. */
    void updateAppearanceOf(int iElement);
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_WIN
# if QT_VERSION >= 0x050000
    /** Win: Handles show @a pEvent. */
    void showEvent(QShowEvent *pEvent);
# endif /* QT_VERSION >= 0x050000 */
#endif /* VBOX_WS_WIN */

#ifdef VBOX_WITH_MASKED_SEAMLESS
    /** Assigns guest seamless mask. */
    void setMask(const QRegion &maskGuest);
#endif /* VBOX_WITH_MASKED_SEAMLESS */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /** Holds the mini-toolbar instance. */
    UIMiniToolBar *m_pMiniToolBar;
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WITH_MASKED_SEAMLESS
    /** Holds the full seamless mask. */
    QRegion m_maskFull;
    /** Holds the guest seamless mask. */
    QRegion m_maskGuest;
#endif /* VBOX_WITH_MASKED_SEAMLESS */

    /** Holds whether the window was minimized before became hidden.
      * Used to restore minimized state when the window shown again. */
    bool m_fWasMinimized;

    /** Factory support. */
    friend class UIMachineWindow;
};

#endif /* !___UIMachineWindowSeamless_h___ */

