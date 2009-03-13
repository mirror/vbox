/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * The main() function
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxSelectorWnd.h"
#include "VBoxConsoleWnd.h"
#include "VBoxUtils.h"
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
# include "QIApplication.h"
#else
# define QIApplication QApplication
#endif
#ifdef QT_MAC_USE_COCOA
# include "darwin/VBoxCocoaApplication.h"
#endif

#ifdef Q_WS_X11
#include <QFontDatabase>
#endif

#include <QCleanlooksStyle>
#include <QPlastiqueStyle>
#include <qmessagebox.h>
#include <qlocale.h>
#include <qtranslator.h>

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#ifdef VBOX_WITH_HARDENING
# include <VBox/sup.h>
#endif

#ifdef RT_OS_LINUX
# include <unistd.h>
#endif

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

#if defined(RT_OS_DARWIN) && defined(RT_ARCH_AMD64)
# include <dlfcn.h>
# include <sys/mman.h>
# include <iprt/asm.h>

/** Really ugly hack to shut up a silly check in AppKit. */
static void ShutUpAppKit(void)
{
    /*
     * Find issetguid() and make it always return 0 by modifying the code.
     */
    void *addr = dlsym(RTLD_DEFAULT, "issetugid");
    int rc = mprotect((void *)((uintptr_t)addr & ~(uintptr_t)0xfff), 0x2000, PROT_WRITE|PROT_READ|PROT_EXEC);
    if (!rc)
        ASMAtomicWriteU32((volatile uint32_t *)addr, 0xccc3c031); /* xor eax, eax; ret; int3 */
}
#endif /* DARWIN + AMD64 */

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

#ifndef Q_WS_WIN
/**
 * Show all available command line parameters.
 */
static void showHelp()
{
    QString mode = "", dflt = "";
#ifdef VBOX_GUI_USE_SDL
    mode += "sdl";
#endif
#ifdef VBOX_GUI_USE_QIMAGE
    if (!mode.isEmpty())
        mode += "|";
    mode += "image";
#endif
#ifdef VBOX_GUI_USE_DDRAW
    if (!mode.isEmpty())
        mode += "|";
    mode += "ddraw";
#endif
#ifdef VBOX_GUI_USE_QUARTZ2D
    if (!mode.isEmpty())
        mode += "|";
    mode += "quartz2d";
#endif
#if defined (Q_WS_MAC) && defined (VBOX_GUI_USE_QUARTZ2D)
    dflt = "quartz2d";
#elif (defined (Q_WS_WIN32) || defined (Q_WS_PM)) && defined (VBOX_GUI_USE_QIMAGE)
    dflt = "image";
#elif defined (Q_WS_X11) && defined (VBOX_GUI_USE_SDL)
    dflt = "sdl";
#else
    dflt = "image";
#endif

    RTPrintf("Sun xVM VirtualBox Graphical User Interface "VBOX_VERSION_STRING"\n"
            "(C) 2005-2009 Sun Microsystems, Inc.\n"
            "All rights reserved.\n"
            "\n"
            "Usage:\n"
            "  -startvm <vmname|UUID>     start a VM by specifying its UUID or name\n"
            "  -rmode %-19s select different render mode (default is %s)\n"
# ifdef VBOX_WITH_DEBUGGER_GUI
            "  -dbg                       enable the GUI debug menu\n"
            "  -debug                     like -dbg and show debug windows at VM startup\n"
            "  -no-debug                  disable the GUI debug menu and debug windows\n"
            "\n"
            "The following environment variables are evaluated:\n"
            "  VBOX_GUI_DBG_ENABLED       enable the GUI debug menu if set\n"
            "  VBOX_GUI_DBG_AUTO_SHOW     show debug windows at VM startup\n"
            "  VBOX_GUI_NO_DEBUGGER       disable the GUI debug menu and debug windows\n"
# endif
            "\n",
            mode.toLatin1().constData(),
            dflt.toLatin1().constData());
}
#endif

