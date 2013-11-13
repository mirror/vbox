/** @file
 * VBox Qt GUI - UIMachineMenuBar class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineMenuBar_h___
#define ___UIMachineMenuBar_h___

/* Qt includes: */
#include <QList>

/* GUI includes: */
#include "UIDefs.h"

/* Forward declarations: */
class QMenu;
class QMenuBar;
class UISession;

/**
 * Menubar factory for virtual machine (Runtime UI).
 * Provides client with the new menu/menubar whenever it necessary.
 */
class UIMachineMenuBar
{
public:

    /** Constructor. Stores UI session pointer for further needs. */
    UIMachineMenuBar(UISession *pSession);

    /** Provides client with new menu. */
    QMenu* createMenu(RuntimeMenuType fOptions = RuntimeMenuType_All);
    /** Provides client with new menubar. */
    QMenuBar* createMenuBar(RuntimeMenuType fOptions = RuntimeMenuType_All);

private:

    /** Populates all the sub-menus client need. */
    QList<QMenu*> prepareSubMenus(RuntimeMenuType fOptions = RuntimeMenuType_All);
    /** Populates <b>Machine</b> sub-menu. */
    void prepareMenuMachine(QMenu *pMenu);
    /** Populates <b>View</b> sub-menu. */
    void prepareMenuView(QMenu *pMenu);
    /** Populates <b>Devices</b> sub-menu. */
    void prepareMenuDevices(QMenu *pMenu);
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Populates <b>Debug</b> sub-menu. */
    void prepareMenuDebug(QMenu *pMenu);
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Populates <b>Help</b> sub-menu. */
    void prepareMenuHelp(QMenu *pMenu);

    /** Contains pointer to parent UI session. */
    UISession *m_pSession;
};

#endif /* !___UIMachineMenuBar_h___ */

