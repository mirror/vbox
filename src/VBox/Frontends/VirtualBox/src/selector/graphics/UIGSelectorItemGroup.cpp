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
#include <QApplication>
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

UIGSelectorItemGroup::UIGSelectorItemGroup(UIGSelectorItem *pParent,
                                           const QString &strName,
                                           int iPosition /* -1 */)
    : UIGSelectorItem(pParent, iPosition)
    , m_strName(strName)
    , m_fClosed(parentItem() ? parentItem()->parentItem() ? true : false : false)
    , m_pToggleButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
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
    : UIGSelectorItem(pParent, iPosition)
    , m_fClosed(parentItem() ? parentItem()->parentItem() ? true : false : false)
    , m_pToggleButton(0)
    , m_pNameEditorWidget(0)
    , m_pNameEditor(0)
    , m_iAdditionalHeight(0)
{
    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);

    /* Prepare: */
    prepare();

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

void UIGSelectorItemGroup::setName(const QString &strName)
{
    m_strName = strName;
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
    m_pToggleButton->setToggled(false, true);
}

void UIGSelectorItemGroup::open()
{
    AssertMsg(parentItem(), ("Can't open root-item!"));
    m_pToggleButton->setToggled(true, true);
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
    /* Rebuild definitions: */
    model()->updateDefinition();
}

void UIGSelectorItemGroup::sltGroupToggled(bool fAnimated)
{
    /* Not for root-item: */
    AssertMsg(parentItem(), ("Can't toggle root-item!"));
    /* Toggle animated? */
    if (fAnimated)
    {
        /* Make sure animation is NOT even created: */
        if (m_pResizeAnimation)
            return;

        /* Prepare animation: */
        QSizeF closedSize = sizeHint(Qt::MinimumSize, true);
        QSizeF openedSize = sizeHint(Qt::MinimumSize, false);
        int iAdditionalHeight = openedSize.height() - closedSize.height();
        m_pResizeAnimation = new QPropertyAnimation(this, "additionalHeight", this);
        m_pResizeAnimation->setEasingCurve(QEasingCurve::InCubic);
        m_pResizeAnimation->setDuration(300);
        if (m_fClosed)
        {
            m_pResizeAnimation->setStartValue(0);
            m_pResizeAnimation->setEndValue(iAdditionalHeight);
            m_pResizeAnimation->setProperty("animation-direction", "forward");
            /* Toggle-state and navigation will be updated after animation... */
        }
        else
        {
            m_pResizeAnimation->setStartValue(iAdditionalHeight);
            m_pResizeAnimation->setEndValue(0);
            m_pResizeAnimation->setProperty("animation-direction", "backward");
            /* Update toggle-state and navigation: */
            m_fClosed = true;
            model()->updateNavigation();
        }
        connect(m_pResizeAnimation, SIGNAL(valueChanged(const QVariant&)),
                this, SLOT(sltHandleResizeAnimationProgress(const QVariant&)));
        m_pResizeAnimation->start();
    }
    /* Toggle simple? */
    else
    {
        /* Update toggle-state and navigation: */
        m_fClosed = !m_fClosed;
        model()->updateNavigation();
        /* Relayout model: */
        model()->updateLayout();
    }
}

void UIGSelectorItemGroup::sltHandleResizeAnimationProgress(const QVariant &value)
{
    /* Determine sender: */
    QPropertyAnimation *pSenderAnimation = qobject_cast<QPropertyAnimation*>(sender());
    /* Determine if that was a final animation step: */
    bool fAnimationFinalStep = false;
    if (pSenderAnimation &&
        pSenderAnimation->endValue().toInt() == value.toInt())
    {
        /* Toggle group: */
        if (pSenderAnimation->property("animation-direction").toString() == "forward")
        {
            /* Update toggle-state and navigation: */
            m_fClosed = false;
            model()->updateNavigation();
        }
        /* Reset additional height: */
        m_iAdditionalHeight = 0;
        /* Cleanup animation: */
        m_pResizeAnimation->deleteLater();
        /* Set flag to 'true': */
        fAnimationFinalStep = true;
    }
    /* Relayout scene: */
    model()->updateLayout();
    /* Update scene selection finally: */
    if (fAnimationFinalStep)
        model()->notifySelectionChanged();
}

