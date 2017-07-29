/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsToolbar class declaration.
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

#ifndef ___UIToolsToolbar_h___
#define ___UIToolsToolbar_h___

/* Qt includes: */
#include <QMap>
#include <QWidget>

/* Forward declarations: */
class QAction;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QStackedLayout;
class QUuid;
class QWidget;
class QIToolButton;
class UIActionPool;
class UITabBar;


/** Machine tool types. */
enum ToolTypeMachine
{
    ToolTypeMachine_Invalid,
    ToolTypeMachine_Desktop,
    ToolTypeMachine_Details,
    ToolTypeMachine_Snapshots,
};

/* Make sure QVariant can eat ToolTypeMachine: */
Q_DECLARE_METATYPE(ToolTypeMachine);


/** Tools toolbar imlementation for Selector UI. */
class UIToolsToolbar : public QWidget
{
    Q_OBJECT;

signals:

    /** Notify listeners about Machine tool of particular @a enmType opened. */
    void sigToolOpenedMachine(const ToolTypeMachine enmType);

    /** Notify listeners about Machine tool of particular @a enmType closed. */
    void sigToolClosedMachine(const ToolTypeMachine enmType);

public:

    /** Tools action types. */
    enum ActionType { ActionType_Machine };

    /** Constructs Tools toolbar passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool to take corresponding actions from. */
    UIToolsToolbar(UIActionPool *pActionPool, QWidget *pParent = 0);

private slots:

    /** Handles request to open Machine tool. */
    void sltHandleOpenToolMachine();

    /** Handles request to close Machine tool with passed @a uuid. */
    void sltHandleCloseToolMachine(const QUuid &uuid);

    /** Handles request to make Machine tool with passed @a uuid current one. */
    void sltHandleToolChosenMachine(const QUuid &uuid);

    /** Handles button toggle. */
    void sltHandleButtonToggle();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares menu. */
    void prepareMenu();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares section button. */
    static QIToolButton *prepareSectionButton(QAction *pAction);
    /** Prepares connections. */
    void prepareConnections();

    /** Rebuilds toolbar shape. */
    void rebuildShape();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the main layout instance. */
    QGridLayout    *m_pLayoutMain;
    /** Holds the stacked layout instance. */
    QStackedLayout *m_pLayoutStacked;
    /** Holds the button layout instance. */
    QHBoxLayout    *m_pLayoutButton;

    /** Holds the Machine tab-bar instance. */
    UITabBar                     *m_pTabBarMachine;
    /** Holds the map of opened Machine tool IDs. */
    QMap<ToolTypeMachine, QUuid>  m_mapTabIdsMachine;

    /** Holds the map of action wrapping buttons. */
    QMap<ActionType, QIToolButton*>  m_mapButtons;

    /** Holds the status label instance. */
    QLabel *m_pLabel;

    /** Holds the 'Machine' menu action instances. */
    QMap<ToolTypeMachine, QAction*>  m_mapActionsMachine;
};

#endif /* !___UIToolsToolbar_h___ */

