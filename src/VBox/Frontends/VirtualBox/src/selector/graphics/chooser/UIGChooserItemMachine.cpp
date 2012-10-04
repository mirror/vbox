/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItemMachine class implementation
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
#include <QGraphicsSceneMouseEvent>

/* GUI includes: */
#include "UIGChooserItemMachine.h"
#include "UIGChooserItemGroup.h"
#include "UIGChooserModel.h"
#include "UIGraphicsToolBar.h"
#include "UIGraphicsZoomButton.h"
#include "VBoxGlobal.h"
#include "UIIconPool.h"
#include "UIActionPoolSelector.h"
#include "UIImageTools.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* static */
QString UIGChooserItemMachine::className() { return "UIGChooserItemMachine"; }

UIGChooserItemMachine::UIGChooserItemMachine(UIGChooserItem *pParent,
                                             const CMachine &machine,
                                             int iPosition /* = -1 */)
    : UIGChooserItem(pParent, pParent->isTemporary())
    , UIVMItem(machine)
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(0)
#ifdef Q_WS_MAC
    , m_iHighlightLightness(115)
    , m_iHoverLightness(110)
    , m_iHoverHighlightLightness(120)
#else /* Q_WS_MAC */
    , m_iHighlightLightness(130)
    , m_iHoverLightness(155)
    , m_iHoverHighlightLightness(175)
#endif /* !Q_WS_MAC */
{
//    /* Prepare: */
//    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);

    /* Translate finally: */
    retranslateUi();
}

UIGChooserItemMachine::UIGChooserItemMachine(UIGChooserItem *pParent,
                                             UIGChooserItemMachine *pCopyFrom,
                                             int iPosition /* = -1 */)
    : UIGChooserItem(pParent, pParent->isTemporary())
    , UIVMItem(pCopyFrom->machine())
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(0)
#ifdef Q_WS_MAC
    , m_iHighlightLightness(115)
    , m_iHoverLightness(110)
    , m_iHoverHighlightLightness(120)
#else /* Q_WS_MAC */
    , m_iHighlightLightness(130)
    , m_iHoverLightness(155)
    , m_iHoverHighlightLightness(175)
#endif /* !Q_WS_MAC */
{
//    /* Prepare: */
//    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);

    /* Translate finally: */
    retranslateUi();
}

UIGChooserItemMachine::~UIGChooserItemMachine()
{
    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus: */
        model()->setFocusItem(0);
    }
    /* If that item is selected: */
    if (model()->currentItems().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromCurrentItems(this);
    }
    /* Remove item from the navigation list: */
    model()->removeFromNavigationList(this);

    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->removeItem(this);
}

QString UIGChooserItemMachine::name() const
{
    return UIVMItem::name();
}

QString UIGChooserItemMachine::definition() const
{
    return QString("m=%1").arg(name());
}

bool UIGChooserItemMachine::isLockedMachine() const
{
    KMachineState state = machineState();
    return state != KMachineState_PoweredOff &&
           state != KMachineState_Saved &&
           state != KMachineState_Teleported &&
           state != KMachineState_Aborted;
}

void UIGChooserItemMachine::updateToolTip()
{
    setToolTip(toolTipText());
}

