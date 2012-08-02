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
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIActionPoolSelector.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* static */
QString UIGChooserItemMachine::className() { return "UIGChooserItemMachine"; }

UIGChooserItemMachine::UIGChooserItemMachine(UIGChooserItem *pParent,
                                             const CMachine &machine,
                                             int iPosition /* = -1 */)
    : UIGChooserItem(pParent)
    , UIVMItem(machine)
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(6)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);
}

UIGChooserItemMachine::UIGChooserItemMachine(UIGChooserItem *pParent,
                                             UIGChooserItemMachine *pCopyFrom,
                                             int iPosition /* = -1 */)
    : UIGChooserItem(pParent)
    , UIVMItem(pCopyFrom->machine())
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(6)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);
}

UIGChooserItemMachine::~UIGChooserItemMachine()
{
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
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->removeItem(this);
}

QString UIGChooserItemMachine::name() const
{
    return UIVMItem::name();
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
        case MachineItemData_TextSpacing: return 2;
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
            machineNameFont.setPointSize(machineNameFont.pointSize() + 1);
            machineNameFont.setWeight(QFont::Bold);
            return machineNameFont;
        }
        case MachineItemData_SnapshotNameFont:
        {
            QFont snapshotStateFont = qApp->font();
            snapshotStateFont.setPointSize(snapshotStateFont.pointSize() + 1);
            return snapshotStateFont;
        }
        case MachineItemData_StateTextFont:
        {
            QFont machineStateFont = qApp->font();
            machineStateFont.setPointSize(machineStateFont.pointSize() + 1);
            return machineStateFont;
        }
        /* Texts: */
        case MachineItemData_Name:
        {
            /* Prepare variables: */
            int iMaximumWidth = data(MachineItemData_FirstRowMaximumWidth).toInt();
            int iMinimumSnapshotNameWidth = data(MachineItemData_MinimumSnapshotNameSize).toSize().width();
            /* Compress name to part width: */
            QString strCompressedName = compressText(data(MachineItemData_NameFont).value<QFont>(),
                                                     name(), iMaximumWidth - iMinimumSnapshotNameWidth);
            return strCompressedName;
        }
        case MachineItemData_SnapshotName:
        {
            /* Prepare variables: */
            int iMaximumWidth = data(MachineItemData_FirstRowMaximumWidth).toInt();
            int iNameWidth = data(MachineItemData_NameSize).toSize().width();
            /* Compress name to part width: */
            QString strCompressedName = compressText(data(MachineItemData_SnapshotNameFont).value<QFont>(),
                                                     snapshotName(), iMaximumWidth - iNameWidth);
            return strCompressedName;
        }
        case MachineItemData_StateText: return gpConverter->toString(machineState());
        /* Sizes: */
        case MachineItemData_PixmapSize: return osIcon().availableSizes().at(0);
        case MachineItemData_StatePixmapSize: return machineStateIcon().availableSizes().at(0);
        case MachineItemData_MinimumNameSize:
        {
            QFont font = data(MachineItemData_NameFont).value<QFont>();
            QFontMetrics fm(font);
            int iMaximumTextWidth = textWidth(font, 15);
            QString strCompressedName = compressText(font, name(), iMaximumTextWidth);
            return QSize(fm.width(strCompressedName), fm.height());
        }
        case MachineItemData_NameSize:
        {
            QFontMetrics fm(data(MachineItemData_NameFont).value<QFont>());
            return QSize(fm.width(data(MachineItemData_Name).toString()), fm.height());
        }
        case MachineItemData_MinimumSnapshotNameSize:
        {
            QFont font = data(MachineItemData_SnapshotNameFont).value<QFont>();
            QFontMetrics fm(font);
            int iMaximumTextWidth = textWidth(font, 10);
            QString strCompressedName = compressText(font, snapshotName(), iMaximumTextWidth);
            return QSize(fm.width(strCompressedName), fm.height());
        }
        case MachineItemData_SnapshotNameSize:
        {
            QFontMetrics fm(data(MachineItemData_SnapshotNameFont).value<QFont>());
            return QSize(fm.width(QString("(%1)").arg(data(MachineItemData_SnapshotName).toString())), fm.height());
        }
        case MachineItemData_FirstRowMaximumWidth:
        {
            /* Prepare variables: */
            int iMargin = data(MachineItemData_Margin).toInt();
            int iPixmapWidth = data(MachineItemData_PixmapSize).toSize().width();
            int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
            int iToolBarWidth = data(MachineItemData_ToolBarSize).toSize().width();
            int iMaximumWidth = (int)geometry().width() - iMargin -
                                                          iPixmapWidth - iMachineItemMajorSpacing -
                                                          iToolBarWidth - iMachineItemMajorSpacing;
            return iMaximumWidth;
        }
        case MachineItemData_StateTextSize:
        {
            QFontMetrics fm(data(MachineItemData_StateTextFont).value<QFont>());
            return QSize(fm.width(data(MachineItemData_StateText).toString()), fm.height());
        }
        case MachineItemData_ToolBarSize: return m_pToolBar->minimumSizeHint().toSize();
        /* Default: */
        default: break;
    }
    return QVariant();
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

