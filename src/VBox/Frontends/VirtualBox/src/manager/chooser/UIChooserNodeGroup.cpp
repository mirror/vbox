/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGroup class implementation.
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

/* GUI includes: */
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeGlobal.h"
#include "UIChooserNodeMachine.h"


UIChooserNodeGroup::UIChooserNodeGroup(UIChooserNode *pParent,
                                       bool fFavorite,
                                       int iPosition,
                                       const QString &strName,
                                       bool fOpened)
    : UIChooserNode(pParent, fFavorite)
    , m_strName(strName)
    , m_fOpened(fOpened)
{
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeGroup::UIChooserNodeGroup(UIChooserNode *pParent,
                                       UIChooserNodeGroup *pCopyFrom,
                                       int iPosition)
    : UIChooserNode(pParent, pCopyFrom->isFavorite())
    , m_strName(pCopyFrom->name())
    , m_fOpened(pCopyFrom->isOpened())
{
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    copyContents(pCopyFrom);
    retranslateUi();
}

UIChooserNodeGroup::~UIChooserNodeGroup()
{
    /* Cleanup groups first, that
     * gives us proper recursion: */
    while (!m_nodesGroup.isEmpty())
        delete m_nodesGroup.last();
    while (!m_nodesGlobal.isEmpty())
        delete m_nodesGlobal.last();
    while (!m_nodesMachine.isEmpty())
        delete m_nodesMachine.last();
    delete item();
    if (parentNode())
        parentNode()->removeNode(this);
}

QString UIChooserNodeGroup::name() const
{
    return m_strName;
}

QString UIChooserNodeGroup::fullName() const
{
    /* Return "/" for root item: */
    if (isRoot())
        return "/";
    /* Get full parent name, append with '/' if not yet appended: */
    QString strFullParentName = parentNode()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIChooserNodeGroup::description() const
{
    return m_strDescription;
}

QString UIChooserNodeGroup::definition() const
{
    return QString("g=%1").arg(name());
}

bool UIChooserNodeGroup::hasNodes(UIChooserItemType enmType /* = UIChooserItemType_Any */) const
{
    switch (enmType)
    {
        case UIChooserItemType_Any:
            return hasNodes(UIChooserItemType_Group) || hasNodes(UIChooserItemType_Global) || hasNodes(UIChooserItemType_Machine);
        case UIChooserItemType_Group:
            return !m_nodesGroup.isEmpty();
        case UIChooserItemType_Global:
            return !m_nodesGlobal.isEmpty();
        case UIChooserItemType_Machine:
            return !m_nodesMachine.isEmpty();
    }
    return false;
}

QList<UIChooserNode*> UIChooserNodeGroup::nodes(UIChooserItemType enmType /* = UIChooserItemType_Any */) const
{
    switch (enmType)
    {
        case UIChooserItemType_Any:     return m_nodesGlobal + m_nodesGroup + m_nodesMachine;
        case UIChooserItemType_Group:   return m_nodesGroup;
        case UIChooserItemType_Global:  return m_nodesGlobal;
        case UIChooserItemType_Machine: return m_nodesMachine;
    }
    AssertFailedReturn(QList<UIChooserNode*>());
}

void UIChooserNodeGroup::addNode(UIChooserNode *pNode, int iPosition)
{
    switch (pNode->type())
    {
        case UIChooserItemType_Group:   m_nodesGroup.insert(iPosition, pNode); return;
        case UIChooserItemType_Global:  m_nodesGlobal.insert(iPosition, pNode); return;
        case UIChooserItemType_Machine: m_nodesMachine.insert(iPosition, pNode); return;
        default: break;
    }
    AssertFailedReturnVoid();
}

void UIChooserNodeGroup::removeNode(UIChooserNode *pNode)
{
    switch (pNode->type())
    {
        case UIChooserItemType_Group:   m_nodesGroup.removeAll(pNode); return;
        case UIChooserItemType_Global:  m_nodesGlobal.removeAll(pNode); return;
        case UIChooserItemType_Machine: m_nodesMachine.removeAll(pNode); return;
        default: break;
    }
    AssertFailedReturnVoid();
}

void UIChooserNodeGroup::removeAllNodes(const QUuid &uId)
{
    foreach (UIChooserNode *pNode, nodes())
        pNode->removeAllNodes(uId);
}

void UIChooserNodeGroup::updateAllNodes(const QUuid &uId)
{
    // Nothing to update for group-node..

    /* Update group-item: */
    item()->updateItem();

    /* Update all the children recursively: */
    foreach (UIChooserNode *pNode, nodes())
        pNode->updateAllNodes(uId);
}

int UIChooserNodeGroup::positionOf(UIChooserNode *pNode)
{
    switch (pNode->type())
    {
        case UIChooserItemType_Group:   return m_nodesGroup.indexOf(pNode->toGroupNode());
        case UIChooserItemType_Global:  return m_nodesGlobal.indexOf(pNode->toGlobalNode());
        case UIChooserItemType_Machine: return m_nodesMachine.indexOf(pNode->toMachineNode());
        default: break;
    }
    AssertFailedReturn(0);
}

void UIChooserNodeGroup::setName(const QString &strName)
{
    /* Make sure something changed: */
    if (m_strName == strName)
        return;

    /* Save name: */
    m_strName = strName;

    /* Update group-item: */
    if (item())
        item()->updateItem();
}

void UIChooserNodeGroup::searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems)
{
    if (iItemSearchFlags & UIChooserItemSearchFlag_Group)
    {
        /* if the search term is empty we just add the node to the matched list: */
        if (strSearchTerm.isEmpty())
            matchedItems << this;
        else
        {
            if (iItemSearchFlags & UIChooserItemSearchFlag_ExactName)
            {
                if (name() == strSearchTerm)
                    matchedItems << this;
            }
            else
            {
                if (name().contains(strSearchTerm, Qt::CaseInsensitive))
                    matchedItems << this;
            }
        }
    }

    foreach (UIChooserNode *pNode, m_nodesGroup)
        pNode->searchForNodes(strSearchTerm, iItemSearchFlags, matchedItems);

    foreach (UIChooserNode *pNode, m_nodesGlobal)
        pNode->searchForNodes(strSearchTerm, iItemSearchFlags, matchedItems);

    foreach (UIChooserNode *pNode, m_nodesMachine)
        pNode->searchForNodes(strSearchTerm, iItemSearchFlags, matchedItems);
}

void UIChooserNodeGroup::sortNodes()
{
    QMap<QString, UIChooserNode*> mapGroup;
    foreach (UIChooserNode *pNode, m_nodesGroup)
        mapGroup[pNode->name()] = pNode;
    m_nodesGroup = mapGroup.values();

    QMap<QString, UIChooserNode*> mapGlobal;
    foreach (UIChooserNode *pNode, m_nodesGlobal)
        mapGlobal[pNode->name()] = pNode;
    m_nodesGlobal = mapGlobal.values();

    QMap<QString, UIChooserNode*> mapMachine;
    foreach (UIChooserNode *pNode, m_nodesMachine)
        mapMachine[pNode->name()] = pNode;
    m_nodesMachine = mapMachine.values();
}

void UIChooserNodeGroup::retranslateUi()
{
    /* Update description: */
    m_strDescription = tr("Virtual Machine group");

    /* Update group-item: */
    if (item())
        item()->updateItem();
}

void UIChooserNodeGroup::copyContents(UIChooserNodeGroup *pCopyFrom)
{
    foreach (UIChooserNode *pNode, pCopyFrom->nodes(UIChooserItemType_Group))
        new UIChooserNodeGroup(this, pNode->toGroupNode(), m_nodesGroup.size());
    foreach (UIChooserNode *pNode, pCopyFrom->nodes(UIChooserItemType_Global))
        new UIChooserNodeGlobal(this, pNode->toGlobalNode(), m_nodesGlobal.size());
    foreach (UIChooserNode *pNode, pCopyFrom->nodes(UIChooserItemType_Machine))
        new UIChooserNodeMachine(this, pNode->toMachineNode(), m_nodesMachine.size());
}
