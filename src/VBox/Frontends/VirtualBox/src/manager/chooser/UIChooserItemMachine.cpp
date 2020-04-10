/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemMachine class implementation.
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

/* Qt includes: */
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWindow>

/* GUI includes: */
#include "UIChooserItemGroup.h"
#include "UIChooserItemMachine.h"
#include "UIChooserModel.h"
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeMachine.h"
#include "UIIconPool.h"
#include "UIVirtualBoxManager.h"
#include "UIVirtualMachineItemLocal.h"

/* Other VBox includes: */
#include "iprt/cpp/utils.h"


UIChooserItemMachine::UIChooserItemMachine(UIChooserItem *pParent, UIChooserNodeMachine *pNode)
    : UIChooserItem(pParent, pNode, 0, 100)
    , m_iDefaultLightnessMin(0)
    , m_iDefaultLightnessMax(0)
    , m_iHoverLightnessMin(0)
    , m_iHoverLightnessMax(0)
    , m_iHighlightLightnessMin(0)
    , m_iHighlightLightnessMax(0)
    , m_iFirstRowMaximumWidth(0)
    , m_iMinimumNameWidth(0)
    , m_iMaximumNameWidth(0)
    , m_iMinimumSnapshotNameWidth(0)
    , m_iMaximumSnapshotNameWidth(0)
{
    prepare();
}

UIChooserItemMachine::~UIChooserItemMachine()
{
    cleanup();
}

QUuid UIChooserItemMachine::id() const
{
    return node()->toMachineNode()->cache()->id();
}

bool UIChooserItemMachine::accessible() const
{
    return node()->toMachineNode()->cache()->accessible();
}

void UIChooserItemMachine::recache()
{
    node()->toMachineNode()->cache()->recache();
}

bool UIChooserItemMachine::isLockedMachine() const
{
    const KMachineState enmState = node()->toMachineNode()->cache()->machineState();
    return enmState != KMachineState_PoweredOff &&
           enmState != KMachineState_Saved &&
           enmState != KMachineState_Teleported &&
           enmState != KMachineState_Aborted;
}

bool UIChooserItemMachine::isToolButtonArea(const QPoint &position, int iMarginMultiplier /* = 1 */) const
{
    const int iFullWidth = geometry().width();
    const int iFullHeight = geometry().height();
    const int iMarginHR = data(MachineItemData_MarginHR).toInt();
    const int iButtonMargin = data(MachineItemData_ButtonMargin).toInt();
    const int iToolPixmapX = iFullWidth - iMarginHR - 1 - m_toolPixmap.width() / m_toolPixmap.devicePixelRatio();
    const int iToolPixmapY = (iFullHeight - m_toolPixmap.height() / m_toolPixmap.devicePixelRatio()) / 2;
    QRect rect = QRect(iToolPixmapX,
                       iToolPixmapY,
                       m_toolPixmap.width() / m_toolPixmap.devicePixelRatio(),
                       m_toolPixmap.height() / m_toolPixmap.devicePixelRatio());
    rect.adjust(-iMarginMultiplier * iButtonMargin, -iMarginMultiplier * iButtonMargin,
                 iMarginMultiplier * iButtonMargin,  iMarginMultiplier * iButtonMargin);
    return rect.contains(position);
}

/* static */
QString UIChooserItemMachine::className()
{
    return "UIChooserItemMachine";
}

/* static */
void UIChooserItemMachine::enumerateMachineItems(const QList<UIChooserItem*> &il,
                                                 QList<UIChooserItemMachine*> &ol,
                                                 int iEnumerationFlags /* = 0 */)
{
    /* Enumerate all the passed items: */
    foreach (UIChooserItem *pItem, il)
    {
        /* If that is machine-item: */
        if (pItem->type() == UIChooserNodeType_Machine)
        {
            /* Get the iterated machine-item: */
            UIChooserItemMachine *pMachineItem = pItem->toMachineItem();
            /* Skip if exactly this item is already enumerated: */
            if (ol.contains(pMachineItem))
                continue;
            /* Skip if item with same ID is already enumerated but we need unique: */
            if ((iEnumerationFlags & UIChooserItemMachineEnumerationFlag_Unique) &&
                checkIfContains(ol, pMachineItem))
                continue;
            /* Skip if this item is accessible and we no need it: */
            if ((iEnumerationFlags & UIChooserItemMachineEnumerationFlag_Inaccessible) &&
                pMachineItem->accessible())
                continue;
            /* Add it: */
            ol << pMachineItem;
        }
        /* If that is group-item: */
        else if (pItem->type() == UIChooserNodeType_Group)
        {
            /* Enumerate all the machine-items recursively: */
            enumerateMachineItems(pItem->items(UIChooserNodeType_Machine), ol, iEnumerationFlags);
            /* Enumerate all the group-items recursively: */
            enumerateMachineItems(pItem->items(UIChooserNodeType_Group), ol, iEnumerationFlags);
        }
    }
}

void UIChooserItemMachine::retranslateUi()
{
}

