/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogic class implementation
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

/* Global includes */
#include <QDir>
#include <QFileInfo>
#include <QProgressBar>
#include <QDesktopWidget>

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "VBoxMediaManagerDlg.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxVMInformationDlg.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsSF.h"
//#include "VBoxDownloaderWgt.h"

#include "QIFileDialog.h"
#include "QIHttp.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineLogicNormal.h"
//#include "UIMachineLogicFullscreen.h"
//#include "UIMachineLogicSeamless.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"

#include <iprt/param.h>
#include <iprt/path.h>
#include <VBox/VMMDev.h>

#ifdef VBOX_WITH_DEBUGGER_GUI
# include <iprt/ldr.h>
#endif

#ifdef Q_WS_X11
# include <XKeyboard.h>
#endif

#ifdef Q_WS_X11
# include <QX11Info>
#endif

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
    USBTarget(bool bAttach, const QString &strId)
        : attach(bAttach), id(strId) {}
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
        setWindowTitle(tr("Network Adapters"));
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
        setWindowTitle(tr("Shared Folders"));
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

#if 0
class UIAdditionsDownloader : public VBoxDownloaderWgt
{
    Q_OBJECT;

public:

    UIAdditionsDownloader (const QString &aSource, const QString &aTarget, QAction *aAction)
        : VBoxDownloaderWgt (aSource, aTarget)
        , mAction (aAction)
    {
        mAction->setEnabled (false);
        retranslateUi();
    }

    void start()
    {
        acknowledgeStart();
    }

protected:

    void retranslateUi()
    {
        mCancelButton->setText (tr ("Cancel"));
        mProgressBar->setToolTip (tr ("Downloading the VirtualBox Guest Additions "
                                      "CD image from <nobr><b>%1</b>...</nobr>")
                                      .arg (mSource.toString()));
        mCancelButton->setToolTip (tr ("Cancel the VirtualBox Guest "
                                       "Additions CD image download"));
    }

private slots:

    void downloadFinished (bool aError)
    {
        if (aError)
            VBoxDownloaderWgt::downloadFinished (aError);
        else
        {
            QByteArray receivedData (mHttp->readAll());
            /* Serialize the incoming buffer into the .iso image. */
            while (true)
            {
                QFile file (mTarget);
                if (file.open (QIODevice::WriteOnly))
                {
                    file.write (receivedData);
                    file.close();
                    //if (vboxProblem().confirmMountAdditions (mSource.toString(),QDir::toNativeSeparators (mTarget)))
                    //    vboxGlobal().consoleWnd().installGuestAdditionsFrom (mTarget);
                    QTimer::singleShot (0, this, SLOT (suicide()));
                    break;
                }
                else
                {
                    vboxProblem().message (window(), VBoxProblemReporter::Error,
                        tr ("<p>Failed to save the downloaded file as "
                            "<nobr><b>%1</b>.</nobr></p>")
                        .arg (QDir::toNativeSeparators (mTarget)));
                }

                QString target = QIFileDialog::getExistingDirectory (
                    QFileInfo (mTarget).absolutePath(), this,
                    tr ("Select folder to save Guest Additions image to"), true);
                if (target.isNull())
                    QTimer::singleShot (0, this, SLOT (suicide()));
                else
                    mTarget = QDir (target).absoluteFilePath (QFileInfo (mTarget).fileName());
            }
        }
    }

    void suicide()
    {
        QStatusBar *sb = qobject_cast <QStatusBar*> (parent());
        Assert (sb);
        sb->removeWidget (this);
        mAction->setEnabled (true);
        VBoxDownloaderWgt::suicide();
    }

private:

    bool confirmDownload()
    {
        return vboxProblem().confirmDownloadAdditions (mSource.toString(),
            mHttp->lastResponse().contentLength());
    }

    void warnAboutError (const QString &aError)
    {
        return vboxProblem().cannotDownloadGuestAdditions (mSource.toString(), aError);
    }

    QAction *mAction;
};
#endif

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
            // logic = new UIMachineLogicFullscreen(pParent, pSession, pActionsPool);
            logic = new UIMachineLogicNormal(pParent, pSession, pActionsPool);
            break;
        case UIVisualStateType_Seamless:
            // logic = new UIMachineLogicSeamless(pParent, pSession, pActionsPool);
            logic = new UIMachineLogicNormal(pParent, pSession, pActionsPool);
            break;
    }
    return logic;
}

UIMachineLogic::UIMachineLogic(QObject *pParent,
                               UISession *pSession,
                               UIActionsPool *pActionsPool,
                               UIVisualStateType visualStateType)
    /* Initialize parent class: */
    : QObject(pParent)
    /* Initialize protected members: */
    , m_pMachineWindowContainer(0)
    /* Initialize private members: */
    , m_pSession(pSession)
    , m_pActionsPool(pActionsPool)
    , m_machineState(KMachineState_Null)
    , m_visualStateType(visualStateType)
    /* Action groups: */
    , m_pRunningActions(0)
    , m_pRunningOrPausedActions(0)
    /* Bool flags: */
    , m_bIsFirstTimeStarted(false)
    , m_bIsOpenViewFinished(false)
    , m_bIsGraphicsSupported(false)
    , m_bIsSeamlessSupported(false)
    , m_bIsAutoSaveMedia(true)
    , m_bIsPreventAutoClose(false)
{
}

