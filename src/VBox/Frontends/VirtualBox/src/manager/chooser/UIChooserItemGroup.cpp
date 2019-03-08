/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemGroup class implementation.
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

/* Qt includes: */
#include <QGraphicsLinearLayout>
#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWindow>

/* GUI includes: */
#include "UIChooserItemGlobal.h"
#include "UIChooserItemGroup.h"
#include "UIChooserItemMachine.h"
#include "UIChooserModel.h"
#include "UIGraphicsRotatorButton.h"
#include "UIGraphicsScrollArea.h"
#include "UIIconPool.h"
#include "UIVirtualBoxManager.h"


/*********************************************************************************************************************************
*   Class UIChooserItemGroup implementation.                                                                                     *
*********************************************************************************************************************************/

UIChooserItemGroup::UIChooserItemGroup(QGraphicsScene *pScene)
    : UIChooserItem(0, false /* favorite? */)
    , m_fClosed(false)
    , m_iAdditionalHeight(0)
    , m_iHeaderDarkness(110)
    , m_pToggleButton(0)
    , m_pEnterButton(0)
    , m_pExitButton(0)
    , m_pNameEditorWidget(0)
    , m_pContainerFavorite(0)
    , m_pLayoutFavorite(0)
    , m_pScrollArea(0)
    , m_pContainer(0)
    , m_pLayout(0)
    , m_pLayoutGlobal(0)
    , m_pLayoutGroup(0)
    , m_pLayoutMachine(0)
{
    /* Prepare: */
    prepare();

    /* Add item to the scene: */
    AssertMsg(pScene, ("Incorrect scene passed!"));
    pScene->addItem(this);

    /* Apply language settings: */
    retranslateUi();

    /* Prepare connections: */
    connect(this, &UIChooserItemGroup::sigMinimumWidthHintChanged,
            model(), &UIChooserModel::sigRootItemMinimumWidthHintChanged);
}

UIChooserItemGroup::UIChooserItemGroup(UIChooserItem *pParent,
                                       const QString &strName,
                                       bool fOpened /* = false */,
                                       int iPosition /* = -1 */)
    : UIChooserItem(pParent, pParent->isFavorite())
    , m_strName(strName)
    , m_fClosed(!fOpened)
    , m_iAdditionalHeight(0)
    , m_iHeaderDarkness(110)
    , m_pToggleButton(0)
    , m_pEnterButton(0)
    , m_pExitButton(0)
    , m_pNameEditorWidget(0)
    , m_pContainerFavorite(0)
    , m_pLayoutFavorite(0)
    , m_pScrollArea(0)
    , m_pContainer(0)
    , m_pLayout(0)
    , m_pLayoutGlobal(0)
    , m_pLayoutGroup(0)
    , m_pLayoutMachine(0)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, false, iPosition);
    connect(this, &UIChooserItemGroup::sigToggleStarted,
            model(), &UIChooserModel::sigToggleStarted);
    connect(this, &UIChooserItemGroup::sigToggleFinished,
            model(), &UIChooserModel::sigToggleFinished,
            Qt::QueuedConnection);
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIChooserItemGroup::sltHandleWindowRemapped);

    /* Apply language settings: */
    retranslateUi();

    /* Init: */
    updatePixmaps();
    updateItemCountInfo();
    updateVisibleName();
    updateToolTip();

    /* Prepare connections: */
    connect(this, &UIChooserItemGroup::sigMinimumWidthHintChanged,
            model(), &UIChooserModel::sigRootItemMinimumWidthHintChanged);
}

UIChooserItemGroup::UIChooserItemGroup(UIChooserItem *pParent,
                                       UIChooserItemGroup *pCopiedItem,
                                       int iPosition /* = -1 */)
    : UIChooserItem(pParent, pParent->isFavorite())
    , m_strName(pCopiedItem->name())
    , m_fClosed(pCopiedItem->isClosed())
    , m_iAdditionalHeight(0)
    , m_iHeaderDarkness(110)
    , m_pToggleButton(0)
    , m_pEnterButton(0)
    , m_pExitButton(0)
    , m_pNameEditorWidget(0)
    , m_pContainerFavorite(0)
    , m_pLayoutFavorite(0)
    , m_pScrollArea(0)
    , m_pContainer(0)
    , m_pLayout(0)
    , m_pLayoutGlobal(0)
    , m_pLayoutGroup(0)
    , m_pLayoutMachine(0)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, false, iPosition);
    connect(this, &UIChooserItemGroup::sigToggleStarted,
            model(), &UIChooserModel::sigToggleStarted);
    connect(this, &UIChooserItemGroup::sigToggleFinished,
            model(), &UIChooserModel::sigToggleFinished);
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIChooserItemGroup::sltHandleWindowRemapped);

    /* Copy content to 'this': */
    copyContent(pCopiedItem, this);

    /* Apply language settings: */
    retranslateUi();

    /* Init: */
    updatePixmaps();
    updateItemCountInfo();
    updateVisibleName();
    updateToolTip();
}

UIChooserItemGroup::~UIChooserItemGroup()
{
    /* Delete group name editor: */
    delete m_pNameEditorWidget;
    m_pNameEditorWidget = 0;

    /* Delete all the items: */
    clearItems();

    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus: */
        model()->setFocusItem(0);
    }
    /* If that item is in selection list: */
    if (model()->currentItems().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromCurrentItems(this);
    }
    /* If that item is in navigation list: */
    if (model()->navigationList().contains(this))
    {
        /* Remove item from the navigation list: */
        model()->removeFromNavigationList(this);
    }

    /* Remove item from the parent: */
    if (parentItem())
        parentItem()->removeItem(this);
}

void UIChooserItemGroup::setName(const QString &strName)
{
    /* Something changed? */
    if (m_strName == strName)
        return;

    /* Remember new name: */
    m_strName = strName;

    /* Update linked values: */
    updateVisibleName();
    updateMinimumHeaderSize();
}

bool UIChooserItemGroup::isClosed() const
{
    return m_fClosed && !isRoot();
}

void UIChooserItemGroup::close(bool fAnimated /* = true */)
{
    AssertMsg(!isRoot(), ("Can't close root-item!"));
    m_pToggleButton->setToggled(false, fAnimated);
}

bool UIChooserItemGroup::isOpened() const
{
    return !m_fClosed || isRoot();
}

void UIChooserItemGroup::open(bool fAnimated /* = true */)
{
    AssertMsg(!isRoot(), ("Can't open root-item!"));
    m_pToggleButton->setToggled(true, fAnimated);
}

void UIChooserItemGroup::updateFavorites()
{
    /* Global items only for now, move items to corresponding layout: */
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Global))
        if (pItem->isFavorite())
        {
            for (int iIndex = 0; iIndex < m_pLayoutGlobal->count(); ++iIndex)
                if (m_pLayoutGlobal->itemAt(iIndex) == pItem)
                    m_pLayoutFavorite->addItem(pItem);
        }
        else
        {
            for (int iIndex = 0; iIndex < m_pLayoutFavorite->count(); ++iIndex)
                if (m_pLayoutFavorite->itemAt(iIndex) == pItem)
                    m_pLayoutGlobal->addItem(pItem);
        }

    /* Update/activate children layout: */
    m_pLayout->updateGeometry();
    m_pLayout->activate();

    /* Relayout model: */
    model()->updateLayout();
}

/* static */
QString UIChooserItemGroup::className()
{
    return "UIChooserItemGroup";
}

void UIChooserItemGroup::retranslateUi()
{
    /* Update description: */
    m_strDescription = tr("Virtual Machine group");

    /* Update group tool-tip: */
    updateToolTip();

    /* Update button tool-tips: */
    if (m_pEnterButton)
        m_pEnterButton->setToolTip(tr("Enter group"));
    if (m_pExitButton)
        m_pExitButton->setToolTip(tr("Exit group"));
    updateToggleButtonToolTip();
}

