/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserModel class implementation.
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

/* Qt includes: */
#include <QDrag>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QScrollBar>
#include <QTimer>

/* GUI includes: */
#include "QIMessageBox.h"
#include "UICommon.h"
#include "UIActionPoolManager.h"
#include "UIChooser.h"
#include "UIChooserHandlerMouse.h"
#include "UIChooserHandlerKeyboard.h"
#include "UIChooserItemGroup.h"
#include "UIChooserItemGlobal.h"
#include "UIChooserItemMachine.h"
#include "UIChooserModel.h"
#include "UIChooserNode.h"
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeGlobal.h"
#include "UIChooserNodeMachine.h"
#include "UIChooserView.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UITask.h"
#include "UIVirtualBoxManagerWidget.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"

/* Type defs: */
typedef QSet<QString> UIStringSet;


UIChooserModel:: UIChooserModel(UIChooser *pParent)
    : UIChooserAbstractModel(pParent)
    , m_pChooser(pParent)
    , m_pScene(0)
    , m_pMouseHandler(0)
    , m_pKeyboardHandler(0)
    , m_pContextMenuGlobal(0)
    , m_pContextMenuGroup(0)
    , m_pContextMenuMachine(0)
    , m_iCurrentSearchResultIndex(-1)
    , m_iScrollingTokenSize(30)
    , m_fIsScrollingInProgress(false)
{
    prepare();
}

UIChooserModel::~UIChooserModel()
{
    cleanup();
}

void UIChooserModel::init()
{
    /* Call to base-class: */
    UIChooserAbstractModel::init();

    /* Build tree for main root: */
    buildTreeForMainRoot();
    updateNavigationItemList();
    updateLayout();

    /* Load last selected-item: */
    loadLastSelectedItem();
}

void UIChooserModel::deinit()
{
    /* Save last selected-item: */
    saveLastSelectedItem();

    /* Clear list of selected-items: */
    clearSelectedItems();

    /* Call to base-class: */
    UIChooserAbstractModel::deinit();
}

UIChooser *UIChooserModel::chooser() const
{
    return m_pChooser;
}

UIActionPool *UIChooserModel::actionPool() const
{
    return chooser() ? chooser()->actionPool() : 0;
}

QGraphicsScene *UIChooserModel::scene() const
{
    return m_pScene;
}

UIChooserView *UIChooserModel::view() const
{
    return scene() && !scene()->views().isEmpty() ? qobject_cast<UIChooserView*>(scene()->views().first()) : 0;
}

QPaintDevice *UIChooserModel::paintDevice() const
{
    return scene() && !scene()->views().isEmpty() ? scene()->views().first() : 0;
}

QGraphicsItem *UIChooserModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene() ? scene()->itemAt(position, deviceTransform) : 0;
}

void UIChooserModel::handleToolButtonClick(UIChooserItem *pItem)
{
    switch (pItem->type())
    {
        case UIChooserNodeType_Global:
            emit sigToolMenuRequested(UIToolClass_Global, pItem->mapToScene(QPointF(pItem->size().width(), 0)).toPoint());
            break;
        case UIChooserNodeType_Machine:
            emit sigToolMenuRequested(UIToolClass_Machine, pItem->mapToScene(QPointF(pItem->size().width(), 0)).toPoint());
            break;
        default:
            break;
    }
}

void UIChooserModel::handlePinButtonClick(UIChooserItem *pItem)
{
    switch (pItem->type())
    {
        case UIChooserNodeType_Global:
            pItem->setFavorite(!pItem->isFavorite());
            break;
        default:
            break;
    }
}

void UIChooserModel::setSelectedItems(const QList<UIChooserItem*> &items)
{
    /* Is there something changed? */
    if (m_selectedItems == items)
        return;

    /* Remember old selected-item list: */
    const QList<UIChooserItem*> oldCurrentItems = m_selectedItems;

    /* Clear current selected-item list: */
    m_selectedItems.clear();

    /* Iterate over all the passed items: */
    foreach (UIChooserItem *pItem, items)
    {
        /* Add item to current selected-item list if navigation list contains it: */
        if (pItem && navigationItems().contains(pItem))
            m_selectedItems << pItem;
        else
            AssertMsgFailed(("Passed item is not in navigation list!"));
    }

    /* Is there something really changed? */
    if (oldCurrentItems == m_selectedItems)
        return;

    /* Update all the old items (they are no longer selected): */
    foreach (UIChooserItem *pItem, oldCurrentItems)
        pItem->update();
    /* Update all the new items (they are selected now): */
    foreach (UIChooserItem *pItem, m_selectedItems)
        pItem->update();

    /* Notify about selection changes: */
    emit sigSelectionChanged();
}

void UIChooserModel::setSelectedItem(UIChooserItem *pItem)
{
    /* Call for wrapper above: */
    QList<UIChooserItem*> items;
    if (pItem)
        items << pItem;
    setSelectedItems(items);

    /* Make selected-item current one as well: */
    setCurrentItem(firstSelectedItem());
}

void UIChooserModel::setSelectedItem(const QString &strDefinition)
{
    /* Ignore if empty definition passed: */
    if (strDefinition.isEmpty())
        return;

    /* Parse definition: */
    UIChooserItem *pItem = 0;
    const QString strItemType = strDefinition.section('=', 0, 0);
    const QString strItemDescriptor = strDefinition.section('=', 1, -1);
    /* Its a group-item definition? */
    if (strItemType == "g")
    {
        /* Search for group-item with passed descriptor (name): */
        pItem = root()->searchForItem(strItemDescriptor,
                                      UIChooserItemSearchFlag_Group |
                                      UIChooserItemSearchFlag_ExactName);
    }
    /* Its a global-item definition? */
    else if (strItemType == "n")
    {
        /* Search for global-item with required name: */
        pItem = root()->searchForItem(strItemDescriptor,
                                      UIChooserItemSearchFlag_Global |
                                      UIChooserItemSearchFlag_ExactName);
    }
    /* Its a machine-item definition? */
    else if (strItemType == "m")
    {
        /* Check if machine-item with passed descriptor (name or id) registered: */
        CMachine comMachine = uiCommon().virtualBox().FindMachine(strItemDescriptor);
        if (!comMachine.isNull())
        {
            /* Search for machine-item with required name: */
            pItem = root()->searchForItem(comMachine.GetName(),
                                          UIChooserItemSearchFlag_Machine |
                                          UIChooserItemSearchFlag_ExactName);
        }
    }

    /* Make sure found item is in navigation list: */
    if (!pItem || !navigationItems().contains(pItem))
        return;

    /* Call for wrapper above: */
    setSelectedItem(pItem);
}

