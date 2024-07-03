/* $Id$ */
/** @file
 * VBox Qt GUI - UICommon class implementation.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QLocale>
#include <QSessionManager>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleOptionSpinBox>
#include <QThread>
#include <QUrl>
#ifdef VBOX_WS_WIN
# include <QSettings>
# include <QStyleFactory>
#endif
#ifdef VBOX_GUI_WITH_PIDFILE
# include <QTextStream>
#endif

/* GUI includes: */
#include "UICommon.h"
#include "UIConverter.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIGlobalSession.h"
#include "UIGuestOSType.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UILocalMachineStuff.h"
#include "UILoggingDefs.h"
#include "UIMediumEnumerator.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UINotificationCenter.h"
#include "UIPopupCenter.h"
#include "UIShortcutPool.h"
#include "UIThreadPool.h"
#include "UITranslator.h"
#include "UITranslationEventListener.h"
#include "UIVersion.h"
#include "UIVirtualBoxClientEventHandler.h"
#include "UIVirtualBoxEventHandler.h"
#ifdef VBOX_WS_MAC
# include "UICocoaApplication.h"
#endif
#ifdef VBOX_WS_WIN
# include "VBoxUtils-win.h"
#endif
#ifdef VBOX_WS_NIX
# include "UIHostComboEditor.h"
#endif
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UINetworkRequestManager.h"
# include "UIUpdateManager.h"
#endif

/* COM includes: */
#include "CCloudMachine.h"
#include "CHostUSBDevice.h"
#include "CHostVideoInputDevice.h"
#include "CMachine.h"
#include "CSystemProperties.h"
#include "CUSBDevice.h"
#include "CUSBDeviceFilter.h"

/* Other VBox includes: */
#include <iprt/ctype.h> /* for RT_C_IS_CNTRL */
#include <iprt/path.h>
#include <iprt/stream.h>
#include <VBox/vd.h>
#include <VBox/com/Guid.h>
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <iprt/env.h> /* for RTEnvExist & RTENV_DEFAULT */
# include <iprt/ldr.h>
# include <VBox/sup.h> /* for SUPR3HardenedLdrLoadAppPriv */
#endif

/* VirtualBox interface declarations: */
#include <VBox/com/VirtualBox.h>

/* External includes: */
#ifdef VBOX_WS_MAC
# include <sys/utsname.h>
#endif
#ifdef VBOX_WS_NIX
# include <xcb/xcb.h>
#endif

/* Namespaces: */
using namespace UIExtraDataDefs;


/* static */
UICommon *UICommon::s_pInstance = 0;

/* static */
void UICommon::create(UIType enmType)
{
    /* Make sure instance is NOT created yet: */
    AssertReturnVoid(!s_pInstance);

    /* Create instance: */
    new UICommon(enmType);
    /* Prepare instance: */
    s_pInstance->prepare();
}

/* static */
void UICommon::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    AssertPtrReturnVoid(s_pInstance);

    /* Cleanup instance:
     * 1. By default, automatically on QApplication::aboutToQuit() signal.
     * 2. But if QApplication was not started at all and we perform
     *    early shutdown, we should do cleanup ourselves. */
    if (s_pInstance->isValid())
        s_pInstance->cleanup();
    /* Destroy instance: */
    delete s_pInstance;
}

UICommon::UICommon(UIType enmType)
    : m_enmType(enmType)
    , m_fValid(false)
    , m_fCleaningUp(false)
#ifdef VBOX_WS_WIN
    , m_fDataCommitted(false)
#endif
#ifdef VBOX_WS_MAC
    , m_enmMacOSVersion(MacOSXRelease_Old)
#endif
#ifdef VBOX_WS_NIX
    , m_enmWindowManagerType(X11WMType_Unknown)
    , m_fCompositingManagerRunning(false)
    , m_enmDisplayServerType(VBGHDISPLAYSERVERTYPE_NONE)
#endif
    , m_fDarkMode(false)
    , m_fSeparateProcess(false)
    , m_fShowStartVMErrors(true)
#if defined(DEBUG_bird)
    , m_fAgressiveCaching(false)
#else
    , m_fAgressiveCaching(true)
#endif
    , m_fRestoreCurrentSnapshot(false)
    , m_fNoKeyboardGrabbing(false)
    , m_fExecuteAllInIem(false)
    , m_uWarpPct(100)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , m_fDbgEnabled(0)
    , m_fDbgAutoShow(0)
    , m_fDbgAutoShowCommandLine(0)
    , m_fDbgAutoShowStatistics(0)
    , m_hVBoxDbg(NIL_RTLDRMOD)
    , m_enmLaunchRunning(LaunchRunning_Default)
#endif
    , m_fSettingsPwSet(false)
    , m_pThreadPool(0)
    , m_pThreadPoolCloud(0)
{
    /* Assign instance: */
    s_pInstance = this;
}

UICommon::~UICommon()
{
    /* Unassign instance: */
    s_pInstance = 0;
}

