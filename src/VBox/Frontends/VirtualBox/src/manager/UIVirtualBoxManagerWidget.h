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
#include "UISlidingAnimation.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"

/* Forward declarations: */
class QStackedWidget;
class QISplitter;
class UIActionPool;
class UIChooser;
class UITabBar;
class UIToolBar;
class UITools;
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

    /** Notifies about Tool type change. */
    void sigToolTypeChange();

public:

    /** Constructs VirtualBox Manager widget. */
    UIVirtualBoxManagerWidget(UIVirtualBoxManager *pParent);
    /** Destructs VirtualBox Manager widget. */
    virtual ~UIVirtualBoxManagerWidget() /* override */;

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool instance. */
        UIActionPool *actionPool() const { return m_pActionPool; }

        /** Returns whether group current-item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether global current-item is selected. */
        bool isGlobalItemSelected() const;
        /** Returns whether machine current-item is selected. */
        bool isMachineItemSelected() const;
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

        /** Returns a type of curent Machine tool. */
        ToolTypeMachine currentMachineTool() const;
        /** Returns a type of curent Global tool. */
        ToolTypeGlobal currentGlobalTool() const;
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
    /** @} */

private slots:

    /** @name Common stuff.
      * @{ */
        /** Handles signal about Chooser-pane index change.
          * @param  fUpdateDetails    Brings whether details should be updated.
          * @param  fUpdateSnapshots  Brings whether snapshots should be updated.
          * @param  fUpdateLogViewer  Brings whether log-viewer should be updated. */
        void sltHandleChooserPaneIndexChange(bool fUpdateDetails = true,
                                             bool fUpdateSnapshots = true,
                                             bool fUpdateLogViewer = true);
        /** Handles signal about Chooser-pane index change the default way. */
        void sltHandleChooserPaneIndexChangeDefault() { sltHandleChooserPaneIndexChange(); }

        /** Handles sliding animation complete signal.
          * @param  enmDirection  Brings which direction was animation finished for. */
        void sltHandleSlidingAnimationComplete(SlidingDirection enmDirection);
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Handles signal abour Tools-pane index change. */
        void sltHandleToolsPaneIndexChange();
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

        /** Update toolbar. */
        void updateToolbar();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;

    /** Holds the main toolbar instance. */
    UIToolBar *m_pToolBar;

    /** Holds the Chooser-pane instance. */
    UIChooser          *m_pPaneChooser;
    /** Holds the stacked-widget. */
    QStackedWidget     *m_pStackedWidget;
    /** Holds the Global Tools-pane instance. */
    UIToolPaneGlobal   *m_pPaneToolsGlobal;
    /** Holds the Machine Tools-pane instance. */
    UIToolPaneMachine  *m_pPaneToolsMachine;
    /** Holds the sliding-animation widget instance. */
    UISlidingAnimation *m_pSlidingAnimation;
    /** Holds the Tools-pane instance. */
    UITools            *m_pPaneTools;
};

#endif /* !___UIVirtualBoxManagerWidget_h___ */
