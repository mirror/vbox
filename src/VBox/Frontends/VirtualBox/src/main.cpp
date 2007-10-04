/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * The main() function
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxSelectorWnd.h"
#include "VBoxConsoleWnd.h"
#ifdef Q_WS_MAC
# include "QIApplication.h"
#else
# define QIApplication QApplication
#endif

#include <qmessagebox.h>
#include <qlocale.h>
#include <qtranslator.h>

#include <iprt/runtime.h>
#include <iprt/stream.h>

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

#endif

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
        case QtFatalMsg:
            Log (("Qt FATAL: %s\n", msg));
#ifdef Q_WS_X11
            RTStrmPrintf(g_pStdErr, "Qt FATAL: %s\n", msg);
#endif
    }
}

int main (int argc, char **argv)
{
    /* initialize VBox Runtime */
    RTR3Init (true, ~(size_t)0);

    LogFlowFuncEnter();

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
    HRESULT hrc = COMBase::initializeCOM();
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

    qInstallMsgHandler (QtMessageOutput);

    int rc = 1; /* failure */

    /* scope the QIApplication variable */
    {
        QIApplication a (argc, argv);

#ifdef Q_WS_WIN
        /* Drag in the sound drivers and DLLs early to get rid of the delay taking
         * place when the main menu bar (or any action from that menu bar) is
         * activated for the first time. This delay is especially annoying if it
         * happens when the VM is executing in real mode (which gives 100% CPU
         * load and slows down the load process that happens on the main GUI
         * thread to several seconds). */
        PlaySound (NULL, NULL, 0);
#endif

#ifndef RT_OS_DARWIN
        /* some gui qt-styles has it's own different color for buttons
         * causing tool-buttons and dropped menu displayed in
         * different annoying color, so fixing palette button's color */
        QPalette pal = a.palette();
        pal.setColor (QPalette::Disabled, QColorGroup::Button,
                      pal.color (QPalette::Disabled, QColorGroup::Background));
        pal.setColor (QPalette::Active, QColorGroup::Button,
                      pal.color (QPalette::Active, QColorGroup::Background));
        pal.setColor (QPalette::Inactive, QColorGroup::Button,
                      pal.color (QPalette::Inactive, QColorGroup::Background));
        a.setPalette (pal);
#endif

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
                                  .arg (QString::fromLatin1 (qAppName()))
                                  .arg (ver_str_base)
                                  .arg (rt_ver_str);
            QMessageBox::critical (
                0, QApplication::tr ("Incompatible Qt Library Error"),
                msg, QMessageBox::Abort, 0);
            qFatal (msg.ascii());
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
                a.setMainWidget (&vboxGlobal().consoleWnd());
                if (vboxGlobal().startMachine (vboxGlobal().managedVMUuid()))
                {
                    vboxGlobal().showRegistrationDialog (false /* aForce */);
                    rc = a.exec();
                }
            }
            else if (noSelector)
            {
                vboxProblem().cannotRunInSelectorMode();
            }
            else
            {
                a.setMainWidget (&vboxGlobal().selectorWnd());
                vboxGlobal().showRegistrationDialog (false /* aForce */);
                vboxGlobal().selectorWnd().show();
                vboxGlobal().startEnumeratingMedia();
                rc = a.exec();
            }
        }
        while (0);
    }

#ifdef Q_WS_WIN
    /* See COMBase::initializeCOM() above */
    if (SUCCEEDED (hrc))
        COMBase::cleanupCOM();
#endif

    LogFlowFunc (("rc=%d\n", rc));
    LogFlowFuncLeave();

    return rc;
}
