/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class declaration
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

#ifndef __UIMachineWindowNormal_h__
#define __UIMachineWindowNormal_h__

/* Global includes */
#include <QLabel>

/* Local includes */
#include "VBoxDefs.h"
#include "QIWithRetranslateUI.h"
#include "QIMainDialog.h"
#include "UIMachineWindow.h"
#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif

/* Local forwards */
class UIIndicatorsPool;
class QIStateIndicator;

class UIMachineWindowNormal : public QIWithRetranslateUI<QIMainDialog>, public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Normal machine window constructor/destructor: */
    UIMachineWindowNormal(UIMachineLogic *pMachineLogic);
    virtual ~UIMachineWindowNormal();

private slots:

    /* Console callback handlers: */
    void sltMachineStateChanged(KMachineState machineState);
    void sltMediumChange(const CMediumAttachment &attachment);
    void sltUSBControllerChange();
    void sltUSBDeviceStateChange();
    void sltNetworkAdapterChange();
    void sltSharedFolderChange();

    /* Runtime menus: */
    void sltPrepareMenuMachine();
    void sltPrepareMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    void sltPrepareMenuDebug();
#endif

    /* LED connections: */
    void sltUpdateIndicators();
    void sltShowIndicatorsContextMenu(QIStateIndicator *pIndicator, QContextMenuEvent *pEvent);
    void sltProcessGlobalSettingChange(const char *aPublicName, const char *aName);

    /* Close window reimplementation: */
    void sltTryClose();

private:

    /* Translate routine: */
    void retranslateUi();

    /* Update routines: */
    void updateAppearanceOf(int aElement);

    /* Event handlers: */
    bool event(QEvent *pEvent);
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif
    void closeEvent(QCloseEvent *pEvent);

    /* Private getters: */
    UIIndicatorsPool* indicatorsPool() { return m_pIndicatorsPool; }

    /* Prepare helpers: */
    void prepareConsoleConnections();
    void prepareMenu();
    void prepareStatusBar();
    void prepareConnections();
    void loadWindowSettings();
    void prepareMachineView();

    /* Cleanup helpers: */
    void cleanupMachineView() {}
    void saveWindowSettings();
    void cleanupConnections() {}
    void cleanupStatusBar();
    void cleanupMenu() {}
    void cleanupConsoleConnections() {}

    /* Indicators pool: */
    UIIndicatorsPool *m_pIndicatorsPool;
    /* Other QWidgets: */
    QWidget *m_pCntHostkey;
    QLabel *m_pNameHostkey;
    /* Other QObjects: */
    QTimer *m_pIdleTimer;
    /* Other members: */
    QRect m_normalGeometry;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowNormal_h__