void UIChooserItemGroup::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::showEvent(pEvent);

    /* Update pixmaps: */
    updatePixmaps();
}

void UIChooserItemGroup::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::resizeEvent(pEvent);

    /* What is the new geometry? */
    const QRectF newGeometry = geometry();

    /* Should we update visible name? */
    if (previousGeometry().width() != newGeometry.width())
        updateVisibleName();

    /* Remember the new geometry: */
    setPreviousGeometry(newGeometry);
}

void UIChooserItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Skip if hovered: */
    if (isHovered())
        return;

    /* Prepare variables: */
    const QPoint pos = pEvent->pos().toPoint();
    const int iMargin = data(GroupItemData_VerticalMargin).toInt();
    const int iHeaderHeight = m_minimumHeaderSize.height();
    const int iFullHeaderHeight = 2 * iMargin + iHeaderHeight;
    /* Skip if hovered part out of the header: */
    if (pos.y() >= iFullHeaderHeight)
        return;

    /* Show enter-button: */
    if (!isRoot() && m_pEnterButton)
        m_pEnterButton->show();

    /* Call to base-class: */
    UIChooserItem::hoverMoveEvent(pEvent);

    /* Update linked values: */
    updateVisibleName();
}

void UIChooserItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Skip if not hovered: */
    if (!isHovered())
        return;

    /* Hide enter-button: */
    if (m_pEnterButton)
        m_pEnterButton->hide();

    /* Call to base-class: */
    UIChooserItem::hoverLeaveEvent(pEvent);

    /* Update linked values: */
    updateVisibleName();
}

void UIChooserItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget* /* pWidget = 0 */)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
    /* Paint frame: */
    paintFrame(pPainter, rectangle);
    /* Paint header: */
    paintHeader(pPainter, rectangle);
}

QString UIChooserItemGroup::name() const
{
    return m_strName;
}

QString UIChooserItemGroup::fullName() const
{
    /* Return "/" for root item: */
    if (isRoot())
        return "/";

    /* Get full parent name, append with '/' if not yet appended: */
    AssertMsg(parentItem(), ("Incorrect parent set!"));
    QString strFullParentName = parentItem()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIChooserItemGroup::description() const
{
    return m_strDescription;
}

QString UIChooserItemGroup::definition() const
{
    return QString("g=%1").arg(name());
}

void UIChooserItemGroup::startEditing()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Not while saving groups: */
    if (model()->isGroupSavingInProgress())
        return;

    /* Make sure item visible: */
    AssertPtrReturnVoid(parentItem());
    parentItem()->toGroupItem()->makeSureItemIsVisible(this);

    /* Assign name-editor text: */
    m_pNameEditorWidget->setText(name());

    /* Layout name-editor: */
    const int iMargin = data(GroupItemData_VerticalMargin).toInt();
    const int iHeaderHeight = 2 * iMargin + m_minimumHeaderSize.height();
    const QSize headerSize = QSize(geometry().width(), iHeaderHeight);
    const QGraphicsView *pView = model()->scene()->views().first();
    const QPointF viewPoint = pView->mapFromScene(mapToScene(QPointF(0, 0)));
    const QPoint globalPoint = pView->parentWidget()->mapToGlobal(viewPoint.toPoint());
    m_pNameEditorWidget->move(globalPoint);
    m_pNameEditorWidget->resize(headerSize);

    /* Show name-editor: */
    m_pNameEditorWidget->show();
    m_pNameEditorWidget->setFocus();
}

void UIChooserItemGroup::updateToolTip()
{
    /* Not for root item: */
    if (isRoot())
        return;

    /* Prepare variables: */
    QStringList toolTipInfo;

    /* Should we add name? */
    if (!name().isEmpty())
    {
        /* Template: */
        QString strTemplateForName = tr("<b>%1</b>", "Group item tool-tip / Group name");

        /* Append value: */
        toolTipInfo << strTemplateForName.arg(name());
    }

    /* Should we add group info? */
    if (!items(UIChooserItemType_Group).isEmpty())
    {
        /* Template: */
        QString strGroupCount = tr("%n group(s)", "Group item tool-tip / Group info", items(UIChooserItemType_Group).size());

        /* Append value: */
        QString strValue = tr("<nobr>%1</nobr>", "Group item tool-tip / Group info wrapper").arg(strGroupCount);
        toolTipInfo << strValue;
    }

    /* Should we add machine info? */
    if (!items(UIChooserItemType_Machine).isEmpty())
    {
        /* Check if 'this' group contains started VMs: */
        int iCountOfStartedMachineItems = 0;
        foreach (UIChooserItem *pItem, items(UIChooserItemType_Machine))
            if (UIVirtualMachineItem::isItemStarted(pItem->toMachineItem()))
                ++iCountOfStartedMachineItems;
        /* Template: */
        QString strMachineCount = tr("%n machine(s)", "Group item tool-tip / Machine info", items(UIChooserItemType_Machine).size());
        QString strStartedMachineCount = tr("(%n running)", "Group item tool-tip / Running machine info", iCountOfStartedMachineItems);

        /* Append value: */
        QString strValue = !iCountOfStartedMachineItems ?
                           tr("<nobr>%1</nobr>", "Group item tool-tip / Machine info wrapper").arg(strMachineCount) :
                           tr("<nobr>%1 %2</nobr>", "Group item tool-tip / Machine info wrapper, including running").arg(strMachineCount).arg(strStartedMachineCount);
        toolTipInfo << strValue;
    }

    /* Set tool-tip: */
    setToolTip(toolTipInfo.join("<br>"));
}

void UIChooserItemGroup::installEventFilterHelper(QObject *pSource)
{
    /* The only object which need's that filter for now is scroll-area: */
    pSource->installEventFilter(m_pScrollArea);
}

bool UIChooserItemGroup::hasItems(UIChooserItemType type /* = UIChooserItemType_Any */) const
{
    switch (type)
    {
        case UIChooserItemType_Any:
            return hasItems(UIChooserItemType_Global) || hasItems(UIChooserItemType_Group) || hasItems(UIChooserItemType_Machine);
        case UIChooserItemType_Global:
            return !m_globalItems.isEmpty();
        case UIChooserItemType_Group:
            return !m_groupItems.isEmpty();
        case UIChooserItemType_Machine:
            return !m_machineItems.isEmpty();
    }
    return false;
}

QList<UIChooserItem*> UIChooserItemGroup::items(UIChooserItemType type /* = UIChooserItemType_Any */) const
{
    switch (type)
    {
        case UIChooserItemType_Any: return items(UIChooserItemType_Global) + items(UIChooserItemType_Group) + items(UIChooserItemType_Machine);
        case UIChooserItemType_Global: return m_globalItems;
        case UIChooserItemType_Group: return m_groupItems;
        case UIChooserItemType_Machine: return m_machineItems;
        default: break;
    }
    return QList<UIChooserItem*>();
}

void UIChooserItemGroup::setItems(const QList<UIChooserItem*> &items, UIChooserItemType type)
{
    /* Check item type: */
    switch (type)
    {
        case UIChooserItemType_Global: m_globalItems = items; break;
        case UIChooserItemType_Group: m_groupItems = items; break;
        case UIChooserItemType_Machine: m_machineItems = items; break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }

    /* Update linked values: */
    updateLayoutSpacings();
    updateItemCountInfo();
    updateToolTip();
    updateGeometry();
}

