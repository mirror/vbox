/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorModel class implementation
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
#include <QGraphicsWidget>
#include <QGraphicsView>
#include <QRegExp>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>

/* GUI includes: */
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"
#include "UIDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIActionPoolSelector.h"
#include "UIGraphicsSelectorMouseHandler.h"
#include "UIGraphicsSelectorKeyboardHandler.h"

/* COM includes: */
#include "CMachine.h"

UIGraphicsSelectorModel::UIGraphicsSelectorModel(QObject *pParent)
    : QObject(pParent)
    , m_pScene(0)
    , m_pRoot(0)
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

UIGraphicsSelectorModel::~UIGraphicsSelectorModel()
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

void UIGraphicsSelectorModel::load()
{
    /* Prepare group-tree: */
    prepareGroupTree();
}

void UIGraphicsSelectorModel::save()
{
    /* Cleanup group-tree: */
    cleanupGroupTree();
}

QGraphicsScene* UIGraphicsSelectorModel::scene() const
{
    return m_pScene;
}

void UIGraphicsSelectorModel::setCurrentItem(int iItemIndex)
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

void UIGraphicsSelectorModel::setCurrentItem(UIGSelectorItem *pItem)
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

void UIGraphicsSelectorModel::unsetCurrentItem()
{
    /* Clear focus/selection: */
    setFocusItem(0, true);
}

UIVMItem* UIGraphicsSelectorModel::currentItem() const
{
    /* Search for the first selected machine: */
    return searchCurrentItem(selectionList());
}

QList<UIVMItem*> UIGraphicsSelectorModel::currentItems() const
{
    /* Populate list of selected machines: */
    QList<UIVMItem*> currentItemList;
    enumerateCurrentItems(selectionList(), currentItemList);
    return currentItemList;
}

void UIGraphicsSelectorModel::setFocusItem(UIGSelectorItem *pItem, bool fWithSelection /* = false */)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Something changed? */
    if (m_pFocusItem != pItem || !pItem)
    {
        /* Remember previous focus item: */
        QPointer<UIGSelectorItem> pPreviousFocusItem = m_pFocusItem;
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

UIGSelectorItem* UIGraphicsSelectorModel::focusItem() const
{
    return m_pFocusItem;
}

QGraphicsItem* UIGraphicsSelectorModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene()->itemAt(position, deviceTransform);
}

void UIGraphicsSelectorModel::updateGroupTree()
{
    updateGroupTree(m_pRoot);
}

const QList<UIGSelectorItem*>& UIGraphicsSelectorModel::navigationList() const
{
    return m_navigationList;
}

void UIGraphicsSelectorModel::removeFromNavigationList(UIGSelectorItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_navigationList.removeAll(pItem);
}

void UIGraphicsSelectorModel::clearNavigationList()
{
    m_navigationList.clear();
}

void UIGraphicsSelectorModel::updateNavigation()
{
    /* Recreate navigation list: */
    clearNavigationList();
    m_navigationList = createNavigationList(m_pRoot);
}

const QList<UIGSelectorItem*>& UIGraphicsSelectorModel::selectionList() const
{
    return m_selectionList;
}

void UIGraphicsSelectorModel::addToSelectionList(UIGSelectorItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_selectionList << pItem;
    pItem->update();
}

void UIGraphicsSelectorModel::removeFromSelectionList(UIGSelectorItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_selectionList.removeAll(pItem);
    pItem->update();
}

void UIGraphicsSelectorModel::clearSelectionList()
{
    QList<UIGSelectorItem*> oldSelectedList = m_selectionList;
    m_selectionList.clear();
    foreach (UIGSelectorItem *pItem, oldSelectedList)
        pItem->update();
}

void UIGraphicsSelectorModel::notifySelectionChanged()
{
    /* Make sure selection item list is never empty
     * if at least one item (for example 'focus') present: */
    if (selectionList().isEmpty() && focusItem())
        addToSelectionList(focusItem());
    /* Notify listeners about selection change: */
    emit sigSelectionChanged();
}