UIMachineLogic::~UIMachineLogic()
{
#ifdef VBOX_WITH_DEBUGGER_GUI // TODO: Should we close debugger now?
    /* Close debugger: */
    //dbgDestroy();
#endif
}

void UIMachineLogic::prepareConsoleConnections()
{
    connect(uisession(), SIGNAL(sigStateChange(KMachineState)), this, SLOT(sltMachineStateChanged(KMachineState)));
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));
}

void UIMachineLogic::prepareActionGroups()
{
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

    /* Move actions into first group: */
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Fullscreen));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Seamless));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_AdjustWindow));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_TypeCAD));
#ifdef Q_WS_X11
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_TypeCABS));
#endif
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_Reset));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_Shutdown));
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_Statistics));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Simple_CommandLine));
    m_pRunningActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Logging));
#endif

    /* Move actions into second group: */
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Menu_MouseIntegration));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_MouseIntegration));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_TakeSnapshot));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_InformationDialog));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Toggle_Pause));
    m_pRunningOrPausedActions->addAction(actionsPool()->action(UIActionIndex_Simple_Close));
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

void UIMachineLogic::prepareActionConnections()
{
    /* "Machine" actions connections */
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

    /* "Devices" actions connections */
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
    /* "Debug" actions connections */
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

void UIMachineLogic::prepareRequiredFeatures()
{
    /* Get current console: */
    CConsole console = session().GetConsole();

    /* Check if the virtualization feature is required. */
    bool bIs64BitsGuest = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetIs64Bit();
    bool fRecommendVirtEx = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetRecommendedVirtEx();
    AssertMsg(!bIs64BitsGuest || fRecommendVirtEx, ("Virtualization support missed for 64bit guest!\n"));
    bool bIsVirtEnabled = console.GetDebugger().GetHWVirtExEnabled();
    if (fRecommendVirtEx && !bIsVirtEnabled)
    {
        bool result;

        pause();

        bool fVTxAMDVSupported = vboxGlobal().virtualBox().GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx);

        if (bIs64BitsGuest)
            result = vboxProblem().warnAboutVirtNotEnabled64BitsGuest(fVTxAMDVSupported);
        else
            result = vboxProblem().warnAboutVirtNotEnabledGuestRequired(fVTxAMDVSupported);

        if (result == true)
            sltClose();
        else
            unpause();
    }

#ifdef Q_WS_MAC
# ifdef VBOX_WITH_ICHAT_THEATER
    initSharedAVManager();
# endif
#endif
}

void UIMachineLogic::loadLogicSettings()
{
    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Extra-data settings: */
    {
        QString strSettings;

        strSettings = machine.GetExtraData(VBoxDefs::GUI_AutoresizeGuest);
        if (strSettings != "off")
            actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setChecked(true);

        strSettings = machine.GetExtraData(VBoxDefs::GUI_FirstRun);
        if (strSettings == "yes")
            m_bIsFirstTimeStarted = true;

        strSettings = machine.GetExtraData(VBoxDefs::GUI_SaveMountedAtRuntime);
        if (strSettings == "no")
            m_bIsAutoSaveMedia = false;
    }

    /* Initial settings: */
    {
        /* Initialize storage stuff: */
        int iDevicesCountCD = 0;
        int iDevicesCountFD = 0;
        const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
        foreach (const CMediumAttachment &attachment, attachments)
        {
            if (attachment.GetType() == KDeviceType_DVD)
                ++ iDevicesCountCD;
            if (attachment.GetType() == KDeviceType_Floppy)
                ++ iDevicesCountFD;
        }
        actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->setData(iDevicesCountCD);
        actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->setData(iDevicesCountFD);
        actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->setVisible(iDevicesCountCD);
        actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->setVisible(iDevicesCountFD);
    }

    /* Availability settings: */
    {
        /* USB Stuff: */
        CUSBController usbController = machine.GetUSBController();
        if (usbController.isNull())
        {
            /* Hide USB menu if controller is NULL: */
            actionsPool()->action(UIActionIndex_Menu_USBDevices)->setVisible(false);
        }
        else
        {
            /* Enable/Disable USB menu depending on USB controller: */
            actionsPool()->action(UIActionIndex_Menu_USBDevices)->setEnabled(usbController.GetEnabled());
        }

        /* VRDP Stuff: */
        CVRDPServer vrdpServer = machine.GetVRDPServer();
        if (vrdpServer.isNull())
        {
            /* Hide VRDP Action: */
            actionsPool()->action(UIActionIndex_Toggle_VRDP)->setVisible(false);
        }
    }
}

void UIMachineLogic::saveLogicSettings()
{
    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Extra-data settings: */
    {
        machine.SetExtraData(VBoxDefs::GUI_AutoresizeGuest,
                             actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->isChecked() ? "on" : "off");

        machine.SetExtraData(VBoxDefs::GUI_FirstRun, QString());

        // TODO: Move to fullscreen/seamless logic:
        //machine.SetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide, mMiniToolBar->isAutoHide() ? "on" : "off");
    }
}

CSession& UIMachineLogic::session()
{
    return m_pSession->session();
}

