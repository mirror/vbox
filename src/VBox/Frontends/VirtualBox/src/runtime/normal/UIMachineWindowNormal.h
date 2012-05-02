/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class declaration
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

#ifndef __UIMachineWindowNormal_h__
#define __UIMachineWindowNormal_h__

/* Global includes: */
#include <QLabel>

/* Local includes: */
#include "UIMachineWindow.h"

/* Forward declarations: */
class CMediumAttachment;
class UIIndicatorsPool;
class QIStateIndicator;

/* Normal machine-window implementation: */
class UIMachineWindowNormal : public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Constructor: */
    UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId);

private slots:

    /* Session event-handlers: */
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

private:

    /* Prepare helpers: */
    void prepareSessionConnections();
    void prepareMenu();
    void prepareStatusBar();
    void prepareVisualState();
    void prepareHandlers();
    void loadSettings();

    /* Cleanup helpers: */
    void saveSettings();
    //void cleanupHandlers() {}
    //coid cleanupVisualState() {}
    void cleanupStatusBar();
    //void cleanupMenu() {}
    //void cleanupConsoleConnections() {}

    /* Translate stuff: */
    void retranslateUi();

    /* Show stuff: */
    void showInNecessaryMode();

    /* Update stuff: */
    void updateAppearanceOf(int aElement);

    /* Event handler: */
    bool event(QEvent *pEvent);

    /* Helpers: */
    UIIndicatorsPool* indicatorsPool() { return m_pIndicatorsPool; }
    bool isMaximizedChecked();
    void updateIndicatorState(QIStateIndicator *pIndicator, KDeviceType deviceType);

    /* Widgets: */
    UIIndicatorsPool *m_pIndicatorsPool;
    QWidget *m_pCntHostkey;
    QLabel *m_pNameHostkey;

    /* Variables: */
    QTimer *m_pIdleTimer;
    QRect m_normalGeometry;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowNormal_h__