void UIChooserModel::clearSelectedItems()
{
    /* Call for wrapper above: */
    setSelectedItem(0);
}

void UIChooserModel::addToSelectedItems(UIChooserItem *pItem)
{
    /* Call for wrapper above: */
    setSelectedItems(QList<UIChooserItem*>(m_selectedItems) << pItem);
}

void UIChooserModel::removeFromSelectedItems(UIChooserItem *pItem)
{
    /* Prepare filtered list: */
    QList<UIChooserItem*> list(m_selectedItems);
    list.removeAll(pItem);
    /* Call for wrapper above: */
    setSelectedItems(list);
}

UIChooserItem *UIChooserModel::firstSelectedItem() const
{
    /* Return first of selected-items, if any: */
    return selectedItems().isEmpty() ? 0 : selectedItems().first();
}

const QList<UIChooserItem*> &UIChooserModel::selectedItems() const
{
    return m_selectedItems;
}

UIVirtualMachineItem *UIChooserModel::firstSelectedMachineItem() const
{
    /* Return first machine-item of the selected-item: */
    return      firstSelectedItem()
             && firstSelectedItem()->firstMachineItem()
             && firstSelectedItem()->firstMachineItem()->node()
             && firstSelectedItem()->firstMachineItem()->node()->toMachineNode()
           ? firstSelectedItem()->firstMachineItem()->node()->toMachineNode()->cache()
           : 0;
}

QList<UIVirtualMachineItem*> UIChooserModel::selectedMachineItems() const
{
    /* Gather list of selected unique machine-items: */
    QList<UIChooserItemMachine*> currentMachineItemList;
    UIChooserItemMachine::enumerateMachineItems(selectedItems(), currentMachineItemList,
                                                UIChooserItemMachineEnumerationFlag_Unique);

    /* Reintegrate machine-items into valid format: */
    QList<UIVirtualMachineItem*> currentMachineList;
    foreach (UIChooserItemMachine *pItem, currentMachineItemList)
        currentMachineList << pItem->node()->toMachineNode()->cache();
    return currentMachineList;
}

bool UIChooserModel::isGroupItemSelected() const
{
    return firstSelectedItem() && firstSelectedItem()->type() == UIChooserNodeType_Group;
}

bool UIChooserModel::isGlobalItemSelected() const
{
    return firstSelectedItem() && firstSelectedItem()->type() == UIChooserNodeType_Global;
}

bool UIChooserModel::isMachineItemSelected() const
{
    return firstSelectedItem() && firstSelectedItem()->type() == UIChooserNodeType_Machine;
}

bool UIChooserModel::isSingleGroupSelected() const
{
    return    selectedItems().size() == 1
           && firstSelectedItem()->type() == UIChooserNodeType_Group;
}

bool UIChooserModel::isSingleLocalGroupSelected() const
{
    return    isSingleGroupSelected()
           && firstSelectedItem()->node()->toGroupNode()->groupType() == UIChooserNodeGroupType_Local;
}

bool UIChooserModel::isSingleCloudProfileGroupSelected() const
{
    return    isSingleGroupSelected()
           && firstSelectedItem()->node()->toGroupNode()->groupType() == UIChooserNodeGroupType_Profile;
}

bool UIChooserModel::isAllItemsOfOneGroupSelected() const
{
    /* Make sure at least one item selected: */
    if (selectedItems().isEmpty())
        return false;

    /* Determine the parent group of the first item: */
    UIChooserItem *pFirstParent = firstSelectedItem()->parentItem();

    /* Make sure this parent is not main root-item: */
    if (pFirstParent == root())
        return false;

    /* Enumerate selected-item set: */
    QSet<UIChooserItem*> currentItemSet;
    foreach (UIChooserItem *pCurrentItem, selectedItems())
        currentItemSet << pCurrentItem;

    /* Enumerate first parent children set: */
    QSet<UIChooserItem*> firstParentItemSet;
    foreach (UIChooserItem *pFirstParentItem, pFirstParent->items())
        firstParentItemSet << pFirstParentItem;

    /* Check if both sets contains the same: */
    return currentItemSet == firstParentItemSet;
}

UIChooserItem *UIChooserModel::findClosestUnselectedItem() const
{
    /* Take the current-item (if any) as a starting point
     * and find the closest non-selected-item. */
    UIChooserItem *pItem = currentItem();
    if (!pItem)
        pItem = firstSelectedItem();
    if (pItem)
    {
        int idxBefore = navigationItems().indexOf(pItem) - 1;
        int idxAfter  = idxBefore + 2;
        while (idxBefore >= 0 || idxAfter < navigationItems().size())
        {
            if (idxBefore >= 0)
            {
                pItem = navigationItems().at(idxBefore);
                if (!selectedItems().contains(pItem) && pItem->type() == UIChooserNodeType_Machine)
                    return pItem;
                --idxBefore;
            }
            if (idxAfter < navigationItems().size())
            {
                pItem = navigationItems().at(idxAfter);
                if (!selectedItems().contains(pItem) && pItem->type() == UIChooserNodeType_Machine)
                    return pItem;
                ++idxAfter;
            }
        }
    }
    return 0;
}

void UIChooserModel::makeSureSomeItemIsSelected()
{
    /* Make sure selection list is never empty if at
     * least one item (for example 'current') present: */
    if (!firstSelectedItem() && currentItem())
        setSelectedItem(currentItem());
}

void UIChooserModel::setCurrentItem(UIChooserItem *pItem)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Is there something changed? */
    if (m_pCurrentItem == pItem)
        return;

    /* Remember old current-item: */
    UIChooserItem *pOldCurrentItem = m_pCurrentItem;

    /* Set new current-item: */
    m_pCurrentItem = pItem;

    /* Disconnect old current-item (if any): */
    if (pOldCurrentItem)
        disconnect(pOldCurrentItem, &UIChooserItem::destroyed, this, &UIChooserModel::sltCurrentItemDestroyed);
    /* Connect new current-item (if any): */
    if (m_pCurrentItem)
        connect(m_pCurrentItem.data(), &UIChooserItem::destroyed, this, &UIChooserModel::sltCurrentItemDestroyed);

    /* If dialog is visible and item exists => make it visible as well: */
    if (view() && view()->window() && root())
        if (view()->window()->isVisible() && pItem)
            root()->toGroupItem()->makeSureItemIsVisible(pItem);
}