void UIChooserItemMachine::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::showEvent(pEvent);

    /* Recache and update pixmaps: */
    node()->toMachineNode()->cache()->recachePixmap();
    updatePixmaps();
}

void UIChooserItemMachine::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::resizeEvent(pEvent);

    /* What is the new geometry? */
    const QRectF newGeometry = geometry();

    /* Should we update visible name? */
    if (previousGeometry().width() != newGeometry.width())
        updateFirstRowMaximumWidth();

    /* Remember the new geometry: */
    setPreviousGeometry(newGeometry);
}

void UIChooserItemMachine::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::mousePressEvent(pEvent);
    /* No drag for inaccessible: */
    if (!accessible())
        pEvent->ignore();
}

void UIChooserItemMachine::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget* /* pWidget = 0 */)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
    /* Paint frame: */
    paintFrame(pPainter, rectangle);
    /* Paint machine info: */
    paintMachineInfo(pPainter, rectangle);
}

void UIChooserItemMachine::startEditing()
{
    AssertMsgFailed(("Machine graphics item do NOT support editing yet!"));
}

void UIChooserItemMachine::updateItem()
{
    /* Update this machine-item: */
    updatePixmaps();
    updateMinimumNameWidth();
    updateVisibleName();
    updateMinimumSnapshotNameWidth();
    updateVisibleSnapshotName();
    updateStateTextSize();
    updateToolTip();
    update();

    /* Update parent group-item: */
    parentItem()->updateToolTip();
    parentItem()->update();
}

void UIChooserItemMachine::updateToolTip()
{
    setToolTip(node()->toMachineNode()->cache()->toolTipText());
}

QList<UIChooserItem*> UIChooserItemMachine::items(UIChooserNodeType) const
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
    return QList<UIChooserItem*>();
}

void UIChooserItemMachine::addItem(UIChooserItem*, bool, int)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIChooserItemMachine::removeItem(UIChooserItem*)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

UIChooserItem* UIChooserItemMachine::searchForItem(const QString &strSearchTag, int iItemSearchFlags)
{
    /* Ignore if we are not searching for the machine-item: */
    if (!(iItemSearchFlags & UIChooserItemSearchFlag_Machine))
        return 0;

    /* Are we searching by the exact ID? */
    if (iItemSearchFlags & UIChooserItemSearchFlag_ExactId)
    {
        /* Exact ID doesn't match? */
        if (id() != QUuid(strSearchTag))
            return 0;
    }
    /* Are we searching by the exact name? */
    else if (iItemSearchFlags & UIChooserItemSearchFlag_ExactName)
    {
        /* Exact name doesn't match? */
        if (name() != strSearchTag)
            return 0;
    }
    /* Are we searching by the few first symbols? */
    else
    {
        /* Name doesn't start with passed symbols? */
        if (!name().startsWith(strSearchTag, Qt::CaseInsensitive))
            return 0;
    }

    /* Returning this: */
    return this;
}

UIChooserItem *UIChooserItemMachine::firstMachineItem()
{
    return this;
}

void UIChooserItemMachine::updateLayout()
{
    // Just do nothing ..
}

int UIChooserItemMachine::minimumWidthHint() const
{
    /* Prepare variables: */
    const int iMarginHL = data(MachineItemData_MarginHL).toInt();
    const int iMarginHR = data(MachineItemData_MarginHR).toInt();
    const int iMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    const int iMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    const int iButtonMargin = data(MachineItemData_ButtonMargin).toInt();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Two margins: */
    iProposedWidth += iMarginHL + iMarginHR;
    /* And machine-item content to take into account: */
    int iTopLineWidth = m_iMinimumNameWidth;
    if (   node()->toMachineNode()->cache()->itemType() == UIVirtualMachineItem::ItemType_Local
        && !node()->toMachineNode()->cache()->toLocal()->snapshotName().isEmpty())
        iTopLineWidth += (iMinorSpacing +
                          m_iMinimumSnapshotNameWidth);
    int iBottomLineWidth = m_statePixmapSize.width() +
                           iMinorSpacing +
                           m_stateTextSize.width();
    int iMiddleColumnWidth = qMax(iTopLineWidth, iBottomLineWidth);
    int iMachineItemWidth = m_pixmapSize.width() +
                            iMajorSpacing +
                            iMiddleColumnWidth +
                            iMajorSpacing +
                            m_toolPixmapSize.width() + 2 * iButtonMargin;
    iProposedWidth += iMachineItemWidth;

    /* Return result: */
    return iProposedWidth;
}

