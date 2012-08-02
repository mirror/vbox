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
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>

/* GUI includes: */
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"
#include "UIDefs.h"
#include "VBoxGlobal.h"
#include "UIActionPoolSelector.h"
#include "UIGraphicsSelectorMouseHandler.h"
#include "UIGraphicsSelectorKeyboardHandler.h"

/* COM includes: */
#include "CMachine.h"

UIGraphicsSelectorModel::UIGraphicsSelectorModel(QObject *pParent)
    : QObject(pParent)
    , m_pScene(0)
    , m_mode(UIGraphicsSelectorMode_Default)
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

    /* Load definitions: */
    loadDefinitions();
}

UIGraphicsSelectorModel::~UIGraphicsSelectorModel()
{
    /* Save definitions: */
    saveDefinitions();

    /* Cleanup handlers: */
    cleanupHandlers();

    /* Prepare context-menu: */
    cleanupContextMenu();

    /* Cleanup root: */
    cleanupRoot();

    /* Cleanup scene: */
    cleanupScene();
 }

QGraphicsScene* UIGraphicsSelectorModel::scene() const
{
    return m_pScene;
}

void UIGraphicsSelectorModel::setMode(UIGraphicsSelectorMode mode)
{
    /* Something changed? */
    if (m_mode != mode)
    {
        /* Rebuild scene for new mode: */
        m_mode = mode;
        rebuild();
    }
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

void UIGraphicsSelectorModel::updateDefinition()
{
    /* Do nothing for mode = UIGraphicsSelectorMode_Default: */
    if (m_mode == UIGraphicsSelectorMode_Default)
        return;

    /* Remove all the empty groups: */
    m_pRoot->cleanup();

    /* Add machines which are not in any group: */
    foreach (const CMachine &machine, vboxGlobal().virtualBox().GetMachines())
        if (!m_pRoot->toGroupItem()->contains(machine.GetId(), true))
            new UIGSelectorItemMachine(m_pRoot, machine);

    /* Serialize definitions: */
    m_strDefinitions = serializeDefinitions(m_pRoot);
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
    m_pRoot->setDesiredWidth(iViewportWidth);
    m_pRoot->setDesiredHeight(iViewportHeight);
    /* Set root item position: */
    m_pRoot->setPos(iSceneMargin, iSceneMargin);
    /* Update all the size-hints recursively: */
    m_pRoot->updateSizeHint();
    /* Relayout root item: */
    m_pRoot->updateLayout();
    /* Notify listener about root-item relayouted: */
    emit sigRootItemResized(m_pRoot->minimumSizeHint());
}

void UIGraphicsSelectorModel::setCurrentDragObject(QDrag *pDragObject)
{
    /* Make sure real focus unset: */
    clearRealFocus();

    /* Remember new drag-object: */
    m_pCurrentDragObject = pDragObject;
    connect(m_pCurrentDragObject, SIGNAL(destroyed(QObject*)), this, SLOT(sltCurrentDragObjectDestroyed()));
}

void UIGraphicsSelectorModel::sltMachineStateChanged(QString strId, KMachineState)
{
    /* Update machine items with passed id: */
    updateMachineItems(m_pRoot, strId);
}

void UIGraphicsSelectorModel::sltMachineDataChanged(QString strId)
{
    /* Update machine items with passed id: */
    updateMachineItems(m_pRoot, strId);
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
            /* Add new machine item into the root: */
            UIGSelectorItemMachine *pItem = new UIGSelectorItemMachine(m_pRoot, machine);
            /* And update model: */
            updateDefinition();
            updateNavigation();
            updateLayout();
            setCurrentItem(pItem);
        }
    }
    /* Existing VM unregistered? */
    else
    {
        /* Remove machine items with passed id: */
        removeMachineItems(m_pRoot, strId);
        /* And update model: */
        updateDefinition();
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
    updateMachineItems(m_pRoot, strId);
}

