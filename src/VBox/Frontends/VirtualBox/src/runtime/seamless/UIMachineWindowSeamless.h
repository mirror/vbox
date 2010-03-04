/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class declaration
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

#ifndef __UIMachineWindowSeamless_h__
#define __UIMachineWindowSeamless_h__

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "QIMainDialog.h"
#include "UIMachineWindow.h"
#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif

class UIMachineWindowSeamless : public QIWithRetranslateUI2<QIMainDialog>, public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Seamless machine window constructor/destructor: */
    UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowSeamless();

private slots:

    /* Console callback handlers: */
    void sltMachineStateChanged();

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
#ifdef Q_WS_MAC
    bool event(QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Prepare helpers: */
    void prepareSeamless();
#ifdef Q_WS_MAC
    void prepareMenu();
#endif /* Q_WS_MAC */
    void prepareMachineView();
#ifdef Q_WS_MAC
    void loadWindowSettings();
#endif /* Q_WS_MAC */

    /* Cleanup helpers: */
#ifdef Q_WS_MAC
    //void saveWindowSettings() {}
#endif /* Q_WS_MAC */
    void cleanupMachineView();
#ifdef Q_WS_MAC
    //void cleanupMenu() {}
#endif /* Q_WS_MAC */
    //void cleanupSeamless() {}

    /* Other members: */
    void setMask(const QRegion &region);
    void clearMask();

    /* Private variables: */
#ifdef Q_WS_WIN
    QRegion m_prevRegion;
#endif /* Q_WS_WIN */

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowSeamless_h__