int UIGChooserItemMachine::minimumWidthHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    int iMachinePixmapWidth = data(MachineItemData_PixmapSize).toSize().width();
    int iMinimumMachineNameWidth = data(MachineItemData_MinimumNameSize).toSize().width();
    int iMinimumSnapshotNameWidth = data(MachineItemData_MinimumSnapshotNameSize).toSize().width();
    int iMachineStatePixmapWidth = data(MachineItemData_StatePixmapSize).toSize().width();
    int iMachineStateTextWidth = data(MachineItemData_StateTextSize).toSize().width();
    int iToolBarWidth = data(MachineItemData_ToolBarSize).toSize().width();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* We are taking into account only left margin,
     * tool-bar contains right one: */
    iProposedWidth += iMachineItemMargin;
    /* And machine item content to take into account: */
    int iFirstLineWidth = iMinimumMachineNameWidth +
                          iMachineItemMinorSpacing +
                          iMinimumSnapshotNameWidth;
    int iSecondLineWidth = iMachineStatePixmapWidth +
                           iMachineItemMinorSpacing +
                           iMachineStateTextWidth;
    int iSecondColumnWidth = qMax(iFirstLineWidth, iSecondLineWidth);
    int iMachineItemWidth = iMachinePixmapWidth +
                            iMachineItemMajorSpacing +
                            iSecondColumnWidth +
                            iMachineItemMajorSpacing +
                            iToolBarWidth + 1;
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

    /* Simple machine item have 2 margins - top and bottom: */
    iProposedHeight += 2 * iMachineItemMargin;
    /* And machine item content to take into account: */
    int iFirstLineHeight = qMax(iMachineNameHeight, iSnapshotNameHeight);
    int iSecondLineHeight = qMax(iMachineStatePixmapHeight, iMachineStateTextHeight);
    int iSecondColumnHeight = iFirstLineHeight +
                              iMachineItemTextSpacing +
                              iSecondLineHeight;
    QList<int> heights;
    heights << iMachinePixmapHeight << iSecondColumnHeight
            << (iToolBarHeight - 2 * iMachineItemMargin + 2);
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
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should try to cast mime to known classes: */
    if (pMimeData->hasFormat(UIGChooserItemMachine::className()))
    {
        /* Make sure passed item id is not ours: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
        return pItem->toMachineItem()->id() != id();
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
                                                                             model()->uniqueGroupName(parentItem()));
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

                /* Update scene: */
                pModel->updateGroupTree();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewGroupItem);
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

void UIGChooserItemMachine::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget * /* pWidget = 0 */)
{
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
    paintFrameRect(pPainter, fullRect, model()->selectionList().contains(this), m_iCornerRadius);
}