void UIMachineLogic::sltMachineStateChanged(KMachineState newMachineState)
{
    if (m_machineState != newMachineState)
    {
        /* Variable flags: */
        bool bIsRunning = newMachineState == KMachineState_Running ||
                          newMachineState == KMachineState_Teleporting ||
                          newMachineState == KMachineState_LiveSnapshotting;
        bool bIsRunningOrPaused = bIsRunning ||
                                  newMachineState == KMachineState_Paused;

        /* Update action groups: */
        m_pRunningActions->setEnabled(bIsRunning);
        m_pRunningOrPausedActions->setEnabled(bIsRunningOrPaused);

        /* Do we have GURU? */
        bool bIsGuruMeditation = false;

        switch (newMachineState)
        {
            case KMachineState_Stuck:
            {
                bIsGuruMeditation = true;
                break;
            }
            case KMachineState_Paused:
            {
                /* Was paused from CMachine side? */
                QAction *pPauseAction = actionsPool()->action(UIActionIndex_Toggle_Pause);
                if (!pPauseAction->isChecked())
                {
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
                /* Was started from CMachine side? */
                QAction *pPauseAction = actionsPool()->action(UIActionIndex_Toggle_Pause);
                if (isPaused() && pPauseAction->isChecked())
                {
                    pPauseAction->blockSignals(true);
                    pPauseAction->setChecked(false);
                    pPauseAction->blockSignals(false);
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

        /* Now storing new state: */
        m_machineState = newMachineState;

        /* Close VM if was closed someway: */
        if (m_machineState == KMachineState_PoweredOff || m_machineState == KMachineState_Saved ||
            m_machineState == KMachineState_Teleported || m_machineState == KMachineState_Aborted)
        {
            /* VM has been powered off or saved or aborted, no matter internally or externally.
             * We must *safely* close the console window unless auto closure is disabled: */
            if (!m_bIsPreventAutoClose)
                machineWindowWrapper()->sltTryClose();
        }

        /* Process GURU: */
        if (bIsGuruMeditation)
        {
            if (machineWindowWrapper()->machineView())
                machineWindowWrapper()->machineView()->setIgnoreGuestResize(true);

            CConsole console = session().GetConsole();
            QString strLogFolder = console.GetMachine().GetLogFolder();

            /* Take the screenshot for debugging purposes and save it */
            QString strFileName = strLogFolder + "/VBox.png";
            CDisplay display = console.GetDisplay();
            QImage shot = QImage(display.GetWidth(), display.GetHeight(), QImage::Format_RGB32);
            display.TakeScreenShot(shot.bits(), shot.width(), shot.height());
            shot.save(QFile::encodeName(strFileName), "PNG");

            /* Warn the user about GURU: */
            if (vboxProblem().remindAboutGuruMeditation(console, QDir::toNativeSeparators(strLogFolder)))
            {
                qApp->processEvents();
                console.PowerDown();
                if (!console.isOk())
                    vboxProblem().cannotStopMachine(console);
            }
        }

#ifdef Q_WS_MAC
        /* Update Dock Overlay: */
        if (machineWindowWrapper())
            machineWindowWrapper()->updateDockOverlay();
#endif
    }
}

void UIMachineLogic::sltAdditionsStateChanged()
{
    /* Get console guest: */
    CGuest guest = session().GetConsole().GetGuest();

    /* Variable flags: */
    bool bIsActive = guest.GetAdditionsActive();
    bool bIsGraphicsSupported = guest.GetSupportsGraphics();
    bool bIsSeamlessSupported = guest.GetSupportsSeamless();

    /* Update action groups: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setEnabled(bIsActive && bIsGraphicsSupported);
    actionsPool()->action(UIActionIndex_Toggle_Seamless)->setEnabled(bIsActive && bIsGraphicsSupported && bIsSeamlessSupported);

    /* Store new values: */
    m_bIsSeamlessSupported = bIsSeamlessSupported;
    m_bIsGraphicsSupported = bIsGraphicsSupported;

#if 0 // TODO: Re-activate seamless if necessary!
    /* If seamless mode should be enabled then check if it is enabled currently and re-enable it if open-view procedure is finished */
    if ((m_bIsSeamlessSupported != bIsSeamlessSupported) || (m_bIsGraphicsSupported != bIsGraphicsSupported))
    {
        if (actionsPool()->action(UIActionIndex_Toggle_Seamless)->isChecked() && m_bIsOpenViewFinished && bIsGraphicsSupported && bIsSeamlessSupported)
            toggleFullscreenMode(true, true);
    }
#endif

    /* Check the GA version only in case of additions are active: */
    if (!bIsActive)
        return;

    /* Check the Guest Additions version and warn the user about possible compatibility issues in case if the installed version is outdated. */
    QString strVersion = guest.GetAdditionsVersion();
    uint uVersion = strVersion.toUInt();
    /** @todo r=bird: This isn't want we want! We want the VirtualBox version of the additions, all three numbers. See @bugref{4084}.*/
    QString strRealVersion = QString("%1.%2").arg(RT_HIWORD(uVersion)).arg(RT_LOWORD(uVersion));
    QString strExpectedVersion = QString("%1.%2").arg(VMMDEV_VERSION_MAJOR).arg(VMMDEV_VERSION_MINOR);
    if (RT_HIWORD(uVersion) < VMMDEV_VERSION_MAJOR)
    {
        vboxProblem().warnAboutTooOldAdditions(machineWindowWrapper()->machineWindow(), strRealVersion, strExpectedVersion);
    }
    else if (RT_HIWORD(uVersion) == VMMDEV_VERSION_MAJOR && RT_LOWORD(uVersion) <  VMMDEV_VERSION_MINOR)
    {
        vboxProblem().warnAboutOldAdditions(machineWindowWrapper()->machineWindow(), strRealVersion, strExpectedVersion);
    }
    else if (uVersion > VMMDEV_VERSION)
    {
        vboxProblem().warnAboutNewAdditions(machineWindowWrapper()->machineWindow(), strRealVersion, strExpectedVersion);
    }
}

void UIMachineLogic::sltToggleGuestAutoresize(bool /* bEnabled */)
{
    /* Do not process if window or view is missing! */
    if (!machineWindowWrapper() || !machineWindowWrapper()->machineView())
        return;

    // TODO: Enable/Disable guest-autoresize for machine view! */
}

void UIMachineLogic::sltAdjustWindow()
{
    /* Do not process if window or view is missing! */
    if (!machineWindowWrapper() || !machineWindowWrapper()->machineView())
        return;

    /* Exit maximized window state if actual: */
    if (machineWindowWrapper()->machineWindow()->isMaximized())
        machineWindowWrapper()->machineWindow()->showNormal();

    /* Normalize view's geometry: */
    machineWindowWrapper()->machineView()->normalizeGeometry(true);
}

void UIMachineLogic::sltToggleMouseIntegration(bool aOff)
{
    /* Do not process if window or view is missing! */
    if (!machineWindowWrapper() || !machineWindowWrapper()->machineView())
        return;

    /* Disable/Enable mouse-integration for view: */
    machineWindowWrapper()->machineView()->setMouseIntegrationEnabled(!aOff);
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
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    /* Remember the paused state. */
    bool bWasPaused = m_machineState == KMachineState_Paused;
    if (!bWasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!pause())
            return;
    }

    CMachine machine = session().GetMachine();

    VBoxTakeSnapshotDlg dlg(machineWindowWrapper()->machineWindow(), machine);

    QString strTypeId = machine.GetOSTypeId();
    dlg.mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(strTypeId));

    /* Search for the max available filter index. */
    QString strNameTemplate = tr("Snapshot %1");
    int iMaxSnapshotIndex = searchMaxSnapshotIndex(machine, machine.GetSnapshot(QString()), strNameTemplate);
    dlg.mLeName->setText(strNameTemplate.arg(++ iMaxSnapshotIndex));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole console = session().GetConsole();

        CProgress progress = console.TakeSnapshot(dlg.mLeName->text().trimmed(), dlg.mTeDescription->toPlainText());

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog */
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindowWrapper()->machineWindow(), 0);

            if (progress.GetResultCode() != 0)
                vboxProblem().cannotTakeSnapshot(progress);
        }
        else
            vboxProblem().cannotTakeSnapshot(console);
    }

    /* Restore the running state if needed. */
    if (!bWasPaused)
        unpause();
}

