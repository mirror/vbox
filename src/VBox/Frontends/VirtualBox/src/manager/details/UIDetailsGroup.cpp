/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsGroup class implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt include: */
# include <QGraphicsScene>
# include <QPainter>
# include <QStyle>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UIDetailsGroup.h"
# include "UIDetailsModel.h"
# include "UIDetailsSet.h"
# include "UIExtraDataManager.h"
# include "UIVirtualMachineItem.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsGroup::UIDetailsGroup(QGraphicsScene *pParent)
    : UIDetailsItem(0)
    , m_pBuildStep(0)
    , m_iPreviousMinimumWidthHint(0)
    , m_iPreviousMinimumHeightHint(0)
{
    /* Add group to the parent scene: */
    pParent->addItem(this);

    /* Prepare connections: */
    prepareConnections();
}

UIDetailsGroup::~UIDetailsGroup()
{
    /* Cleanup items: */
    clearItems();
}

void UIDetailsGroup::buildGroup(const QList<UIVirtualMachineItem*> &machineItems)
{
    /* Remember passed machine-items: */
    m_machineItems = machineItems;

    /* Cleanup superflous items: */
    bool fCleanupPerformed = m_items.size() > m_machineItems.size();
    while (m_items.size() > m_machineItems.size())
        delete m_items.last();
    if (fCleanupPerformed)
        updateGeometry();

    /* Start building group: */
    rebuildGroup();
}

void UIDetailsGroup::rebuildGroup()
{
    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Generate new group-id: */
    m_strGroupId = QUuid::createUuid().toString();

    /* Request to build first step: */
    emit sigBuildStep(m_strGroupId, 0);
}

void UIDetailsGroup::stopBuildingGroup()
{
    /* Generate new group-id: */
    m_strGroupId = QUuid::createUuid().toString();
}

QList<UIDetailsItem*> UIDetailsGroup::items(UIDetailsItemType enmType /* = UIDetailsItemType_Set */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Set: return m_items;
        case UIDetailsItemType_Any: return items(UIDetailsItemType_Set);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIDetailsItem*>();
}

void UIDetailsGroup::updateLayout()
{
    /* Prepare variables: */
    const int iMargin = data(GroupData_Margin).toInt();
    const int iSpacing = data(GroupData_Spacing).toInt();
    const int iMaximumWidth = geometry().size().toSize().width();
    int iVerticalIndent = iMargin;

    /* Layout all the sets: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Ignore sets with no details: */
        if (UIDetailsSet *pSetItem = pItem->toSet())
            if (!pSetItem->hasDetails())
                continue;
        /* Move set: */
        pItem->setPos(0, iVerticalIndent);
        /* Resize set: */
        pItem->resize(iMaximumWidth, pItem->minimumHeightHint());
        /* Layout set content: */
        pItem->updateLayout();
        /* Advance indent: */
        iVerticalIndent += (pItem->minimumHeightHint() + iSpacing);
    }
}

int UIDetailsGroup::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMinimumWidthHint = 0;

    /* For each the set we have: */
    bool fHasItems = false;
    foreach (UIDetailsItem *pItem, items())
    {
        /* Ignore which are with no details: */
        if (UIDetailsSet *pSetItem = pItem->toSet())
            if (!pSetItem->hasDetails())
                continue;
        /* And take into account all the others: */
        iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
        if (!fHasItems)
            fHasItems = true;
    }

    /* Return result: */
    return iMinimumWidthHint;
}

int UIDetailsGroup::minimumHeightHint() const
{
    /* Prepare variables: */
    const int iMargin = data(GroupData_Margin).toInt();
    const int iSpacing = data(GroupData_Spacing).toInt();
    int iMinimumHeightHint = 0;
    bool fHasItems = false;

    /* For each the set we have: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Ignore which are with no details: */
        if (UIDetailsSet *pSetItem = pItem->toSet())
            if (!pSetItem->hasDetails())
                continue;
        /* And take into account all the others: */
        iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
        if (!fHasItems)
            fHasItems = true;
    }
    /* Minus last spacing: */
    if (fHasItems)
        iMinimumHeightHint -= iSpacing;

    /* Add two margins finally: */
    if (fHasItems)
        iMinimumHeightHint += 2 * iMargin;

    /* Return result: */
    return iMinimumHeightHint;
}