void UIGChooserItemMachine::paintBackground(QPainter *pPainter, const QRect &rect)
{
    /* Save painter: */
    pPainter->save();

    /* Fill rectangle with white color: */
    pPainter->fillRect(rect, Qt::white);

    /* Prepare color: */
    QPalette pal = palette();
    QColor base = pal.color(QPalette::Active, model()->selectionList().contains(this) ?
                            QPalette::Highlight : QPalette::Window);

    /* Make even less rectangle: */
    QRect backGroundRect = rect;
    backGroundRect.setTopLeft(backGroundRect.topLeft() + QPoint(2, 2));
    backGroundRect.setBottomRight(backGroundRect.bottomRight() - QPoint(2, 2));
    /* Add even more clipping: */
    QPainterPath roundedPath;
    roundedPath.addRoundedRect(backGroundRect, m_iCornerRadius, m_iCornerRadius);
    pPainter->setClipPath(roundedPath);

    /* Calculate top rectangle: */
    QRect tRect = backGroundRect;
    tRect.setBottom(tRect.top() + tRect.height() / 3);
    /* Calculate bottom rectangle: */
    QRect bRect = backGroundRect;
    bRect.setTop(bRect.bottom() - bRect.height() / 3);
    /* Calculate middle rectangle: */
    QRect midRect = QRect(tRect.bottomLeft(), bRect.topRight());

    /* Prepare top gradient: */
    QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
    tGradient.setColorAt(0, base.darker(gradient()));
    tGradient.setColorAt(1, base.darker(104));
    /* Prepare bottom gradient: */
    QLinearGradient bGradient(bRect.topLeft(), bRect.bottomLeft());
    bGradient.setColorAt(0, base.darker(gradient()));
    bGradient.setColorAt(1, base.darker(104));

    /* Paint all the stuff: */
    pPainter->fillRect(midRect, base.darker(gradient()));
    pPainter->fillRect(tRect, tGradient);
    pPainter->fillRect(bRect, bGradient);

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

    /* Restore painter: */
    pPainter->restore();
}

void UIGChooserItemMachine::paintMachineInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Initialize some necessary variables: */
    QRect fullRect = pOption->rect;
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    QSize machinePixmapSize = data(MachineItemData_PixmapSize).toSize();
    QSize machineNameSize = data(MachineItemData_NameSize).toSize();
    QString strSnapshotName = data(MachineItemData_SnapshotName).toString();
    QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
    QSize machineStatePixmapSize = data(MachineItemData_StatePixmapSize).toSize();
    QSize machineStateTextSize = data(MachineItemData_StateTextSize).toSize();

    /* Paint pixmap: */
    {
        /* Calculate attributes: */
        int iMachinePixmapHeight = machinePixmapSize.height();
        int iFirstLineHeight = qMax(machineNameSize.height(), snapshotNameSize.height());
        int iSecondLineHeight = qMax(machineStatePixmapSize.height(), machineStateTextSize.height());
        int iRightSizeHeight = iFirstLineHeight + iMachineItemTextSpacing + iSecondLineHeight;
        int iMinimumHeight = qMin(iMachinePixmapHeight, iRightSizeHeight);
        int iMaximumHeight = qMax(iMachinePixmapHeight, iRightSizeHeight);
        int iDelta = iMaximumHeight - iMinimumHeight;
        int iHalfDelta = iDelta / 2;
        int iMachinePixmapX = iMachineItemMargin;
        int iMachinePixmapY = iMachinePixmapHeight >= iRightSizeHeight ?
                              iMachineItemMargin : iMachineItemMargin + iHalfDelta;

        paintPixmap(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    QRect(fullRect.topLeft() +
                          QPoint(iMachinePixmapX, iMachinePixmapY),
                          machinePixmapSize),
                    /* Pixmap to paint: */
                    data(MachineItemData_Pixmap).value<QIcon>().pixmap(machinePixmapSize));
    }

    /* Paint name: */
    {
        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(fullRect.topLeft() +
                        QPoint(iMachineItemMargin, iMachineItemMargin) +
                        QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0),
                        machineNameSize),
                  /* Font to paint text: */
                  data(MachineItemData_NameFont).value<QFont>(),
                  /* Text to paint: */
                  data(MachineItemData_Name).toString());
    }

    /* Paint snapshot name (if necessary): */
    if (!strSnapshotName.isEmpty())
    {
        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(fullRect.topLeft() +
                        QPoint(iMachineItemMargin, iMachineItemMargin) +
                        QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0) +
                        QPoint(machineNameSize.width() + iMachineItemMinorSpacing, 0),
                        snapshotNameSize),
                  /* Font to paint text: */
                  data(MachineItemData_SnapshotNameFont).value<QFont>(),
                  /* Text to paint: */
                  QString("(%1)").arg(strSnapshotName));
    }

    /* Paint state pixmap: */
    {
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    QRect(fullRect.topLeft() +
                          QPoint(iMachineItemMargin, iMachineItemMargin) +
                          QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0) +
                          QPoint(0, machineNameSize.height() + iMachineItemTextSpacing),
                          machineStatePixmapSize),
                    /* Pixmap to paint: */
                    data(MachineItemData_StatePixmap).value<QIcon>().pixmap(machineStatePixmapSize));
    }

    /* Paint state text: */
    {
        paintText(/* Painter: */
                  pPainter,
                  /* Rectangle to paint in: */
                  QRect(fullRect.topLeft() +
                        QPoint(iMachineItemMargin, iMachineItemMargin) +
                        QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0) +
                        QPoint(0, machineNameSize.height() + iMachineItemTextSpacing) +
                        QPoint(machineStatePixmapSize.width() + iMachineItemMinorSpacing, 0),
                        machineStateTextSize),
                  /* Font to paint text: */
                  data(MachineItemData_StateTextFont).value<QFont>(),
                  /* Text to paint: */
                  data(MachineItemData_StateText).toString());
    }

    /* Show/hide start-button: */
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