void UIMachineLogic::sltShowInformationDialog()
{
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    // TODO: Call for singleton information dialog for this machine!
    //VBoxVMInformationDlg::createInformationDlg(session(), machineWindowWrapper()->machineWindow());
}

void UIMachineLogic::sltReset()
{
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    /* Confirm/Reset current console: */
    if (vboxProblem().confirmVMReset(machineWindowWrapper()->machineWindow()))
        session().GetConsole().Reset();
}

void UIMachineLogic::sltPause(bool aOn)
{
    pause(aOn);
}

void UIMachineLogic::sltACPIShutdown()
{
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
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    /* Close machine window: */
    machineWindowWrapper()->machineWindow()->close();
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
            bool currentUsed = false;
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
                    if (!currentUsed && !currentMedium.isNull() && mediumsToBeShown == maxMediumsToBeShown - 1)
                        medium = currentMedium;

                    if (medium.GetId() == currentId)
                        currentUsed = true;

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
                    callVMMAction->setText(tr("More CD/DVD Images..."));
                    unmountMediumAction->setText(tr("Unmount CD/DVD Device"));
                    unmountMediumAction->setIcon(VBoxGlobal::iconSet(":/cd_unmount_16px.png",
                                                                     ":/cd_unmount_dis_16px.png"));
                    break;
                case VBoxDefs::MediumType_Floppy:
                    callVMMAction->setText(tr("More Floppy Images..."));
                    unmountMediumAction->setText(tr("Unmount Floppy Device"));
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
                pEmptyMenuAction->setText(tr("No CD/DVD Devices Attached"));
                pEmptyMenuAction->setToolTip(tr("No CD/DVD devices attached to that VM"));
                break;
            case VBoxDefs::MediumType_Floppy:
                pEmptyMenuAction->setText(tr("No Floppy Devices Attached"));
                pEmptyMenuAction->setToolTip(tr("No floppy devices attached to that VM"));
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
    bool selectWithMediaManager = target.type != VBoxDefs::MediumType_Invalid;

    /* Open Virtual Media Manager to select image id: */
    if (selectWithMediaManager)
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
        VBoxMediaManagerDlg dlg(machineWindowWrapper()->machineWindow());
        dlg.setup(target.type, true /* select? */, true /* refresh? */, machine, currentId, true, usedImages);
        if (dlg.exec() == QDialog::Accepted)
            newId = dlg.selectedId();
        else return;
    }
    /* Use medium which was sent: */
    else if (!target.id.isNull() && target.id != currentId)
        newId = target.id;

    bool mount = !newId.isEmpty();

    /* Remount medium to the predefined port/device: */
    bool wasMounted = false;
    machine.MountMedium(target.name, target.port, target.device, newId, false /* force */);
    if (machine.isOk())
        wasMounted = true;
    else
    {
        /* Ask for force remounting: */
        if (vboxProblem().cannotRemountMedium(machineWindowWrapper()->machineWindow(), machine, vboxGlobal().findMedium (mount ? newId : currentId), mount, true /* retry? */) == QIMessageBox::Ok)
        {
            /* Force remount medium to the predefined port/device: */
            machine.MountMedium(target.name, target.port, target.device, newId, true /* force */);
            if (machine.isOk())
                wasMounted = true;
            else
                vboxProblem().cannotRemountMedium(machineWindowWrapper()->machineWindow(), machine, vboxGlobal().findMedium (mount ? newId : currentId), mount, false /* retry? */);
        }
    }

    /* Save medium mounted at runtime */
    if (wasMounted && m_bIsAutoSaveMedia)
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
    QMenu *pUSBDevicesMenu = actionsPool()->action(UIActionIndex_Menu_USBDevices)->menu();
    AssertMsg(pMenu == pUSBDevicesMenu, ("This slot should only be called on hovering USB menu!\n"));
    pMenu->clear();

    /* Get HOST: */
    CHost host = vboxGlobal().virtualBox().GetHost();

    /* Get USB devices list: */
    CHostUSBDeviceVector devices = host.GetUSBDevices();

    /* Fill USB devices menu: */
    bool bIsUSBListEmpty = devices.size() == 0;
    if (bIsUSBListEmpty)
    {
        /* Fill USB devices menu: */
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        pEmptyMenuAction->setText(tr("No USB Devices Connected"));
        pEmptyMenuAction->setIcon(VBoxGlobal::iconSet(":/delete_16px.png", ":/delete_dis_16px.png"));
        pEmptyMenuAction->setToolTip(tr("No supported devices connected to the host PC"));
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
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    /* Show network settings dialog: */
    UINetworkAdaptersDialog dlg(machineWindowWrapper()->machineWindow(), session());
    dlg.exec();
}

