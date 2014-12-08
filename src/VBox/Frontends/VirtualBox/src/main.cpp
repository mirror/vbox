/* $Id$ */
/** @file
 * VBox Qt GUI - The main() function.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
# ifdef Q_WS_MAC
#  include "UICocoaApplication.h"
# endif /* Q_WS_MAC */
#else /* !VBOX_WITH_PRECOMPILED_HEADERS */

# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UISelectorWindow.h"
# include "UIMachine.h"
# include "VBoxUtils.h"
# include "UIModalWindowManager.h"
# include "UIExtraDataManager.h"
# ifdef Q_WS_MAC
#  include "UICocoaApplication.h"
# endif

# ifdef Q_WS_X11
#  include <iprt/env.h>
# endif

# include <QMessageBox>
# include <QLocale>
# include <QTranslator>

# include <iprt/buildconfig.h>
# include <iprt/initterm.h>
# include <iprt/process.h>
# include <iprt/stream.h>
# include <VBox/version.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <VBox/err.h>

#include <QCleanlooksStyle>
#include <QPlastiqueStyle>

#ifdef Q_WS_X11
# include <QFontDatabase>
# include <X11/Xlib.h>
# include <dlfcn.h>
#endif

#ifdef VBOX_WITH_HARDENING
# include <VBox/sup.h>
#endif

#ifdef RT_OS_LINUX
# include <unistd.h>
#endif

#include <cstdio>


/* XXX Temporarily. Don't rely on the user to hack the Makefile himself! */
QString g_QStrHintLinuxNoMemory = QApplication::tr(
  "This error means that the kernel driver was either not able to "
  "allocate enough memory or that some mapping operation failed."
  );

QString g_QStrHintLinuxNoDriver = QApplication::tr(
  "The VirtualBox Linux kernel driver (vboxdrv) is either not loaded or "
  "there is a permission problem with /dev/vboxdrv. Please reinstall the kernel "
  "module by executing<br/><br/>"
  "  <font color=blue>'/etc/init.d/vboxdrv setup'</font><br/><br/>"
  "as root. If it is available in your distribution, you should install the "
  "DKMS package first. This package keeps track of Linux kernel changes and "
  "recompiles the vboxdrv kernel module if necessary."
  );

QString g_QStrHintOtherWrongDriverVersion = QApplication::tr(
  "The VirtualBox kernel modules do not match this version of "
  "VirtualBox. The installation of VirtualBox was apparently not "
  "successful. Please try completely uninstalling and reinstalling "
  "VirtualBox."
  );

QString g_QStrHintLinuxWrongDriverVersion = QApplication::tr(
  "The VirtualBox kernel modules do not match this version of "
  "VirtualBox. The installation of VirtualBox was apparently not "
  "successful. Executing<br/><br/>"
  "  <font color=blue>'/etc/init.d/vboxdrv setup'</font><br/><br/>"
  "may correct this. Make sure that you do not mix the "
  "OSE version and the PUEL version of VirtualBox."
  );

QString g_QStrHintOtherNoDriver = QApplication::tr(
  "Make sure the kernel module has been loaded successfully."
  );

/* I hope this isn't (C), (TM) or (R) Microsoft support ;-) */
QString g_QStrHintReinstall = QApplication::tr(
  "Please try reinstalling VirtualBox."
  );

#if defined(DEBUG) && defined(Q_WS_X11) && defined(RT_OS_LINUX)

#include <signal.h>
#include <execinfo.h>

/* get REG_EIP from ucontext.h */
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <ucontext.h>
#ifdef RT_ARCH_AMD64
# define REG_PC REG_RIP
#else
# define REG_PC REG_EIP
#endif



/**
 * the signal handler that prints out a backtrace of the call stack.
 * the code is taken from http://www.linuxjournal.com/article/6391.
 */
