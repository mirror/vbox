/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserModel class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserModel_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserModel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QPointer>

/* GUI includes: */
#include "UIChooserAbstractModel.h"
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudMachine.h"
#include "CMachine.h"

/* Forward declaration: */
class QDrag;
class UIActionPool;
class UIChooser;
class UIChooserHandlerMouse;
class UIChooserHandlerKeyboard;
class UIChooserItem;
class UIChooserItemMachine;
class UIChooserNode;
class UIChooserView;
class UIVirtualMachineItem;


/** Context-menu types. */
enum UIGraphicsSelectorContextMenuType
{
    UIGraphicsSelectorContextMenuType_Global,
    UIGraphicsSelectorContextMenuType_Group,
    UIGraphicsSelectorContextMenuType_Machine
};


/** UIChooserAbstractModel extension used as VM Chooser-pane model.
  * This class is used to operate on tree of visible tree items
  * representing VMs and their groups. */
class UIChooserModel : public UIChooserAbstractModel
{
    Q_OBJECT;

signals:

    /** @name General stuff.
      * @{ */
        /** Notify listeners about tool menu popup request for certain @a enmClass and @a position. */
        void sigToolMenuRequested(UIToolClass enmClass, const QPoint &position);
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Notifies about selection changed. */
        void sigSelectionChanged();
        /** Notifies about selection invalidated. */
        void sigSelectionInvalidated();

        /** Notifies about group toggling started. */
        void sigToggleStarted();
        /** Notifies about group toggling finished. */
        void sigToggleFinished();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Notifies about root item minimum width @a iHint changed. */
        void sigRootItemMinimumWidthHintChanged(int iHint);
    /** @} */

    /** @name Action stuff.
      * @{ */
        /** Notify listeners about start or show request. */
        void sigStartOrShowRequest();
    /** @} */

public:

    /** Constructs Chooser-model passing @a pParent to the base-class. */
    UIChooserModel(UIChooser *pParent);
    /** Destructs Chooser-model. */
    virtual ~UIChooserModel() /* override */;

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        virtual void init() /* override */;
        /** Deinits model. */
        virtual void deinit() /* override */;

        /** Returns the Chooser reference. */
        UIChooser *chooser() const;
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;
        /** Returns the scene reference. */
        QGraphicsScene *scene() const;
        /** Returns the reference of the first view of the scene(). */
        UIChooserView *view() const;
        /** Returns the paint device reference. */
        QPaintDevice *paintDevice() const;

        /** Returns item at @a position, taking into account possible @a deviceTransform. */
        QGraphicsItem *itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

        /** Handles tool button click for certain @a pItem. */
        void handleToolButtonClick(UIChooserItem *pItem);
        /** Handles pin button click for certain @a pItem. */
        void handlePinButtonClick(UIChooserItem *pItem);
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Sets a list of selected @a items. */
        void setSelectedItems(const QList<UIChooserItem*> &items);
        /** Defines selected @a pItem. */
        void setSelectedItem(UIChooserItem *pItem);
        /** Defines selected-item by @a definition. */
        void setSelectedItem(const QString &strDefinition);
        /** Clear selected-items list. */
        void clearSelectedItems();

        /** Returns a list of selected-items. */
        const QList<UIChooserItem*> &selectedItems() const;

        /** Adds @a pItem to list of selected. */
        void addToSelectedItems(UIChooserItem *pItem);
        /** Removes @a pItem from list of selected. */
        void removeFromSelectedItems(UIChooserItem *pItem);

        /** Returns first selected-item. */
        UIChooserItem *firstSelectedItem() const;
        /** Returns first selected machine item. */
        UIVirtualMachineItem *firstSelectedMachineItem() const;
        /** Returns a list of selected machine items. */
        QList<UIVirtualMachineItem*> selectedMachineItems() const;

        /** Makes sure at least one item selected. */
        void makeSureAtLeastOneItemSelected();

        /** Returns whether group item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether global item is selected. */
        bool isGlobalItemSelected() const;
        /** Returns whether machine item is selected. */
        bool isMachineItemSelected() const;

        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether single local group is selected. */
        bool isSingleLocalGroupSelected() const;
        /** Returns whether single cloud profile group is selected. */
        bool isSingleCloudProfileGroupSelected() const;
        /** Returns whether all machine items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;

        /** Returns full name of currently selected group. */
        QString fullGroupName() const;

        /** Finds closest non-selected-item. */
        UIChooserItem *findClosestUnselectedItem() const;

        /** Defines current @a pItem. */
        void setCurrentItem(UIChooserItem *pItem);
        /** Returns current-item. */
        UIChooserItem *currentItem() const;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns a list of navigation-items. */
        const QList<UIChooserItem*> &navigationItems() const;
        /** Removes @a pItem from navigation list. */
        void removeFromNavigationItems(UIChooserItem *pItem);
        /** Updates navigation list. */
        void updateNavigationItemList();
    /** @} */

    /** @name Search stuff.
      * @{ */
        /** Performs a search starting from the m_pInvisibleRootNode. */
        virtual void performSearch(const QString &strSearchTerm, int iItemSearchFlags) /* override */;
        /** Cleans the search result data members and disables item's visual effects.
          * Also returns a list of all nodes which may be utilized by the calling code. */
        virtual QList<UIChooserNode*> resetSearch() /* override */;

        /** Selects next/prev (wrt. @a fIsNext) search result. */
        void selectSearchResult(bool fIsNext);
        /** Shows/hides machine search widget. */
        void setSearchWidgetVisible(bool fVisible);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns the root instance. */
        UIChooserItem *root() const;

