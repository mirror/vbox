/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogic class implementation
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

/* Global includes */
#include <QDir>
#include <QFileInfo>
#include <QDesktopWidget>
#include <QTimer>

/* Local includes */
#include "COMDefs.h"
#include "VBoxProblemReporter.h"

#include "VBoxMediaManagerDlg.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxVMInformationDlg.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsSF.h"
#include "UIDownloaderAdditions.h"
#ifdef Q_WS_MAC
# include "DockIconPreview.h"
#endif /* Q_WS_MAC */

#include "QIFileDialog.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"

#include <iprt/path.h>
#include <VBox/VMMDev.h>

#ifdef VBOX_WITH_DEBUGGER_GUI
# include <iprt/ldr.h>
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_X11
# include <XKeyboard.h>
# include <QX11Info>
#endif /* Q_WS_X11 */

struct MediumTarget
{
    MediumTarget() : name(QString("")), port(0), device(0), id(QString()), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice)
        : name(strName), port(iPort), device(iDevice), id(QString()), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice, const QString &strId)
        : name(strName), port(iPort), device(iDevice), id(strId), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice, VBoxDefs::MediumType eType)
        : name(strName), port(iPort), device(iDevice), id(QString()), type(eType) {}
    QString name;
    LONG port;
    LONG device;
    QString id;
    VBoxDefs::MediumType type;
};
Q_DECLARE_METATYPE(MediumTarget);

struct USBTarget
{
    USBTarget() : attach(false), id(QString()) {}
    USBTarget(bool fAttach, const QString &strId)
        : attach(fAttach), id(strId) {}
    bool attach;
    QString id;
};
Q_DECLARE_METATYPE(USBTarget);

class UINetworkAdaptersDialog : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    UINetworkAdaptersDialog(QWidget *pParent, CSession &session)
        : QIWithRetranslateUI<QDialog>(pParent)
        , m_pSettings(0)
        , m_session(session)
    {
        /* Setup Dialog's options */
        setModal(true);
        setWindowIcon(QIcon(":/nw_16px.png"));
        setSizeGripEnabled(true);

        /* Setup main dialog's layout */
        QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        VBoxGlobal::setLayoutMargin(pMainLayout, 10);
        pMainLayout->setSpacing(10);

        /* Setup settings layout */
        m_pSettings = new VBoxVMSettingsNetworkPage(true);
        m_pSettings->setOrderAfter(this);
        VBoxGlobal::setLayoutMargin(m_pSettings->layout(), 0);
        m_pSettings->getFrom(m_session.GetMachine());
        pMainLayout->addWidget(m_pSettings);

        /* Setup button's layout */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);

        connect(pButtonBox, SIGNAL(helpRequested()), &vboxProblem(), SLOT(showHelpHelpDialog()));
        connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
        pMainLayout->addWidget(pButtonBox);

        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setWindowTitle(QApplication::translate("VBoxNetworkDialog", "Network Adapters"));
    }

protected slots:

    virtual void accept()
    {
        m_pSettings->putBackTo();
        CMachine machine = m_session.GetMachine();
        machine.SaveSettings();
        if (!machine.isOk())
            vboxProblem().cannotSaveMachineSettings(machine);
        QDialog::accept();
    }

protected:

    void showEvent(QShowEvent *pEvent)
    {
        resize(450, 300);
        VBoxGlobal::centerWidget(this, parentWidget());
        setMinimumWidth(400);
        QDialog::showEvent(pEvent);
    }

private:

    VBoxSettingsPage *m_pSettings;
    CSession &m_session;
};

class UISharedFoldersDialog : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    UISharedFoldersDialog(QWidget *pParent, CSession &session)
        : QIWithRetranslateUI<QDialog>(pParent)
        , m_pSettings(0)
        , m_session(session)
    {
        /* Setup Dialog's options */
        setModal(true);
        setWindowIcon(QIcon(":/select_file_16px.png"));
        setSizeGripEnabled(true);

        /* Setup main dialog's layout */
        QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        VBoxGlobal::setLayoutMargin(pMainLayout, 10);
        pMainLayout->setSpacing(10);

        /* Setup settings layout */
        m_pSettings = new VBoxVMSettingsSF(MachineType | ConsoleType, this);
        VBoxGlobal::setLayoutMargin(m_pSettings->layout(), 0);
        m_pSettings->getFromConsole(m_session.GetConsole());
        m_pSettings->getFromMachine(m_session.GetMachine());
        pMainLayout->addWidget(m_pSettings);

        /* Setup button's layout */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);

        connect(pButtonBox, SIGNAL(helpRequested()), &vboxProblem(), SLOT(showHelpHelpDialog()));
        connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
        pMainLayout->addWidget(pButtonBox);

        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setWindowTitle(QApplication::translate("VBoxSFDialog", "Shared Folders"));
    }

protected slots:

    virtual void accept()
    {
        m_pSettings->putBackToConsole();
        m_pSettings->putBackToMachine();
        CMachine machine = m_session.GetMachine();
        machine.SaveSettings();
        if (!machine.isOk())
            vboxProblem().cannotSaveMachineSettings(machine);
        QDialog::accept();
    }

protected:

    void showEvent (QShowEvent *aEvent)
    {
        resize(450, 300);
        VBoxGlobal::centerWidget(this, parentWidget());
        setMinimumWidth(400);
        QDialog::showEvent(aEvent);
    }

private:

    VBoxVMSettingsSF *m_pSettings;
    CSession &m_session;
};

UIMachineLogic* UIMachineLogic::create(QObject *pParent,
                                       UISession *pSession,
                                       UIActionsPool *pActionsPool,
                                       UIVisualStateType visualStateType)
{
    UIMachineLogic *logic = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            logic = new UIMachineLogicNormal(pParent, pSession, pActionsPool);
            break;
        case UIVisualStateType_Fullscreen:
            logic = new UIMachineLogicFullscreen(pParent, pSession, pActionsPool);
            break;
        case UIVisualStateType_Seamless:
            logic = new UIMachineLogicSeamless(pParent, pSession, pActionsPool);
            break;
    }
    return logic;
}

bool UIMachineLogic::checkAvailability()
{
    return true;
}

UIMachineWindow* UIMachineLogic::mainMachineWindow() const
{
    /* Return null if windows are not created yet: */
    if (!isMachineWindowsCreated())
        return 0;

    return machineWindows()[0];
}

UIMachineWindow* UIMachineLogic::defaultMachineWindow() const
{
    /* Return null if windows are not created yet: */
    if (!isMachineWindowsCreated())
        return 0;

    /* Select main machine window by default: */
    UIMachineWindow *pWindowToPropose = mainMachineWindow();

    /* Check if there is active window present: */
    foreach (UIMachineWindow *pWindowToCheck, machineWindows())
    {
        if (pWindowToCheck->machineWindow()->isActiveWindow())
        {
            pWindowToPropose = pWindowToCheck;
            break;
        }
    }

    /* Return default machine window: */
    return pWindowToPropose;
}

#ifdef Q_WS_MAC
void UIMachineLogic::updateDockIcon()
{
    if (!isMachineWindowsCreated())
        return;

    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview)
        if(UIMachineView *pView = machineWindows().at(m_DockIconPreviewMonitor)->machineView())
            if (CGImageRef image = pView->vmContentImage())
            {
                m_pDockIconPreview->updateDockPreview(image);
                CGImageRelease(image);
            }
}

