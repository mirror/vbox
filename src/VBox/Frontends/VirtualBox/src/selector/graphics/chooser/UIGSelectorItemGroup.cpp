/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItemGroup class implementation
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
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneDragDropEvent>
#include <QPropertyAnimation>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

/* GUI includes: */
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"
#include "UIGraphicsSelectorModel.h"
#include "UIIconPool.h"
#include "UIGraphicsRotatorButton.h"

/* static */
QString UIGSelectorItemGroup::className() { return "UIGSelectorItemGroup"; }

UIGSelectorItemGroup::UIGSelectorItemGroup(UIGSelectorItem *pParent /* = 0 */,
                                           const QString &strName /* = QString() */,
                                           int iPosition /* = -1 */)
    : UIGSelectorItem(pParent)
    , m_strName(strName)
    , m_fClosed(parentItem() ? true : false)
    , m_pButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    if (parentItem())
        parentItem()->addItem(this, iPosition);
}

UIGSelectorItemGroup::UIGSelectorItemGroup(UIGSelectorItem *pParent,
                                           UIGSelectorItemGroup *pCopyFrom,
                                           int iPosition /* = -1 */)
    : UIGSelectorItem(pParent)
    , m_strName(pCopyFrom->name())
    , m_fClosed(pCopyFrom->closed())
    , m_pButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
{
    /* Prepare: */
    prepare();

    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);

    /* Add item to the parent: */
    if (parentItem())
        parentItem()->addItem(this, iPosition);
}

UIGSelectorItemGroup::~UIGSelectorItemGroup()
{
    /* Delete all the items: */
    clearItems();

    /* Remove item from the parent: */
    if (parentItem())
        parentItem()->removeItem(this);
}

QString UIGSelectorItemGroup::name() const
{
    return m_strName;
}

bool UIGSelectorItemGroup::closed() const
{
    return m_fClosed;
}

bool UIGSelectorItemGroup::opened() const
{
    return !m_fClosed;
}

void UIGSelectorItemGroup::close()
{
    AssertMsg(parentItem(), ("Can't close root-item!"));
    m_pButton->setToggled(false);
}

void UIGSelectorItemGroup::open()
{
    AssertMsg(parentItem(), ("Can't open root-item!"));
    m_pButton->setToggled(true);
}

void UIGSelectorItemGroup::sltOpen()
{
    AssertMsg(parentItem(), ("Can't open root-item!"));
    m_pButton->setToggled(true, false);
}

bool UIGSelectorItemGroup::contains(const QString &strId, bool fRecursively /* = false */) const
{
    /* Check machine items: */
    foreach (UIGSelectorItem *pItem, m_machineItems)
        if (pItem->toMachineItem()->id() == strId)
            return true;
    /* If recursively => check group items: */
    if (fRecursively)
        foreach (UIGSelectorItem *pItem, m_groupItems)
            if (pItem->toGroupItem()->contains(strId, fRecursively))
                return true;
    return false;
}

void UIGSelectorItemGroup::sltNameEditingFinished()
{
    /* Lock name-editor: */
    m_pNameEditor->hide();

    /* Update group name: */
    QString strNewName = m_pNameEditorWidget->text();
    if (!strNewName.isEmpty())
        m_strName = strNewName;
}

void UIGSelectorItemGroup::sltGroupToggleStart()
{
    /* Not for root-item: */
    AssertMsg(parentItem(), ("Can't toggle root-item!"));

    /* Setup animation: */
    updateAnimationParameters();

    /* Group closed, we are opening it: */
    if (m_fClosed)
    {
        /* Toggle-state and navigation will be
         * updated on toggle finish signal! */
    }
    /* Group opened, we are closing it: */
    else
    {
        /* Update toggle-state: */
        m_fClosed = true;
        /* Update navigation: */
        model()->updateNavigation();
        /* Relayout model: */
        model()->updateLayout();
    }
}

void UIGSelectorItemGroup::sltGroupToggleFinish(bool fToggled)
{
    /* Not for root-item: */
    AssertMsg(parentItem(), ("Can't toggle root-item!"));

    /* Update toggle-state: */
    m_fClosed = !fToggled;
    /* Update navigation: */
    model()->updateNavigation();
    /* Relayout model: */
    model()->updateLayout();
}

