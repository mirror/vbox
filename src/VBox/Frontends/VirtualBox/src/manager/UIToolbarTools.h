/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolbarTools class declaration.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolbarTools_h___
#define ___UIToolbarTools_h___

/* Qt includes: */
#include <QMap>
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"

/* Forward declarations: */
class QAction;
class QHBoxLayout;
class QUuid;
class QWidget;
class QIToolButton;
class UIActionPool;
class UITabBar;
class UIToolBar;


/** Tools toolbar imlementation for Selector UI. */
class UIToolbarTools : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about Machine tool of particular @a enmType opened. */
    void sigToolOpenedMachine(const ToolTypeMachine enmType);
    /** Notifies listeners about Global tool of particular @a enmType opened. */
    void sigToolOpenedGlobal(const ToolTypeGlobal enmType);

    /** Notifies listeners about Machine tool of particular @a enmType closed. */
    void sigToolClosedMachine(const ToolTypeMachine enmType);
    /** Notifies listeners about Global tool of particular @a enmType closed. */
    void sigToolClosedGlobal(const ToolTypeGlobal enmType);

public:

    /** Tab-bar types. */
    enum TabBarType { TabBarType_Machine, TabBarType_Global };

    /** Constructs Tools toolbar passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool to take corresponding actions from. */
    UIToolbarTools(UIActionPool *pActionPool, QWidget *pParent = 0);

    /** Switches to tab-bar of certain @a enmType. */
    void switchToTabBar(TabBarType enmType);

    /** Defines whether Machine tab-bar is @a fEnabled. */
    void setTabBarEnabledMachine(bool fEnabled);
    /** Defines whether Global tab-bar is @a fEnabled. */
    void setTabBarEnabledGlobal(bool fEnabled);

    /** Returns Machine tab-bar order. */
    QList<ToolTypeMachine> tabOrderMachine() const;
    /** Returns Global tab-bar order. */
    QList<ToolTypeGlobal> tabOrderGlobal() const;

private slots:

    /** Handles request to open Machine tool. */
    void sltHandleOpenToolMachine();
    /** Handles request to open Global tool. */
    void sltHandleOpenToolGlobal();

    /** Handles request to close Machine tool with passed @a uuid. */
    void sltHandleCloseToolMachine(const QUuid &uuid);
    /** Handles request to close Global tool with passed @a uuid. */
    void sltHandleCloseToolGlobal(const QUuid &uuid);

    /** Handles request to make Machine tool with passed @a uuid current one. */
    void sltHandleToolChosenMachine(const QUuid &uuid);
    /** Handles request to make Global tool with passed @a uuid current one. */
    void sltHandleToolChosenGlobal(const QUuid &uuid);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares menu. */
    void prepareMenu();
    /** Prepares widgets. */
    void prepareWidgets();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the main layout instance. */
    QHBoxLayout  *m_pLayoutMain;
    /** Holds the Machine tab-bar instance. */
    UITabBar     *m_pTabBarMachine;
    /** Holds the Global tab-bar instance. */
    UITabBar     *m_pTabBarGlobal;
    /** Holds the Machine tab-bar button. */
    QIToolButton *m_pButtonMachine;
    /** Holds the Global tab-bar button. */
    QIToolButton *m_pButtonGlobal;

    /** Holds the map of opened Machine tool IDs. */
    QMap<ToolTypeMachine, QUuid>  m_mapTabIdsMachine;
    /** Holds the map of opened Global tool IDs. */
    QMap<ToolTypeGlobal, QUuid>   m_mapTabIdsGlobal;
};

#endif /* !___UIToolbarTools_h___ */

