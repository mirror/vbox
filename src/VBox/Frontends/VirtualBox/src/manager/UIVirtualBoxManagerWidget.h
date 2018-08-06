/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManagerWidget class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVirtualBoxManagerWidget_h___
#define ___UIVirtualBoxManagerWidget_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"

/* Forward declarations: */
class QISplitter;
class UIActionPool;
class UIChooser;
class UISlidingWidget;
class UITabBar;
class UIToolBar;
class UIToolbarTools;
class UIVirtualBoxManager;
class UIVirtualMachineItem;

/** QWidget extension used as VirtualBox Manager Widget instance. */
class UIVirtualBoxManagerWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about Chooser-pane index change. */
    void sigChooserPaneIndexChange();
    /** Notifies about Chooser-pane group saving change. */
    void sigGroupSavingStateChanged();

    /** Notifies aboud Details-pane link clicked. */
    void sigMachineSettingsLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

    /** Notifies about Tool type switch. */
    void sigToolsTypeSwitch();

public:

    /** Constructs VirtualBox Manager widget. */
    UIVirtualBoxManagerWidget(UIVirtualBoxManager *pParent);
    /** Destructs VirtualBox Manager widget. */
    virtual ~UIVirtualBoxManagerWidget() /* override */;

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool instance. */
        UIActionPool *actionPool() const { return m_pActionPool; }

        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
        /** Returns whether all items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;
        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;

        /** Returns whether Machine tool of passed @a enmType is opened. */
        bool isToolOpened(ToolTypeMachine enmType) const;
        /** Returns whether Global tool of passed @a enmType is opened. */
        bool isToolOpened(ToolTypeGlobal enmType) const;
        /** Switches to Machine tool of passed @a enmType. */
        void switchToTool(ToolTypeMachine enmType);
        /** Switches to Global tool of passed @a enmType. */
        void switchToTool(ToolTypeGlobal enmType);
    /** @} */

public slots:

    /** @name Common stuff.
      * @{ */
        /** Handles context-menu request for passed @a position. */
        void sltHandleContextMenuRequest(const QPoint &position);
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        /** Handles first show @a pEvent. */
        virtual void polishEvent(QShowEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Common stuff.
      * @{ */
        /** Handles polishing in the async way. */
        void sltHandlePolishEvent();

        /** Handles signal about Chooser-pane index change.
          * @param  fUpdateDetails    Brings whether details should be updated.
          * @param  fUpdateSnapshots  Brings whether snapshots should be updated.
          * @param  fUpdateLogViewer  Brings whether log-viewer should be updated. */
        void sltHandleChooserPaneIndexChange(bool fUpdateDetails = true,
                                             bool fUpdateSnapshots = true,
                                             bool fUpdateLogViewer = true);
        /** Handles signal about Chooser-pane index change the default way. */
        void sltHandleChooserPaneIndexChangeDefault() { sltHandleChooserPaneIndexChange(); }
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Handles tools type switch. */
        void sltHandleToolsTypeSwitch();

        /** Handles request to show Machine tab-bar. */
        void sltHandleShowTabBarMachine();
        /** Handles request to show Global tab-bar. */
        void sltHandleShowTabBarGlobal();

        /** Handles rquest to open Machine tool of passed @a enmType. */
        void sltHandleToolOpenedMachine(ToolTypeMachine enmType);
        /** Handles rquest to open Global tool of passed @a enmType. */
        void sltHandleToolOpenedGlobal(ToolTypeGlobal enmType);

        /** Handles rquest to close Machine tool of passed @a enmType. */
        void sltHandleToolClosedMachine(ToolTypeMachine enmType);
        /** Handles rquest to close Global tool of passed @a enmType. */
        void sltHandleToolClosedGlobal(ToolTypeGlobal enmType);
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares window. */
        void prepare();
        /** Prepares toolbar. */
        void prepareToolbar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** Holds whether the dialog is polished. */
    bool  m_fPolished : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the sliding-widget isntance. */
    UISlidingWidget *m_pSlidingWidget;

    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;

    /** Holds the main toolbar instance. */
    UIToolBar *m_pToolBar;

    /** Holds the Machine tab-bar instance. */
    UITabBar *m_pTabBarMachine;
    /** Holds the Global tab-bar instance. */
    UITabBar *m_pTabBarGlobal;

    /** Holds the Tools-toolbar instance. */
    UIToolbarTools *m_pToolbarTools;

    /** Holds the Machine Tools order. */
    QList<ToolTypeMachine>  m_orderMachine;
    /** Holds the Global Tools order. */
    QList<ToolTypeGlobal>   m_orderGlobal;

    /** Holds the Chooser-pane instance. */
    UIChooser         *m_pPaneChooser;
    /** Holds the Machine Tools-pane instance. */
    UIToolPaneMachine *m_pPaneToolsMachine;
    /** Holds the Global Tools-pane instance. */
    UIToolPaneGlobal  *m_pPaneToolsGlobal;
};

#endif /* !___UIVirtualBoxManagerWidget_h___ */