void UIMachineLogic::updateDockIconSize(int screenId, int width, int height)
{
    if (!isMachineWindowsCreated())
        return;

    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview
        && m_DockIconPreviewMonitor == screenId)
        m_pDockIconPreview->setOriginalSize(width, height);
}

UIMachineView* UIMachineLogic::dockPreviewView() const
{
    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview)
        return machineWindows().at(m_DockIconPreviewMonitor)->machineView();
    return 0;
}
#endif /* Q_WS_MAC */

UIMachineLogic::UIMachineLogic(QObject *pParent,
                               UISession *pSession,
                               UIActionsPool *pActionsPool,
                               UIVisualStateType visualStateType)
    : QIWithRetranslateUI3<QObject>(pParent)
    , m_pSession(pSession)
    , m_pActionsPool(pActionsPool)
    , m_visualStateType(visualStateType)
    , m_pRunningActions(0)
    , m_pRunningOrPausedActions(0)
    , m_fIsWindowsCreated(false)
    , m_fIsPreventAutoClose(false)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , m_pDbgGui(0)
    , m_pDbgGuiVT(0)
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef Q_WS_MAC
    , m_fIsDockIconEnabled(true)
    , m_pDockIconPreview(0)
    , m_pDockPreviewSelectMonitorGroup(0)
    , m_DockIconPreviewMonitor(0)
#endif /* Q_WS_MAC */
{
}

UIMachineLogic::~UIMachineLogic()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Close debugger: */
    dbgDestroy();
#endif /* VBOX_WITH_DEBUGGER_GUI */
}

CSession& UIMachineLogic::session()
{
    return uisession()->session();
}

void UIMachineLogic::addMachineWindow(UIMachineWindow *pMachineWindow)
{
    m_machineWindowsList << pMachineWindow;
}

void UIMachineLogic::retranslateUi()
{
#ifdef Q_WS_MAC
    if (m_pDockPreviewSelectMonitorGroup)
    {
        const QList<QAction*> &actions = m_pDockPreviewSelectMonitorGroup->actions();
        for (int i = 0; i < actions.size(); ++i)
        {
            QAction *pAction = actions.at(i);
            pAction->setText(QApplication::translate("UIMachineLogic", "Preview Monitor %1").arg(pAction->data().toInt() + 1));
        }
    }
#endif /* Q_WS_MAC */
}

#ifdef Q_WS_MAC
void UIMachineLogic::updateDockOverlay()
{
    /* Only to an update to the realtime preview if this is enabled by the user
     * & we are in an state where the framebuffer is likely valid. Otherwise to
     * the overlay stuff only. */
    KMachineState state = uisession()->machineState();
    if (m_fIsDockIconEnabled &&
        (state == KMachineState_Running ||
         state == KMachineState_Paused ||
         state == KMachineState_Teleporting ||
         state == KMachineState_LiveSnapshotting ||
         state == KMachineState_Restoring ||
         state == KMachineState_TeleportingPausedVM ||
         state == KMachineState_TeleportingIn ||
         state == KMachineState_Saving ||
         state == KMachineState_DeletingSnapshotOnline ||
         state == KMachineState_DeletingSnapshotPaused))
        updateDockIcon();
    else if (m_pDockIconPreview)
        m_pDockIconPreview->updateDockOverlay();
}
#endif /* Q_WS_MAC */

void UIMachineLogic::prepareSessionConnections()
{
    /* Machine power-up notifier: */
    connect(uisession(), SIGNAL(sigMachineStarted()), this, SLOT(sltCheckRequestedModes()));

    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));

    /* Mouse capability state-change updater: */
    connect(uisession(), SIGNAL(sigMouseCapabilityChange()), this, SLOT(sltMouseCapabilityChanged()));

    /* USB devices state-change updater: */
    connect(uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)));

    /* Runtime errors notifier: */
    connect(uisession(), SIGNAL(sigRuntimeError(bool, const QString &, const QString &)),
            this, SLOT(sltRuntimeError(bool, const QString &, const QString &)));

#ifdef Q_WS_MAC
    /* Show windows: */
    connect(uisession(), SIGNAL(sigShowWindows()),
            this, SLOT(sltShowWindows()));
#endif /* Q_WS_MAC */
}

void UIMachineLogic::prepareActionConnections()
{
    /* "Machine" actions connections: */
    connect(actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleGuestAutoresize(bool)));
    connect(actionsPool()->action(UIActionIndex_Simple_AdjustWindow), SIGNAL(triggered()),
            this, SLOT(sltAdjustWindow()));
    connect(actionsPool()->action(UIActionIndex_Toggle_MouseIntegration), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleMouseIntegration(bool)));
    connect(actionsPool()->action(UIActionIndex_Simple_TypeCAD), SIGNAL(triggered()),
            this, SLOT(sltTypeCAD()));
#ifdef Q_WS_X11
    connect(actionsPool()->action(UIActionIndex_Simple_TypeCABS), SIGNAL(triggered()),
            this, SLOT(sltTypeCABS()));
#endif
    connect(actionsPool()->action(UIActionIndex_Simple_TakeSnapshot), SIGNAL(triggered()),
            this, SLOT(sltTakeSnapshot()));
    connect(actionsPool()->action(UIActionIndex_Simple_InformationDialog), SIGNAL(triggered()),
            this, SLOT(sltShowInformationDialog()));
    connect(actionsPool()->action(UIActionIndex_Toggle_Pause), SIGNAL(toggled(bool)),
            this, SLOT(sltPause(bool)));
    connect(actionsPool()->action(UIActionIndex_Simple_Reset), SIGNAL(triggered()),
            this, SLOT(sltReset()));
    connect(actionsPool()->action(UIActionIndex_Simple_Shutdown), SIGNAL(triggered()),
            this, SLOT(sltACPIShutdown()));
    connect(actionsPool()->action(UIActionIndex_Simple_Close), SIGNAL(triggered()),
            this, SLOT(sltClose()));

    /* "Devices" actions connections: */
    connect(actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_USBDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareUSBMenu()));
    connect(actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog), SIGNAL(triggered()),
            this, SLOT(sltOpenNetworkAdaptersDialog()));
    connect(actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog), SIGNAL(triggered()),
            this, SLOT(sltOpenSharedFoldersDialog()));
    connect(actionsPool()->action(UIActionIndex_Toggle_VRDP), SIGNAL(toggled(bool)),
            this, SLOT(sltSwitchVrdp(bool)));
    connect(actionsPool()->action(UIActionIndex_Simple_InstallGuestTools), SIGNAL(triggered()),
            this, SLOT(sltInstallGuestAdditions()));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* "Debug" actions connections: */
    connect(actionsPool()->action(UIActionIndex_Menu_Debug)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareDebugMenu()));
    connect(actionsPool()->action(UIActionIndex_Simple_Statistics), SIGNAL(triggered()),
            this, SLOT(sltShowDebugStatistics()));
    connect(actionsPool()->action(UIActionIndex_Simple_CommandLine), SIGNAL(triggered()),
            this, SLOT(sltShowDebugCommandLine()));
    connect(actionsPool()->action(UIActionIndex_Toggle_Logging), SIGNAL(toggled(bool)),
            this, SLOT(sltLoggingToggled(bool)));
#endif
}