QVariant UIGSelectorItemGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupItemData_HorizonalMargin: return parentItem() ? 8 : 1;
        case GroupItemData_VerticalMargin: return parentItem() ? 2 : 1;
        case GroupItemData_MajorSpacing: return parentItem() ? 10 : 0;
        case GroupItemData_MinorSpacing: return 1;
        /* Pixmaps: */
        case GroupItemData_ButtonPixmap: return UIIconPool::iconSet(":/arrow_right_10px.png");
        case GroupItemData_GroupPixmap: return UIIconPool::iconSet(":/nw_16px.png");
        case GroupItemData_MachinePixmap: return UIIconPool::iconSet(":/machine_16px.png");
        /* Fonts: */
        case GroupItemData_NameFont:
        {
            QFont nameFont = font();
            nameFont.setWeight(QFont::Bold);
            return nameFont;
        }
        case GroupItemData_InfoFont:
        {
            QFont infoFont = font();
            return infoFont;
        }
        /* Texts: */
        case GroupItemData_Name: return m_strName;
        case GroupItemData_GroupCountText: return QString::number(m_groupItems.size());
        case GroupItemData_MachineCountText: return QString::number(m_machineItems.size());
        /* Sizes: */
        case GroupItemData_ButtonSize: return m_pButton ? m_pButton->minimumSizeHint() : QSizeF(0, 0);
        case GroupItemData_NameSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_NameFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_Name).toString()), fm.height());
        }
        case GroupItemData_NameEditorSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            return m_pNameEditorWidget->minimumSizeHint();
        }
        case GroupItemData_GroupPixmapSize:
            return parentItem() ? data(GroupItemData_GroupPixmap).value<QIcon>().availableSizes().at(0) : QSizeF(0, 0);
        case GroupItemData_MachinePixmapSize:
            return parentItem() ? data(GroupItemData_MachinePixmap).value<QIcon>().availableSizes().at(0) : QSizeF(0, 0);
        case GroupItemData_GroupCountTextSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_InfoFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_GroupCountText).toString()), fm.height());
        }
        case GroupItemData_MachineCountTextSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_InfoFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_MachineCountText).toString()), fm.height());
        }
        case GroupItemData_FullHeaderSize:
        {
            /* Prepare variables: */
            int iMajorSpacing = data(GroupItemData_MajorSpacing).toInt();
            int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
            QSize buttonSize = data(GroupItemData_ButtonSize).toSize();
            QSize nameSize = data(GroupItemData_NameSize).toSize();
            QSize nameEditorSize = data(GroupItemData_NameEditorSize).toSize();
            QSize groupPixmapSize = data(GroupItemData_GroupPixmapSize).toSize();
            QSize machinePixmapSize = data(GroupItemData_MachinePixmapSize).toSize();
            QSize groupCountTextSize = data(GroupItemData_GroupCountTextSize).toSize();
            QSize machineCountTextSize = data(GroupItemData_MachineCountTextSize).toSize();

            /* Calculate minimum width: */
            int iGroupItemHeaderWidth = /* Button width: */
                                        buttonSize.width() +
                                        /* Spacing between button and name: */
                                        iMajorSpacing +
                                        /* Some hardcoded magic: */
                                        1 /* frame width from Qt sources */ +
                                        2 /* internal QLineEdit margin from Qt sources */ +
                                        1 /* internal QLineEdit align shifting from Qt sources */ +
                                        /* Name width: */
                                        nameSize.width() +
                                        /* Spacing between name and info: */
                                        iMajorSpacing +
                                        /* Group stuff width: */
                                        groupPixmapSize.width() + iMinorSpacing +
                                        groupCountTextSize.width() + iMinorSpacing +
                                        /* Machine stuff width: */
                                        machinePixmapSize.width() + iMinorSpacing +
                                        machineCountTextSize.width();

            /* Search for maximum height: */
            QList<int> heights;
            heights << buttonSize.height()
                    << nameSize.height() << nameEditorSize.height()
                    << groupPixmapSize.height() << machinePixmapSize.height()
                    << groupCountTextSize.height() << machineCountTextSize.height();
            int iGroupItemHeaderHeight = 0;
            foreach (int iHeight, heights)
                iGroupItemHeaderHeight = qMax(iGroupItemHeaderHeight, iHeight);

            /* Return result: */
            return QSize(iGroupItemHeaderWidth, iGroupItemHeaderHeight);
        }
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGSelectorItemGroup::startEditing()
{
    /* Not for root-item: */
    AssertMsg(parentItem(), ("Can't edit root-item!"));
    /* Unlock name-editor: */
    m_pNameEditor->show();
    m_pNameEditorWidget->setFocus();
}