void UIChooserItemGroup::clearItems(UIChooserItemType type /* = UIChooserItemType_Any */)
{
    switch (type)
    {
        case UIChooserItemType_Any:
        {
            clearItems(UIChooserItemType_Global);
            clearItems(UIChooserItemType_Group);
            clearItems(UIChooserItemType_Machine);
            break;
        }
        case UIChooserItemType_Global:
        {
            while (!m_globalItems.isEmpty()) { delete m_globalItems.last(); }
            AssertMsg(m_globalItems.isEmpty(), ("Global items cleanup failed!"));
            break;
        }
        case UIChooserItemType_Group:
        {
            while (!m_groupItems.isEmpty()) { delete m_groupItems.last(); }
            AssertMsg(m_groupItems.isEmpty(), ("Group items cleanup failed!"));
            break;
        }
        case UIChooserItemType_Machine:
        {
            while (!m_machineItems.isEmpty()) { delete m_machineItems.last(); }
            AssertMsg(m_machineItems.isEmpty(), ("Machine items cleanup failed!"));
            break;
        }
    }

    /* Update linked values: */
    updateLayoutSpacings();
    updateItemCountInfo();
    updateToolTip();
    updateGeometry();
}

void UIChooserItemGroup::addItem(UIChooserItem *pItem, bool fFavorite, int iPosition)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIChooserItemType_Global:
        {
            AssertMsg(!m_globalItems.contains(pItem), ("Global-item already added!"));
            if (iPosition < 0 || iPosition >= m_globalItems.size())
            {
                if (fFavorite)
                    m_pLayoutFavorite->addItem(pItem);
                else
                    m_pLayoutGlobal->addItem(pItem);
                m_globalItems.append(pItem);
            }
            else
            {
                if (fFavorite)
                    m_pLayoutFavorite->insertItem(iPosition, pItem);
                else
                    m_pLayoutGlobal->insertItem(iPosition, pItem);
                m_globalItems.insert(iPosition, pItem);
            }
            break;
        }
        case UIChooserItemType_Group:
        {
            AssertMsg(!m_groupItems.contains(pItem), ("Group-item already added!"));
            if (iPosition < 0 || iPosition >= m_groupItems.size())
            {
                m_pLayoutGroup->addItem(pItem);
                m_groupItems.append(pItem);
            }
            else
            {
                m_pLayoutGroup->insertItem(iPosition, pItem);
                m_groupItems.insert(iPosition, pItem);
            }
            break;
        }
        case UIChooserItemType_Machine:
        {
            AssertMsg(!m_machineItems.contains(pItem), ("Machine-item already added!"));
            if (iPosition < 0 || iPosition >= m_machineItems.size())
            {
                m_pLayoutMachine->addItem(pItem);
                m_machineItems.append(pItem);
            }
            else
            {
                m_pLayoutMachine->insertItem(iPosition, pItem);
                m_machineItems.insert(iPosition, pItem);
            }
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }

    /* Update linked values: */
    updateLayoutSpacings();
    updateItemCountInfo();
    updateToolTip();
    updateGeometry();
}

void UIChooserItemGroup::removeItem(UIChooserItem *pItem)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIChooserItemType_Global:
        {
            AssertMsg(m_globalItems.contains(pItem), ("Global-item was not found!"));
            m_globalItems.removeAt(m_globalItems.indexOf(pItem));
            break;
        }
        case UIChooserItemType_Group:
        {
            AssertMsg(m_groupItems.contains(pItem), ("Group-item was not found!"));
            m_groupItems.removeAt(m_groupItems.indexOf(pItem));
            break;
        }
        case UIChooserItemType_Machine:
        {
            AssertMsg(m_machineItems.contains(pItem), ("Machine-item was not found!"));
            m_machineItems.removeAt(m_machineItems.indexOf(pItem));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }

    /* Update linked values: */
    updateLayoutSpacings();
    updateItemCountInfo();
    updateToolTip();
    updateGeometry();
}

void UIChooserItemGroup::updateAllItems(const QUuid &uId)
{
    /* Update all the required items recursively: */
    foreach (UIChooserItem *pItem, items())
        pItem->updateAllItems(uId);
}

void UIChooserItemGroup::removeAllItems(const QUuid &uId)
{
    /* Remove all the required items recursively: */
    foreach (UIChooserItem *pItem, items())
        pItem->removeAllItems(uId);
}

UIChooserItem* UIChooserItemGroup::searchForItem(const QString &strSearchTag, int iItemSearchFlags)
{
    /* Are we searching among group-items? */
    if (iItemSearchFlags & UIChooserItemSearchFlag_Group)
    {
        /* Are we searching by the exact name? */
        if (iItemSearchFlags & UIChooserItemSearchFlag_ExactName)
        {
            /* Exact name matches? */
            if (name() == strSearchTag)
                return this;
        }
        /* Are we searching by the few first symbols? */
        else
        {
            /* Name starts with passed symbols? */
            if (name().startsWith(strSearchTag, Qt::CaseInsensitive))
                return this;
        }
    }

    /* Search among all the children, but machines first: */
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Machine))
        if (UIChooserItem *pFoundItem = pItem->searchForItem(strSearchTag, iItemSearchFlags))
            return pFoundItem;
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Global))
        if (UIChooserItem *pFoundItem = pItem->searchForItem(strSearchTag, iItemSearchFlags))
            return pFoundItem;
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Group))
        if (UIChooserItem *pFoundItem = pItem->searchForItem(strSearchTag, iItemSearchFlags))
            return pFoundItem;

    /* Found nothing? */
    return 0;
}

UIChooserItem *UIChooserItemGroup::firstMachineItem()
{
    /* If this group-item have at least one machine-item: */
    if (hasItems(UIChooserItemType_Machine))
        /* Return the first machine-item: */
        return items(UIChooserItemType_Machine).first()->firstMachineItem();
    /* If this group-item have at least one group-item: */
    else if (hasItems(UIChooserItemType_Group))
        /* Return the first machine-item of the first group-item: */
        return items(UIChooserItemType_Group).first()->firstMachineItem();
    /* Found nothing? */
    return 0;
}

void UIChooserItemGroup::sortItems()
{
    /* Sort group-items: */
    QMap<QString, UIChooserItem*> sorter;
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Group))
        sorter.insert(pItem->name().toLower(), pItem);
    setItems(sorter.values(), UIChooserItemType_Group);

    /* Sort machine-items: */
    sorter.clear();
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Machine))
        sorter.insert(pItem->name().toLower(), pItem);
    setItems(sorter.values(), UIChooserItemType_Machine);

    /* Update model: */
    model()->updateNavigation();
    model()->updateLayout();
}

void UIChooserItemGroup::updateGeometry()
{
    /* Call to base-class: */
    UIChooserItem::updateGeometry();

    /* Update/activate children layout: */
    m_pLayout->updateGeometry();
    m_pLayout->activate();
}

