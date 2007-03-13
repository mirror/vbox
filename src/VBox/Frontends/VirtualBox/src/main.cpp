/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * The main() function
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#include <iprt/runtime.h>
#include <iprt/stream.h>

/// @todo (dmik) later
///* German NLS is builtin */
//#include <vboxgui_de.h>

#if defined(DEBUG) && defined(Q_WS_X11)

#include <signal.h>
#include <execinfo.h>

/* get REG_EIP from ucontext.h */
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <ucontext.h>
#ifdef __AMD64__
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

#ifdef __DARWIN__
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/env.h>

/**
 * App bundle tweaks.
 */
static void DarwinInit (void)
{
    /* This probably belongs else where. */
    static struct
    {
        char var[sizeof ("VBOX_XPCOM_HOME=") - 1];
        char path[RTPATH_MAX];
    } s;

    strcpy (s.var, "VBOX_XPCOM_HOME=");
    int rc = RTPathProgram (&s.path[0], sizeof (s.path));
    if (RT_FAILURE (rc))
    {
        RTPrintf ("RTPathProgram failed!\n");
        exit (1);
    }

    if (!RTEnvGet ("VBOX_XPCOM_HOME"))
        RTEnvPut (s.var);

    /** @todo automatically start VBoxSVC. */
}

#endif


int main( int argc, char ** argv )
{
    /* initialize VBox Runtime */
    RTR3Init (true, ~(size_t)0);

    LogFlowFuncEnter();

#if defined(DEBUG) && defined(Q_WS_X11)
    /* install our signal handler to backtrace the call stack */
    struct sigaction sa;
    sa.sa_sigaction = bt_sighandler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction (SIGSEGV, &sa, NULL);
    sigaction (SIGBUS, &sa, NULL);
    sigaction (SIGUSR1, &sa, NULL);
#endif

#ifdef __DARWIN__
    DarwinInit();
#endif

    qInstallMsgHandler (QtMessageOutput);

    QIApplication a (argc, argv);

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
    if (rt_ver < (ver & 0xFFFF00)) {
        QString msg =
            QApplication::tr ("Executable <b>%1</b> requires Qt %2.x, found Qt %3.")
                .arg (QString::fromLatin1 (qAppName()))
                .arg (ver_str_base)
                .arg (rt_ver_str);
        QMessageBox::critical (
            0, QApplication::tr ("Incompatible Qt Library Error"),
            msg, QMessageBox::Abort, 0
        );
        qFatal (msg.ascii());
    }
#endif

/// @todo (dmik) later
//    QTranslator translator(0);
//    // German is builtin
//    if (strncmp(QTextCodec::locale(), "de", 2) == 0)
//    {
//        translator.load((const uchar*)vboxgui_de_qm_data, vboxgui_de_qm_len);
//    } else
//    {
//        // try to load from the current directory
//        translator.load(QString("vboxgui_") + QTextCodec::locale(), ".");
//    }
//    a.installTranslator(&translator);

    int rc = 1;

    if (vboxGlobal().isValid())
    {
        VMGlobalSettings settings = vboxGlobal().settings();
        /* Searching for known keys */
        const char *noSelector = "noSelector";
        bool forcedConsole = settings.isFeatureActivated (noSelector);

        if (vboxGlobal().isVMConsoleProcess())
        {
            a.setMainWidget( &vboxGlobal().consoleWnd());
            if (vboxGlobal().startMachine (vboxGlobal().managedVMUuid()))
                rc = a.exec();
        }
        else if (forcedConsole)
        {
            vboxProblem().cannotRunInSelectorMode();
        }
        else
        {
            a.setMainWidget (&vboxGlobal().selectorWnd());
            vboxGlobal().selectorWnd().show();
            vboxGlobal().startEnumeratingMedia();
            rc = a.exec();
        }
    }

    LogFlowFunc (("rc=%d\n", rc));
    LogFlowFuncLeave();

    return rc;
}