void UIGraphicsSelectorModel::updateLayout()
{
    /* Initialize variables: */
    int iSceneMargin = data(SelectorModelData_Margin).toInt();
    QSize viewportSize = scene()->views()[0]->viewport()->size();
    int iViewportWidth = viewportSize.width() - 2 * iSceneMargin;
    int iViewportHeight = viewportSize.height() - 2 * iSceneMargin;
    /* Update all the size-hints recursively: */
    m_pRoot->updateSizeHint();
    /* Set root item position: */
    m_pRoot->setPos(iSceneMargin, iSceneMargin);
    /* Set root item size: */
    m_pRoot->resize(iViewportWidth, iViewportHeight);
    /* Relayout root item: */
    m_pRoot->updateLayout();
    /* Notify listener about root-item relayouted: */
    emit sigRootItemResized(m_pRoot->geometry().size(), m_pRoot->minimumWidthHint());
}

void UIGraphicsSelectorModel::setCurrentDragObject(QDrag *pDragObject)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Remember new drag-object: */
    m_pCurrentDragObject = pDragObject;
    connect(m_pCurrentDragObject, SIGNAL(destroyed(QObject*)), this, SLOT(sltCurrentDragObjectDestroyed()));
}

void UIGraphicsSelectorModel::activate()
{
    gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow)->activate(QAction::Trigger);
}

void UIGraphicsSelectorModel::sltMachineStateChanged(QString strId, KMachineState)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, m_pRoot);
}

void UIGraphicsSelectorModel::sltMachineDataChanged(QString strId)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, m_pRoot);
}

void UIGraphicsSelectorModel::sltMachineRegistered(QString strId, bool fRegistered)
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
            addMachineIntoTheTree(machine);
            /* And update model: */
            updateNavigation();
            updateLayout();
            setCurrentItem(getMachineItem(strId, m_pRoot));
        }
    }
    /* Existing VM unregistered? */
    else
    {
        /* Remove machine items with passed id: */
        removeMachineItems(strId, m_pRoot);
        /* And update model: */
        updateGroupTree();
        updateNavigation();
        updateLayout();
        if (m_pRoot->hasItems())
            setCurrentItem(0);
        else
            unsetCurrentItem();
    }
}

void UIGraphicsSelectorModel::sltSessionStateChanged(QString strId, KSessionState)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, m_pRoot);
}

void UIGraphicsSelectorModel::sltSnapshotChanged(QString strId, QString)
{
    /* Update machine items with passed id: */
    updateMachineItems(strId, m_pRoot);
}

void UIGraphicsSelectorModel::sltHandleViewResized()
{
    /* Relayout: */
    updateLayout();
}

void UIGraphicsSelectorModel::sltCurrentDragObjectDestroyed()
{
    /* Reset drag tokens starting from the root item: */
    m_pRoot->resetDragToken();
}

void UIGraphicsSelectorModel::sltRemoveCurrentlySelectedGroup()
{
    /* Check which item is focused now: */
    if (focusItem()->type() == UIGSelectorItemType_Group)
    {
        /* Delete that item: */
        delete focusItem();
        /* And update model: */
        updateGroupTree();
        updateNavigation();
        updateLayout();
        if (m_pRoot->hasItems())
            setCurrentItem(0);
        else
            unsetCurrentItem();
    }
}

void UIGraphicsSelectorModel::sltActionHovered(QAction *pAction)
{
    emit sigShowStatusMessage(pAction->statusTip());
}

void UIGraphicsSelectorModel::sltFocusItemDestroyed()
{
    AssertMsgFailed(("Focus item destroyed!"));
}

QVariant UIGraphicsSelectorModel::data(int iKey) const
{
    switch (iKey)
    {
        case SelectorModelData_Margin: return 0;
        default: break;
    }
    return QVariant();
}

void UIGraphicsSelectorModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    m_pScene->installEventFilter(this);
}

void UIGraphicsSelectorModel::prepareRoot()
{
    m_pRoot = new UIGSelectorItemGroup;
    scene()->addItem(m_pRoot);
}