QVariant UIGChooserItemMachine::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case MachineItemData_Margin: return 5;
        case MachineItemData_MajorSpacing: return 10;
        case MachineItemData_MinorSpacing: return 4;
        case MachineItemData_TextSpacing: return 0;

        /* Pixmaps: */
        case MachineItemData_Pixmap: return osIcon();
        case MachineItemData_StatePixmap: return machineStateIcon();
        case MachineItemData_SettingsButtonPixmap: return UIIconPool::iconSet(":/settings_16px.png");
        case MachineItemData_StartButtonPixmap: return UIIconPool::iconSet(":/start_16px.png");
        case MachineItemData_PauseButtonPixmap: return UIIconPool::iconSet(":/pause_16px.png");
        case MachineItemData_CloseButtonPixmap: return UIIconPool::iconSet(":/exit_16px.png");

        /* Fonts: */
        case MachineItemData_NameFont:
        {
            QFont machineNameFont = qApp->font();
            machineNameFont.setWeight(QFont::Bold);
            return machineNameFont;
        }
        case MachineItemData_SnapshotNameFont:
        {
            QFont snapshotStateFont = qApp->font();
            return snapshotStateFont;
        }
        case MachineItemData_StateTextFont:
        {
            QFont machineStateFont = qApp->font();
            return machineStateFont;
        }

        /* Texts: */
        case MachineItemData_Name:
        {
            return compressText(data(MachineItemData_NameFont).value<QFont>(), model()->paintDevice(),
                                name(), data(MachineItemData_MaximumNameWidth).toInt());
        }
        case MachineItemData_SnapshotName:
        {
            QPaintDevice *pPaintDevice = model()->paintDevice();
            int iBracketWidth = QFontMetrics(data(MachineItemData_SnapshotNameFont).value<QFont>(),
                                             pPaintDevice).width("()");
            QString strCompressedName = compressText(data(MachineItemData_SnapshotNameFont).value<QFont>(),
                                                     pPaintDevice, snapshotName(),
                                                     data(MachineItemData_MaximumSnapshotNameWidth).toInt() - iBracketWidth);
            return QString("(%1)").arg(strCompressedName);
        }
        case MachineItemData_StateText: return machineStateName();

        /* Sizes: */
        case MachineItemData_PixmapSize: return osIcon().availableSizes().at(0);
        case MachineItemData_StatePixmapSize: return machineStateIcon().availableSizes().at(0);

        case MachineItemData_NameSize:
        {
            QFontMetrics fm(data(MachineItemData_NameFont).value<QFont>(), model()->paintDevice());
            return QSize(fm.width(data(MachineItemData_Name).toString()) + 2, fm.height());
        }
        case MachineItemData_MinimumNameWidth:
        {
            QFont font = data(MachineItemData_NameFont).value<QFont>();
            QPaintDevice *pPaintDevice = model()->paintDevice();
            return QFontMetrics(font, pPaintDevice).width(compressText(font, pPaintDevice,
                                                                       name(), textWidth(font, pPaintDevice, 15)));
        }
        case MachineItemData_MaximumNameWidth:
        {
            return data(MachineItemData_FirstRowMaximumWidth).toInt() -
                   data(MachineItemData_MinimumSnapshotNameWidth).toInt();
        }

        case MachineItemData_SnapshotNameSize:
        {
            QFontMetrics fm(data(MachineItemData_SnapshotNameFont).value<QFont>(), model()->paintDevice());
            return QSize(fm.width(data(MachineItemData_SnapshotName).toString()) + 2, fm.height());
        }
        case MachineItemData_MinimumSnapshotNameWidth:
        {
            if (snapshotName().isEmpty())
                return 0;
            QFontMetrics fm(data(MachineItemData_SnapshotNameFont).value<QFont>(), model()->paintDevice());
            int iBracketWidth = fm.width("()");
            int iActualTextWidth = fm.width(snapshotName());
            int iMinimumTextWidth = fm.width("...");
            return iBracketWidth + qMin(iActualTextWidth, iMinimumTextWidth);
        }
        case MachineItemData_MaximumSnapshotNameWidth:
        {
            return data(MachineItemData_FirstRowMaximumWidth).toInt() -
                   data(MachineItemData_NameSize).toSize().width();
        }

        case MachineItemData_FirstRowMaximumWidth:
        {
            /* Prepare variables: */
            int iMargin = data(MachineItemData_Margin).toInt();
            int iPixmapWidth = data(MachineItemData_PixmapSize).toSize().width();
            int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
            int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
            int iToolBarWidth = data(MachineItemData_ToolBarSize).toSize().width();
            int iMaximumWidth = (int)geometry().width() - 2 * iMargin -
                                                          iPixmapWidth -
                                                          iMachineItemMajorSpacing;
            if (!snapshotName().isEmpty())
                iMaximumWidth -= iMachineItemMinorSpacing;
            if (m_pToolBar)
                iMaximumWidth -= (iToolBarWidth + iMachineItemMajorSpacing);
            return iMaximumWidth;
        }
        case MachineItemData_StateTextSize:
        {
            QFontMetrics fm(data(MachineItemData_StateTextFont).value<QFont>(), model()->paintDevice());
            return QSize(fm.width(data(MachineItemData_StateText).toString()) + 2, fm.height());
        }
        case MachineItemData_ToolBarSize:
        {
            return m_pToolBar ? m_pToolBar->minimumSizeHint().toSize() : QSize(0, 0);
        }
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGChooserItemMachine::retranslateUi()
{
    updateToolTip();
}

