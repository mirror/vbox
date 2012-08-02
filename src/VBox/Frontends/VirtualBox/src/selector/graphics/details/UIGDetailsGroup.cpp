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

/* Qt includes: */
#include <QGraphicsLinearLayout>

/* GUI includes: */
#include "UIGDetailsGroup.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsModel.h"
#include "UIConverter.h"
#include "VBoxGlobal.h"

/* Other VBox includes: */
#include <iprt/assert.h>

UIGDetailsGroup::UIGDetailsGroup()
    : UIGDetailsItem(0)
    , m_pMainLayout(0)
    , m_pLayout(0)
    , m_pStep(0)
    , m_iStep(0)
{
    /* Prepare layout: */
    prepareLayout();

    /* Prepare connections: */
    connect(this, SIGNAL(sigStartFirstStep(QString)), this, SLOT(sltFirstStep(QString)), Qt::QueuedConnection);
}

UIGDetailsGroup::~UIGDetailsGroup()
{
    /* Clear items: */
    clearItems();
}

void UIGDetailsGroup::setItems(const QList<UIVMItem*> &items)
{
    prepareSets(items);
}

void UIGDetailsGroup::rebuildItems()
{
    recreateSets();
}

void UIGDetailsGroup::addItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Set: m_sets.append(pItem); m_pLayout->addItem(pItem); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

void UIGDetailsGroup::removeItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Set: m_sets.removeAt(m_sets.indexOf(pItem)); m_pLayout->removeItem(pItem); break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

QList<UIGDetailsItem*> UIGDetailsGroup::items(UIGDetailsItemType type /* = UIGDetailsItemType_Set */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Any: return items(UIGDetailsItemType_Set);
        case UIGDetailsItemType_Set: return m_sets;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIGDetailsItem*>();
}

bool UIGDetailsGroup::hasItems(UIGDetailsItemType type /* = UIGDetailsItemType_Set */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Any: return hasItems(UIGDetailsItemType_Set);
        case UIGDetailsItemType_Set: return !m_sets.isEmpty();
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIGDetailsGroup::clearItems(UIGDetailsItemType type /* = UIGDetailsItemType_Set */)
{
    switch (type)
    {
        case UIGDetailsItemType_Any: clearItems(UIGDetailsItemType_Set); break;
        case UIGDetailsItemType_Set: while (!m_sets.isEmpty()) { delete m_sets.last(); } break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
}

QVariant UIGDetailsGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupData_Margin: return 1;
        case GroupData_Spacing: return 10;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGDetailsGroup::updateSizeHint()
{
    /* Update size-hints for all the items: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateGeometry();
}

void UIGDetailsGroup::updateLayout()
{
    m_pMainLayout->activate();
    m_pLayout->activate();
    foreach (UIGDetailsItem *pItem, items())
        pItem->updateLayout();
}

void UIGDetailsGroup::sltFirstStep(QString strId)
{
    /* Prepare first set: */
    if (strId == m_strCurrentId)
    {
        m_iStep = 0;
        prepareSet();
    }
}

void UIGDetailsGroup::sltStepDone()
{
    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

    /* Prepare next set: */
    ++m_iStep;
    prepareSet();
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

void UIGDetailsGroup::prepareLayout()
{
    /* Prepare variables: */
    int iMargin = data(GroupData_Margin).toInt();
    int iSpacing = data(GroupData_Spacing).toInt();

    /* Prepare layout: */
    m_pMainLayout = new QGraphicsLinearLayout(Qt::Vertical);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(0);
    m_pLayout = new QGraphicsLinearLayout(Qt::Vertical);
    m_pLayout->setContentsMargins(iMargin, iMargin, iMargin, iMargin);
    m_pLayout->setSpacing(iSpacing);
    m_pMainLayout->addItem(m_pLayout);
    m_pMainLayout->addStretch();
    setLayout(m_pMainLayout);
}

void UIGDetailsGroup::prepareSets(const QList<UIVMItem*> &items)
{
    /* Clear sets first: */
    clearItems();
    /* Clear step: */
    if (m_pStep)
    {
        delete m_pStep;
        m_pStep = 0;
    }

    /* Load settings: */
    loadSettings();

    /* Remember new items: */
    m_items = items;

    /* Prepare first set: */
    m_strCurrentId = QUuid::createUuid().toString();
    emit sigStartFirstStep(m_strCurrentId);
}

void UIGDetailsGroup::recreateSets()
{
    /* Clear sets first: */
    clearItems();
    /* Clear step: */
    if (m_pStep)
    {
        delete m_pStep;
        m_pStep = 0;
    }

    /* Load settings: */
    loadSettings();

    /* Prepare first set: */
    m_strCurrentId = QUuid::createUuid().toString();
    emit sigStartFirstStep(m_strCurrentId);
}

void UIGDetailsGroup::prepareSet()
{
    /* Step number feats the bounds: */
    if (m_iStep >= 0 && m_iStep < m_items.size())
    {
        /* Create prepare step & set: */
        m_pStep = new UIPrepareStep(this);
        UIGDetailsSet *pSet = new UIGDetailsSet(this, m_items[m_iStep], m_settings, m_items.size() == 1);
        connect(pSet, SIGNAL(sigSetCreationDone()), m_pStep, SLOT(sltStepDone()), Qt::QueuedConnection);
        model()->updateLayout();
    }
}