void UIGSelectorItemGroup::addItem(UIGSelectorItem *pItem, int iPosition)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGSelectorItemType_Group:
        {
            if (iPosition < 0 || iPosition >= m_groupItems.size())
                m_groupItems.append(pItem);
            else
                m_groupItems.insert(iPosition, pItem);
            break;
        }
        case UIGSelectorItemType_Machine:
        {
            if (iPosition < 0 || iPosition >= m_machineItems.size())
                m_machineItems.append(pItem);
            else
                m_machineItems.insert(iPosition, pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

void UIGSelectorItemGroup::removeItem(UIGSelectorItem *pItem)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGSelectorItemType_Group:
        {
            m_groupItems.removeAt(m_groupItems.indexOf(pItem));
            break;
        }
        case UIGSelectorItemType_Machine:
        {
            m_machineItems.removeAt(m_machineItems.indexOf(pItem));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

void UIGSelectorItemGroup::moveItemTo(UIGSelectorItem *pItem, int iPosition)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGSelectorItemType_Group:
        {
            AssertMsg(m_groupItems.contains(pItem), ("Passed item is not our child!"));
            m_groupItems.removeAt(m_groupItems.indexOf(pItem));
            if (iPosition < 0 || iPosition >= m_groupItems.size())
                m_groupItems.append(pItem);
            else
                m_groupItems.insert(iPosition, pItem);
            break;
        }
        case UIGSelectorItemType_Machine:
        {
            AssertMsg(m_machineItems.contains(pItem), ("Passed item is not our child!"));
            m_machineItems.removeAt(m_machineItems.indexOf(pItem));
            if (iPosition < 0 || iPosition >= m_machineItems.size())
                m_machineItems.append(pItem);
            else
                m_machineItems.insert(iPosition, pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

QList<UIGSelectorItem*> UIGSelectorItemGroup::items(UIGSelectorItemType type) const
{
    switch (type)
    {
        case UIGSelectorItemType_Any: return items(UIGSelectorItemType_Group) + items(UIGSelectorItemType_Machine);
        case UIGSelectorItemType_Group: return m_groupItems;
        case UIGSelectorItemType_Machine: return m_machineItems;
        default: break;
    }
    return QList<UIGSelectorItem*>();
}

bool UIGSelectorItemGroup::hasItems(UIGSelectorItemType type /* = UIGSelectorItemType_Any */) const
{
    switch (type)
    {
        case UIGSelectorItemType_Any:
            return hasItems(UIGSelectorItemType_Group) || hasItems(UIGSelectorItemType_Machine);
        case UIGSelectorItemType_Group:
            return !m_groupItems.isEmpty();
        case UIGSelectorItemType_Machine:
            return !m_machineItems.isEmpty();
    }
    return false;
}

void UIGSelectorItemGroup::clearItems(UIGSelectorItemType type /* = UIGSelectorItemType_Any */)
{
    switch (type)
    {
        case UIGSelectorItemType_Any:
        {
            clearItems(UIGSelectorItemType_Group);
            clearItems(UIGSelectorItemType_Machine);
            break;
        }
        case UIGSelectorItemType_Group:
        {
            while (!m_groupItems.isEmpty()) { delete m_groupItems.last(); }
            break;
        }
        case UIGSelectorItemType_Machine:
        {
            while (!m_machineItems.isEmpty()) { delete m_machineItems.last(); }
            break;
        }
    }
}

void UIGSelectorItemGroup::updateSizeHint()
{
    /* Update size-hints for all the items: */
    foreach (UIGSelectorItem *pItem, items())
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateGeometry();
}

void UIGSelectorItemGroup::updateLayout()
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iButtonHeight = data(GroupItemData_ButtonSize).toSize().height();
    int iButtonWidth = data(GroupItemData_ButtonSize).toSize().width();
    int iNameEditorHeight = data(GroupItemData_NameEditorSize).toSize().height();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
    int iPreviousVerticalIndent = 0;

    /* Header (non-root item): */
    if (parentItem())
    {
        /* Layout button: */
        int iButtonX = iHorizontalMargin;
        int iButtonY = iButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                       iVerticalMargin + (iFullHeaderHeight - iButtonHeight) / 2;
        m_pButton->setPos(iButtonX, iButtonY);

        /* Layout name-editor: */
        int iNameEditorX = iHorizontalMargin + iButtonWidth + iMinorSpacing;
        int iNameEditorY = iNameEditorHeight == iFullHeaderHeight ? iVerticalMargin :
                           iVerticalMargin + (iFullHeaderHeight - iNameEditorHeight) / 2;
        m_pNameEditor->setPos(iNameEditorX, iNameEditorY);
        m_pNameEditorWidget->resize(geometry().width() - iNameEditorX - iHorizontalMargin, m_pNameEditorWidget->height());

        /* Prepare body indent: */
        iPreviousVerticalIndent = 2 * iVerticalMargin + iFullHeaderHeight;
    }
    /* Header (root item): */
    else
    {
        /* Prepare body indent: */
        iPreviousVerticalIndent = iVerticalMargin;
    }

    /* No body for closed group: */
    if (m_fClosed)
    {
        /* Hide all the items: */
        foreach (UIGSelectorItem *pItem, items())
            if (pItem->isVisible())
                pItem->hide();
    }
    /* Body for opened group: */
    else
    {
        /* Prepare variables: */
        int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
        QSize size = geometry().size().toSize();
        /* Layout all the items: */
        foreach (UIGSelectorItem *pItem, items())
        {
            /* Show if hidden: */
            if (!pItem->isVisible())
                pItem->show();
            /* Get item's height-hint: */
            int iMinimumHeight = pItem->minimumHeightHint();
            /* Set item's position: */
            pItem->setPos(iHorizontalMargin, iPreviousVerticalIndent);
            /* Set item's size: */
            pItem->resize(size.width() - 2 * iHorizontalMargin, iMinimumHeight);
            /* Relayout group: */
            pItem->updateLayout();
            /* Update indent for next items: */
            iPreviousVerticalIndent += (iMinimumHeight + iMinorSpacing);
        }
    }
}

int UIGSelectorItemGroup::minimumWidthHint(bool fClosedGroup) const
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    QSize fullHeaderSize = data(GroupItemData_FullHeaderSize).toSize();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Simple group item have 2 margins - left and right: */
    iProposedWidth += 2 * iHorizontalMargin;
    /* And full header width to take into account: */
    iProposedWidth += fullHeaderSize.width();
    /* But if group is opened: */
    if (!fClosedGroup)
    {
        /* We have to make sure that we had taken into account: */
        foreach (UIGSelectorItem *pItem, items())
        {
            int iItemWidth = 2 * iHorizontalMargin + pItem->minimumWidthHint();
            iProposedWidth = qMax(iProposedWidth, iItemWidth);
        }
    }

    /* Return result: */
    return iProposedWidth;
}

int UIGSelectorItemGroup::minimumHeightHint(bool fClosedGroup) const
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    QSize fullHeaderSize = data(GroupItemData_FullHeaderSize).toSize();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Simple group item have 2 margins - top and bottom: */
    iProposedHeight += 2 * iVerticalMargin;
    /* And full header height to take into account: */
    iProposedHeight += fullHeaderSize.height();
    /* But if group is opened: */
    if (!fClosedGroup)
    {
        /* We should take into account: */
        for (int i = 0; i < m_groupItems.size(); ++i)
        {
            /* Every group item height: */
            UIGSelectorItem *pItem = m_groupItems[i];
            iProposedHeight += pItem->minimumHeightHint();
            /* And every spacing between sub-groups: */
            if (i < m_groupItems.size() - 1)
                iProposedHeight += iMinorSpacing;
        }
        /* Minor spacing between group and machine item: */
        if (hasItems(UIGSelectorItemType_Group) && hasItems(UIGSelectorItemType_Machine))
            iProposedHeight += iMinorSpacing;
        for (int i = 0; i < m_machineItems.size(); ++i)
        {
            /* Every machine item height: */
            UIGSelectorItem *pItem = m_machineItems[i];
            iProposedHeight += pItem->minimumHeightHint();
            /* And every spacing between sub-machines: */
            if (i < m_machineItems.size() - 1)
                iProposedHeight += iMinorSpacing;
        }
        /* Bottom margin: */
        iProposedHeight += iHorizontalMargin;
    }
    /* Finally, additional height during animation: */
    if (fClosedGroup && m_pButton && m_pButton->isAnimationRunning())
        iProposedHeight += m_iAdditionalHeight;

    /* Return result: */
    return iProposedHeight;
}

int UIGSelectorItemGroup::minimumWidthHint() const
{
    return minimumWidthHint(m_fClosed);
}

int UIGSelectorItemGroup::minimumHeightHint() const
{
    return minimumHeightHint(m_fClosed);
}

QSizeF UIGSelectorItemGroup::minimumSizeHint(bool fClosedGroup) const
{
    return QSizeF(minimumWidthHint(fClosedGroup), minimumHeightHint(fClosedGroup));
}

QSizeF UIGSelectorItemGroup::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
        return minimumSizeHint(m_fClosed);
    /* Else call to base-class: */
    return UIGSelectorItem::sizeHint(which, constraint);
}

QPixmap UIGSelectorItemGroup::toPixmap()
{
    QSize minimumSize = minimumSizeHint(true).toSize();
    QPixmap pixmap(minimumSize);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSize);
    paint(&painter, &options, true);
    return pixmap;
}

