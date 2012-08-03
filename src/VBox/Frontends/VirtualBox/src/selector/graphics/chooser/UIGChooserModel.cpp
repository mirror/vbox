/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserModel class implementation
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

/* Qt includes: */
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QRegExp>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

/* GUI includes: */
#include "UIGChooserModel.h"
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"
#include "UIDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIActionPoolSelector.h"
#include "UIGChooserHandlerMouse.h"
#include "UIGChooserHandlerKeyboard.h"

/* COM includes: */
#include "CMachine.h"
#include "CVirtualBox.h"

UIGChooserModel::UIGChooserModel(QObject *pParent)
    : QObject(pParent)
    , m_pScene(0)
    , m_fSliding(false)
    , m_pLeftRoot(0)
    , m_pRightRoot(0)
    , m_pAfterSlidingFocus(0)
    , m_pMouseHandler(0)
    , m_pKeyboardHandler(0)
    , m_pContextMenuRoot(0)
    , m_pContextMenuGroup(0)
    , m_pContextMenuMachine(0)
{
    /* Prepare scene: */
    prepareScene();

    /* Prepare root: */
    prepareRoot();

    /* Prepare context-menu: */
    prepareContextMenu();

    /* Prepare handlers: */
    prepareHandlers();
}

UIGChooserModel::~UIGChooserModel()
{
    /* Cleanup handlers: */
    cleanupHandlers();

    /* Prepare context-menu: */
    cleanupContextMenu();

    /* Cleanup root: */
    cleanupRoot();

    /* Cleanup scene: */
    cleanupScene();
 }

void UIGChooserModel::load()
{
    /* Prepare group-tree: */
    prepareGroupTree();
}

void UIGChooserModel::save()
{
    /* Cleanup group-tree: */
    cleanupGroupTree();
}

QGraphicsScene* UIGChooserModel::scene() const
{
    return m_pScene;
}

UIGChooserItem* UIGChooserModel::mainRoot() const
{
    return m_rootStack.first();
}

UIGChooserItem* UIGChooserModel::root() const
{
    return m_rootStack.last();
}

void UIGChooserModel::indentRoot(UIGChooserItem *pNewRootItem)
{
    /* Do nothing is sliding already: */
    if (m_fSliding)
        return;

    /* We are sliding: */
    m_fSliding = true;
    emit sigSlidingStarted();

    /* Hiding root: */
    root()->hide();

    /* Create left root: */
    m_pLeftRoot = new UIGChooserItemGroup(scene(), root()->toGroupItem());
    m_pLeftRoot->setPos(0, 0);
    m_pLeftRoot->resize(root()->geometry().size());

    /* Create right root: */
    m_pRightRoot = new UIGChooserItemGroup(scene(), pNewRootItem->toGroupItem());
    m_pRightRoot->setPos(root()->geometry().width(), 0);
    m_pRightRoot->resize(root()->geometry().size());

    /* Indent root: */
    m_rootStack << pNewRootItem;
    root()->setRoot(true);
    m_pAfterSlidingFocus = root()->items().first();

    /* Slide root: */
    slideRoot(true);
}

void UIGChooserModel::unindentRoot()
{
    /* Do nothing is sliding already: */
    if (m_fSliding)
        return;

    /* We are sliding: */
    m_fSliding = true;
    emit sigSlidingStarted();

    /* Hiding root: */
    root()->hide();
    root()->setRoot(false);

    /* Create left root: */
    m_pLeftRoot = new UIGChooserItemGroup(scene(), m_rootStack.at(m_rootStack.size() - 2)->toGroupItem());
    m_pLeftRoot->setPos(- root()->geometry().width(), 0);
    m_pLeftRoot->resize(root()->geometry().size());

    /* Create right root: */
    m_pRightRoot = new UIGChooserItemGroup(scene(), root()->toGroupItem());
    m_pRightRoot->setPos(0, 0);
    m_pRightRoot->resize(root()->geometry().size());

    /* Unindent root: */
    m_pAfterSlidingFocus = root();
    m_rootStack.removeLast();

    /* Slide root: */
    slideRoot(false);
}

bool UIGChooserModel::isSlidingInProgress() const
{
    return m_fSliding;
}

void UIGChooserModel::setCurrentItem(int iItemIndex)
{
    /* Make sure passed index feats the bounds: */
    if (iItemIndex >= 0 && iItemIndex < navigationList().size())
    {
        /* And call for other wrapper: */
        setCurrentItem(navigationList().at(iItemIndex));
    }
    else
        AssertMsgFailed(("Passed index out of bounds!"));
}

void UIGChooserModel::setCurrentItem(UIGChooserItem *pItem)
{
    /* If navigation list contains passed item: */
    if (navigationList().contains(pItem))
    {
        /* Pass focus/selection to that item: */
        setFocusItem(pItem, true);
    }
    else
        AssertMsgFailed(("Passed item not in navigation list!"));
}

void UIGChooserModel::unsetCurrentItem()
{
    /* Clear focus/selection: */
    setFocusItem(0, true);
}

UIVMItem* UIGChooserModel::currentItem() const
{
    /* Search for the first selected machine: */
    return searchCurrentItem(selectionList());
}

QList<UIVMItem*> UIGChooserModel::currentItems() const
{
    /* Populate list of selected machines: */
    QList<UIVMItem*> currentItemList;
    enumerateCurrentItems(selectionList(), currentItemList);
    return currentItemList;
}

void UIGChooserModel::setCurrentItemDefinition(const QString &strDefinition)
{
    /* Make sure something was passed: */
    if (strDefinition.isEmpty())
    {
        if (mainRoot()->hasItems())
            setCurrentItem(0);
        else
            unsetCurrentItem();
        return;
    }

    /* Parse definitions: */
    QString strItemType = strDefinition.section('=', 0, 0);
    QString strItemName = strDefinition.section('=', 1, -1);
    UIGChooserItem *pItem = 0;

    /* Its group item? */
    if (strItemType == "g")
    {
        /* Make sure group item with passed id exists: */
        pItem = findGroupItem(strItemName, mainRoot());
    }
    /* Its machine item? */
    else if (strItemType == "m")
    {
        /* Make sure machine with passed name registered: */
        CMachine machine = vboxGlobal().virtualBox().FindMachine(strItemName);
        if (!machine.isNull())
        {
            /* Make sure machine item with passed id exists: */
            pItem = findMachineItem(machine.GetName(), mainRoot());
        }
    }

    /* Found nothing? */
    if (!pItem)
    {
        setCurrentItem(0);
        return;
    }

    /* Select desired item: */
    if (navigationList().contains(pItem))
        setCurrentItem(pItem);
    else
        setCurrentItem(0);
}