void bt_sighandler (int sig, siginfo_t *info, void *secret) {

    void *trace[16];
    char **messages = (char **)NULL;
    int i, trace_size = 0;
    ucontext_t *uc = (ucontext_t *)secret;

    /* Do something useful with siginfo_t */
    if (sig == SIGSEGV)
        Log (("GUI: Got signal %d, faulty address is %p, from %p\n",
              sig, info->si_addr, uc->uc_mcontext.gregs[REG_PC]));
    else
        Log (("GUI: Got signal %d\n", sig));

    trace_size = backtrace (trace, 16);
    /* overwrite sigaction with caller's address */
    trace[1] = (void *) uc->uc_mcontext.gregs [REG_PC];

    messages = backtrace_symbols (trace, trace_size);
    /* skip first stack frame (points here) */
    Log (("GUI: [bt] Execution path:\n"));
    for (i = 1; i < trace_size; ++i)
        Log (("GUI: [bt] %s\n", messages[i]));

    exit (0);
}

#endif /* DEBUG && X11 && LINUX*/

#if defined(RT_OS_DARWIN)
# include <dlfcn.h>
# include <sys/mman.h>
# include <iprt/asm.h>
# include <iprt/system.h>

/** Really ugly hack to shut up a silly check in AppKit. */
static void ShutUpAppKit(void)
{
    /* Check for Snow Leopard or higher */
    char szInfo[64];
    int rc = RTSystemQueryOSInfo (RTSYSOSINFO_RELEASE, szInfo, sizeof(szInfo));
    if (   RT_SUCCESS (rc)
        && szInfo[0] == '1') /* higher than 1x.x.x */
    {
        /*
         * Find issetguid() and make it always return 0 by modifying the code.
         */
        void *addr = dlsym(RTLD_DEFAULT, "issetugid");
        int rc = mprotect((void *)((uintptr_t)addr & ~(uintptr_t)0xfff), 0x2000, PROT_WRITE|PROT_READ|PROT_EXEC);
        if (!rc)
            ASMAtomicWriteU32((volatile uint32_t *)addr, 0xccc3c031); /* xor eax, eax; ret; int3 */
    }
}
#endif /* DARWIN */

static void QtMessageOutput (QtMsgType type, const char *msg)
{
#ifndef Q_WS_X11
    NOREF(msg);
#endif
    switch (type)
    {
        case QtDebugMsg:
            Log (("Qt DEBUG: %s\n", msg));
            break;
        case QtWarningMsg:
            Log (("Qt WARNING: %s\n", msg));
#ifdef Q_WS_X11
            /* Needed for instance for the message ``cannot connect to X server'' */
            RTStrmPrintf(g_pStdErr, "Qt WARNING: %s\n", msg);
#endif
            break;
        case QtCriticalMsg:
            Log (("Qt CRITICAL: %s\n", msg));
#ifdef Q_WS_X11
            /* Needed for instance for the message ``cannot connect to X server'' */
            RTStrmPrintf(g_pStdErr, "Qt CRITICAL: %s\n", msg);
#endif
            break;
        case QtFatalMsg:
            Log (("Qt FATAL: %s\n", msg));
#ifdef Q_WS_X11
            RTStrmPrintf(g_pStdErr, "Qt FATAL: %s\n", msg);
#endif
    }
}

/**
 * Show all available command line parameters.
 */
