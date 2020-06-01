/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGlobal class implementation.
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
#include "UIChooserNodeGlobal.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIChooserNodeGlobal::UIChooserNodeGlobal(UIChooserNode *pParent,
                                         int iPosition,
                                         bool fFavorite,
                                         const QString &)
    : UIChooserNode(pParent, fFavorite)
{
    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeGlobal::UIChooserNodeGlobal(UIChooserNode *pParent,
                                         int iPosition,
                                         UIChooserNodeGlobal *pCopyFrom)
    : UIChooserNode(pParent, pCopyFrom->isFavorite())
{
    /* Add to parent: */
    if (parentNode())
        parentNode()->addNode(this, iPosition);

    /* Apply language settings: */
    retranslateUi();
}

UIChooserNodeGlobal::~UIChooserNodeGlobal()
{
    /* Delete item: */
    delete item();

    /* Remove from parent: */
    if (parentNode())
        parentNode()->removeNode(this);
}

QString UIChooserNodeGlobal::name() const
{
    return m_strName;
}

QString UIChooserNodeGlobal::fullName() const
{
    return name();
}

QString UIChooserNodeGlobal::description() const
{
    return name();
}

QString UIChooserNodeGlobal::definition(bool fFull /* = false */) const
{
    const QString strNodePrefix = UIChooserAbstractModel::prefixToString(UIChooserNodeDataPrefixType_Global);
    const QString strNodeOptionFavorite = UIChooserAbstractModel::optionToString(UIChooserNodeDataOptionType_GlobalFavorite);
    const QString strNodeValueDefault = UIChooserAbstractModel::valueToString(UIChooserNodeDataValueType_GlobalDefault);
    return   fFull
           ? QString("%1%2=%3").arg(strNodePrefix).arg(isFavorite() ? strNodeOptionFavorite : "").arg(strNodeValueDefault)
           : QString("%1=%2").arg(strNodePrefix).arg(strNodeValueDefault);
}

bool UIChooserNodeGlobal::hasNodes(UIChooserNodeType enmType /* = UIChooserNodeType_Any */) const
{
    Q_UNUSED(enmType);
    AssertFailedReturn(false);
}

QList<UIChooserNode*> UIChooserNodeGlobal::nodes(UIChooserNodeType enmType /* = UIChooserNodeType_Any */) const
{
    Q_UNUSED(enmType);
    AssertFailedReturn(QList<UIChooserNode*>());
}

void UIChooserNodeGlobal::addNode(UIChooserNode *pNode, int iPosition)
{
    Q_UNUSED(pNode);
    Q_UNUSED(iPosition);
    AssertFailedReturnVoid();
}

void UIChooserNodeGlobal::removeNode(UIChooserNode *pNode)
{
    Q_UNUSED(pNode);
    AssertFailedReturnVoid();
}

void UIChooserNodeGlobal::removeAllNodes(const QUuid &)
{
    // Nothing to remove for global-node..
}

void UIChooserNodeGlobal::updateAllNodes(const QUuid &)
{
    // Nothing to update for global-node..

    /* Update global-item: */
    item()->updateItem();
}

int UIChooserNodeGlobal::positionOf(UIChooserNode *pNode)
{
    Q_UNUSED(pNode);
    AssertFailedReturn(0);
}

void UIChooserNodeGlobal::searchForNodes(const QString &strSearchTerm, int iSearchFlags, QList<UIChooserNode*> &matchedItems)
{
    /* Ignore if we are not searching for the global-node: */
    if (!(iSearchFlags & UIChooserItemSearchFlag_Global))
        return;

    /* If the search term is empty we just add the node to the matched list: */
    if (strSearchTerm.isEmpty())
        matchedItems << this;
    else
    {
        /* If exact name flag specified => check full node name: */
        if (iSearchFlags & UIChooserItemSearchFlag_ExactName)
        {
            if (name() == strSearchTerm)
                matchedItems << this;
        }
        /* Otherwise check if name contains search term: */
        else
        {
            if (name().contains(strSearchTerm, Qt::CaseInsensitive))
                matchedItems << this;
        }
    }
}

void UIChooserNodeGlobal::sortNodes()
{
    AssertFailedReturnVoid();
}

void UIChooserNodeGlobal::retranslateUi()
{
    /* Translate name: */
    m_strName = tr("Tools");

    /* Update global-item: */
    if (item())
        item()->updateItem();
}