QString UIGChooserModel::currentItemDefinition() const
{
    /* Determine item type: */
    QString strItemType;
    QString strItemName;

    /* Get first selected item: */
    UIGChooserItem *pSelectedItem = selectionList().isEmpty() ? 0 : selectionList().first();
    /* Item exists? */
    if (pSelectedItem)
    {
        /* Update item type: */
        if (pSelectedItem->type() == UIGChooserItemType_Group)
            strItemType = "g";
        else if (pSelectedItem->type() == UIGChooserItemType_Machine)
            strItemType = "m";

        /* Update item name: */
        strItemName = pSelectedItem->name();
    }

    /* Return result: */
    return pSelectedItem ? strItemType + "=" + strItemName : QString();
}

bool UIGChooserModel::singleGroupSelected() const
{
    return selectionList().size() == 1 &&
           selectionList().first()->type() == UIGChooserItemType_Group;
}

void UIGChooserModel::setFocusItem(UIGChooserItem *pItem, bool fWithSelection /* = false */)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Something changed? */
    if (m_pFocusItem != pItem || !pItem)
    {
        /* Remember previous focus item: */
        QPointer<UIGChooserItem> pPreviousFocusItem = m_pFocusItem;
        /* Set new focus item: */
        m_pFocusItem = pItem;

        /* Should we move selection too? */
        if (fWithSelection)
        {
            /* Clear selection: */
            clearSelectionList();
            /* Add focus item into selection (if any): */
            if (m_pFocusItem)
                addToSelectionList(m_pFocusItem);
            /* Notify selection changed: */
            notifySelectionChanged();
        }

        /* Update previous focus item (if any): */
        if (pPreviousFocusItem)
        {
            pPreviousFocusItem->disconnect(this);
            pPreviousFocusItem->update();
        }
        /* Update new focus item (if any): */
        if (m_pFocusItem)
        {
            connect(m_pFocusItem, SIGNAL(destroyed(QObject*)), this, SLOT(sltFocusItemDestroyed()));
            m_pFocusItem->update();
        }
    }
}

UIGChooserItem* UIGChooserModel::focusItem() const
{
    return m_pFocusItem;
}

QGraphicsItem* UIGChooserModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene()->itemAt(position, deviceTransform);
}

void UIGChooserModel::updateGroupTree()
{
    updateGroupTree(mainRoot());
}

const QList<UIGChooserItem*>& UIGChooserModel::navigationList() const
{
    return m_navigationList;
}

void UIGChooserModel::removeFromNavigationList(UIGChooserItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_navigationList.removeAll(pItem);
}

void UIGChooserModel::clearNavigationList()
{
    m_navigationList.clear();
}

void UIGChooserModel::updateNavigation()
{
    /* Recreate navigation list: */
    clearNavigationList();
    m_navigationList = createNavigationList(root());
}

const QList<UIGChooserItem*>& UIGChooserModel::selectionList() const
{
    return m_selectionList;
}

void UIGChooserModel::addToSelectionList(UIGChooserItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_selectionList << pItem;
    pItem->update();
}

void UIGChooserModel::removeFromSelectionList(UIGChooserItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_selectionList.removeAll(pItem);
    pItem->update();
}

void UIGChooserModel::clearSelectionList()
{
    QList<UIGChooserItem*> oldSelectedList = m_selectionList;
    m_selectionList.clear();
    foreach (UIGChooserItem *pItem, oldSelectedList)
        pItem->update();
}

void UIGChooserModel::notifySelectionChanged()
{
    /* Make sure selection item list is never empty
     * if at least one item (for example 'focus') present: */
    if (selectionList().isEmpty() && focusItem())
        addToSelectionList(focusItem());
    /* Notify listeners about selection change: */
    emit sigSelectionChanged();
}

void UIGChooserModel::updateLayout()
{
    /* No layout updates while sliding: */
    if (m_fSliding)
        return;

    /* Initialize variables: */
    int iSceneMargin = data(SelectorModelData_Margin).toInt();
    QSize viewportSize = scene()->views()[0]->viewport()->size();
    int iViewportWidth = viewportSize.width() - 2 * iSceneMargin;
    int iViewportHeight = viewportSize.height() - 2 * iSceneMargin;
    /* Update all the size-hints recursively: */
    root()->updateSizeHint();
    /* Set root item position: */
    root()->setPos(iSceneMargin, iSceneMargin);
    /* Set root item size: */
    root()->resize(iViewportWidth, iViewportHeight);
    /* Relayout root item: */
    root()->updateLayout();
    /* Notify listener about root-item relayouted: */
    emit sigRootItemResized(root()->geometry().size(), root()->minimumWidthHint());
}

void UIGChooserModel::setCurrentDragObject(QDrag *pDragObject)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Remember new drag-object: */
    m_pCurrentDragObject = pDragObject;
    connect(m_pCurrentDragObject, SIGNAL(destroyed(QObject*)), this, SLOT(sltCurrentDragObjectDestroyed()));
}

void UIGChooserModel::activate()
{
    gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow)->activate(QAction::Trigger);
}

QString UIGChooserModel::uniqueGroupName(UIGChooserItem *pRoot)
{
    /* Enumerate all the group names: */
    QStringList groupNames;
    foreach (UIGChooserItem *pItem, pRoot->items(UIGChooserItemType_Group))
        groupNames << pItem->name();

    /* Prepare reg-exp: */
    QString strMinimumName = tr("New group");
    QString strShortTemplate = strMinimumName;
    QString strFullTemplate = strShortTemplate + QString(" (\\d+)");
    QRegExp shortRegExp(strShortTemplate);
    QRegExp fullRegExp(strFullTemplate);

    /* Search for the maximum index: */
    int iMinimumPossibleNumber = 0;
    foreach (const QString &strName, groupNames)
    {
        if (shortRegExp.exactMatch(strName))
            iMinimumPossibleNumber = qMax(iMinimumPossibleNumber, 2);
        else if (fullRegExp.exactMatch(strName))
            iMinimumPossibleNumber = qMax(iMinimumPossibleNumber, fullRegExp.cap(1).toInt() + 1);
    }

    /* Prepare result: */
    QString strResult = strMinimumName;
    if (iMinimumPossibleNumber)
        strResult += " " + QString::number(iMinimumPossibleNumber);
    return strResult;
}

