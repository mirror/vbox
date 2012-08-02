/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItemGroup class implementation
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
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>

/* GUI includes: */
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"
#include "UIGChooserModel.h"
#include "UIIconPool.h"
#include "UIGraphicsRotatorButton.h"

/* static */
QString UIGChooserItemGroup::className() { return "UIGChooserItemGroup"; }

UIGChooserItemGroup::UIGChooserItemGroup(QGraphicsScene *pScene)
    : UIGChooserItem(0)
    , m_fClosed(false)
    , m_pButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
{
    /* Add item to the scene: */
    if (pScene)
        pScene->addItem(this);
}

UIGChooserItemGroup::UIGChooserItemGroup(QGraphicsScene *pScene,
                                         UIGChooserItemGroup *pCopyFrom)
    : UIGChooserItem(0)
    , m_strName(pCopyFrom->name())
    , m_fClosed(pCopyFrom->closed())
    , m_pButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
{
    /* Add item to the scene: */
    if (pScene)
        pScene->addItem(this);

    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);
}

UIGChooserItemGroup::UIGChooserItemGroup(UIGChooserItem *pParent,
                                         const QString &strName,
                                         bool fOpened /* = false */,
                                         int iPosition /* = -1 */)
    : UIGChooserItem(pParent)
    , m_strName(strName)
    , m_fClosed(!fOpened)
    , m_pButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);
}

UIGChooserItemGroup::UIGChooserItemGroup(UIGChooserItem *pParent,
                                         UIGChooserItemGroup *pCopyFrom,
                                         int iPosition /* = -1 */)
    : UIGChooserItem(pParent)
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

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);

    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);
}

UIGChooserItemGroup::~UIGChooserItemGroup()
{
    /* Delete all the items: */
    clearItems();

    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus/selection: */
        model()->setFocusItem(0, true);
    }
    /* If that item is NOT focused, but selected: */
    else if (model()->selectionList().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromSelectionList(this);
    }
    /* Remove item from the navigation list: */
    model()->removeFromNavigationList(this);

    /* Remove item from the parent: */
    if (parentItem())
        parentItem()->removeItem(this);
}

QString UIGChooserItemGroup::name() const
{
    return m_strName;
}

bool UIGChooserItemGroup::closed() const
{
    return m_fClosed && !isRoot();
}

bool UIGChooserItemGroup::opened() const
{
    return !m_fClosed || isRoot();
}

void UIGChooserItemGroup::close()
{
    AssertMsg(parentItem(), ("Can't close root-item!"));
    m_pButton->setToggled(false);
}

void UIGChooserItemGroup::open()
{
    AssertMsg(parentItem(), ("Can't open root-item!"));
    m_pButton->setToggled(true);
}

bool UIGChooserItemGroup::contains(const QString &strId, bool fRecursively /* = false */) const
{
    /* Check machine items: */
    foreach (UIGChooserItem *pItem, m_machineItems)
        if (pItem->toMachineItem()->id() == strId)
            return true;
    /* If recursively => check group items: */
    if (fRecursively)
        foreach (UIGChooserItem *pItem, m_groupItems)
            if (pItem->toGroupItem()->contains(strId, fRecursively))
                return true;
    return false;
}

void UIGChooserItemGroup::sltNameEditingFinished()
{
    /* Not for root-item: */
    if (isRoot())
        return;

    /* Lock name-editor: */
    m_pNameEditor->hide();

    /* Enumerate all the group names: */
    QStringList groupNames;
    foreach (UIGChooserItem *pItem, parentItem()->items(UIGChooserItemType_Group))
        groupNames << pItem->name();

    /* If proposed name is empty or not unique, reject it: */
    QString strNewName = m_pNameEditorWidget->text().trimmed();
    if (strNewName.isEmpty() || groupNames.contains(strNewName))
        return;

    m_strName = strNewName;
}