static void showHelp()
{
    RTPrintf(VBOX_PRODUCT " Manager %s\n"
            "(C) 2005-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
            "All rights reserved.\n"
            "\n"
            "Usage:\n"
            "  --startvm <vmname|UUID>    start a VM by specifying its UUID or name\n"
            "  --separate                 start a separate VM process\n"
            "  --seamless                 switch to seamless mode during startup\n"
            "  --fullscreen               switch to fullscreen mode during startup\n"
            "  --no-startvm-errormsgbox   do not show a message box for VM start errors\n"
            "  --restore-current          restore the current snapshot before starting\n"
            "  --no-aggressive-caching    delays caching media info in VM processes\n"
# ifdef VBOX_GUI_WITH_PIDFILE
            "  --pidfile <file>           create a pidfile file when a VM is up and running\n"
# endif
# ifdef VBOX_WITH_DEBUGGER_GUI
            "  --dbg                      enable the GUI debug menu\n"
            "  --debug                    like --dbg and show debug windows at VM startup\n"
            "  --debug-command-line       like --dbg and show command line window at VM startup\n"
            "  --debug-statistics         like --dbg and show statistics window at VM startup\n"
            "  --no-debug                 disable the GUI debug menu and debug windows\n"
            "  --start-paused             start the VM in the paused state\n"
            "  --start-running            start the VM running (for overriding --debug*)\n"
            "\n"
# endif
            "Expert options:\n"
            "  --disable-patm             disable code patching (ignored by AMD-V/VT-x)\n"
            "  --disable-csam             disable code scanning (ignored by AMD-V/VT-x)\n"
            "  --recompile-supervisor     recompiled execution of supervisor code (*)\n"
            "  --recompile-user           recompiled execution of user code (*)\n"
            "  --recompile-all            recompiled execution of all code, with disabled\n"
            "                             code patching and scanning\n"
            "  --execute-all-in-iem       For debugging the interpreted execution mode.\n"
            "  --warp-pct <pct>           time warp factor, 100%% (= 1.0) = normal speed\n"
            "  (*) For AMD-V/VT-x setups the effect is --recompile-all.\n"
            "\n"
# ifdef VBOX_WITH_DEBUGGER_GUI
            "The following environment (and extra data) variables are evaluated:\n"
            "  VBOX_GUI_DBG_ENABLED (GUI/Dbg/Enabled)\n"
            "                             enable the GUI debug menu if set\n"
            "  VBOX_GUI_DBG_AUTO_SHOW (GUI/Dbg/AutoShow)\n"
            "                             show debug windows at VM startup\n"
            "  VBOX_GUI_NO_DEBUGGER       disable the GUI debug menu and debug windows\n"
# endif
            "\n",
            RTBldCfgVersion());
    /** @todo Show this as a dialog on windows. */
}

#ifdef Q_WS_X11
/** This is a workaround for a bug on old libX11 versions, fixed in commit
 *      941f02ede63baa46f93ed8abccebe76fb29c0789 and released in version 1.1. */
Status VBoxXInitThreads(void)
{
    void *pvProcess = dlopen(NULL, RTLD_GLOBAL | RTLD_LAZY);
    Status rc = 1;
    if (pvProcess && dlsym(pvProcess, "xcb_connect"))
        rc = XInitThreads();
    if (pvProcess)
        dlclose(pvProcess);
    return rc;
}
#endif

extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char ** /*envp*/)
{
    /* Start logging: */
    LogFlowFuncEnter();

    /* Failed result initially: */
    int iResultCode = 1;

#ifdef Q_WS_X11
    if (!VBoxXInitThreads())
        return 1;
#endif /* Q_WS_X11 */

    /* Simulate try-catch block: */
    do
    {
#ifdef RT_OS_DARWIN
        ShutUpAppKit();
#endif /* RT_OS_DARWIN */

        /* Console help preprocessing: */
        bool fHelpShown = false;
        for (int i = 0; i < argc; ++i)
        {
            if (   !strcmp(argv[i], "-h")
                || !strcmp(argv[i], "-?")
                || !strcmp(argv[i], "-help")
                || !strcmp(argv[i], "--help"))
            {
                showHelp();
                fHelpShown = true;
                break;
            }
        }
        if (fHelpShown)
        {
            iResultCode = 0;
            break;
        }

#if defined(DEBUG) && defined(Q_WS_X11) && defined(RT_OS_LINUX)
        /* Install our signal handler to backtrace the call stack: */
        struct sigaction sa;
        sa.sa_sigaction = bt_sighandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
#endif

#ifdef VBOX_WITH_HARDENING
        /* Make sure the image verification code works (VBoxDbg.dll and other plugins). */
        SUPR3HardenedVerifyInit();
#endif

#ifdef Q_WS_MAC
        /* Mavericks font fix: */
        if (VBoxGlobal::osRelease() >= MacOSXRelease_Mavericks)
            QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
# ifdef QT_MAC_USE_COCOA
        /* Instantiate our NSApplication derivative before QApplication
         * forces NSApplication to be instantiated. */
        UICocoaApplication::instance();
# endif /* QT_MAC_USE_COCOA */
#endif /* Q_WS_MAC */

        /* Install Qt console message handler: */
        qInstallMsgHandler(QtMessageOutput);

        /* Create application: */
        QApplication a(argc, argv);

#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
        /* Enable HiDPI icons. For this we require a patched version of Qt 4.x with
         * the changes from https://codereview.qt-project.org/#change,54636 applied. */
        qApp->setAttribute(Qt::AA_UseHighDpiPixmaps);
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */

#ifdef Q_OS_SOLARIS
        /* Use plastique look&feel for Solaris instead of the default motif (Qt 4.7.x): */
        QApplication::setStyle(new QPlastiqueStyle);
#endif /* Q_OS_SOLARIS */

#ifdef Q_WS_X11
        /* This patch is not used for now on Solaris & OpenSolaris because
         * there is no anti-aliasing enabled by default, Qt4 to be rebuilt. */
# ifndef Q_OS_SOLARIS
        /* Cause Qt4 has the conflict with fontconfig application as a result
         * sometimes substituting some fonts with non scaleable-anti-aliased
         * bitmap font we are reseting substitutes for the current application
         * font family if it is non scaleable-anti-aliased. */
        QFontDatabase fontDataBase;

        QString currentFamily(QApplication::font().family());
        bool isCurrentScaleable = fontDataBase.isScalable(currentFamily);

        QString subFamily(QFont::substitute(currentFamily));
        bool isSubScaleable = fontDataBase.isScalable(subFamily);

        if (isCurrentScaleable && !isSubScaleable)
            QFont::removeSubstitution(currentFamily);
# endif /* Q_OS_SOLARIS */
#endif /* Q_WS_X11 */

#ifdef Q_WS_WIN
        /* Drag in the sound drivers and DLLs early to get rid of the delay taking
         * place when the main menu bar (or any action from that menu bar) is
         * activated for the first time. This delay is especially annoying if it
         * happens when the VM is executing in real mode (which gives 100% CPU
         * load and slows down the load process that happens on the main GUI
         * thread to several seconds). */
        PlaySound(NULL, NULL, 0);
#endif /* Q_WS_WIN */

#ifdef Q_WS_MAC
        /* Disable menu icons on MacOS X host: */
        ::darwinDisableIconsInMenus();
#endif /* Q_WS_MAC */

#ifdef Q_WS_X11
        /* Qt version check (major.minor are sensitive, fix number is ignored): */
        if (VBoxGlobal::qtRTVersion() < (VBoxGlobal::qtCTVersion() & 0xFFFF00))
        {
            QString strMsg = QApplication::tr("Executable <b>%1</b> requires Qt %2.x, found Qt %3.")
                                              .arg(qAppName())
                                              .arg(VBoxGlobal::qtCTVersionString().section('.', 0, 1))
                                              .arg(VBoxGlobal::qtRTVersionString());
            QMessageBox::critical(0, QApplication::tr("Incompatible Qt Library Error"),
                                  strMsg, QMessageBox::Abort, 0);
            qFatal("%s", strMsg.toUtf8().constData());
            break;
        }
#endif /* Q_WS_X11 */

        /* Create modal-window manager: */
        UIModalWindowManager::create();

        /* Create global UI instance: */
        VBoxGlobal::create();

        /* Simulate try-catch block: */
        do
        {
            /* Exit if VBoxGlobal is not valid: */
            if (!vboxGlobal().isValid())
                break;

            /* Exit if VBoxGlobal was able to pre-process arguments: */
            if (vboxGlobal().processArgs())
                break;

#ifdef RT_OS_LINUX
            /* Make sure no wrong USB mounted: */
            VBoxGlobal::checkForWrongUSBMounted();
#endif /* RT_OS_LINUX */

            /* Load application settings: */
            VBoxGlobalSettings settings = vboxGlobal().settings();

            /* VM console process: */
            if (vboxGlobal().isVMConsoleProcess())
            {
                /* Make sure VM is started: */
                if (!UIMachine::startMachine(vboxGlobal().managedVMUuid()))
                    break;

                /* Start application: */
                iResultCode = a.exec();
            }
            /* VM selector process: */
            else
            {
                /* Make sure VM selector is permitted: */
                if (settings.isFeatureActive("noSelector"))
                {
                    msgCenter().cannotStartSelector();
                    break;
                }

#ifdef VBOX_BLEEDING_EDGE
                msgCenter().showExperimentalBuildWarning();
#else /* VBOX_BLEEDING_EDGE */
# ifndef DEBUG
                /* Check for BETA version: */
                const QString vboxVersion(vboxGlobal().virtualBox().GetVersion());
                if (   vboxVersion.contains("BETA")
                    && gEDataManager->preventBetaBuildWarningForVersion() != vboxVersion)
                    msgCenter().showBetaBuildWarning();
# endif /* !DEBUG */
#endif /* !VBOX_BLEEDING_EDGE*/

                /* Create/show selector window: */
                vboxGlobal().selectorWnd().show();

                /* Start application: */
                iResultCode = a.exec();
            }
        }
        while (0);

        /* Destroy global UI instance: */
        VBoxGlobal::destroy();

        /* Destroy modal-window manager: */
        UIModalWindowManager::destroy();
    }
    while (0);

    /* Finish logging: */
    LogFlowFunc(("rc=%d\n", iResultCode));
    LogFlowFuncLeave();

    /* Return result: */
    return iResultCode;
}

