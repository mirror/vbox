/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class declaration
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

#ifndef __UIMachineWindowSeamless_h__
#define __UIMachineWindowSeamless_h__

/* Local includes */
#include "UIMachineWindow.h"

/* Local forwards */
class VBoxMiniToolBar;

class UIMachineWindowSeamless : public UIMachineWindow
{
    Q_OBJECT;

public slots:

    void sltPlaceOnScreen();

protected:

    /* Seamless machine window constructor/destructor: */
    UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowSeamless();

private slots:

    /* Popup main menu: */
    void sltPopupMainMenu();

#ifndef RT_OS_DARWIN /* Something is *really* broken in regards of the moc here */
    /* Update mini tool-bar mask: */
    void sltUpdateMiniToolBarMask();
#endif /* RT_OS_DARWIN */

private:

    /* Event handlers: */
#ifdef Q_WS_MAC
    bool event(QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Prepare helpers: */
    void prepareSeamless();
    void prepareMenu();
#ifndef Q_WS_MAC
    void prepareMiniToolBar();
#endif /* Q_WS_MAC */
    void prepareMachineView();
#ifdef Q_WS_MAC
    void loadWindowSettings();
#endif /* Q_WS_MAC */

    /* Cleanup helpers: */
    void saveWindowSettings();
    void cleanupMachineView();
#ifndef Q_WS_MAC
    void cleanupMiniToolBar();
#endif /* Q_WS_MAC */
    void cleanupMenu();
    //void cleanupSeamless() {}

    /* Update routines: */
    void updateAppearanceOf(int iElement);

    /* Other members: */
    void showInNecessaryMode();
    void setMask(const QRegion &region);

    /* Private variables: */
    QMenu *m_pMainMenu;
#ifndef Q_WS_MAC
    VBoxMiniToolBar *m_pMiniToolBar;
#endif /* Q_WS_MAC */
#ifdef Q_WS_WIN
    QRegion m_prevRegion;
#endif /* Q_WS_WIN */

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowSeamless_h__