void UIGChooserModel::sltMachineStateChanged(QString strId, KMachineState)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, mainRoot());
}

void UIGChooserModel::sltMachineDataChanged(QString strId)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, mainRoot());
}

void UIGChooserModel::sltMachineRegistered(QString strId, bool fRegistered)
{
    /* New VM registered? */
    if (fRegistered)
    {
        /* Search for corresponding machine: */
        CMachine machine = vboxGlobal().virtualBox().FindMachine(strId);
        /* Machine was found? */
        if (!machine.isNull())
        {
            /* Add new machine item: */
            addMachineIntoTheTree(machine, true);
            /* And update model: */
            updateNavigation();
            updateLayout();
            setCurrentItem(findMachineItem(machine.GetName(), mainRoot()));
        }
    }
    /* Existing VM unregistered? */
    else
    {
        /* Remove machine items with passed id: */
        removeMachineItems(strId, mainRoot());
        /* And update model: */
        updateGroupTree();
        updateNavigation();
        updateLayout();
        if (mainRoot()->hasItems())
            setCurrentItem(0);
        else
            unsetCurrentItem();
    }
}

void UIGChooserModel::sltSessionStateChanged(QString strId, KSessionState)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, mainRoot());
}

void UIGChooserModel::sltSnapshotChanged(QString strId, QString)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, mainRoot());
}

void UIGChooserModel::sltHandleViewResized()
{
    /* Relayout: */
    updateLayout();
}

void UIGChooserModel::sltCurrentDragObjectDestroyed()
{
    /* Reset drag tokens starting from the root item: */
    root()->resetDragToken();
}

void UIGChooserModel::sltRemoveCurrentlySelectedGroup()
{
    /* Make sure focus item is of group type! */
    AssertMsg(focusItem()->type() == UIGChooserItemType_Group, ("This is not group item!"));

    /* Compose focus group name: */
    QString strFocusGroupName = fullName(focusItem());
    /* Enumerate all the unique sub-machine-items recursively: */
    QList<UIVMItem*> items;
    enumerateCurrentItems(focusItem()->items(), items);
    /* Enumerate all the machine groups recursively: */
    QMap<QString, QStringList> groupMap;
    gatherGroupTree(groupMap, mainRoot());

    /* For each the sub-machine-item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Get machine groups: */
        QStringList groups = groupMap.value(pItem->id());
        /* For each the machine group: */
        bool fUniqueMachine = true;
        foreach (const QString &strGroupName, groups)
        {
            /* Check if this item is unique: */
            if (strGroupName != strFocusGroupName &&
                !strGroupName.startsWith(strFocusGroupName + "/"))
            {
                fUniqueMachine = false;
                break;
            }
        }
        /* Add unique item into root: */
        if (fUniqueMachine)
            createMachineItem(pItem->machine(), mainRoot());
    }

    /* Delete focus group: */
    delete focusItem();

    /* And update model: */
    updateGroupTree();
    updateNavigation();
    updateLayout();
    if (mainRoot()->hasItems())
        setCurrentItem(0);
    else
        unsetCurrentItem();
}

void UIGChooserModel::sltRemoveCurrentlySelectedMachine()
{
    /* Enumerate all the selected machine items: */
    QList<UIGChooserItem*> selectedMachineItemList = gatherMachineItems(selectionList());
    /* Enumerate all the existing machine items: */
    QList<UIGChooserItem*> existingMachineItemList = gatherMachineItems(mainRoot()->items());

    /* Prepare removing map: */
    QMap<QString, bool> removingMap;

    /* For each selected machine item: */
    foreach (UIGChooserItem *pItem, selectedMachineItemList)
    {
        /* Get item name: */
        QString strName = pItem->name();

        /* Check if we already decided for that machine: */
        if (removingMap.contains(strName))
            continue;

        /* Selected copy count: */
        int iSelectedCopyCount = 0;
        foreach (UIGChooserItem *pSelectedItem, selectedMachineItemList)
            if (pSelectedItem->name() == strName)
                ++iSelectedCopyCount;

        /* Existing copy count: */
        int iExistingCopyCount = 0;
        foreach (UIGChooserItem *pExistingItem, existingMachineItemList)
            if (pExistingItem->name() == strName)
                ++iExistingCopyCount;

        /* If selected copy count equal to existing copy count,
         * we will propose ro unregister machine fully else
         * we will just propose to remove selected items: */
        removingMap.insert(strName, iSelectedCopyCount == iExistingCopyCount);
    }

    /* If we have something to remove: */
    if (removingMap.values().contains(false))
    {
        /* Gather names: */
        QStringList names;
        foreach (const QString &strName, removingMap.keys())
            if (!removingMap[strName])
                names << strName;
        removeMachineItems(names, selectedMachineItemList);
    }
    /* If we have something to unregister: */
    if (removingMap.values().contains(true))
    {
        /* Gather names: */
        QStringList names;
        foreach (const QString &strName, removingMap.keys())
            if (removingMap[strName])
                names << strName;
        unregisterMachines(names);
    }
}

void UIGChooserModel::sltAddGroupBasedOnChosenItems()
{
    /* Create new group in the current root: */
    UIGChooserItemGroup *pNewGroupItem = new UIGChooserItemGroup(root(), uniqueGroupName(root()));
    /* Enumerate all the currently chosen items: */
    QStringList busyGroupNames;
    QStringList busyMachineNames;
    foreach (UIGChooserItem *pItem, selectionList())
    {
        /* For each of known types: */
        switch (pItem->type())
        {
            case UIGChooserItemType_Group:
            {
                /* Avoid name collisions: */
                if (busyGroupNames.contains(pItem->name()))
                    break;
                /* Copy group item: */
                new UIGChooserItemGroup(pNewGroupItem, pItem->toGroupItem());
                busyGroupNames << pItem->name();
                break;
            }
            case UIGChooserItemType_Machine:
            {
                /* Avoid name collisions: */
                if (busyMachineNames.contains(pItem->name()))
                    break;
                /* Copy machine item: */
                new UIGChooserItemMachine(pNewGroupItem, pItem->toMachineItem());
                busyMachineNames << pItem->name();
                break;
            }
        }
    }
    /* Update model: */
    updateNavigation();
    updateLayout();
    setCurrentItem(pNewGroupItem);
}

void UIGChooserModel::sltStartEditingSelectedGroup()
{
    /* Only for single selected group: */
    if (!singleGroupSelected())
        return;

    /* Start editing group name: */
    selectionList().first()->startEditing();
}