void UIGraphicsSelectorModel::prepareContextMenu()
{
    /* Context menu for empty group: */
    m_pContextMenuRoot = new QMenu;
    m_pContextMenuRoot->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard));
    m_pContextMenuRoot->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog));

    /* Context menu for group: */
    m_pContextMenuGroup = new QMenu;
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveGroupDialog));
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset));
    m_pContextMenuGroup->addMenu(gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu());
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndex_Simple_LogDialog));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh));
    m_pContextMenuGroup->addSeparator();
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager));
    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut));
//    m_pContextMenuGroup->addSeparator();
//    m_pContextMenuGroup->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort));

    /* Context menu for machine(s): */
    m_pContextMenuMachine = new QMenu;
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog));
    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard));
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
//    m_pContextMenuMachine->addSeparator();
//    m_pContextMenuMachine->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort));

    connect(m_pContextMenuRoot, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));
    connect(m_pContextMenuGroup, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));
    connect(m_pContextMenuMachine, SIGNAL(hovered(QAction*)), this, SLOT(sltActionHovered(QAction*)));
}

void UIGraphicsSelectorModel::prepareHandlers()
{
    m_pMouseHandler = new UIGraphicsSelectorMouseHandler(this);
    m_pKeyboardHandler = new UIGraphicsSelectorKeyboardHandler(this);
}

void UIGraphicsSelectorModel::prepareGroupTree()
{
    /* Load group tree: */
    loadGroupTree();
    /* Load order: */
    loadGroupsOrder();

    /* Update model: */
    updateNavigation();
    updateLayout();
    if (m_pRoot->hasItems())
        setCurrentItem(0);
    else
        unsetCurrentItem();
}

void UIGraphicsSelectorModel::cleanupGroupTree()
{
    /* Save group tree: */
    saveGroupTree();
    /* Save order: */
    saveGroupsOrder();
}

void UIGraphicsSelectorModel::cleanupHandlers()
{
    delete m_pKeyboardHandler;
    m_pKeyboardHandler = 0;
    delete m_pMouseHandler;
    m_pMouseHandler = 0;
}

void UIGraphicsSelectorModel::cleanupContextMenu()
{
    delete m_pContextMenuRoot;
    m_pContextMenuRoot = 0;
    delete m_pContextMenuGroup;
    m_pContextMenuGroup = 0;
    delete m_pContextMenuMachine;
    m_pContextMenuMachine = 0;
}

void UIGraphicsSelectorModel::cleanupRoot()
{
    delete m_pRoot;
    m_pRoot = 0;
}

void UIGraphicsSelectorModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

bool UIGraphicsSelectorModel::eventFilter(QObject *pWatched, QEvent *pEvent)
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

void UIGraphicsSelectorModel::clearRealFocus()
{
    /* Set real focus to null: */
    scene()->setFocusItem(0);
}

UIVMItem* UIGraphicsSelectorModel::searchCurrentItem(const QList<UIGSelectorItem*> &list) const
{
    /* Iterate over all the passed items: */
    foreach (UIGSelectorItem *pItem, list)
    {
        /* If item is machine, just return it: */
        if (pItem->type() == UIGSelectorItemType_Machine)
        {
            if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
                return pMachineItem;
        }
        /* If item is group: */
        else if (pItem->type() == UIGSelectorItemType_Group)
        {
            /* If it have at least one machine item: */
            if (pItem->hasItems(UIGSelectorItemType_Machine))
                /* Iterate over all the machine items recursively: */
                return searchCurrentItem(pItem->items(UIGSelectorItemType_Machine));
            /* If it have at least one group item: */
            else if (pItem->hasItems(UIGSelectorItemType_Group))
                /* Iterate over all the group items recursively: */
                return searchCurrentItem(pItem->items(UIGSelectorItemType_Group));
        }
    }
    return 0;
}