#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv, char **envp)
{
#ifdef Q_WS_X11
    if (!VBoxXInitThreads())
        return 1;
#endif /* Q_WS_X11 */

    /* Initialize VBox Runtime.
     * Initialize the SUPLib as well only if we are really about to start a VM.
     * Don't do this if we are only starting the selector window or a separate VM process. */
    bool fStartVM = false;
    bool fSeparateProcess = false;
    for (int i = 1; i < argc && !(fStartVM && fSeparateProcess); ++i)
    {
        /* NOTE: the check here must match the corresponding check for the
         * options to start a VM in hardenedmain.cpp and VBoxGlobal.cpp exactly,
         * otherwise there will be weird error messages. */
        if (   !::strcmp(argv[i], "--startvm")
            || !::strcmp(argv[i], "-startvm"))
        {
            fStartVM = true;
        }
        else if (   !::strcmp(argv[i], "--separate")
                 || !::strcmp(argv[i], "-separate"))
        {
            fSeparateProcess = true;
        }
    }

    uint32_t fFlags = fStartVM && !fSeparateProcess ? RTR3INIT_FLAGS_SUPLIB : 0;

    int rc = RTR3InitExe(argc, &argv, fFlags);

    /* Initialization failed: */
    if (RT_FAILURE(rc))
    {
        /* We have to create QApplication anyway
         * just to show the only one error-message: */
        QApplication a(argc, &argv[0]);
        Q_UNUSED(a);

#ifdef Q_OS_SOLARIS
        /* Use plastique look&feel for Solaris instead of the default motif (Qt 4.7.x): */
        QApplication::setStyle(new QPlastiqueStyle);
#endif /* Q_OS_SOLARIS */

        /* Prepare the error-message: */
        QString strTitle = QApplication::tr("VirtualBox - Runtime Error");
        QString strText = "<html>";
        switch (rc)
        {
            case VERR_VM_DRIVER_NOT_INSTALLED:
            case VERR_VM_DRIVER_LOAD_ERROR:
                strText += QApplication::tr("<b>Cannot access the kernel driver!</b><br/><br/>");
# ifdef RT_OS_LINUX
                strText += g_QStrHintLinuxNoDriver;
# else /* RT_OS_LINUX */
                strText += g_QStrHintOtherNoDriver;
# endif /* !RT_OS_LINUX */
                break;
# ifdef RT_OS_LINUX
            case VERR_NO_MEMORY:
                strText += g_QStrHintLinuxNoMemory;
                break;
# endif /* RT_OS_LINUX */
            case VERR_VM_DRIVER_NOT_ACCESSIBLE:
                strText += QApplication::tr("Kernel driver not accessible");
                break;
            case VERR_VM_DRIVER_VERSION_MISMATCH:
# ifdef RT_OS_LINUX
                strText += g_QStrHintLinuxWrongDriverVersion;
# else /* RT_OS_LINUX */
                strText += g_QStrHintOtherWrongDriverVersion;
# endif /* !RT_OS_LINUX */
                break;
            default:
                strText += QApplication::tr("Unknown error %2 during initialization of the Runtime").arg(rc);
                break;
        }
        strText += "</html>";

        /* Show the error-message: */
        QMessageBox::critical(0 /* parent */, strTitle, strText,
                              QMessageBox::Abort /* 1st button */, 0 /* 2nd button */);

        /* Default error-result: */
        return 1;
    }

    /* Actual main function: */
    return TrustedMain(argc, argv, envp);
}