void UIGChooserItemGroup::sltGroupToggleStart()
{
    /* Not for root-item: */
    if (isRoot())
        return;

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

void UIGChooserItemGroup::sltGroupToggleFinish(bool fToggled)
{
    /* Not for root-item: */
    if (isRoot())
        return;

    /* Update toggle-state: */
    m_fClosed = !fToggled;
    /* Update navigation: */
    model()->updateNavigation();
    /* Relayout model: */
    model()->updateLayout();
}

QVariant UIGChooserItemGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupItemData_HorizonalMargin: return isRoot() ? 1 : 8;
        case GroupItemData_VerticalMargin: return isRoot() ? 1 : 5;
        case GroupItemData_MajorSpacing: return isRoot() ? 0 : 10;
        case GroupItemData_MinorSpacing: return 0;
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
            if (isRoot())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_NameFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_Name).toString()), fm.height());
        }
        case GroupItemData_NameEditorSize:
        {
            if (isRoot())
                return QSizeF(0, 0);
            return m_pNameEditorWidget->minimumSizeHint();
        }
        case GroupItemData_GroupPixmapSize:
            return isRoot() ? QSizeF(0, 0) : data(GroupItemData_GroupPixmap).value<QIcon>().availableSizes().at(0);
        case GroupItemData_MachinePixmapSize:
            return isRoot() ? QSizeF(0, 0) : data(GroupItemData_MachinePixmap).value<QIcon>().availableSizes().at(0);
        case GroupItemData_GroupCountTextSize:
        {
            if (isRoot())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_InfoFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_GroupCountText).toString()), fm.height());
        }
        case GroupItemData_MachineCountTextSize:
        {
            if (isRoot())
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
                    << nameSize.height()
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

void UIGChooserItemGroup::show()
{
    /* Call to base-class: */
    UIGChooserItem::show();
    /* Show children: */
    if (!closed())
        foreach (UIGChooserItem *pItem, items())
            pItem->show();
}

void UIGChooserItemGroup::hide()
{
    /* Call to base-class: */
    UIGChooserItem::hide();
    /* Hide children: */
    foreach (UIGChooserItem *pItem, items())
        pItem->hide();
}

void UIGChooserItemGroup::startEditing()
{
    /* Not for root-item: */
    if (isRoot())
        return;

    /* Unlock name-editor: */
    m_pNameEditor->show();
    m_pNameEditorWidget->setText(m_strName);
    m_pNameEditorWidget->setFocus();
}

void UIGChooserItemGroup::addItem(UIGChooserItem *pItem, int iPosition)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGChooserItemType_Group:
        {
            if (iPosition < 0 || iPosition >= m_groupItems.size())
                m_groupItems.append(pItem);
            else
                m_groupItems.insert(iPosition, pItem);
            scene()->addItem(pItem);
            break;
        }
        case UIGChooserItemType_Machine:
        {
            if (iPosition < 0 || iPosition >= m_machineItems.size())
                m_machineItems.append(pItem);
            else
                m_machineItems.insert(iPosition, pItem);
            scene()->addItem(pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

void UIGChooserItemGroup::removeItem(UIGChooserItem *pItem)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGChooserItemType_Group:
        {
            scene()->removeItem(pItem);
            m_groupItems.removeAt(m_groupItems.indexOf(pItem));
            break;
        }
        case UIGChooserItemType_Machine:
        {
            scene()->removeItem(pItem);
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

QList<UIGChooserItem*> UIGChooserItemGroup::items(UIGChooserItemType type /* = UIGChooserItemType_Any */) const
{
    switch (type)
    {
        case UIGChooserItemType_Any: return items(UIGChooserItemType_Group) + items(UIGChooserItemType_Machine);
        case UIGChooserItemType_Group: return m_groupItems;
        case UIGChooserItemType_Machine: return m_machineItems;
        default: break;
    }
    return QList<UIGChooserItem*>();
}

bool UIGChooserItemGroup::hasItems(UIGChooserItemType type /* = UIGChooserItemType_Any */) const
{
    switch (type)
    {
        case UIGChooserItemType_Any:
            return hasItems(UIGChooserItemType_Group) || hasItems(UIGChooserItemType_Machine);
        case UIGChooserItemType_Group:
            return !m_groupItems.isEmpty();
        case UIGChooserItemType_Machine:
            return !m_machineItems.isEmpty();
    }
    return false;
}

void UIGChooserItemGroup::clearItems(UIGChooserItemType type /* = UIGChooserItemType_Any */)
{
    switch (type)
    {
        case UIGChooserItemType_Any:
        {
            clearItems(UIGChooserItemType_Group);
            clearItems(UIGChooserItemType_Machine);
            break;
        }
        case UIGChooserItemType_Group:
        {
            while (!m_groupItems.isEmpty()) { delete m_groupItems.last(); }
            break;
        }
        case UIGChooserItemType_Machine:
        {
            while (!m_machineItems.isEmpty()) { delete m_machineItems.last(); }
            break;
        }
    }
}

void UIGChooserItemGroup::updateSizeHint()
{
    /* Update size-hints for all the items: */
    foreach (UIGChooserItem *pItem, items())
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateGeometry();
}

void UIGChooserItemGroup::updateLayout()
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iButtonHeight = data(GroupItemData_ButtonSize).toSize().height();
    int iButtonWidth = data(GroupItemData_ButtonSize).toSize().width();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
    int iPreviousVerticalIndent = 0;

    /* Header (root item): */
    if (isRoot())
    {
        /* Hide button: */
        if (m_pButton && m_pButton->isVisible())
            m_pButton->hide();

        /* Hide name-editor: */
        if (m_pNameEditor && m_pNameEditor->isVisible())
            m_pNameEditor->hide();

        /* Prepare body indent: */
        iPreviousVerticalIndent = iVerticalMargin;
    }
    /* Header (non-root item): */
    else
    {
        /* Show button: */
        if (!m_pButton->isVisible())
            m_pButton->show();
        /* Layout button: */
        int iButtonX = iHorizontalMargin;
        int iButtonY = iButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                       iVerticalMargin + (iFullHeaderHeight - iButtonHeight) / 2;
        m_pButton->setPos(iButtonX, iButtonY);

        /* Layout name-editor: */
        int iNameEditorX = iHorizontalMargin + iButtonWidth + iMinorSpacing;
        int iNameEditorY = 1;
        m_pNameEditor->setPos(iNameEditorX, iNameEditorY);
        m_pNameEditorWidget->resize(geometry().width() - iNameEditorX - iHorizontalMargin, m_pNameEditorWidget->height());

        /* Prepare body indent: */
        iPreviousVerticalIndent = 2 * iVerticalMargin + iFullHeaderHeight;
    }

    /* No body for closed group: */
    if (closed())
    {
        /* Hide all the items: */
        foreach (UIGChooserItem *pItem, items())
            if (pItem->isVisible())
                pItem->hide();
    }
    /* Body for opened group: */
    else
    {
        /* Prepare variables: */
        int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
        QRect geo = geometry().toRect();
        int iX = geo.x();
        int iY = geo.y();
        int iWidth = geo.width();
        /* Layout all the items: */
        foreach (UIGChooserItem *pItem, items())
        {
            /* Show if hidden: */
            if (!pItem->isVisible())
                pItem->show();
            /* Get item's height-hint: */
            int iMinimumHeight = pItem->minimumHeightHint();
            /* Set item's position: */
            pItem->setPos(iX + iHorizontalMargin, iY + iPreviousVerticalIndent);
            /* Set item's size: */
            pItem->resize(iWidth - 2 * iHorizontalMargin, iMinimumHeight);
            /* Relayout group: */
            pItem->updateLayout();
            /* Update indent for next items: */
            iPreviousVerticalIndent += (iMinimumHeight + iMinorSpacing);
        }
    }
}

int UIGChooserItemGroup::minimumWidthHint(bool fClosedGroup) const
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
        foreach (UIGChooserItem *pItem, items())
        {
            int iItemWidth = 2 * iHorizontalMargin + pItem->minimumWidthHint();
            iProposedWidth = qMax(iProposedWidth, iItemWidth);
        }
    }

    /* Return result: */
    return iProposedWidth;
}

int UIGChooserItemGroup::minimumHeightHint(bool fClosedGroup) const
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
            UIGChooserItem *pItem = m_groupItems[i];
            iProposedHeight += pItem->minimumHeightHint();
            /* And every spacing between sub-groups: */
            if (i < m_groupItems.size() - 1)
                iProposedHeight += iMinorSpacing;
        }
        /* Minor spacing between group and machine item: */
        if (hasItems(UIGChooserItemType_Group) && hasItems(UIGChooserItemType_Machine))
            iProposedHeight += iMinorSpacing;
        for (int i = 0; i < m_machineItems.size(); ++i)
        {
            /* Every machine item height: */
            UIGChooserItem *pItem = m_machineItems[i];
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

int UIGChooserItemGroup::minimumWidthHint() const
{
    return minimumWidthHint(closed());
}

int UIGChooserItemGroup::minimumHeightHint() const
{
    return minimumHeightHint(closed());
}

QSizeF UIGChooserItemGroup::minimumSizeHint(bool fClosedGroup) const
{
    return QSizeF(minimumWidthHint(fClosedGroup), minimumHeightHint(fClosedGroup));
}

QSizeF UIGChooserItemGroup::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
        return minimumSizeHint(closed());
    /* Else call to base-class: */
    return UIGChooserItem::sizeHint(which, constraint);
}