void UIGChooserModel::sltActionHovered(QAction *pAction)
{
    emit sigShowStatusMessage(pAction->statusTip());
}

void UIGChooserModel::sltFocusItemDestroyed()
{
    AssertMsgFailed(("Focus item destroyed!"));
}

void UIGChooserModel::sltLeftRootSlidingProgress()
{
    /* Update left root: */
    m_pLeftRoot->updateSizeHint();
    m_pLeftRoot->updateLayout();
}

void UIGChooserModel::sltRightRootSlidingProgress()
{
    /* Update right root: */
    m_pRightRoot->updateSizeHint();
    m_pRightRoot->updateLayout();
}

void UIGChooserModel::sltSlidingComplete()
{
    /* Delete temporary roots: */
    delete m_pLeftRoot;
    m_pLeftRoot = 0;
    delete m_pRightRoot;
    m_pRightRoot = 0;

    /* We are no more sliding: */
    m_fSliding = false;

    /* Update model: */
    updateGroupTree();
    updateNavigation();
    updateLayout();
    if (m_pAfterSlidingFocus)
    {
        setCurrentItem(m_pAfterSlidingFocus);
        m_pAfterSlidingFocus = 0;
    }
    else
    {
        if (root()->hasItems())
            setCurrentItem(root()->items().first());
        else
            unsetCurrentItem();
    }
}

void UIGChooserModel::sltSortParentGroup()
{
    if (!selectionList().isEmpty())
        sortItems(selectionList().first()->parentItem());
}

void UIGChooserModel::sltSortGroup()
{
    if (singleGroupSelected())
        sortItems(selectionList().first());
}

QVariant UIGChooserModel::data(int iKey) const
{
    switch (iKey)
    {
        case SelectorModelData_Margin: return 0;
        default: break;
    }
    return QVariant();
}

void UIGChooserModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    m_pScene->installEventFilter(this);
}

void UIGChooserModel::prepareRoot()
{
    m_rootStack << new UIGChooserItemGroup(scene());
}

void UIGChooserModel::prepareContextMenu()
{
    /* Context menu for empty group: */
    m_pContextMenuRoot = new QMenu;
    m_pContextMenuRoot->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_NewWizard));
    m_pContextMenuRoot->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_AddDialog));

    /* Context menu for group: */
    m_pContextMenuGroup = new QMenu;
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_NewWizard));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_AddDialog));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_RenameDialog));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_RemoveDialog));
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_State_Group_StartOrShow));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Toggle_Group_PauseAndResume));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_Reset));
    m_pContextMenuGroup->addMenu(gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu());
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndex_Simple_LogDialog));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_Refresh));
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_ShowInFileManager));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_CreateShortcut));
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Common_SortParent));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Group_Sort));

    /* Context menu for machine(s): */
    m_pContextMenuMachine = new QMenu;
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddGroupDialog));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog));
    m_pContextMenuMachine->addSeparator();
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset));
    m_pContextMenuMachine->addMenu(gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu());
    m_pContextMenuMachine->addSeparator();
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndex_Simple_LogDialog));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh));
    m_pContextMenuMachine->addSeparator();
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut));
    m_pContextMenuMachine->addSeparator();
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Common_SortParent));

    connect(m_pContextMenuRoot, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));
    connect(m_pContextMenuGroup, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));
    connect(m_pContextMenuMachine, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));

    connect(gActionPool->action(UIActionIndexSelector_Simple_Group_RenameDialog), SIGNAL(triggered()),
            this, SLOT(sltStartEditingSelectedGroup()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Group_RemoveDialog), SIGNAL(triggered()),
            this, SLOT(sltRemoveCurrentlySelectedGroup()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddGroupDialog), SIGNAL(triggered()),
            this, SLOT(sltAddGroupBasedOnChosenItems()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog), SIGNAL(triggered()),
            this, SLOT(sltRemoveCurrentlySelectedMachine()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Common_SortParent), SIGNAL(triggered()),
            this, SLOT(sltSortParentGroup()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Group_Sort), SIGNAL(triggered()),
            this, SLOT(sltSortGroup()));
}

void UIGChooserModel::prepareHandlers()
{
    m_pMouseHandler = new UIGChooserHandlerMouse(this);
    m_pKeyboardHandler = new UIGChooserHandlerKeyboard(this);
}

void UIGChooserModel::prepareGroupTree()
{
    /* Load group tree: */
    loadGroupTree();

    /* Update model: */
    updateNavigation();
    updateLayout();
    if (mainRoot()->hasItems())
        setCurrentItem(0);
    else
        unsetCurrentItem();
}

void UIGChooserModel::cleanupGroupTree()
{
    /* Save group tree: */
    saveGroupTree();
    /* Save order: */
    saveGroupsOrder();
}

void UIGChooserModel::cleanupHandlers()
{
    delete m_pKeyboardHandler;
    m_pKeyboardHandler = 0;
    delete m_pMouseHandler;
    m_pMouseHandler = 0;
}

void UIGChooserModel::cleanupContextMenu()
{
    delete m_pContextMenuRoot;
    m_pContextMenuRoot = 0;
    delete m_pContextMenuGroup;
    m_pContextMenuGroup = 0;
    delete m_pContextMenuMachine;
    m_pContextMenuMachine = 0;
}

void UIGChooserModel::cleanupRoot()
{
    delete mainRoot();
    m_rootStack.clear();
}

void UIGChooserModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

bool UIGChooserModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != m_pScene)
        return QObject::eventFilter(pWatched, pEvent);

    /* Process only item is focused by model, not by scene: */
    if (scene()->focusItem())
        return QObject::eventFilter(pWatched, pEvent);

    /* Checking event-type: */
    switch (pEvent->type())
    {
        /* Keyboard handler: */
        case QEvent::KeyPress:
            return m_pKeyboardHandler->handle(static_cast<QKeyEvent*>(pEvent), UIKeyboardEventType_Press);
        case QEvent::KeyRelease:
            return m_pKeyboardHandler->handle(static_cast<QKeyEvent*>(pEvent), UIKeyboardEventType_Release);
        /* Mouse handler: */
        case QEvent::GraphicsSceneMousePress:
            return m_pMouseHandler->handle(static_cast<QGraphicsSceneMouseEvent*>(pEvent), UIMouseEventType_Press);
        case QEvent::GraphicsSceneMouseRelease:
            return m_pMouseHandler->handle(static_cast<QGraphicsSceneMouseEvent*>(pEvent), UIMouseEventType_Release);
        case QEvent::GraphicsSceneMouseDoubleClick:
            return m_pMouseHandler->handle(static_cast<QGraphicsSceneMouseEvent*>(pEvent), UIMouseEventType_DoubleClick);
        /* Context menu: */
        case QEvent::GraphicsSceneContextMenu:
            return processContextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent*>(pEvent));
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIGChooserModel::clearRealFocus()
{
    /* Set real focus to null: */
    scene()->setFocusItem(0);
}