UIChooserItem *UIChooserModel::currentItem() const
{
    return m_pCurrentItem;
}

const QList<UIChooserItem*> &UIChooserModel::navigationItems() const
{
    return m_navigationItems;
}

void UIChooserModel::removeFromNavigationItems(UIChooserItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_navigationItems.removeAll(pItem);
}

void UIChooserModel::updateNavigationItemList()
{
    m_navigationItems.clear();
    m_navigationItems = createNavigationItemList(root());
}

void UIChooserModel::performSearch(const QString &strSearchTerm, int iItemSearchFlags)
{
    /* Call to base-class: */
    UIChooserAbstractModel::performSearch(strSearchTerm, iItemSearchFlags);

    /* Select 1st found item: */
    selectSearchResult(true);
}

QList<UIChooserNode*> UIChooserModel::resetSearch()
{
    /* Reset search result index: */
    m_iCurrentSearchResultIndex = -1;

    /* Call to base-class: */
    return UIChooserAbstractModel::resetSearch();
}

void UIChooserModel::selectSearchResult(bool fIsNext)
{
    /* If nothing was found: */
    if (searchResult().isEmpty())
    {
        /* Reset search result index: */
        m_iCurrentSearchResultIndex = -1;
    }
    /* If something was found: */
    else
    {
        /* Advance index forward: */
        if (fIsNext)
        {
            if (++m_iCurrentSearchResultIndex >= searchResult().size())
                m_iCurrentSearchResultIndex = 0;
        }
        /* Advance index backward: */
        else
        {
            if (--m_iCurrentSearchResultIndex < 0)
                m_iCurrentSearchResultIndex = searchResult().size() - 1;
        }

        /* If found item exists: */
        if (searchResult().at(m_iCurrentSearchResultIndex))
        {
            /* Select curresponding found item, make sure it's visible, scroll if necessary: */
            UIChooserItem *pItem = searchResult().at(m_iCurrentSearchResultIndex)->item();
            if (pItem)
            {
                pItem->makeSureItsVisible();
                setSelectedItem(pItem);
            }
        }
    }

    /* Update the search widget's match count(s): */
    if (view())
        view()->setSearchResultsCount(searchResult().size(), m_iCurrentSearchResultIndex);
}

void UIChooserModel::setSearchWidgetVisible(bool fVisible)
{
    if (view())
        view()->setSearchWidgetVisible(fVisible);
}

UIChooserItem *UIChooserModel::root() const
{
    return m_pRoot.data();
}

void UIChooserModel::startEditingGroupItemName()
{
    sltEditGroupName();
}

void UIChooserModel::activateMachineItem()
{
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->activate(QAction::Trigger);
}

void UIChooserModel::setCurrentDragObject(QDrag *pDragObject)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Remember new drag-object: */
    m_pCurrentDragObject = pDragObject;
    connect(m_pCurrentDragObject.data(), &QDrag::destroyed,
            this, &UIChooserModel::sltCurrentDragObjectDestroyed);
}

void UIChooserModel::lookFor(const QString &strLookupText)
{
    if (view())
    {
        view()->setSearchWidgetVisible(true);
        view()->appendToSearchString(strLookupText);
    }
}

void UIChooserModel::updateLayout()
{
    if (!view() || !root())
        return;

    /* Initialize variables: */
    const QSize viewportSize = view()->size();
    const int iViewportWidth = viewportSize.width();
    const int iViewportHeight = viewportSize.height();

    /* Set root-item position: */
    root()->setPos(0, 0);
    /* Set root-item size: */
    root()->resize(iViewportWidth, iViewportHeight);
    /* Relayout root-item: */
    root()->updateLayout();
    /* Make sure root-item is shown: */
    root()->show();
}

void UIChooserModel::setGlobalItemHeightHint(int iHint)
{
    /* Walk thrugh all the items of navigation list: */
    foreach (UIChooserItem *pItem, navigationItems())
    {
        /* And for each global item: */
        if (pItem->type() == UIChooserNodeType_Global)
        {
            /* Apply the height hint we have: */
            UIChooserItemGlobal *pGlobalItem = pItem->toGlobalItem();
            if (pGlobalItem)
                pGlobalItem->setHeightHint(iHint);
        }
    }
}

void UIChooserModel::sltHandleViewResized()
{
    /* Relayout: */
    updateLayout();

    /* Make current item visible asynchronously: */
    QMetaObject::invokeMethod(this, "sltMakeSureCurrentItemVisible", Qt::QueuedConnection);
}

bool UIChooserModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != scene())
        return QObject::eventFilter(pWatched, pEvent);

    /* Process only item focused by model: */
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
        /* Context-menu handler: */
        case QEvent::GraphicsSceneContextMenu:
            return processContextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent*>(pEvent));
        /* Drag&drop scroll-event (drag-move) handler: */
        case QEvent::GraphicsSceneDragMove:
            return processDragMoveEvent(static_cast<QGraphicsSceneDragDropEvent*>(pEvent));
        /* Drag&drop scroll-event (drag-leave) handler: */
        case QEvent::GraphicsSceneDragLeave:
            return processDragLeaveEvent(static_cast<QGraphicsSceneDragDropEvent*>(pEvent));
        default: break; /* Shut up MSC */
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIChooserModel::sltMachineRegistered(const QUuid &uId, const bool fRegistered)
{
    /* Call to base-class: */
    UIChooserAbstractModel::sltMachineRegistered(uId, fRegistered);

    /* Existing VM unregistered? */
    if (!fRegistered)
    {
        /* Update tree for main root: */
        updateNavigationItemList();
        updateLayout();

        /* Make sure selected-item present, if possible: */
        if (!firstSelectedItem() && !navigationItems().isEmpty())
        {
            setSelectedItem(navigationItems().first());
            emit sigSelectionInvalidated();
        }
        /* Make sure current-item present, if possible: */
        else if (!currentItem() && firstSelectedItem())
            setCurrentItem(firstSelectedItem());
        /* Notify about selected-item change: */
        emit sigSelectionChanged();
    }
    /* New VM registered? */
    else
    {
        /* Should we show this VM? */
        if (gEDataManager->showMachineInVirtualBoxManagerChooser(uId))
        {
            /* Rebuild tree for main root: */
            buildTreeForMainRoot();
            updateNavigationItemList();
            updateLayout();

            /* Select newly added item: */
            CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
            setSelectedItem(root()->searchForItem(comMachine.GetName(),
                                                 UIChooserItemSearchFlag_Machine |
                                                 UIChooserItemSearchFlag_ExactName));
        }
    }
}