bool UIGSelectorItemGroup::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const
{
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should check mime format: */
    if (pMimeData->hasFormat(UIGSelectorItemGroup::className()))
    {
        /* Get passed group item: */
        const UIGSelectorItemMimeData *pCastedMimeData = qobject_cast<const UIGSelectorItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGSelectorItemMimeData!"));
        UIGSelectorItem *pItem = pCastedMimeData->item();
        /* Make sure passed group is not 'this': */
        if (pItem == this)
            return false;
        /* Make sure passed group is not among our parents: */
        const UIGSelectorItem *pTestedWidget = this;
        while (UIGSelectorItem *pParentOfTestedWidget = pTestedWidget->parentItem())
        {
            if (pItem == pParentOfTestedWidget)
                return false;
            pTestedWidget = pParentOfTestedWidget;
        }
        return true;
    }
    else if (pMimeData->hasFormat(UIGSelectorItemMachine::className()))
    {
        /* Get passed machine item: */
        const UIGSelectorItemMimeData *pCastedMimeData = qobject_cast<const UIGSelectorItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGSelectorItemMimeData!"));
        UIGSelectorItem *pItem = pCastedMimeData->item();
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            {
                /* Make sure passed item is ours or there is no other item with such id: */
                return m_machineItems.contains(pItem) || !contains(pItem->toMachineItem()->id());
            }
            case Qt::CopyAction:
            {
                /* Make sure there is no other item with such id: */
                return !contains(pItem->toMachineItem()->id());
            }
        }
    }
    /* That was invalid mime: */
    return false;
}

