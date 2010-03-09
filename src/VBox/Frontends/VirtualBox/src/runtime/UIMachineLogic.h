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
#include "UIMachineDefs.h"
#include <QIWithRetranslateUI.h>
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/dbggui.h>
#endif

/* Global forwards */
class QAction;
class QActionGroup;

/* Local forwards */
class CSession;
class CMachine;
class CSnapshot;
class CUSBDevice;
class CVirtualBoxErrorInfo;
class UISession;
class UIActionsPool;
class UIMachineWindow;
class UIDockIconPreview;
class VBoxChangeDockIconUpdateEvent;

class UIMachineLogic : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

public:

    /* Factory function to create required logic sub-child: */
    static UIMachineLogic* create(QObject *pParent,
                                  UISession *pSession,
                                  UIActionsPool *pActionsPool,
                                  UIVisualStateType visualStateType);

    /* Check if this mode is available */
    virtual bool checkAvailability() { return true; }

    /* Do the real initialization of the object */
    virtual void initialize() {}

    /* Main getters/setters: */
    UISession* uisession() const { return m_pSession; }
    UIActionsPool* actionsPool() const { return m_pActionsPool; }
    UIVisualStateType visualStateType() const { return m_visualStateType; }
    QList<UIMachineWindow*>& machineWindows() { return m_machineWindowsList; }
    UIMachineWindow* mainMachineWindow();
    UIMachineWindow* defaultMachineWindow();

    /* Maintenance getters/setters: */
    bool isPreventAutoStart() const { return m_fIsPreventAutoStart; }
    bool isPreventAutoClose() const { return m_fIsPreventAutoClose; }
    void setPreventAutoStart(bool fIsPreventAutoStart) { m_fIsPreventAutoStart = fIsPreventAutoStart; }
    void setPreventAutoClose(bool fIsPreventAutoClose) { m_fIsPreventAutoClose = fIsPreventAutoClose; }

#ifdef Q_WS_MAC
    void updateDockIcon();
#endif /* Q_WS_MAC */

protected:

    /* Machine logic constructor/destructor: */
    UIMachineLogic(QObject *pParent,
                   UISession *pSession,
                   UIActionsPool *pActionsPool,
                   UIVisualStateType visualStateType);
    virtual ~UIMachineLogic();

    /* Protected getters/setters: */
    bool isMachineWindowsCreated() const { return m_fIsWindowsCreated; }
    void setMachineWindowsCreated(bool fIsWindowsCreated) { m_fIsWindowsCreated = fIsWindowsCreated; }

    /* Protected wrappers: */
    CSession& session();

    /* Protected members: */
    void addMachineWindow(UIMachineWindow *pMachineWindow);
    void retranslateUi();
#ifdef Q_WS_MAC
    bool isDockIconPreviewEnabled() const { return m_fIsDockIconEnabled; }
    void setDockIconPreviewEnabled(bool fIsDockIconPreviewEnabled) { m_fIsDockIconEnabled = fIsDockIconPreviewEnabled; }
    void updateDockOverlay();
#endif /* Q_WS_MAC */

    /* Prepare helpers: */
    virtual void prepareConsoleConnections();
    virtual void prepareActionGroups();
    virtual void prepareActionConnections();
#ifdef Q_WS_MAC
    virtual void prepareDock();
#endif /* Q_WS_MAC */
    virtual void prepareRequiredFeatures();
    virtual void prepareConsolePowerUp();

    /* Cleanup helpers: */
    //virtual void cleanupConsolePowerUp() {}
    //virtual void cleanupRequiredFeatures() {}
#ifdef Q_WS_MAC
    //virtual void cleanupDock() {}
#endif /* Q_WS_MAC */
    //virtual void cleanupActionConnections() {}
    //virtual void cleanupActionGroups() {}
    //virtual void cleanupConsoleConnections() {}

protected slots:

    /* Console callback handlers: */
    virtual void sltMachineStateChanged();
    virtual void sltAdditionsStateChanged();
    virtual void sltMouseCapabilityChanged();
    virtual void sltUSBDeviceStateChange(const CUSBDevice &device, bool fIsAttached, const CVirtualBoxErrorInfo &error);
    virtual void sltRuntimeError(bool fIsFatal, const QString &strErrorId, const QString &strMessage);

private slots:

    /* "Machine" menu funtionality */
    void sltToggleGuestAutoresize(bool fEnabled);
    void sltAdjustWindow();
    void sltToggleMouseIntegration(bool fDisabled);
    void sltTypeCAD();
#ifdef Q_WS_X11
    void sltTypeCABS();
#endif
    void sltTakeSnapshot();
    void sltShowInformationDialog();
    void sltReset();
    void sltPause(bool fOn);
    void sltACPIShutdown();
    void sltClose();

    /* "Device" menu funtionality */
    void sltPrepareStorageMenu();
    void sltMountStorageMedium();
    void sltPrepareUSBMenu();
    void sltAttachUSBDevice();
    void sltOpenNetworkAdaptersDialog();
    void sltOpenSharedFoldersDialog();
    void sltSwitchVrdp(bool fOn);
    void sltInstallGuestAdditions();

#ifdef VBOX_WITH_DEBUGGER_GUI
    void sltPrepareDebugMenu();
    void sltShowDebugStatistics();
    void sltShowDebugCommandLine();
    void sltLoggingToggled(bool);
#endif

#ifdef RT_OS_DARWIN /* Something is *really* broken in regards of the moc here */
    void sltDockPreviewModeChanged(QAction *pAction);
    void sltDockPreviewMonitorChanged(QAction *pAction);
    void sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event);
#endif /* RT_OS_DARWIN */

private:

    /* Utility functions: */
    void installGuestAdditionsFrom(const QString &strSource);
    static int searchMaxSnapshotIndex(const CMachine &machine,
                                      const CSnapshot &snapshot,
                                      const QString &strNameTemplate);

    /* Private variables: */
    UISession *m_pSession;
    UIActionsPool *m_pActionsPool;
    UIVisualStateType m_visualStateType;
    QList<UIMachineWindow*> m_machineWindowsList;

    QActionGroup *m_pRunningActions;
    QActionGroup *m_pRunningOrPausedActions;

    bool m_fIsWindowsCreated : 1;
    bool m_fIsPreventAutoStart : 1;
    bool m_fIsPreventAutoClose : 1;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debugger functionality: */
    bool dbgCreated();
    void dbgDestroy();
    void dbgAdjustRelativePos();
    /* The handle to the debugger gui: */
    PDBGGUI m_dbgGui;
    /* The virtual method table for the debugger GUI: */
    PCDBGGUIVT m_dbgGuiVT;
#endif

#ifdef Q_WS_MAC
    bool m_fIsDockIconEnabled;
    UIDockIconPreview *m_pDockIconPreview;
    QActionGroup *m_pDockPreviewSelectMonitorGroup;
    int m_DockIconPreviewMonitor;
#endif /* Q_WS_MAC */

    /* Friend classes: */
    friend class UIMachineWindow;

#if 0 // TODO: Where to move that?
    void setViewInSeamlessMode(const QRect &aTargetRect);
#endif
};

#endif // __UIMachineLogic_h__

