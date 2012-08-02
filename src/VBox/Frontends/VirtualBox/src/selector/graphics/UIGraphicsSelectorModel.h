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

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsScene;
class QGraphicsItem;
class QDrag;
class QMenu;
class QAction;
class UIGSelectorItem;
class UIVMItem;
class UIGraphicsSelectorMouseHandler;
class UIGraphicsSelectorKeyboardHandler;

/* UIGraphicsSelector mode enumerator: */
enum UIGraphicsSelectorMode
{
    UIGraphicsSelectorMode_Default,
    UIGraphicsSelectorMode_ShowGroups
};

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
    void sigRootItemResized(const QSizeF &size);

    /* Notify listeners about selection change: */
    void sigSelectionChanged();

    /* Notify listeners to show corresponding status-bar message: */
    void sigClearStatusMessage();
    void sigShowStatusMessage(const QString &strStatusMessage);

public:

    /* Constructor/destructor: */
    UIGraphicsSelectorModel(QObject *pParent);
    ~UIGraphicsSelectorModel();

    /* API: Scene getter: */
    QGraphicsScene* scene() const;

    /* API: Mode setter: */
    void setMode(UIGraphicsSelectorMode mode);

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

    /* API: Definition stuff: */
    void updateDefinition();

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
    void loadDefinitions();

    /* Helpers: Cleanup stuff: */
    void saveDefinitions();
    void cleanupHandlers();
    void cleanupContextMenu();
    void cleanupRoot();
    void cleanupScene();

    /* Event handlers: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helpers: Focus item stuff: */
    void clearRealFocus();

    /* Helpers: */
    void rebuild();

    /* Helpers: Current item stuff: */
    UIVMItem* searchCurrentItem(const QList<UIGSelectorItem*> &list) const;
    void enumerateCurrentItems(const QList<UIGSelectorItem*> &il, QList<UIVMItem*> &ol) const;
    bool contains(const QList<UIVMItem*> &list, UIVMItem *pItem) const;

    /* Helpers: Definition stuff: */
    void populateDefinitions();
    void extractDefinitions(const QString &strDefinitions, UIGSelectorItem *pParent = 0, int iLevel = 0);
    QString serializeDefinitions(UIGSelectorItem *pParent) const;

    /* Helpers: Navigation stuff: */
    QList<UIGSelectorItem*> createNavigationList(UIGSelectorItem *pItem);

    /* Helpers: Global events stuff: */
    void updateMachineItems(UIGSelectorItem *pParent, const QString &strId);
    void removeMachineItems(UIGSelectorItem *pParent, const QString &strId);

    /* Helpers: Context menu stuff: */
    void popupContextMenu(UIGraphicsSelectorContextMenuType type, QPoint point);

    /* Variables: */
    QGraphicsScene *m_pScene;
    UIGraphicsSelectorMode m_mode;
    QString m_strDefinitions;
    UIGSelectorItem *m_pRoot;
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