void UIMachineLogic::prepareActionGroups()
{
#ifdef Q_WS_MAC
    /* On Mac OS X, all QMenu's are consumed by Qt after they are added to
     * another QMenu or a QMenuBar. This means we have to recreate all QMenus
     * when creating a new QMenuBar. */
    uisession()->actionsPool()->createMenus();
#endif /* Q_WS_MAC */

    /* Create group for all actions that are enabled only when the VM is running.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningActions = new QActionGroup(this);
    m_pRunningActions->setExclusive(false);

    /* Create group for all actions that are enabled when the VM is running or paused.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningOrPausedActions = new QActionGroup(this);
    m_pRunningOrPausedActions->setExclusive(false);

    /* Move actions into running actions group: */
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Fullscreen));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Seamless));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_AdjustWindow));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_TypeCAD));
#ifdef Q_WS_X11
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_TypeCABS));
#endif
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_Reset));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_Shutdown));

    /* Move actions into running-n-paused actions group: */
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_MouseIntegration));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_MouseIntegration));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_TakeSnapshot));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_InformationDialog));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Pause));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_OpticalDevices));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_FloppyDevices));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_USBDevices));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_NetworkAdapters));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_SharedFolders));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_VRDP));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_InstallGuestTools));
}

#ifdef Q_WS_MAC
void UIMachineLogic::prepareDock()
{
    QMenu *pDockMenu = actionsPool()->action(UIActionIndex_Menu_Dock)->menu();

    /* Add all VM menu entries to the dock menu. Leave out close and stuff like
     * this. */
    QList<QAction*> actions = actionsPool()->action(UIActionIndex_Menu_Machine)->menu()->actions();
    for (int i=0; i < actions.size(); ++i)
        if (actions.at(i)->menuRole() == QAction::TextHeuristicRole)
            pDockMenu->addAction(actions.at(i));
    pDockMenu->addSeparator();

    QMenu *pDockSettingsMenu = actionsPool()->action(UIActionIndex_Menu_DockSettings)->menu();
    QActionGroup *pDockPreviewModeGroup = new QActionGroup(this);
    QAction *pDockDisablePreview = actionsPool()->action(UIActionIndex_Toggle_DockDisableMonitor);
    pDockPreviewModeGroup->addAction(pDockDisablePreview);
    QAction *pDockEnablePreviewMonitor = actionsPool()->action(UIActionIndex_Toggle_DockPreviewMonitor);
    pDockPreviewModeGroup->addAction(pDockEnablePreviewMonitor);
    pDockSettingsMenu->addActions(pDockPreviewModeGroup->actions());

    connect(pDockPreviewModeGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(sltDockPreviewModeChanged(QAction*)));
    connect(&vboxGlobal(), SIGNAL(dockIconUpdateChanged(const VBoxChangeDockIconUpdateEvent &)),
            this, SLOT(sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &)));

    /* Monitor selection if there are more than one monitor */
    int cGuestScreens = uisession()->session().GetMachine().GetMonitorCount();
    if (cGuestScreens > 1)
    {
        pDockSettingsMenu->addSeparator();
        m_DockIconPreviewMonitor = qMin(session().GetMachine().GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor).toInt(), cGuestScreens - 1);
        m_pDockPreviewSelectMonitorGroup = new QActionGroup(this);
        for (int i = 0; i < cGuestScreens; ++i)
        {
            QAction *pAction = new QAction(m_pDockPreviewSelectMonitorGroup);
            pAction->setCheckable(true);
            pAction->setData(i);
            if (m_DockIconPreviewMonitor == i)
                pAction->setChecked(true);
        }
        pDockSettingsMenu->addActions(m_pDockPreviewSelectMonitorGroup->actions());
        connect(m_pDockPreviewSelectMonitorGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltDockPreviewMonitorChanged(QAction*)));
    }

    pDockMenu->addMenu(pDockSettingsMenu);

    /* Add it to the dock. */
    extern void qt_mac_set_dock_menu(QMenu *);
    qt_mac_set_dock_menu(pDockMenu);

    /* Now the dock icon preview */
    QString osTypeId = session().GetConsole().GetGuest().GetOSTypeId();
    m_pDockIconPreview = new UIDockIconPreview(m_pSession, vboxGlobal().vmGuestOSTypeIcon(osTypeId));

    QString strTest = session().GetMachine().GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateEnabled).toLower();
    /* Default to true if it is an empty value */
    bool f = (strTest.isEmpty() || strTest == "true");
    if (f)
        pDockEnablePreviewMonitor->setChecked(true);
    else
    {
        pDockDisablePreview->setChecked(true);
        if(m_pDockPreviewSelectMonitorGroup)
            m_pDockPreviewSelectMonitorGroup->setEnabled(false);
    }

    /* Default to true if it is an empty value */
    setDockIconPreviewEnabled(f);
    updateDockOverlay();
}
#endif /* Q_WS_MAC */

void UIMachineLogic::prepareRequiredFeatures()
{
#ifdef Q_WS_MAC
# ifdef VBOX_WITH_ICHAT_THEATER
    /* Init shared AV manager: */
    initSharedAVManager();
# endif
#endif
}

void UIMachineLogic::cleanupMachineWindows()
{
#ifdef Q_WS_MAC
    /* We need to clean up the dock stuff before the machine windows. */
    cleanupDock();
#endif /* Q_WS_MAC */
}

#ifdef Q_WS_MAC
void UIMachineLogic::cleanupDock()
{
    if (m_pDockIconPreview)
    {
        delete m_pDockIconPreview;
        m_pDockIconPreview = 0;
    }
}
#endif /* Q_WS_MAC */

void UIMachineLogic::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();

    /* Update action groups: */
    m_pRunningActions->setEnabled(uisession()->isRunning());
    m_pRunningOrPausedActions->setEnabled(uisession()->isRunning() || uisession()->isPaused());

    switch (state)
    {
        case KMachineState_Stuck: // TODO: Test it!
        {
            /* Prevent machine view from resizing: */
            uisession()->setGuestResizeIgnored(true);

            /* Get console: */
            CConsole console = session().GetConsole();

            /* Take the screenshot for debugging purposes and save it. */
            QString strLogFolder = console.GetMachine().GetLogFolder();
            CDisplay display = console.GetDisplay();
            int cGuestScreens = uisession()->session().GetMachine().GetMonitorCount();
            for (int i=0; i < cGuestScreens; ++i)
            {
                QString strFileName;
                if (i == 0)
                    strFileName = strLogFolder + "/VBox.png";
                else
                    strFileName = QString("%1/VBox.%2.png").arg(strLogFolder).arg(i);
                ULONG width = 0;
                ULONG height = 0;
                ULONG bpp = 0;
                display.GetScreenResolution(i, width, height, bpp);
                QImage shot = QImage(width, height, QImage::Format_RGB32);
                display.TakeScreenShot(i, shot.bits(), shot.width(), shot.height());
                shot.save(QFile::encodeName(strFileName), "PNG");
            }

            /* Warn the user about GURU: */
            if (vboxProblem().remindAboutGuruMeditation(console, QDir::toNativeSeparators(strLogFolder)))
            {
                console.PowerDown();
                if (!console.isOk())
                    vboxProblem().cannotStopMachine(console);
            }
            break;
        }
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            QAction *pPauseAction = actionsPool()->action(UIActionIndex_Toggle_Pause);
            if (!pPauseAction->isChecked())
            {
                /* Was paused from CSession side: */
                pPauseAction->blockSignals(true);
                pPauseAction->setChecked(true);
                pPauseAction->blockSignals(false);
            }
            break;
        }
        case KMachineState_Running:
        case KMachineState_Teleporting:
        case KMachineState_LiveSnapshotting:
        {
            QAction *pPauseAction = actionsPool()->action(UIActionIndex_Toggle_Pause);
            if (pPauseAction->isChecked())
            {
                /* Was resumed from CSession side: */
                pPauseAction->blockSignals(true);
                pPauseAction->setChecked(false);
                pPauseAction->blockSignals(false);
            }
            break;
        }
        case KMachineState_PoweredOff:
        case KMachineState_Saved:
        case KMachineState_Teleported:
        case KMachineState_Aborted:
        {
            /* Close VM if it was turned off and closure allowed: */
            if (!isPreventAutoClose())
            {
                /* VM has been powered off, saved or aborted, no matter
                 * internally or externally. We must *safely* close VM window(s): */
                QTimer::singleShot(0, uisession(), SLOT(sltCloseVirtualSession()));
            }
            break;
        }
