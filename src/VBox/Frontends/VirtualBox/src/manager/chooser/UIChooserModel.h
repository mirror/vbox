/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserModel class declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIChooserModel_h___
#define ___UIChooserModel_h___

/* Qt includes: */
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTransform>

/* GUI includes: */
#include "UIChooserItem.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QAction;
class QDrag;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsSceneContextMenuEvent;
class QMenu;
class QPaintDevice;
class QTimer;
class UIActionPool;
class UIChooser;
class UIChooserHandlerMouse;
class UIChooserHandlerKeyboard;
class UIVirtualMachineItem;
class CMachine;


/** Context-menu types. */
enum UIGraphicsSelectorContextMenuType
{
    UIGraphicsSelectorContextMenuType_Group,
    UIGraphicsSelectorContextMenuType_Machine
};


/** QObject extension used as VM chooser pane model: */
class UIChooserModel : public QObject
{
    Q_OBJECT;

signals:

    /** @name Selection stuff.
      * @{ */
        /** Notifies about selection changed. */
        void sigSelectionChanged();
        /** Notifies about focus changed. */
        void sigFocusChanged();

        /** Notifies about root sliding started. */
        void sigSlidingStarted();

        /** Notifies about group toggling started. */
        void sigToggleStarted();
        /** Notifies about group toggling finished. */
        void sigToggleFinished();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Notifies about root item minimum width @a iHint changed. */
        void sigRootItemMinimumWidthHintChanged(int iHint);
        /** Notifies about root item minimum height @a iHint changed. */
        void sigRootItemMinimumHeightHintChanged(int iHint);
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Notifies about group saving started. */
        void sigGroupSavingStarted();
        /** Notifies about group saving state changed. */
        void sigGroupSavingStateChanged();
    /** @} */

public:

    /** Constructs chooser model passing @a pParent to the base-class. */
    UIChooserModel(UIChooser *pParent);
    /** Destructs chooser model. */
    virtual ~UIChooserModel() /* override */;

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        void init();
        /** Deinits model. */
        void deinit();

        /** Returns the chooser reference. */
        UIChooser *chooser() const;
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;
        /** Returns the scene reference. */
        QGraphicsScene *scene() const;
        /** Returns the paint device reference. */
        QPaintDevice *paintDevice() const;

        /** Returns item at @a position, taking into account possible @a deviceTransform. */
        QGraphicsItem *itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;
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

    /** @name Children stuff.
      * @{ */
        /** Holds the main root instance. */
        UIChooserItem *mainRoot() const;
        /** Holds the current root reference. */
        UIChooserItem *root() const;

        /** Indents stack of root items with @a pNewRootItem. */
        void indentRoot(UIChooserItem *pNewRootItem);
        /** Unindents stack of root items flushing top-most. */
        void unindentRoot();
        /** Returns whether root indenting/unindenting is in progress. */
        bool isSlidingInProgress() const;

        /** Starts editing group name. */
        void startEditingGroupItemName();

        /** Cleanups group tree. */
        void cleanupGroupTree();

        /** Activates machine item. */
        void activateMachineItem();

        /** Defines current @a pDragObject. */
        void setCurrentDragObject(QDrag *pDragObject);

        /** Looks for item with certain @a strLookupSymbol. */
        void lookFor(const QString &strLookupSymbol);
        /** Returns whether looking is in progress. */
        bool isLookupInProgress() const;

        /** Generates unique group name traversing recursively starting from @a pRoot. */
        static QString uniqueGroupName(UIChooserItem *pRoot);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        void updateLayout();