void UIChooserItemGroup::updateLayout()
{
    /* Prepare variables: */
    const int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    const int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    const int iParentIndent = data(GroupItemData_ParentIndent).toInt();
    const int iFullHeaderHeight = m_minimumHeaderSize.height();
    int iPreviousVerticalIndent = 0;

    /* Header (root-item): */
    if (isRoot())
    {
        /* Acquire view: */
        const QGraphicsView *pView = model()->scene()->views().first();

#if 0
        /* Header (non-main root-item): */
        if (!isMainRoot())
        {
            /* Hide unnecessary buttons: */
            if (m_pToggleButton)
                m_pToggleButton->hide();
            if (m_pEnterButton)
                m_pEnterButton->hide();

            /* Exit-button: */
            if (m_pExitButton)
            {
                /* Prepare variables: */
                int iExitButtonHeight = m_exitButtonSize.height();
                /* Layout exit-button: */
                int iExitButtonX = iHorizontalMargin;
                int iExitButtonY = iExitButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                   iVerticalMargin + (iFullHeaderHeight - iExitButtonHeight) / 2;
                m_pExitButton->setPos(iExitButtonX, iExitButtonY);
                /* Show exit-button: */
                m_pExitButton->show();
            }

            /* Prepare body indent: */
            iPreviousVerticalIndent = iVerticalMargin + iFullHeaderHeight + iVerticalMargin;
        }
#endif

        /* Adjust scroll-view geometry: */
        QSize viewSize = pView->size();
        viewSize.setHeight(viewSize.height() - iPreviousVerticalIndent);
        /* Adjust favorite children container: */
        m_pContainerFavorite->resize(viewSize.width(), m_pContainerFavorite->minimumSizeHint().height());
        m_pContainerFavorite->setPos(0, iPreviousVerticalIndent);
        iPreviousVerticalIndent += m_pContainerFavorite->minimumSizeHint().height();
        /* Adjust other children scroll-area: */
        m_pScrollArea->resize(viewSize.width(), viewSize.height() - m_pContainerFavorite->minimumSizeHint().height());
        m_pScrollArea->setPos(0, iPreviousVerticalIndent);
    }
    /* Header (non-root-item): */
    else
    {
        /* Hide unnecessary button: */
        if (m_pExitButton)
            m_pExitButton->hide();

        /* Toggle-button: */
        if (m_pToggleButton)
        {
            /* Prepare variables: */
            int iToggleButtonHeight = m_toggleButtonSize.height();
            /* Layout toggle-button: */
            int iToggleButtonX = iHorizontalMargin + iParentIndent * level();
            int iToggleButtonY = iToggleButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                 iVerticalMargin + (iFullHeaderHeight - iToggleButtonHeight) / 2;
            m_pToggleButton->setPos(iToggleButtonX, iToggleButtonY);
            /* Show toggle-button: */
            m_pToggleButton->show();
        }

        /* Enter-button: */
        if (m_pEnterButton)
        {
            /* Prepare variables: */
            int iFullWidth = (int)geometry().width();
            int iEnterButtonWidth = m_enterButtonSize.width();
            int iEnterButtonHeight = m_enterButtonSize.height();
            /* Layout enter-button: */
            int iEnterButtonX = iFullWidth - iHorizontalMargin - iEnterButtonWidth;
            int iEnterButtonY = iEnterButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                iVerticalMargin + (iFullHeaderHeight - iEnterButtonHeight) / 2;
            m_pEnterButton->setPos(iEnterButtonX, iEnterButtonY);
        }

        /* Prepare body indent: */
        iPreviousVerticalIndent = 2 * iVerticalMargin + iFullHeaderHeight;

        /* Adjust scroll-view geometry: */
        QSize itemSize = size().toSize();
        itemSize.setHeight(itemSize.height() - iPreviousVerticalIndent);
        /* Adjust favorite children container: */
        m_pContainerFavorite->resize(itemSize.width(), m_pContainerFavorite->minimumSizeHint().height());
        m_pContainerFavorite->setPos(0, iPreviousVerticalIndent);
        iPreviousVerticalIndent += m_pContainerFavorite->minimumSizeHint().height();
        /* Adjust other children scroll-area: */
        m_pScrollArea->resize(itemSize.width(), itemSize.height() - m_pContainerFavorite->minimumSizeHint().height());
        m_pScrollArea->setPos(0, iPreviousVerticalIndent);
    }

    /* No body for closed group: */
    if (isClosed())
    {
        m_pContainerFavorite->hide();
        m_pScrollArea->hide();
    }
    /* Body for opened group: */
    else
    {
        m_pContainerFavorite->show();
        m_pScrollArea->show();
        foreach (UIChooserItem *pItem, items())
            pItem->updateLayout();
    }
}

int UIChooserItemGroup::minimumWidthHint() const
{
    return minimumWidthHintForGroup(isOpened());
}

int UIChooserItemGroup::minimumHeightHint() const
{
    return minimumHeightHintForGroup(isOpened());
}

QSizeF UIChooserItemGroup::sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (enmWhich == Qt::MinimumSize)
        return minimumSizeHintForGroup(isOpened());
    /* Else call to base-class: */
    return UIChooserItem::sizeHint(enmWhich, constraint);
}

void UIChooserItemGroup::makeSureItemIsVisible(UIChooserItem *pItem)
{
    /* Make sure item exists: */
    AssertPtrReturnVoid(pItem);

    /* Convert child rectangle to local coordinates for this group. This also
     * works for a child at any sub-level, doesn't necessary of this group. */
    const QPointF positionInScene = pItem->mapToScene(QPointF(0, 0));
    const QPointF positionInGroup = mapFromScene(positionInScene);
    const QRectF itemRectInGroup = QRectF(positionInGroup, pItem->size());
    m_pScrollArea->makeSureRectIsVisible(itemRectInGroup);
}

QPixmap UIChooserItemGroup::toPixmap()
{
    /* Ask item to paint itself into pixmap: */
    qreal dDpr = gpManager->windowHandle()->devicePixelRatio();
    QSize actualSize = size().toSize();
    QPixmap pixmap(actualSize * dDpr);
    pixmap.setDevicePixelRatio(dDpr);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), actualSize);
    paint(&painter, &options);
    return pixmap;
}

bool UIChooserItemGroup::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, UIChooserItemDragToken where) const
{
    /* No drops while saving groups: */
    if (model()->isGroupSavingInProgress())
        return false;
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != UIChooserItemDragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should check mime format: */
    if (pMimeData->hasFormat(UIChooserItemGroup::className()))
    {
        /* Get passed group-item: */
        const UIChooserItemMimeData *pCastedMimeData = qobject_cast<const UIChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
        UIChooserItem *pItem = pCastedMimeData->item();
        /* Make sure passed group is mutable within this group: */
        if (pItem->toGroupItem()->isContainsLockedMachine() &&
            !items(UIChooserItemType_Group).contains(pItem))
            return false;
        /* Make sure passed group is not 'this': */
        if (pItem == this)
            return false;
        /* Make sure passed group is not among our parents: */
        const UIChooserItem *pTestedWidget = this;
        while (UIChooserItem *pParentOfTestedWidget = pTestedWidget->parentItem())
        {
            if (pItem == pParentOfTestedWidget)
                return false;
            pTestedWidget = pParentOfTestedWidget;
        }
        return true;
    }
    else if (pMimeData->hasFormat(UIChooserItemMachine::className()))
    {
        /* Get passed machine-item: */
        const UIChooserItemMimeData *pCastedMimeData = qobject_cast<const UIChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
        UIChooserItem *pItem = pCastedMimeData->item();
        /* Make sure passed machine is mutable within this group: */
        if (pItem->toMachineItem()->isLockedMachine() &&
            !items(UIChooserItemType_Machine).contains(pItem))
            return false;
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            {
                /* Make sure passed item is ours or there is no other item with such id: */
                return m_machineItems.contains(pItem) || !isContainsMachine(pItem->toMachineItem()->id());
            }
            case Qt::CopyAction:
            {
                /* Make sure there is no other item with such id: */
                return !isContainsMachine(pItem->toMachineItem()->id());
            }
            default: break; /* Shut up, MSC! */
        }
    }
    /* That was invalid mime: */
    return false;
}

