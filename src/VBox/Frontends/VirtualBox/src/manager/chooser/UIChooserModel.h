/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserModel class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIChooserModel_h__
#define __UIChooserModel_h__

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QTransform>
#include <QMap>
#include <QThread>

/* GUI includes: */
#include "UIChooserItem.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsScene;
class QGraphicsItem;
class QDrag;
class QMenu;
class QAction;
class QGraphicsSceneContextMenuEvent;
class QTimer;
class QPaintDevice;
class UIVirtualMachineItem;
class UIChooser;
class UIActionPool;
class UIChooserHandlerMouse;
class UIChooserHandlerKeyboard;
class CMachine;

/* Context-menu type: */
enum UIGraphicsSelectorContextMenuType
{
    UIGraphicsSelectorContextMenuType_Group,
    UIGraphicsSelectorContextMenuType_Machine
};

/* Graphics chooser-model: */
class UIChooserModel : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Current-item stuff: */
    void sigSelectionChanged();

    /* Notifier: Focus-item stuff: */
    void sigFocusChanged(UIChooserItem *pFocusItem);

    /* Notifiers: Root-item stuff: */
    void sigRootItemMinimumWidthHintChanged(int iRootItemMinimumWidthHint);
    void sigRootItemMinimumHeightHintChanged(int iRootItemMinimumHeightHint);
    void sigSlidingStarted();

    /* Notifiers: Group-item stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

    /* Notifiers: Group-saving stuff: */
    void sigStartGroupSaving();
    void sigGroupSavingStateChanged();

public:

    /* Constructor/destructor: */
    UIChooserModel(UIChooser *pParent);
    ~UIChooserModel();

    /* API: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /** Returns the chooser reference. */
    UIChooser* chooser() const { return m_pChooser; }
    /** Returns the action-pool reference. */
    UIActionPool* actionPool() const;

    /* API: Scene stuff: */
    QGraphicsScene* scene() const;
    QPaintDevice* paintDevice() const;
    QGraphicsItem* itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

    /* API: Layout stuff: */
    void updateLayout();
    /** Defines global item height @a iHint. */
    void setGlobalItemHeightHint(int iHint);

    /* API: Navigation stuff: */
    const QList<UIChooserItem*>& navigationList() const;
    void removeFromNavigationList(UIChooserItem *pItem);
    void updateNavigation();

    /* API: Current-item stuff: */
    bool isGroupItemSelected() const;
    bool isGlobalItemSelected() const;
    bool isMachineItemSelected() const;
    UIVirtualMachineItem *currentMachineItem() const;
    QList<UIVirtualMachineItem*> currentMachineItems() const;
    UIChooserItem* currentItem() const;
    const QList<UIChooserItem*>& currentItems() const;
    void setCurrentItems(const QList<UIChooserItem*> &items);
    void setCurrentItem(UIChooserItem *pItem);
    void setCurrentItem(const QString &strDefinition);
    void unsetCurrentItem();
    void addToCurrentItems(UIChooserItem *pItem);
    void removeFromCurrentItems(UIChooserItem *pItem);
    UIChooserItem *findClosestUnselectedItem() const;
    void makeSureSomeItemIsSelected();
    void notifyCurrentItemChanged();
    bool isSingleGroupSelected() const;
    bool isAllItemsOfOneGroupSelected() const;

    /* API: Focus-item stuff: */
    UIChooserItem* focusItem() const;
    void setFocusItem(UIChooserItem *pItem);

    /* API: Root-item stuff: */
    UIChooserItem* mainRoot() const;
    UIChooserItem* root() const;
    void indentRoot(UIChooserItem *pNewRootItem);
    void unindentRoot();
    bool isSlidingInProgress() const;

    /* API: Group-item stuff: */
    void startEditingGroupItemName();
    void cleanupGroupTree();
    static QString uniqueGroupName(UIChooserItem *pRoot);

    /* API: Machine-item stuff: */
    void activateMachineItem();

    /* API: Drag&drop stuff: */
    void setCurrentDragObject(QDrag *pDragObject);

    /* API: Item lookup stuff: */
    void lookFor(const QString &strLookupSymbol);
    bool isLookupInProgress() const;

    /* API: Saving stuff: */
    void saveGroupSettings();
    bool isGroupSavingInProgress() const;

public slots:

    /* Handler: Chooser-view stuff: */
    void sltHandleViewResized();

private slots:

    /* Handlers: Global events: */
    void sltMachineStateChanged(QString strId, KMachineState state);
    void sltMachineDataChanged(QString strId);
    void sltMachineRegistered(QString strId, bool fRegistered);
    void sltSessionStateChanged(QString strId, KSessionState state);
    void sltSnapshotChanged(QString strId, QString strSnapshotId);

    /* Handler: Focus-item stuff: */
    void sltFocusItemDestroyed();

    /* Handlers: Root-item stuff: */
    void sltLeftRootSlidingProgress();
    void sltRightRootSlidingProgress();
    void sltSlidingComplete();

    /* Handlers: Group-item stuff: */
    void sltEditGroupName();
    void sltSortGroup();
    void sltUngroupSelectedGroup();

    /* Handlers: Machine-item stuff: */
    void sltCreateNewMachine();
    void sltGroupSelectedMachines();
    void sltReloadMachine(const QString &strId);
    void sltSortParentGroup();
    void sltPerformRefreshAction();
    void sltRemoveSelectedMachine();

    /* Handlers: Drag&drop stuff: */
    void sltStartScrolling();
    void sltCurrentDragObjectDestroyed();

    /* Handler: Item lookup stuff: */
    void sltEraseLookupTimer();

    /* Handlers: Saving stuff: */
    void sltGroupSavingStart();
    void sltGroupDefinitionsSaveComplete();
    void sltGroupOrdersSaveComplete();