int UIChooserItemMachine::minimumHeightHint() const
{
    /* Prepare variables: */
    const int iMarginV = data(MachineItemData_MarginV).toInt();
    const int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    const int iButtonMargin = data(MachineItemData_ButtonMargin).toInt();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMarginV;
    /* And machine-item content to take into account: */
    int iTopLineHeight = qMax(m_visibleNameSize.height(), m_visibleSnapshotNameSize.height());
    int iBottomLineHeight = qMax(m_statePixmapSize.height(), m_stateTextSize.height());
    int iMiddleColumnHeight = iTopLineHeight +
                              iMachineItemTextSpacing +
                              iBottomLineHeight;
    QList<int> heights;
    heights << m_pixmapSize.height() << iMiddleColumnHeight << m_toolPixmapSize.height() + 2 * iButtonMargin;
    int iMaxHeight = 0;
    foreach (int iHeight, heights)
        iMaxHeight = qMax(iMaxHeight, iHeight);
    iProposedHeight += iMaxHeight;

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIChooserItemMachine::sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (enmWhich == Qt::MinimumSize)
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    /* Else call to base-class: */
    return UIChooserItem::sizeHint(enmWhich, constraint);
}

QPixmap UIChooserItemMachine::toPixmap()
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

bool UIChooserItemMachine::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, UIChooserItemDragToken where) const
{
    /* No drops while saving groups: */
    if (model()->isGroupSavingInProgress())
        return false;
    /* No drops for immutable item: */
    if (isLockedMachine())
        return false;
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != UIChooserItemDragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should make sure machine is accessible: */
    if (!accessible())
        return false;
    /* Else we should try to cast mime to known classes: */
    if (pMimeData->hasFormat(UIChooserItemMachine::className()))
    {
        /* Make sure passed item id is not ours: */
        const UIChooserItemMimeData *pCastedMimeData = qobject_cast<const UIChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
        UIChooserItem *pItem = pCastedMimeData->item();
        UIChooserItemMachine *pMachineItem = pItem->toMachineItem();
        /* Make sure passed machine is mutable: */
        if (pMachineItem->isLockedMachine())
            return false;
        return pMachineItem->id() != id();
    }
    /* That was invalid mime: */
    return false;
}

void UIChooserItemMachine::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, UIChooserItemDragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Make sure this handler called by this item (not by children): */
    AssertMsg(!pFromWho && where == UIChooserItemDragToken_Off, ("Machine graphics item do NOT support children!"));
    Q_UNUSED(pFromWho);
    Q_UNUSED(where);
    if (pMime->hasFormat(UIChooserItemMachine::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIChooserModel *pModel = model();

                /* Get passed item: */
                const UIChooserItemMimeData *pCastedMime = qobject_cast<const UIChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIChooserItemMimeData!"));
                UIChooserNode *pNode = pCastedMime->item()->node();

                /* Group passed item with current-item into the new group: */
                UIChooserNodeGroup *pNewGroupNode = new UIChooserNodeGroup(parentItem()->node(),
                                                                           false /* favorite */,
                                                                           parentItem()->node()->nodes().size(),
                                                                           UIChooserModel::uniqueGroupName(parentItem()->node()),
                                                                           true /* true */);
                UIChooserItemGroup *pNewGroupItem = new UIChooserItemGroup(parentItem(), pNewGroupNode);
                UIChooserNodeMachine *pNewMachineNode1 = new UIChooserNodeMachine(pNewGroupNode,
                                                                                  node()->toMachineNode(),
                                                                                  pNewGroupNode->nodes().size());
                new UIChooserItemMachine(pNewGroupItem, pNewMachineNode1);
                UIChooserNodeMachine *pNewMachineNode2 = new UIChooserNodeMachine(pNewGroupNode,
                                                                                  pNode->toMachineNode(),
                                                                                  pNewGroupNode->nodes().size());
                new UIChooserItemMachine(pNewGroupItem, pNewMachineNode2);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed node: */
                    delete pNode;
                }
                /* Delete this node: */
                delete node();

                /* Update model: */
                pModel->wipeOutEmptyGroups();
                pModel->updateNavigationItemList();
                pModel->updateLayout();
                pModel->setSelectedItem(pNewGroupItem);
                pModel->saveGroupSettings();
                break;
            }
            default:
                break;
        }
    }
}

void UIChooserItemMachine::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != UIChooserItemDragToken_Off)
    {
        setDragTokenPlace(UIChooserItemDragToken_Off);
        update();
    }
}

QMimeData* UIChooserItemMachine::createMimeData()
{
    return new UIChooserItemMimeData(this);
}

void UIChooserItemMachine::sltHandleWindowRemapped()
{
    /* Recache and update pixmaps: */
    node()->toMachineNode()->cache()->recachePixmap();
    updatePixmaps();
}

