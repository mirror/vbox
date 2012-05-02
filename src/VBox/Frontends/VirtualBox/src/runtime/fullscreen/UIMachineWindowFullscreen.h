/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class declaration
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

#ifndef __UIMachineWindowFullscreen_h__
#define __UIMachineWindowFullscreen_h__

/* Local includes */
#include "UIMachineWindow.h"

/* Local forwards */
class VBoxMiniToolBar;

class UIMachineWindowFullscreen : public UIMachineWindow
{
    Q_OBJECT;

public slots:

    void sltPlaceOnScreen();

protected:

    /* Fullscreen machine window constructor/destructor: */
    UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowFullscreen();

private slots:

    /* Popup main menu: */
    void sltPopupMainMenu();

private:

    /* Prepare helpers: */
    void prepareMenu();
    void prepareMiniToolBar();
    void prepareMachineView();
    //void loadWindowSettings() {}

    /* Cleanup helpers: */
    void saveWindowSettings();
    void cleanupMachineView();
    void cleanupMiniToolBar();
    void cleanupMenu();

    /* Update routines: */
    void updateAppearanceOf(int iElement);

    /* Other members: */
    void showInNecessaryMode();

    /* Private variables: */
    QMenu *m_pMainMenu;
    VBoxMiniToolBar *m_pMiniToolBar;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowFullscreen_h__