UIVMItem* UIGChooserModel::searchCurrentItem(const QList<UIGChooserItem*> &list) const
{
    /* Iterate over all the passed items: */
    foreach (UIGChooserItem *pItem, list)
    {
        /* If item is machine, just return it: */
        if (pItem->type() == UIGChooserItemType_Machine)
        {
            if (UIGChooserItemMachine *pMachineItem = pItem->toMachineItem())
                return pMachineItem;
        }
        /* If item is group: */
        else if (pItem->type() == UIGChooserItemType_Group)
        {
            /* If it have at least one machine item: */
            if (pItem->hasItems(UIGChooserItemType_Machine))
                /* Iterate over all the machine items recursively: */
                return searchCurrentItem(pItem->items(UIGChooserItemType_Machine));
            /* If it have at least one group item: */
            else if (pItem->hasItems(UIGChooserItemType_Group))
                /* Iterate over all the group items recursively: */
                return searchCurrentItem(pItem->items(UIGChooserItemType_Group));
        }
    }
    return 0;
}

void UIGChooserModel::enumerateCurrentItems(const QList<UIGChooserItem*> &il, QList<UIVMItem*> &ol) const
{
    /* Enumerate all the passed items: */
    foreach (UIGChooserItem *pItem, il)
    {
        /* If item is machine, add if missed: */
        if (pItem->type() == UIGChooserItemType_Machine)
        {
            if (UIGChooserItemMachine *pMachineItem = pItem->toMachineItem())
                if (!contains(ol, pMachineItem))
                    ol << pMachineItem;
        }
        /* If item is group: */
        else if (pItem->type() == UIGChooserItemType_Group)
        {
            /* Enumerate all the machine items recursively: */
            enumerateCurrentItems(pItem->items(UIGChooserItemType_Machine), ol);
            /* Enumerate all the group items recursively: */
            enumerateCurrentItems(pItem->items(UIGChooserItemType_Group), ol);
        }
    }
}

bool UIGChooserModel::contains(const QList<UIVMItem*> &list, UIVMItem *pItem) const
{
    /* Check if passed list contains passed item: */
    foreach (UIVMItem *pIteratedItem, list)
        if (pIteratedItem->id() == pItem->id())
            return true;
    return false;
}

void UIGChooserModel::loadGroupTree()
{
    /* Add all the machines we have into the group-tree: */
    foreach (const CMachine &machine, vboxGlobal().virtualBox().GetMachines())
        addMachineIntoTheTree(machine);
}

void UIGChooserModel::addMachineIntoTheTree(const CMachine &machine, bool fMakeItVisible /* = false */)
{
    /* Which groups passed machine attached to? */
    QVector<QString> groups = machine.GetGroups();
    foreach (QString strGroup, groups)
    {
        /* Remove last '/' if any: */
        if (strGroup.right(1) == "/")
            strGroup.truncate(strGroup.size() - 1);
        /* Create machine item with found group item as parent: */
        createMachineItem(machine, getGroupItem(strGroup, mainRoot(), fMakeItVisible));
    }
    /* Update group definitions: */
    m_groups[machine.GetId()] = UIStringSet::fromList(groups.toList());
}

UIGChooserItem* UIGChooserModel::getGroupItem(const QString &strName, UIGChooserItem *pParentItem, bool fAllGroupsOpened)
{
    /* Check passed stuff: */
    if (pParentItem->name() == strName)
        return pParentItem;

    /* Prepare variables: */
    QString strFirstSubName = strName.section('/', 0, 0);
    QString strFirstSuffix = strName.section('/', 1, -1);
    QString strSecondSubName = strFirstSuffix.section('/', 0, 0);
    QString strSecondSuffix = strFirstSuffix.section('/', 1, -1);

    /* Passed group name equal to first sub-name: */
    if (pParentItem->name() == strFirstSubName)
    {
        /* Make sure first-suffix is NOT empty: */
        AssertMsg(!strFirstSuffix.isEmpty(), ("Invalid group name!"));
        /* Trying to get group item among our children: */
        foreach (UIGChooserItem *pGroupItem, pParentItem->items(UIGChooserItemType_Group))
            if (pGroupItem->name() == strSecondSubName)
                return getGroupItem(strFirstSuffix, pGroupItem, fAllGroupsOpened);
    }

    /* Found nothing? Creating: */
    UIGChooserItemGroup *pNewGroupItem =
            new UIGChooserItemGroup(/* Parent item and desired group name: */
                                    pParentItem, strSecondSubName,
                                    /* Should be new group opened when created? */
                                    fAllGroupsOpened || shouldBeGroupOpened(pParentItem, strSecondSubName),
                                    /* Which position new group item should be placed in? */
                                    getDesiredPosition(pParentItem, UIGChooserItemType_Group, strSecondSubName));
    return strSecondSuffix.isEmpty() ? pNewGroupItem : getGroupItem(strFirstSuffix, pNewGroupItem, fAllGroupsOpened);
}

bool UIGChooserModel::shouldBeGroupOpened(UIGChooserItem *pParentItem, const QString &strName)
{
    /* Prepare extra-data key for the parent-item: */
    QString strExtraDataKey = UIDefs::GUI_GroupDefinitions + fullName(pParentItem);
    /* Read group definitions: */
    QStringList definitions = vboxGlobal().virtualBox().GetExtraDataStringList(strExtraDataKey);
    /* Return 'false' if no definitions found: */
    if (definitions.isEmpty())
        return false;

    /* Prepare required group definition reg-exp: */
    QString strDefinitionTemplate = QString("g(\\S)*=%1").arg(strName);
    QRegExp definitionRegExp(strDefinitionTemplate);
    /* For each the group definition: */
    for (int i = 0; i < definitions.size(); ++i)
    {
        /* Get current definition: */
        const QString &strDefinition = definitions[i];
        /* Check if this is required definition: */
        if (definitionRegExp.indexIn(strDefinition) == 0)
        {
            /* Get group descriptor: */
            QString strDescriptor(definitionRegExp.cap(1));
            if (strDescriptor.contains('o'))
                return true;
        }
    }

    /* Return 'false' by default: */
    return false;
}