void UIChooserItemMachine::prepare()
{
    /* Color tones: */
#ifdef VBOX_WS_MAC
    m_iHighlightLightnessMin = 105;
    m_iHighlightLightnessMax = 115;
    m_iHoverLightnessMin = 115;
    m_iHoverLightnessMax = 125;
    m_iDefaultLightnessMin = 145;
    m_iDefaultLightnessMax = 155;
#else /* VBOX_WS_MAC */
    m_iHighlightLightnessMin = 130;
    m_iHighlightLightnessMax = 160;
    m_iHoverLightnessMin = 160;
    m_iHoverLightnessMax = 190;
    m_iDefaultLightnessMin = 160;
    m_iDefaultLightnessMax = 190;
#endif /* !VBOX_WS_MAC */

    /* Fonts: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);
    m_snapshotNameFont = font();
    m_stateTextFont = font();

    /* Sizes: */
    m_iFirstRowMaximumWidth = 0;
    m_iMinimumNameWidth = 0;
    m_iMaximumNameWidth = 0;
    m_iMinimumSnapshotNameWidth = 0;
    m_iMaximumSnapshotNameWidth = 0;

    /* Add item to the parent: */
    AssertPtrReturnVoid(parentItem());
    parentItem()->addItem(this, isFavorite(), position());

    /* Configure connections: */
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIChooserItemMachine::sltHandleWindowRemapped);
    connect(model(), &UIChooserModel::sigSelectionChanged,
            this, &UIChooserItemMachine::sltUpdateFirstRowMaximumWidth);
    connect(this, &UIChooserItemMachine::sigHoverEnter,
            this, &UIChooserItemMachine::sltUpdateFirstRowMaximumWidth);
    connect(this, &UIChooserItemMachine::sigHoverLeave,
            this, &UIChooserItemMachine::sltUpdateFirstRowMaximumWidth);

    /* Init: */
    updateItem();

    /* Apply language settings: */
    retranslateUi();
}

void UIChooserItemMachine::cleanup()
{
    /* If that item is current: */
    if (model()->currentItem() == this)
    {
        /* Unset current-item: */
        model()->setCurrentItem(0);
    }
    /* If that item is in selection list: */
    if (model()->selectedItems().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromSelectedItems(this);
    }
    /* If that item is in navigation list: */
    if (model()->navigationItems().contains(this))
    {
        /* Remove item from the navigation list: */
        model()->removeFromNavigationItems(this);
    }

    /* Remove item from the parent: */
    AssertPtrReturnVoid(parentItem());
    parentItem()->removeItem(this);
}

QVariant UIChooserItemMachine::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case MachineItemData_MarginHL:     return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        case MachineItemData_MarginHR:     return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4 * 5;
        case MachineItemData_MarginV:      return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4 * 3;
        case MachineItemData_MajorSpacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;
        case MachineItemData_MinorSpacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;
        case MachineItemData_TextSpacing:  return 0;
        case MachineItemData_ButtonMargin: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIChooserItemMachine::updatePixmaps()
{
    /* Update pixmap: */
    updatePixmap();
    /* Update state-pixmap: */
    updateStatePixmap();
    /* Update tool-pixmap: */
    updateToolPixmap();
}

void UIChooserItemMachine::updatePixmap()
{
    /* Get new pixmap and pixmap-size: */
    QSize pixmapSize;
    QPixmap pixmap = node()->toMachineNode()->cache()->osPixmap(&pixmapSize);
    /* Update linked values: */
    if (m_pixmapSize != pixmapSize)
    {
        m_pixmapSize = pixmapSize;
        updateFirstRowMaximumWidth();
        updateGeometry();
    }
    if (m_pixmap.toImage() != pixmap.toImage())
    {
        m_pixmap = pixmap;
        update();
    }
}

void UIChooserItemMachine::updateStatePixmap()
{
    /* Determine icon metric: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    /* Get new state-pixmap and state-pixmap size: */
    const QIcon stateIcon = node()->toMachineNode()->cache()->machineStateIcon();
    AssertReturnVoid(!stateIcon.isNull());
    const QSize statePixmapSize = QSize(iIconMetric, iIconMetric);
    const QPixmap statePixmap = stateIcon.pixmap(gpManager->windowHandle(), statePixmapSize);
    /* Update linked values: */
    if (m_statePixmapSize != statePixmapSize)
    {
        m_statePixmapSize = statePixmapSize;
        updateGeometry();
    }
    if (m_statePixmap.toImage() != statePixmap.toImage())
    {
        m_statePixmap = statePixmap;
        update();
    }
}

void UIChooserItemMachine::updateToolPixmap()
{
    /* Determine icon metric: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize) * .75;
    /* Create new tool-pixmap and tool-pixmap size: */
    const QIcon toolIcon = UIIconPool::iconSet(":/tools_menu_24px.png");
    AssertReturnVoid(!toolIcon.isNull());
    const QSize toolPixmapSize = QSize(iIconMetric, iIconMetric);
    const QPixmap toolPixmap = toolIcon.pixmap(gpManager->windowHandle(), toolPixmapSize);
    /* Update linked values: */
    if (m_toolPixmapSize != toolPixmapSize)
    {
        m_toolPixmapSize = toolPixmapSize;
        updateGeometry();
    }
    if (m_toolPixmap.toImage() != toolPixmap.toImage())
    {
        m_toolPixmap = toolPixmap;
        update();
    }
}