QVariant UIGSelectorItemGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupItemData_GroupItemMargin: return parentItem() ? 10 : 1;
        case GroupItemData_GroupItemMajorSpacing: return parentItem() ? 10 : 0;
        case GroupItemData_GroupItemMinorSpacing: return 3;
        /* Fonts: */
        case GroupItemData_GroupNameFont:
        {
            QFont groupNameFont = qApp->font();
            groupNameFont.setPointSize(groupNameFont.pointSize() + 1);
            groupNameFont.setWeight(QFont::Bold);
            return groupNameFont;
        }
        /* Sizes: */
        case GroupItemData_GroupButtonSize: return m_pToggleButton ? m_pToggleButton->minimumSizeHint() : QSizeF(0, 0);
        case GroupItemData_GroupNameSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            QFontMetrics fm(data(GroupItemData_GroupNameFont).value<QFont>());
            return QSize(fm.width(data(GroupItemData_GroupName).toString()), fm.height());
        }
        case GroupItemData_GroupNameEditorSize:
        {
            if (!parentItem())
                return QSizeF(0, 0);
            return m_pNameEditorWidget->minimumSizeHint();
        }
        /* Pixmaps: */
        case GroupItemData_GroupPixmap: return UIIconPool::iconSet(":/arrow_right_10px.png");
        /* Texts: */
        case GroupItemData_GroupName: return m_strName;
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

