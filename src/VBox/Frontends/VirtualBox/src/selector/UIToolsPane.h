/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsPane class declaration.
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

#ifndef ___UIToolsPane_h___
#define ___UIToolsPane_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAction;
class QHBoxLayout;
class QMenu;
class QStackedLayout;
class QTabBar;
class QVBoxLayout;
class UIMenuToolBar;
class UISnapshotPane;
class CMachine;


/** Tool types. */
enum ToolType
{
    ToolType_Invalid,
    ToolType_SnapshotManager,
    ToolType_VirtualMediaManager,
    ToolType_HostNetworkManager,
};

/* Make sure QVariant can eat ToolType: */
Q_DECLARE_METATYPE(ToolType);


/** QWidget subclass representing container for tool panes. */
class UIToolsPane : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolsPane(QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolsPane() /* override */;

    /** Defines the @a comMachine object. */
    void setMachine(const CMachine &comMachine);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles menu-toolbar trigger. */
    void sltHandleMenuToolbarTrigger();

    /** Handles Tools tab-bar tab movement. */
    void sltHandleTabBarTabMoved(int iFrom, int iTo);
    /** Handles Tools tab-bar current tab change. */
    void sltHandleTabBarCurrentChange(int iIndex);
    /** Handles Tools tab-bar button click. */
    void sltHandleTabBarButtonClick();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Prepares tab-bar. */
    void prepareTabBar();
    /** Prepares menu-toolbar. */
    void prepareMenuToolbar();
    /** Prepares menu. */
    void prepareMenu();
    /** Cleanups all. */
    void cleanup();

    /** Activates corresponding tab-bar tab, adds new if necessary. */
    void activateTabBarTab(ToolType enmType, bool fCloseable);

    /** Holds the main layout isntance. */
    QVBoxLayout               *m_pLayoutMain;
    /** Holds the stacked-layout instance. */
    QStackedLayout            *m_pStackedLayout;
    /** Holds the snapshot pane. */
    UISnapshotPane            *m_pPaneSnapshots;
    /** Holds the controls layout instance. */
    QHBoxLayout               *m_pLayoutControls;
    /** Holds the tab-bar instance. */
    QTabBar                   *m_pTabBar;
    /** Holds the menu-toolbar instance. */
    UIMenuToolBar             *m_pMenuToolbar;
    /** Holds the menu instance. */
    QMenu                     *m_pMenu;
    /** Holds the menu action instances. */
    QMap<ToolType, QAction*>  m_actions;
};

#endif /* !___UIToolsPane_h___ */