void UIGSelectorItemGroup::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho, DragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Check mime format: */
    if (pMime->hasFormat(UIGSelectorItemGroup::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGraphicsSelectorModel *pModel = model();

                /* Get passed group item: */
                const UIGSelectorItemMimeData *pCastedMime = qobject_cast<const UIGSelectorItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGSelectorItemMimeData!"));
                UIGSelectorItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_groupItems.size();
                if (pFromWho && where != DragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_groupItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_groupItems.contains(pFromWho))
                    {
                        iPosition = m_groupItems.indexOf(pFromWho);
                        if (where == DragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed item into this group: */
                UIGSelectorItem *pNewGroupItem = new UIGSelectorItemGroup(this, pItem->toGroupItem(), iPosition);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update scene: */
                pModel->updateGroupTree();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewGroupItem->parentItem()->toGroupItem()->opened() ?
                                       pNewGroupItem : pNewGroupItem->parentItem());
                break;
            }
            default:
                break;
        }
    }
    else if (pMime->hasFormat(UIGSelectorItemMachine::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGraphicsSelectorModel *pModel = model();

                /* Get passed item: */
                const UIGSelectorItemMimeData *pCastedMime = qobject_cast<const UIGSelectorItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGSelectorItemMimeData!"));
                UIGSelectorItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_machineItems.size();
                if (pFromWho && where != DragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_machineItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_machineItems.contains(pFromWho))
                    {
                        iPosition = m_machineItems.indexOf(pFromWho);
                        if (where == DragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed machine item into this group: */
                UIGSelectorItem *pNewMachineItem = new UIGSelectorItemMachine(this, pItem->toMachineItem(), iPosition);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update scene: */
                pModel->updateGroupTree();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewMachineItem->parentItem()->toGroupItem()->opened() ?
                                       pNewMachineItem : pNewMachineItem->parentItem());
                break;
            }
            default:
                break;
        }
    }
}