QPixmap UIGChooserItemGroup::toPixmap()
{
    QSize minimumSize = minimumSizeHint(true).toSize();
    QPixmap pixmap(minimumSize);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSize);
    paint(&painter, &options, true);
    return pixmap;
}

bool UIGChooserItemGroup::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const
{
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should check mime format: */
    if (pMimeData->hasFormat(UIGChooserItemGroup::className()))
    {
        /* Get passed group item: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
        /* Make sure passed group is not 'this': */
        if (pItem == this)
            return false;
        /* Make sure passed group is not among our parents: */
        const UIGChooserItem *pTestedWidget = this;
        while (UIGChooserItem *pParentOfTestedWidget = pTestedWidget->parentItem())
        {
            if (pItem == pParentOfTestedWidget)
                return false;
            pTestedWidget = pParentOfTestedWidget;
        }
        return true;
    }
    else if (pMimeData->hasFormat(UIGChooserItemMachine::className()))
    {
        /* Get passed machine item: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
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

void UIGChooserItemGroup::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho, DragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Check mime format: */
    if (pMime->hasFormat(UIGChooserItemGroup::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGChooserModel *pModel = model();

                /* Get passed group item: */
                const UIGChooserItemMimeData *pCastedMime = qobject_cast<const UIGChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
                UIGChooserItem *pItem = pCastedMime->item();

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
                UIGChooserItem *pNewGroupItem = new UIGChooserItemGroup(this, pItem->toGroupItem(), iPosition);

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
    else if (pMime->hasFormat(UIGChooserItemMachine::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGChooserModel *pModel = model();

                /* Get passed item: */
                const UIGChooserItemMimeData *pCastedMime = qobject_cast<const UIGChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
                UIGChooserItem *pItem = pCastedMime->item();

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
                UIGChooserItem *pNewMachineItem = new UIGChooserItemMachine(this, pItem->toMachineItem(), iPosition);

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

void UIGChooserItemGroup::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != DragToken_Off)
    {
        setDragTokenPlace(DragToken_Off);
        update();
    }
    /* Reset drag tokens for all the items: */
    foreach (UIGChooserItem *pItem, items())
        pItem->resetDragToken();
}

QMimeData* UIGChooserItemGroup::createMimeData()
{
    return new UIGChooserItemMimeData(this);
}

void UIGChooserItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget* /* pWidget = 0 */)
{
    paint(pPainter, pOption, closed());
}

void UIGChooserItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup)
{
    /* Non-root item: */
    if (!isRoot())
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
    if (!isRoot())
    {
        /* Paint group info: */
        paintGroupInfo(pPainter, pOption, fClosedGroup);
    }
}

void UIGChooserItemGroup::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Prepare variables: */
    QRect fullRect = pOption->rect;

    /* Any item: */
    {
        /* Paint background: */
        paintBackground(/* Painter: */
                        pPainter,
                        /* Rectangle to paint in: */
                        fullRect);
    }

    /* Non-root item: */
    if (!isRoot())
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

void UIGChooserItemGroup::paintBackground(QPainter *pPainter, const QRect &rect)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    QPalette pal = palette();
    QColor base = pal.color(QPalette::Active, model()->selectionList().contains(this) ?
                            QPalette::Highlight : QPalette::Window);

    /* Root item: */
    if (isRoot())
    {
        /* Simple and clear: */
        pPainter->fillRect(rect, base.darker(100));
    }
    /* Non-root item: */
    else
    {
        /* Prepare variables: */
        int iMargin = data(GroupItemData_VerticalMargin).toInt();
        int iHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
        int iFullHeaderHeight = 2 * iMargin + iHeaderHeight;

        /* Fill rectangle with brush/color: */
        pPainter->fillRect(rect, Qt::white);

        /* Make even less rectangle: */
        QRect backGroundRect = rect;
        backGroundRect.setTopLeft(backGroundRect.topLeft() + QPoint(2, 2));
        backGroundRect.setBottomRight(backGroundRect.bottomRight() - QPoint(2, 2));
        /* Add even more clipping: */
        QPainterPath roundedPath;
        roundedPath.addRoundedRect(backGroundRect, m_iCornerRadius, m_iCornerRadius);
        pPainter->setClipPath(roundedPath);

        /* Calculate bottom rectangle: */
        QRect bRect = backGroundRect;
        bRect.setTop(bRect.bottom() - iFullHeaderHeight);
        /* Prepare bottom gradient: */
        QLinearGradient bGradient(bRect.topLeft(), bRect.bottomLeft());
        bGradient.setColorAt(0, base.darker(gradient()));
        bGradient.setColorAt(1, base.darker(104));
        /* Fill bottom rectangle: */
        pPainter->fillRect(bRect, bGradient);

        /* Calculate top rectangle: */
        QRect tRect = backGroundRect;
        tRect.setBottom(tRect.top() + iFullHeaderHeight);
        /* Prepare top gradient: */
        QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
        tGradient.setColorAt(0, base.darker(gradient()));
        tGradient.setColorAt(1, base.darker(104));
        /* Fill top rectangle: */
        pPainter->fillRect(tRect, tGradient);

        if (bRect.top() > tRect.bottom())
        {
            /* Calculate middle rectangle: */
            QRect midRect = QRect(tRect.bottomLeft(), bRect.topRight());
            /* Paint all the stuff: */
            pPainter->fillRect(midRect, base.darker(gradient()));
        }

        /* Paint drag token UP? */
        if (dragTokenPlace() != DragToken_Off)
        {
            QLinearGradient dragTokenGradient;
            QRect dragTokenRect = backGroundRect;
            if (dragTokenPlace() == DragToken_Up)
            {
                dragTokenRect.setHeight(5);
                dragTokenGradient.setStart(dragTokenRect.bottomLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
            }
            else if (dragTokenPlace() == DragToken_Down)
            {
                dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 5));
                dragTokenGradient.setStart(dragTokenRect.topLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
            }
            dragTokenGradient.setColorAt(0, base.darker(110));
            dragTokenGradient.setColorAt(1, base.darker(150));
            pPainter->fillRect(dragTokenRect, dragTokenGradient);
        }
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIGChooserItemGroup::paintGroupInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool)
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
    if (isHovered())
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

void UIGChooserItemGroup::updateAnimationParameters()
{
    /* Only for item with button: */
    if (!m_pButton)
        return;

    /* Recalculate animation parameters: */
    QSizeF openedSize = minimumSizeHint(false);
    QSizeF closedSize = minimumSizeHint(true);
    int iAdditionalHeight = openedSize.height() - closedSize.height();
    m_pButton->setAnimationRange(0, iAdditionalHeight);
}

void UIGChooserItemGroup::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
    model()->updateLayout();
}

int UIGChooserItemGroup::additionalHeight() const
{
    return m_iAdditionalHeight;
}

void UIGChooserItemGroup::prepare()
{
    /* Setup toggle-button: */
    m_pButton = new UIGraphicsRotatorButton(this, "additionalHeight", opened());
    m_pButton->setIcon(data(GroupItemData_ButtonPixmap).value<QIcon>());
    connect(m_pButton, SIGNAL(sigRotationStart()), this, SLOT(sltGroupToggleStart()));
    connect(m_pButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltGroupToggleFinish(bool)));
    m_pButton->hide();

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
void UIGChooserItemGroup::copyContent(UIGChooserItemGroup *pFrom, UIGChooserItemGroup *pTo)
{
    /* Copy group items: */
    foreach (UIGChooserItem *pGroupItem, pFrom->items(UIGChooserItemType_Group))
        new UIGChooserItemGroup(pTo, pGroupItem->toGroupItem());
    /* Copy machine items: */
    foreach (UIGChooserItem *pMachineItem, pFrom->items(UIGChooserItemType_Machine))
        new UIGChooserItemMachine(pTo, pMachineItem->toMachineItem());
}