void UIGChooserItemMachine::startEditing()
{
    AssertMsgFailed(("Machine graphics item do NOT support editing yet!"));
}

void UIGChooserItemMachine::addItem(UIGChooserItem*, int)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGChooserItemMachine::removeItem(UIGChooserItem*)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGChooserItemMachine::setItems(const QList<UIGChooserItem*>&, UIGChooserItemType)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

QList<UIGChooserItem*> UIGChooserItemMachine::items(UIGChooserItemType) const
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
    return QList<UIGChooserItem*>();
}

bool UIGChooserItemMachine::hasItems(UIGChooserItemType) const
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
    return false;
}

void UIGChooserItemMachine::clearItems(UIGChooserItemType)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGChooserItemMachine::updateSizeHint()
{
    updateGeometry();
}

void UIGChooserItemMachine::updateLayout()
{
    if (m_pToolBar)
    {
        /* Prepare variables: */
        QSize size = geometry().size().toSize();

        /* Prepare variables: */
        int iMachineItemWidth = size.width();
        int iMachineItemHeight = size.height();
        int iToolBarHeight = data(MachineItemData_ToolBarSize).toSize().height();

        /* Configure tool-bar: */
        QSize toolBarSize = m_pToolBar->minimumSizeHint().toSize();
        int iToolBarX = iMachineItemWidth - 1 - toolBarSize.width();
        int iToolBarY = (iMachineItemHeight - iToolBarHeight) / 2;
        m_pToolBar->setPos(iToolBarX, iToolBarY);
        m_pToolBar->resize(toolBarSize);
        m_pToolBar->updateLayout();

        /* Configure buttons: */
        m_pStartButton->updateAnimation();
        m_pSettingsButton->updateAnimation();
        m_pCloseButton->updateAnimation();
        m_pPauseButton->updateAnimation();
    }
}

int UIGChooserItemMachine::minimumWidthHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    int iMachinePixmapWidth = data(MachineItemData_PixmapSize).toSize().width();
    int iMinimumNameWidth = data(MachineItemData_MinimumNameWidth).toInt();
    int iMinimumSnapshotNameWidth = data(MachineItemData_MinimumSnapshotNameWidth).toInt();
    int iMachineStatePixmapWidth = data(MachineItemData_StatePixmapSize).toSize().width();
    int iMachineStateTextWidth = data(MachineItemData_StateTextSize).toSize().width();
    int iToolBarWidth = data(MachineItemData_ToolBarSize).toSize().width();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Two margins: */
    iProposedWidth += 2 * iMachineItemMargin;
    /* And machine item content to take into account: */
    int iTopLineWidth = iMinimumNameWidth +
                        iMachineItemMinorSpacing +
                        iMinimumSnapshotNameWidth;
    int iBottomLineWidth = iMachineStatePixmapWidth +
                           iMachineItemMinorSpacing +
                           iMachineStateTextWidth;
    int iRightColumnWidth = qMax(iTopLineWidth, iBottomLineWidth);
    int iMachineItemWidth = iMachinePixmapWidth +
                            iMachineItemMajorSpacing +
                            iRightColumnWidth;
    if (m_pToolBar)
        iMachineItemWidth += (iMachineItemMajorSpacing + iToolBarWidth);
    iProposedWidth += iMachineItemWidth;

    /* Return result: */
    return iProposedWidth;
}

int UIGChooserItemMachine::minimumHeightHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    int iMachinePixmapHeight = data(MachineItemData_PixmapSize).toSize().height();
    int iMachineNameHeight = data(MachineItemData_NameSize).toSize().height();
    int iSnapshotNameHeight = data(MachineItemData_SnapshotNameSize).toSize().height();
    int iMachineStatePixmapHeight = data(MachineItemData_StatePixmapSize).toSize().height();
    int iMachineStateTextHeight = data(MachineItemData_StateTextSize).toSize().height();
    int iToolBarHeight = data(MachineItemData_ToolBarSize).toSize().height();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMachineItemMargin;
    /* And machine item content to take into account: */
    int iTopLineHeight = qMax(iMachineNameHeight, iSnapshotNameHeight);
    int iBottomLineHeight = qMax(iMachineStatePixmapHeight, iMachineStateTextHeight);
    int iRightColumnHeight = iTopLineHeight +
                              iMachineItemTextSpacing +
                              iBottomLineHeight;
    QList<int> heights;
    heights << iMachinePixmapHeight << iRightColumnHeight << iToolBarHeight;
    int iMaxHeight = 0;
    foreach (int iHeight, heights)
        iMaxHeight = qMax(iMaxHeight, iHeight);
    iProposedHeight += iMaxHeight;

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIGChooserItemMachine::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
    {
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    }

    /* Call to base-class: */
    return UIGChooserItem::sizeHint(which, constraint);
}

