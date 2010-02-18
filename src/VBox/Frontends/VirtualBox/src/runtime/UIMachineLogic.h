/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogic class declaration
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

#ifndef __UIMachineLogic_h__
#define __UIMachineLogic_h__

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/dbggui.h>
#endif

/* Global forwards */
class QActionGroup;

/* Local forwards */
class UIActionsPool;
class UIMachineWindow;

class UIMachineLogic : public QObject
{
    Q_OBJECT;

public:

    /* Factory function to create required logic sub-child: */
    static UIMachineLogic* create(QObject *pParent,
                                  const CSession &session,
                                  UIActionsPool *pActionsPool,
                                  UIVisualStateType visualStateType);

    /* Public getters: */
    CSession& session() { return m_session; }
    UIActionsPool* actionsPool() { return m_pActionsPool; }
    KMachineState machineState() const { return m_machineState; }
    UIVisualStateType visualStateType() const { return m_visualStateType; }
    bool isPaused() const { return m_machineState == KMachineState_Paused || m_machineState == KMachineState_TeleportingPausedVM; }

    /* Public setters: */
    bool pause(bool bPaused);

protected:

    /* Common machine logic constructor: */
    UIMachineLogic(QObject *pParent,
                   const CSession &session,
                   UIActionsPool *pActionsPool,
                   UIVisualStateType visualStateType);
    /* Common machine logic destructor: */
    virtual ~UIMachineLogic();

    /* Update routines: */
    virtual void updateAppearanceOf(int iElement);

    /* Protected getters: */
    UIMachineWindow* machineWindowWrapper() { return m_pMachineWindowContainer; }
    bool isFirstTimeStarted() const { return m_bIsFirstTimeStarted; }
    bool isPreventAutoClose() const { return m_bIsPreventAutoClose; }

    /* Pretected setters: */
    void setMachineState(KMachineState state) { m_machineState = state; }
    void setPreventAutoClose(bool bIsPreventAutoClose) { m_bIsPreventAutoClose = bIsPreventAutoClose; }
    void setOpenViewFinished(bool bIsOpenViewFinished) { m_bIsOpenViewFinished = bIsOpenViewFinished; }

    /* Protected variables: */
    UIMachineWindow *m_pMachineWindowContainer;

private slots:

    /* "Machine" menu funtionality */
    void sltAdjustWindow();
    void sltToggleMouseIntegration(bool bOff);
    void sltTypeCAD();
#ifdef Q_WS_X11
    void sltTypeCABS();
#endif
    void sltTakeSnapshot();
    void sltShowInformationDialog();
    void sltReset();
    void sltPause(bool aOn);
    void sltACPIShutdown();
    void sltClose();

    /* "Device" menu funtionality */
    void sltPrepareStorageMenu();
    void sltMountStorageMedium();
    void sltOpenNetworkAdaptersDialog();
    void sltOpenSharedFoldersDialog();
    void sltPrepareUSBMenu();
    void sltAttachUSBDevice();
    void sltSwitchVrdp(bool bOn);
    void sltInstallGuestAdditions();

#ifdef VBOX_WITH_DEBUGGER_GUI
    void sltPrepareDebugMenu();
    void sltShowDebugStatistics();
    void sltShowDebugCommandLine();
    void sltLoggingToggled(bool);
#endif

    /* Machine state change handler */
    void sltUpdateMachineState(KMachineState machineState);
    /* Guest Additions state change handler */
    void sltUpdateAdditionsState(const QString &strVersion, bool bActive, bool bSeamlessSupported, bool bGraphicsSupported);
    /* Mouse Integration state change handler */
    void sltUpdateMouseState(int iState);

private:

    /* Prepare helpers: */
    void prepareActionGroups();
    void prepareActionConnections();
    void prepareRequiredFeatures();
    void loadLogicSettings();

    /* Cleanup helpers: */
    void saveLogicSettings();
    //void cleanupRequiredFeatures();
    //void cleanupActionConnections();
    //void cleanupActionGroups();

    /* Utility functions: */
    void installGuestAdditionsFrom(const QString &strSource);
    static int searchMaxSnapshotIndex(const CMachine &machine, const CSnapshot &snapshot, const QString &strNameTemplate);
#ifdef VBOX_WITH_DEBUGGER_GUI
    bool dbgCreated();
    void dbgDestroy();
    void dbgAdjustRelativePos();
#endif

#if 0 // TODO: Where to move that?
# ifdef Q_WS_MAC
    void fadeToBlack();
    void fadeToNormal();
# endif
    bool toggleFullscreenMode(bool aOn, bool aSeamless);
    void switchToFullscreen(bool aOn, bool aSeamless);
    void setViewInSeamlessMode(const QRect &aTargetRect);
#endif

    /* Private variables: */
    CSession m_session;
    UIActionsPool *m_pActionsPool;
    KMachineState m_machineState;
    UIVisualStateType m_visualStateType;

    QActionGroup *m_pRunningActions;
    QActionGroup *m_pRunningOrPausedActions;

    bool m_bIsFirstTimeStarted : 1;
    bool m_bIsOpenViewFinished : 1;
    bool m_bIsGraphicsSupported : 1;
    bool m_bIsSeamlessSupported : 1;
    bool m_bIsAutoSaveMedia : 1;
    bool m_bIsPreventAutoClose : 1;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* The handle to the debugger gui: */
    PDBGGUI m_dbgGui;
    /* The virtual method table for the debugger GUI: */
    PCDBGGUIVT m_dbgGuiVT;
#endif
};

#endif // __UIMachineLogic_h__