extern "C" DECLEXPORT(int) TrustedMain (int argc, char **argv, char ** /*envp*/)
{
    LogFlowFuncEnter();
# if defined(RT_OS_DARWIN) && defined(RT_ARCH_AMD64)
    ShutUpAppKit();
# endif

#ifdef Q_WS_WIN
    /* Initialize COM early, before QApplication calls OleInitialize(), to
     * make sure we enter the multi threded apartment instead of a single
     * threaded one. Note that this will make some non-threadsafe system
     * services that use OLE and require STA (such as Drag&Drop) not work
     * anymore, however it's still better because otherwise VBox will not work
     * on some Windows XP systems at all since it requires MTA (we cannot
     * leave STA by calling CoUninitialize() and re-enter MTA on those systems
     * for some unknown reason), see also src/VBox/Main/glue/initterm.cpp. */
    /// @todo find a proper solution that satisfies both OLE and VBox
    HRESULT hrc = COMBase::InitializeCOM();
#endif

#ifndef Q_WS_WIN
    int i;
    for (i=0; i<argc; i++)
        if (   !strcmp(argv[i], "-h")
            || !strcmp(argv[i], "-?")
            || !strcmp(argv[i], "-help")
            || !strcmp(argv[i], "--help"))
        {
            showHelp();
            return 0;
        }
#endif

#if defined(DEBUG) && defined(Q_WS_X11) && defined(RT_OS_LINUX)
    /* install our signal handler to backtrace the call stack */
    struct sigaction sa;
    sa.sa_sigaction = bt_sighandler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction (SIGSEGV, &sa, NULL);
    sigaction (SIGBUS, &sa, NULL);
    sigaction (SIGUSR1, &sa, NULL);
#endif

#ifdef QT_MAC_USE_COCOA
    /* Instantiate our NSApplication derivative before QApplication
     * forces NSApplication to be instantiated. */
    VBoxCocoaApplication_sharedApplication();
#endif

    qInstallMsgHandler (QtMessageOutput);

    int rc = 1; /* failure */

    /* scope the QIApplication variable */
    {
        QIApplication a (argc, argv);

        /* Qt4.3 version has the QProcess bug which freezing the application
         * for 30 seconds. This bug is internally used at initialization of
         * Cleanlooks style. So we have to change this style to another one.
         * See http://trolltech.com/developer/task-tracker/index_html?id=179200&method=entry
         * for details. */
        if (QString (qVersion()).startsWith ("4.3") &&
            qobject_cast <QCleanlooksStyle*> (QApplication::style()))
            QApplication::setStyle (new QPlastiqueStyle);

#ifdef Q_WS_X11
        /* This patch is not used for now on Solaris & OpenSolaris because
         * there is no anti-aliasing enabled by default, Qt4 to be rebuilt. */
#ifndef Q_OS_SOLARIS
        /* Cause Qt4 has the conflict with fontconfig application as a result
         * sometimes substituting some fonts with non scaleable-anti-aliased
         * bitmap font we are reseting substitutes for the current application
         * font family if it is non scaleable-anti-aliased. */
        QFontDatabase fontDataBase;

        QString currentFamily (QApplication::font().family());
        bool isCurrentScaleable = fontDataBase.isScalable (currentFamily);

        /*
        LogFlowFunc (("Font: Current family is '%s'. It is %s.\n",
            currentFamily.toLatin1().constData(),
            isCurrentScaleable ? "scalable" : "not scalable"));
        QStringList subFamilies (QFont::substitutes (currentFamily));
        foreach (QString sub, subFamilies)
        {
            bool isSubScalable = fontDataBase.isScalable (sub);
            LogFlowFunc (("Font: Substitute family is '%s'. It is %s.\n",
                sub.toLatin1().constData(),
                isSubScalable ? "scalable" : "not scalable"));
        }
        */

        QString subFamily (QFont::substitute (currentFamily));
        bool isSubScaleable = fontDataBase.isScalable (subFamily);

        if (isCurrentScaleable && !isSubScaleable)
            QFont::removeSubstitution (currentFamily);
#endif /* Q_OS_SOLARIS */
#endif

#ifdef Q_WS_WIN
        /* Drag in the sound drivers and DLLs early to get rid of the delay taking
         * place when the main menu bar (or any action from that menu bar) is
         * activated for the first time. This delay is especially annoying if it
         * happens when the VM is executing in real mode (which gives 100% CPU
         * load and slows down the load process that happens on the main GUI
         * thread to several seconds). */
        PlaySound (NULL, NULL, 0);
#endif

#ifdef Q_WS_MAC
        ::darwinDisableIconsInMenus();
#endif /* Q_WS_MAC */

#ifdef Q_WS_X11
        /* version check (major.minor are sensitive, fix number is ignored) */
        QString ver_str = QString::fromLatin1 (QT_VERSION_STR);
        QString ver_str_base = ver_str.section ('.', 0, 1);
        QString rt_ver_str = QString::fromLatin1 (qVersion());
        uint ver =
            (ver_str.section ('.', 0, 0).toInt() << 16) +
            (ver_str.section ('.', 1, 1).toInt() << 8) +
            ver_str.section ('.', 2, 2).toInt();
        uint rt_ver =
            (rt_ver_str.section ('.', 0, 0).toInt() << 16) +
            (rt_ver_str.section ('.', 1, 1).toInt() << 8) +
            rt_ver_str.section ('.', 2, 2).toInt();
        if (rt_ver < (ver & 0xFFFF00))
        {
            QString msg =
                QApplication::tr ("Executable <b>%1</b> requires Qt %2.x, found Qt %3.")
                                  .arg (qAppName())
                                  .arg (ver_str_base)
                                  .arg (rt_ver_str);
            QMessageBox::critical (
                0, QApplication::tr ("Incompatible Qt Library Error"),
                msg, QMessageBox::Abort, 0);
            qFatal ("%s", msg.toAscii().constData());
        }
#endif

        /* load a translation based on the current locale */
        VBoxGlobal::loadLanguage();

        do
        {
#ifdef Q_WS_WIN
            /* Check for the COM error after we've initialized Qt */
            if (FAILED (hrc))
            {
                vboxProblem().cannotInitCOM (hrc);
                break;
            }
#endif

            if (!vboxGlobal().isValid())
                break;

#ifndef VBOX_OSE
#ifdef Q_WS_X11
            /* show the user license file */
            if (!vboxGlobal().showVirtualBoxLicense())
                break;
#endif
#endif

            VBoxGlobalSettings settings = vboxGlobal().settings();
            /* Process known keys */
            bool noSelector = settings.isFeatureActive ("noSelector");

            if (vboxGlobal().isVMConsoleProcess())
            {
                vboxGlobal().setMainWindow (&vboxGlobal().consoleWnd());
#ifdef VBOX_GUI_WITH_SYSTRAY
                if (vboxGlobal().trayIconInstall())
                {
                    /* Nothing to do here yet. */
                }
#endif
                if (vboxGlobal().startMachine (vboxGlobal().managedVMUuid()))
                    rc = a.exec();
            }
            else if (noSelector)
            {
                vboxProblem().cannotRunInSelectorMode();
            }
            else
            {
                vboxGlobal().setMainWindow (&vboxGlobal().selectorWnd());
#ifdef VBOX_GUI_WITH_SYSTRAY
                if (vboxGlobal().trayIconInstall())
                {
                    /* Nothing to do here yet. */
                }

                if (false == vboxGlobal().isTrayMenu())
                {
#endif
                    vboxGlobal().selectorWnd().show();
#ifdef VBOX_WITH_REGISTRATION_REQUEST
                    vboxGlobal().showRegistrationDialog (false /* aForce */);
#endif
#ifdef VBOX_WITH_UPDATE_REQUEST
                    vboxGlobal().showUpdateDialog (false /* aForce */);
#endif
#ifdef VBOX_GUI_WITH_SYSTRAY
                }

                do
                {
#endif
                    rc = a.exec();
#ifdef VBOX_GUI_WITH_SYSTRAY
                } while (vboxGlobal().isTrayMenu());
#endif
            }
        }
        while (0);
    }

#ifdef Q_WS_WIN
    /* See COMBase::initializeCOM() above */
    if (SUCCEEDED (hrc))
        COMBase::CleanupCOM();
#endif

    LogFlowFunc (("rc=%d\n", rc));
    LogFlowFuncLeave();

    return rc;
}