void UIGraphicsSelectorModel::sltSnapshotChanged(QString strId, QString)
{
    /* Update machine items with passed id: */
    updateMachineItems(m_pRoot, strId);
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
        updateDefinition();
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
    m_pRoot = new UIGSelectorItemGroup(0, "root");
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

void UIGraphicsSelectorModel::loadDefinitions()
{
    m_strDefinitions = vboxGlobal().virtualBox().GetExtraData(GUI_Definitions);
}

void UIGraphicsSelectorModel::saveDefinitions()
{
    vboxGlobal().virtualBox().SetExtraData(GUI_Definitions, m_strDefinitions);
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
        {
            /* Do we have a context menu request? */
            QGraphicsSceneContextMenuEvent *pContextMenuEvent = static_cast<QGraphicsSceneContextMenuEvent*>(pEvent);
            /* Selection list is empty? */
            if (selectionList().isEmpty())
            {
                /* Root context menu: */
                popupContextMenu(UIGraphicsSelectorContextMenuType_Root, pContextMenuEvent->screenPos());
            }
            else
            {
                /* Get first selected item: */
                UIGSelectorItem *pItem = selectionList().first();
                /* Selection list contains just one item and that is group: */
                if (selectionList().size() == 1 && pItem->type() == UIGSelectorItemType_Group)
                {
                    /* Group context menu in that case: */
                    popupContextMenu(UIGraphicsSelectorContextMenuType_Group, pContextMenuEvent->screenPos());
                }
                else
                {
                    /* Machine context menu for all the other cases: */
                    popupContextMenu(UIGraphicsSelectorContextMenuType_Machine, pContextMenuEvent->screenPos());
                }
            }
            break;
        }
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIGraphicsSelectorModel::clearRealFocus()
{
    /* Set real focus to null: */
    scene()->setFocusItem(0);
}

void UIGraphicsSelectorModel::rebuild()
{
    /* Unset current item: */
    unsetCurrentItem();

    /* If there is something to clear first: */
    if (m_pRoot->hasItems())
        m_pRoot->clearItems();

    /* Extract definitions: */
    switch (m_mode)
    {
        case UIGraphicsSelectorMode_Default:
        {
            populateDefinitions();
            break;
        }
        case UIGraphicsSelectorMode_ShowGroups:
        {
            extractDefinitions(m_strDefinitions, m_pRoot);
            break;
        }
    }

    /* Update model: */
    updateDefinition();
    updateNavigation();
    updateLayout();
    if (m_pRoot->hasItems())
        setCurrentItem(0);
    else
        unsetCurrentItem();
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

void UIGraphicsSelectorModel::populateDefinitions()
{
    foreach (const CMachine &machine, vboxGlobal().virtualBox().GetMachines())
        new UIGSelectorItemMachine(m_pRoot, machine);
}

void UIGraphicsSelectorModel::extractDefinitions(const QString &strDefinitions,
                                                 UIGSelectorItem *pParent, int iLevel)
{
    if (strDefinitions.isEmpty())
        return;

    /* Prepare indent: */
    QString strIndent;
    for (int i = 0; i < iLevel; ++i)
        strIndent += ' ';

    /* Calculate maximum definitions depth: */
    int iMaximumDefinitionsDepth = 0;
    int iCurrentDefinitionsDepth = 0;
    foreach (const QChar &symbol, strDefinitions)
    {
        if (symbol == '{')
        {
            ++iCurrentDefinitionsDepth;
            if (iCurrentDefinitionsDepth > iMaximumDefinitionsDepth)
                iMaximumDefinitionsDepth = iCurrentDefinitionsDepth;
        }
        else if (symbol == '}')
            --iCurrentDefinitionsDepth;
    }
    AssertMsg(iCurrentDefinitionsDepth == 0, ("Incorrect definitions!"));

    /* Prepare templates: */
    QString strDefinitionHeaderTemplate("%1=([^\\{\\}\\,]+)");
    QString strDefinitionBodyTemplate("\\{%1\\}");
    QString strHeaderForGroup(strDefinitionHeaderTemplate.arg("g"));
    QString strHeaderForMachine(strDefinitionHeaderTemplate.arg("m"));
    QString strDefinitionForGroup(strHeaderForGroup + strDefinitionBodyTemplate);
    QString strDefinitionForMachine(strHeaderForMachine);
    QString strMinimumDefinitionBody("[^\\{\\}]+");
    QString strFullDefinitionBody = QString("(") + strDefinitionForGroup + QString("|") + strDefinitionForMachine + QString(")");
    QString strDefinitionTemplate("%1");
    QString strDefinition(strDefinitionTemplate.arg(strFullDefinitionBody));
    QString strDefinitionSetTemplate("(%1(,%1)*)");
    QString strDefinitionSet(strDefinitionSetTemplate.arg(strFullDefinitionBody));

    /* Generate expressions of maximum depth: */
    QString strFullDefinition(strDefinition);
    for (int i = 0; i < iMaximumDefinitionsDepth - 1; ++i)
        strFullDefinition = strFullDefinition.arg(strDefinitionSet);
    strFullDefinition = strFullDefinition.arg(strMinimumDefinitionBody);
    QString strFullSetDefinition(strDefinitionSet);
    for (int i = 0; i < iMaximumDefinitionsDepth - 1; ++i)
        strFullSetDefinition = strFullSetDefinition.arg(strDefinitionSet);
    strFullSetDefinition = strFullSetDefinition.arg(strMinimumDefinitionBody);

    /* Parsing: */
    QRegExp thisLevelRegExp(strFullDefinition);
    int iOffset = 0;
    while (thisLevelRegExp.indexIn(strDefinitions, iOffset) != -1)
    {
        /* Get iterated definition: */
        QString strParsedTag(thisLevelRegExp.cap());

        /* Prepare reg-exps: */
        QRegExp groupRegExp(QString("^%1$").arg(strDefinitionForGroup.arg("[\\s\\S]+")));
        QRegExp machineRegExp(QString("^%1$").arg(strDefinitionForMachine));

        /* Parsing groups: */
        if (groupRegExp.indexIn(strParsedTag) != -1)
        {
            /* Prepare group name: */
            QString strGroupName(groupRegExp.cap(1));
            printf("%s*** Group: %s\n", strIndent.toAscii().constData(), strGroupName.toAscii().constData());

            /* Create new group item: */
            UIGSelectorItem *pNextLevelParent = new UIGSelectorItemGroup(pParent, strGroupName);;

            /* Parsing group content: */
            QRegExp groupContentsRegExp(strFullSetDefinition);
            groupContentsRegExp.indexIn(strParsedTag, 1);
            QString strGroupContents(groupContentsRegExp.cap());
            if (!strGroupContents.isEmpty())
                extractDefinitions(strGroupContents, pNextLevelParent, iLevel + 1);
        }
        /* Parsing machine: */
        else if (machineRegExp.indexIn(strParsedTag) != -1)
        {
            /* Prepare machine name: */
            QString strMachineName(machineRegExp.cap(1));
            printf("%s*** Machine: %s\n", strIndent.toAscii().constData(), strMachineName.toAscii().constData());

            /* Search for machine: */
            CMachine machine = vboxGlobal().virtualBox().FindMachine(strMachineName);
            /* Create new machine item if machine was found: */
            if (!machine.isNull())
                new UIGSelectorItemMachine(pParent, machine);
        }

        /* Update offset with iterated definition length: */
        iOffset = thisLevelRegExp.pos() + strParsedTag.size();
    }
}

QString UIGraphicsSelectorModel::serializeDefinitions(UIGSelectorItem *pParent) const
{
    /* Prepare definitions: */
    QString strDefinitions;
    /* Items: */
    QStringList items;
    /* Serialize group items: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        items << serializeDefinitions(pItem);
    /* Serialize machine items: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        items << QString("m=%1").arg(pItem->name());
    /* Join group definitions: */
    strDefinitions += items.join(",");
    /* Is this non-root group? */
    if (pParent->parentItem())
        strDefinitions = QString("g=%1{%2}").arg(pParent->name(), strDefinitions);
    /* Return definitions: */
    return strDefinitions;
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

void UIGraphicsSelectorModel::updateMachineItems(UIGSelectorItem *pParent, const QString &strId)
{
    /* For each group item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        updateMachineItems(pItem->toGroupItem(), strId);
    /* For each machine item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        if (UIGSelectorItemMachine *pMachineItem = pItem->toMachineItem())
            if (pMachineItem->id() == strId)
            {
                pMachineItem->recache();
                pMachineItem->update();
            }
}

void UIGraphicsSelectorModel::removeMachineItems(UIGSelectorItem *pParent, const QString &strId)
{
    /* For each group item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Group))
        removeMachineItems(pItem->toGroupItem(), strId);
    /* For each machine item in passed parent: */
    foreach (UIGSelectorItem *pItem, pParent->items(UIGSelectorItemType_Machine))
        if (pItem->toMachineItem()->id() == strId)
            delete pItem;
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