void UIChooserItemMachine::updateFirstRowMaximumWidth()
{
    /* Prepare variables: */
    const int iMarginHL = data(MachineItemData_MarginHL).toInt();
    const int iMarginHR = data(MachineItemData_MarginHR).toInt();
    const int iMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    const int iButtonMargin = data(MachineItemData_ButtonMargin).toInt();

    /* Calculate new maximum width for the first row: */
    int iFirstRowMaximumWidth = (int)geometry().width();
    iFirstRowMaximumWidth -= iMarginHL; /* left margin */
    iFirstRowMaximumWidth -= m_pixmapSize.width(); /* left pixmap width */
    iFirstRowMaximumWidth -= iMajorSpacing; /* spacing between left pixmap and name(s) */
    if (   model()->firstSelectedItem() == this
        || isHovered())
    {
        iFirstRowMaximumWidth -= iMajorSpacing; /* spacing between name(s) and right pixmap */
        iFirstRowMaximumWidth -= m_toolPixmapSize.width() + 2 * iButtonMargin; /* right pixmap width */
    }
    iFirstRowMaximumWidth -= iMarginHR; /* right margin */

    /* Is there something changed? */
    if (m_iFirstRowMaximumWidth == iFirstRowMaximumWidth)
        return;

    /* Update linked values: */
    m_iFirstRowMaximumWidth = iFirstRowMaximumWidth;
    updateMaximumNameWidth();
    updateMaximumSnapshotNameWidth();
}

void UIChooserItemMachine::updateMinimumNameWidth()
{
    /* Calculate new minimum name width: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    QFontMetrics fm(m_nameFont, pPaintDevice);
    int iMinimumNameWidth = fm.width(compressText(m_nameFont, pPaintDevice, name(), textWidth(m_nameFont, pPaintDevice, 15)));

    /* Is there something changed? */
    if (m_iMinimumNameWidth == iMinimumNameWidth)
        return;

    /* Update linked values: */
    m_iMinimumNameWidth = iMinimumNameWidth;
    updateGeometry();
}

void UIChooserItemMachine::updateMinimumSnapshotNameWidth()
{
    /* Calculate new minimum snapshot-name width: */
    int iMinimumSnapshotNameWidth = 0;
    /* Is there any snapshot exists? */
    if (   node()->toMachineNode()->cache()->itemType() == UIVirtualMachineItem::ItemType_Local
        && !node()->toMachineNode()->cache()->toLocal()->snapshotName().isEmpty())
    {
        QFontMetrics fm(m_snapshotNameFont, model()->paintDevice());
        int iBracketWidth = fm.width("()"); /* bracket width */
        int iActualTextWidth = fm.width(node()->toMachineNode()->cache()->toLocal()->snapshotName()); /* snapshot-name width */
        int iMinimumTextWidth = fm.width("..."); /* ellipsis width */
        iMinimumSnapshotNameWidth = iBracketWidth + qMin(iActualTextWidth, iMinimumTextWidth);
    }

    /* Is there something changed? */
    if (m_iMinimumSnapshotNameWidth == iMinimumSnapshotNameWidth)
        return;

    /* Update linked values: */
    m_iMinimumSnapshotNameWidth = iMinimumSnapshotNameWidth;
    updateMaximumNameWidth();
    updateGeometry();
}

void UIChooserItemMachine::updateMaximumNameWidth()
{
    /* Calculate new maximum name width: */
    int iMaximumNameWidth = m_iFirstRowMaximumWidth;
    /* Do we have a minimum snapshot-name width? */
    if (m_iMinimumSnapshotNameWidth != 0)
    {
        /* Prepare variables: */
        int iMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
        /* Take spacing and snapshot-name into account: */
        iMaximumNameWidth -= (iMinorSpacing + m_iMinimumSnapshotNameWidth);
    }

    /* Is there something changed? */
    if (m_iMaximumNameWidth == iMaximumNameWidth)
        return;

    /* Update linked values: */
    m_iMaximumNameWidth = iMaximumNameWidth;
    updateVisibleName();
}

void UIChooserItemMachine::updateMaximumSnapshotNameWidth()
{
    /* Prepare variables: */
    int iMinorSpacing = data(MachineItemData_MinorSpacing).toInt();

    /* Calculate new maximum snapshot-name width: */
    int iMaximumSnapshotNameWidth = m_iFirstRowMaximumWidth;
    iMaximumSnapshotNameWidth -= (iMinorSpacing + m_visibleNameSize.width());

    /* Is there something changed? */
    if (m_iMaximumSnapshotNameWidth == iMaximumSnapshotNameWidth)
        return;

    /* Update linked values: */
    m_iMaximumSnapshotNameWidth = iMaximumSnapshotNameWidth;
    updateVisibleSnapshotName();
}

void UIChooserItemMachine::updateVisibleName()
{
    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new visible name and name-size: */
    QString strVisibleName = compressText(m_nameFont, pPaintDevice, name(), m_iMaximumNameWidth);
    QSize visibleNameSize = textSize(m_nameFont, pPaintDevice, strVisibleName);

    /* Update linked values: */
    if (m_visibleNameSize != visibleNameSize)
    {
        m_visibleNameSize = visibleNameSize;
        updateMaximumSnapshotNameWidth();
        updateGeometry();
    }
    if (m_strVisibleName != strVisibleName)
    {
        m_strVisibleName = strVisibleName;
        update();
    }
}

