/* $Id$ */
/** @file
 * VBox Qt GUI - UIStarter class implementation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || !defined(VBOX_RUNTIME_UI)
# ifdef VBOX_GUI_WITH_NEW_MANAGER
#  include "UIVirtualBoxManager.h"
# else
#  include "UISelectorWindow.h"
# endif
#endif
#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || defined(VBOX_RUNTIME_UI)
# include "UIMachine.h"
# include "UISession.h"
# endif


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

#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || !defined(VBOX_RUNTIME_UI)
    /* Show Selector UI: */
    if (!vboxGlobal().isVMConsoleProcess())
    {
        /* Make sure Selector UI is permitted, quit if not: */
        if (gEDataManager->guiFeatureEnabled(GUIFeatureType_NoSelector))
        {
            msgCenter().cannotStartSelector();
            return QApplication::quit();
        }

# ifdef VBOX_GUI_WITH_NEW_MANAGER
        /* Create/show manager-window: */
        UIVirtualBoxManager::create();
# else
        /* Create/show selector-window: */
        UISelectorWindow::create();
# endif

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
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || !VBOX_RUNTIME_UI */

#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || defined(VBOX_RUNTIME_UI)
    /* Show Runtime UI: */
    if (vboxGlobal().isVMConsoleProcess())
    {
        /* Make sure machine is started, quit if not: */
        if (!UIMachine::startMachine(vboxGlobal().managedVMUuid()))
            return QApplication::quit();
    }
# if defined(VBOX_GUI_WITH_SHARED_LIBRARY) && defined(VBOX_RUNTIME_UI)
    /* Show the error message otherwise and quit: */
    else
    {
        msgCenter().cannotStartRuntime();
        return QApplication::quit();
    }
# endif /* VBOX_GUI_WITH_SHARED_LIBRARY && VBOX_RUNTIME_UI */
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || VBOX_RUNTIME_UI */
}

void UIStarter::sltRestartUI()
{
#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || !defined(VBOX_RUNTIME_UI)
# ifdef VBOX_GUI_WITH_NEW_MANAGER
    /* Recreate/show manager-window: */
    UIVirtualBoxManager::destroy();
    UIVirtualBoxManager::create();
# else
    /* Recreate/show selector-window: */
    UISelectorWindow::destroy();
    UISelectorWindow::create();
# endif
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || !VBOX_RUNTIME_UI */
}

void UIStarter::cleanup()
{
#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || !defined(VBOX_RUNTIME_UI)
# ifdef VBOX_GUI_WITH_NEW_MANAGER
    /* Destroy Manager UI: */
    if (gpManager)
        UIVirtualBoxManager::destroy();
# else
    /* Destroy Selector UI: */
    if (gpSelectorWindow)
        UISelectorWindow::destroy();
# endif
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || !VBOX_RUNTIME_UI */

#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || defined(VBOX_RUNTIME_UI)
    /* Destroy Runtime UI: */
    if (gpMachine)
        UIMachine::destroy();
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || VBOX_RUNTIME_UI */
}

void UIStarter::sltOpenURLs()
{
#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || !defined(VBOX_RUNTIME_UI)
# ifdef VBOX_GUI_WITH_NEW_MANAGER
    /* Create/show manager-window: */
    UIVirtualBoxManager::create();

    /* Ask the Manager UI to open URLs asynchronously: */
    QMetaObject::invokeMethod(gpManager, "sltHandleOpenUrlCall", Qt::QueuedConnection);
# else
    /* Create/show selector-window: */
    UISelectorWindow::create();

    /* Ask the Selector UI to open URLs asynchronously: */
    QMetaObject::invokeMethod(gpSelectorWindow, "sltOpenUrls", Qt::QueuedConnection);
# endif
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || !VBOX_RUNTIME_UI */
}

void UIStarter::sltHandleCommitDataRequest()
{
    /* Exit if VBoxGlobal is not valid: */
    if (!vboxGlobal().isValid())
        return;

#if !defined(VBOX_GUI_WITH_SHARED_LIBRARY) || defined(VBOX_RUNTIME_UI)
    /* For VM process: */
    if (vboxGlobal().isVMConsoleProcess())
    {
        /* Temporary override the default close action to 'SaveState' if necessary: */
        if (gpMachine->uisession()->defaultCloseAction() == MachineCloseAction_Invalid)
            gpMachine->uisession()->setDefaultCloseAction(MachineCloseAction_SaveState);
    }
#endif /* !VBOX_GUI_WITH_SHARED_LIBRARY || VBOX_RUNTIME_UI */
}