int UIGChooserModel::getDesiredPosition(UIGChooserItem *pParentItem, UIGChooserItemType type, const QString &strName)
{
    /* End of list (by default)? */
    int iNewItemDesiredPosition = -1;
    /* Which position should be new item placed by definitions: */
    int iNewItemDefinitionPosition = positionFromDefinitions(pParentItem, type, strName);
    /* If some position wanted: */
    if (iNewItemDefinitionPosition != -1)
    {
        /* Start of list if some definition present: */
        iNewItemDesiredPosition = 0;
        /* We have to check all the existing item positions: */
        QList<UIGChooserItem*> items = pParentItem->items(type);
        for (int i = items.size() - 1; i >= 0; --i)
        {
            /* Get current item: */
            UIGChooserItem *pItem = items[i];
            /* Which position should be current item placed by definitions? */
            int iItemDefinitionPosition = positionFromDefinitions(pParentItem, type, pItem->name());
            /* If some position wanted: */
            if (iItemDefinitionPosition != -1)
            {
                AssertMsg(iItemDefinitionPosition != iNewItemDefinitionPosition, ("Incorrect definitions!"));
                if (iItemDefinitionPosition < iNewItemDefinitionPosition)
                {
                    iNewItemDesiredPosition = i + 1;
                    break;
                }
            }
        }
    }
    /* Return desired item position: */
    return iNewItemDesiredPosition;
}

int UIGChooserModel::positionFromDefinitions(UIGChooserItem *pParentItem, UIGChooserItemType type, const QString &strName)
{
    /* Prepare extra-data key for the parent-item: */
    QString strExtraDataKey = UIDefs::GUI_GroupDefinitions + fullName(pParentItem);
    /* Read group definitions: */
    QStringList definitions = vboxGlobal().virtualBox().GetExtraDataStringList(strExtraDataKey);
    /* Return 'false' if no definitions found: */
    if (definitions.isEmpty())
        return -1;

    /* Prepare definition reg-exp: */
    QString strDefinitionTemplateShort;
    QString strDefinitionTemplateFull;
    switch (type)
    {
        case UIGChooserItemType_Group:
            strDefinitionTemplateShort = QString("^g(\\S)*=");
            strDefinitionTemplateFull = QString("^g(\\S)*=%1$").arg(strName);
            break;
        case UIGChooserItemType_Machine:
            strDefinitionTemplateShort = QString("m=");
            strDefinitionTemplateFull = QString("m=%1").arg(strName);
            break;
        default: return -1;
    }
    QRegExp definitionRegExpShort(strDefinitionTemplateShort);
    QRegExp definitionRegExpFull(strDefinitionTemplateFull);

    /* For each the definition: */
    int iDefinitionIndex = -1;
    for (int i = 0; i < definitions.size(); ++i)
    {
        /* Get current definition: */
        QString strDefinition = definitions[i];
        /* Check if this definition is of required type: */
        if (definitionRegExpShort.indexIn(strDefinition) == 0)
        {
            ++iDefinitionIndex;
            /* Check if this definition is exactly what we need: */
            if (definitionRegExpFull.indexIn(strDefinition) == 0)
                return iDefinitionIndex;
        }
    }

    /* Return result: */
    return -1;
}

void UIGChooserModel::createMachineItem(const CMachine &machine, UIGChooserItem *pParentItem)
{
    /* Create corresponding item: */
    new UIGChooserItemMachine(/* Parent item and corresponding machine: */
                              pParentItem, machine,
                              /* Which position new group item should be placed in? */
                              getDesiredPosition(pParentItem, UIGChooserItemType_Machine, machine.GetName()));
}

void UIGChooserModel::saveGroupTree()
{
    /* Prepare machine map: */
    QMap<QString, QStringList> groups;
    /* Iterate over all the items: */
    gatherGroupTree(groups, mainRoot());
    /* Saving groups: */
    foreach (const QString &strId, groups.keys())
    {
        /* Get new group list: */
        const QStringList &newGroupList = groups.value(strId);
        /* Get old group set: */
        AssertMsg(m_groups.contains(strId), ("Invalid group set!"));
        const UIStringSet &oldGroupSet = m_groups.value(strId);
        /* Get new group set: */
        const UIStringSet &newGroupSet = UIStringSet::fromList(newGroupList);
        /* Is group set changed? */
        if (oldGroupSet != newGroupSet)
        {
            /* Open session to save machine settings: */
            CSession session = vboxGlobal().openSession(strId);
            if (session.isNull())
                return;
            /* Get machine: */
            CMachine machine = session.GetMachine();
            /* Save groups: */
//            printf(" Saving groups for machine {%s}: {%s}\n",
//                   machine.GetName().toAscii().constData(),
//                   newGroupList.join(",").toAscii().constData());
            machine.SetGroups(newGroupList.toVector());
            machine.SaveSettings();
            if (!machine.isOk())
                msgCenter().cannotSaveMachineSettings(machine);
            /* Close the session: */
            session.UnlockMachine();
        }
    }
}

void UIGChooserModel::saveGroupsOrder()
{
    /* Clear all the extra-data records related to group-definitions: */
    const QVector<QString> extraDataKeys = vboxGlobal().virtualBox().GetExtraDataKeys();
    foreach (const QString &strKey, extraDataKeys)
        if (strKey.startsWith(UIDefs::GUI_GroupDefinitions))
            vboxGlobal().virtualBox().SetExtraData(strKey, QString());
    /* Save order starting from the root-item: */
    saveGroupsOrder(mainRoot());
}

void UIGChooserModel::saveGroupsOrder(UIGChooserItem *pParentItem)
{
    /* Prepare extra-data key for current group: */
    QString strExtraDataKey = UIDefs::GUI_GroupDefinitions + fullName(pParentItem);
    /* Gather item order: */
    QStringList order;
    foreach (UIGChooserItem *pItem, pParentItem->items(UIGChooserItemType_Group))
    {
        saveGroupsOrder(pItem);
        QString strGroupDescriptor(pItem->toGroupItem()->opened() ? "go" : "gc");
        order << QString("%1=%2").arg(strGroupDescriptor, pItem->name());
    }
    foreach (UIGChooserItem *pItem, pParentItem->items(UIGChooserItemType_Machine))
        order << QString("m=%1").arg(pItem->name());
    vboxGlobal().virtualBox().SetExtraDataStringList(strExtraDataKey, order);
}

