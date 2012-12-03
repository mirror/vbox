/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsGroup class implementation
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

/* GUI includes: */
#include "UIGDetailsGroup.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsModel.h"
#include "UIConverter.h"
#include "VBoxGlobal.h"

UIGDetailsGroup::UIGDetailsGroup()
    : UIGDetailsItem(0)
    , m_pStep(0)
    , m_iStep(0)
{
    /* Prepare connections: */
    prepareConnections();
}

UIGDetailsGroup::~UIGDetailsGroup()
{
    /* Cleanup items: */
    clearItems();
}

void UIGDetailsGroup::setItems(const QList<UIVMItem*> &items)
{
    prepareSets(items);
}

void UIGDetailsGroup::updateItems()
{
    updateSets();
}

void UIGDetailsGroup::stopPopulatingItems()
{
    /* Generate new group-id: */
    m_strGroupId = QUuid::createUuid().toString();
}

void UIGDetailsGroup::sltFirstStep(QString strGroupId)
{
    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

    /* Is step id valid? */
    if (strGroupId != m_strGroupId)
        return;

    /* Prepare first set: */
    m_iStep = 0;
    prepareSet(strGroupId);
}

void UIGDetailsGroup::sltNextStep(QString strGroupId)
{
    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

    /* Is step id valid? */
    if (strGroupId != m_strGroupId)
        return;

    /* Prepare next set: */
    ++m_iStep;
    prepareSet(strGroupId);
}

QVariant UIGDetailsGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupData_Margin: return 2;
        case GroupData_Spacing: return 10;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGDetailsGroup::addItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Set: m_items.append(pItem); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

void UIGDetailsGroup::removeItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Set: m_items.removeAt(m_items.indexOf(pItem)); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

QList<UIGDetailsItem*> UIGDetailsGroup::items(UIGDetailsItemType type /* = UIGDetailsItemType_Set */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Set: return m_items;
        case UIGDetailsItemType_Any: return items(UIGDetailsItemType_Set);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIGDetailsItem*>();
}

bool UIGDetailsGroup::hasItems(UIGDetailsItemType type /* = UIGDetailsItemType_Set */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Set: return !m_items.isEmpty();
        case UIGDetailsItemType_Any: return hasItems(UIGDetailsItemType_Set);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIGDetailsGroup::clearItems(UIGDetailsItemType type /* = UIGDetailsItemType_Set */)
{
    switch (type)
    {
        case UIGDetailsItemType_Set: while (!m_items.isEmpty()) { delete m_items.last(); } break;
        case UIGDetailsItemType_Any: clearItems(UIGDetailsItemType_Set); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

void UIGDetailsGroup::prepareConnections()
{
    connect(this, SIGNAL(sigStartFirstStep(QString)), this, SLOT(sltFirstStep(QString)), Qt::QueuedConnection);
}

void UIGDetailsGroup::loadSettings()
{
    /* Load settings: */
    m_settings = vboxGlobal().virtualBox().GetExtraDataStringList(GUI_DetailsPageBoxes);
    /* If settings are empty: */
    if (m_settings.isEmpty())
    {
        /* Propose the defaults: */
        m_settings << gpConverter->toInternalString(DetailsElementType_General);
        m_settings << gpConverter->toInternalString(DetailsElementType_Preview);
        m_settings << gpConverter->toInternalString(DetailsElementType_System);
        m_settings << gpConverter->toInternalString(DetailsElementType_Display);
        m_settings << gpConverter->toInternalString(DetailsElementType_Storage);
        m_settings << gpConverter->toInternalString(DetailsElementType_Audio);
        m_settings << gpConverter->toInternalString(DetailsElementType_Network);
        m_settings << gpConverter->toInternalString(DetailsElementType_USB);
        m_settings << gpConverter->toInternalString(DetailsElementType_SF);
        m_settings << gpConverter->toInternalString(DetailsElementType_Description);
        vboxGlobal().virtualBox().SetExtraDataStringList(GUI_DetailsPageBoxes, m_settings);
    }
}

void UIGDetailsGroup::prepareSets(const QList<UIVMItem*> &machineItems)
{
    /* Remember new items: */
    m_machineItems = machineItems;

    /* Cleanup superflous sets: */
    while (m_items.size() > m_machineItems.size())
        delete m_items.last();

    /* Update sets: */
    updateSets();
}

void UIGDetailsGroup::updateSets()
{
    /* Load settings: */
    loadSettings();

    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

    /* Generate new group-id: */
    m_strGroupId = QUuid::createUuid().toString();

    /* Prepare first set: */
    emit sigStartFirstStep(m_strGroupId);
}

void UIGDetailsGroup::prepareSet(QString strGroupId)
{
    /* Step number feats the bounds: */
    if (m_iStep >= 0 && m_iStep < m_machineItems.size())
    {
        /* Should we create set? */
        UIGDetailsSet *pSet = 0;
        if (m_iStep > m_items.size() - 1)
            pSet = new UIGDetailsSet(this);
        else
            pSet = m_items.at(m_iStep)->toSet();
        /* Create prepare step: */
        m_pStep = new UIPrepareStep(this, strGroupId);
        connect(pSet, SIGNAL(sigSetCreationDone()), m_pStep, SLOT(sltStepDone()), Qt::QueuedConnection);
        connect(m_pStep, SIGNAL(sigStepDone(const QString&)), this, SLOT(sltNextStep(const QString&)), Qt::QueuedConnection);
        /* Configure set: */
        pSet->configure(m_machineItems[m_iStep], m_settings, m_machineItems.size() == 1);
    }
    else
    {
        /* Update model after group update: */
        model()->updateLayout();
    }
}

int UIGDetailsGroup::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(GroupData_Margin).toInt();
    int iMinimumWidthHint = 0;

    /* Take into account all the sets: */
    foreach (UIGDetailsItem *pItem, items())
        iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());

    /* And two margins finally: */
    iMinimumWidthHint += 2 * iMargin;

    /* Return result: */
    return iMinimumWidthHint;
}

int UIGDetailsGroup::minimumHeightHint() const
{
    /* Prepare variables: */
    int iMargin = data(GroupData_Margin).toInt();
    int iSpacing = data(GroupData_Spacing).toInt();
    int iMinimumHeightHint = 0;

    /* Take into account all the sets: */
    foreach (UIGDetailsItem *pItem, items())
        iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);

    /* Minus last spacing: */
    iMinimumHeightHint -= iSpacing;

    /* And two margins finally: */
    iMinimumHeightHint += 2 * iMargin;

    /* Return result: */
    return iMinimumHeightHint;
}

void UIGDetailsGroup::updateLayout()
{
    /* Update size-hints for all the children: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateSizeHint();

    /* Prepare variables: */
    int iMargin = data(GroupData_Margin).toInt();
    int iSpacing = data(GroupData_Spacing).toInt();
    int iMaximumWidth = (int)geometry().width() - 2 * iMargin;
    int iVerticalIndent = iMargin;

    /* Layout all the sets: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Move set: */
        pItem->setPos(iMargin, iVerticalIndent);
        /* Resize set: */
        int iWidth = iMaximumWidth;
        pItem->resize(iWidth, pItem->minimumHeightHint());
        /* Layout set content: */
        pItem->updateLayout();
        /* Advance indent: */
        iVerticalIndent += (pItem->minimumHeightHint() + iSpacing);
    }
}

