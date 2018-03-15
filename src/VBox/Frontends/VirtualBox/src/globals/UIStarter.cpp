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
#include "UIMachine.h"
#include "UIMessageCenter.h"
#include "UISelectorWindow.h"
#include "UIStarter.h"


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

void UIStarter::prepare()
{
    /* Listen for QApplication signals: */
    connect(qApp, &QGuiApplication::aboutToQuit,
            this, &UIStarter::sltDestroyUI);
}

void UIStarter::init()
{
    /* Listen for VBoxGlobal signals: */
    connect(&vboxGlobal(), &VBoxGlobal::sigAskToRestartUI,
            this, &UIStarter::sltRestartUI);
    connect(&vboxGlobal(), &VBoxGlobal::sigAskToOpenURLs,
            this, &UIStarter::sltOpenURLs);
}

void UIStarter::deinit()
{
    /* Listen for VBoxGlobal signals no more: */
    disconnect(&vboxGlobal(), &VBoxGlobal::sigAskToRestartUI,
               this, &UIStarter::sltRestartUI);
    disconnect(&vboxGlobal(), &VBoxGlobal::sigAskToOpenURLs,
               this, &UIStarter::sltOpenURLs);
}

void UIStarter::cleanup()
{
    /* Destroy UI if there is still something to: */
    sltDestroyUI();
}

void UIStarter::sltShowUI()
{
    /* Exit if VBoxGlobal is not valid: */
    if (!vboxGlobal().isValid())
        return;

    /* Show Selector UI: */
    if (!vboxGlobal().isVMConsoleProcess())
    {
        /* Make sure Selector UI is permitted, quit if not: */
        if (gEDataManager->guiFeatureEnabled(GUIFeatureType_NoSelector))
        {
            msgCenter().cannotStartSelector();
            return QApplication::quit();
        }

        /* Create/show selector-window: */
        UISelectorWindow::create();

#ifdef VBOX_BLEEDING_EDGE
        /* Show EXPERIMENTAL BUILD warning: */
        msgCenter().showExperimentalBuildWarning();
#else /* !VBOX_BLEEDING_EDGE */
# ifndef DEBUG
        /* Show BETA warning if necessary: */
        const QString vboxVersion(vboxGlobal().virtualBox().GetVersion());
        if (   vboxVersion.contains("BETA")
            && gEDataManager->preventBetaBuildWarningForVersion() != vboxVersion)
            msgCenter().showBetaBuildWarning();
# endif /* !DEBUG */
#endif /* !VBOX_BLEEDING_EDGE */
    }
    /* Show Runtime UI: */
    else
    {
        /* Make sure machine is started, quit if not: */
        if (!UIMachine::startMachine(vboxGlobal().managedVMUuid()))
            return QApplication::quit();
    }
}

void UIStarter::sltRestartUI()
{
    /* Recreate/show selector-window: */
    UISelectorWindow::destroy();
    UISelectorWindow::create();
}

void UIStarter::sltDestroyUI()
{
    /* Destroy the root GUI windows: */
    if (gpSelectorWindow)
        UISelectorWindow::destroy();
    if (gpMachine)
        UIMachine::destroy();
}

void UIStarter::sltOpenURLs()
{
    /* Create/show selector-window: */
    UISelectorWindow::create();

    /* Ask the Selector UI to open URLs asynchronously: */
    QMetaObject::invokeMethod(gpSelectorWindow, "sltOpenUrls", Qt::QueuedConnection);
}

