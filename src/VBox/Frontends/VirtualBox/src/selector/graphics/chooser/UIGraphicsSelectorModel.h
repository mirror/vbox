/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorModel class declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGraphicsSelectorModel_h__
#define __UIGraphicsSelectorModel_h__

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QTransform>
#include <QMap>
#include <QSet>

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsScene;
class QGraphicsItem;
class QDrag;
class QMenu;
class QAction;
class QGraphicsSceneContextMenuEvent;
class CMachine;
class UIGSelectorItem;
class UIVMItem;
class UIGraphicsSelectorMouseHandler;
class UIGraphicsSelectorKeyboardHandler;

/* Type defs: */
typedef QSet<QString> UIStringSet;

/* Context-menu type: */
enum UIGraphicsSelectorContextMenuType
{
    UIGraphicsSelectorContextMenuType_Root,
    UIGraphicsSelectorContextMenuType_Group,
    UIGraphicsSelectorContextMenuType_Machine
};

/* Graphics selector model: */
class UIGraphicsSelectorModel : public QObject
{
    Q_OBJECT;

signals:

    /* Notify listeners about root-item height changed: */
    void sigRootItemResized(const QSizeF &size, int iMinimumWidth);

    /* Notify listeners about selection change: */
    void sigSelectionChanged();

    /* Notify listeners to show corresponding status-bar message: */
    void sigClearStatusMessage();
    void sigShowStatusMessage(const QString &strStatusMessage);

public:

    /* Constructor/destructor: */
    UIGraphicsSelectorModel(QObject *pParent);
    ~UIGraphicsSelectorModel();

    /* API: Load/save stuff: */
    void load();
    void save();

    /* API: Scene getter: */
    QGraphicsScene* scene() const;

    /* API: Current item stuff: */
    void setCurrentItem(int iItemIndex);
    void setCurrentItem(UIGSelectorItem *pItem);
    void unsetCurrentItem();
    UIVMItem* currentItem() const;
    QList<UIVMItem*> currentItems() const;

    /* API: Focus item stuff: */
    void setFocusItem(UIGSelectorItem *pItem, bool fWithSelection = false);
    UIGSelectorItem* focusItem() const;

    /* API: Positioning item stuff: */
    QGraphicsItem* itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

    /* API: Update stuff: */
    void updateGroupTree();

    /* API: Navigation stuff: */
    const QList<UIGSelectorItem*>& navigationList() const;
    void removeFromNavigationList(UIGSelectorItem *pItem);
    void clearNavigationList();
    void updateNavigation();

    /* API: Selection stuff: */
    const QList<UIGSelectorItem*>& selectionList() const;
    void addToSelectionList(UIGSelectorItem *pItem);
    void removeFromSelectionList(UIGSelectorItem *pItem);
    void clearSelectionList();
    void notifySelectionChanged();

    /* API: Layout stuff: */
    void updateLayout();

    /* API: Drag and drop stuff: */
    void setCurrentDragObject(QDrag *pDragObject);

    /* API: Activate stuff: */
    void activate();

private slots:

    /* Handlers: Global events: */
    void sltMachineStateChanged(QString strId, KMachineState state);
    void sltMachineDataChanged(QString strId);
    void sltMachineRegistered(QString strId, bool fRegistered);
    void sltSessionStateChanged(QString strId, KSessionState state);
    void sltSnapshotChanged(QString strId, QString strSnapshotId);

    /* Handler: View-resize: */
    void sltHandleViewResized();

    /* Handler: Drag object destruction: */
    void sltCurrentDragObjectDestroyed();

    /* Handler: Remove currently selected group: */
    void sltRemoveCurrentlySelectedGroup();

    /* Handler: Context menu hovering: */
    void sltActionHovered(QAction *pAction);

    /* Handler: Focus item destruction: */
    void sltFocusItemDestroyed();

private:

    /* Data enumerator: */
    enum SelectorModelData
    {
        /* Layout hints: */
        SelectorModelData_Margin
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Prepare stuff: */
    void prepareScene();
    void prepareRoot();
    void prepareContextMenu();
    void prepareHandlers();
    void prepareGroupTree();

    /* Helpers: Cleanup stuff: */
    void cleanupGroupTree();
    void cleanupHandlers();
    void cleanupContextMenu();
    void cleanupRoot();
    void cleanupScene();

    /* Event handlers: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helpers: Focus item stuff: */
    void clearRealFocus();

    /* Helpers: Current item stuff: */
    UIVMItem* searchCurrentItem(const QList<UIGSelectorItem*> &list) const;
    void enumerateCurrentItems(const QList<UIGSelectorItem*> &il, QList<UIVMItem*> &ol) const;
    bool contains(const QList<UIVMItem*> &list, UIVMItem *pItem) const;

    /* Helpers: Loading: */
    void loadGroupTree();
    void loadGroupsOrder();
    void loadGroupsOrder(UIGSelectorItem *pParentItem);
    void addMachineIntoTheTree(const CMachine &machine);
    UIGSelectorItem* getGroupItem(const QString &strName, UIGSelectorItem *pParent);
    void createMachineItem(const CMachine &machine, UIGSelectorItem *pWhere);

    /* Helpers: Saving: */
    void saveGroupTree();
    void saveGroupsOrder();
    void saveGroupsOrder(UIGSelectorItem *pParentItem);
    void gatherGroupTree(QMap<QString, QStringList> &groups, UIGSelectorItem *pParentGroup);
    QString fullName(UIGSelectorItem *pItem);

    /* Helpers: Update stuff: */
    void updateGroupTree(UIGSelectorItem *pGroupItem);

    /* Helpers: Navigation stuff: */
    QList<UIGSelectorItem*> createNavigationList(UIGSelectorItem *pItem);

    /* Helpers: Global event stuff: */
    void updateMachineItems(const QString &strId, UIGSelectorItem *pParent);
    void removeMachineItems(const QString &strId, UIGSelectorItem *pParent);
    UIGSelectorItem* getMachineItem(const QString &strId, UIGSelectorItem *pParent);

    /* Helpers: Context menu stuff: */
    bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);
    void popupContextMenu(UIGraphicsSelectorContextMenuType type, QPoint point);

    /* Variables: */
    QGraphicsScene *m_pScene;
    UIGSelectorItem *m_pRoot;
    QMap<QString, UIStringSet> m_groups;
    QList<UIGSelectorItem*> m_navigationList;
    QList<UIGSelectorItem*> m_selectionList;
    UIGraphicsSelectorMouseHandler *m_pMouseHandler;
    UIGraphicsSelectorKeyboardHandler *m_pKeyboardHandler;
    QPointer<QDrag> m_pCurrentDragObject;
    QPointer<UIGSelectorItem> m_pFocusItem;
    QMenu *m_pContextMenuRoot;
    QMenu *m_pContextMenuGroup;
    QMenu *m_pContextMenuMachine;
};

#endif /* __UIGraphicsSelectorModel_h__ */