#ifndef VBOX_WITH_HARDENING

int main (int argc, char **argv, char **envp)
{
    /* Initialize VBox Runtime. Initialize the SUPLib as well only if we
     * are really about to start a VM. Don't do this if we are only starting
     * the selector window. */
    bool fInitSUPLib = false;
    for (int i = 1; i < argc; i++)
    {
        if (!::strcmp (argv[i], "-startvm" ))
        {
            fInitSUPLib = true;
            break;
        }
    }

    if (!fInitSUPLib)
        RTR3Init();
    else
        RTR3InitAndSUPLib();

    return TrustedMain (argc, argv, envp);
}

#else  /* VBOX_WITH_HARDENING */

/**
 * Hardened main failed, report the error without any unnecessary fuzz.
 *
 * @remarks Do not call IPRT here unless really required, it might not be
 *          initialized.
 */
extern "C" DECLEXPORT(void) TrustedError (const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va)
{
# if defined(RT_OS_DARWIN) && defined(RT_ARCH_AMD64)
    ShutUpAppKit();
# endif

    /*
     * Init the Qt application object. This is a bit hackish as we
     * don't have the argument vector handy.
     */
    int argc = 0;
    char *argv[2] = { NULL, NULL };
    QApplication a (argc, &argv[0]);

    /*
     * Compose and show the error message.
     */
    QString msgTitle = QApplication::tr ("VirtualBox - Error In %1").arg (pszWhere);

    char msgBuf[1024];
    vsprintf (msgBuf, pszMsgFmt, va);

    QString msgText = QApplication::tr (
            "<html><b>%1 (rc=%2)</b><br/><br/>").arg (msgBuf).arg (rc);
    switch (enmWhat)
    {
        case kSupInitOp_Driver:
            msgText += QApplication::tr (
#ifdef RT_OS_LINUX
            "The VirtualBox Linux kernel driver (vboxdrv) is either not loaded or "
            "there is a permission problem with /dev/vboxdrv. Re-setup the kernel "
            "module by executing<br/><br/>"
            "  <font color=blue>'/etc/init.d/vboxdrv setup'</font><br/><br/>"
            "as root. Users of Ubuntu or Fedora should install the DKMS package "
            "at first. This package keeps track of Linux kernel changes and "
            "recompiles the vboxdrv kernel module if necessary."
#else
            "Make sure the kernel module has been loaded successfully."
#endif
            );
            break;
        case kSupInitOp_IPRT:
        case kSupInitOp_Integrity:
        case kSupInitOp_RootCheck:
            msgText += QApplication::tr ("It may help to reinstall VirtualBox."); /* hope this isn't (C), (TM) or (R) Microsoft support ;-) */
            break;
        default:
            /* no hints here */
            break;
    }
    msgText += "</html>";

#ifdef RT_OS_LINUX
    sleep(2);
#endif
    QMessageBox::critical (
        0,                      /* parent */
        msgTitle,               /* title */
        msgText,                /* text */
        QMessageBox::Abort,     /* button0 */
        0);                     /* button1 */
    qFatal ("%s", msgText.toAscii().constData());
}

#endif /* VBOX_WITH_HARDENING */