void UIGSelectorItemGroup::cleanup()
{
    /* Cleanup all the group items first: */
    foreach (UIGSelectorItem *pItem, items(UIGSelectorItemType_Group))
        pItem->cleanup();
    /* Cleanup ourself only for non-root groups: */
    if (parentItem() && !hasItems())
        delete this;
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

QList<UIGSelectorItem*> UIGSelectorItemGroup::items(UIGSelectorItemType type) const
{
    switch (type)
    {
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
    /* Update size-hints for group items: */
    foreach (UIGSelectorItem *pItem, m_groupItems)
        pItem->updateSizeHint();
    /* Update size-hints for machine items: */
    foreach (UIGSelectorItem *pItem, m_machineItems)
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateGeometry();
}

void UIGSelectorItemGroup::updateLayout()
{
    /* Prepare variables: */
    int iGroupItemMargin = data(GroupItemData_GroupItemMargin).toInt();
    int iGroupItemMajorSpacing = data(GroupItemData_GroupItemMajorSpacing).toInt();

    /* For non-root item: */
    if (parentItem())
    {
        /* Calculate attributes: */
        int iToggleButtonHeight = m_pToggleButton->minimumSizeHint().toSize().height();
        int iNameEditorHeight = m_pNameEditorWidget->minimumSizeHint().height();
        int iMinimumHeight = qMin(iToggleButtonHeight, iNameEditorHeight);
        int iMaximumHeight = qMax(iToggleButtonHeight, iNameEditorHeight);
        int iDelta = iMaximumHeight - iMinimumHeight;
        int iHalfDelta = iDelta / 2;

        int iToggleButtonX = iGroupItemMargin;
        int iToggleButtonY = iToggleButtonHeight >= iNameEditorHeight ? iGroupItemMargin :
                             iGroupItemMargin + iHalfDelta;

        int iNameEditorX = iGroupItemMargin +
                           m_pToggleButton->minimumSizeHint().toSize().width() +
                           iGroupItemMajorSpacing;
        int iNameEditorY = iNameEditorHeight >= iToggleButtonHeight ? iGroupItemMargin :
                           iGroupItemMargin + iHalfDelta;

        /* Layout widgets: */
        m_pToggleButton->setPos(iToggleButtonX, iToggleButtonY);
        m_pNameEditor->setPos(iNameEditorX, iNameEditorY);
        m_pNameEditorWidget->resize(desiredWidth() - iNameEditorX - iGroupItemMargin, m_pNameEditorWidget->height());
    }

    /* If group is closed: */
    if (m_fClosed)
    {
        /* Hide all the group items: */
        foreach (UIGSelectorItem *pItem, m_groupItems)
            if (pItem->isVisible())
                pItem->hide();
        /* Hide all the machine items: */
        foreach (UIGSelectorItem *pItem, m_machineItems)
            if (pItem->isVisible())
                pItem->hide();
    }
    /* If group is opened: */
    else
    {
        /* Prepare other variables: */
        int iGroupItemMinorSpacing = data(GroupItemData_GroupItemMinorSpacing).toInt();
        QSize groupButtonSize = data(GroupItemData_GroupButtonSize).toSize();
        QSize groupNameSize = data(GroupItemData_GroupNameEditorSize).toSize();
        int iGroupItemHeaderHeight = qMax(groupButtonSize.height(), groupNameSize.height());
        int iPreviousVerticalIndent = iGroupItemMargin +
                                      iGroupItemHeaderHeight +
                                      iGroupItemMajorSpacing;
        /* Layout all the group items: */
        foreach (UIGSelectorItem *pItem, m_groupItems)
        {
            /* Show if hidden: */
            if (!pItem->isVisible())
                pItem->show();
            /* Set item's position: */
            pItem->setPos(iGroupItemMargin, iPreviousVerticalIndent);
            /* Relayout group: */
            pItem->updateLayout();
            /* Update indent for next items: */
            iPreviousVerticalIndent += (pItem->minimumSizeHint().toSize().height() + iGroupItemMinorSpacing);
        }
        /* Layout all the machine items: */
        foreach (UIGSelectorItem *pItem, m_machineItems)
        {
            /* Show if hidden: */
            if (!pItem->isVisible())
                pItem->show();
            /* Set item's position: */
            pItem->setPos(iGroupItemMargin, iPreviousVerticalIndent);
            /* Update indent for next items: */
            iPreviousVerticalIndent += (pItem->minimumSizeHint().toSize().height() + iGroupItemMinorSpacing);
        }
    }
}

void UIGSelectorItemGroup::setDesiredWidth(int iDesiredWidth)
{
    /* Call to base-class: */
    UIGSelectorItem::setDesiredWidth(iDesiredWidth);

    /* Calculate desired width for children: */
    int iMargin = data(GroupItemData_GroupItemMargin).toInt();
    int iDesiredWidthForChildren = iDesiredWidth - 2 * iMargin;
    /* Update desired hints of the group items: */
    foreach (UIGSelectorItem *pItem, m_groupItems)
        pItem->setDesiredWidth(iDesiredWidthForChildren);
    /* Update desired hints of the machine items: */
    foreach (UIGSelectorItem *pItem, m_machineItems)
        pItem->setDesiredWidth(iDesiredWidthForChildren);
}

QSizeF UIGSelectorItemGroup::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* Use calculation which is suitable for Qt::MaximumSize and Qt::MinimumSize hints only: */
    if (which == Qt::MaximumSize || which == Qt::MinimumSize)
        return sizeHint(which, m_fClosed);
    /* Else call to base-class: */
    return UIGSelectorItem::sizeHint(which, constraint);
}

QSizeF UIGSelectorItemGroup::sizeHint(Qt::SizeHint which, bool fClosedGroup) const
{
    /* This calculation is suitable only for Qt::MaximumSize and Qt::MinimumSize hints: */
    if (which == Qt::MaximumSize || which == Qt::MinimumSize)
    {
        /* First of all, we have to prepare few variables: */
        int iGroupItemMargin = data(GroupItemData_GroupItemMargin).toInt();
        int iGroupItemMajorSpacing = data(GroupItemData_GroupItemMajorSpacing).toInt();
        int iGroupItemMinorSpacing = data(GroupItemData_GroupItemMinorSpacing).toInt();
        QSize groupButtonSize = data(GroupItemData_GroupButtonSize).toSize();
        QSize groupNameSize = data(GroupItemData_GroupNameEditorSize).toSize();

        /* Calculating proposed height: */
        int iProposedHeight = 0;
        {
            /* Simple group item have 2 margins - top and bottom: */
            iProposedHeight += 2 * iGroupItemMargin;
            /* And group header to take into account: */
            int iGroupItemHeaderHeight = qMax(groupButtonSize.height(), groupNameSize.height());
            iProposedHeight += iGroupItemHeaderHeight;
            /* But if group is opened: */
            if (!fClosedGroup)
            {
                /* We should take major spacing before any items into account: */
                if (hasItems())
                    iProposedHeight += iGroupItemMajorSpacing;
                for (int i = 0; i < m_groupItems.size(); ++i)
                {
                    /* Every group item height: */
                    UIGSelectorItem *pItem = m_groupItems[i];
                    iProposedHeight += pItem->maximumSizeHint().height();
                    /* And every spacing between sub-groups: */
                    if (i < m_groupItems.size() - 1)
                        iProposedHeight += iGroupItemMinorSpacing;
                }
                /* We should take minor spacing between group and machine items into account: */
                if (hasItems(UIGSelectorItemType_Group) && hasItems(UIGSelectorItemType_Machine))
                    iProposedHeight += iGroupItemMinorSpacing;
                for (int i = 0; i < m_machineItems.size(); ++i)
                {
                    /* Every machine item height: */
                    UIGSelectorItem *pItem = m_machineItems[i];
                    iProposedHeight += pItem->maximumSizeHint().height();
                    /* And every spacing between sub-machines: */
                    if (i < m_machineItems.size() - 1)
                        iProposedHeight += iGroupItemMinorSpacing;
                }
            }
            /* And we should take into account minimum height restrictions: */
            iProposedHeight = qMax(iProposedHeight, desiredHeight());
            /* Finally, additional height during animation: */
            if (m_pResizeAnimation)
                iProposedHeight += m_iAdditionalHeight;
        }

        /* If Qt::MaximumSize was requested: */
        if (which == Qt::MaximumSize)
        {
            /* We are interested only in height calculation,
             * getting width from the base-class hint: */
            QSize maximumSize = UIGSelectorItem::sizeHint(Qt::MaximumSize).toSize();
            return QSize(maximumSize.width(), iProposedHeight);
        }

        /* Calculating proposed width: */
        int iProposedWidth = 0;
        {
            /* Simple group item have 2 margins - left and right: */
            iProposedWidth += 2 * iGroupItemMargin;
            /* And group header to take into account: */
            int iGroupItemHeaderWidth = groupButtonSize.width() + iGroupItemMajorSpacing + groupNameSize.width();
            iProposedWidth += iGroupItemHeaderWidth;
            /* But if group is opened: */
            if (!fClosedGroup)
            {
                /* We have to make sure that we had taken into account: */
                for (int i = 0; i < m_groupItems.size(); ++i)
                {
                    /* All the group item widths: */
                    UIGSelectorItem *pItem = m_groupItems[i];
                    iProposedWidth = qMax(iProposedWidth, pItem->minimumSizeHint().toSize().width());
                }
                for (int i = 0; i < m_machineItems.size(); ++i)
                {
                    /* And all the machine item widths: */
                    UIGSelectorItem *pItem = m_machineItems[i];
                    iProposedWidth = qMax(iProposedWidth, pItem->minimumSizeHint().toSize().width());
                }
            }
            /* And finally, we should take into account minimum width restrictions: */
            iProposedWidth = qMax(iProposedWidth, desiredWidth());
        }

        /* If Qt::MinimumSize was requested: */
        if (which == Qt::MinimumSize)
        {
            /* We had calculated both height and width already: */
            return QSize(iProposedWidth, iProposedHeight);
        }
    }
    return QSizeF();
}

QPixmap UIGSelectorItemGroup::toPixmap()
{
    QSize minimumSizeHint = sizeHint(Qt::MinimumSize, true).toSize();
    QPixmap pixmap(minimumSizeHint);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSizeHint);
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
                pModel->updateDefinition();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewGroupItem);
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
                pModel->updateDefinition();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewMachineItem);
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
    /* Reset drag tokens for all the group items: */
    foreach (UIGSelectorItem *pItem, m_groupItems)
        pItem->resetDragToken();
    /* Reset drag tokens for all the group items: */
    foreach (UIGSelectorItem *pItem, m_machineItems)
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
    /* Initialize some necessary variables: */
    QRect fullRect = pOption->rect;
    int iGroupItemMargin = data(GroupItemData_GroupItemMargin).toInt();
    int iGroupItemMajorSpacing = data(GroupItemData_GroupItemMajorSpacing).toInt();
    QSize groupButtonSize = data(GroupItemData_GroupButtonSize).toSize();
    QSize groupNameEditorSize = data(GroupItemData_GroupNameEditorSize).toSize();

    /* If group is opened: */
    if (!fClosedGroup)
    {
        /* We are prepare clipping: */
        QRegion region(fullRect);
        int iGroupItemMinorSpacing = data(GroupItemData_GroupItemMinorSpacing).toInt();
        int iGroupItemHeaderHeight = qMax(groupButtonSize.height(), groupNameEditorSize.height());
        int iPreviousIndent = iGroupItemMargin + iGroupItemHeaderHeight + iGroupItemMajorSpacing;

        /* For all the sub-groups: */
        foreach(UIGSelectorItem *pItem, m_groupItems)
        {
            /* Calculate item rect: */
            QSize itemSize;
            itemSize.rwidth() = fullRect.width() - 2 * iGroupItemMargin;
            itemSize.rheight() = pItem->minimumSizeHint().toSize().height();
            QRect itemRect(fullRect.topLeft(), itemSize);
            itemRect.translate(iGroupItemMargin, iPreviousIndent);
            /* Grow for 1 pixel all the directions: */
            itemRect.setTopLeft(itemRect.topLeft() - QPoint(1, 1));
            itemRect.setBottomRight(itemRect.bottomRight() + QPoint(1, 1));
            /* Substract cliping rectangle: */
            region -= itemRect;
            /* Remember previous indent: */
            iPreviousIndent += (itemSize.height() + iGroupItemMinorSpacing);
        }

        /* For all the sub-machines: */
        foreach(UIGSelectorItem *pItem, m_machineItems)
        {
            /* Calculate item rect: */
            QSize itemSize;
            itemSize.rwidth() = fullRect.width() - 2 * iGroupItemMargin;
            itemSize.rheight() = pItem->minimumSizeHint().toSize().height();
            QRect itemRect(fullRect.topLeft(), itemSize);
            itemRect.translate(iGroupItemMargin, iPreviousIndent);
            /* Grow for 1 pixel all the directions: */
            itemRect.setTopLeft(itemRect.topLeft() - QPoint(1, 1));
            itemRect.setBottomRight(itemRect.bottomRight() + QPoint(1, 1));
            /* Substract cliping rectangle: */
            region -= itemRect;
            /* Remember previous indent: */
            iPreviousIndent += (itemSize.height() + iGroupItemMinorSpacing);
        }

        /* Apply clipping: */
        pPainter->setClipRegion(region);
    }

    /* Paint background: */
    paintBackground(/* Painter; */
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
                    dragTokenPlace());

    /* Paint name: */
    if (parentItem())
    {
        /* Initialize some necessary variables: */
        QSize groupNameSize = data(GroupItemData_GroupNameSize).toSize();

        /* Calculate attributes: */
        int iGroupNameHeight = groupNameSize.height();
        int iNameEditorHeight = m_pNameEditorWidget->minimumSizeHint().height();
        int iMinimumHeight = qMin(iGroupNameHeight, iNameEditorHeight);
        int iMaximumHeight = qMax(iGroupNameHeight, iNameEditorHeight);
        int iDelta = iMaximumHeight - iMinimumHeight;
        int iHalfDelta = iDelta / 2;
        int iNameX = iGroupItemMargin +
                     m_pToggleButton->minimumSizeHint().toSize().width() +
                     iGroupItemMajorSpacing +
                     1 /* frame width from Qt sources */ +
                     2 /* internal QLineEdit margin from Qt sources */;
        int iNameY = iGroupNameHeight >= iNameEditorHeight ? iGroupItemMargin :
                     iGroupItemMargin + iHalfDelta;

        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(QPoint(iNameX, iNameY), groupNameSize),
                  /* Font to paint text: */
                  data(GroupItemData_GroupNameFont).value<QFont>(),
                  /* Text to paint: */
                  data(GroupItemData_GroupName).toString());
    }

    /* Paint focus (if necessary): */
    if (parentItem() && model()->focusItem() == this)
        paintFocus(/* Painter: */
                   pPainter,
                   /* Rectangle to paint in: */
                   fullRect);
}

