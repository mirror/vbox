/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserModel class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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

/* Forward declaration: */
class QDrag;
class UIActionPool;
class UIChooser;
class UIChooserHandlerMouse;
class UIChooserHandlerKeyboard;
class UIChooserItem;
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

public:

    /** Constructs Chooser-model passing @a pParent to the base-class. */
    UIChooserModel(UIChooser *pParent);
    /** Destructs Chooser-model. */
    virtual ~UIChooserModel() /* override */;

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        void init();
        /** Deinits model. */
        void deinit();

        /** Returns the Chooser reference. */
        UIChooser *chooser() const;
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;
        /** Returns the scene reference. */
        QGraphicsScene *scene() const;
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
        /** Sets a list of current @a items. */
        void setCurrentItems(const QList<UIChooserItem*> &items);
        /** Defines current @a pItem. */
        void setCurrentItem(UIChooserItem *pItem);
        /** Defines current item by @a definition. */
        void setCurrentItem(const QString &strDefinition);
        /** Unsets all current items. */
        void unsetCurrentItems();

        /** Adds @a pItem to list of current. */
        void addToCurrentItems(UIChooserItem *pItem);
        /** Removes @a pItem from list of current. */
        void removeFromCurrentItems(UIChooserItem *pItem);

        /** Returns current item. */
        UIChooserItem *currentItem() const;
        /** Returns a list of current items. */
        const QList<UIChooserItem*> &currentItems() const;

        /** Returns current machine item. */
        UIVirtualMachineItem *currentMachineItem() const;
        /** Returns a list of current machine items. */
        QList<UIVirtualMachineItem*> currentMachineItems() const;

        /** Returns whether group item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether global item is selected. */
        bool isGlobalItemSelected() const;
        /** Returns whether machine item is selected. */
        bool isMachineItemSelected() const;

        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether all machine items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;

        /** Finds closest non-selected item. */
        UIChooserItem *findClosestUnselectedItem() const;

        /** Makes sure some item is selected. */
        void makeSureSomeItemIsSelected();

        /** Defines focus @a pItem. */
        void setFocusItem(UIChooserItem *pItem);
        /** Returns focus item. */
        UIChooserItem *focusItem() const;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns navigation item list. */
        const QList<UIChooserItem*> &navigationList() const;
        /** Removes @a pItem from navigation list. */
        void removeFromNavigationList(UIChooserItem *pItem);
        /** Updates navigation list. */
        void updateNavigation();
    /** @} */

    /** @name Virtual Machine/Group search stuff.
      * @{ */
        /** Performs a search starting from the m_pInvisibleRootNode. */
        void performSearch(const QString &strSearchTerm, int iItemSearchFlags);
        /** Clean the search result data members and disables item's visual effects. Also returns a list of
          * all nodes which may be utilized by the calling code. */
        QList<UIChooserNode*> resetSearch();
        /** Scrolls to next/prev (wrt. @a fIsNext) search result. */
        void scrollToSearchResult(bool fIsNext);
        /** Shows/hides machine search widget. */
        void setSearchWidgetVisible(bool fVisible);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns the root instance. */
        UIChooserItem *root() const;

        /** Starts editing group name. */
        void startEditingGroupItemName();

        /** Activates machine item. */
        void activateMachineItem();

        /** Defines current @a pDragObject. */
        void setCurrentDragObject(QDrag *pDragObject);

        /** Looks for item with certain @a strLookupSymbol. */
        void lookFor(const QString &strLookupSymbol);
        /** Returns whether looking is in progress. */
        bool isLookupInProgress() const;
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
        /** Handles machine registering/unregistering for machine with certain @a uId. */
        virtual void sltMachineRegistered(const QUuid &uId, const bool fRegistered) /* override */;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles reload machine with certain @a uId request. */
        virtual void sltReloadMachine(const QUuid &uId) /* override */;
    /** @} */

private slots:

    /** @name Selection stuff.
      * @{ */
        /** Handles focus item destruction. */
        void sltFocusItemDestroyed();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles group rename request. */
        void sltEditGroupName();
        /** Handles group sort request. */
        void sltSortGroup();
        /** Handles machine search widget show/hide request. */
        void sltShowHideSearchWidget();
        /** Handles group destroy request. */
        void sltUngroupSelectedGroup();

        /** Handles create new machine request. */
        void sltCreateNewMachine();
        /** Handles group selected machines request. */
        void sltGroupSelectedMachines();
        /** Handles sort parent group request. */
        void sltSortParentGroup();
        /** Handles refresh request. */
        void sltPerformRefreshAction();
        /** Handles remove selected machine request. */
        void sltRemoveSelectedMachine();

        /** Handles D&D scrolling. */
        void sltStartScrolling();
        /** Handles D&D object destruction. */
        void sltCurrentDragObjectDestroyed();

        /** Handles request to erase lookup timer. */
        void sltEraseLookupTimer();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares scene. */
        void prepareScene();
        /** Prepares lookup. */
        void prepareLookup();
        /** Prepares context-menu. */
        void prepareContextMenu();
        /** Prepares handlers. */
        void prepareHandlers();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads last selected items. */
        void loadLastSelectedItem();

        /** Saves last selected items. */
        void saveLastSelectedItem();
        /** Cleanups connections. */
        void cleanupHandlers();
        /** Cleanups context-menu. */
        void cleanupContextMenu();
        /** Cleanups lookup. */
        void cleanupLookup();
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
        /** Returns the reference of the first view of the scene(). */
        UIChooserView *view();
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Clears real focus. */
        void clearRealFocus();
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Creates navigation list for passed root @a pItem. */
        QList<UIChooserItem*> createNavigationList(UIChooserItem *pItem);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Build tree for main root. */
        void buildTreeForMainRoot();

        /** Removes machine @a items. */
        void removeItems(const QList<UIChooserItem*> &items);
        /** Unregisters virtual machines using list of @a ids. */
        void unregisterMachines(const QList<QUuid> &ids);

        /** Processes drag move @a pEvent. */
        bool processDragMoveEvent(QGraphicsSceneDragDropEvent *pEvent);
        /** Processes drag leave @a pEvent. */
        bool processDragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent);

        /** Performs sorting for @a pNode. */
        void sortNodes(UIChooserNode *pNode);
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
        /** Holds the focus item reference. */
        QPointer<UIChooserItem>  m_pFocusItem;
    /** @} */

    /** @name Virtual Machine/Group search stuff.
      * @{ */
        /** Stores the results of the current search. */
        QList<UIChooserNode*> m_searchResults;
        /** Stores the index (within the m_searchResults) of the currently scrolled item. */
        int m_iCurrentScrolledIndex;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the root instance. */
        QPointer<UIChooserItem>  m_pRoot;

        /** Holds the navigation list. */
        QList<UIChooserItem*>  m_navigationList;
        QList<UIChooserItem*>  m_currentItems;

        /** Holds the current drag object instance. */
        QPointer<QDrag>  m_pCurrentDragObject;
        /** Holds the drag scrolling token size. */
        int              m_iScrollingTokenSize;
        /** Holds whether drag scrolling is in progress. */
        bool             m_fIsScrollingInProgress;

        /** Holds the item lookup timer instance. */
        QTimer  *m_pLookupTimer;
        /** Holds the item lookup string. */
        QString  m_strLookupString;
    /** @} */
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserModel_h */
