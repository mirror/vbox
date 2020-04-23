/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeMachine class implementation.
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

/* GUI includes: */
#include "UIChooserAbstractModel.h"
#include "UIChooserNodeMachine.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"


UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int  iPosition,
                                           const CMachine &comMachine)
    : UIChooserNode(pParent, fFavorite)
    , m_pCache(new UIVirtualMachineItemLocal(comMachine))
{
    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int iPosition,
                                           const CCloudMachine &comCloudMachine)
    : UIChooserNode(pParent, fFavorite)
    , m_pCache(new UIVirtualMachineItemCloud(comCloudMachine))
{
    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Cloud VM item can notify machine node only directly (no console), we have to setup listener: */
    connect(static_cast<UIVirtualMachineItemCloud*>(m_pCache), &UIVirtualMachineItemCloud::sigStateChange,
            this, &UIChooserNodeMachine::sltHandleStateChange);
    connect(static_cast<UIVirtualMachineItemCloud*>(m_pCache), &UIVirtualMachineItemCloud::sigStateChange,
            static_cast<UIChooserAbstractModel*>(model()), &UIChooserAbstractModel::sltHandleCloudMachineStateChange);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int  iPosition)
    : UIChooserNode(pParent, fFavorite)
    , m_pCache(new UIVirtualMachineItemCloud)
{
    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           UIChooserNodeMachine *pCopyFrom,
                                           int iPosition)
    : UIChooserNode(pParent, pCopyFrom->isFavorite())
{
    /* Prepare cache of corresponding type: */
    switch (pCopyFrom->cacheType())
    {
        case UIVirtualMachineItemType_Local:
            m_pCache = new UIVirtualMachineItemLocal(pCopyFrom->cache()->toLocal()->machine());
            break;
        case UIVirtualMachineItemType_CloudFake:
            m_pCache = new UIVirtualMachineItemCloud;
            break;
        case UIVirtualMachineItemType_CloudReal:
            m_pCache = new UIVirtualMachineItemCloud(pCopyFrom->cache()->toCloud()->machine());
            break;
        default:
            break;
    }

    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeMachine::~UIChooserNodeMachine()
{
    /* Delete item: */
    delete item();

    /* Remove from parent: */
    if (parentNode())
        parentNode()->removeNode(this);

    /* Cleanup cache: */
    delete m_pCache;
}

QString UIChooserNodeMachine::name() const
{
    return m_pCache->name();
}

QString UIChooserNodeMachine::fullName() const
{
    /* Get full parent name, append with '/' if not yet appended: */
    AssertReturn(parentNode(), name());
    QString strFullParentName = parentNode()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIChooserNodeMachine::description() const
{
    return m_strDescription;
}

QString UIChooserNodeMachine::definition() const
{
    return QString("m=%1").arg(UIChooserAbstractModel::toOldStyleUuid(id()));
}

bool UIChooserNodeMachine::hasNodes(UIChooserNodeType enmType /* = UIChooserNodeType_Any */) const
{
    Q_UNUSED(enmType);
    AssertFailedReturn(false);
}

QList<UIChooserNode*> UIChooserNodeMachine::nodes(UIChooserNodeType enmType /* = UIChooserNodeType_Any */) const
{
    Q_UNUSED(enmType);
    AssertFailedReturn(QList<UIChooserNode*>());
}

void UIChooserNodeMachine::addNode(UIChooserNode *pNode, int iPosition)
{
    Q_UNUSED(pNode);
    Q_UNUSED(iPosition);
    AssertFailedReturnVoid();
}

void UIChooserNodeMachine::removeNode(UIChooserNode *pNode)
{
    Q_UNUSED(pNode);
    AssertFailedReturnVoid();
}

void UIChooserNodeMachine::removeAllNodes(const QUuid &uId)
{
    /* Skip other ids: */
    if (id() != uId)
        return;

    /* Remove this node: */
    delete this;
}

void UIChooserNodeMachine::updateAllNodes(const QUuid &uId)
{
    /* Skip other ids: */
    if (id() != uId)
        return;

    /* Update cache: */
    m_pCache->recache();

    /* Update machine-item: */
    if (item())
        item()->updateItem();
}

int UIChooserNodeMachine::positionOf(UIChooserNode *pNode)
{
    Q_UNUSED(pNode);
    AssertFailedReturn(0);
}

void UIChooserNodeMachine::searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems)
{
    /* Ignore if we are not searching for the machine-node: */
    if (!(iItemSearchFlags & UIChooserItemSearchFlag_Machine))
        return;

    /* If the search term is empty we just add the node to the matched list: */
    if (strSearchTerm.isEmpty())
        matchedItems << this;
    else
    {
        /* If exact ID flag specified => check node ID:  */
        if (iItemSearchFlags & UIChooserItemSearchFlag_ExactId)
        {
            if (id() == QUuid(strSearchTerm))
                matchedItems << this;
        }
        /* If exact name flag specified => check full node name: */
        else if (iItemSearchFlags & UIChooserItemSearchFlag_ExactName)
        {
            if (name() == strSearchTerm)
                matchedItems << this;
        }
        /* Otherwise check if name contains search term, including wildcards: */
        else
        {
            QRegExp searchRegEx(strSearchTerm, Qt::CaseInsensitive, QRegExp::WildcardUnix);
            if (name().contains(searchRegEx))
                matchedItems << this;
        }
    }
}

void UIChooserNodeMachine::sortNodes()
{
    AssertFailedReturnVoid();
}

UIVirtualMachineItem *UIChooserNodeMachine::cache() const
{
    return m_pCache;
}

UIVirtualMachineItemType UIChooserNodeMachine::cacheType() const
{
    return cache() ? cache()->itemType() : UIVirtualMachineItemType_Invalid;
}

QUuid UIChooserNodeMachine::id() const
{
    return cache() ? cache()->id() : QUuid();
}

bool UIChooserNodeMachine::accessible() const
{
    return cache() ? cache()->accessible() : false;
}

void UIChooserNodeMachine::retranslateUi()
{
    /* Update internal stuff: */
    m_strDescription = tr("Virtual Machine");

    /* Update machine-item: */
    if (item())
        item()->updateItem();
}

void UIChooserNodeMachine::sltHandleStateChange()
{
    /* Update machine-item: */
    if (item())
        item()->updateItem();
}