#ifdef Q_WS_X11
        case KMachineState_Starting:
        case KMachineState_Restoring:
        case KMachineState_TeleportingIn:
        {
            /* The keyboard handler may wish to do some release logging on startup.
             * Tell it that the logger is now active. */
            doXKeyboardLogging(QX11Info::display());
            break;
        }
#endif
        default:
            break;
    }

#ifdef Q_WS_MAC
    /* Update Dock Overlay: */
    updateDockOverlay();
#endif /* Q_WS_MAC */
}

void UIMachineLogic::sltAdditionsStateChanged()
{
    /* Variable falgs: */
    bool fIsAdditionsActive = uisession()->isGuestAdditionsActive();
    bool fIsSupportsGraphics = uisession()->isGuestSupportsGraphics();
    bool fIsSupportsSeamless = uisession()->isGuestSupportsSeamless();

    /* Update action states: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setEnabled(fIsSupportsGraphics);
    actionsPool()->action(UIActionIndex_Toggle_Seamless)->setEnabled(fIsSupportsSeamless);

    /* Check if we should enter some extended mode: */
    sltCheckRequestedModes();

    /* Check the GA version only in case of additions are active: */
    if (!fIsAdditionsActive)
        return;
    /* Check the Guest Additions version and warn the user about possible compatibility issues in case if the installed version is outdated. */
    CGuest guest = session().GetConsole().GetGuest();
    QString strVersion = guest.GetAdditionsVersion();
    uint uVersion = strVersion.toUInt();
    /** @todo r=bird: This isn't want we want! We want the VirtualBox version of the additions, all three numbers. See @bugref{4084}.*/
    QString strRealVersion = QString("%1.%2").arg(RT_HIWORD(uVersion)).arg(RT_LOWORD(uVersion));
    QString strExpectedVersion = QString("%1.%2").arg(VMMDEV_VERSION_MAJOR).arg(VMMDEV_VERSION_MINOR);
    if (RT_HIWORD(uVersion) < VMMDEV_VERSION_MAJOR)
    {
        vboxProblem().warnAboutTooOldAdditions(0, strRealVersion, strExpectedVersion);
    }
    else if (RT_HIWORD(uVersion) == VMMDEV_VERSION_MAJOR && RT_LOWORD(uVersion) <  VMMDEV_VERSION_MINOR)
    {
        vboxProblem().warnAboutOldAdditions(0, strRealVersion, strExpectedVersion);
    }
    else if (uVersion > VMMDEV_VERSION)
    {
        vboxProblem().warnAboutNewAdditions(0, strRealVersion, strExpectedVersion);
    }
}

void UIMachineLogic::sltMouseCapabilityChanged()
{
    /* Variable falgs: */
    bool fIsMouseSupportsAbsolute = uisession()->isMouseSupportsAbsolute();
    bool fIsMouseSupportsRelative = uisession()->isMouseSupportsRelative();
    bool fIsMouseHostCursorNeeded = uisession()->isMouseHostCursorNeeded();

    /* Update action state: */
    QAction *pAction = actionsPool()->action(UIActionIndex_Toggle_MouseIntegration);
    pAction->setEnabled(fIsMouseSupportsAbsolute && fIsMouseSupportsRelative && !fIsMouseHostCursorNeeded);
    if (fIsMouseHostCursorNeeded)
        pAction->setChecked(false);
}

void UIMachineLogic::sltUSBDeviceStateChange(const CUSBDevice &device, bool fIsAttached, const CVirtualBoxErrorInfo &error)
{
    bool fSuccess = error.isNull();

    if (!fSuccess)
    {
        if (fIsAttached)
            vboxProblem().cannotAttachUSBDevice(session().GetConsole(), vboxGlobal().details(device), error);
        else
            vboxProblem().cannotDetachUSBDevice(session().GetConsole(), vboxGlobal().details(device), error);
    }
}

void UIMachineLogic::sltRuntimeError(bool fIsFatal, const QString &strErrorId, const QString &strMessage)
{
    vboxProblem().showRuntimeError(session().GetConsole(), fIsFatal, strErrorId, strMessage);
}

#ifdef Q_WS_MAC
void UIMachineLogic::sltShowWindows()
{
    for (int i=0; i < m_machineWindowsList.size(); ++i)
    {
        UIMachineWindow *pMachineWindow = m_machineWindowsList.at(i);
        /* Dunno what Qt thinks a window that has minimized to the dock
         * should be - it is not hidden, neither is it minimized. OTOH it is
         * marked shown and visible, but not activated. This latter isn't of
         * much help though, since at this point nothing is marked activated.
         * I might have overlooked something, but I'm buggered what if I know
         * what. So, I'll just always show & activate the stupid window to
         * make it get out of the dock when the user wishes to show a VM. */
        pMachineWindow->machineWindow()->raise();
        pMachineWindow->machineWindow()->activateWindow();
    }
}
#endif /* Q_WS_MAC */

void UIMachineLogic::sltCheckRequestedModes()
{
    /* Do not try to enter extended mode if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* If seamless mode is requested, supported and we are NOT currently in seamless mode: */
    if (uisession()->isSeamlessModeRequested() &&
        uisession()->isGuestSupportsSeamless() &&
        visualStateType() != UIVisualStateType_Seamless)
    {
        uisession()->setSeamlessModeRequested(false);
        QAction *pSeamlessModeAction = actionsPool()->action(UIActionIndex_Toggle_Seamless);
        AssertMsg(!pSeamlessModeAction->isChecked(), ("Seamless action should not be triggered before us!\n"));
        QTimer::singleShot(0, pSeamlessModeAction, SLOT(trigger()));
    }
    /* If seamless mode is NOT requested, NOT supported and we are currently in seamless mode: */
    else if (!uisession()->isSeamlessModeRequested() &&
             !uisession()->isGuestSupportsSeamless() &&
             visualStateType() == UIVisualStateType_Seamless)
    {
        uisession()->setSeamlessModeRequested(true);
        QAction *pSeamlessModeAction = actionsPool()->action(UIActionIndex_Toggle_Seamless);
        AssertMsg(pSeamlessModeAction->isChecked(), ("Seamless action should not be triggered before us!\n"));
        QTimer::singleShot(0, pSeamlessModeAction, SLOT(trigger()));
    }
}