void UIChooserItemGroup::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, UIChooserItemDragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Check mime format: */
    if (pMime->hasFormat(UIChooserItemGroup::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIChooserModel *pModel = model();

                /* Get passed group-item: */
                const UIChooserItemMimeData *pCastedMime = qobject_cast<const UIChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
                UIChooserItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_groupItems.size();
                if (pFromWho && where != UIChooserItemDragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_groupItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_groupItems.contains(pFromWho))
                    {
                        iPosition = m_groupItems.indexOf(pFromWho);
                        if (where == UIChooserItemDragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed group-item into this group: */
                UIChooserItem *pNewGroupItem = new UIChooserItemGroup(this, pItem->toGroupItem(), iPosition);
                if (isClosed())
                    open(false);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update model: */
                pModel->wipeOutEmptyGroups();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewGroupItem);
                pModel->saveGroupSettings();
                break;
            }
            default:
                break;
        }
    }
    else if (pMime->hasFormat(UIChooserItemMachine::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIChooserModel *pModel = model();

                /* Get passed machine-item: */
                const UIChooserItemMimeData *pCastedMime = qobject_cast<const UIChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
                UIChooserItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_machineItems.size();
                if (pFromWho && where != UIChooserItemDragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_machineItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_machineItems.contains(pFromWho))
                    {
                        iPosition = m_machineItems.indexOf(pFromWho);
                        if (where == UIChooserItemDragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed machine-item into this group: */
                UIChooserItem *pNewMachineItem = new UIChooserItemMachine(this, pItem->toMachineItem(), iPosition);
                if (isClosed())
                    open(false);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update model: */
                pModel->wipeOutEmptyGroups();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewMachineItem);
                pModel->saveGroupSettings();
                break;
            }
            default:
                break;
        }
    }
}

void UIChooserItemGroup::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != UIChooserItemDragToken_Off)
    {
        setDragTokenPlace(UIChooserItemDragToken_Off);
        update();
    }
    /* Reset drag tokens for all the items: */
    foreach (UIChooserItem *pItem, items())
        pItem->resetDragToken();
}

QMimeData* UIChooserItemGroup::createMimeData()
{
    return new UIChooserItemMimeData(this);
}

void UIChooserItemGroup::sltHandleWindowRemapped()
{
    /* Update pixmaps: */
    updatePixmaps();
}

void UIChooserItemGroup::sltNameEditingFinished()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Close name-editor: */
    m_pNameEditorWidget->close();

    /* Enumerate all the group names: */
    QStringList groupNames;
    foreach (UIChooserItem *pItem, parentItem()->items(UIChooserItemType_Group))
        groupNames << pItem->name();
    /* If proposed name is empty or not unique, reject it: */
    QString strNewName = m_pNameEditorWidget->text().trimmed();
    if (strNewName.isEmpty() || groupNames.contains(strNewName))
        return;

    /* We should replace forbidden symbols
     * with ... well, probably underscores: */
    strNewName.replace(QRegExp("[\\\\/:*?\"<>]"), "_");

    /* Set new name, save settings: */
    setName(strNewName);
    model()->saveGroupSettings();
}

void UIChooserItemGroup::sltGroupToggleStart()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Toggle started: */
    emit sigToggleStarted();

    /* Setup animation: */
    updateAnimationParameters();

    /* Group closed, we are opening it: */
    if (m_fClosed)
    {
        /* Toggle-state and navigation will be
         * updated on toggle-finish signal! */
    }
    /* Group opened, we are closing it: */
    else
    {
        /* Update toggle-state: */
        m_fClosed = true;
        /* Update geometry: */
        updateGeometry();
        /* Update navigation: */
        model()->updateNavigation();
        /* Relayout model: */
        model()->updateLayout();
    }
}

void UIChooserItemGroup::sltGroupToggleFinish(bool fToggled)
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Update toggle-state: */
    m_fClosed = !fToggled;
    /* Update geometry: */
    updateGeometry();
    /* Update navigation: */
    model()->updateNavigation();
    /* Relayout model: */
    model()->updateLayout();
    /* Update toggle-button tool-tip: */
    updateToggleButtonToolTip();

    /* Toggle finished: */
    emit sigToggleFinished();
}

void UIChooserItemGroup::sltIndentRoot()
{
    /* Unhover before indenting: */
    setHovered(false);
}

void UIChooserItemGroup::sltUnindentRoot()
{
    /* Unhover before unindenting: */
    setHovered(false);
}

void UIChooserItemGroup::prepare()
{
    /* Prepare self: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);
    m_infoFont = font();
    m_minimumHeaderSize = QSize(0, 0);

    /* Prepare header widgets of root item: */
    if (isRoot())
    {
#if 0
        /* Except main root ofc: */
        if (!isMainRoot())
        {
            /* Setup exit-button: */
            m_pExitButton = new UIGraphicsButton(this, UIIconPool::iconSet(":/previous_16px.png"));
            if (m_pExitButton)
            {
                m_pExitButton->hide();
                QSizeF sh = m_pExitButton->minimumSizeHint();
                m_pExitButton->setTransformOriginPoint(sh.width() / 2, sh.height() / 2);
                connect(m_pExitButton, &UIGraphicsButton::sigButtonClicked,
                        this, &UIChooserItemGroup::sltUnindentRoot);
            }
            m_exitButtonSize = m_pExitButton ? m_pExitButton->minimumSizeHint().toSize() : QSize(0, 0);
        }
#endif
    }
    /* Prepare header widgets of non-root item: */
    else
    {
        /* Setup toggle-button: */
        m_pToggleButton = new UIGraphicsRotatorButton(this, "additionalHeight", isOpened());
        if (m_pToggleButton)
        {
            m_pToggleButton->hide();
            connect(m_pToggleButton, &UIGraphicsRotatorButton::sigRotationStart,
                    this, &UIChooserItemGroup::sltGroupToggleStart);
            connect(m_pToggleButton, &UIGraphicsRotatorButton::sigRotationFinish,
                    this, &UIChooserItemGroup::sltGroupToggleFinish);
        }
        m_toggleButtonSize = m_pToggleButton ? m_pToggleButton->minimumSizeHint().toSize() : QSize(0, 0);

        /* Setup enter-button: */
        m_pEnterButton = new UIGraphicsButton(this, UIIconPool::iconSet(":/next_16px.png"));
        if (m_pEnterButton)
        {
            m_pEnterButton->hide();
            connect(m_pEnterButton, &UIGraphicsButton::sigButtonClicked,
                    this, &UIChooserItemGroup::sltIndentRoot);
        }
        m_enterButtonSize = m_pEnterButton ? m_pEnterButton->minimumSizeHint().toSize() : QSize(0, 0);

        /* Setup name-editor: */
        m_pNameEditorWidget = new UIEditorGroupRename(name());
        if (m_pNameEditorWidget)
        {
            m_pNameEditorWidget->setFont(m_nameFont);
            connect(m_pNameEditorWidget, &UIEditorGroupRename::sigEditingFinished,
                    this, &UIChooserItemGroup::sltNameEditingFinished);
        }
    }

    /* Prepare favorite children container: */
    m_pContainerFavorite = new QIGraphicsWidget(this);
    if (m_pContainerFavorite)
    {
        /* Make it always above other children scroll-area: */
        m_pContainerFavorite->setZValue(1);

        /* Prepare favorite children layout: */
        m_pLayoutFavorite = new QGraphicsLinearLayout(Qt::Vertical, m_pContainerFavorite);
        if (m_pLayoutFavorite)
        {
            m_pLayoutFavorite->setContentsMargins(0, 0, 0, 0);
            m_pLayoutFavorite->setSpacing(0);
        }
    }

    /* Prepare scroll-area: */
    m_pScrollArea = new UIGraphicsScrollArea(Qt::Vertical, this);
    if (m_pScrollArea)
    {
        /* Prepare container: */
        m_pContainer = new QIGraphicsWidget;
        if (m_pContainer)
        {
            /* Prepare layout: */
            m_pLayout = new QGraphicsLinearLayout(Qt::Vertical, m_pContainer);
            if (m_pLayout)
            {
                m_pLayout->setContentsMargins(0, 0, 0, 0);
                m_pLayout->setSpacing(0);

                /* Prepare global layout: */
                m_pLayoutGlobal = new QGraphicsLinearLayout(Qt::Vertical);
                if (m_pLayoutGlobal)
                {
                    m_pLayoutGlobal->setContentsMargins(0, 0, 0, 0);
                    m_pLayoutGlobal->setSpacing(1);
                    m_pLayout->addItem(m_pLayoutGlobal);
                }

                /* Prepare group layout: */
                m_pLayoutGroup = new QGraphicsLinearLayout(Qt::Vertical);
                if (m_pLayoutGroup)
                {
                    m_pLayoutGroup->setContentsMargins(0, 0, 0, 0);
                    m_pLayoutGroup->setSpacing(1);
                    m_pLayout->addItem(m_pLayoutGroup);
                }

                /* Prepare machine layout: */
                m_pLayoutMachine = new QGraphicsLinearLayout(Qt::Vertical);
                if (m_pLayoutMachine)
                {
                    m_pLayoutMachine->setContentsMargins(0, 0, 0, 0);
                    m_pLayoutMachine->setSpacing(1);
                    m_pLayout->addItem(m_pLayoutMachine);
                }
            }

            /* Assign to scroll-area: */
            m_pScrollArea->setViewport(m_pContainer);
        }
    }
}