void UIChooserModel::sltReloadMachine(const QUuid &uId)
{
    /* Call to base-class: */
    UIChooserAbstractModel::sltReloadMachine(uId);

    /* Show machine if we should: */
    if (gEDataManager->showMachineInVirtualBoxManagerChooser(uId))
    {
        /* Rebuild tree for main root: */
        buildTreeForMainRoot();
        updateNavigationItemList();
        updateLayout();

        /* Select newly added item: */
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        setSelectedItem(root()->searchForItem(comMachine.GetName(),
                                             UIChooserItemSearchFlag_Machine |
                                             UIChooserItemSearchFlag_ExactName));
    }
    else
    {
        /* Make sure at least one item selected after that: */
        if (!firstSelectedItem() && !navigationItems().isEmpty())
        {
            setSelectedItem(navigationItems().first());
            emit sigSelectionInvalidated();
        }
    }

    /* Notify listeners about selection change: */
    emit sigSelectionChanged();
}

#ifdef VBOX_GUI_WITH_CLOUD_VMS
void UIChooserModel::sltHandleCloudListMachinesTaskComplete(UITask *pTask)
{
    /* Skip unrelated tasks: */
    if (!pTask || pTask->type() != UITask::Type_CloudAcquireInstances)
        return;

    /* Call to base-class: */
    UIChooserAbstractModel::sltHandleCloudListMachinesTaskComplete(pTask);

    /* Remember first selected item definition: */
    const QString strDefinition = firstSelectedItem()->definition();

    /* Rebuild tree for main root: */
    buildTreeForMainRoot();
    updateNavigationItemList();
    updateLayout();

    /* Restore selection: */
    setSelectedItem(strDefinition);
}
#endif /* VBOX_GUI_WITH_CLOUD_VMS */

void UIChooserModel::sltMakeSureCurrentItemVisible()
{
    root()->toGroupItem()->makeSureItemIsVisible(currentItem());
}

void UIChooserModel::sltCurrentItemDestroyed()
{
    AssertMsgFailed(("Current-item destroyed!"));
}

void UIChooserModel::sltEditGroupName()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Group_S_Rename)->isEnabled())
        return;
    /* Only for single selected group: */
    if (!isSingleGroupSelected())
        return;

    /* Start editing group name: */
    firstSelectedItem()->startEditing();
}

void UIChooserModel::sltSortGroup()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Group_S_Sort)->isEnabled())
        return;
    /* Only for single selected group: */
    if (!isSingleGroupSelected())
        return;

    /* Sort nodes: */
    firstSelectedItem()->node()->sortNodes();

    /* Rebuild tree for main root: */
    buildTreeForMainRoot();
    updateNavigationItemList();
    updateLayout();
}

void UIChooserModel::sltUngroupSelectedGroup()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Group_S_Remove)->isEnabled())
        return;

    /* Make sure current-item is of group type! */
    AssertMsg(currentItem()->type() == UIChooserNodeType_Group, ("This is not group-item!"));

    /* Check if we have collisions with our siblings: */
    UIChooserItem *pCurrentItem = currentItem();
    UIChooserNode *pCurrentNode = pCurrentItem->node();
    UIChooserItem *pParentItem = pCurrentItem->parentItem();
    UIChooserNode *pParentNode = pParentItem->node();
    QList<UIChooserNode*> siblings = pParentNode->nodes();
    QList<UIChooserNode*> toBeRenamed;
    QList<UIChooserNode*> toBeRemoved;
    foreach (UIChooserNode *pNode, pCurrentNode->nodes())
    {
        QString strItemName = pNode->name();
        UIChooserNode *pCollisionSibling = 0;
        foreach (UIChooserNode *pSibling, siblings)
            if (pSibling != pCurrentNode && pSibling->name() == strItemName)
                pCollisionSibling = pSibling;
        if (pCollisionSibling)
        {
            if (pNode->type() == UIChooserNodeType_Machine)
            {
                if (pCollisionSibling->type() == UIChooserNodeType_Machine)
                    toBeRemoved << pNode;
                else if (pCollisionSibling->type() == UIChooserNodeType_Group)
                {
                    msgCenter().cannotResolveCollisionAutomatically(strItemName, pParentNode->name());
                    return;
                }
            }
            else if (pNode->type() == UIChooserNodeType_Group)
            {
                if (msgCenter().confirmAutomaticCollisionResolve(strItemName, pParentNode->name()))
                    toBeRenamed << pNode;
                else
                    return;
            }
        }
    }

    /* Copy all the children into our parent: */
    QList<UIChooserItem*> copiedItems;
    foreach (UIChooserNode *pNode, pCurrentNode->nodes())
    {
        if (toBeRemoved.contains(pNode))
            continue;
        switch (pNode->type())
        {
            case UIChooserNodeType_Group:
            {
                UIChooserNodeGroup *pGroupNode = new UIChooserNodeGroup(pParentNode,
                                                                        pNode->toGroupNode(),
                                                                        pParentNode->nodes().size());
                UIChooserItemGroup *pGroupItem = new UIChooserItemGroup(pParentItem, pGroupNode);
                if (toBeRenamed.contains(pNode))
                    pGroupNode->setName(uniqueGroupName(pParentNode));
                copiedItems << pGroupItem;
                break;
            }
            case UIChooserNodeType_Machine:
            {
                UIChooserNodeMachine *pMachineNode = new UIChooserNodeMachine(pParentNode,
                                                                              pNode->toMachineNode(),
                                                                              pParentNode->nodes().size());
                UIChooserItemMachine *pMachineItem = new UIChooserItemMachine(pParentItem, pMachineNode);
                copiedItems << pMachineItem;
                break;
            }
            default:
                break;
        }
    }

    /* Delete current group: */
    delete pCurrentNode;

    /* And update model: */
    updateNavigationItemList();
    updateLayout();
    if (!copiedItems.isEmpty())
    {
        setSelectedItems(copiedItems);
        setCurrentItem(firstSelectedItem());
    }
    else
    {
        setSelectedItem(navigationItems().first());
        emit sigSelectionInvalidated();
    }
    saveGroupSettings();
}