void UIMachineLogic::sltToggleGuestAutoresize(bool fEnabled)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Toggle guest-autoresize feature for all view(s)! */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->machineView()->setGuestAutoresizeEnabled(fEnabled);
}

void UIMachineLogic::sltAdjustWindow()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Adjust all window(s)! */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
    {
        /* Exit maximized window state if actual: */
        if (pMachineWindow->machineWindow()->isMaximized())
            pMachineWindow->machineWindow()->showNormal();

        /* Normalize view's geometry: */
        pMachineWindow->machineView()->normalizeGeometry(true);
    }
}

void UIMachineLogic::sltToggleMouseIntegration(bool fOff)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Disable/Enable mouse-integration for all view(s): */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->machineView()->setMouseIntegrationEnabled(!fOff);
}

void UIMachineLogic::sltTypeCAD()
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    Assert(!keyboard.isNull());
    keyboard.PutCAD();
    AssertWrapperOk(keyboard);
}

#ifdef Q_WS_X11
void UIMachineLogic::sltTypeCABS()
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    Assert(!keyboard.isNull());
    static QVector<LONG> aSequence(6);
    aSequence[0] = 0x1d; /* Ctrl down */
    aSequence[1] = 0x38; /* Alt down */
    aSequence[2] = 0x0E; /* Backspace down */
    aSequence[3] = 0x8E; /* Backspace up */
    aSequence[4] = 0xb8; /* Alt up */
    aSequence[5] = 0x9d; /* Ctrl up */
    keyboard.PutScancodes(aSequence);
    AssertWrapperOk(keyboard);
}
#endif

void UIMachineLogic::sltTakeSnapshot()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Remember the paused state. */
    bool fWasPaused = uisession()->isPaused();
    if (!fWasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!uisession()->pause())
            return;
    }

    CMachine machine = session().GetMachine();

    VBoxTakeSnapshotDlg dlg(defaultMachineWindow()->machineWindow(), machine);

    QString strTypeId = machine.GetOSTypeId();
    dlg.mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(strTypeId));

    /* Search for the max available filter index. */
    QString strNameTemplate = QApplication::translate("UIMachineLogic", "Snapshot %1");
    int iMaxSnapshotIndex = searchMaxSnapshotIndex(machine, machine.GetSnapshot(QString()), strNameTemplate);
    dlg.mLeName->setText(strNameTemplate.arg(++ iMaxSnapshotIndex));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole console = session().GetConsole();

        CProgress progress = console.TakeSnapshot(dlg.mLeName->text().trimmed(), dlg.mTeDescription->toPlainText());

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog */
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), 0, 0);

            if (progress.GetResultCode() != 0)
                vboxProblem().cannotTakeSnapshot(progress);
        }
        else
            vboxProblem().cannotTakeSnapshot(console);
    }

    /* Restore the running state if needed. */
    if (!fWasPaused)
    {
        /* Make sure machine-state-change callback is processed: */
        QApplication::sendPostedEvents(uisession(), UIConsoleEventType_StateChange);
        /* Unpause VM: */
        uisession()->unpause();
    }
}

void UIMachineLogic::sltShowInformationDialog()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    VBoxVMInformationDlg::createInformationDlg(mainMachineWindow());
}

void UIMachineLogic::sltReset()
{
    /* Confirm/Reset current console: */
    if (vboxProblem().confirmVMReset(0))
        session().GetConsole().Reset();

    /* TODO_NEW_CORE: On reset the additional screens didn't get a display
       update. Emulate this for now until it get fixed. */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    for (ulong uScreenId = 1; uScreenId < uMonitorCount; ++uScreenId)
        machineWindows().at(uScreenId)->machineWindow()->update();
}

void UIMachineLogic::sltPause(bool fOn)
{
    uisession()->setPause(fOn);
}

void UIMachineLogic::sltACPIShutdown()
{
    /* Get console: */
    CConsole console = session().GetConsole();

    /* Warn the user about ACPI is not available if so: */
    if (!console.GetGuestEnteredACPIMode())
        return vboxProblem().cannotSendACPIToMachine();

    /* Send ACPI shutdown signal, warn if failed: */
    console.PowerButton();
    if (!console.isOk())
        vboxProblem().cannotACPIShutdownMachine(console);
}

void UIMachineLogic::sltClose()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Propose to close default machine window: */
    defaultMachineWindow()->sltTryClose();
}