private:

    /* Data enumerator: */
    enum ChooserModelData
    {
        /* Layout margin: */
        ChooserModelData_Margin
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Prepare stuff: */
    void prepareScene();
    void prepareRoot();
    void prepareLookup();
    void prepareContextMenu();
    void prepareHandlers();
    void prepareConnections();
    void loadLastSelectedItem();

    /* Helpers: Cleanup stuff: */
    void saveLastSelectedItem();
    void cleanupHandlers();
    void cleanupContextMenu();
    void cleanupLookup();
    void cleanupRoot();
    void cleanupScene();

    /* Handler: Event-filter: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helper: Navigation stuff: */
    QList<UIChooserItem*> createNavigationList(UIChooserItem *pItem);

    /* Helper: Focus-item stuff: */
    void clearRealFocus();

    /* Helper: Root-item stuff: */
    void slideRoot(bool fForward);

    /* Helper: Group-item stuff: */
    void cleanupGroupTree(UIChooserItem *pGroupItem);

    /* Helpers: Machine-item stuff: */
    void removeItems(const QList<UIChooserItem*> &itemsToRemove);
    void unregisterMachines(const QStringList &ids);

    /* Helpers: Context-menu stuff: */
    bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);
    void popupContextMenu(UIGraphicsSelectorContextMenuType type, QPoint point);

    /* Handler: Drag&drop event: */
    bool processDragMoveEvent(QGraphicsSceneDragDropEvent *pEvent);
    bool processDragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent);

    /* Helpers: Loading stuff: */
    void loadGroupTree();
    void addMachineIntoTheTree(const CMachine &machine, bool fMakeItVisible = false);
    UIChooserItem* getGroupItem(const QString &strName, UIChooserItem *pParentItem, bool fAllGroupsOpened);
    bool shouldBeGroupOpened(UIChooserItem *pParentItem, const QString &strName);
    int getDesiredPosition(UIChooserItem *pParentItem, UIChooserItemType type, const QString &strName);
    int positionFromDefinitions(UIChooserItem *pParentItem, UIChooserItemType type, const QString &strName);
    void createMachineItem(const CMachine &machine, UIChooserItem *pParentItem);
    void createGlobalItem(UIChooserItem *pParentItem);

    /* Helpers: Saving stuff: */
    void saveGroupDefinitions();
    void saveGroupOrders();
    void gatherGroupDefinitions(QMap<QString, QStringList> &groups, UIChooserItem *pParentGroup);
    void gatherGroupOrders(QMap<QString, QStringList> &groups, UIChooserItem *pParentItem);
    void makeSureGroupDefinitionsSaveIsFinished();
    void makeSureGroupOrdersSaveIsFinished();

    /** Holds the chooser reference. */
    UIChooser *m_pChooser;

    /* Variables: */
    QGraphicsScene *m_pScene;

    QList<UIChooserItem*> m_rootStack;
    bool m_fSliding;
    UIChooserItem *m_pLeftRoot;
    UIChooserItem *m_pRightRoot;
    QPointer<UIChooserItem> m_pAfterSlidingFocus;

    QMap<QString, QStringList> m_groups;
    QList<UIChooserItem*> m_navigationList;
    QList<UIChooserItem*> m_currentItems;
    UIChooserHandlerMouse *m_pMouseHandler;
    UIChooserHandlerKeyboard *m_pKeyboardHandler;
    QPointer<QDrag> m_pCurrentDragObject;
    int m_iScrollingTokenSize;
    bool m_fIsScrollingInProgress;
    QPointer<UIChooserItem> m_pFocusItem;
    QMenu *m_pContextMenuGroup;
    QMenu *m_pContextMenuMachine;

    /* Variables: Lookup stuff: */
    QTimer *m_pLookupTimer;
    QString m_strLookupString;

    /** Holds the Id of last VM created from the GUI side. */
    QString m_strLastCreatedMachineId;
};

/* Allows to save group definitions asynchronously: */
class UIThreadGroupDefinitionSave : public QThread
{
    Q_OBJECT;

signals:

    /* Notifier: Reload stuff: */
    void sigReload(QString strId);

    /* Notifier: Complete stuff: */
    void sigComplete();

public:

    /* Singleton stuff: */
    static UIThreadGroupDefinitionSave* instance();
    static void prepare();
    static void cleanup();

    /* API: Configuring stuff: */
    void configure(QObject *pParent,
                   const QMap<QString, QStringList> &oldLists,
                   const QMap<QString, QStringList> &newLists);

private:

    /* Constructor/destructor: */
    UIThreadGroupDefinitionSave();
    ~UIThreadGroupDefinitionSave();

    /* Worker thread stuff: */
    void run();

    /* Variables: */
    static UIThreadGroupDefinitionSave *m_spInstance;
    QMap<QString, QStringList> m_oldLists;
    QMap<QString, QStringList> m_newLists;
};

/* Allows to save group order asynchronously: */
class UIThreadGroupOrderSave : public QThread
{
    Q_OBJECT;

signals:

    /* Notifier: Complete stuff: */
    void sigComplete();

public:

    /* Singleton stuff: */
    static UIThreadGroupOrderSave* instance();
    static void prepare();
    static void cleanup();

    /* API: Configuring stuff: */
    void configure(QObject *pParent, const QMap<QString, QStringList> &groups);

private:

    /* Constructor/destructor: */
    UIThreadGroupOrderSave();
    ~UIThreadGroupOrderSave();

    /* Worker thread stuff: */
    void run();

    /* Variables: */
    static UIThreadGroupOrderSave *m_spInstance;
    QMap<QString, QStringList> m_groups;
};

#endif /* __UIChooserModel_h__ */