void UIChooserModel::sltCreateNewMachine()
{
    /* Check if at least one of actions is enabled: */
    if (   !actionPool()->action(UIActionIndexST_M_Welcome_S_New)->isEnabled()
        && !actionPool()->action(UIActionIndexST_M_Machine_S_New)->isEnabled()
        && !actionPool()->action(UIActionIndexST_M_Group_S_New)->isEnabled())
        return;

    /* Select the parent: */
    UIChooserItem *pGroup = 0;
    if (isSingleGroupSelected())
        pGroup = firstSelectedItem();
    else if (!selectedItems().isEmpty())
        pGroup = firstSelectedItem()->parentItem();
    QString strGroupName;
    if (pGroup)
        strGroupName = pGroup->fullName();

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_Welcome_S_New)->setEnabled(false);
    actionPool()->action(UIActionIndexST_M_Machine_S_New)->setEnabled(false);
    actionPool()->action(UIActionIndexST_M_Group_S_New)->setEnabled(false);

    /* What first item do we have? */
    if (  !firstSelectedMachineItem()
        ||firstSelectedMachineItem()->itemType() == UIVirtualMachineItem::ItemType_Local)
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pWizardParent = windowManager().realParentWindow(chooser()->managerWidget());
        UISafePointerWizardNewVM pWizard = new UIWizardNewVM(pWizardParent, strGroupName);
        windowManager().registerNewParent(pWizard, pWizardParent);
        pWizard->prepare();

        /* Execute wizard: */
        pWizard->exec();
        delete pWizard;
    }
    else
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pWizardParent = windowManager().realParentWindow(chooser()->managerWidget());
        UISafePointerWizardAddCloudVM pWizard = new UIWizardAddCloudVM(pWizardParent);
        windowManager().registerNewParent(pWizard, pWizardParent);
        pWizard->prepare();

        /* Execute wizard: */
        pWizard->exec();

        // WORKAROUND:
        // Hehey! Now we have to inject created VM nodes and then rebuild tree for the main root node ourselves
        // cause there is no corresponding event yet. Later this to be done in corresponding event handler instead.
        foreach (const CCloudMachine &comMachine, pWizard->machines())
        {
            // Create new node:
            UIChooserNodeMachine *pNode = new UIChooserNodeMachine(pGroup->node(),
                                                                   false /* favorite */,
                                                                   pGroup->node()->nodes().size() /* position */,
                                                                   comMachine);
            // Request async node update:
            pNode->cache()->toCloud()->updateInfoAsync(false /* delayed? */);
        }
        // Remember first selected item definition:
        const QString strDefinition = firstSelectedItem()->definition();
        // Rebuild tree for main root:
        buildTreeForMainRoot();
        updateNavigationItemList();
        updateLayout();
        // Restore selection:
        setSelectedItem(strDefinition);

        delete pWizard;
    }

    /* Unlock the action allowing further calls: */
    actionPool()->action(UIActionIndexST_M_Welcome_S_New)->setEnabled(true);
    actionPool()->action(UIActionIndexST_M_Machine_S_New)->setEnabled(true);
    actionPool()->action(UIActionIndexST_M_Group_S_New)->setEnabled(true);
}

void UIChooserModel::sltGroupSelectedMachines()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)->isEnabled())
        return;

    /* Create new group node in the current root: */
    UIChooserNodeGroup *pNewGroupNode = new UIChooserNodeGroup(invisibleRoot(),
                                                               false /* favorite */,
                                                               invisibleRoot()->nodes().size() /* position */,
                                                               uniqueGroupName(invisibleRoot()),
                                                               UIChooserNodeGroupType_Local,
                                                               true /* opened */);
    UIChooserItemGroup *pNewGroupItem = new UIChooserItemGroup(root(), pNewGroupNode);

    /* Enumerate all the currently selected-items: */
    QStringList busyGroupNames;
    QStringList busyMachineNames;
    foreach (UIChooserItem *pItem, selectedItems())
    {
        /* For each of known types: */
        switch (pItem->type())
        {
            case UIChooserNodeType_Group:
            {
                /* Avoid name collisions: */
                if (busyGroupNames.contains(pItem->name()))
                    break;
                /* Add name to busy: */
                busyGroupNames << pItem->name();
                /* Copy or move group-item: */
                UIChooserNodeGroup *pNewGroupSubNode = new UIChooserNodeGroup(pNewGroupNode,
                                                                              pItem->node()->toGroupNode(),
                                                                              pNewGroupNode->nodes().size());
                new UIChooserItemGroup(pNewGroupItem, pNewGroupSubNode);
                delete pItem->node();
                break;
            }
            case UIChooserNodeType_Machine:
            {
                /* Avoid name collisions: */
                if (busyMachineNames.contains(pItem->name()))
                    break;
                /* Add name to busy: */
                busyMachineNames << pItem->name();
                /* Copy or move machine-item: */
                UIChooserNodeMachine *pNewMachineSubNode = new UIChooserNodeMachine(pNewGroupNode,
                                                                                    pItem->node()->toMachineNode(),
                                                                                    pNewGroupNode->nodes().size());
                new UIChooserItemMachine(pNewGroupItem, pNewMachineSubNode);
                delete pItem->node();
                break;
            }
        }
    }

    /* Update model: */
    wipeOutEmptyGroups();
    updateNavigationItemList();
    updateLayout();
    setSelectedItem(pNewGroupItem);
    saveGroupSettings();
}

void UIChooserModel::sltSortParentGroup()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Machine_S_SortParent)->isEnabled())
        return;

    /* Only if some item selected: */
    if (!firstSelectedItem())
        return;

    /* Sort nodes: */
    firstSelectedItem()->parentItem()->node()->sortNodes();

    /* Rebuild tree for main root: */
    buildTreeForMainRoot();
    updateNavigationItemList();
    updateLayout();
}