void UIMachineLogic::sltPrepareStorageMenu()
{
    /* Get the sender() menu: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should only be called on hovering storage menu!\n"));
    pMenu->clear();

    /* Short way to common storage menus: */
    QMenu *pOpticalDevicesMenu = actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->menu();
    QMenu *pFloppyDevicesMenu = actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->menu();

    /* Determine device type: */
    KDeviceType deviceType = pMenu == pOpticalDevicesMenu ? KDeviceType_DVD :
                             pMenu == pFloppyDevicesMenu  ? KDeviceType_Floppy :
                                                            KDeviceType_Null;
    AssertMsg(deviceType != KDeviceType_Null, ("Incorrect storage device type!\n"));

    /* Determine medium type: */
    VBoxDefs::MediumType mediumType = pMenu == pOpticalDevicesMenu ? VBoxDefs::MediumType_DVD :
                                      pMenu == pFloppyDevicesMenu  ? VBoxDefs::MediumType_Floppy :
                                                                     VBoxDefs::MediumType_Invalid;
    AssertMsg(mediumType != VBoxDefs::MediumType_Invalid, ("Incorrect storage medium type!\n"));

    /* Fill attachments menu: */
    CMachine machine = session().GetMachine();
    const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        CStorageController controller = machine.GetStorageControllerByName(attachment.GetController());
        if (!controller.isNull() && (attachment.GetType() == deviceType))
        {
            /* Attachment menu item: */
            QMenu *pAttachmentMenu = 0;
            if (pMenu->menuAction()->data().toInt() > 1)
            {
                pAttachmentMenu = new QMenu(pMenu);
                pAttachmentMenu->setTitle(QString("%1 (%2)").arg(controller.GetName())
                                          .arg (vboxGlobal().toString(StorageSlot(controller.GetBus(),
                                                                                  attachment.GetPort(),
                                                                                  attachment.GetDevice()))));
                switch (controller.GetBus())
                {
                    case KStorageBus_IDE:
                        pAttachmentMenu->setIcon(QIcon(":/ide_16px.png")); break;
                    case KStorageBus_SATA:
                        pAttachmentMenu->setIcon(QIcon(":/sata_16px.png")); break;
                    case KStorageBus_SCSI:
                        pAttachmentMenu->setIcon(QIcon(":/scsi_16px.png")); break;
                    case KStorageBus_Floppy:
                        pAttachmentMenu->setIcon(QIcon(":/floppy_16px.png")); break;
                    default:
                        break;
                }
                pMenu->addMenu(pAttachmentMenu);
            }
            else pAttachmentMenu = pMenu;

            /* Mount Medium actions: */
            CMediumVector mediums;
            switch (mediumType)
            {
                case VBoxDefs::MediumType_DVD:
                    mediums += vboxGlobal().virtualBox().GetHost().GetDVDDrives();
                    mediums += vboxGlobal().virtualBox().GetDVDImages();
                    break;
                case VBoxDefs::MediumType_Floppy:
                    mediums += vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
                    mediums += vboxGlobal().virtualBox().GetFloppyImages();
                    break;
                default:
                    break;
            }

            /* Mediums to be shown: */
            int mediumsToBeShown = 0;
            const int maxMediumsToBeShown = 5;
            CMedium currentMedium = attachment.GetMedium();
            QString currentId = currentMedium.isNull() ? QString::null : currentMedium.GetId();
            bool fCurrentUsed = false;
            foreach (CMedium medium, mediums)
            {
                bool isMediumUsed = false;
                foreach (const CMediumAttachment &otherAttachment, attachments)
                {
                    if (otherAttachment != attachment)
                    {
                        CMedium otherMedium = otherAttachment.GetMedium();
                        if (!otherMedium.isNull() && otherMedium.GetId() == medium.GetId())
                        {
                            isMediumUsed = true;
                            break;
                        }
                    }
                }
                if (!isMediumUsed)
                {
                    if (!fCurrentUsed && !currentMedium.isNull() && mediumsToBeShown == maxMediumsToBeShown - 1)
                        medium = currentMedium;

                    if (medium.GetId() == currentId)
                        fCurrentUsed = true;

                    QAction *mountMediumAction = new QAction(VBoxMedium(medium, mediumType).name(), pAttachmentMenu);
                    mountMediumAction->setCheckable(true);
                    mountMediumAction->setChecked(!currentMedium.isNull() && medium.GetId() == currentId);
                    mountMediumAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(),
                                                                                attachment.GetPort(),
                                                                                attachment.GetDevice(),
                                                                                medium.GetId())));
                    connect(mountMediumAction, SIGNAL(triggered(bool)), this, SLOT(sltMountStorageMedium()));
                    pAttachmentMenu->addAction(mountMediumAction);
                    ++ mediumsToBeShown;
                    if (mediumsToBeShown == maxMediumsToBeShown)
                        break;
                }
            }

            /* Virtual Media Manager action: */
            QAction *callVMMAction = new QAction(pAttachmentMenu);
            callVMMAction->setIcon(QIcon(":/diskimage_16px.png"));
            callVMMAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(),
                                                                    attachment.GetPort(),
                                                                    attachment.GetDevice(),
                                                                    mediumType)));
            connect(callVMMAction, SIGNAL(triggered(bool)), this, SLOT(sltMountStorageMedium()));
            pAttachmentMenu->addAction(callVMMAction);

            /* Insert separator: */
            pAttachmentMenu->addSeparator();

            /* Unmount Medium action: */
            QAction *unmountMediumAction = new QAction(pAttachmentMenu);
            unmountMediumAction->setEnabled(!currentMedium.isNull());
            unmountMediumAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(),
                                                                          attachment.GetPort(),
                                                                          attachment.GetDevice())));
            connect(unmountMediumAction, SIGNAL(triggered(bool)), this, SLOT(sltMountStorageMedium()));
            pAttachmentMenu->addAction(unmountMediumAction);

            /* Switch CD/FD naming */
            switch (mediumType)
            {
                case VBoxDefs::MediumType_DVD:
                    callVMMAction->setText(QApplication::translate("UIMachineLogic", "More CD/DVD Images..."));
                    unmountMediumAction->setText(QApplication::translate("UIMachineLogic", "Unmount CD/DVD Device"));
                    unmountMediumAction->setIcon(VBoxGlobal::iconSet(":/cd_unmount_16px.png",
                                                                     ":/cd_unmount_dis_16px.png"));
                    break;
                case VBoxDefs::MediumType_Floppy:
                    callVMMAction->setText(QApplication::translate("UIMachineLogic", "More Floppy Images..."));
                    unmountMediumAction->setText(QApplication::translate("UIMachineLogic", "Unmount Floppy Device"));
                    unmountMediumAction->setIcon(VBoxGlobal::iconSet(":/fd_unmount_16px.png",
                                                                     ":/fd_unmount_dis_16px.png"));
                    break;
                default:
                    break;
            }
        }
    }

    if (pMenu->menuAction()->data().toInt() == 0)
    {
        /* Empty menu item */
        Assert(pMenu->isEmpty());
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        switch (mediumType)
        {
            case VBoxDefs::MediumType_DVD:
                pEmptyMenuAction->setText(QApplication::translate("UIMachineLogic", "No CD/DVD Devices Attached"));
                pEmptyMenuAction->setToolTip(QApplication::translate("UIMachineLogic", "No CD/DVD devices attached to that VM"));
                break;
            case VBoxDefs::MediumType_Floppy:
                pEmptyMenuAction->setText(QApplication::translate("UIMachineLogic", "No Floppy Devices Attached"));
                pEmptyMenuAction->setToolTip(QApplication::translate("UIMachineLogic", "No floppy devices attached to that VM"));
                break;
            default:
                break;
        }
        pEmptyMenuAction->setIcon(VBoxGlobal::iconSet(":/delete_16px.png", ":/delete_dis_16px.png"));
        pMenu->addAction(pEmptyMenuAction);
    }
}

void UIMachineLogic::sltMountStorageMedium()
{
    /* Get sender action: */
    QAction *action = qobject_cast<QAction*>(sender());
    AssertMsg(action, ("This slot should only be called on selecting storage menu item!\n"));

    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Get mount-target: */
    MediumTarget target = action->data().value<MediumTarget>();

    /* Current mount-target attributes: */
    CMediumAttachment currentAttachment = machine.GetMediumAttachment(target.name, target.port, target.device);
    CMedium currentMedium = currentAttachment.GetMedium();
    QString currentId = currentMedium.isNull() ? QString("") : currentMedium.GetId();

    /* New mount-target attributes: */
    QString newId = QString("");
    bool fSelectWithMediaManager = target.type != VBoxDefs::MediumType_Invalid;

    /* Open Virtual Media Manager to select image id: */
    if (fSelectWithMediaManager)
    {
        /* Search for already used images: */
        QStringList usedImages;
        foreach (const CMediumAttachment &attachment, machine.GetMediumAttachments())
        {
            CMedium medium = attachment.GetMedium();
            if (attachment != currentAttachment && !medium.isNull() && !medium.GetHostDrive())
                usedImages << medium.GetId();
        }
        /* Open VMM Dialog: */
        VBoxMediaManagerDlg dlg(defaultMachineWindow()->machineWindow());
        dlg.setup(target.type, true /* select? */, true /* refresh? */, machine, currentId, true, usedImages);
        if (dlg.exec() == QDialog::Accepted)
            newId = dlg.selectedId();
        else return;
    }
    /* Use medium which was sent: */
    else if (!target.id.isNull() && target.id != currentId)
        newId = target.id;

    bool fMount = !newId.isEmpty();

    /* Remount medium to the predefined port/device: */
    bool fWasMounted = false;
    machine.MountMedium(target.name, target.port, target.device, newId, false /* force */);
    if (machine.isOk())
        fWasMounted = true;
    else
    {
        /* Ask for force remounting: */
        if (vboxProblem().cannotRemountMedium(0, machine, vboxGlobal().findMedium (fMount ? newId : currentId), fMount, true /* retry? */) == QIMessageBox::Ok)
        {
            /* Force remount medium to the predefined port/device: */
            machine.MountMedium(target.name, target.port, target.device, newId, true /* force */);
            if (machine.isOk())
                fWasMounted = true;
            else
                vboxProblem().cannotRemountMedium(0, machine, vboxGlobal().findMedium (fMount ? newId : currentId), fMount, false /* retry? */);
        }
    }

    /* Save medium mounted at runtime */
    if (fWasMounted && !uisession()->isIgnoreRuntimeMediumsChanging())
    {
        machine.SaveSettings();
        if (!machine.isOk())
            vboxProblem().cannotSaveMachineSettings(machine);
    }
}