void UIGChooserModel::gatherGroupTree(QMap<QString, QStringList> &groups,
                                              UIGChooserItem *pParentGroup)
{
    /* Iterate over all the machine items: */
    foreach (UIGChooserItem *pItem, pParentGroup->items(UIGChooserItemType_Machine))
        if (UIGChooserItemMachine *pMachineItem = pItem->toMachineItem())
            groups[pMachineItem->id()] << fullName(pParentGroup);
    /* Iterate over all the group items: */
    foreach (UIGChooserItem *pItem, pParentGroup->items(UIGChooserItemType_Group))
        gatherGroupTree(groups, pItem);
}

QString UIGChooserModel::fullName(UIGChooserItem *pItem)
{
    /* Return '/' for root-group: */
    if (!pItem->parentItem())
        return QString("/");
    /* Get full parent name, append with '/' if not yet appended: */
    QString strParentFullName = fullName(pItem->parentItem());
    if (!strParentFullName.endsWith("/"))
        strParentFullName += QString("/");
    /* Return full item name based on parent prefix: */
    return strParentFullName + pItem->name();
}

void UIGChooserModel::updateGroupTree(UIGChooserItem *pGroupItem)
{
    /* Cleanup all the group items first: */
    foreach (UIGChooserItem *pSubGroupItem, pGroupItem->items(UIGChooserItemType_Group))
        updateGroupTree(pSubGroupItem);
    if (!pGroupItem->hasItems())
    {
        /* Cleanup only non-root items: */
        if (!pGroupItem->isRoot())
            delete pGroupItem;
        /* Unindent root items: */
        else if (root() != mainRoot())
            unindentRoot();
    }
}

QList<UIGChooserItem*> UIGChooserModel::createNavigationList(UIGChooserItem *pItem)
{
    /* Prepare navigation list: */
    QList<UIGChooserItem*> navigationItems;

    /* Iterate over all the group items: */
    foreach (UIGChooserItem *pGroupItem, pItem->items(UIGChooserItemType_Group))
    {
        navigationItems << pGroupItem;
        if (pGroupItem->toGroupItem()->opened())
            navigationItems << createNavigationList(pGroupItem);
    }
    /* Iterate over all the machine items: */
    foreach (UIGChooserItem *pMachineItem, pItem->items(UIGChooserItemType_Machine))
        navigationItems << pMachineItem;

    /* Return navigation list: */
    return navigationItems;
}

void UIGChooserModel::updateMachineItems(const QString &strId, UIGChooserItem *pParent)
{
    /* For each group item in passed parent: */
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Group))
        updateMachineItems(strId, pItem->toGroupItem());
    /* For each machine item in passed parent: */
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Machine))
        if (UIGChooserItemMachine *pMachineItem = pItem->toMachineItem())
            if (pMachineItem->id() == strId)
            {
                pMachineItem->recache();
                pMachineItem->update();
            }
}

void UIGChooserModel::removeMachineItems(const QString &strId, UIGChooserItem *pParent)
{
    /* For each group item in passed parent: */
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Group))
        removeMachineItems(strId, pItem->toGroupItem());
    /* For each machine item in passed parent: */
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Machine))
        if (pItem->toMachineItem()->id() == strId)
            delete pItem;
}

UIGChooserItem* UIGChooserModel::findGroupItem(const QString &strName, UIGChooserItem *pParent)
{
    /* Search among all the group items of passed parent: */
    foreach (UIGChooserItem *pGroupItem, pParent->items(UIGChooserItemType_Group))
        if (pGroupItem->name() == strName)
            return pGroupItem;
    /* Recursively iterate into each the group item of the passed parent: */
    foreach (UIGChooserItem *pGroupItem, pParent->items(UIGChooserItemType_Group))
        if (UIGChooserItem *pSubGroupItem = findGroupItem(strName, pGroupItem))
            return pSubGroupItem;
    /* Nothing found? */
    return 0;
}

UIGChooserItem* UIGChooserModel::findMachineItem(const QString &strName, UIGChooserItem *pParent)
{
    /* Search among all the machine items of passed parent: */
    foreach (UIGChooserItem *pMachineItem, pParent->items(UIGChooserItemType_Machine))
        if (pMachineItem->name() == strName)
            return pMachineItem;
    /* Recursively iterate into each the group item of the passed parent: */
    foreach (UIGChooserItem *pGroupItem, pParent->items(UIGChooserItemType_Group))
        if (UIGChooserItem *pSubMachineItem = findMachineItem(strName, pGroupItem))
            return pSubMachineItem;
    /* Nothing found? */
    return 0;
}