void UIMachineLogic::sltOpenSharedFoldersDialog()
{
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    /* Show shared folders settings dialog: */
    UISharedFoldersDialog dlg(machineWindowWrapper()->machineWindow(), session());
    dlg.exec();
}

void UIMachineLogic::sltSwitchVrdp(bool aOn)
{
    /* Enable VRDP server if possible: */
    CVRDPServer server = session().GetMachine().GetVRDPServer();
    AssertMsg(!server.isNull(), ("VRDP server should not be null!\n"));
    server.SetEnabled(aOn);
}

void UIMachineLogic::sltInstallGuestAdditions()
{
    /* Do not process if window is missing! */
    if (!machineWindowWrapper())
        return;

    char strAppPrivPath[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch(strAppPrivPath, sizeof(strAppPrivPath));
    AssertRC (rc);

    QString strSrc1 = QString(strAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString strSrc2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";

    /* Check the standard image locations */
    if (QFile::exists(strSrc1))
        return installGuestAdditionsFrom(strSrc1);
    else if (QFile::exists(strSrc2))
        return installGuestAdditionsFrom(strSrc2);

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
            return installGuestAdditionsFrom(path);
    }

#if 0 // TODO: Rework additions downloader logic...
    /* Download the required image */
    int result = vboxProblem().cannotFindGuestAdditions(QDir::toNativeSeparators(strSrc1), QDir::toNativeSeparators(strSrc2));
    if (result == QIMessageBox::Yes)
    {
        QString source = QString("http://download.virtualbox.org/virtualbox/%1/").arg(vbox.GetVersion().remove("_OSE")) + name;
        QString target = QDir(vboxGlobal().virtualBox().GetHomeFolder()).absoluteFilePath(name);

        //UIAdditionsDownloader *pdl = new UIAdditionsDownloader(source, target, mDevicesInstallGuestToolsAction);
        //machineWindowWrapper()->statusBar()->addWidget(pdl, 0);
        //pdl->start();
    }
#endif
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
        m_dbgGuiVT->pfnShowStatistics(m_dbgGui);
}

void UIMachineLogic::sltShowDebugCommandLine()
{
    if (dbgCreated())
        m_dbgGuiVT->pfnShowCommandLine(m_dbgGui);
}

void UIMachineLogic::sltLoggingToggled(bool bState)
{
    NOREF(bState);
    CConsole console = session().GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
            cdebugger.SetLogEnabled(bState);
    }
}
#endif

void UIMachineLogic::sltMouseStateChanged(int iState)
{
    actionsPool()->action(UIActionIndex_Toggle_MouseIntegration)->setEnabled(iState & UIMouseStateType_MouseAbsolute);
}

bool UIMachineLogic::pause(bool bOn)
{
    if (isPaused() == bOn)
        return true;

    CConsole console = session().GetConsole();

    if (bOn)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (bOn)
            vboxProblem().cannotPauseMachine(console);
        else
            vboxProblem().cannotResumeMachine(console);
    }

    return ok;
}

