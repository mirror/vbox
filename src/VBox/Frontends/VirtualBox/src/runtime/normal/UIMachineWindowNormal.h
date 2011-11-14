/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class declaration
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

#ifndef __UIMachineWindowNormal_h__
#define __UIMachineWindowNormal_h__

/* Global includes */
#include <QMainWindow>
#include <QLabel>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "UIMachineWindow.h"
#include "COMDefs.h"

/* Local forwards */
class CMediumAttachment;
class UIIndicatorsPool;
class QIStateIndicator;

class UIMachineWindowNormal : public QIWithRetranslateUI2<QMainWindow>, public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Normal machine window constructor/destructor: */
    UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindowNormal();

private slots:

    /* Console callback handlers: */
    void sltMachineStateChanged();
    void sltMediumChange(const CMediumAttachment &attachment);
    void sltUSBControllerChange();
    void sltUSBDeviceStateChange();
    void sltNetworkAdapterChange();
    void sltSharedFolderChange();
    void sltCPUExecutionCapChange();

    /* LED connections: */
    void sltUpdateIndicators();
    void sltShowIndicatorsContextMenu(QIStateIndicator *pIndicator, QContextMenuEvent *pEvent);
    void sltProcessGlobalSettingChange(const char *aPublicName, const char *aName);

    /* Close window reimplementation: */
    void sltTryClose();

    /* Downloader listeners: */
    void sltEmbedDownloaderForAdditions();
    void sltEmbedDownloaderForUserManual();
    void sltEmbedDownloaderForExtensionPack();

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
    void prepareMachineView();
    void loadWindowSettings();

    /* Cleanup helpers: */
    void saveWindowSettings();
    void cleanupMachineView();
    //void cleanupConnections() {}
    void cleanupStatusBar();
    //void cleanupMenu() {}
    //void cleanupConsoleConnections() {}

    /* Other members: */
    void showSimple();
    bool isMaximizedChecked();
    void updateIndicatorState(QIStateIndicator *pIndicator, KDeviceType deviceType);

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