        /** Defines global item height @a iHint. */
        void setGlobalItemHeightHint(int iHint);
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Commands to save group settings. */
        void saveGroupSettings();
        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles chooser view resize. */
        void sltHandleViewResized();
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Preprocesses Qt @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Main event handling stuff.
      * @{ */
        /** Handles machine @a enmState change for machine with certain @a strId. */
        void sltMachineStateChanged(QString strId, KMachineState enmState);
        /** Handles machine data change for machine with certain @a strId. */
        void sltMachineDataChanged(QString strId);
        /** Handles machine registering/unregistering for machine with certain @a strId. */
        void sltMachineRegistered(QString strId, bool fRegistered);
        /** Handles session @a enmState change for machine with certain @a strId. */
        void sltSessionStateChanged(QString strId, KSessionState enmState);
        /** Handles snapshot change for machine/snapshot with certain @a strId / @a strSnapshotId. */
        void sltSnapshotChanged(QString strId, QString strSnapshotId);
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Handles focus item destruction. */
        void sltFocusItemDestroyed();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles left root sliding progress. */
        void sltLeftRootSlidingProgress();
        /** Handles right root sliding progress. */
        void sltRightRootSlidingProgress();
        /** Handles sliding progress complete. */
        void sltSlidingComplete();

        /** Handles group rename request. */
        void sltEditGroupName();
        /** Handles group sort request. */
        void sltSortGroup();
        /** Handles group destroy request. */
        void sltUngroupSelectedGroup();

        /** Handles create new machine request. */
        void sltCreateNewMachine();
        /** Handles group selected machines request. */
        void sltGroupSelectedMachines();
        /** Handles reload machine with certain @a strId request. */
        void sltReloadMachine(const QString &strId);
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

    /** @name Group saving stuff.
      * @{ */
        /** Handles request to start group saving. */
        void sltGroupSavingStart();
        /** Handles group definition saving complete. */
        void sltGroupDefinitionsSaveComplete();
        /** Handles group order saving complete. */
        void sltGroupOrdersSaveComplete();
    /** @} */

private:

    /** Data field types. */
    enum ChooserModelData
    {
        /* Layout margin: */
        ChooserModelData_Margin
    };

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares scene. */
        void prepareScene();
        /** Prepares root. */
        void prepareRoot();
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
        /** Cleanups root. */
        void cleanupRoot();
        /** Cleanups scene. */
        void cleanupScene();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;

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
        QList<UIChooserItem*> createNavigationList(UIChooserItem *pItem);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Performs root sliding, @a fForward if specified. */
        void slideRoot(bool fForward);

        /** Loads group tree. */
        void loadGroupTree();
        /** Adds machine item based on certain @a comMachine and optionally @a fMakeItVisible. */
        void addMachineIntoTheTree(const CMachine &comMachine, bool fMakeItVisible = false);
        /** Cleanups group tree starting from the passed @a pParentItem. */
        void cleanupGroupTree(UIChooserItem *pParentItem);

        /** Acquires group item, creates one if necessary.
          * @param  strName           Brings the name of group we looking for.
          * @param  pParentItem       Brings the parent we starting to look for a group from.
          * @param  fAllGroupsOpened  Brings whether we should open all the groups till the required one. */
        UIChooserItem *getGroupItem(const QString &strName, UIChooserItem *pParentItem, bool fAllGroupsOpened);
        /** Returns whether group with certain @a strName should be opened, searching starting from the passed @a pParentItem. */
        bool shouldBeGroupOpened(UIChooserItem *pParentItem, const QString &strName);

        /** Acquires desired position for an child of @a pParentItem with specified @a enmType and @a strName. */
        int getDesiredPosition(UIChooserItem *pParentItem, UIChooserItemType enmType, const QString &strName);
        /** Acquires saved position for an child of @a pParentItem with specified @a enmType and @a strName. */
        int positionFromDefinitions(UIChooserItem *pParentItem, UIChooserItemType enmType, const QString &strName);

        /** Creates machine item based on certain @a comMachine as a child of specified @a pParentItem. */
        void createMachineItem(const CMachine &comMachine, UIChooserItem *pParentItem);
        /** Creates global item as a child of specified @a pParentItem. */
        void createGlobalItem(UIChooserItem *pParentItem);

        /** Removes machine @a items. */
        void removeItems(const QList<UIChooserItem*> &items);
        /** Unregisters virtual machines using list of @a ids. */
        void unregisterMachines(const QStringList &ids);

        /** Processes drag move @a pEvent. */
        bool processDragMoveEvent(QGraphicsSceneDragDropEvent *pEvent);
        /** Processes drag leave @a pEvent. */
        bool processDragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent);
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Saves group definitions. */
        void saveGroupDefinitions();
        /** Saves group orders. */
        void saveGroupOrders();

        /** Gathers group @a definitions of @a pParentGroup. */
        void gatherGroupDefinitions(QMap<QString, QStringList> &definitions, UIChooserItem *pParentGroup);
        /** Gathers group @a orders of @a pParentGroup. */
        void gatherGroupOrders(QMap<QString, QStringList> &orders, UIChooserItem *pParentItem);

        /** Makes sure group definitions saving is finished. */
        void makeSureGroupDefinitionsSaveIsFinished();
        /** Makes sure group orders saving is finished. */
        void makeSureGroupOrdersSaveIsFinished();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the chooser reference. */
        UIChooser *m_pChooser;

