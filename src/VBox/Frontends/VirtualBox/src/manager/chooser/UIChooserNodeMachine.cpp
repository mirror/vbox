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
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int iPosition,
                                           const QString &strName)
    : UIChooserNode(pParent, fFavorite)
    , m_pCache(new UIVirtualMachineItemCloud(strName))
{
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int  iPosition)
    : UIChooserNode(pParent, fFavorite)
    , m_pCache(new UIVirtualMachineItemCloud)
{
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           UIChooserNodeMachine *pCopyFrom,
                                           int iPosition)
    : UIChooserNode(pParent, pCopyFrom->isFavorite())
{
    switch (pCopyFrom->cache()->itemType())
    {
        case UIVirtualMachineItem::ItemType_Local:
            m_pCache = new UIVirtualMachineItemLocal(pCopyFrom->cache()->toLocal()->machine());
            break;
        case UIVirtualMachineItem::ItemType_CloudFake:
            m_pCache = new UIVirtualMachineItemCloud;
            break;
        default:
            break;
    }

    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeMachine::~UIChooserNodeMachine()
{
    delete item();
    if (parentNode())
        parentNode()->removeNode(this);
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
    return QString("m=%1").arg(name());
}

bool UIChooserNodeMachine::hasNodes(UIChooserItemType enmType /* = UIChooserItemType_Any */) const
{
    Q_UNUSED(enmType);
    AssertFailedReturn(false);
}

QList<UIChooserNode*> UIChooserNodeMachine::nodes(UIChooserItemType enmType /* = UIChooserItemType_Any */) const
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
    if (QUuid(m_pCache->id()) != uId)
        return;

    /* Remove this node: */
    delete this;
}

void UIChooserNodeMachine::updateAllNodes(const QUuid &uId)
{
    /* Skip other ids: */
    if (QUuid(m_pCache->id()) != uId)
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
    if (!(iItemSearchFlags & UIChooserItemSearchFlag_Machine))
        return;

    if (strSearchTerm.isEmpty())
    {
        matchedItems << this;
        return;
    }

    if (iItemSearchFlags & UIChooserItemSearchFlag_ExactName)
    {
        if (name() == strSearchTerm)
            matchedItems << this;
        return;
    }
    QRegExp searchRegEx(strSearchTerm, Qt::CaseInsensitive, QRegExp::WildcardUnix);
    if (name().contains(searchRegEx))
        matchedItems << this;
}

void UIChooserNodeMachine::sortNodes()
{
    AssertFailedReturnVoid();
}

void UIChooserNodeMachine::retranslateUi()
{
    /* Update internal stuff: */
    m_strDescription = tr("Virtual Machine");

    /* Update machine-item: */
    if (item())
        item()->updateItem();
}