void UIMachineLogic::installGuestAdditionsFrom(const QString &strSource)
{
    CMachine machine = session().GetMachine();
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strUuid;

    CMedium image = vbox.FindDVDImage(strSource);
    if (image.isNull())
    {
        image = vbox.OpenDVDImage(strSource, strUuid);
        if (vbox.isOk())
            strUuid = image.GetId();
    }
    else
        strUuid = image.GetId();

    if (!vbox.isOk())
    {
        vboxProblem().cannotOpenMedium(machineWindowWrapper()->machineWindow(), vbox, VBoxDefs::MediumType_DVD, strSource);
        return;
    }

    AssertMsg(!strUuid.isNull(), ("Guest Additions image UUID should be valid!\n"));

    QString strCntName;
    LONG iCntPort = -1, iCntDevice = -1;
    /* Searching for the first suitable slot */
    {
        CStorageControllerVector controllers = machine.GetStorageControllers();
        int i = 0;
        while (i < controllers.size() && strCntName.isNull())
        {
            CStorageController controller = controllers[i];
            CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
            int j = 0;
            while (j < attachments.size() && strCntName.isNull())
            {
                CMediumAttachment attachment = attachments[j];
                if (attachment.GetType() == KDeviceType_DVD)
                {
                    strCntName = controller.GetName();
                    iCntPort = attachment.GetPort();
                    iCntDevice = attachment.GetDevice();
                }
                ++ j;
            }
            ++ i;
        }
    }

    if (!strCntName.isNull())
    {
        bool isMounted = false;

        /* Mount medium to the predefined port/device */
        machine.MountMedium(strCntName, iCntPort, iCntDevice, strUuid, false /* force */);
        if (machine.isOk())
            isMounted = true;
        else
        {
            /* Ask for force mounting */
            if (vboxProblem().cannotRemountMedium(machineWindowWrapper()->machineWindow(), machine, VBoxMedium(image, VBoxDefs::MediumType_DVD),
                                                  true /* mount? */, true /* retry? */) == QIMessageBox::Ok)
            {
                /* Force mount medium to the predefined port/device */
                machine.MountMedium(strCntName, iCntPort, iCntDevice, strUuid, true /* force */);
                if (machine.isOk())
                    isMounted = true;
                else
                    vboxProblem().cannotRemountMedium(machineWindowWrapper()->machineWindow(), machine, VBoxMedium(image, VBoxDefs::MediumType_DVD),
                                                      true /* mount? */, false /* retry? */);
            }
        }

        /* Save medium mounted at runtime */
        if (isMounted && m_bIsAutoSaveMedia)
        {
            machine.SaveSettings();
            if (!machine.isOk())
                vboxProblem().cannotSaveMachineSettings(machine);
        }
    }
    else
        vboxProblem().cannotMountGuestAdditions(machine.GetName());
}

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
    if (m_dbgGui)
        return true;

    RTLDRMOD hLdrMod = vboxGlobal().getDebuggerModule();
    if (hLdrMod == NIL_RTLDRMOD)
        return false;

    PFNDBGGUICREATE pfnGuiCreate;
    int rc = RTLdrGetSymbol (hLdrMod, "DBGGuiCreate", (void**) &pfnGuiCreate);
    if (RT_SUCCESS (rc))
    {
        ISession *pISession = session().raw();
        rc = pfnGuiCreate (pISession, &m_dbgGui, &m_dbgGuiVT);
        if (RT_SUCCESS (rc))
        {
            if (DBGGUIVT_ARE_VERSIONS_COMPATIBLE (m_dbgGuiVT->u32Version, DBGGUIVT_VERSION) ||
                m_dbgGuiVT->u32EndVersion == m_dbgGuiVT->u32Version)
            {
                m_dbgGuiVT->pfnSetParent (m_dbgGui, (QWidget*) machineWindowWrapper());
                m_dbgGuiVT->pfnSetMenu (m_dbgGui, (QMenu*) actionsPool()->action(UIActionIndex_Menu_Debug));
                dbgAdjustRelativePos();
                return true;
            }

            LogRel (("DBGGuiCreate failed, incompatible versions (loaded %#x/%#x, expected %#x)\n",
                     m_dbgGuiVT->u32Version, m_dbgGuiVT->u32EndVersion, DBGGUIVT_VERSION));
        }
        else
            LogRel (("DBGGuiCreate failed, rc=%Rrc\n", rc));
    }
    else
        LogRel (("RTLdrGetSymbol(,\"DBGGuiCreate\",) -> %Rrc\n", rc));

    m_dbgGui = 0;
    m_dbgGuiVT = 0;
    return false;
}

void UIMachineLogic::dbgDestroy()
{
    if (m_dbgGui)
    {
        m_dbgGuiVT->pfnDestroy(m_dbgGui);
        m_dbgGui = 0;
        m_dbgGuiVT = 0;
    }
}