void UIGraphicsSelectorModel::enumerateCurrentItems(const QList<UIGSelectorItem*> &il, QList<UIVMItem*> &ol) const
{
    /* Enumerate all the passed items: */
    foreach (UIGSelectorItem *pItem, il)
    {
        /* If item is machine, add if missed: */
        if (pItem->type() == UIGSelectorItemType_Machine)
        {
            if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
                if (!contains(ol, pMachineItem))
                    ol << pMachineItem;
        }
        /* If item is group: */
        else if (pItem->type() == UIGSelectorItemType_Group)
        {
            /* Enumerate all the machine items recursively: */
            enumerateCurrentItems(pItem->items(UIGSelectorItemType_Machine), ol);
            /* Enumerate all the group items recursively: */
            enumerateCurrentItems(pItem->items(UIGSelectorItemType_Group), ol);
        }
    }
}

bool UIGraphicsSelectorModel::contains(const QList<UIVMItem*> &list, UIVMItem *pItem) const
{
    /* Check if passed list contains passed item: */
    foreach (UIVMItem *pIteratedItem, list)
        if (pIteratedItem->id() == pItem->id())
            return true;
    return false;
}

void UIGraphicsSelectorModel::loadGroupTree()
{
    /* Add all the machines we have into the group-tree: */
    foreach (const CMachine &machine, vboxGlobal().virtualBox().GetMachines())
        addMachineIntoTheTree(machine);
}

void UIGraphicsSelectorModel::loadGroupsOrder()
{
    /* Load order starting form the root-item: */
    loadGroupsOrder(m_pRoot);
}

void UIGraphicsSelectorModel::loadGroupsOrder(UIGSelectorItem *pParentItem)
{
    /* Prepare extra-data key for current group: */
    QString strExtraDataKey = UIDefs::GUI_GroupDefinitions + fullName(pParentItem);
    /* Read groups order: */
    QStringList order = vboxGlobal().virtualBox().GetExtraDataStringList(strExtraDataKey);
    /* Current groups list: */
    const QList<UIGSelectorItem*> groupItems = pParentItem->items(UIGSelectorItemType_Group);
    /* Current machine list: */
    const QList<UIGSelectorItem*> machineItems = pParentItem->items(UIGSelectorItemType_Machine);
    /* Iterate order list in backward direction: */
    for (int i = order.size() - 1; i >= 0; --i)
    {
        /* Get current element: */
        QString strOrderElement = order.at(i);
        /* Get corresponding list of items: */
        QList<UIGSelectorItem*> items;
        QRegExp groupDescriptorRegExp("g(\\S)*=");
        QRegExp machineDescriptorRegExp("m=");
        if (groupDescriptorRegExp.indexIn(strOrderElement) == 0)
            items = groupItems;
        else if (machineDescriptorRegExp.indexIn(strOrderElement) == 0)
            items = machineItems;
        if (items.isEmpty())
            continue;
        /* Get element name: */
        QString strElementName = strOrderElement.section('=', 1, -1);
        /* Search for corresponding item: */
        for (int i = 0; i < items.size(); ++i)
        {
            /* Get current item: */
            UIGSelectorItem *pItem = items[i];
            /* If item found: */
            if (pItem->name() == strElementName)
            {
                /* Move it to the start of list: */
                pParentItem->moveItemTo(pItem, 0);
                /* If item is group: */
                if (pItem->type() == UIGSelectorItemType_Group)
                {
                    /* Check descriptors we have: */
                    QString strDescriptor(groupDescriptorRegExp.cap(1));
                    if (strDescriptor.contains('o'))
                        QTimer::singleShot(0, pItem, SLOT(sltOpen()));
                }
                break;
            }
        }
    }
    /* Update groups order for the sub-groups: */
    foreach (UIGSelectorItem *pItem, pParentItem->items(UIGSelectorItemType_Group))
        loadGroupsOrder(pItem);
}