void UIGSelectorItemGroup::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != DragToken_Off)
    {
        setDragTokenPlace(DragToken_Off);
        update();
    }
    /* Reset drag tokens for all the items: */
    foreach (UIGSelectorItem *pItem, items())
        pItem->resetDragToken();
}

QMimeData* UIGSelectorItemGroup::createMimeData()
{
    return new UIGSelectorItemMimeData(this);
}

void UIGSelectorItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget* /* pWidget = 0 */)
{
    paint(pPainter, pOption, m_fClosed);
}

void UIGSelectorItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup)
{
    /* Non-root item: */
    if (parentItem())
    {
        /* Configure painter shape: */
        configurePainterShape(pPainter, pOption, m_iCornerRadius);
    }

    /* Any item: */
    {
        /* Paint decorations: */
        paintDecorations(pPainter, pOption);
    }

    /* Non-root item: */
    if (parentItem())
    {
        /* Paint group info: */
        paintGroupInfo(pPainter, pOption, fClosedGroup);
    }
}

void UIGSelectorItemGroup::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Prepare variables: */
    QRect fullRect = pOption->rect;

    /* Any item: */
    {
        /* Paint background: */
        paintBackground(/* Painter: */
                        pPainter,
                        /* Rectangle to paint in: */
                        fullRect,
                        /* Has parent? */
                        parentItem(),
                        /* Is item selected? */
                        model()->selectionList().contains(this),
                        /* Gradient darkness for animation: */
                        gradient(),
                        /* Show drag where? */
                        dragTokenPlace(),
                        /* Rounded corners radius: */
                        m_iCornerRadius);
    }

    /* Non-root item: */
    if (parentItem())
    {
        /* Paint frame: */
        paintFrameRect(/* Painter: */
                       pPainter,
                       /* Rectangle to paint in: */
                       fullRect,
                       /* Is item selected? */
                       model()->selectionList().contains(this),
                       /* Rounded corner radius: */
                       m_iCornerRadius);
    }
}

void UIGSelectorItemGroup::paintGroupInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool)
{
    /* Prepare variables: */
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    QSize buttonSize = data(GroupItemData_ButtonSize).toSize();
    QSize nameSize = data(GroupItemData_NameSize).toSize();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();

    /* Paint name: */
    int iNameX = iHorizontalMargin + buttonSize.width() + iMinorSpacing +
                 1 /* frame width from Qt sources */ +
                 2 /* internal QLineEdit margin from Qt sources */ +
                 1 /* internal QLineEdit align shifting from Qt sources */;
    int iNameY = nameSize.height() == iFullHeaderHeight ? iVerticalMargin :
                 iVerticalMargin + (iFullHeaderHeight - nameSize.height()) / 2;
    paintText(/* Painter: */
              pPainter,
              /* Rectangle to paint in: */
              QRect(QPoint(iNameX, iNameY), nameSize),
              /* Font to paint text: */
              data(GroupItemData_NameFont).value<QFont>(),
              /* Text to paint: */
              data(GroupItemData_Name).toString());

    /* Should we add more info? */
    if (model()->selectionList().contains(this))
    {
        /* Prepare variables: */
        QRect fullRect = pOption->rect;
        int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
        QSize groupPixmapSize = data(GroupItemData_GroupPixmapSize).toSize();
        QSize machinePixmapSize = data(GroupItemData_MachinePixmapSize).toSize();
        QSize groupCountTextSize = data(GroupItemData_GroupCountTextSize).toSize();
        QSize machineCountTextSize = data(GroupItemData_MachineCountTextSize).toSize();
        QFont infoFont = data(GroupItemData_InfoFont).value<QFont>();
        QString strGroupCountText = data(GroupItemData_GroupCountText).toString();
        QString strMachineCountText = data(GroupItemData_MachineCountText).toString();
        QPixmap groupPixmap = data(GroupItemData_GroupPixmap).value<QIcon>().pixmap(groupPixmapSize);
        QPixmap machinePixmap = data(GroupItemData_MachinePixmap).value<QIcon>().pixmap(machinePixmapSize);

        /* We should add machine count: */
        int iMachineCountTextX = fullRect.right() -
                                 iHorizontalMargin -
                                 machineCountTextSize.width();
        int iMachineCountTextY = machineCountTextSize.height() == iFullHeaderHeight ?
                                 iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - machineCountTextSize.height()) / 2;
        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(QPoint(iMachineCountTextX, iMachineCountTextY), machineCountTextSize),
                  /* Font to paint text: */
                  infoFont,
                  /* Text to paint: */
                  strMachineCountText);

        /* We should draw machine pixmap: */
        int iMachinePixmapX = iMachineCountTextX -
                              iMinorSpacing -
                              machinePixmapSize.width();
        int iMachinePixmapY = machinePixmapSize.height() == iFullHeaderHeight ?
                              iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - machinePixmapSize.height()) / 2;
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    QRect(QPoint(iMachinePixmapX, iMachinePixmapY), machinePixmapSize),
                    /* Pixmap to paint: */
                    machinePixmap);

        /* We should add group count: */
        int iGroupCountTextX = iMachinePixmapX -
                               iMinorSpacing -
                               groupCountTextSize.width();
        int iGroupCountTextY = groupCountTextSize.height() == iFullHeaderHeight ?
                               iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - groupCountTextSize.height()) / 2;
        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(QPoint(iGroupCountTextX, iGroupCountTextY), groupCountTextSize),
                  /* Font to paint text: */
                  infoFont,
                  /* Text to paint: */
                  strGroupCountText);

        /* We should draw group pixmap: */
        int iGroupPixmapX = iGroupCountTextX -
                            iMinorSpacing -
                            groupPixmapSize.width();
        int iGroupPixmapY = groupPixmapSize.height() == iFullHeaderHeight ?
                            iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - groupPixmapSize.height()) / 2;
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    QRect(QPoint(iGroupPixmapX, iGroupPixmapY), groupPixmapSize),
                    /* Pixmap to paint: */
                    groupPixmap);
    }
}