QPixmap UIGChooserItemMachine::toPixmap()
{
    /* Ask item to paint itself into pixmap: */
    QSize minimumSize = minimumSizeHint().toSize();
    QPixmap pixmap(minimumSize);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSize);
    paint(&painter, &options);
    return pixmap;
}

bool UIGChooserItemMachine::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const
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
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should make sure machine is accessible: */
    if (!accessible())
        return false;
    /* Else we should try to cast mime to known classes: */
    if (pMimeData->hasFormat(UIGChooserItemMachine::className()))
    {
        /* Make sure passed item id is not ours: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
        UIGChooserItemMachine *pMachineItem = pItem->toMachineItem();
        /* Make sure passed machine is mutable: */
        if (pMachineItem->isLockedMachine())
            return false;
        return pMachineItem->id() != id();
    }
    /* That was invalid mime: */
    return false;
}

void UIGChooserItemMachine::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho, DragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Make sure this handler called by this item (not by children): */
    AssertMsg(!pFromWho && where == DragToken_Off, ("Machine graphics item do NOT support children!"));
    Q_UNUSED(pFromWho);
    Q_UNUSED(where);
    if (pMime->hasFormat(UIGChooserItemMachine::className()))
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

                /* Group passed item with current item into the new group: */
                UIGChooserItemGroup *pNewGroupItem = new UIGChooserItemGroup(parentItem(),
                                                                             model()->uniqueGroupName(parentItem()),
                                                                             true);
                new UIGChooserItemMachine(pNewGroupItem, this);
                new UIGChooserItemMachine(pNewGroupItem, pItem->toMachineItem());

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }
                /* Delete this item: */
                delete this;

                /* Update model: */
                pModel->updateGroupTree();
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
}

void UIGChooserItemMachine::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != DragToken_Off)
    {
        setDragTokenPlace(DragToken_Off);
        update();
    }
}

QMimeData* UIGChooserItemMachine::createMimeData()
{
    return new UIGChooserItemMimeData(this);
}

void UIGChooserItemMachine::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    UIGChooserItem::mousePressEvent(pEvent);
    /* No drag for inaccessible: */
    if (!accessible())
        pEvent->ignore();
}

void UIGChooserItemMachine::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget * /* pWidget = 0 */)
{
    /* Setup: */
    pPainter->setRenderHint(QPainter::Antialiasing);

    /* Configure painter shape: */
    configurePainterShape(pPainter, pOption, m_iCornerRadius);

    /* Paint decorations: */
    paintDecorations(pPainter, pOption);

    /* Paint machine info: */
    paintMachineInfo(pPainter, pOption);
}

void UIGChooserItemMachine::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Prepare variables: */
    QRect fullRect = pOption->rect;

    /* Paint background: */
    paintBackground(pPainter, fullRect);

    /* Paint frame: */
    paintFrameRectangle(pPainter, fullRect);
}

