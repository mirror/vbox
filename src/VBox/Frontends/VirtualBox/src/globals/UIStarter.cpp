/* $Id$ */
/** @file
 * VBox Qt GUI - UIStarter class implementation.
 */

/*
 * Copyright (C) 2018-2022 Oracle Corporation
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
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
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
}

UIStarter::~UIStarter()
{
    /* Unassign instance: */
    s_pInstance = 0;
}

void UIStarter::init()
{
    /* Listen for UICommon signals: */
    connect(&uiCommon(), &UICommon::sigAskToRestartUI,
            this, &UIStarter::sltRestartUI);
    connect(&uiCommon(), &UICommon::sigAskToCloseUI,
            this, &UIStarter::sltCloseUI);
}

void UIStarter::deinit()
{
    /* Listen for UICommon signals no more: */
    disconnect(&uiCommon(), &UICommon::sigAskToRestartUI,
               this, &UIStarter::sltRestartUI);
    disconnect(&uiCommon(), &UICommon::sigAskToCloseUI,
               this, &UIStarter::sltCloseUI);
}

void UIStarter::sltStartUI()
{
    /* Exit if UICommon is not valid: */
    if (!uiCommon().isValid())
        return;

#ifndef VBOX_RUNTIME_UI

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
    UINotificationMessage::remindAboutExperimentalBuild();
# else /* !VBOX_BLEEDING_EDGE */
#  ifndef DEBUG
    /* Show BETA warning if necessary: */
    const QString vboxVersion(uiCommon().virtualBox().GetVersion());
    if (   vboxVersion.contains("BETA")
        && gEDataManager->preventBetaBuildWarningForVersion() != vboxVersion)
        UINotificationMessage::remindAboutBetaBuild();
#  endif /* !DEBUG */
# endif /* !VBOX_BLEEDING_EDGE */

#else /* VBOX_RUNTIME_UI */

    /* Make sure Runtime UI is even possible, quit if not: */
    if (uiCommon().managedVMUuid().isNull())
    {
        msgCenter().cannotStartRuntime();
        return QApplication::quit();
    }

    /* Make sure machine is started, quit if not: */
    if (!UIMachine::startMachine(uiCommon().managedVMUuid()))
        return QApplication::quit();

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

void UIStarter::sltCloseUI()
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