void UIChooserModel::sltPerformRefreshAction()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Group_S_Refresh)->isEnabled())
        return;

    /* Gather list of current unique inaccessible machine-items: */
    QList<UIChooserItemMachine*> inaccessibleMachineItemList;
    UIChooserItemMachine::enumerateMachineItems(selectedItems(), inaccessibleMachineItemList,
                                                UIChooserItemMachineEnumerationFlag_Unique |
                                                UIChooserItemMachineEnumerationFlag_Inaccessible);

    /* For each machine-item: */
    UIChooserItem *pSelectedItem = 0;
    foreach (UIChooserItemMachine *pItem, inaccessibleMachineItemList)
    {
        switch (pItem->node()->toMachineNode()->cache()->itemType())
        {
            case UIVirtualMachineItem::ItemType_Local:
            {
                /* Recache: */
                pItem->recache();

                /* Become accessible? */
                if (pItem->accessible())
                {
                    /* Machine name: */
                    const QString strMachineName = ((UIChooserItem*)pItem)->name();
                    /* We should reload this machine: */
                    sltReloadMachine(pItem->id());
                    /* Select first of reloaded items: */
                    if (!pSelectedItem)
                        pSelectedItem = root()->searchForItem(strMachineName,
                                                              UIChooserItemSearchFlag_Machine |
                                                              UIChooserItemSearchFlag_ExactName);
                }

                break;
            }
            case UIVirtualMachineItem::ItemType_CloudReal:
            {
                /* Much more simple than for local items, we are not reloading them, just refreshing: */
                pItem->node()->toMachineNode()->cache()->toCloud()->updateInfoAsync(false /* delayed */);

                break;
            }
            default:
                break;
        }
    }

    /* Some item to be selected? */
    if (pSelectedItem)
    {
        pSelectedItem->makeSureItsVisible();
        setSelectedItem(pSelectedItem);
    }
}

void UIChooserModel::sltRemoveSelectedMachine()
{
    /* Check if action is enabled: */
    if (!actionPool()->action(UIActionIndexST_M_Machine_S_Remove)->isEnabled())
        return;

    /* Enumerate all the selected machine-items: */
    QList<UIChooserItemMachine*> selectedMachineItemList;
    UIChooserItemMachine::enumerateMachineItems(selectedItems(), selectedMachineItemList);
    /* Enumerate all the existing machine-items: */
    QList<UIChooserItemMachine*> existingMachineItemList;
    UIChooserItemMachine::enumerateMachineItems(root()->items(), existingMachineItemList);

    /* Prepare arrays: */
    QMap<QUuid, bool> verdicts;
    QList<UIChooserItem*> itemsToRemove;
    QList<QUuid> machinesToUnregister;

    /* For each selected machine-item: */
    foreach (UIChooserItem *pItem, selectedMachineItemList)
    {
        /* Get machine-item id: */
        QUuid uId = pItem->toMachineItem()->id();

        /* We already decided for that machine? */
        if (verdicts.contains(uId))
        {
            /* To remove similar machine items? */
            if (!verdicts[uId])
                itemsToRemove << pItem;
            continue;
        }

        /* Selected copy count: */
        int iSelectedCopyCount = 0;
        foreach (UIChooserItem *pSelectedItem, selectedMachineItemList)
            if (pSelectedItem->toMachineItem()->id() == uId)
                ++iSelectedCopyCount;
        /* Existing copy count: */
        int iExistingCopyCount = 0;
        foreach (UIChooserItem *pExistingItem, existingMachineItemList)
            if (pExistingItem->toMachineItem()->id() == uId)
                ++iExistingCopyCount;
        /* If selected copy count equal to existing copy count,
         * we will propose ro unregister machine fully else
         * we will just propose to remove selected-items: */
        bool fVerdict = iSelectedCopyCount == iExistingCopyCount;
        verdicts.insert(uId, fVerdict);
        if (fVerdict)
            machinesToUnregister.append(uId);
        else
            itemsToRemove << pItem;
    }

    /* If we have something to remove: */
    if (!itemsToRemove.isEmpty())
        removeItems(itemsToRemove);
    /* If we have something to unregister: */
    if (!machinesToUnregister.isEmpty())
        unregisterMachines(machinesToUnregister);
}

void UIChooserModel::sltStartScrolling()
{
    /* Make sure view exists: */
    AssertPtrReturnVoid(view());

    /* Should we scroll? */
    if (!m_fIsScrollingInProgress)
        return;

    /* Reset scrolling progress: */
    m_fIsScrollingInProgress = false;

    /* Convert mouse position to view co-ordinates: */
    const QPoint mousePos = view()->mapFromGlobal(QCursor::pos());
    /* Mouse position is at the top of view? */
    if (mousePos.y() < m_iScrollingTokenSize && mousePos.y() > 0)
    {
        int iValue = mousePos.y();
        if (!iValue)
            iValue = 1;
        const int iDelta = m_iScrollingTokenSize / iValue;
        /* Backward scrolling: */
        root()->toGroupItem()->scrollBy(- 2 * iDelta);
        m_fIsScrollingInProgress = true;
        QTimer::singleShot(10, this, SLOT(sltStartScrolling()));
    }
    /* Mouse position is at the bottom of view? */
    else if (mousePos.y() > view()->height() - m_iScrollingTokenSize && mousePos.y() < view()->height())
    {
        int iValue = view()->height() - mousePos.y();
        if (!iValue)
            iValue = 1;
        const int iDelta = m_iScrollingTokenSize / iValue;
        /* Forward scrolling: */
        root()->toGroupItem()->scrollBy(2 * iDelta);
        m_fIsScrollingInProgress = true;
        QTimer::singleShot(10, this, SLOT(sltStartScrolling()));
    }
}

void UIChooserModel::sltCurrentDragObjectDestroyed()
{
    root()->resetDragToken();
}

void UIChooserModel::sltShowHideSearchWidget()
{
    if (view())
        setSearchWidgetVisible(!view()->isSearchWidgetVisible());
}

void UIChooserModel::prepare()
{
    prepareScene();
    prepareContextMenu();
    prepareHandlers();
    prepareConnections();
}

void UIChooserModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIChooserModel::prepareContextMenu()
{
    /* Context menu for global(s): */
    m_pContextMenuGlobal = new QMenu;
    if (m_pContextMenuGlobal)
    {
        /* Check if Ext Pack is ready, some of actions my depend on it: */
        CExtPack extPack = uiCommon().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
        const bool fExtPackAccessible = !extPack.isNull() && extPack.GetUsable();

#ifdef VBOX_WS_MAC
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndex_M_Application_S_About));
        m_pContextMenuGlobal->addSeparator();
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
        m_pContextMenuGlobal->addSeparator();
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM));
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
        if (fExtPackAccessible)
            m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowCloudProfileManager));

#else /* !VBOX_WS_MAC */

        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
        m_pContextMenuGlobal->addSeparator();
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM));
        m_pContextMenuGlobal->addSeparator();
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
        if (fExtPackAccessible)
            m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowCloudProfileManager));
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndex_M_Application_S_NetworkAccessManager));
        if (gEDataManager->applicationUpdateEnabled())
            m_pContextMenuGlobal->addAction(actionPool()->action(UIActionIndex_M_Application_S_CheckForUpdates));