void UIMachineLogic::sltPrepareUSBMenu()
{
    /* Get the sender() menu: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
#ifdef RT_STRICT
    QMenu *pUSBDevicesMenu = actionsPool()->action(UIActionIndex_Menu_USBDevices)->menu();
#endif
    AssertMsg(pMenu == pUSBDevicesMenu, ("This slot should only be called on hovering USB menu!\n"));
    pMenu->clear();

    /* Get HOST: */
    CHost host = vboxGlobal().virtualBox().GetHost();

    /* Get USB devices list: */
    CHostUSBDeviceVector devices = host.GetUSBDevices();

    /* Fill USB devices menu: */
    bool fIsUSBListEmpty = devices.size() == 0;
    if (fIsUSBListEmpty)
    {
        /* Fill USB devices menu: */
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        pEmptyMenuAction->setText(QApplication::translate("UIMachineLogic", "No USB Devices Connected"));
        pEmptyMenuAction->setIcon(VBoxGlobal::iconSet(":/delete_16px.png", ":/delete_dis_16px.png"));
        pEmptyMenuAction->setToolTip(QApplication::translate("UIMachineLogic", "No supported devices connected to the host PC"));
    }
    else
    {
        foreach (const CHostUSBDevice hostDevice, devices)
        {
            /* Get common USB device: */
            CUSBDevice device(hostDevice);

            /* Create USB device action: */
            QAction *attachUSBAction = new QAction(vboxGlobal().details(device), pMenu);
            attachUSBAction->setCheckable(true);
            connect(attachUSBAction, SIGNAL(triggered(bool)), this, SLOT(sltAttachUSBDevice()));
            pMenu->addAction(attachUSBAction);

            /* Check if that USB device was alread attached to this session: */
            CConsole console = session().GetConsole();
            CUSBDevice attachedDevice = console.FindUSBDeviceById(device.GetId());
            attachUSBAction->setChecked(!attachedDevice.isNull());
            attachUSBAction->setEnabled(hostDevice.GetState() != KUSBDeviceState_Unavailable);

            /* Set USB attach data: */
            attachUSBAction->setData(QVariant::fromValue(USBTarget(!attachUSBAction->isChecked(), device.GetId())));
            attachUSBAction->setToolTip(vboxGlobal().toolTip(device));
        }
    }
}

void UIMachineLogic::sltAttachUSBDevice()
{
    /* Get sender action: */
    QAction *action = qobject_cast<QAction*>(sender());
    AssertMsg(action, ("This slot should only be called on selecting USB menu item!\n"));

    /* Get current console: */
    CConsole console = session().GetConsole();

    /* Get USB target: */
    USBTarget target = action->data().value<USBTarget>();
    CUSBDevice device = console.FindUSBDeviceById(target.id);

    /* Attach USB device: */
    if (target.attach)
    {
        console.AttachUSBDevice(target.id);
        if (!console.isOk())
            vboxProblem().cannotAttachUSBDevice(console, vboxGlobal().details(device));
    }
    else
    {
        console.DetachUSBDevice(target.id);
        if (!console.isOk())
            vboxProblem().cannotDetachUSBDevice(console, vboxGlobal().details(device));
    }
}

void UIMachineLogic::sltOpenNetworkAdaptersDialog()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Show network settings dialog: */
    UINetworkAdaptersDialog dlg(defaultMachineWindow()->machineWindow(), session());
    dlg.exec();
}

void UIMachineLogic::sltOpenSharedFoldersDialog()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Show shared folders settings dialog: */
    UISharedFoldersDialog dlg(defaultMachineWindow()->machineWindow(), session());
    dlg.exec();
}

void UIMachineLogic::sltSwitchVrdp(bool fOn)
{
    /* Enable VRDP server if possible: */
    CVRDPServer server = session().GetMachine().GetVRDPServer();
    AssertMsg(!server.isNull(), ("VRDP server should not be null!\n"));
    server.SetEnabled(fOn);
}

void UIMachineLogic::sltInstallGuestAdditions()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    char strAppPrivPath[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch(strAppPrivPath, sizeof(strAppPrivPath));
    AssertRC (rc);

    QString strSrc1 = QString(strAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString strSrc2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";

    /* Check the standard image locations */
    if (QFile::exists(strSrc1))
        return uisession()->sltInstallGuestAdditionsFrom(strSrc1);
    else if (QFile::exists(strSrc2))
        return uisession()->sltInstallGuestAdditionsFrom(strSrc2);

    /* Check for the already registered image */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString name = QString("VBoxGuestAdditions_%1.iso").arg(vbox.GetVersion().remove("_OSE"));

    CMediumVector vec = vbox.GetDVDImages();
    for (CMediumVector::ConstIterator it = vec.begin(); it != vec.end(); ++ it)
    {
        QString path = it->GetLocation();
        /* Compare the name part ignoring the file case */
        QString fn = QFileInfo(path).fileName();
        if (RTPathCompare(name.toUtf8().constData(), fn.toUtf8().constData()) == 0)
            return uisession()->sltInstallGuestAdditionsFrom(path);
    }

    /* Download the required image */
    int result = vboxProblem().cannotFindGuestAdditions(QDir::toNativeSeparators(strSrc1), QDir::toNativeSeparators(strSrc2));
    if (result == QIMessageBox::Yes)
    {
        QString source = QString("http://download.virtualbox.org/virtualbox/%1/").arg(vbox.GetVersion().remove("_OSE")) + name;
        QString target = QDir(vboxGlobal().virtualBox().GetHomeFolder()).absoluteFilePath(name);

        UIDownloaderAdditions *pDl = UIDownloaderAdditions::create();
        /* Configure the additions downloader. */
        pDl->setSource(source);
        pDl->setTarget(target);
        pDl->setAction(actionsPool()->action(UIActionIndex_Simple_InstallGuestTools));
        pDl->setParentWidget(mainMachineWindow()->machineWindow());
        /* After the download is finished the user may like to install the
         * additions.*/
        connect(pDl, SIGNAL(downloadFinished(const QString&)),
                uisession(), SLOT(sltInstallGuestAdditionsFrom(const QString&)));
        /* Some of the modes may show additional info of the download progress: */
        emit sigDownloaderAdditionsCreated();
        /* Start the download: */
        pDl->startDownload();
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineLogic::sltPrepareDebugMenu()
{
    /* The "Logging" item. */
    bool fEnabled = false;
    bool fChecked = false;
    CConsole console = session().GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
        {
            fEnabled = true;
            fChecked = cdebugger.GetLogEnabled() != FALSE;
        }
    }
    if (fEnabled != actionsPool()->action(UIActionIndex_Toggle_Logging)->isEnabled())
        actionsPool()->action(UIActionIndex_Toggle_Logging)->setEnabled(fEnabled);
    if (fChecked != actionsPool()->action(UIActionIndex_Toggle_Logging)->isChecked())
        actionsPool()->action(UIActionIndex_Toggle_Logging)->setChecked(fChecked);
}

void UIMachineLogic::sltShowDebugStatistics()
{
    if (dbgCreated())
        m_pDbgGuiVT->pfnShowStatistics(m_pDbgGui);
}

void UIMachineLogic::sltShowDebugCommandLine()
{
    if (dbgCreated())
        m_pDbgGuiVT->pfnShowCommandLine(m_pDbgGui);
}

void UIMachineLogic::sltLoggingToggled(bool fState)
{
    NOREF(fState);
    CConsole console = session().GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
            cdebugger.SetLogEnabled(fState);
    }
}
#endif

#ifdef Q_WS_MAC
void UIMachineLogic::sltDockPreviewModeChanged(QAction *pAction)
{
    CMachine machine = m_pSession->session().GetMachine();
    if (!machine.isNull())
    {
        bool fEnabled = true;
        if (pAction == actionsPool()->action(UIActionIndex_Toggle_DockDisableMonitor))
            fEnabled = false;

        machine.SetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateEnabled, fEnabled ? "true" : "false");
        updateDockOverlay();
    }
}