        /** Holds the scene reference. */
        QGraphicsScene *m_pScene;

        /** Holds the mouse handler instance. */
        UIChooserHandlerMouse    *m_pMouseHandler;
        /** Holds the keyboard handler instance. */
        UIChooserHandlerKeyboard *m_pKeyboardHandler;

        /** Holds the group item context menu instance. */
        QMenu *m_pContextMenuGroup;
        /** Holds the machine item context menu instance. */
        QMenu *m_pContextMenuMachine;
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Holds the focus item reference. */
        QPointer<UIChooserItem> m_pFocusItem;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the root stack. */
        QList<UIChooserItem*>    m_rootStack;
        /** Holds whether root sliding is in progress. */
        bool                     m_fSliding;
        /** Holds left temporary root instance. */
        UIChooserItem           *m_pLeftRoot;
        /** Holds right temporary root instance. */
        UIChooserItem           *m_pRightRoot;
        /** Holds the item whish should be ficused after sliding. */
        QPointer<UIChooserItem>  m_pAfterSlidingFocus;

        /** Holds the navigation list. */
        QList<UIChooserItem*> m_navigationList;
        QList<UIChooserItem*> m_currentItems;

        /** Holds the current drag object instance. */
        QPointer<QDrag>  m_pCurrentDragObject;
        /** Holds the drag scrolling token size. */
        int m_iScrollingTokenSize;
        /** Holds whether drag scrolling is in progress. */
        bool m_fIsScrollingInProgress;

        /** Holds the item lookup timer instance. */
        QTimer *m_pLookupTimer;
        /** Holds the item lookup string. */
        QString m_strLookupString;

        /** Holds the Id of last VM created from the GUI side. */
        QString m_strLastCreatedMachineId;
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Holds the consolidated map of group definitions/orders. */
        QMap<QString, QStringList> m_groups;
    /** @} */
};


/** QThread subclass allowing to save group definitions asynchronously. */
class UIThreadGroupDefinitionSave : public QThread
{
    Q_OBJECT;

signals:

    /** Notifies about machine with certain @a strId to be reloaded. */
    void sigReload(QString strId);

    /** Notifies about task is complete. */
    void sigComplete();

public:

    /** Returns group saving thread instance. */
    static UIThreadGroupDefinitionSave* instance();
    /** Prepares group saving thread instance. */
    static void prepare();
    /** Cleanups group saving thread instance. */
    static void cleanup();

    /** Configures @a groups saving thread with corresponding @a pListener.
      * @param  oldLists  Brings the old definition list to be compared.
      * @param  newLists  Brings the new definition list to be saved. */
    void configure(QObject *pParent,
                   const QMap<QString, QStringList> &oldLists,
                   const QMap<QString, QStringList> &newLists);

protected:

    /** Constructs group saving thread. */
    UIThreadGroupDefinitionSave();
    /** Destructs group saving thread. */
    virtual ~UIThreadGroupDefinitionSave() /* override */;

    /** Contains a thread task to be executed. */
    void run();

    /** Holds the singleton instance. */
    static UIThreadGroupDefinitionSave *s_pInstance;

    /** Holds the map of group definitions to be compared. */
    QMap<QString, QStringList> m_oldLists;
    /** Holds the map of group definitions to be saved. */
    QMap<QString, QStringList> m_newLists;
};


/** QThread subclass allowing to save group order asynchronously. */
class UIThreadGroupOrderSave : public QThread
{
    Q_OBJECT;

signals:

    /** Notifies about task is complete. */
    void sigComplete();

public:

    /** Returns group saving thread instance. */
    static UIThreadGroupOrderSave *instance();
    /** Prepares group saving thread instance. */
    static void prepare();
    /** Cleanups group saving thread instance. */
    static void cleanup();

    /** Configures group saving thread with corresponding @a pListener.
      * @param  groups  Brings the groups to be saved. */
    void configure(QObject *pListener,
                   const QMap<QString, QStringList> &groups);

protected:

    /** Constructs group saving thread. */
    UIThreadGroupOrderSave();
    /** Destructs group saving thread. */
    virtual ~UIThreadGroupOrderSave() /* override */;

    /** Contains a thread task to be executed. */
    virtual void run() /* override */;

    /** Holds the singleton instance. */
    static UIThreadGroupOrderSave *s_pInstance;

    /** Holds the map of groups to be saved. */
    QMap<QString, QStringList>  m_groups;
};


#endif /* !___UIChooserModel_h___ */
