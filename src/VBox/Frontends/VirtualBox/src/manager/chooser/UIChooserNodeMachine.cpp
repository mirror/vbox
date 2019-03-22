/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeMachine class implementation.
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
#include "UIChooserNodeMachine.h"


UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           int  iPosition,
                                           const CMachine &comMachine)
    : UIChooserNode(pParent, fFavorite)
    , UIVirtualMachineItem(comMachine)
{
    if (parentNode())
        parentNode()->addNode(this, iPosition);
    retranslateUi();
}

UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           UIChooserNodeMachine *pCopyFrom,
                                           int iPosition)
    : UIChooserNode(pParent, pCopyFrom->isFavorite())
    , UIVirtualMachineItem(pCopyFrom->machine())
{
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
    return UIVirtualMachineItem::name();
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

    /* Update machine-node: */
    recache();

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

void UIChooserNodeMachine::retranslateUi()
{
    /* Update description: */
    m_strDescription = tr("Virtual Machine");

    /* Update machine-item: */
    if (item())
        item()->updateItem();
}
