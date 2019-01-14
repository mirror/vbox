/* $Id$ */
/** @file
 * VBox Qt GUI - UIStarter class implementation.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UIStarter.h"
#ifndef VBOX_RUNTIME_UI
# include "UIVirtualBoxManager.h"
#else
# include "UIMachine.h"
# include "UISession.h"
#endif


/* static */
UIStarter *UIStarter::s_pInstance = 0;

/* static */
void UIStarter::create()
{
    /* Pretect versus double 'new': */
    if (s_pInstance)
        return;

    /* Create instance: */
    new UIStarter;
}

/* static */
void UIStarter::destroy()
{
    /* Pretect versus double 'delete': */
    if (!s_pInstance)
        return;

    /* Destroy instance: */
    delete s_pInstance;
}

UIStarter::UIStarter()
{
    /* Assign instance: */
    s_pInstance = this;

    /* Prepare: */
    prepare();
}

UIStarter::~UIStarter()
{
    /* Cleanup: */
    cleanup();

    /* Unassign instance: */
    s_pInstance = 0;
}

void UIStarter::init()
{
    /* Listen for VBoxGlobal signals: */
    connect(&vboxGlobal(), &VBoxGlobal::sigAskToRestartUI,
            this, &UIStarter::sltRestartUI);
    connect(&vboxGlobal(), &VBoxGlobal::sigAskToOpenURLs,
            this, &UIStarter::sltOpenURLs);
    connect(&vboxGlobal(), &VBoxGlobal::sigAskToCommitData,
            this, &UIStarter::sltHandleCommitDataRequest);
}

void UIStarter::deinit()
{
    /* Listen for VBoxGlobal signals no more: */
    disconnect(&vboxGlobal(), &VBoxGlobal::sigAskToRestartUI,
               this, &UIStarter::sltRestartUI);
    disconnect(&vboxGlobal(), &VBoxGlobal::sigAskToOpenURLs,
               this, &UIStarter::sltOpenURLs);
    disconnect(&vboxGlobal(), &VBoxGlobal::sigAskToCommitData,
               this, &UIStarter::sltHandleCommitDataRequest);
}

void UIStarter::prepare()
{
    /* Listen for QApplication signals: */
    connect(qApp, &QGuiApplication::aboutToQuit,
            this, &UIStarter::cleanup);
}

void UIStarter::sltStartUI()
{
    /* Exit if VBoxGlobal is not valid: */
    if (!vboxGlobal().isValid())
        return;

#ifndef VBOX_RUNTIME_UI

    /* Show Selector UI: */
    if (!vboxGlobal().isVMConsoleProcess())
    {
        /* Make sure Selector UI is permitted, quit if not: */
        if (gEDataManager->guiFeatureEnabled(GUIFeatureType_NoSelector))
        {
            msgCenter().cannotStartSelector();
            return QApplication::quit();
        }

        /* Create/show manager-window: */
        UIVirtualBoxManager::create();

# ifdef VBOX_BLEEDING_EDGE
        /* Show EXPERIMENTAL BUILD warning: */
        msgCenter().showExperimentalBuildWarning();
# else /* !VBOX_BLEEDING_EDGE */
#  ifndef DEBUG
        /* Show BETA warning if necessary: */
        const QString vboxVersion(vboxGlobal().virtualBox().GetVersion());
        if (   vboxVersion.contains("BETA")
            && gEDataManager->preventBetaBuildWarningForVersion() != vboxVersion)
            msgCenter().showBetaBuildWarning();
#  endif /* !DEBUG */
# endif /* !VBOX_BLEEDING_EDGE */
    }

#else /* VBOX_RUNTIME_UI */

    /* Show Runtime UI: */
    if (vboxGlobal().isVMConsoleProcess())
    {
        /* Make sure machine is started, quit if not: */
        if (!UIMachine::startMachine(vboxGlobal().managedVMUuid()))
            return QApplication::quit();
    }
    /* Show the error message otherwise and quit: */
    else
    {
        msgCenter().cannotStartRuntime();
        return QApplication::quit();
    }

#endif /* VBOX_RUNTIME_UI */
}

void UIStarter::sltRestartUI()
{
#ifndef VBOX_RUNTIME_UI
    /* Recreate/show manager-window: */
    UIVirtualBoxManager::destroy();
    UIVirtualBoxManager::create();
#endif
}

void UIStarter::cleanup()
{
#ifndef VBOX_RUNTIME_UI
    /* Destroy Manager UI: */
    if (gpManager)
        UIVirtualBoxManager::destroy();
#else
    /* Destroy Runtime UI: */
    if (gpMachine)
        UIMachine::destroy();
#endif
}

void UIStarter::sltOpenURLs()
{
#ifndef VBOX_RUNTIME_UI
    /* Create/show manager-window: */
    UIVirtualBoxManager::create();

    /* Ask the Manager UI to open URLs asynchronously: */
    QMetaObject::invokeMethod(gpManager, "sltHandleOpenUrlCall", Qt::QueuedConnection);
#endif
}

void UIStarter::sltHandleCommitDataRequest()
{
    /* Exit if VBoxGlobal is not valid: */
    if (!vboxGlobal().isValid())
        return;

#ifdef VBOX_RUNTIME_UI
    /* For VM process: */
    if (vboxGlobal().isVMConsoleProcess())
    {
        /* Temporary override the default close action to 'SaveState' if necessary: */
        if (gpMachine->uisession()->defaultCloseAction() == MachineCloseAction_Invalid)
            gpMachine->uisession()->setDefaultCloseAction(MachineCloseAction_SaveState);
    }
#endif
}