QVariant UIChooserItemGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupItemData_HorizonalMargin: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;
        case GroupItemData_VerticalMargin:  return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;
        case GroupItemData_HeaderSpacing:   return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;
        case GroupItemData_ChildrenSpacing: return 1;
        case GroupItemData_ParentIndent:    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 3;

        /* Default: */
        default: break;
    }
    return QVariant();
}

int UIChooserItemGroup::additionalHeight() const
{
    return m_iAdditionalHeight;
}

void UIChooserItemGroup::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
    updateGeometry();
    model()->updateLayout();
}

void UIChooserItemGroup::updateAnimationParameters()
{
    /* Only for item with button: */
    if (!m_pToggleButton)
        return;

    /* Recalculate animation parameters: */
    QSizeF openedSize = minimumSizeHintForGroup(true);
    QSizeF closedSize = minimumSizeHintForGroup(false);
    int iAdditionalHeight = (int)(openedSize.height() - closedSize.height());
    m_pToggleButton->setAnimationRange(0, iAdditionalHeight);
}

void UIChooserItemGroup::updateToggleButtonToolTip()
{
    /* Only for item with button: */
    if (!m_pToggleButton)
        return;

    /* Update toggle-button tool-tip: */
    m_pToggleButton->setToolTip(isOpened() ? tr("Collapse group") : tr("Expand group"));
}

/* static */
void UIChooserItemGroup::copyContent(UIChooserItemGroup *pFrom, UIChooserItemGroup *pTo)
{
    /* Copy group-items: */
    foreach (UIChooserItem *pGroupItem, pFrom->items(UIChooserItemType_Group))
        new UIChooserItemGroup(pTo, pGroupItem->toGroupItem());
    /* Copy global-items: */
    foreach (UIChooserItem *pGlobalItem, pFrom->items(UIChooserItemType_Global))
        new UIChooserItemGlobal(pTo, pGlobalItem->toGlobalItem());
    /* Copy machine-items: */
    foreach (UIChooserItem *pMachineItem, pFrom->items(UIChooserItemType_Machine))
        new UIChooserItemMachine(pTo, pMachineItem->toMachineItem());
}

bool UIChooserItemGroup::isContainsMachine(const QUuid &uId) const
{
    /* Check each machine-item: */
    foreach (UIChooserItem *pItem, m_machineItems)
        if (pItem->toMachineItem()->id() == uId)
            return true;
    /* Found nothing? */
    return false;
}

bool UIChooserItemGroup::isContainsLockedMachine()
{
    /* Check each machine-item: */
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Machine))
        if (pItem->toMachineItem()->isLockedMachine())
            return true;
    /* Check each group-item: */
    foreach (UIChooserItem *pItem, items(UIChooserItemType_Group))
        if (pItem->toGroupItem()->isContainsLockedMachine())
            return true;
    /* Found nothing? */
    return false;
}

void UIChooserItemGroup::updateItemCountInfo()
{
    /* Not for root item: */
    if (isRoot())
        return;

    /* Update item info attributes: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    QString strInfoGroups = m_groupItems.isEmpty() ? QString() : QString::number(m_groupItems.size());
    QString strInfoMachines = m_machineItems.isEmpty() ? QString() : QString::number(m_machineItems.size());
    QSize infoSizeGroups = textSize(m_infoFont, pPaintDevice, strInfoGroups);
    QSize infoSizeMachines = textSize(m_infoFont, pPaintDevice, strInfoMachines);

    /* Update linked values: */
    bool fSomethingChanged = false;
    if (m_strInfoGroups != strInfoGroups)
    {
        m_strInfoGroups = strInfoGroups;
        fSomethingChanged = true;
    }
    if (m_strInfoMachines != strInfoMachines)
    {
        m_strInfoMachines = strInfoMachines;
        fSomethingChanged = true;
    }
    if (m_infoSizeGroups != infoSizeGroups)
    {
        m_infoSizeGroups = infoSizeGroups;
        fSomethingChanged = true;
    }
    if (m_infoSizeMachines != infoSizeMachines)
    {
        m_infoSizeMachines = infoSizeMachines;
        fSomethingChanged = true;
    }
    if (fSomethingChanged)
    {
        updateVisibleName();
        updateMinimumHeaderSize();
    }
}

int UIChooserItemGroup::minimumWidthHintForGroup(bool fGroupOpened) const
{
    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* For root item: */
    if (isRoot())
    {
        /* Main root-item always takes body into account: */
        if (hasItems())
        {
            /* We have to take maximum children width into account: */
            iProposedWidth = qMax(m_pContainerFavorite->minimumSizeHint().width(),
                                  m_pContainer->minimumSizeHint().width());
        }
    }
    /* For other items: */
    else
    {
        /* Prepare variables: */
        const int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
        const int iParentIndent = data(GroupItemData_ParentIndent).toInt();

        /* Basically we have to take header width into account: */
        iProposedWidth += m_minimumHeaderSize.width();

        /* But if group-item is opened: */
        if (fGroupOpened)
        {
            /* We have to take maximum children width into account: */
            iProposedWidth = qMax(m_pContainerFavorite->minimumSizeHint().width(),
                                  m_pContainer->minimumSizeHint().width());
        }

        /* And 2 margins at last - left and right: */
        iProposedWidth += 2 * iHorizontalMargin + iParentIndent * level();
    }

    /* Return result: */
    return iProposedWidth;
}