void UIGSelectorItemGroup::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
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
    m_pToggleButton = new UIGraphicsRotatorButton(this);
    m_pToggleButton->setToggled(!m_fClosed, false);
    m_pToggleButton->setIcon(data(GroupItemData_GroupPixmap).value<QIcon>());
    connect(m_pToggleButton, SIGNAL(sigButtonToggled(bool)), this, SLOT(sltGroupToggled(bool)));

    /* Setup name-editor: */
    m_pNameEditorWidget = new QLineEdit(m_strName);
    m_pNameEditorWidget->setTextMargins(0, 0, 0, 0);
    m_pNameEditorWidget->setFont(data(GroupItemData_GroupNameFont).value<QFont>());
    connect(m_pNameEditorWidget, SIGNAL(editingFinished()), this, SLOT(sltNameEditingFinished()));
    m_pNameEditor = new QGraphicsProxyWidget(this);
    m_pNameEditor->setWidget(m_pNameEditorWidget);
    m_pNameEditor->hide();
}

/* static */
void UIGSelectorItemGroup::copyContent(UIGSelectorItemGroup *pFrom, UIGSelectorItemGroup *pTo)
{
    /* Copy name: */
    pTo->setName(pFrom->name());
    /* Copy group items: */
    foreach (UIGSelectorItem *pGroupItem, pFrom->items(UIGSelectorItemType_Group))
        new UIGSelectorItemGroup(pTo, pGroupItem->toGroupItem());
    /* Copy machine items: */
    foreach (UIGSelectorItem *pMachineItem, pFrom->items(UIGSelectorItemType_Machine))
        new UIGSelectorItemMachine(pTo, pMachineItem->toMachineItem());
}