void UIGSelectorItemGroup::updateAnimationParameters()
{
    /* Recalculate animation parameters: */
    if (m_pButton)
    {
        QSizeF openedSize = minimumSizeHint(false);
        QSizeF closedSize = minimumSizeHint(true);
        int iAdditionalHeight = openedSize.height() - closedSize.height();
        m_pButton->setAnimationRange(0, iAdditionalHeight);
    }
}

void UIGSelectorItemGroup::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
    /* Relayout scene: */
    model()->updateLayout();
}

int UIGSelectorItemGroup::additionalHeight() const
{
    return m_iAdditionalHeight;
}

void UIGSelectorItemGroup::prepare()
{
    /* Nothing for root: */
    if (!parentItem())
        return;

    /* Setup toggle-button: */
    m_pButton = new UIGraphicsRotatorButton(this, "additionalHeight", !m_fClosed);
    m_pButton->setIcon(data(GroupItemData_ButtonPixmap).value<QIcon>());
    connect(m_pButton, SIGNAL(sigRotationStart()), this, SLOT(sltGroupToggleStart()));
    connect(m_pButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltGroupToggleFinish(bool)));

    /* Setup name-editor: */
    m_pNameEditorWidget = new QLineEdit(m_strName);
    m_pNameEditorWidget->setTextMargins(0, 0, 0, 0);
    m_pNameEditorWidget->setFont(data(GroupItemData_NameFont).value<QFont>());
    connect(m_pNameEditorWidget, SIGNAL(editingFinished()), this, SLOT(sltNameEditingFinished()));
    m_pNameEditor = new QGraphicsProxyWidget(this);
    m_pNameEditor->setWidget(m_pNameEditorWidget);
    m_pNameEditor->hide();
}

/* static */
void UIGSelectorItemGroup::copyContent(UIGSelectorItemGroup *pFrom, UIGSelectorItemGroup *pTo)
{
    /* Copy group items: */
    foreach (UIGSelectorItem *pGroupItem, pFrom->items(UIGSelectorItemType_Group))
        new UIGSelectorItemGroup(pTo, pGroupItem->toGroupItem());
    /* Copy machine items: */
    foreach (UIGSelectorItem *pMachineItem, pFrom->items(UIGSelectorItemType_Machine))
        new UIGSelectorItemMachine(pTo, pMachineItem->toMachineItem());
}