int UIChooserItemGroup::minimumHeightHintForGroup(bool fGroupOpened) const
{
    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* For root item: */
    if (isRoot())
    {
        /* Main root-item always takes body into account: */
        if (hasItems())
        {
            /* We have to take maximum children height into account: */
            iProposedHeight += m_pContainerFavorite->minimumSizeHint().height();
            iProposedHeight += m_pContainer->minimumSizeHint().height();
        }
    }
    /* For other items: */
    else
    {
        /* Prepare variables: */
        const int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();

        /* Group-item header have 2 margins - top and bottom: */
        iProposedHeight += 2 * iVerticalMargin;
        /* And header content height to take into account: */
        iProposedHeight += m_minimumHeaderSize.height();

        /* But if group-item is opened: */
        if (fGroupOpened)
        {
            /* We have to take maximum children height into account: */
            iProposedHeight += m_pContainerFavorite->minimumSizeHint().height();
            iProposedHeight += m_pContainer->minimumSizeHint().height();
        }

        /* Finally, additional height during animation: */
        if (!fGroupOpened && m_pToggleButton && m_pToggleButton->isAnimationRunning())
            iProposedHeight += m_iAdditionalHeight;
    }

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIChooserItemGroup::minimumSizeHintForGroup(bool fGroupOpened) const
{
    return QSizeF(minimumWidthHintForGroup(fGroupOpened), minimumHeightHintForGroup(fGroupOpened));
}

void UIChooserItemGroup::updateVisibleName()
{
    /* Not for root item: */
    if (isRoot())
        return;

    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iHeaderSpacing = data(GroupItemData_HeaderSpacing).toInt();
    int iToggleButtonWidth = m_toggleButtonSize.width();
    int iEnterButtonWidth = m_enterButtonSize.width();
    int iExitButtonWidth = m_exitButtonSize.width();
    int iGroupPixmapWidth = m_pixmapSizeGroups.width();
    int iMachinePixmapWidth = m_pixmapSizeMachines.width();
    int iGroupCountTextWidth = m_infoSizeGroups.width();
    int iMachineCountTextWidth = m_infoSizeMachines.width();
    int iMaximumWidth = (int)geometry().width();

    /* Left margin: */
    iMaximumWidth -= iHorizontalMargin;
    /* Button width: */
    if (isRoot())
        iMaximumWidth -= iExitButtonWidth;
    else
        iMaximumWidth -= iToggleButtonWidth;
    /* Spacing between button and name: */
    iMaximumWidth -= iHeaderSpacing;
    if (isHovered())
    {
        /* Spacing between name and info: */
        iMaximumWidth -= iHeaderSpacing;
        /* Group info width: */
        if (!m_groupItems.isEmpty())
            iMaximumWidth -= (iGroupPixmapWidth + iGroupCountTextWidth);
        /* Machine info width: */
        if (!m_machineItems.isEmpty())
            iMaximumWidth -= (iMachinePixmapWidth + iMachineCountTextWidth);
        /* Spacing + button width: */
        if (!isRoot())
            iMaximumWidth -= iEnterButtonWidth;
    }
    /* Right margin: */
    iMaximumWidth -= iHorizontalMargin;
    /* Calculate new visible name and name-size: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    QString strVisibleName = compressText(m_nameFont, pPaintDevice, name(), iMaximumWidth);
    QSize visibleNameSize = textSize(m_nameFont, pPaintDevice, strVisibleName);

    /* Update linked values: */
    if (m_visibleNameSize != visibleNameSize)
    {
        m_visibleNameSize = visibleNameSize;
        updateGeometry();
    }
    if (m_strVisibleName != strVisibleName)
    {
        m_strVisibleName = strVisibleName;
        update();
    }
}

void UIChooserItemGroup::updatePixmaps()
{
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_groupsPixmap = UIIconPool::iconSet(":/group_abstract_16px.png").pixmap(gpManager->windowHandle(),
                                                                             QSize(iIconMetric, iIconMetric));
    m_machinesPixmap = UIIconPool::iconSet(":/machine_abstract_16px.png").pixmap(gpManager->windowHandle(),
                                                                                 QSize(iIconMetric, iIconMetric));
    m_pixmapSizeGroups = m_groupsPixmap.size() / m_groupsPixmap.devicePixelRatio();
    m_pixmapSizeMachines = m_machinesPixmap.size() / m_machinesPixmap.devicePixelRatio();
}

void UIChooserItemGroup::updateMinimumHeaderSize()
{
    /* Not for root item: */
    if (isRoot())
        return;

    /* Prepare variables: */
    int iHeaderSpacing = data(GroupItemData_HeaderSpacing).toInt();

    /* Calculate minimum visible name size: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    QFontMetrics fm(m_nameFont, pPaintDevice);
    int iMaximumNameWidth = textWidth(m_nameFont, pPaintDevice, 20);
    QString strCompressedName = compressText(m_nameFont, pPaintDevice, name(), iMaximumNameWidth);
    int iMinimumNameWidth = fm.width(strCompressedName);
    int iMinimumNameHeight = fm.height();

    /* Calculate minimum width: */
    int iHeaderWidth = 0;
    /* Button width: */
    if (isRoot())
        iHeaderWidth += m_exitButtonSize.width();
    else
        iHeaderWidth += m_toggleButtonSize.width();
    iHeaderWidth += /* Spacing between button and name: */
                    iHeaderSpacing +
                    /* Minimum name width: */
                    iMinimumNameWidth +
                    /* Spacing between name and info: */
                    iHeaderSpacing;
    /* Group info width: */
    if (!m_groupItems.isEmpty())
        iHeaderWidth += (m_pixmapSizeGroups.width() + m_infoSizeGroups.width());
    /* Machine info width: */
    if (!m_machineItems.isEmpty())
        iHeaderWidth += (m_pixmapSizeMachines.width() + m_infoSizeMachines.width());
    /* Spacing + button width: */
    if (!isRoot())
        iHeaderWidth += m_enterButtonSize.width();

    /* Calculate maximum height: */
    QList<int> heights;
    /* Button height: */
    if (isRoot())
        heights << m_exitButtonSize.height();
    else
        heights << m_toggleButtonSize.height();
    heights /* Minimum name height: */
            << iMinimumNameHeight
            /* Group info heights: */
            << m_pixmapSizeGroups.height() << m_infoSizeGroups.height()
            /* Machine info heights: */
            << m_pixmapSizeMachines.height() << m_infoSizeMachines.height();
    /* Button height: */
    if (!isRoot())
        heights << m_enterButtonSize.height();
    int iHeaderHeight = 0;
    foreach (int iHeight, heights)
        iHeaderHeight = qMax(iHeaderHeight, iHeight);

    /* Calculate new minimum header size: */
    QSize minimumHeaderSize = QSize(iHeaderWidth, iHeaderHeight);

    /* Is there something changed? */
    if (m_minimumHeaderSize == minimumHeaderSize)
        return;

    /* Update linked values: */
    m_minimumHeaderSize = minimumHeaderSize;
    updateGeometry();
}

void UIChooserItemGroup::updateLayoutSpacings()
{
    m_pLayout->setItemSpacing(0, m_globalItems.isEmpty() ? 0 : 1);
    m_pLayout->setItemSpacing(1, m_groupItems.isEmpty() ? 0 : 1);
    m_pLayout->setItemSpacing(2, m_machineItems.isEmpty() ? 0 : 1);
}

void UIChooserItemGroup::paintBackground(QPainter *pPainter, const QRect &rect)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();
    const QColor headerColor = pal.color(QPalette::Active,
                                         model()->currentItems().contains(this) ?
                                         QPalette::Highlight : QPalette::Midlight);

    /* Root-item: */
    if (isRoot())
    {
#if 0
        /* Non-main root-item: */
        if (!isMainRoot())
        {
            /* Prepare variables: */
            const int iMargin = data(GroupItemData_VerticalMargin).toInt();
            const int iFullHeaderHeight = 2 * iMargin + m_minimumHeaderSize.height();
            QRect headerRect = QRect(0, 0, rect.width(), iFullHeaderHeight);

            /* Fill background: */
            QLinearGradient headerGradient(headerRect.bottomLeft(), headerRect.topLeft());
            headerGradient.setColorAt(1, headerColor.darker(headerDarkness()));
            headerGradient.setColorAt(0, headerColor.darker(animatedValue()));
            pPainter->fillRect(headerRect, headerGradient);
        }
#endif
    }
    /* Non-root-item: */
    else
    {
        /* Prepare variables: */
        const int iMargin = data(GroupItemData_VerticalMargin).toInt();
        const int iFullHeaderHeight = 2 * iMargin + m_minimumHeaderSize.height();

        /* Calculate top rectangle: */
        QRect tRect = rect;
        if (!m_fClosed)
            tRect.setBottom(tRect.top() + iFullHeaderHeight - 1);

        /* Prepare top gradient: */
        QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
        tGradient.setColorAt(1, headerColor.darker(animatedValue()));
        tGradient.setColorAt(0, headerColor.darker(headerDarkness()));

        /* Fill top rectangle: */
        pPainter->fillRect(tRect, tGradient);

        /* Paint drag token UP? */
        if (dragTokenPlace() != UIChooserItemDragToken_Off)
        {
            QLinearGradient dragTokenGradient;
            QRect dragTokenRect = rect;
            if (dragTokenPlace() == UIChooserItemDragToken_Up)
            {
                dragTokenRect.setHeight(5);
                dragTokenGradient.setStart(dragTokenRect.bottomLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
            }
            else if (dragTokenPlace() == UIChooserItemDragToken_Down)
            {
                dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 5));
                dragTokenGradient.setStart(dragTokenRect.topLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
            }
            dragTokenGradient.setColorAt(0, headerColor.darker(dragTokenDarkness()));
            dragTokenGradient.setColorAt(1, headerColor.darker(dragTokenDarkness() + 40));
            pPainter->fillRect(dragTokenRect, dragTokenGradient);
        }
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemGroup::paintFrame(QPainter *pPainter, const QRect &rectangle)
{
    /* Not for roots: */
    if (isRoot())
        return;

    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const int iMargin = data(GroupItemData_VerticalMargin).toInt();
    const int iFullHeaderHeight = 2 * iMargin + m_minimumHeaderSize.height();

    /* Prepare color: */
    const QPalette pal = palette();
    const QColor strokeColor = pal.color(QPalette::Active,
                                         model()->currentItems().contains(this) ?
                                         QPalette::Highlight : QPalette::Midlight).darker(headerDarkness() + 10);

    /* Create/assign pen: */
    QPen pen(strokeColor);
    pen.setWidth(0);
    pPainter->setPen(pen);

    /* Calculate top rectangle: */
    QRect topRect = rectangle;
    if (!m_fClosed)
        topRect.setBottom(topRect.top() + iFullHeaderHeight - 1);

    /* Draw borders: */
    pPainter->drawLine(topRect.bottomLeft(), topRect.bottomRight() + QPoint(1, 0));
    pPainter->drawLine(topRect.topLeft(),    topRect.bottomLeft());

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemGroup::paintHeader(QPainter *pPainter, const QRect &rect)
{
    /* Not for root item: */
    if (isRoot())
        return;

    /* Prepare variables: */
    const int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    const int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    const int iHeaderSpacing = data(GroupItemData_HeaderSpacing).toInt();
    const int iParentIndent = data(GroupItemData_ParentIndent).toInt();
    const int iFullHeaderHeight = m_minimumHeaderSize.height();

    /* Configure painter color: */
    pPainter->setPen(palette().color(QPalette::Active,
                                     model()->currentItems().contains(this) ?
                                     QPalette::HighlightedText : QPalette::ButtonText));

    /* Paint name: */
    int iNameX = iHorizontalMargin + iParentIndent * level();
    if (isRoot())
        iNameX += m_exitButtonSize.width();
    else
        iNameX += m_toggleButtonSize.width();
    iNameX += iHeaderSpacing;
    int iNameY = m_visibleNameSize.height() == iFullHeaderHeight ? iVerticalMargin :
                 iVerticalMargin + (iFullHeaderHeight - m_visibleNameSize.height()) / 2;
    paintText(/* Painter: */
              pPainter,
              /* Point to paint in: */
              QPoint(iNameX, iNameY),
              /* Font to paint text: */
              m_nameFont,
              /* Paint device: */
              model()->paintDevice(),
              /* Text to paint: */
              m_strVisibleName);

    /* Should we add more info? */
    if (isHovered())
    {
        /* Prepare variables: */
        int iEnterButtonWidth = m_enterButtonSize.width();

        /* Indent: */
        int iHorizontalIndent = rect.right() - iHorizontalMargin;
        if (!isRoot())
            iHorizontalIndent -= iEnterButtonWidth;

        /* Should we draw machine count info? */
        if (!m_strInfoMachines.isEmpty())
        {
            iHorizontalIndent -= m_infoSizeMachines.width();
            int iMachineCountTextX = iHorizontalIndent;
            int iMachineCountTextY = m_infoSizeMachines.height() == iFullHeaderHeight ?
                                     iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - m_infoSizeMachines.height()) / 2;
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iMachineCountTextX, iMachineCountTextY),
                      /* Font to paint text: */
                      m_infoFont,
                      /* Paint device: */
                      model()->paintDevice(),
                      /* Text to paint: */
                      m_strInfoMachines);

            iHorizontalIndent -= m_pixmapSizeMachines.width();
            int iMachinePixmapX = iHorizontalIndent;
            int iMachinePixmapY = m_pixmapSizeMachines.height() == iFullHeaderHeight ?
                                  iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - m_pixmapSizeMachines.height()) / 2;
            paintPixmap(/* Painter: */
                        pPainter,
                        /* Point to paint in: */
                        QPoint(iMachinePixmapX, iMachinePixmapY),
                        /* Pixmap to paint: */
                        m_machinesPixmap);
        }

        /* Should we draw group count info? */
        if (!m_strInfoGroups.isEmpty())
        {
            iHorizontalIndent -= m_infoSizeGroups.width();
            int iGroupCountTextX = iHorizontalIndent;
            int iGroupCountTextY = m_infoSizeGroups.height() == iFullHeaderHeight ?
                                   iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - m_infoSizeGroups.height()) / 2;
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iGroupCountTextX, iGroupCountTextY),
                      /* Font to paint text: */
                      m_infoFont,
                      /* Paint device: */
                      model()->paintDevice(),
                      /* Text to paint: */
                      m_strInfoGroups);

            iHorizontalIndent -= m_pixmapSizeGroups.width();
            int iGroupPixmapX = iHorizontalIndent;
            int iGroupPixmapY = m_pixmapSizeGroups.height() == iFullHeaderHeight ?
                                iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - m_pixmapSizeGroups.height()) / 2;
            paintPixmap(/* Painter: */
                        pPainter,
                        /* Point to paint in: */
                        QPoint(iGroupPixmapX, iGroupPixmapY),
                        /* Pixmap to paint: */
                        m_groupsPixmap);
        }
    }
}


/*********************************************************************************************************************************
*   Class UIEditorGroupRename implementation.                                                                                    *
*********************************************************************************************************************************/

UIEditorGroupRename::UIEditorGroupRename(const QString &strName)
    : QWidget(0, Qt::Popup)
    , m_pLineEdit(0)
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    if (pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create line-edit: */
        m_pLineEdit = new QLineEdit(strName);
        if (m_pLineEdit)
        {
            setFocusProxy(m_pLineEdit);
            m_pLineEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            m_pLineEdit->setTextMargins(0, 0, 0, 0);
            connect(m_pLineEdit, &QLineEdit::returnPressed,
                    this, &UIEditorGroupRename::sigEditingFinished);
        }

        /* Add into layout: */
        pLayout->addWidget(m_pLineEdit);
    }
}

QString UIEditorGroupRename::text() const
{
    return m_pLineEdit->text();
}

void UIEditorGroupRename::setText(const QString &strText)
{
    m_pLineEdit->setText(strText);
}

void UIEditorGroupRename::setFont(const QFont &font)
{
    QWidget::setFont(font);
    m_pLineEdit->setFont(font);
}