# endif
#endif /* !VBOX_WS_MAC */
    }

    /* Context menu for group(s): */
    m_pContextMenuGroup = new QMenu;
    if (m_pContextMenuGroup)
    {
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_New));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Add));
        m_pContextMenuGroup->addSeparator();
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Rename));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Remove));
        m_pContextMenuGroup->addSeparator();
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_T_Pause));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Reset));
        m_pContextMenuGroup->addMenu(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu());
        m_pContextMenuGroup->addSeparator();
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Discard));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Refresh));
        m_pContextMenuGroup->addSeparator();
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager));
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut));
        m_pContextMenuGroup->addSeparator();
        m_pContextMenuGroup->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Sort));
    }

    /* Context menu for machine(s): */
    m_pContextMenuMachine = new QMenu;
    if (m_pContextMenuMachine)
    {
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Clone));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Move));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Remove));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup));
        m_pContextMenuMachine->addSeparator();
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_T_Pause));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Reset));
        m_pContextMenuMachine->addMenu(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu());
        m_pContextMenuMachine->addSeparator();
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh));
        m_pContextMenuMachine->addSeparator();
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut));
        m_pContextMenuMachine->addSeparator();
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent));
        m_pContextMenuMachine->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Search));
    }
}

void UIChooserModel::prepareHandlers()
{
    m_pMouseHandler = new UIChooserHandlerMouse(this);
    m_pKeyboardHandler = new UIChooserHandlerKeyboard(this);
}

void UIChooserModel::prepareConnections()
{
    UIChooser* pChooser = qobject_cast<UIChooser*>(parent());
    AssertPtrReturnVoid(pChooser);
    {
        /* Setup parent connections: */
        connect(this, &UIChooserModel::sigSelectionChanged,
                pChooser, &UIChooser::sigSelectionChanged);
        connect(this, &UIChooserModel::sigSelectionInvalidated,
                pChooser, &UIChooser::sigSelectionInvalidated);
        connect(this, &UIChooserModel::sigToggleStarted,
                pChooser, &UIChooser::sigToggleStarted);
        connect(this, &UIChooserModel::sigToggleFinished,
                pChooser, &UIChooser::sigToggleFinished);
    }

    /* Setup action connections: */
    connect(actionPool()->action(UIActionIndexST_M_Welcome_S_New), &UIAction::triggered,
            this, &UIChooserModel::sltCreateNewMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_New), &UIAction::triggered,
            this, &UIChooserModel::sltCreateNewMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_New), &UIAction::triggered,
            this, &UIChooserModel::sltCreateNewMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Rename), &UIAction::triggered,
            this, &UIChooserModel::sltEditGroupName);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Remove), &UIAction::triggered,
            this, &UIChooserModel::sltUngroupSelectedGroup);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Remove), &UIAction::triggered,
            this, &UIChooserModel::sltRemoveSelectedMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup), &UIAction::triggered,
            this, &UIChooserModel::sltGroupSelectedMachines);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Refresh), &UIAction::triggered,
            this, &UIChooserModel::sltPerformRefreshAction);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh), &UIAction::triggered,
            this, &UIChooserModel::sltPerformRefreshAction);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent), &UIAction::triggered,
            this, &UIChooserModel::sltSortParentGroup);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Sort), &UIAction::triggered,
            this, &UIChooserModel::sltSortGroup);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Search), &UIAction::triggered,
            this, &UIChooserModel::sltShowHideSearchWidget);
}

void UIChooserModel::loadLastSelectedItem()
{
    /* Load last selected-item (choose first if unable to load): */
    setSelectedItem(gEDataManager->selectorWindowLastItemChosen());
    if (!firstSelectedItem() && !navigationItems().isEmpty())
    {
        setSelectedItem(navigationItems().first());
        emit sigSelectionInvalidated();
    }
}

void UIChooserModel::saveLastSelectedItem()
{
    /* Save last selected-item: */
    gEDataManager->setSelectorWindowLastItemChosen(firstSelectedItem() ? firstSelectedItem()->definition() : QString());
}

void UIChooserModel::cleanupConnections()
{
    /* Disconnect selection-changed & selection-invalidated signal prematurelly.
     * Keep in mind, we are using static_cast instead of qobject_cast here to be
     * sure connection is disconnected even if parent is self-destroyed. */
    disconnect(this, &UIChooserModel::sigSelectionChanged,
               static_cast<UIChooser*>(parent()), &UIChooser::sigSelectionChanged);
    disconnect(this, &UIChooserModel::sigSelectionInvalidated,
               static_cast<UIChooser*>(parent()), &UIChooser::sigSelectionInvalidated);
}

void UIChooserModel::cleanupHandlers()
{
    delete m_pKeyboardHandler;
    m_pKeyboardHandler = 0;
    delete m_pMouseHandler;
    m_pMouseHandler = 0;
}

void UIChooserModel::cleanupContextMenu()
{
    delete m_pContextMenuGlobal;
    m_pContextMenuGlobal = 0;
    delete m_pContextMenuGroup;
    m_pContextMenuGroup = 0;
    delete m_pContextMenuMachine;
    m_pContextMenuMachine = 0;
}

void UIChooserModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIChooserModel::cleanup()
{
    cleanupConnections();
    cleanupHandlers();
    cleanupContextMenu();
    cleanupScene();
}

bool UIChooserModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
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
                    case UIChooserNodeType_Global:
                    {
                        /* Global context menu for global item cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Global, pEvent->screenPos());
                        return true;
                    }
                    case UIChooserNodeType_Group:
                    {
                        /* Get group-item: */
                        UIChooserItem *pGroupItem = qgraphicsitem_cast<UIChooserItemGroup*>(pItem);
                        /* Make sure thats not root: */
                        if (pGroupItem->isRoot())
                            return false;
                        /* Is this group-item only the one selected? */
                        if (selectedItems().contains(pGroupItem) && selectedItems().size() == 1)
                        {
                            /* Group context menu in that case: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pEvent->screenPos());
                            return true;
                        }
                    }
                    RT_FALL_THRU();
                    case UIChooserNodeType_Machine:
                    {
                        /* Machine context menu for other Group/Machine cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pEvent->screenPos());
                        return true;
                    }
                    default:
                        break;
                }
            }
            return true;
        }
        case QGraphicsSceneContextMenuEvent::Keyboard:
        {
            /* Get first selected-item: */
            if (UIChooserItem *pItem = firstSelectedItem())
            {
                /* If this item of known type? */
                switch (pItem->type())
                {
                    case UIChooserNodeType_Global:
                    {
                        /* Global context menu for global item cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pEvent->screenPos());
                        return true;
                    }
                    case UIChooserNodeType_Group:
                    {
                        /* Is this group-item only the one selected? */
                        if (selectedItems().size() == 1)
                        {
                            /* Group context menu in that case: */
                            popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pEvent->screenPos());
                            return true;
                        }
                    }
                    RT_FALL_THRU();
                    case UIChooserNodeType_Machine:
                    {
                        /* Machine context menu for other Group/Machine cases: */
                        popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pEvent->screenPos());
                        return true;
                    }
                    default:
                        break;
                }
            }
            return true;
        }
        default:
            break;
    }
    /* Pass others context menu events: */
    return false;
}