void UIChooserItemMachine::updateVisibleSnapshotName()
{
    /* Make sure this is local machine item: */
    if (node()->toMachineNode()->cache()->itemType() != UIVirtualMachineItem::ItemType_Local)
        return;

    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new visible snapshot-name: */
    int iBracketWidth = QFontMetrics(m_snapshotNameFont, pPaintDevice).width("()");
    QString strVisibleSnapshotName = compressText(m_snapshotNameFont, pPaintDevice, node()->toMachineNode()->cache()->toLocal()->snapshotName(),
                                                  m_iMaximumSnapshotNameWidth - iBracketWidth);
    strVisibleSnapshotName = QString("(%1)").arg(strVisibleSnapshotName);
    QSize visibleSnapshotNameSize = textSize(m_snapshotNameFont, pPaintDevice, strVisibleSnapshotName);

    /* Update linked values: */
    if (m_visibleSnapshotNameSize != visibleSnapshotNameSize)
    {
        m_visibleSnapshotNameSize = visibleSnapshotNameSize;
        updateGeometry();
    }
    if (m_strVisibleSnapshotName != strVisibleSnapshotName)
    {
        m_strVisibleSnapshotName = strVisibleSnapshotName;
        update();
    }
}

void UIChooserItemMachine::updateStateTextSize()
{
    /* Get new state-text and state-text size: */
    const QSize stateTextSize = textSize(m_stateTextFont, model()->paintDevice(), node()->toMachineNode()->cache()->machineStateName());

    /* Update linked values: */
    if (m_stateTextSize != stateTextSize)
    {
        m_stateTextSize = stateTextSize;
        updateGeometry();
    }
}