void UIDetailsGroup::sltBuildStep(QString strStepId, int iStepNumber)
{
    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Is step id valid? */
    if (strStepId != m_strGroupId)
        return;

    /* Step number feats the bounds: */
    if (iStepNumber >= 0 && iStepNumber < m_machineItems.size())
    {
        /* Should we create a new set for this step? */
        UIDetailsSet *pSet = 0;
        if (iStepNumber > m_items.size() - 1)
            pSet = new UIDetailsSet(this);
        /* Or use existing? */
        else
            pSet = m_items.at(iStepNumber)->toSet();

        /* Create next build-step: */
        m_pBuildStep = new UIPrepareStep(this, pSet, strStepId, iStepNumber + 1);

        /* Build set: */
        pSet->buildSet(m_machineItems[iStepNumber], m_machineItems.size() == 1, model()->settings());
    }
    else
    {
        /* Notify listener about build done: */
        emit sigBuildDone();
    }
}

void UIDetailsGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *)
{
    /* Paint background: */
    paintBackground(pPainter, pOptions);
}

void UIDetailsGroup::addItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Set: m_items.append(pItem); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

void UIDetailsGroup::removeItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Set: m_items.removeAt(m_items.indexOf(pItem)); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

bool UIDetailsGroup::hasItems(UIDetailsItemType enmType /* = UIDetailsItemType_Set */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Set: return !m_items.isEmpty();
        case UIDetailsItemType_Any: return hasItems(UIDetailsItemType_Set);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIDetailsGroup::clearItems(UIDetailsItemType enmType /* = UIDetailsItemType_Set */)
{
    switch (enmType)
    {
        case UIDetailsItemType_Set: while (!m_items.isEmpty()) { delete m_items.last(); } break;
        case UIDetailsItemType_Any: clearItems(UIDetailsItemType_Set); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

void UIDetailsGroup::updateGeometry()
{
    /* Call to base class: */
    UIDetailsItem::updateGeometry();

    /* Group-item should notify details-view if minimum-width-hint was changed: */
    int iMinimumWidthHint = minimumWidthHint();
    if (m_iPreviousMinimumWidthHint != iMinimumWidthHint)
    {
        /* Save new minimum-width-hint, notify listener: */
        m_iPreviousMinimumWidthHint = iMinimumWidthHint;
        emit sigMinimumWidthHintChanged(m_iPreviousMinimumWidthHint);
    }
    /* Group-item should notify details-view if minimum-height-hint was changed: */
    int iMinimumHeightHint = minimumHeightHint();
    if (m_iPreviousMinimumHeightHint != iMinimumHeightHint)
    {
        /* Save new minimum-height-hint, notify listener: */
        m_iPreviousMinimumHeightHint = iMinimumHeightHint;
        emit sigMinimumHeightHintChanged(m_iPreviousMinimumHeightHint);
    }
}

void UIDetailsGroup::prepareConnections()
{
    /* Prepare group-item connections: */
    connect(this, SIGNAL(sigMinimumWidthHintChanged(int)),
            model(), SIGNAL(sigRootItemMinimumWidthHintChanged(int)));
    connect(this, SIGNAL(sigMinimumHeightHintChanged(int)),
            model(), SIGNAL(sigRootItemMinimumHeightHintChanged(int)));
}

QVariant UIDetailsGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupData_Margin: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 6;
        case GroupData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 6;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIDetailsGroup::paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const QRect optionRect = pOptions->rect;

    /* Paint default background: */
    const QColor defaultColor = palette().color(QPalette::Active, QPalette::Midlight).darker(110);
    pPainter->fillRect(optionRect, defaultColor);

    /* Restore painter: */
    pPainter->restore();
}