void UIGChooserItemMachine::paintBackground(QPainter *pPainter, const QRect &rect)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    QPalette pal = palette();

    /* Selection background: */
    if (model()->currentItems().contains(this))
    {
        /* Prepare color: */
        QColor highlight = pal.color(QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rect.topLeft(), rect.bottomLeft());
        bgGrad.setColorAt(0, highlight.lighter(m_iHighlightLightness));
        bgGrad.setColorAt(1, highlight);
        pPainter->fillRect(rect, bgGrad);
    }

    /* Hovering background: */
    else if (isHovered())
    {
        /* Prepare color: */
        QColor highlight = pal.color(QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rect.topLeft(), rect.bottomLeft());
        bgGrad.setColorAt(0, highlight.lighter(m_iHoverHighlightLightness));
        bgGrad.setColorAt(1, highlight.lighter(m_iHoverLightness));
        pPainter->fillRect(rect, bgGrad);
    }

    /* Paint drag token UP? */
    if (dragTokenPlace() != DragToken_Off)
    {
        /* Window color: */
        QColor base = pal.color(QPalette::Active, model()->currentItems().contains(this) ?
                                QPalette::Highlight : QPalette::Window);
        QLinearGradient dragTokenGradient;
        QRect dragTokenRect = rect;
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
        dragTokenGradient.setColorAt(0, base.darker(dragTokenDarkness()));
        dragTokenGradient.setColorAt(1, base.darker(dragTokenDarkness() + 40));
        pPainter->fillRect(dragTokenRect, dragTokenGradient);
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIGChooserItemMachine::paintFrameRectangle(QPainter *pPainter, const QRect &rect)
{
    /* Only chosen and/or hovered item should have a frame: */
    if (!model()->currentItems().contains(this) && !isHovered())
        return;

    /* Simple white frame: */
    pPainter->save();
    QPalette pal = palette();
    QColor hc = pal.color(QPalette::Active, QPalette::Highlight);
    if (model()->currentItems().contains(this))
        hc = hc.darker(strokeDarkness());
    pPainter->setPen(hc);
    pPainter->drawRect(rect);
    pPainter->restore();
}

void UIGChooserItemMachine::paintMachineInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Prepare variables: */
    QRect fullRect = pOption->rect;
    int iFullHeight = fullRect.height();
    int iMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    QSize machinePixmapSize = data(MachineItemData_PixmapSize).toSize();
    QSize machineNameSize = data(MachineItemData_NameSize).toSize();
    QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
    QSize machineStatePixmapSize = data(MachineItemData_StatePixmapSize).toSize();
    QSize machineStateTextSize = data(MachineItemData_StateTextSize).toSize();

    /* Update palette: */
    if (model()->currentItems().contains(this))
    {
        QPalette pal = palette();
        pPainter->setPen(pal.color(QPalette::HighlightedText));
    }

    /* Calculate indents: */
    int iLeftColumnIndent = iMargin;

    /* Paint left column: */
    {
        /* Prepare variables: */
        int iMachinePixmapX = iLeftColumnIndent;
        int iMachinePixmapY = (iFullHeight - machinePixmapSize.height()) / 2;
        /* Paint pixmap: */
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    QRect(QPoint(iMachinePixmapX, iMachinePixmapY), machinePixmapSize),
                    /* Pixmap to paint: */
                    data(MachineItemData_Pixmap).value<QIcon>().pixmap(machinePixmapSize));
    }

    /* Calculate indents: */
    int iRightColumnIndent = iLeftColumnIndent +
                             machinePixmapSize.width() +
                             iMachineItemMajorSpacing;

    /* Paint right column: */
    {
        /* Calculate indents: */
        int iTopLineHeight = qMax(machineNameSize.height(), snapshotNameSize.height());
        int iBottomLineHeight = qMax(machineStatePixmapSize.height(), machineStateTextSize.height());
        int iRightColumnHeight = iTopLineHeight + iMachineItemTextSpacing + iBottomLineHeight;
        int iTopLineIndent = (iFullHeight - iRightColumnHeight) / 2;

        /* Paint top line: */
        {
            /* Paint left element: */
            {
                /* Prepare variables: */
                int iNameX = iRightColumnIndent;
                int iNameY = iTopLineIndent;
                /* Paint name: */
                paintText(/* Painter: */
                          pPainter,
                          /* Point to paint in: */
                          QPoint(iNameX, iNameY),
                          /* Font to paint text: */
                          data(MachineItemData_NameFont).value<QFont>(),
                          /* Paint device: */
                          model()->paintDevice(),
                          /* Text to paint: */
                          data(MachineItemData_Name).toString());
            }

            /* Calculate indents: */
            int iSnapshotNameIndent = iRightColumnIndent +
                                      machineNameSize.width() +
                                      iMachineItemMinorSpacing;

            /* Paint right element: */
            if (!snapshotName().isEmpty())
            {
                /* Prepare variables: */
                int iSnapshotNameX = iSnapshotNameIndent;
                int iSnapshotNameY = iTopLineIndent;
                /* Paint snapshot name: */
                paintText(/* Painter: */
                          pPainter,
                          /* Point to paint in: */
                          QPoint(iSnapshotNameX, iSnapshotNameY),
                          /* Font to paint text: */
                          data(MachineItemData_SnapshotNameFont).value<QFont>(),
                          /* Paint device: */
                          model()->paintDevice(),
                          /* Text to paint: */
                          data(MachineItemData_SnapshotName).toString());
            }
        }

        /* Calculate indents: */
        int iBottomLineIndent = iTopLineIndent + iTopLineHeight;

        /* Paint bottom line: */
        {
            /* Paint left element: */
            {
                /* Prepare variables: */
                int iMachineStatePixmapX = iRightColumnIndent;
                int iMachineStatePixmapY = iBottomLineIndent;
                /* Paint state pixmap: */
                paintPixmap(/* Painter: */
                            pPainter,
                            /* Rectangle to paint in: */
                            QRect(QPoint(iMachineStatePixmapX, iMachineStatePixmapY), machineStatePixmapSize),
                            /* Pixmap to paint: */
                            data(MachineItemData_StatePixmap).value<QIcon>().pixmap(machineStatePixmapSize));
            }

            /* Calculate indents: */
            int iMachineStateTextIndent = iRightColumnIndent +
                                          machineStatePixmapSize.width() +
                                          iMachineItemMinorSpacing;

            /* Paint right element: */
            {
                /* Prepare variables: */
                int iMachineStateTextX = iMachineStateTextIndent;
                int iMachineStateTextY = iBottomLineIndent;
                /* Paint state text: */
                paintText(/* Painter: */
                          pPainter,
                          /* Point to paint in: */
                          QPoint(iMachineStateTextX, iMachineStateTextY),
                          /* Font to paint text: */
                          data(MachineItemData_StateTextFont).value<QFont>(),
                          /* Paint device: */
                          model()->paintDevice(),
                          /* Text to paint: */
                          data(MachineItemData_StateText).toString());
            }
        }
    }

    /* Tool-bar: */
    if (m_pToolBar)
    {
        /* Show/hide tool-bar: */
        if (isHovered())
        {
            if (!m_pToolBar->isVisible())
                m_pToolBar->show();
        }
        else
        {
            if (m_pToolBar->isVisible())
                m_pToolBar->hide();
        }
    }
}