void UIChooserItemMachine::paintBackground(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();

    /* Selected-item background: */
    if (model()->selectedItems().contains(unconst(this)))
    {
        /* Prepare color: */
        QColor backgroundColor = pal.color(QPalette::Active, QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iHighlightLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iHighlightLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);

        if (isHovered())
        {
            /* Prepare color: */
            QColor animationColor1 = QColor(Qt::white);
            QColor animationColor2 = QColor(Qt::white);
#ifdef VBOX_WS_MAC
            animationColor1.setAlpha(90);
#else
            animationColor1.setAlpha(30);
#endif
            animationColor2.setAlpha(0);
            /* Draw hovered-item animated gradient: */
            QRect animatedRect = rectangle;
            animatedRect.setWidth(animatedRect.height());
            const int iLength = 2 * animatedRect.width() + rectangle.width();
            const int iShift = - animatedRect.width() + iLength * animatedValue() / 100;
            animatedRect.moveLeft(iShift);
            QLinearGradient bgAnimatedGrad(animatedRect.topLeft(), animatedRect.bottomRight());
            bgAnimatedGrad.setColorAt(0,   animationColor2);
            bgAnimatedGrad.setColorAt(0.1, animationColor2);
            bgAnimatedGrad.setColorAt(0.5, animationColor1);
            bgAnimatedGrad.setColorAt(0.9, animationColor2);
            bgAnimatedGrad.setColorAt(1,   animationColor2);
            pPainter->fillRect(rectangle, bgAnimatedGrad);
        }
    }
    /* Hovered-item background: */
    else if (isHovered())
    {
        /* Prepare color: */
        QColor backgroundColor = pal.color(QPalette::Active, QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iHoverLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iHoverLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);

        /* Prepare color: */
        QColor animationColor1 = QColor(Qt::white);
        QColor animationColor2 = QColor(Qt::white);
#ifdef VBOX_WS_MAC
        animationColor1.setAlpha(120);
#else
        animationColor1.setAlpha(50);
#endif
        animationColor2.setAlpha(0);
        /* Draw hovered-item animated gradient: */
        QRect animatedRect = rectangle;
        animatedRect.setWidth(animatedRect.height());
        const int iLength = 2 * animatedRect.width() + rectangle.width();
        const int iShift = - animatedRect.width() + iLength * animatedValue() / 100;
        animatedRect.moveLeft(iShift);
        QLinearGradient bgAnimatedGrad(animatedRect.topLeft(), animatedRect.bottomRight());
        bgAnimatedGrad.setColorAt(0,   animationColor2);
        bgAnimatedGrad.setColorAt(0.1, animationColor2);
        bgAnimatedGrad.setColorAt(0.5, animationColor1);
        bgAnimatedGrad.setColorAt(0.9, animationColor2);
        bgAnimatedGrad.setColorAt(1,   animationColor2);
        pPainter->fillRect(rectangle, bgAnimatedGrad);
    }
    /* Default background: */
    else
    {
        /* Prepare color: */
        QColor backgroundColor = pal.color(QPalette::Active, QPalette::Mid);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iDefaultLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iDefaultLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);
    }

    /* Paint drag token UP? */
    if (dragTokenPlace() != UIChooserItemDragToken_Off)
    {
        /* Window color: */
        QColor color1;
        QColor color2;

        QLinearGradient dragTokenGradient;
        QRect dragTokenRect = rectangle;
        if (dragTokenPlace() == UIChooserItemDragToken_Up)
        {
            /* Selected-item background: */
            if (model()->selectedItems().contains(unconst(this)))
            {
                QColor backgroundColor = pal.color(QPalette::Active, QPalette::Highlight);
                color1 = backgroundColor.lighter(m_iHighlightLightnessMax);
                color2 = backgroundColor.lighter(m_iHighlightLightnessMax - 70);
            }
            /* Default background: */
            else
            {
                QColor backgroundColor = pal.color(QPalette::Active, QPalette::Mid);
                color1 = backgroundColor.lighter(m_iDefaultLightnessMax);
                color2 = backgroundColor.lighter(m_iDefaultLightnessMax - 70);
            }

            dragTokenRect.setHeight(5);
            dragTokenGradient.setStart(dragTokenRect.bottomLeft());
            dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
        }
        else if (dragTokenPlace() == UIChooserItemDragToken_Down)
        {
            /* Selected-item background: */
            if (model()->selectedItems().contains(unconst(this)))
            {
                QColor backgroundColor = pal.color(QPalette::Active, QPalette::Highlight);
                color1 = backgroundColor.lighter(m_iHighlightLightnessMin);
                color2 = backgroundColor.lighter(m_iHighlightLightnessMin - 40);
            }
            /* Default background: */
            else
            {
                QColor backgroundColor = pal.color(QPalette::Active, QPalette::Mid);
                color1 = backgroundColor.lighter(m_iDefaultLightnessMin);
                color2 = backgroundColor.lighter(m_iDefaultLightnessMin - 40);
            }

            dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 4));
            dragTokenGradient.setStart(dragTokenRect.topLeft());
            dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
        }
        dragTokenGradient.setColorAt(0, color1);
        dragTokenGradient.setColorAt(1, color2);
        pPainter->fillRect(dragTokenRect, dragTokenGradient);
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemMachine::paintFrame(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();
    QColor strokeColor;

    /* Selected-item frame: */
    if (model()->selectedItems().contains(unconst(this)))
        strokeColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(m_iHighlightLightnessMin - 40);
    /* Hovered-item frame: */
    else if (isHovered())
        strokeColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(m_iHoverLightnessMin - 50);
    /* Default frame: */
    else
        strokeColor = pal.color(QPalette::Active, QPalette::Mid).lighter(m_iDefaultLightnessMin);

    /* Create/assign pen: */
    QPen pen(strokeColor);
    pen.setWidth(0);
    pPainter->setPen(pen);

    /* Draw borders: */
    if (dragTokenPlace() != UIChooserItemDragToken_Up)
        pPainter->drawLine(rectangle.topLeft(),    rectangle.topRight()    + QPoint(1, 0));
    if (dragTokenPlace() != UIChooserItemDragToken_Down)
        pPainter->drawLine(rectangle.bottomLeft(), rectangle.bottomRight() + QPoint(1, 0));
    pPainter->drawLine(rectangle.topLeft(),    rectangle.bottomLeft());

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemMachine::paintMachineInfo(QPainter *pPainter, const QRect &rectangle) const
{
    /* Prepare variables: */
    const int iFullWidth = rectangle.width();
    const int iFullHeight = rectangle.height();
    const int iMarginHL = data(MachineItemData_MarginHL).toInt();
    const int iMarginHR = data(MachineItemData_MarginHR).toInt();
    const int iMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    const int iMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    const int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    const int iButtonMargin = data(MachineItemData_ButtonMargin).toInt();

    /* Selected or hovered item foreground: */
    if (model()->selectedItems().contains(unconst(this)) || isHovered())
    {
        /* Prepare color: */
        QPalette pal = palette();
        QColor highlight = pal.color(QPalette::Active, QPalette::Highlight);
        QColor hhl = highlight.lighter(m_iHoverLightnessMax);
        if (hhl.value() - hhl.saturation() > 0)
            pPainter->setPen(pal.color(QPalette::Active, QPalette::Text));
        else
            pPainter->setPen(pal.color(QPalette::Active, QPalette::HighlightedText));
    }

    /* Calculate indents: */
    int iLeftColumnIndent = iMarginHL;

    /* Paint left column: */
    {
        /* Prepare variables: */
        int iMachinePixmapX = iLeftColumnIndent;
        int iMachinePixmapY = (iFullHeight - m_pixmap.height() / m_pixmap.devicePixelRatio()) / 2;
        /* Paint pixmap: */
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Point to paint in: */
                    QPoint(iMachinePixmapX, iMachinePixmapY),
                    /* Pixmap to paint: */
                    m_pixmap);
    }

    /* Calculate indents: */
    int iMiddleColumnIndent = iLeftColumnIndent +
                              m_pixmapSize.width() +
                              iMajorSpacing;

    /* Paint middle column: */
    {
        /* Calculate indents: */
        int iTopLineHeight = qMax(m_visibleNameSize.height(), m_visibleSnapshotNameSize.height());
        int iBottomLineHeight = qMax(m_statePixmapSize.height(), m_stateTextSize.height());
        int iRightColumnHeight = iTopLineHeight + iMachineItemTextSpacing + iBottomLineHeight;
        int iTopLineIndent = (iFullHeight - iRightColumnHeight) / 2 - 1;

        /* Paint top line: */
        {
            /* Paint left element: */
            {
                /* Prepare variables: */
                int iNameX = iMiddleColumnIndent;
                int iNameY = iTopLineIndent;
                /* Paint name: */
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
            }

            /* Calculate indents: */
            int iSnapshotNameIndent = iMiddleColumnIndent +
                                      m_visibleNameSize.width() +
                                      iMinorSpacing;

            /* Paint middle element: */
            if (   node()->toMachineNode()->cache()->itemType() == UIVirtualMachineItem::ItemType_Local
                && !node()->toMachineNode()->cache()->toLocal()->snapshotName().isEmpty())
            {
                /* Prepare variables: */
                int iSnapshotNameX = iSnapshotNameIndent;
                int iSnapshotNameY = iTopLineIndent;
                /* Paint snapshot-name: */
                paintText(/* Painter: */
                          pPainter,
                          /* Point to paint in: */
                          QPoint(iSnapshotNameX, iSnapshotNameY),
                          /* Font to paint text: */
                          m_snapshotNameFont,
                          /* Paint device: */
                          model()->paintDevice(),
                          /* Text to paint: */
                          m_strVisibleSnapshotName);
            }
        }

        /* Calculate indents: */
        int iBottomLineIndent = iTopLineIndent + iTopLineHeight + 1;

        /* Paint bottom line: */
        {
            /* Paint left element: */
            {
                /* Prepare variables: */
                int iMachineStatePixmapX = iMiddleColumnIndent;
                int iMachineStatePixmapY = iBottomLineIndent;
                /* Paint state pixmap: */
                paintPixmap(/* Painter: */
                            pPainter,
                            /* Point to paint in: */
                            QPoint(iMachineStatePixmapX, iMachineStatePixmapY),
                            /* Pixmap to paint: */
                            m_statePixmap);
            }

            /* Calculate indents: */
            int iMachineStateTextIndent = iMiddleColumnIndent +
                                          m_statePixmapSize.width() +
                                          iMinorSpacing;

            /* Paint right element: */
            {
                /* Prepare variables: */
                int iMachineStateTextX = iMachineStateTextIndent;
                int iMachineStateTextY = iBottomLineIndent + 1;
                /* Paint state text: */
                paintText(/* Painter: */
                          pPainter,
                          /* Point to paint in: */
                          QPoint(iMachineStateTextX, iMachineStateTextY),
                          /* Font to paint text: */
                          m_stateTextFont,
                          /* Paint device: */
                          model()->paintDevice(),
                          /* Text to paint: */
                          node()->toMachineNode()->cache()->machineStateName());
            }
        }
    }

    /* Calculate indents: */
    QGraphicsView *pView = model()->scene()->views().first();
    const QPointF sceneCursorPosition = pView->mapToScene(pView->mapFromGlobal(QCursor::pos()));
    const QPoint itemCursorPosition = mapFromScene(sceneCursorPosition).toPoint();
    int iRightColumnIndent = iFullWidth - iMarginHR - 1 - m_toolPixmap.width() / m_toolPixmap.devicePixelRatio();

    /* Paint right column: */
    if (   model()->firstSelectedItem() == this
        || isHovered())
    {
        /* Prepare variables: */
        const int iToolPixmapX = iRightColumnIndent;
        const int iToolPixmapY = (iFullHeight - m_toolPixmap.height() / m_toolPixmap.devicePixelRatio()) / 2;
        QRect toolButtonRectangle = QRect(iToolPixmapX,
                                          iToolPixmapY,
                                          m_toolPixmap.width() / m_toolPixmap.devicePixelRatio(),
                                          m_toolPixmap.height() / m_toolPixmap.devicePixelRatio());
        toolButtonRectangle.adjust(- iButtonMargin, -iButtonMargin, iButtonMargin, iButtonMargin);

        /* Paint tool button: */
        if (   isHovered()
            && isToolButtonArea(itemCursorPosition, 4))
            paintFlatButton(/* Painter: */
                            pPainter,
                            /* Button rectangle: */
                            toolButtonRectangle,
                            /* Cursor position: */
                            itemCursorPosition);

        /* Paint pixmap: */
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Point to paint in: */
                    QPoint(iToolPixmapX, iToolPixmapY),
                    /* Pixmap to paint: */
                    m_toolPixmap);
    }
}

/* static */
bool UIChooserItemMachine::checkIfContains(const QList<UIChooserItemMachine*> &list, UIChooserItemMachine *pItem)
{
    /* Check if passed list contains passed machine-item id: */
    foreach (UIChooserItemMachine *pIteratedItem, list)
        if (pIteratedItem->id() == pItem->id())
            return true;
    /* Found nothing? */
    return false;
}
