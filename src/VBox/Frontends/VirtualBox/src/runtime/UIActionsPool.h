/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionsPool class declaration
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

#ifndef __UIActionsPool_h__
#define __UIActionsPool_h__

/* Global includes */
#include <QAction>

/* Local includes */
#include "QIWithRetranslateUI.h"

enum UIActionType
{
    UIActionType_Separator,
    UIActionType_Simple,
    UIActionType_Toggle,
    UIActionType_Menu
};

class UIAction : public QIWithRetranslateUI3<QAction>
{
    Q_OBJECT;

public:

    UIAction(QObject *pParent, UIActionType type);

    UIActionType type() const;

private:

    UIActionType m_type;
};

enum UIActionIndex
{
    /* Common actions */
    UIActionIndex_Separator,

    /* "Machine" menu actions: */
    UIActionIndex_Menu_Machine,
    UIActionIndex_Toggle_Fullscreen,
    UIActionIndex_Toggle_Seamless,
    UIActionIndex_Toggle_GuestAutoresize,
    UIActionIndex_Simple_AdjustWindow,
    UIActionIndex_Menu_MouseIntegration,
    UIActionIndex_Toggle_MouseIntegration,
    UIActionIndex_Simple_TypeCAD,
#ifdef Q_WS_X11
    UIActionIndex_Simple_TypeCABS,
#endif
    UIActionIndex_Simple_TakeSnapshot,
    UIActionIndex_Simple_InformationDialog,
    UIActionIndex_Toggle_Pause,
    UIActionIndex_Simple_Reset,
    UIActionIndex_Simple_Shutdown,
    UIActionIndex_Simple_Close,

    /* "Devices" menu actions: */
    UIActionIndex_Menu_Devices,
    UIActionIndex_Menu_OpticalDevices,
    UIActionIndex_Menu_FloppyDevices,
    UIActionIndex_Menu_USBDevices,
    UIActionIndex_Menu_NetworkAdapters,
    UIActionIndex_Simple_NetworkAdaptersDialog,
    UIActionIndex_Menu_SharedFolders,
    UIActionIndex_Simple_SharedFoldersDialog,
    UIActionIndex_Toggle_VRDP,
    UIActionIndex_Simple_InstallGuestTools,

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* "Debugger" menu actions: */
    UIActionIndex_Menu_Debug,
    UIActionIndex_Simple_Statistics,
    UIActionIndex_Simple_CommandLine,
    UIActionIndex_Toggle_Logging,
#endif

    UIActionIndex_End
};

class UIActionsPool : public QObject
{
    Q_OBJECT;

public:

    UIActionsPool(QObject *pParent);
    virtual ~UIActionsPool();

    UIAction* action(UIActionIndex index) const;

    bool processHotKey(const QKeySequence &key);

protected:

    bool event(QEvent *pEvent);

private:

    QVector<UIAction*> m_actionsPool;
};

#endif // __UIActionsPool_h__