bool UIGChooserModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    /* Whats the reason? */
    switch (pEvent->reason())
    {
        case QGraphicsSceneContextMenuEvent::Mouse:
        {
            /* First of all we should look for an item under cursor: */
            if (QGraphicsItem *pItem = itemAt(pEvent->scenePos()))
            {
                /* If this item of known type? */
                switch (pItem->type())
                {
                    case UIGChooserItemType_Group:
                    {
                        /* Get group item: */
                        UIGChooserItem *pGroupItem = qgraphicsitem_cast<UIGChooserItemGroup*>(pItem);
                        /* Is this group item only the one selected? */
                        if (selectionList().contains(pGroupItem) && selectionList().size() == 1)
                        {
                            /* Group context menu in that case: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pEvent->screenPos());
                            return true;
                        }
                        /* Is this root-group item? */
                        else if (!pGroupItem->parentItem())
                        {
                            /* Root context menu in that cases: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Root, pEvent->screenPos());
                            return true;
                        }
                    }
                    case UIGChooserItemType_Machine:
                    {
                        /* Machine context menu for other Group/Machine cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pEvent->screenPos());
                        return true;
                    }
                    default:
                        break;
                }
            }
            /* Root context menu for all the other cases: */
            popupContextMenu(UIGraphicsSelectorContextMenuType_Root, pEvent->screenPos());
            return true;
        }
        case QGraphicsSceneContextMenuEvent::Keyboard:
        {
            /* Get first selected item: */
            if (UIGChooserItem *pItem = selectionList().first())
            {
                /* If this item of known type? */
                switch (pItem->type())
                {
                    case UIGChooserItemType_Group:
                    {
                        /* Is this group item only the one selected? */
                        if (selectionList().size() == 1)
                        {
                            /* Group context menu in that case: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pEvent->screenPos());
                            return true;
                        }
                    }
                    case UIGChooserItemType_Machine:
                    {
                        /* Machine context menu for other Group/Machine cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pEvent->screenPos());
                        return true;
                    }
                    default:
                        break;
                }
            }
            /* Root context menu for all the other cases: */
            popupContextMenu(UIGraphicsSelectorContextMenuType_Root, pEvent->screenPos());
            return true;
        }
        default:
            break;
    }
    /* Pass others context menu events: */
    return false;
}

void UIGChooserModel::popupContextMenu(UIGraphicsSelectorContextMenuType type, QPoint point)
{
    /* Which type of context-menu requested? */
    switch (type)
    {
        /* For empty group? */
        case UIGraphicsSelectorContextMenuType_Root:
        {
            m_pContextMenuRoot->exec(point);
            break;
        }
        /* For group? */
        case UIGraphicsSelectorContextMenuType_Group:
        {
            m_pContextMenuGroup->exec(point);
            break;
        }
        /* For machine(s)? */
        case UIGraphicsSelectorContextMenuType_Machine:
        {
            m_pContextMenuMachine->exec(point);
            break;
        }
    }
    /* Clear status-bar: */
    emit sigClearStatusMessage();
}

void UIGChooserModel::slideRoot(bool fForward)
{
    /* Animation group: */
    QParallelAnimationGroup *pAnimation = new QParallelAnimationGroup(this);
    connect(pAnimation, SIGNAL(finished()), this, SLOT(sltSlidingComplete()), Qt::QueuedConnection);

    /* Left root animation: */
    {
        QPropertyAnimation *pLeftAnimation = new QPropertyAnimation(m_pLeftRoot, "geometry", this);
        connect(pLeftAnimation, SIGNAL(valueChanged(const QVariant&)), this, SLOT(sltLeftRootSlidingProgress()));
        QRectF startGeo = m_pLeftRoot->geometry();
        QRectF endGeo = fForward ? startGeo.translated(- startGeo.width(), 0) :
                                   startGeo.translated(startGeo.width(), 0);
        pLeftAnimation->setEasingCurve(QEasingCurve::InCubic);
        pLeftAnimation->setDuration(500);
        pLeftAnimation->setStartValue(startGeo);
        pLeftAnimation->setEndValue(endGeo);
        pAnimation->addAnimation(pLeftAnimation);
    }

    /* Right root animation: */
    {
        QPropertyAnimation *pRightAnimation = new QPropertyAnimation(m_pRightRoot, "geometry", this);
        connect(pRightAnimation, SIGNAL(valueChanged(const QVariant&)), this, SLOT(sltRightRootSlidingProgress()));
        QRectF startGeo = m_pRightRoot->geometry();
        QRectF endGeo = fForward ? startGeo.translated(- startGeo.width(), 0) :
                                   startGeo.translated(startGeo.width(), 0);
        pRightAnimation->setEasingCurve(QEasingCurve::InCubic);
        pRightAnimation->setDuration(500);
        pRightAnimation->setStartValue(startGeo);
        pRightAnimation->setEndValue(endGeo);
        pAnimation->addAnimation(pRightAnimation);
    }

    /* Start animation: */
    pAnimation->start();
}

QList<UIGChooserItem*> UIGChooserModel::gatherMachineItems(const QList<UIGChooserItem*> &selectedItems) const
{
    QList<UIGChooserItem*> machineItems;
    foreach (UIGChooserItem *pItem, selectedItems)
    {
        if (pItem->type() == UIGChooserItemType_Machine)
            machineItems << pItem;
        if (pItem->type() == UIGChooserItemType_Group)
            machineItems << gatherMachineItems(pItem->items());
    }
    return machineItems;
}

void UIGChooserModel::removeMachineItems(const QStringList &names, QList<UIGChooserItem*> &selectedItems)
{
    /* Show machine items remove dialog: */
    int rc = msgCenter().confirmMachineItemRemoval(names);
    if (rc == QIMessageBox::Cancel)
        return;

    /* Remove all the required items: */
    foreach (UIGChooserItem *pItem, selectedItems)
        if (names.contains(pItem->name()))
            delete pItem;

    /* And update model: */
    updateGroupTree();
    updateNavigation();
    updateLayout();
    if (mainRoot()->hasItems())
        setCurrentItem(0);
    else
        unsetCurrentItem();
}

void UIGChooserModel::unregisterMachines(const QStringList &names)
{
    /* Populate machine list: */
    QList<CMachine> machines;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    foreach (const QString &strName, names)
    {
        CMachine machine = vbox.FindMachine(strName);
        if (!machine.isNull())
            machines << machine;
    }

    /* Show machine remove dialog: */
    int rc = msgCenter().confirmMachineDeletion(machines);
    if (rc != QIMessageBox::Cancel)
    {
        /* For every selected item: */
        foreach (CMachine machine, machines)
        {
            if (rc == QIMessageBox::Yes)
            {
                /* Unregister and cleanup machine's data & hard-disks: */
                CMediumVector mediums = machine.Unregister(KCleanupMode_DetachAllReturnHardDisksOnly);
                if (machine.isOk())
                {
                    /* Delete machine hard-disks: */
                    CProgress progress = machine.Delete(mediums);
                    if (machine.isOk())
                    {
                        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_delete_90px.png", 0, true);
                        if (progress.GetResultCode() != 0)
                            msgCenter().cannotDeleteMachine(machine, progress);
                    }
                }
                if (!machine.isOk())
                    msgCenter().cannotDeleteMachine(machine);
            }
            else
            {
                /* Just unregister machine: */
                machine.Unregister(KCleanupMode_DetachAllReturnNone);
                if (!machine.isOk())
                    msgCenter().cannotDeleteMachine(machine);
            }
        }
    }
}

void UIGChooserModel::sortItems(UIGChooserItem *pParent, bool fRecursively /* = false */)
{
    /* Sort group items: */
    QMap<QString, UIGChooserItem*> sorter;
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Group))
    {
        sorter.insert(pItem->name().toLower(), pItem);
        if (fRecursively)
            sortItems(pItem, fRecursively);
    }
    pParent->setItems(sorter.values(), UIGChooserItemType_Group);

    /* Sort machine items: */
    sorter.clear();
    foreach (UIGChooserItem *pItem, pParent->items(UIGChooserItemType_Machine))
        sorter.insert(pItem->name().toLower(), pItem);
    pParent->setItems(sorter.values(), UIGChooserItemType_Machine);

    /* Update model: */
    updateNavigation();
    updateLayout();
}