void UIChooserModel::popupContextMenu(UIGraphicsSelectorContextMenuType enmType, QPoint point)
{
    /* Which type of context-menu requested? */
    switch (enmType)
    {
        /* For global item? */
        case UIGraphicsSelectorContextMenuType_Global:
        {
            m_pContextMenuGlobal->exec(point);
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
}

void UIChooserModel::clearRealFocus()
{
    /* Set the real focus to null: */
    scene()->setFocusItem(0);
}

QList<UIChooserItem*> UIChooserModel::createNavigationItemList(UIChooserItem *pItem)
{
    /* Prepare navigation list: */
    QList<UIChooserItem*> navigationItems;

    /* Iterate over all the global-items: */
    foreach (UIChooserItem *pGlobalItem, pItem->items(UIChooserNodeType_Global))
        navigationItems << pGlobalItem;
    /* Iterate over all the group-items: */
    foreach (UIChooserItem *pGroupItem, pItem->items(UIChooserNodeType_Group))
    {
        navigationItems << pGroupItem;
        if (pGroupItem->toGroupItem()->isOpened())
            navigationItems << createNavigationItemList(pGroupItem);
    }
    /* Iterate over all the machine-items: */
    foreach (UIChooserItem *pMachineItem, pItem->items(UIChooserNodeType_Machine))
        navigationItems << pMachineItem;

    /* Return navigation list: */
    return navigationItems;
}

void UIChooserModel::buildTreeForMainRoot()
{
    /* Cleanup previous tree if exists: */
    delete m_pRoot;
    m_pRoot = 0;

    /* Build whole tree for invisible root item: */
    m_pRoot = new UIChooserItemGroup(scene(), invisibleRoot()->toGroupNode());

    /* Install root as event-filter for scene view,
     * we need QEvent::Scroll events from it: */
    root()->installEventFilterHelper(view());
}

void UIChooserModel::removeItems(const QList<UIChooserItem*> &itemsToRemove)
{
    /* Confirm machine-items removal: */
    QStringList names;
    foreach (UIChooserItem *pItem, itemsToRemove)
        names << pItem->name();
    if (!msgCenter().confirmMachineItemRemoval(names))
        return;

    /* Remove all the passed nodes: */
    foreach (UIChooserItem *pItem, itemsToRemove)
        delete pItem->node();

    /* And update model: */
    wipeOutEmptyGroups();
    updateNavigationItemList();
    updateLayout();
    if (!navigationItems().isEmpty())
    {
        setSelectedItem(navigationItems().first());
        emit sigSelectionInvalidated();
    }
    else
        clearSelectedItems();
    saveGroupSettings();
}

void UIChooserModel::unregisterMachines(const QList<QUuid> &ids)
{
    /* Populate machine list: */
    QList<CMachine> machines;
    CVirtualBox vbox = uiCommon().virtualBox();
    foreach (const QUuid &uId, ids)
    {
        CMachine machine = vbox.FindMachine(uId.toString());
        if (!machine.isNull())
            machines << machine;
    }

    /* Confirm machine removal: */
    int iResultCode = msgCenter().confirmMachineRemoval(machines);
    if (iResultCode == AlertButton_Cancel)
        return;

    /* Change selection to some close by item: */
    setSelectedItem(findClosestUnselectedItem());

    /* For every selected-item: */
    for (int iMachineIndex = 0; iMachineIndex < machines.size(); ++iMachineIndex)
    {
        /* Get iterated machine: */
        CMachine &machine = machines[iMachineIndex];
        if (iResultCode == AlertButton_Choice1)
        {
            /* Unregister machine first: */
            CMediumVector media = machine.Unregister(KCleanupMode_DetachAllReturnHardDisksOnly);
            if (!machine.isOk())
            {
                msgCenter().cannotRemoveMachine(machine);
                continue;
            }
            /* Prepare cleanup progress: */
            CProgress progress = machine.DeleteConfig(media);
            if (!machine.isOk())
            {
                msgCenter().cannotRemoveMachine(machine);
                continue;
            }
            /* And show cleanup progress finally: */
            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_delete_90px.png");
            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotRemoveMachine(machine, progress);
                continue;
            }
        }
        else if (iResultCode == AlertButton_Choice2 || iResultCode == AlertButton_Ok)
        {
            /* Unregister machine first: */
            CMediumVector media = machine.Unregister(KCleanupMode_DetachAllReturnHardDisksOnly);
            if (!machine.isOk())
            {
                msgCenter().cannotRemoveMachine(machine);
                continue;
            }
            /* Finally close all media, deliberately ignoring errors: */
            foreach (CMedium medium, media)
            {
                if (!medium.isNull())
                    medium.Close();
            }
        }
    }
}

bool UIChooserModel::processDragMoveEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Make sure view exists: */
    AssertPtrReturn(view(), false);

    /* Do we scrolling already? */
    if (m_fIsScrollingInProgress)
        return false;

    /* Check scroll-area: */
    const QPoint eventPoint = view()->mapFromGlobal(pEvent->screenPos());
    if (   (eventPoint.y() < m_iScrollingTokenSize)
        || (eventPoint.y() > view()->height() - m_iScrollingTokenSize))
    {
        /* Set scrolling in progress: */
        m_fIsScrollingInProgress = true;
        /* Start scrolling: */
        QTimer::singleShot(200, this, SLOT(sltStartScrolling()));
    }

    /* Pass event: */
    return false;
}

bool UIChooserModel::processDragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Event object is not required here: */
    Q_UNUSED(pEvent);

    /* Make sure to stop scrolling as drag-leave event happened: */
    if (m_fIsScrollingInProgress)
        m_fIsScrollingInProgress = false;

    /* Pass event: */
    return false;
}