        /** Starts editing selected group item name. */
        void startEditingSelectedGroupItemName();
        /** Disbands selected group item. */
        void disbandSelectedGroupItem();
        /** Moves selected machine items to new group item. */
        void moveSelectedMachineItemsToNewGroupItem();
        /** Starts or shows selected items. */
        void startOrShowSelectedItems();
        /** Sorts selected [parent] group item. */
        void sortSelectedGroupItem();

        /** Defines current @a pDragObject. */
        void setCurrentDragObject(QDrag *pDragObject);

        /** Looks for item with certain @a strLookupText. */
        void lookFor(const QString &strLookupText);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        void updateLayout();

        /** Defines global item height @a iHint. */
        void setGlobalItemHeightHint(int iHint);
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles Chooser-view resize. */
        void sltHandleViewResized();
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Preprocesses Qt @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    /** @} */

protected slots:

    /** @name Main event handling stuff.
      * @{ */
        /** Handles local machine registering/unregistering for machine with certain @a uId. */
        virtual void sltLocalMachineRegistered(const QUuid &uId, const bool fRegistered) /* override */;
        /** Handles cloud machine registering/unregistering for machine with certain @a uId.
          * @param  strProviderName  Brings provider short name.
          * @param  strProfileName   Brings profile name. */
        virtual void sltCloudMachineRegistered(const QString &strProviderName, const QString &strProfileName,
                                               const QUuid &uId, const bool fRegistered);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles reload machine with certain @a uId request. */
        virtual void sltReloadMachine(const QUuid &uId) /* override */;
    /** @} */

    /** @name Cloud stuff.
      * @{ */
        /** Handles list cloud machines task complete signal. */
        virtual void sltHandleCloudListMachinesTaskComplete(UITask *pTask) /* override */;
    /** @} */

private slots:

    /** @name Selection stuff.
      * @{ */
        /** Makes sure current item is visible. */
        void sltMakeSureCurrentItemVisible();

        /** Handles current-item destruction. */
        void sltCurrentItemDestroyed();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles refresh request. */
        void sltPerformRefreshAction();
        /** Handles remove selected machine request. */
        void sltRemoveSelectedMachine();

        /** Handles D&D scrolling. */
        void sltStartScrolling();
        /** Handles D&D object destruction. */
        void sltCurrentDragObjectDestroyed();

        /** Handles machine search widget show/hide request. */
        void sltShowHideSearchWidget();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares scene. */
        void prepareScene();
        /** Prepares context-menu. */
        void prepareContextMenu();
        /** Prepares handlers. */
        void prepareHandlers();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads last selected-items. */
        void loadLastSelectedItem();

        /** Saves last selected-items. */
        void saveLastSelectedItem();
        /** Cleanups handlers. */
        void cleanupHandlers();
        /** Cleanups context-menu. */
        void cleanupContextMenu();
        /** Cleanups scene. */
        void cleanupScene();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Handles context-menu @a pEvent. */
        bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);
        /** Popups context-menu of certain @a enmType in specified @a point. */
        void popupContextMenu(UIGraphicsSelectorContextMenuType enmType, QPoint point);
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Clears real focus. */
        void clearRealFocus();
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Creates navigation list for passed root @a pItem. */
        QList<UIChooserItem*> createNavigationItemList(UIChooserItem *pItem);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Build tree for main root. */
        void buildTreeForMainRoot();
        /** Update tree for main root. */
        void updateTreeForMainRoot();

        /** Removes @a machineItems. */
        void removeItems(const QList<UIChooserItemMachine*> &machineItems);
        /** Unregisters a list of local virtual @a machines. */
        void unregisterLocalMachines(const QList<CMachine> &machines);
        /** Unregisters a list of cloud virtual @a machines. */
        void unregisterCloudMachines(const QList<CCloudMachine> &machines);

        /** Processes drag move @a pEvent. */
        bool processDragMoveEvent(QGraphicsSceneDragDropEvent *pEvent);
        /** Processes drag leave @a pEvent. */
        bool processDragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent);
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the Chooser reference. */
        UIChooser *m_pChooser;

        /** Holds the scene reference. */
        QGraphicsScene *m_pScene;

        /** Holds the mouse handler instance. */
        UIChooserHandlerMouse    *m_pMouseHandler;
        /** Holds the keyboard handler instance. */
        UIChooserHandlerKeyboard *m_pKeyboardHandler;

        /** Holds the global item context menu instance. */
        QMenu *m_pContextMenuGlobal;
        /** Holds the group item context menu instance. */
        QMenu *m_pContextMenuGroup;
        /** Holds the machine item context menu instance. */
        QMenu *m_pContextMenuMachine;
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Holds the current-item reference. */
        QPointer<UIChooserItem>  m_pCurrentItem;
    /** @} */

    /** @name Search stuff.
      * @{ */
        /** Stores the index (within the m_searchResults) of the currently selected found item. */
        int  m_iCurrentSearchResultIndex;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the root instance. */
        QPointer<UIChooserItem>  m_pRoot;

        /** Holds the navigation-items. */
        QList<UIChooserItem*>  m_navigationItems;
        /** Holds the selected-items. */
        QList<UIChooserItem*>  m_selectedItems;

        /** Holds the current drag object instance. */
        QPointer<QDrag>  m_pCurrentDragObject;
        /** Holds the drag scrolling token size. */
        int              m_iScrollingTokenSize;
        /** Holds whether drag scrolling is in progress. */
        bool             m_fIsScrollingInProgress;
    /** @} */
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserModel_h */