void UIGChooserItemMachine::prepare()
{
    /* Create tool-bar: */
    m_pToolBar = new UIGraphicsToolBar(this, 2, 2);

    /* Create buttons: */
    m_pSettingsButton = new UIGraphicsZoomButton(m_pToolBar, UIGraphicsZoomDirection_Top | UIGraphicsZoomDirection_Left);
    m_pSettingsButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pSettingsButton->setIcon(data(MachineItemData_SettingsButtonPixmap).value<QIcon>());
    m_pToolBar->insertItem(m_pSettingsButton, 0, 0);

    m_pStartButton = new UIGraphicsZoomButton(m_pToolBar, UIGraphicsZoomDirection_Top | UIGraphicsZoomDirection_Right);
    m_pStartButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pStartButton->setIcon(data(MachineItemData_StartButtonPixmap).value<QIcon>());
    m_pToolBar->insertItem(m_pStartButton, 0, 1);

    m_pPauseButton = new UIGraphicsZoomButton(m_pToolBar, UIGraphicsZoomDirection_Bottom | UIGraphicsZoomDirection_Left);
    m_pPauseButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pPauseButton->setIcon(data(MachineItemData_PauseButtonPixmap).value<QIcon>());
    m_pToolBar->insertItem(m_pPauseButton, 1, 0);

    m_pCloseButton = new UIGraphicsZoomButton(m_pToolBar, UIGraphicsZoomDirection_Bottom | UIGraphicsZoomDirection_Right);
    m_pCloseButton->setIndent(m_pToolBar->toolBarMargin() - 1);
    m_pCloseButton->setIcon(data(MachineItemData_CloseButtonPixmap).value<QIcon>());
    m_pToolBar->insertItem(m_pCloseButton, 1, 1);

    connect(m_pSettingsButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pStartButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pPauseButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume), SLOT(trigger()),
            Qt::QueuedConnection);
    connect(m_pCloseButton, SIGNAL(sigButtonClicked()),
            gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff), SLOT(trigger()),
            Qt::QueuedConnection);
}