#else  /* VBOX_WITH_HARDENING */

extern "C" DECLEXPORT(void) TrustedError(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va)
{
# ifdef RT_OS_DARWIN
    ShutUpAppKit();
# endif /* RT_OS_DARWIN */

    /* We have to create QApplication anyway just to show the only one error-message.
     * This is a bit hackish as we don't have the argument vector handy. */
    int argc = 0;
    char *argv[2] = { NULL, NULL };
    QApplication a(argc, &argv[0]);

    /* Prepare the error-message: */
    QString strTitle = QApplication::tr("VirtualBox - Error In %1").arg(pszWhere);

    char szMsgBuf[1024];
    RTStrPrintfV(szMsgBuf, sizeof(szMsgBuf), pszMsgFmt, va);
    QString strText = QApplication::tr("<html><b>%1 (rc=%2)</b><br/><br/>").arg(szMsgBuf).arg(rc);
    strText.replace(QString("\n"), QString("<br>"));

    switch (enmWhat)
    {
        case kSupInitOp_Driver:
# ifdef RT_OS_LINUX
            strText += g_QStrHintLinuxNoDriver;
# else /* RT_OS_LINUX */
            strText += g_QStrHintOtherNoDriver;
# endif /* !RT_OS_LINUX */
            break;
# ifdef RT_OS_LINUX
        case kSupInitOp_IPRT:
        case kSupInitOp_Misc:
            if (rc == VERR_NO_MEMORY)
                strText += g_QStrHintLinuxNoMemory;
            else
# endif /* RT_OS_LINUX */
            if (rc == VERR_VM_DRIVER_VERSION_MISMATCH)
# ifdef RT_OS_LINUX
                strText += g_QStrHintLinuxWrongDriverVersion;
# else /* RT_OS_LINUX */
                strText += g_QStrHintOtherWrongDriverVersion;
# endif /* !RT_OS_LINUX */
            else
                strText += g_QStrHintReinstall;
            break;
        case kSupInitOp_Integrity:
        case kSupInitOp_RootCheck:
            strText += g_QStrHintReinstall;
            break;
        default:
            /* no hints here */
            break;
    }

    strText += "</html>";

# ifdef RT_OS_LINUX
    /* We have to to make sure that we display the error-message
     * after the parent displayed its own message. */
    sleep(2);
# endif /* RT_OS_LINUX */

    QMessageBox::critical(0 /* parent */, strTitle, strText,
                          QMessageBox::Abort /* 1st button */, 0 /* 2nd button */);
    qFatal("%s", strText.toUtf8().constData());
}

#endif /* VBOX_WITH_HARDENING */