void UIMachineLogic::sltDockPreviewMonitorChanged(QAction *pAction)
{
    CMachine machine = m_pSession->session().GetMachine();
    if (!machine.isNull())
    {
        int monitor = pAction->data().toInt();
        machine.SetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor, QString::number(monitor));
        updateDockOverlay();
    }
}

void UIMachineLogic::sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event)
{
    if (isMachineWindowsCreated())
    {
        setDockIconPreviewEnabled(event.mChanged);
        if (m_pDockPreviewSelectMonitorGroup)
        {
            m_pDockPreviewSelectMonitorGroup->setEnabled(event.mChanged);
            CMachine machine = session().GetMachine();
            m_DockIconPreviewMonitor = qMin(machine.GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor).toInt(), (int)machine.GetMonitorCount() - 1);
        }
        /* Resize the dock icon in the case the preview monitor has changed. */
        QSize size = machineWindows().at(m_DockIconPreviewMonitor)->machineView()->size();
        updateDockIconSize(m_DockIconPreviewMonitor, size.width(), size.height());
        updateDockOverlay();
    }
}
#endif /* Q_WS_MAC */

int UIMachineLogic::searchMaxSnapshotIndex(const CMachine &machine,
                                           const CSnapshot &snapshot,
                                           const QString &strNameTemplate)
{
    int iMaxIndex = 0;
    QRegExp regExp(QString("^") + strNameTemplate.arg("([0-9]+)") + QString("$"));
    if (!snapshot.isNull())
    {
        /* Check the current snapshot name */
        QString strName = snapshot.GetName();
        int iPos = regExp.indexIn(strName);
        if (iPos != -1)
            iMaxIndex = regExp.cap(1).toInt() > iMaxIndex ? regExp.cap(1).toInt() : iMaxIndex;
        /* Traversing all the snapshot children */
        foreach (const CSnapshot &child, snapshot.GetChildren())
        {
            int iMaxIndexOfChildren = searchMaxSnapshotIndex(machine, child, strNameTemplate);
            iMaxIndex = iMaxIndexOfChildren > iMaxIndex ? iMaxIndexOfChildren : iMaxIndex;
        }
    }
    return iMaxIndex;
}

#ifdef VBOX_WITH_DEBUGGER_GUI
bool UIMachineLogic::dbgCreated()
{
    if (m_pDbgGui)
        return true;

    RTLDRMOD hLdrMod = vboxGlobal().getDebuggerModule();
    if (hLdrMod == NIL_RTLDRMOD)
        return false;

    PFNDBGGUICREATE pfnGuiCreate;
    int rc = RTLdrGetSymbol(hLdrMod, "DBGGuiCreate", (void**)&pfnGuiCreate);
    if (RT_SUCCESS(rc))
    {
        ISession *pISession = session().raw();
        rc = pfnGuiCreate(pISession, &m_pDbgGui, &m_pDbgGuiVT);
        if (RT_SUCCESS(rc))
        {
            if (DBGGUIVT_ARE_VERSIONS_COMPATIBLE(m_pDbgGuiVT->u32Version, DBGGUIVT_VERSION) ||
                m_pDbgGuiVT->u32EndVersion == m_pDbgGuiVT->u32Version)
            {
                m_pDbgGuiVT->pfnSetParent(m_pDbgGui, defaultMachineWindow()->machineWindow());
                m_pDbgGuiVT->pfnSetMenu(m_pDbgGui, actionsPool()->action(UIActionIndex_Menu_Debug));
                dbgAdjustRelativePos();
                return true;
            }

            LogRel(("DBGGuiCreate failed, incompatible versions (loaded %#x/%#x, expected %#x)\n",
                    m_pDbgGuiVT->u32Version, m_pDbgGuiVT->u32EndVersion, DBGGUIVT_VERSION));
        }
        else
            LogRel(("DBGGuiCreate failed, rc=%Rrc\n", rc));
    }
    else
        LogRel(("RTLdrGetSymbol(,\"DBGGuiCreate\",) -> %Rrc\n", rc));

    m_pDbgGui = 0;
    m_pDbgGuiVT = 0;
    return false;
}

void UIMachineLogic::dbgDestroy()
{
    if (m_pDbgGui)
    {
        m_pDbgGuiVT->pfnDestroy(m_pDbgGui);
        m_pDbgGui = 0;
        m_pDbgGuiVT = 0;
    }
}

void UIMachineLogic::dbgAdjustRelativePos()
{
    if (m_pDbgGui)
    {
        QRect rct = defaultMachineWindow()->machineWindow()->frameGeometry();
        m_pDbgGuiVT->pfnAdjustRelativePos(m_pDbgGui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}
#endif

#if 0 // TODO: Where to move that?
void UIMachineLogic::setViewInSeamlessMode (const QRect &aTargetRect)
{
#ifndef Q_WS_MAC
    /* It isn't guaranteed that the guest os set the video mode that
     * we requested. So after all the resizing stuff set the clipping
     * mask and the spacing shifter to the corresponding values. */
    QDesktopWidget *dtw = QApplication::desktop();
    QRect sRect = dtw->screenGeometry (this);
    QRect aRect (aTargetRect);
    mMaskShift.scale (aTargetRect.left(), aTargetRect.top(), Qt::IgnoreAspectRatio);
    /* Set the clipping mask */
    mStrictedRegion = aRect;
    /* Set the shifting spacer */
    mShiftingSpacerLeft->changeSize (RT_ABS (sRect.left() - aRect.left()), 0,
                                     QSizePolicy::Fixed, QSizePolicy::Preferred);
    mShiftingSpacerTop->changeSize (0, RT_ABS (sRect.top() - aRect.top()),
                                    QSizePolicy::Preferred, QSizePolicy::Fixed);
    mShiftingSpacerRight->changeSize (RT_ABS (sRect.right() - aRect.right()), 0,
                                      QSizePolicy::Fixed, QSizePolicy::Preferred);
    mShiftingSpacerBottom->changeSize (0, RT_ABS (sRect.bottom() - aRect.bottom()),
                                           QSizePolicy::Preferred, QSizePolicy::Fixed);
#else // !Q_WS_MAC
    NOREF (aTargetRect);
#endif // !Q_WS_MAC
}
#endif

#include "UIMachineLogic.moc"