void UICommon::prepare()
{
    /* Make sure QApplication cleanup us on exit: */
#ifndef VBOX_IS_QT6_OR_LATER /** @todo qt6: ... */
    qApp->setFallbackSessionManagementEnabled(false);
#endif
    connect(qApp, &QGuiApplication::aboutToQuit,
            this, &UICommon::sltCleanup);
#ifndef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* Make sure we handle host OS session shutdown as well: */
    connect(qApp, &QGuiApplication::commitDataRequest,
            this, &UICommon::sltHandleCommitDataRequest);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */

#ifdef VBOX_WS_MAC
    /* Determine OS release early: */
    m_enmMacOSVersion = determineOsRelease();
#endif

#ifdef VBOX_WS_NIX
    /* Detect display server type: */
    m_enmDisplayServerType = VBGHDisplayServerTypeDetect();
#endif

    /* Create converter: */
    UIConverter::create();

    /* Create desktop-widget watchdog: */
    UIDesktopWidgetWatchdog::create();

    /* Create message-center: */
    UIMessageCenter::create();
    /* Create popup-center: */
    UIPopupCenter::create();

    /* Prepare general icon-pool: */
    UIIconPoolGeneral::create();

    /* Load translation based on the current locale: */
    UITranslator::loadLanguage();

    /* Init COM: */
    UIGlobalSession::create();
    if (!gpGlobalSession->prepare())
        return;
    connect(gpGlobalSession, &UIGlobalSession::sigVBoxSVCAvailabilityChange,
            this, &UICommon::sltHandleVBoxSVCAvailabilityChange);

    /* Create extra-data manager right after COM init: */
    UIExtraDataManager::create();

    /* Prepare thread-pool instances: */
    m_pThreadPool = new UIThreadPool(3 /* worker count */, 5000 /* worker timeout */);
    m_pThreadPoolCloud = new UIThreadPool(2 /* worker count */, 1000 /* worker timeout */);

    /* Load whether host OS is in Dark mode: */
#if defined(VBOX_WS_MAC)
    m_fDarkMode = UICocoaApplication::instance()->isDarkMode();
#elif defined(VBOX_WS_WIN)
    m_fDarkMode = isWindowsInDarkMode();
#else /* Linux, BSD, Solaris */
    m_fDarkMode = isPaletteInDarkMode();
#endif /* Linux, BSD, Solaris */
    /* Load color theme: */
    loadColorTheme();

    /* Load translation based on the user settings: */
    QString strLanguageId = gEDataManager->languageId();
    if (!strLanguageId.isNull())
        UITranslator::loadLanguage(strLanguageId);

    retranslateUi();

    /* Create translation event listener instance: */
    UITranslationEventListener::create();

    connect(gEDataManager, &UIExtraDataManager::sigLanguageChange,
            this, &UICommon::sltGUILanguageChange);
    connect(gEDataManager, &UIExtraDataManager::sigFontScaleFactorChanged,
            this, &UICommon::sltHandleFontScaleFactorChanged);

    qApp->installEventFilter(this);

    /* process command line */

    UIVisualStateType visualStateType = UIVisualStateType_Invalid;

#ifdef VBOX_WS_NIX
    /* Check whether we have compositing manager running: */
    m_fCompositingManagerRunning = NativeWindowSubsystem::isCompositingManagerRunning(X11ServerAvailable());

    /* Acquire current Window Manager type: */
    m_enmWindowManagerType = NativeWindowSubsystem::windowManagerType(X11ServerAvailable());
#endif /* VBOX_WS_NIX */

#ifdef VBOX_WITH_DEBUGGER_GUI
# ifdef VBOX_WITH_DEBUGGER_GUI_MENU
    initDebuggerVar(&m_fDbgEnabled, "VBOX_GUI_DBG_ENABLED", GUI_Dbg_Enabled, true);
# else
    initDebuggerVar(&m_fDbgEnabled, "VBOX_GUI_DBG_ENABLED", GUI_Dbg_Enabled, false);
# endif
    initDebuggerVar(&m_fDbgAutoShow, "VBOX_GUI_DBG_AUTO_SHOW", GUI_Dbg_AutoShow, false);
    m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = m_fDbgAutoShow;
#endif

    /*
     * Parse the command line options.
     *
     * This is a little sloppy but we're trying to tighten it up.  Unfortuately,
     * both on X11 and darwin (IIRC) there might be additional arguments aimed
     * for client libraries with GUI processes.  So, using RTGetOpt or similar
     * is a bit hard since we have to cope with unknown options.
     */
    m_fShowStartVMErrors = true;
    bool startVM = false;
    bool fSeparateProcess = false;
    QString vmNameOrUuid;

    const QStringList &arguments = QCoreApplication::arguments();
    const int argc = arguments.size();
    int i = 1;
    while (i < argc)
    {
        const QByteArray &argBytes = arguments.at(i).toUtf8();
        const char *arg = argBytes.constData();
        enum { OptType_Unknown, OptType_VMRunner, OptType_VMSelector, OptType_MaybeBoth } enmOptType = OptType_Unknown;
        /* NOTE: the check here must match the corresponding check for the
         * options to start a VM in main.cpp and hardenedmain.cpp exactly,
         * otherwise there will be weird error messages. */
        if (   !::strcmp(arg, "--startvm")
            || !::strcmp(arg, "-startvm"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
            {
                vmNameOrUuid = arguments.at(i);
                startVM = true;
            }
        }
        else if (!::strcmp(arg, "-separate") || !::strcmp(arg, "--separate"))
        {
            enmOptType = OptType_VMRunner;
            fSeparateProcess = true;
        }
#ifdef VBOX_GUI_WITH_PIDFILE
        else if (!::strcmp(arg, "-pidfile") || !::strcmp(arg, "--pidfile"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
                m_strPidFile = arguments.at(i);
        }
#endif /* VBOX_GUI_WITH_PIDFILE */
        /* Visual state type options: */
        else if (!::strcmp(arg, "-normal") || !::strcmp(arg, "--normal"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Normal;
        }
        else if (!::strcmp(arg, "-fullscreen") || !::strcmp(arg, "--fullscreen"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Fullscreen;
        }
        else if (!::strcmp(arg, "-seamless") || !::strcmp(arg, "--seamless"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Seamless;
        }
        else if (!::strcmp(arg, "-scale") || !::strcmp(arg, "--scale"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Scale;
        }
        /* Passwords: */
        else if (!::strcmp(arg, "--settingspw"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
            {
                RTStrCopy(m_astrSettingsPw, sizeof(m_astrSettingsPw), arguments.at(i).toLocal8Bit().constData());
                m_fSettingsPwSet = true;
            }
        }
        else if (!::strcmp(arg, "--settingspwfile"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
            {
                const QByteArray &argFileBytes = arguments.at(i).toLocal8Bit();
                const char *pszFile = argFileBytes.constData();
                bool fStdIn = !::strcmp(pszFile, "stdin");
                int vrc = VINF_SUCCESS;
                PRTSTREAM pStrm;
                if (!fStdIn)
                    vrc = RTStrmOpen(pszFile, "r", &pStrm);
                else
                    pStrm = g_pStdIn;
                if (RT_SUCCESS(vrc))
                {
                    size_t cbFile;
                    vrc = RTStrmReadEx(pStrm, m_astrSettingsPw, sizeof(m_astrSettingsPw) - 1, &cbFile);
                    if (RT_SUCCESS(vrc))
                    {
                        if (cbFile >= sizeof(m_astrSettingsPw) - 1)
                            cbFile = sizeof(m_astrSettingsPw) - 1;
                        unsigned i;
                        for (i = 0; i < cbFile && !RT_C_IS_CNTRL(m_astrSettingsPw[i]); i++)
                            ;
                        m_astrSettingsPw[i] = '\0';
                        m_fSettingsPwSet = true;
                    }
                    if (!fStdIn)
                        RTStrmClose(pStrm);
                }
            }
        }
        /* Misc options: */
        else if (!::strcmp(arg, "-comment") || !::strcmp(arg, "--comment"))
        {
            enmOptType = OptType_MaybeBoth;
            ++i;
        }
        else if (!::strcmp(arg, "--no-startvm-errormsgbox"))
        {
            enmOptType = OptType_VMRunner;
            m_fShowStartVMErrors = false;
        }
        else if (!::strcmp(arg, "--aggressive-caching"))
        {
            enmOptType = OptType_MaybeBoth;
            m_fAgressiveCaching = true;
        }
        else if (!::strcmp(arg, "--no-aggressive-caching"))
        {
            enmOptType = OptType_MaybeBoth;
            m_fAgressiveCaching = false;
        }
        else if (!::strcmp(arg, "--restore-current"))
        {
            enmOptType = OptType_VMRunner;
            m_fRestoreCurrentSnapshot = true;
        }
        else if (!::strcmp(arg, "--no-keyboard-grabbing"))
        {
            enmOptType = OptType_VMRunner;
            m_fNoKeyboardGrabbing = true;
        }
        /* Ad hoc VM reconfig options: */
        else if (!::strcmp(arg, "--fda"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_uFloppyImage = QUuid(arguments.at(i));
        }
        else if (!::strcmp(arg, "--dvd") || !::strcmp(arg, "--cdrom"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_uDvdImage = QUuid(arguments.at(i));
        }
        /* VMM Options: */
        else if (!::strcmp(arg, "--execute-all-in-iem"))
        {
            enmOptType = OptType_VMRunner;
            m_fExecuteAllInIem = true;
        }
        else if (!::strcmp(arg, "--driverless"))
            enmOptType = OptType_VMRunner;
        else if (!::strcmp(arg, "--warp-pct"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_uWarpPct = RTStrToUInt32(arguments.at(i).toLocal8Bit().constData());
        }
#ifdef VBOX_WITH_DEBUGGER_GUI
        /* Debugger/Debugging options: */
        else if (!::strcmp(arg, "-dbg") || !::strcmp(arg, "--dbg"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
        }
        else if (!::strcmp( arg, "-debug") || !::strcmp(arg, "--debug"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, true);
            setDebuggerVar(&m_fDbgAutoShowStatistics, true);
        }
        else if (!::strcmp(arg, "--debug-command-line"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, true);
        }
        else if (!::strcmp(arg, "--debug-statistics"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowStatistics, true);
        }
        else if (!::strcmp(arg, "--statistics-expand") || !::strcmp(arg, "--stats-expand"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
            {
                if (!m_strDbgStatisticsExpand.isEmpty())
                    m_strDbgStatisticsExpand.append('|');
                m_strDbgStatisticsExpand.append(arguments.at(i));
            }
        }
        else if (!::strncmp(arg, RT_STR_TUPLE("--statistics-expand=")) || !::strncmp(arg, RT_STR_TUPLE("--stats-expand=")))
        {
            enmOptType = OptType_VMRunner;
            if (!m_strDbgStatisticsExpand.isEmpty())
                m_strDbgStatisticsExpand.append('|');
            m_strDbgStatisticsExpand.append(arguments.at(i).section('=', 1));
        }
        else if (!::strcmp(arg, "--statistics-filter") || !::strcmp(arg, "--stats-filter"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_strDbgStatisticsFilter = arguments.at(i);
        }
        else if (!::strncmp(arg, RT_STR_TUPLE("--statistics-filter=")) || !::strncmp(arg, RT_STR_TUPLE("--stats-filter=")))
        {
            enmOptType = OptType_VMRunner;
            m_strDbgStatisticsFilter = arguments.at(i).section('=', 1);
        }
        else if (!::strcmp(arg, "--statistics-config") || !::strcmp(arg, "--stats-config"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_strDbgStatisticsConfig = arguments.at(i);
        }
        else if (!::strncmp(arg, RT_STR_TUPLE("--statistics-config=")) || !::strncmp(arg, RT_STR_TUPLE("--stats-config=")))
        {
            enmOptType = OptType_VMRunner;
            m_strDbgStatisticsConfig = arguments.at(i).section('=', 1);
        }
        else if (!::strcmp(arg, "-no-debug") || !::strcmp(arg, "--no-debug"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, false);
            setDebuggerVar(&m_fDbgAutoShow, false);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, false);
            setDebuggerVar(&m_fDbgAutoShowStatistics, false);
        }
        /* Not quite debug options, but they're only useful with the debugger bits. */
        else if (!::strcmp(arg, "--start-paused"))
        {
            enmOptType = OptType_VMRunner;
            m_enmLaunchRunning = LaunchRunning_No;
        }
        else if (!::strcmp(arg, "--start-running"))
        {
            enmOptType = OptType_VMRunner;
            m_enmLaunchRunning = LaunchRunning_Yes;
        }
#endif
        if (enmOptType == OptType_VMRunner && m_enmType != UIType_RuntimeUI)
            msgCenter().cannotHandleRuntimeOption(arg);

        i++;
    }

    if (uiType() == UIType_RuntimeUI && startVM)
    {
        /* m_fSeparateProcess makes sense only if a VM is started. */
        m_fSeparateProcess = fSeparateProcess;

        /* Search for corresponding VM: */
        QUuid uuid = QUuid(vmNameOrUuid);
        const CVirtualBox comVBox = gpGlobalSession->virtualBox();
        const CMachine comMachine = comVBox.FindMachine(vmNameOrUuid);
        if (!uuid.isNull())
        {
            if (comMachine.isNull() && showStartVMErrors())
                return msgCenter().cannotFindMachineById(comVBox, uuid);
        }
        else
        {
            if (comMachine.isNull() && showStartVMErrors())
                return msgCenter().cannotFindMachineByName(comVBox, vmNameOrUuid);
        }
        m_uManagedVMId = comMachine.GetId();

        if (m_fSeparateProcess)
        {
            /* Create a log file for VirtualBoxVM process. */
            QString str = comMachine.GetLogFolder();
            com::Utf8Str logDir(str.toUtf8().constData());

            /* make sure the Logs folder exists */
            if (!RTDirExists(logDir.c_str()))
                RTDirCreateFullPath(logDir.c_str(), 0700);

            com::Utf8Str logFile = com::Utf8StrFmt("%s%cVBoxUI.log",
                                                   logDir.c_str(), RTPATH_DELIMITER);

            com::VBoxLogRelCreate("GUI (separate)", logFile.c_str(),
                                  RTLOGFLAGS_PREFIX_TIME_PROG | RTLOGFLAGS_RESTRICT_GROUPS,
                                  "all all.restrict -default.restrict",
                                  "VBOX_RELEASE_LOG", RTLOGDEST_FILE,
                                  32768 /* cMaxEntriesPerGroup */,
                                  0 /* cHistory */, 0 /* uHistoryFileTime */,
                                  0 /* uHistoryFileSize */, NULL);
        }
    }

    /* For Selector UI: */
    if (uiType() == UIType_ManagerUI)
    {
        /* We should create separate logging file for VM selector: */
        char szLogFile[RTPATH_MAX];
        const char *pszLogFile = NULL;
        com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        RTPathAppend(szLogFile, sizeof(szLogFile), "selectorwindow.log");
        pszLogFile = szLogFile;
        /* Create release logger, to file: */
        com::VBoxLogRelCreate("GUI VM Selector Window",
                              pszLogFile,
                              RTLOGFLAGS_PREFIX_TIME_PROG,
                              "all",
                              "VBOX_GUI_SELECTORWINDOW_RELEASE_LOG",
                              RTLOGDEST_FILE | RTLOGDEST_F_NO_DENY,
                              UINT32_MAX,
                              10,
                              60 * 60,
                              _1M,
                              NULL /*pErrInfo*/);

        LogRel(("Qt version: %s\n", UIVersionInfo::qtRTVersionString().toUtf8().constData()));
    }

    if (m_fSettingsPwSet)
    {
        CVirtualBox comVBox = gpGlobalSession->virtualBox();
        comVBox.SetSettingsSecret(m_astrSettingsPw);
    }

    if (visualStateType != UIVisualStateType_Invalid && !m_uManagedVMId.isNull())
        gEDataManager->setRequestedVisualState(visualStateType, m_uManagedVMId);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* For Runtime UI: */
    if (uiType() == UIType_RuntimeUI)
    {
        /* Setup the debugger GUI: */
        if (RTEnvExist("VBOX_GUI_NO_DEBUGGER"))
            m_fDbgEnabled = m_fDbgAutoShow =  m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = false;
        if (m_fDbgEnabled)
        {
            RTERRINFOSTATIC ErrInfo;
            RTErrInfoInitStatic(&ErrInfo);
            int vrc = SUPR3HardenedLdrLoadAppPriv("VBoxDbg", &m_hVBoxDbg, RTLDRLOAD_FLAGS_LOCAL, &ErrInfo.Core);
            if (RT_FAILURE(vrc))
            {
                m_hVBoxDbg = NIL_RTLDRMOD;
                m_fDbgAutoShow = m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = false;
                LogRel(("Failed to load VBoxDbg, rc=%Rrc - %s\n", vrc, ErrInfo.Core.pszMsg));
            }
        }
    }
#endif

    m_fValid = true;

    /* Create medium-enumerator but don't do any immediate caching: */
    UIMediumEnumerator::create();

    /* Create shortcut pool: */
    UIShortcutPool::create(uiType());

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Create network manager: */
    UINetworkRequestManager::create();

    /* Schedule update manager: */
    UIUpdateManager::schedule();
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

#ifdef RT_OS_LINUX
    /* Make sure no wrong USB mounted: */
    checkForWrongUSBMounted();
#endif /* RT_OS_LINUX */

    iOriginalFontPixelSize = qApp->font().pixelSize();
    iOriginalFontPointSize = qApp->font().pointSize();
    sltHandleFontScaleFactorChanged(gEDataManager->fontScaleFactor());
}

void UICommon::cleanup()
{
    LogRel(("GUI: UICommon: Handling aboutToQuit request..\n"));

    /// @todo Shouldn't that be protected with a mutex or something?
    /* Remember that the cleanup is in progress preventing any unwanted
     * stuff which could be called from the other threads: */
    m_fCleaningUp = true;

#ifdef VBOX_WS_WIN
    /* Ask listeners to commit data if haven't yet: */
    if (!m_fDataCommitted)
    {
        emit sigAskToCommitData();
        m_fDataCommitted = true;
    }
#else
    /* Ask listeners to commit data: */
    emit sigAskToCommitData();
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* For Runtime UI: */
    if (   uiType() == UIType_RuntimeUI
        && m_hVBoxDbg != NIL_RTLDRMOD)
    {
        RTLdrClose(m_hVBoxDbg);
        m_hVBoxDbg = NIL_RTLDRMOD;
    }
#endif

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Shutdown update manager: */
    UIUpdateManager::shutdown();

    /* Destroy network manager: */
    UINetworkRequestManager::destroy();
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Destroy shortcut pool: */
    UIShortcutPool::destroy();

#ifdef VBOX_GUI_WITH_PIDFILE
    deletePidfile();
#endif /* VBOX_GUI_WITH_PIDFILE */

    /* Destroy medium-enumerator: */
    UIMediumEnumerator::destroy();

    /* Destroy the global (VirtualBox and VirtualBoxClient) Main event
     * handlers which are used in both Manager and Runtime UIs. */
    UIVirtualBoxEventHandler::destroy();
    UIVirtualBoxClientEventHandler::destroy();

    /* Destroy the extra-data manager finally after everything
     * above which could use it already destroyed: */
    UIExtraDataManager::destroy();

    /* Destroy converter: */
    UIConverter::destroy();

    /* Cleanup thread-pools: */
    delete m_pThreadPool;
    m_pThreadPool = 0;
    delete m_pThreadPoolCloud;
    m_pThreadPoolCloud = 0;

    /* First, make sure we don't use COM any more: */
    emit sigAskToDetachCOM();

    /* Cleanup COM: */
    gpGlobalSession->cleanup();
    UIGlobalSession::destroy();

    /* Notify listener it can close UI now: */
    emit sigAskToCloseUI();

    /* Cleanup general icon-pool: */
    UIIconPoolGeneral::destroy();

    /* Destroy popup-center: */
    UIPopupCenter::destroy();
    /* Destroy message-center: */
    UIMessageCenter::destroy();

    /* Destroy desktop-widget watchdog: */
    UIDesktopWidgetWatchdog::destroy();

    /* Destroy translation event listener instance: */
    UITranslationEventListener::destroy();

    m_fValid = false;

    LogRel(("GUI: UICommon: aboutToQuit request handled!\n"));
}

#ifdef VBOX_WS_MAC
/* static */
MacOSXRelease UICommon::determineOsRelease()
{
    /* Prepare 'utsname' struct: */
    utsname info;
    if (uname(&info) != -1)
    {
        /* Cut the major release index of the string we have, s.a. 'man uname': */
        const int iRelease = QString(info.release).section('.', 0, 0).toInt();
        /* Check boundaries: */
        if (iRelease <= MacOSXRelease_FirstUnknown)
            return MacOSXRelease_Old;
        else if (iRelease >= MacOSXRelease_LastUnknown)
            return MacOSXRelease_New;
        else
            return (MacOSXRelease)iRelease;
    }
    /* Return 'Old' by default: */
    return MacOSXRelease_Old;
}
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_NIX
bool UICommon::X11ServerAvailable() const
{
    return VBGHDisplayServerTypeIsXAvailable(m_enmDisplayServerType);
}

VBGHDISPLAYSERVERTYPE UICommon::displayServerType() const
{
    return m_enmDisplayServerType;
}
#endif

QString UICommon::hostOperatingSystem() const
{
    const CHost comHost = gpGlobalSession->host();
    return comHost.GetOperatingSystem();
}

#if defined(VBOX_WS_MAC)
// Provided by UICocoaApplication ..

#elif defined(VBOX_WS_WIN)

bool UICommon::isWindowsInDarkMode() const
{
    /* Load saved color theme: */
    UIColorThemeType enmColorTheme = gEDataManager->colorTheme();

    /* Check whether we have dark system theme requested: */
    if (enmColorTheme == UIColorThemeType_Auto)
    {
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                           QSettings::NativeFormat);
        if (settings.value("AppsUseLightTheme") == 0)
            enmColorTheme = UIColorThemeType_Dark;
    }

    /* Return result: */
    return enmColorTheme == UIColorThemeType_Dark;
}

#else /* Linux, BSD, Solaris */

bool UICommon::isPaletteInDarkMode() const
{
    const QPalette pal = qApp->palette();
    const QColor background = pal.color(QPalette::Active, QPalette::Window);
    const double dLuminance = (0.299 * background.red() + 0.587 * background.green() + 0.114 * background.blue()) / 255;
    return dLuminance < 0.5;
}
#endif /* Linux, BSD, Solaris */

void UICommon::loadColorTheme()
{
#if defined (VBOX_WS_MAC)
    /* macOS has Window color hardcoded somewhere inside, Qt has no access to it,
     * moreover these colors are influenced by window background blending,
     * making Qt default colors incredibly inconsistent with native macOS apps. */

    /* Redefine colors for known OS types: */
    enum ColorSlot
    {
        ColorSlot_DarkActive,
        ColorSlot_DarkInactive,
        ColorSlot_DarkAlternate,
        ColorSlot_LightActive,
        ColorSlot_LightInactive,
        ColorSlot_LightAlternate,
    };
    QMap<ColorSlot, QColor> colors;
    switch (osRelease())
    {
        case MacOSXRelease_BigSur:
        {
            colors[ColorSlot_DarkActive] = QColor("#282628");
            colors[ColorSlot_DarkInactive] = QColor("#2E292E");
            colors[ColorSlot_LightActive] = QColor("#E7E2E3");
            colors[ColorSlot_LightInactive] = QColor("#EEE9EA");
            break;
        }
        case MacOSXRelease_Monterey:
        {
            colors[ColorSlot_DarkActive] = QColor("#252328");
            colors[ColorSlot_DarkInactive] = QColor("#2A2630");
            colors[ColorSlot_LightActive] = QColor("#E1DEE4");
            colors[ColorSlot_LightInactive] = QColor("#EEE8E9");
            break;
        }
        case MacOSXRelease_Ventura:
        {
            colors[ColorSlot_DarkActive] = QColor("#322827");
            colors[ColorSlot_DarkInactive] = QColor("#332A28");
            colors[ColorSlot_LightActive] = QColor("#E5E0DF");
            colors[ColorSlot_LightInactive] = QColor("#ECE7E5");
            break;
        }
        default:
            break;
    }
    /* Redefine colors common for various OS types: */
    // we do it only if we have redefined something above:
    if (!colors.isEmpty())
    {
        colors[ColorSlot_DarkAlternate] = QColor("#2F2A2F");
        colors[ColorSlot_LightAlternate] = QColor("#F4F5F5");
    }

    /* Do we have redefined colors? */
    if (!colors.isEmpty())
    {
        QPalette pal = qApp->palette();
        if (isInDarkMode())
        {
            pal.setColor(QPalette::Active, QPalette::Window, colors.value(ColorSlot_DarkActive));
            pal.setColor(QPalette::Inactive, QPalette::Window, colors.value(ColorSlot_DarkInactive));
            pal.setColor(QPalette::Active, QPalette::AlternateBase, colors.value(ColorSlot_DarkAlternate));
            pal.setColor(QPalette::Inactive, QPalette::AlternateBase, colors.value(ColorSlot_DarkAlternate));
        }
        else
        {
            pal.setColor(QPalette::Active, QPalette::Window, colors.value(ColorSlot_LightActive));
            pal.setColor(QPalette::Inactive, QPalette::Window, colors.value(ColorSlot_LightInactive));
            pal.setColor(QPalette::Active, QPalette::AlternateBase, colors.value(ColorSlot_LightAlternate));
            pal.setColor(QPalette::Inactive, QPalette::AlternateBase, colors.value(ColorSlot_LightAlternate));
        }
        qApp->setPalette(pal);
    }

#elif defined(VBOX_WS_WIN)

    /* For the Dark mode! */
    if (isInDarkMode())
    {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        QColor windowColor1 = QColor(59, 60, 61);
        QColor windowColor2 = QColor(63, 64, 65);
        QColor baseColor1 = QColor(46, 47, 48);
        QColor baseColor2 = QColor(56, 57, 58);
        QColor disabledColor = QColor(113, 114, 115);
        darkPalette.setColor(QPalette::Window, windowColor1);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, disabledColor);
        darkPalette.setColor(QPalette::Base, baseColor1);
        darkPalette.setColor(QPalette::AlternateBase, baseColor2);
        darkPalette.setColor(QPalette::PlaceholderText, disabledColor);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, windowColor2);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(179, 214, 242));
        darkPalette.setColor(QPalette::Highlight, QColor(29, 84, 92));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);
        qApp->setPalette(darkPalette);
        qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2b2b2b; border: 1px solid #737373; }");
    }

#else /* Linux, BSD, Solaris */

    /* For the Dark mode! */
    if (isInDarkMode())
    {
        // WORKAROUND:
        // Have seen it on Linux with Qt5 but still see it with Qt6.
        // In Dark themes on KDE (at least) PlaceholderText foreground
        // is indistinguishable on Base background.

        /* Acquire global palette: */
        QPalette darkPalette = qApp->palette();

        /* Get text base color: */
        const QColor base = darkPalette.color(QPalette::Active, QPalette::Base);

        /* Get possible foreground colors: */
        const QColor simpleText = darkPalette.color(QPalette::Active, QPalette::Text);
        const QColor placeholderText = darkPalette.color(QPalette::Active, QPalette::PlaceholderText);
        QColor lightText = simpleText.black() < placeholderText.black() ? simpleText : placeholderText;
        QColor darkText = simpleText.black() > placeholderText.black() ? simpleText : placeholderText;
        if (lightText.black() > 128)
            lightText = QColor(Qt::white);
        lightText = lightText.darker(150);
        if (darkText.black() < 128)
            darkText = QColor(Qt::black);
        darkText = darkText.lighter(150);

        /* Measure base luminance: */
        double dLuminance = (0.299 * base.red() + 0.587 * base.green() + 0.114 * base.blue()) / 255;

        /* Adjust color accordingly: */
        darkPalette.setColor(QPalette::Active, QPalette::PlaceholderText,
                             dLuminance > 0.5 ? darkText : lightText);

        /* Put palette back: */
        qApp->setPalette(darkPalette);
    }

#endif /* Linux, BSD, Solaris */
}

bool UICommon::processArgs()
{
    /* Among those arguments: */
    bool fResult = false;
    const QStringList args = qApp->arguments();

    /* We are looking for a list of file URLs passed to the executable: */
    QList<QUrl> listArgUrls;
    for (int i = 1; i < args.size(); ++i)
    {
        /* But we break out after the first parameter, cause there
         * could be parameters with arguments (e.g. --comment comment). */
        if (args.at(i).startsWith("-"))
            break;

#ifdef VBOX_WS_MAC
        const QString strArg = ::darwinResolveAlias(args.at(i));
#else
        const QString strArg = args.at(i);
#endif

        /* So if the argument file exists, we add it to URL list: */
        if (   !strArg.isEmpty()
            && QFile::exists(strArg))
            listArgUrls << QUrl::fromLocalFile(QFileInfo(strArg).absoluteFilePath());
    }

    /* If there are file URLs: */
    if (!listArgUrls.isEmpty())
    {
        /* We enumerate them and: */
        for (int i = 0; i < listArgUrls.size(); ++i)
        {
            /* Check which of them has allowed VM extensions: */
            const QUrl url = listArgUrls.at(i);
            const QString strFile = url.toLocalFile();
            if (UICommon::hasAllowedExtension(strFile, VBoxFileExts))
            {
                /* So that we could run existing VMs: */
                CVirtualBox comVBox = gpGlobalSession->virtualBox();
                CMachine comMachine = comVBox.FindMachine(strFile);
                if (!comMachine.isNull())
                {
                    fResult = true;
                    launchMachine(comMachine);
                    /* And remove their URLs from the ULR list: */
                    listArgUrls.removeAll(url);
                }
            }
        }
    }

    /* And if there are *still* URLs: */
    if (!listArgUrls.isEmpty())
    {
        /* We store them, they will be handled later: */
        m_listArgUrls = listArgUrls;
    }

    return fResult;
}

bool UICommon::argumentUrlsPresent() const
{
    return !m_listArgUrls.isEmpty();
}

QList<QUrl> UICommon::takeArgumentUrls()
{
    const QList<QUrl> result = m_listArgUrls;
    m_listArgUrls.clear();
    return result;
}

#ifdef VBOX_WITH_DEBUGGER_GUI

bool UICommon::isDebuggerEnabled() const
{
    return isDebuggerWorker(&m_fDbgEnabled, GUI_Dbg_Enabled);
}

bool UICommon::isDebuggerAutoShowEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShow, GUI_Dbg_AutoShow);
}

bool UICommon::isDebuggerAutoShowCommandLineEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShowCommandLine, GUI_Dbg_AutoShow);
}

bool UICommon::isDebuggerAutoShowStatisticsEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShowStatistics, GUI_Dbg_AutoShow);
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

bool UICommon::shouldStartPaused() const
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    return m_enmLaunchRunning == LaunchRunning_Default ? isDebuggerAutoShowEnabled() : m_enmLaunchRunning == LaunchRunning_No;
#else
    return false;
#endif
}

#ifdef VBOX_GUI_WITH_PIDFILE

void UICommon::createPidfile()
{
    if (!m_strPidFile.isEmpty())
    {
        const qint64 iPid = qApp->applicationPid();
        QFile file(m_strPidFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
             QTextStream out(&file);
             out << iPid << endl;
        }
        else
            LogRel(("Failed to create pid file %s\n", m_strPidFile.toUtf8().constData()));
    }
}

void UICommon::deletePidfile()
{
    if (   !m_strPidFile.isEmpty()
        && QFile::exists(m_strPidFile))
        QFile::remove(m_strPidFile);
}

#endif /* VBOX_GUI_WITH_PIDFILE */

void UICommon::notifyCloudMachineUnregistered(const QString &strProviderShortName,
                                              const QString &strProfileName,
                                              const QUuid &uId)
{
    emit sigCloudMachineUnregistered(strProviderShortName, strProfileName, uId);
}

void UICommon::notifyCloudMachineRegistered(const QString &strProviderShortName,
                                            const QString &strProfileName,
                                            const CCloudMachine &comMachine)
{
    emit sigCloudMachineRegistered(strProviderShortName, strProfileName, comMachine);
}

#ifdef RT_OS_LINUX
/* static */
void UICommon::checkForWrongUSBMounted()
{
    /* Make sure '/proc/mounts' exists and can be opened: */
    QFile file("/proc/mounts");
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    /* Fetch contents: */
    QStringList contents;
    for (;;)
    {
        QByteArray line = file.readLine();
        if (line.isEmpty())
            break;
        contents << line;
    }
    /* Grep contents for usbfs presence: */
    QStringList grep1(contents.filter("/sys/bus/usb/drivers"));
    QStringList grep2(grep1.filter("usbfs"));
    if (grep2.isEmpty())
        return;

    /* Show corresponding warning: */
    msgCenter().warnAboutWrongUSBMounted();
}
#endif /* RT_OS_LINUX */

/* static */
QString UICommon::usbDetails(const CUSBDevice &comDevice)
{
    QString strDetails;
    if (comDevice.isNull())
        strDetails = tr("Unknown device", "USB device details");
    else
    {
        QVector<QString> devInfoVector = comDevice.GetDeviceInfo();
        QString strManufacturer;
        QString strProduct;

        if (devInfoVector.size() >= 1)
            strManufacturer = devInfoVector[0].trimmed();
        if (devInfoVector.size() >= 2)
            strProduct = devInfoVector[1].trimmed();

        if (strManufacturer.isEmpty() && strProduct.isEmpty())
        {
            strDetails =
                tr("Unknown device %1:%2", "USB device details")
                   .arg(QString::number(comDevice.GetVendorId(),  16).toUpper().rightJustified(4, '0'))
                   .arg(QString::number(comDevice.GetProductId(), 16).toUpper().rightJustified(4, '0'));
        }
        else
        {
            if (strProduct.toUpper().startsWith(strManufacturer.toUpper()))
                strDetails = strProduct;
            else
                strDetails = strManufacturer + " " + strProduct;
        }
        ushort iRev = comDevice.GetRevision();
        if (iRev != 0)
        {
            strDetails += " [";
            strDetails += QString::number(iRev, 16).toUpper().rightJustified(4, '0');
            strDetails += "]";
        }
    }

    return strDetails.trimmed();
}

/* static */
QString UICommon::usbToolTip(const CUSBDevice &comDevice)
{
    QString strTip =
        tr("<nobr>Vendor ID: %1</nobr><br>"
           "<nobr>Product ID: %2</nobr><br>"
           "<nobr>Revision: %3</nobr>", "USB device tooltip")
           .arg(QString::number(comDevice.GetVendorId(),  16).toUpper().rightJustified(4, '0'))
           .arg(QString::number(comDevice.GetProductId(), 16).toUpper().rightJustified(4, '0'))
           .arg(QString::number(comDevice.GetRevision(),  16).toUpper().rightJustified(4, '0'));

    const QString strSerial = comDevice.GetSerialNumber();
    if (!strSerial.isEmpty())
        strTip += QString(tr("<br><nobr>Serial No. %1</nobr>", "USB device tooltip"))
                             .arg(strSerial);

    /* Add the state field if it's a host USB device: */
    CHostUSBDevice hostDev(comDevice);
    if (!hostDev.isNull())
    {
        strTip += QString(tr("<br><nobr>State: %1</nobr>", "USB device tooltip"))
                             .arg(gpConverter->toString(hostDev.GetState()));
    }

    return strTip;
}

/* static */
QString UICommon::usbToolTip(const CUSBDeviceFilter &comFilter)
{
    QString strTip;

    const QString strVendorId = comFilter.GetVendorId();
    if (!strVendorId.isEmpty())
        strTip += tr("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip")
                     .arg(strVendorId);

    const QString strProductId = comFilter.GetProductId();
    if (!strProductId.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product ID: %2</nobr>", "USB filter tooltip")
                                                     .arg(strProductId);

    const QString strRevision = comFilter.GetRevision();
    if (!strRevision.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Revision: %3</nobr>", "USB filter tooltip")
                                                     .arg(strRevision);

    const QString strProduct = comFilter.GetProduct();
    if (!strProduct.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product: %4</nobr>", "USB filter tooltip")
                                                     .arg(strProduct);

    const QString strManufacturer = comFilter.GetManufacturer();
    if (!strManufacturer.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip")
                                                     .arg(strManufacturer);

    const QString strSerial = comFilter.GetSerialNumber();
    if (!strSerial.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Serial No.: %1</nobr>", "USB filter tooltip")
                                                     .arg(strSerial);

    const QString strPort = comFilter.GetPort();
    if (!strPort.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Port: %1</nobr>", "USB filter tooltip")
                                                     .arg(strPort);

    /* Add the state field if it's a host USB device: */
    CHostUSBDevice hostDev(comFilter);
    if (!hostDev.isNull())
    {
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                     .arg(gpConverter->toString(hostDev.GetState()));
    }

    return strTip;
}

/* static */
QString UICommon::usbToolTip(const CHostVideoInputDevice &comWebcam)
{
    QStringList records;

    const QString strName = comWebcam.GetName();
    if (!strName.isEmpty())
        records << strName;

    const QString strPath = comWebcam.GetPath();
    if (!strPath.isEmpty())
        records << strPath;

    return records.join("<br>");
}

int UICommon::supportedRecordingFeatures() const
{
    int iSupportedFlag = 0;
    CSystemProperties comProperties = gpGlobalSession->virtualBox().GetSystemProperties();
    foreach (const KRecordingFeature &enmFeature, comProperties.GetSupportedRecordingFeatures())
        iSupportedFlag |= enmFeature;
    return iSupportedFlag;
}

/* static */
QString UICommon::helpFile()
{
    const QString strName = "UserManual";
    const QString strSuffix = "qhc";

    /* Where are the docs located? */
    char szDocsPath[RTPATH_MAX];
    int rc = RTPathAppDocs(szDocsPath, sizeof(szDocsPath));
    AssertRC(rc);

    /* Make sure that the language is in two letter code.
     * Note: if languageId() returns an empty string lang.name() will
     * return "C" which is an valid language code. */
    QLocale lang(UITranslator::languageId());

    /* Construct the path and the filename: */
    QString strManual = QString("%1/%2_%3.%4").arg(szDocsPath)
                                              .arg(strName)
                                              .arg(lang.name())
                                              .arg(strSuffix);

    /* Check if a help file with that name exists: */
    QFileInfo fi(strManual);
    if (fi.exists())
        return strManual;

    /* Fall back to the standard: */
    strManual = QString("%1/%2.%4").arg(szDocsPath)
                                   .arg(strName)
                                   .arg(strSuffix);
    return strManual;
}

/* static */
QString UICommon::documentsPath()
{
    QString strPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir dir(strPath);
    if (dir.exists())
        return QDir::cleanPath(dir.canonicalPath());
    else
    {
        dir.setPath(QDir::homePath() + "/Documents");
        if (dir.exists())
            return QDir::cleanPath(dir.canonicalPath());
        else
            return QDir::homePath();
    }
}

/* static */
bool UICommon::hasAllowedExtension(const QString &strFileName, const QStringList &extensions)
{
    foreach (const QString &strExtension, extensions)
        if (strFileName.endsWith(strExtension, Qt::CaseInsensitive))
            return true;
    return false;
}

/* static */
QString UICommon::findUniqueFileName(const QString &strFullFolderPath, const QString &strBaseFileName)
{
    QDir folder(strFullFolderPath);
    if (!folder.exists())
        return strBaseFileName;
    QFileInfoList folderContent = folder.entryInfoList();
    QSet<QString> fileNameSet;
    foreach (const QFileInfo &fileInfo, folderContent)
    {
        /* Remove the extension : */
        fileNameSet.insert(fileInfo.completeBaseName());
    }
    int iSuffix = 0;
    QString strNewName(strBaseFileName);
    while (fileNameSet.contains(strNewName))
    {
        strNewName = strBaseFileName + QString("_") + QString::number(++iSuffix);
    }
    return strNewName;
}

/* static */
void UICommon::setMinimumWidthAccordingSymbolCount(QSpinBox *pSpinBox, int cCount)
{
    /* Shame on Qt it hasn't stuff for tuning
     * widget size suitable for reflecting content of desired size.
     * For example QLineEdit, QSpinBox and similar widgets should have a methods
     * to strict the minimum width to reflect at least [n] symbols. */

    /* Load options: */
    QStyleOptionSpinBox option;
    option.initFrom(pSpinBox);

    /* Acquire edit-field rectangle: */
    QRect rect = pSpinBox->style()->subControlRect(QStyle::CC_SpinBox,
                                                   &option,
                                                   QStyle::SC_SpinBoxEditField,
                                                   pSpinBox);

    /* Calculate minimum-width magic: */
    const int iSpinBoxWidth = pSpinBox->width();
    const int iSpinBoxEditFieldWidth = rect.width();
    const int iSpinBoxDelta = qMax(0, iSpinBoxWidth - iSpinBoxEditFieldWidth);
    const QFontMetrics metrics(pSpinBox->font(), pSpinBox);
    const QString strDummy(cCount, '0');
    const int iTextWidth = metrics.horizontalAdvance(strDummy);

    /* Tune spin-box minimum-width: */
    pSpinBox->setMinimumWidth(iTextWidth + iSpinBoxDelta);
}

/* static */
void UICommon::setHelpKeyword(QObject *pObject, const QString &strHelpKeyword)
{
    if (pObject)
        pObject->setProperty("helpkeyword", strHelpKeyword);
}

/* static */
QString UICommon::helpKeyword(const QObject *pObject)
{
    if (!pObject)
        return QString();
    return pObject->property("helpkeyword").toString();
}

bool UICommon::openURL(const QString &strUrl) const
{
    /** Service event. */
    class ServiceEvent : public QEvent
    {
    public:

        /** Constructs service event on th basis of passed @a fResult. */
        ServiceEvent(bool fResult)
            : QEvent(QEvent::User)
            , m_fResult(fResult)
        {}

        /** Returns the result which event brings. */
        bool result() const { return m_fResult; }

    private:

        /** Holds the result which event brings. */
        bool m_fResult;
    };

    /** Service client object. */
    class ServiceClient : public QEventLoop
    {
    public:

        /** Constructs service client on the basis of passed @a fResult. */
        ServiceClient()
            : m_fResult(false)
        {}

        /** Returns the result which event brings. */
        bool result() const { return m_fResult; }

    private:

        /** Handles any Qt @a pEvent. */
        bool event(QEvent *pEvent) RT_OVERRIDE RT_FINAL
        {
            /* Handle service event: */
            if (pEvent->type() == QEvent::User)
            {
                ServiceEvent *pServiceEvent = static_cast<ServiceEvent*>(pEvent);
                m_fResult = pServiceEvent->result();
                pServiceEvent->accept();
                quit();
                return true;
            }
            return false;
        }

        bool m_fResult;
    };

    /** Service server object. */
    class ServiceServer : public QThread
    {
    public:

        /** Constructs service server on the basis of passed @a client and @a strUrl. */
        ServiceServer(ServiceClient &client, const QString &strUrl)
            : m_client(client), m_strUrl(strUrl) {}

    private:

        /** Executes thread task. */
        void run() RT_OVERRIDE RT_FINAL
        {
            QApplication::postEvent(&m_client, new ServiceEvent(QDesktopServices::openUrl(m_strUrl)));
        }

        /** Holds the client reference. */
        ServiceClient &m_client;
        /** Holds the URL to be processed. */
        const QString &m_strUrl;
    };

    /* Create client & server: */
    ServiceClient client;
    ServiceServer server(client, strUrl);
    server.start();
    client.exec();
    server.wait();

    /* Acquire client result: */
    bool fResult = client.result();
    if (!fResult)
        UINotificationMessage::cannotOpenURL(strUrl);

    return fResult;
}

void UICommon::sltGUILanguageChange(QString strLanguage)
{
    /* Make sure medium-enumeration is not in progress! */
    AssertReturnVoid(!gpMediumEnumerator->isMediumEnumerationInProgress());
    /* Load passed language: */
    UITranslator::loadLanguage(strLanguage);
}

void UICommon::sltHandleCloudMachineAdded(const QString &strProviderShortName,
                                          const QString &strProfileName,
                                          const CCloudMachine &comMachine)
{
    /* Make sure we cached added cloud VM in GUI: */
    notifyCloudMachineRegistered(strProviderShortName,
                                 strProfileName,
                                 comMachine);
}

bool UICommon::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /** @todo Just use the QIWithRetranslateUI3 template wrapper. */

    if (   pEvent->type() == QEvent::LanguageChange
        && pObject->isWidgetType()
        && qobject_cast<QWidget*>(pObject)->isWindow())
    {
        /* Catch the language change event before any other widget gets it in
         * order to invalidate cached string resources (like the details view
         * templates) that may be used by other widgets. */
        QWidgetList list = QApplication::topLevelWidgets();
        if (list.first() == pObject)
        {
            /* Call this only once per every language change (see
             * QApplication::installTranslator() for details): */
            retranslateUi();
        }
    }

    /* Handle application palette change event: */
    if (   pEvent->type() == QEvent::ApplicationPaletteChange
        && pObject == windowManager().mainWindowShown())
    {
#if defined(VBOX_WS_MAC)
        const bool fDarkMode = UICocoaApplication::instance()->isDarkMode();
#elif defined(VBOX_WS_WIN)
        const bool fDarkMode = isWindowsInDarkMode();
#else /* Linux, BSD, Solaris */
        const bool fDarkMode = isPaletteInDarkMode();
#endif /* Linux, BSD, Solaris */
        if (m_fDarkMode != fDarkMode)
        {
            m_fDarkMode = fDarkMode;
            loadColorTheme();
            emit sigThemeChange();
        }
    }

    /* Call to base-class: */
    return QObject::eventFilter(pObject, pEvent);
}

void UICommon::sltHandleFontScaleFactorChanged(int iFontScaleFactor)
{
    QFont appFont = qApp->font();

    /* Let's round up some double var: */
    auto roundUp = [](double dValue)
    {
        const int iValue = dValue;
        return dValue > (double)iValue ? iValue + 1 : iValue;
    };

    /* Do we have pixel font? */
    if (iOriginalFontPixelSize != -1)
        appFont.setPixelSize(roundUp(iFontScaleFactor / 100.f * iOriginalFontPixelSize));
    /* Point font otherwise: */
    else
        appFont.setPointSize(roundUp(iFontScaleFactor / 100.f * iOriginalFontPointSize));

    qApp->setFont(appFont);
}

void UICommon::retranslateUi()
{
    /* Re-enumerate uimedium since they contain some translations too: */
    if (isValid())
        gpMediumEnumerator->refreshMedia();

#ifdef VBOX_WS_NIX
    // WORKAROUND:
    // As X11 do not have functionality for providing human readable key names,
    // we keep a table of them, which must be updated when the language is changed.
    UINativeHotKey::retranslateKeyNames();
#endif
}

#ifndef VBOX_GUI_WITH_CUSTOMIZATIONS1
void UICommon::sltHandleCommitDataRequest(QSessionManager &manager)
{
    LogRel(("GUI: UICommon: Commit data request...\n"));

    /* Ask listener to commit data: */
    emit sigAskToCommitData();
# ifdef VBOX_WS_WIN
    m_fDataCommitted = true;
# endif

    /* Depending on UI type: */
    switch (uiType())
    {
        /* For Runtime UI: */
        case UIType_RuntimeUI:
        {
            /* Thin clients will be able to shutdown properly,
             * but for fat clients: */
            if (!isSeparateProcess())
            {
# if defined(VBOX_WS_MAC) && defined(VBOX_IS_QT6_OR_LATER) /** @todo qt6: ... */
                Q_UNUSED(manager);
                /* This code prevents QWindowSystemInterface::handleApplicationTermination
                   for running, so among other things QApplication::closeAllWindows isn't
                   called and we're somehow stuck in a half closed down state.  That said,
                   just disabling this isn't sufficent, there we also have to accept() the
                   QCloseEvent in UIMachineWindow. */
                /** @todo qt6: This isn't quite the right fix, I bet...  I'm sure I haven't
                 *  quite understood all that's going on here.  So, leaving this for
                 *  the real GUI experts to look into... :-)   */
# else
                // WORKAROUND:
                // We can't save VM state in one go for fat clients, so we have to ask session manager to cancel shutdown.
                // To next major release this should be removed in any case, since there will be no fat clients after all.
                manager.cancel();

#  ifdef VBOX_WS_WIN
                // WORKAROUND:
                // In theory that's Qt5 who should allow us to provide canceling reason as well, but that functionality
                // seems to be missed in Windows platform plugin, so we are making that ourselves.
                NativeWindowSubsystem::ShutdownBlockReasonCreateAPI((HWND)windowManager().mainWindowShown()->winId(), L"VM is still running.");
#  endif
# endif
            }

            break;
        }
        default:
            break;
    }
}
#endif /* !VBOX_GUI_WITH_CUSTOMIZATIONS1 */

void UICommon::sltHandleVBoxSVCAvailabilityChange(bool fAvailable)
{
    /* If VBoxSVC is available: */
    if (fAvailable)
    {
        /* For Selector UI: */
        if (uiType() == UIType_ManagerUI)
        {
            /* Recreate Main event listeners: */
            UIVirtualBoxEventHandler::destroy();
            UIVirtualBoxClientEventHandler::destroy();
            UIExtraDataManager::destroy();
            UIExtraDataManager::instance();
            UIVirtualBoxEventHandler::instance();
            UIVirtualBoxClientEventHandler::instance();
            /* Ask UIStarter to restart UI: */
            emit sigAskToRestartUI();
        }
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI

# define UICOMMON_DBG_CFG_VAR_FALSE       (0)
# define UICOMMON_DBG_CFG_VAR_TRUE        (1)
# define UICOMMON_DBG_CFG_VAR_MASK        (1)
# define UICOMMON_DBG_CFG_VAR_CMD_LINE    RT_BIT(3)
# define UICOMMON_DBG_CFG_VAR_DONE        RT_BIT(4)

void UICommon::initDebuggerVar(int *piDbgCfgVar, const char *pszEnvVar, const char *pszExtraDataName, bool fDefault)
{
    QString strEnvValue;
    char    szEnvValue[256];
    int rc = RTEnvGetEx(RTENV_DEFAULT, pszEnvVar, szEnvValue, sizeof(szEnvValue), NULL);
    if (RT_SUCCESS(rc))
    {
        strEnvValue = QString::fromUtf8(&szEnvValue[0]).toLower().trimmed();
        if (strEnvValue.isEmpty())
            strEnvValue = "yes";
    }
    else if (rc != VERR_ENV_VAR_NOT_FOUND)
        strEnvValue = "veto";

    CVirtualBox comVBox = gpGlobalSession->virtualBox();
    QString strExtraValue = comVBox.GetExtraData(pszExtraDataName).toLower().trimmed();
    if (strExtraValue.isEmpty())
        strExtraValue = QString();

    if ( strEnvValue.contains("veto") || strExtraValue.contains("veto"))
        *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_DONE | UICOMMON_DBG_CFG_VAR_FALSE;
    else if (strEnvValue.isNull() && strExtraValue.isNull())
        *piDbgCfgVar = fDefault ? UICOMMON_DBG_CFG_VAR_TRUE : UICOMMON_DBG_CFG_VAR_FALSE;
    else
    {
        QString *pStr = !strEnvValue.isEmpty() ? &strEnvValue : &strExtraValue;
        if (   pStr->startsWith("y")  // yes
            || pStr->startsWith("e")  // enabled
            || pStr->startsWith("t")  // true
            || pStr->startsWith("on")
            || pStr->toLongLong() != 0)
            *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_TRUE;
        else if (   pStr->startsWith("n")  // o
                 || pStr->startsWith("d")  // disable
                 || pStr->startsWith("f")  // false
                 || pStr->startsWith("off")
                 || pStr->contains("veto") /* paranoia */
                 || pStr->toLongLong() == 0)
            *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_FALSE;
        else
        {
            LogFunc(("Ignoring unknown value '%s' for '%s'\n", pStr->toUtf8().constData(), pStr == &strEnvValue ? pszEnvVar : pszExtraDataName));
            *piDbgCfgVar = fDefault ? UICOMMON_DBG_CFG_VAR_TRUE : UICOMMON_DBG_CFG_VAR_FALSE;
        }
    }
}

void UICommon::setDebuggerVar(int *piDbgCfgVar, bool fState)
{
    if (!(*piDbgCfgVar & UICOMMON_DBG_CFG_VAR_DONE))
        *piDbgCfgVar = (fState ? UICOMMON_DBG_CFG_VAR_TRUE : UICOMMON_DBG_CFG_VAR_FALSE)
                     | UICOMMON_DBG_CFG_VAR_CMD_LINE;
}

bool UICommon::isDebuggerWorker(int *piDbgCfgVar, const char *pszExtraDataName) const
{
    if (!(*piDbgCfgVar & UICOMMON_DBG_CFG_VAR_DONE))
    {
        const QString str = gEDataManager->debugFlagValue(pszExtraDataName);
        if (str.contains("veto"))
            *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_DONE | UICOMMON_DBG_CFG_VAR_FALSE;
        else if (str.isEmpty() || (*piDbgCfgVar & UICOMMON_DBG_CFG_VAR_CMD_LINE))
            *piDbgCfgVar |= UICOMMON_DBG_CFG_VAR_DONE;
        else if (   str.startsWith("y")  // yes
                 || str.startsWith("e")  // enabled
                 || str.startsWith("t")  // true
                 || str.startsWith("on")
                 || str.toLongLong() != 0)
            *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_DONE | UICOMMON_DBG_CFG_VAR_TRUE;
        else if (   str.startsWith("n")  // no
                 || str.startsWith("d")  // disable
                 || str.startsWith("f")  // false
                 || str.toLongLong() == 0)
            *piDbgCfgVar = UICOMMON_DBG_CFG_VAR_DONE | UICOMMON_DBG_CFG_VAR_FALSE;
        else
            *piDbgCfgVar |= UICOMMON_DBG_CFG_VAR_DONE;
    }

    return (*piDbgCfgVar & UICOMMON_DBG_CFG_VAR_MASK) == UICOMMON_DBG_CFG_VAR_TRUE;
}

#endif /* VBOX_WITH_DEBUGGER_GUI */
