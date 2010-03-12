/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIMachineWindowFullscreen_h__
#define __UIMachineWindowFullscreen_h__

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "QIMainDialog.h"
#include "UIMachineWindow.h"
#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif

/* Local forwards */
class VBoxMiniToolBar;

class UIMachineWindowFullscreen : public QIWithRetranslateUI2<QIMainDialog>, public UIMachineWindow
{
    Q_OBJECT;

public slots:

    void sltPlaceOnScreen();

protected:

    /* Fullscreen machine window constructor/destructor: */
    UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowFullscreen();

private slots:

    /* Console callback handlers: */
    void sltMachineStateChanged();

    /* Popup main menu: */
    void sltPopupMainMenu();

    /* Close window reimplementation: */
    void sltTryClose();

private:

    /* Translate routine: */
    void retranslateUi();

    /* Event handlers: */
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif /* Q_WS_X11 */
    void closeEvent(QCloseEvent *pEvent);

    /* Prepare helpers: */
    void prepareMenu();
    void prepareMiniToolBar();
    void prepareMachineView();
    //void loadWindowSettings() {}

    /* Cleanup helpers: */
    void saveWindowSettings();
    void cleanupMachineView();
    //void cleanupMiniToolBar() {}
    void cleanupMenu();

    /* Update routines: */
    void updateAppearanceOf(int iElement);

    /* Private variables: */
    QMenu *m_pMainMenu;
    VBoxMiniToolBar *m_pMiniToolBar;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowFullscreen_h__