void UIGraphicsSelectorModel::addMachineIntoTheTree(const CMachine &machine)
{
    /* Which groups current machine attached to? */
    QVector<QString> groups = machine.GetGroups();
    foreach (QString strGroup, groups)
    {
        /* Remove last '/' if any: */
        if (strGroup.right(1) == "/")
            strGroup.remove(strGroup.size() - 1, 1);
        /* Search for the group item, create machine item: */
        createMachineItem(machine, getGroupItem(strGroup, m_pRoot));
    }
    /* Update group definitions: */
    m_groups[machine.GetId()] = UIStringSet::fromList(groups.toList());
}

UIGSelectorItem* UIGraphicsSelectorModel::getGroupItem(const QString &strName, UIGSelectorItem *pParent)
{
    /* Check passed stuff: */
    if (pParent->name() == strName)
        return pParent;

    /* Prepare variables: */
    QString strFirstSubName = strName.section('/', 0, 0);
    QString strFirstSuffix = strName.section('/', 1, -1);
    QString strSecondSubName = strFirstSuffix.section('/', 0, 0);
    QString strSecondSuffix = strFirstSuffix.section('/', 1, -1);

    /* Passed group name equal to first sub-name: */
    if (pParent->name() == strFirstSubName)
    {
        /* Make sure first-suffix is NOT empty: */
        AssertMsg(!strFirstSuffix.isEmpty(), ("Invalid group name!"));
        /* Trying to get group item among our children: */
        foreach (UIGSelectorItem *pGroupItem, pParent->items(UIGSelectorItemType_Group))
            if (pGroupItem->name() == strSecondSubName)
                return getGroupItem(strFirstSuffix, pGroupItem);
    }

    /* Found nothing? Creating: */
    UIGSelectorItemGroup *pNewGroupItem = new UIGSelectorItemGroup(pParent, strSecondSubName);
    return strSecondSuffix.isEmpty() ? pNewGroupItem : getGroupItem(strFirstSuffix, pNewGroupItem);
}

void UIGraphicsSelectorModel::createMachineItem(const CMachine &machine, UIGSelectorItem *pWhere)
{
    /* Create corresponding item: */
    new UIGSelectorItemMachine(pWhere, machine);
}

void UIGraphicsSelectorModel::saveGroupTree()
{
    /* Prepare machine map: */
    QMap<QString, QStringList> groups;
    /* Iterate over all the items: */
    gatherGroupTree(groups, m_pRoot);
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
            printf(" Saving groups for machine {%s}: {%s}\n",
                   machine.GetName().toAscii().constData(),
                   newGroupList.join(",").toAscii().constData());
            machine.SetGroups(newGroupList.toVector());
            machine.SaveSettings();
            if (!machine.isOk())
                msgCenter().cannotSaveMachineSettings(machine);
            /* Close the session: */
            session.UnlockMachine();
        }
    }
}

void UIGraphicsSelectorModel::saveGroupsOrder()
{
    /* Clear all the extra-data records related to group-definitions: */
    const QVector<QString> extraDataKeys = vboxGlobal().virtualBox().GetExtraDataKeys();
    foreach (const QString &strKey, extraDataKeys)
        if (strKey.startsWith(UIDefs::GUI_GroupDefinitions))
            vboxGlobal().virtualBox().SetExtraData(strKey, QString());
    /* Save order starting from the root-item: */
    saveGroupsOrder(m_pRoot);
}

void UIGraphicsSelectorModel::saveGroupsOrder(UIGSelectorItem *pParentItem)
{
    /* Prepare extra-data key for current group: */
    QString strExtraDataKey = UIDefs::GUI_GroupDefinitions + fullName(pParentItem);
    /* Gather item order: */
    QStringList order;
    foreach (UIGSelectorItem *pItem, pParentItem->items(UIGSelectorItemType_Group))
    {
        saveGroupsOrder(pItem);
        QString strGroupDescriptor(pItem->toGroupItem()->opened() ? "go" : "gc");
        order << QString("%1=%2").arg(strGroupDescriptor, pItem->name());
    }
    foreach (UIGSelectorItem *pItem, pParentItem->items(UIGSelectorItemType_Machine))
        order << QString("m=%1").arg(pItem->name());
    vboxGlobal().virtualBox().SetExtraDataStringList(strExtraDataKey, order);
}

