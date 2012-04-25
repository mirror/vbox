/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowScale class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineWindowScale_h__
#define __UIMachineWindowScale_h__

/* Global includes */
#include <QMainWindow>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "UIMachineWindow.h"

class UIMachineWindowScale : public QIWithRetranslateUI2<QMainWindow>, public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Scale machine window constructor/destructor: */
    UIMachineWindowScale(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowScale();

private slots:

    /* Console callback handlers: */
    void sltMachineStateChanged();
    void sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);

    /* Popup main menu: */
    void sltPopupMainMenu();

    /* Close window reimplementation: */
    void sltTryClose();

private:

    /* Translate routine: */
    void retranslateUi();

    /* Event handlers: */
    bool event(QEvent *pEvent);
#ifdef Q_WS_WIN
    bool winEvent(MSG *pMessage, long *pResult);
#endif
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif
    void closeEvent(QCloseEvent *pEvent);

    /* Prepare helpers: */
    void prepareMenu();
    void prepareMachineViewContainer();
    void prepareMachineView();
    void loadWindowSettings();

    /* Cleanup helpers: */
    void saveWindowSettings();
    void cleanupMachineView();
    //void cleanupMachineViewContainer() {}
    void cleanupMenu();

    /* Other members: */
    void showInNecessaryMode();
    bool isMaximizedChecked();

    /* Other members: */
    QMenu *m_pMainMenu;
    QRect m_normalGeometry;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowScale_h__