void UIMachineLogic::dbgAdjustRelativePos()
{
    if (m_dbgGui)
    {
        QRect rct = machineWindowWrapper()->machineWindow()->frameGeometry();
        m_dbgGuiVT->pfnAdjustRelativePos(m_dbgGui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}
#endif

#if 0 // TODO: Where to move that?
# ifdef Q_WS_MAC
void UIMachineLogic::fadeToBlack()
{
    /* Fade to black */
    CGAcquireDisplayFadeReservation (kCGMaxDisplayReservationInterval, &mFadeToken);
    CGDisplayFade (mFadeToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, true);
}
void UIMachineLogic::fadeToNormal()
{
    /* Fade back to the normal gamma */
    CGDisplayFade (mFadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
    CGReleaseDisplayFadeReservation (mFadeToken);
    mConsole->setMouseCoalescingEnabled (true);
}
# endif
bool UIMachineLogic::toggleFullscreenMode (bool aOn, bool aSeamless)
{
    /* Please note: For some platforms like the Mac, the calling order of the
     * functions in this methods is vital. So please be careful on changing
     * this. */

    QSize initialSize = size();
    if (aSeamless || mConsole->isAutoresizeGuestActive())
    {
        QRect screen = aSeamless ?
            QApplication::desktop()->availableGeometry (this) :
            QApplication::desktop()->screenGeometry (this);
        ULONG64 availBits = mSession.GetMachine().GetVRAMSize() /* vram */
                          * _1M /* mb to bytes */
                          * 8; /* to bits */
        ULONG guestBpp = mConsole->console().GetDisplay().GetBitsPerPixel();
        ULONG64 usedBits = (screen.width() /* display width */
                         * screen.height() /* display height */
                         * guestBpp
                         + _1M * 8) /* current cache per screen - may be changed in future */
                         * mSession.GetMachine().GetMonitorCount() /**< @todo fix assumption that all screens have same resolution */
                         + 4096 * 8; /* adapter info */
        if (aOn && (availBits < usedBits))
        {
            if (aSeamless)
            {
                vboxProblem().cannotEnterSeamlessMode (
                    screen.width(), screen.height(), guestBpp,
                    (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
                return false;
            }
            else
            {
                int result = vboxProblem().cannotEnterFullscreenMode (
                    screen.width(), screen.height(), guestBpp,
                    (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
                if (result == QIMessageBox::Cancel)
                    return false;
            }
        }
    }

    AssertReturn (mConsole, false);
    AssertReturn ((mHiddenChildren.empty() == aOn), false);
    AssertReturn ((aSeamless && mIsSeamless != aOn) ||
                  (!aSeamless && mIsFullscreen != aOn), false);
    if (aOn)
        AssertReturn ((aSeamless && !mIsFullscreen) ||
                      (!aSeamless && !mIsSeamless), false);

    if (aOn)
    {
        /* Take the toggle hot key from the menu item. Since
         * VBoxGlobal::extractKeyFromActionText gets exactly the
         * linked key without the 'Host+' part we are adding it here. */
        QString hotKey = QString ("Host+%1")
            .arg (VBoxGlobal::extractKeyFromActionText (aSeamless ?
                  mVmSeamlessAction->text() : mVmFullscreenAction->text()));

        Assert (!hotKey.isEmpty());

        /* Show the info message. */
        bool ok = aSeamless ?
            vboxProblem().confirmGoingSeamless (hotKey) :
            vboxProblem().confirmGoingFullscreen (hotKey);
        if (!ok)
            return false;
    }

#ifdef Q_WS_MAC
    if (!aSeamless)
    {
    }
#endif

    if (aSeamless)
    {
        /* Activate the auto-resize feature required for the seamless mode. */
        if (!mVmAutoresizeGuestAction->isChecked())
            mVmAutoresizeGuestAction->setChecked (true);

        /* Activate the mouse integration feature for the seamless mode. */
        if (mVmDisableMouseIntegrAction->isChecked())
            mVmDisableMouseIntegrAction->setChecked (false);

        mVmAdjustWindowAction->setEnabled (!aOn);
        mVmFullscreenAction->setEnabled (!aOn);
        mVmAutoresizeGuestAction->setEnabled (!aOn);
        mVmDisableMouseIntegrAction->setEnabled (!aOn);

        mConsole->console().GetDisplay().SetSeamlessMode (aOn);
        mIsSeamless = aOn;
    }
    else
    {
        mIsFullscreen = aOn;
        mVmAdjustWindowAction->setEnabled (!aOn);
        mVmSeamlessAction->setEnabled (!aOn && m_bIsSeamlessSupported && m_bIsGraphicsSupported);
    }

    bool wasHidden = isHidden();

    /* Temporarily disable the mode-related action to make sure
     * user can not leave the mode before he enter it and inside out. */
    aSeamless ? mVmSeamlessAction->setEnabled (false) :
                mVmFullscreenAction->setEnabled (false);

    /* Calculate initial console size */
    QSize consoleSize;

    if (aOn)
    {
        consoleSize = mConsole->frameSize();
        consoleSize -= QSize (mConsole->frameWidth() * 2, mConsole->frameWidth() * 2);

        /* Toggle console to manual resize mode. */
        mConsole->setIgnoreMainwndResize (true);

        /* Memorize the maximized state. */
        QDesktopWidget *dtw = QApplication::desktop();
        mWasMax = isWindowMaximized() &&
                  dtw->availableGeometry().width()  == frameSize().width() &&
                  dtw->availableGeometry().height() == frameSize().height();

        /* Save the previous scroll-view minimum size before entering
         * fullscreen/seamless state to restore this minimum size before
         * the exiting fullscreen. Required for correct scroll-view and
         * guest display update in SDL mode. */
        mPrevMinSize = mConsole->minimumSize();
        mConsole->setMinimumSize (0, 0);

        /* let the widget take the whole available desktop space */
        QRect scrGeo = aSeamless ?
            dtw->availableGeometry (this) : dtw->screenGeometry (this);

        /* It isn't guaranteed that the guest os set the video mode that
         * we requested. So after all the resizing stuff set the clipping
         * mask and the spacing shifter to the corresponding values. */
        if (aSeamless)
            setViewInSeamlessMode (scrGeo);

#ifdef Q_WS_WIN
        mPrevRegion = dtw->screenGeometry (this);
#endif

        /* Hide all but the central widget containing the console view. */
        QList <QWidget*> list (findChildren <QWidget*> ());
        QList <QWidget*> excludes;
        excludes << centralWidget() << centralWidget()->findChildren <QWidget*> ();
        foreach (QWidget *w, list)
        {
            if (!excludes.contains (w))
            {
                if (!w->isHidden())
                {
                    w->hide();
                    mHiddenChildren.append (w);
                }
            }
        }

        /* Adjust colors and appearance. */
        mErasePalette = centralWidget()->palette();
        QPalette palette(mErasePalette);
        palette.setColor (centralWidget()->backgroundRole(), Qt::black);
        centralWidget()->setPalette (palette);
        centralWidget()->setAutoFillBackground (!aSeamless);
        mConsoleStyle = mConsole->frameStyle();
        mConsole->setFrameStyle (QFrame::NoFrame);
        mConsole->setMaximumSize (scrGeo.size());
        mConsole->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        mConsole->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    }
    else
    {
        /* Reset the shifting spacers. */
        mShiftingSpacerLeft->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerTop->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerRight->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerBottom->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

        /* Restore the previous scroll-view minimum size before the exiting
         * fullscreen. Required for correct scroll-view and guest display
         * update in SDL mode. */
        mConsole->setMinimumSize (mPrevMinSize);

#ifdef Q_WS_MAC
        if (aSeamless)
        {
            /* Please note: All the stuff below has to be done before the
             * window switch back to normal size. Qt changes the winId on the
             * fullscreen switch and make this stuff useless with the old
             * winId. So please be careful on rearrangement of the method
             * calls. */
            /* Undo all mac specific installations */
            ::darwinSetShowsWindowTransparent (this, false);
        }
#endif

        /* Adjust colors and appearance. */
        clearMask();
        centralWidget()->setPalette (mErasePalette);
        centralWidget()->setAutoFillBackground (false);
        mConsole->setFrameStyle (mConsoleStyle);
        mConsole->setMaximumSize (mConsole->sizeHint());
        mConsole->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        mConsole->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);

        /* Show everything hidden when going fullscreen. */
        foreach (QPointer <QWidget> child, mHiddenChildren)
            if (child) child->show();
        mHiddenChildren.clear();
    }

    /* Set flag for waiting host resize if it awaited during mode entering */
    if ((mIsFullscreen || mIsSeamless) && (consoleSize != initialSize))
        mIsWaitingModeResize = true;

    if (!aOn)
    {
        /* Animation takes a bit long, the mini toolbar is still disappearing
         * when switched to normal mode so hide it completely */
        mMiniToolBar->hide();
        mMiniToolBar->updateDisplay (false, true);
    }

    /* Toggle qt full-screen mode */
    switchToFullscreen (aOn, aSeamless);

    if (aOn)
    {
        mMiniToolBar->setSeamlessMode (aSeamless);
        mMiniToolBar->updateDisplay (true, true);
    }

#ifdef Q_WS_MAC
    if (aOn && aSeamless)
    {
        /* Please note: All the stuff below has to be done after the window has
         * switched to fullscreen. Qt changes the winId on the fullscreen
         * switch and make this stuff useless with the old winId. So please be
         * careful on rearrangement of the method calls. */
        ::darwinSetShowsWindowTransparent (this, true);
    }
#endif

    /* Send guest size hint */
    mConsole->toggleFSMode (consoleSize);

    /* Process all console attributes changes and sub-widget hidings */
    qApp->processEvents();

    if (!mIsWaitingModeResize)
        onExitFullscreen();

    /* Unlock FS actions locked during modes toggling */
    QTimer::singleShot (300, this, SLOT (unlockActionsSwitch()));

#ifdef Q_WS_MAC /* wasHidden is wrong on the mac it seems. */
    /** @todo figure out what is really wrong here... */
    if (!wasHidden)
        show();
#else
    if (wasHidden)
        hide();
#endif
    return true;
}
void UIMachineLogic::switchToFullscreen (bool aOn, bool aSeamless)
{
#ifdef Q_WS_MAC
# ifndef QT_MAC_USE_COCOA
    /* setWindowState removes the window group connection somehow. So save it
     * temporary. */
    WindowGroupRef g = GetWindowGroup (::darwinToNativeWindow (this));
# endif  /* !QT_MAC_USE_COCOA */
    if (aSeamless)
        if (aOn)
        {
            /* Save for later restoring */
            mNormalGeometry = geometry();
            mSavedFlags = windowFlags();
            /* Remove the frame from the window */
            const QRect fullscreen (qApp->desktop()->screenGeometry (qApp->desktop()->screenNumber (this)));
            setParent (0, Qt::Window | Qt::FramelessWindowHint | (windowFlags() & 0xffff0000));
            setGeometry (fullscreen);
            /* Set it maximized */
            setWindowState (windowState() ^ Qt::WindowMaximized);
        }
        else
        {
            /* Restore old values */
            setParent (0, mSavedFlags);
            setGeometry (mNormalGeometry);
        }
    else
    {
        /* Here we are going really fullscreen */
        setWindowState (windowState() ^ Qt::WindowFullScreen);
        sltChangePresentationMode (VBoxChangePresentationModeEvent(aOn));
    }

# ifndef QT_MAC_USE_COCOA
    /* Reassign the correct window group. */
    SetWindowGroup (::darwinToNativeWindow (this), g);
# endif /* !QT_MAC_USE_COCOA */
#else
    NOREF (aOn);
    NOREF (aSeamless);
    setWindowState (windowState() ^ Qt::WindowFullScreen);
#endif
}
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