void UIGraphicsSelectorModel::gatherGroupTree(QMap<QString, QStringList> &groups,
                                              UIGSelectorItem *pParentGroup)
{
    /* Iterate over all the machine items: */
    foreach (UIGSelectorItem *pItem, pParentGroup->items(UIGSelectorItemType_Machine))
        if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
            groups[pMachineItem->id()] << fullName(pParentGroup);
    /* Iterate over all the group items: */
    foreach (UIGSelectorItem *pItem, pParentGroup->items(UIGSelectorItemType_Group))
        gatherGroupTree(groups, pItem);
}

QString UIGraphicsSelectorModel::fullName(UIGSelectorItem *pItem)
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

void UIGraphicsSelectorModel::updateGroupTree(UIGSelectorItem *pGroupItem)
{
    /* Cleanup all the group items first: */
    foreach (UIGSelectorItem *pSubGroupItem, pGroupItem->items(UIGSelectorItemType_Group))
        updateGroupTree(pSubGroupItem);
    /* Cleanup only non-root items: */
    if (pGroupItem->parentItem() && !pGroupItem->hasItems())
        delete pGroupItem;
}

QList<UIGSelectorItem*> UIGraphicsSelectorModel::createNavigationList(UIGSelectorItem *pItem)
{
    /* Prepare navigation list: */
    QList<UIGSelectorItem*> navigationItems;

    /* Iterate over all the group items: */
    foreach (UIGSelectorItem *pGroupItem, pItem->items(UIGSelectorItemType_Group))
    {
        navigationItems << pGroupItem;
        if (pGroupItem->toGroupItem()->opened())
            navigationItems << createNavigationList(pGroupItem);
    }
    /* Iterate over all the machine items: */
    foreach (UIGSelectorItem *pMachineItem, pItem->items(UIGSelectorItemType_Machine))
        navigationItems << pMachineItem;

    /* Return navigation list: */
    return navigationItems;
}

void UIGraphicsSelectorModel::updateMachineItems(const QString &strId, UIGSelectorItem *pParent)
{
    /* For each group item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        updateMachineItems(strId, pItem->toGroupItem());
    /* For each machine item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
            if (pMachineItem->id() == strId)
            {
                pMachineItem->recache();
                pMachineItem->update();
            }
}

void UIGraphicsSelectorModel::removeMachineItems(const QString &strId, UIGSelectorItem *pParent)
{
    /* For each group item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        removeMachineItems(strId, pItem->toGroupItem());
    /* For each machine item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        if (pItem->toMachineItem()->id() == strId)
            delete pItem;
}

UIGSelectorItem* UIGraphicsSelectorModel::getMachineItem(const QString &strId, UIGSelectorItem *pParent)
{
    /* Search among all the machine items of passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
            if (pMachineItem->id() == strId)
                return pMachineItem;
    /* Search among all the group items of passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        if (UIGSelectorItem *pMachineItem = getMachineItem(strId, pItem))
            return pMachineItem;
    /* Nothing found? */
    return 0;
}

bool UIGraphicsSelectorModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
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
                    case UIGSelectorItemType_Group:
                    {
                        /* Get group item: */
                        UIGSelectorItem *pGroupItem = qgraphicsitem_cast<UIGSelectorItemGroup*>(pItem);
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
                    case UIGSelectorItemType_Machine:
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
            if (UIGSelectorItem *pItem = selectionList().first())
            {
                /* If this item of known type? */
                switch (pItem->type())
                {
                    case UIGSelectorItemType_Group:
                    {
                        /* Is this group item only the one selected? */
                        if (selectionList().size() == 1)
                        {
                            /* Group context menu in that case: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pEvent->screenPos());
                            return true;
                        }
                    }
                    case UIGSelectorItemType_Machine:
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

void UIGraphicsSelectorModel::popupContextMenu(UIGraphicsSelectorContextMenuType type, QPoint point)
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