void UIGChooserItemMachine::prepare()
{
    /* Create tool-bar: */
    m_pToolBar = new UIGraphicsToolBar(this, 2, 2);

    /* Create buttons: */
    m_pSettingsButton = new UIGraphicsZoomButton(m_pToolBar,
                                                 data(MachineItemData_SettingsButtonPixmap).value<QIcon>(),
                                                 UIGraphicsZoomDirection_Top | UIGraphicsZoomDirection_Left);
    m_pSettingsButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pToolBar->insertItem(m_pSettingsButton, 0, 0);

    m_pStartButton = new UIGraphicsZoomButton(m_pToolBar,
                                              data(MachineItemData_StartButtonPixmap).value<QIcon>(),
                                              UIGraphicsZoomDirection_Top | UIGraphicsZoomDirection_Right);
    m_pStartButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pToolBar->insertItem(m_pStartButton, 0, 1);

    m_pPauseButton = new UIGraphicsZoomButton(m_pToolBar,
                                              data(MachineItemData_PauseButtonPixmap).value<QIcon>(),
                                              UIGraphicsZoomDirection_Bottom | UIGraphicsZoomDirection_Left);
    m_pPauseButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pToolBar->insertItem(m_pPauseButton, 1, 0);

    m_pCloseButton = new UIGraphicsZoomButton(m_pToolBar,
                                              data(MachineItemData_CloseButtonPixmap).value<QIcon>(),
                                              UIGraphicsZoomDirection_Bottom | UIGraphicsZoomDirection_Right);
    m_pCloseButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pToolBar->insertItem(m_pCloseButton, 1, 1);

    connect(m_pSettingsButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Simple_Machine_Settings), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pStartButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_State_Common_StartOrShow), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pPauseButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Toggle_Common_PauseAndResume), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pCloseButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff), SLOT(trigger()),
            Qt::QueuedConnection);
}

