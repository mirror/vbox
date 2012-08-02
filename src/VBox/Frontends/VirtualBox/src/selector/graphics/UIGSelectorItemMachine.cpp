/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItemMachine class implementation
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
#include "UIGSelectorItemMachine.h"
#include "UIGSelectorItemGroup.h"
#include "UIGraphicsSelectorModel.h"
#include "VBoxGlobal.h"
#include "UIConverter.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* static */
QString UIGSelectorItemMachine::className() { return "UIGSelectorItemMachine"; }

UIGSelectorItemMachine::UIGSelectorItemMachine(UIGSelectorItem *pParent,
                                               const CMachine &machine,
                                               int iPosition /* = -1 */)
    : UIGSelectorItem(pParent, iPosition)
    , UIVMItem(machine)
{
    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
}

UIGSelectorItemMachine::UIGSelectorItemMachine(UIGSelectorItem *pParent,
                                               UIGSelectorItemMachine *pCopyFrom,
                                               int iPosition /* = -1 */)
    : UIGSelectorItem(pParent, iPosition)
    , UIVMItem(pCopyFrom->machine())
{
    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);
}

UIGSelectorItemMachine::~UIGSelectorItemMachine()
{
    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->removeItem(this);
}

QString UIGSelectorItemMachine::name() const
{
    return UIVMItem::name();
}

QVariant UIGSelectorItemMachine::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case MachineItemData_MachineItemMargin: return 4;
        case MachineItemData_MachineItemMajorSpacing: return 10;
        case MachineItemData_MachineItemMinorSpacing: return 4;
        case MachineItemData_MachineItemTextSpacing: return 2;
        /* Fonts: */
        case MachineItemData_MachineNameFont:
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
        case MachineItemData_MachineStateTextFont:
        {
            QFont machineStateFont = qApp->font();
            machineStateFont.setPointSize(machineStateFont.pointSize() + 1);
            return machineStateFont;
        }
        /* Sizes: */
        case MachineItemData_MachinePixmapSize: return QSizeF(32, 32);
        case MachineItemData_MachineNameSize:
        {
            QFontMetrics fm(data(MachineItemData_MachineNameFont).value<QFont>());
            return QSize(fm.width(data(MachineItemData_MachineName).toString()), fm.height());
        }
        case MachineItemData_SnapshotNameSize:
        {
            QFontMetrics fm(data(MachineItemData_SnapshotNameFont).value<QFont>());
            return QSize(fm.width(QString("(%1)").arg(data(MachineItemData_SnapshotName).toString())), fm.height());
        }
        case MachineItemData_MachineStatePixmapSize: return QSizeF(16, 16);
        case MachineItemData_MachineStateTextSize:
        {
            QFontMetrics fm(data(MachineItemData_MachineStateTextFont).value<QFont>());
            return QSize(fm.width(data(MachineItemData_MachineStateText).toString()), fm.height());
        }
        /* Pixmaps: */
        case MachineItemData_MachinePixmap: return osIcon();
        case MachineItemData_MachineStatePixmap: return machineStateIcon();
        /* Texts: */
        case MachineItemData_MachineName: return name();
        case MachineItemData_SnapshotName: return snapshotName();
        case MachineItemData_MachineStateText: return gpConverter->toString(data(MachineItemData_MachineState).value<KMachineState>());
        /* Other values: */
        case MachineItemData_MachineId: return id();
        case MachineItemData_MachineOSTypeId: return osTypeId();
        case MachineItemData_MachineState: return QVariant::fromValue(machineState());
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGSelectorItemMachine::startEditing()
{
    AssertMsgFailed(("Machine graphics item do NOT support editing yet!"));
}

void UIGSelectorItemMachine::cleanup()
{
    AssertMsgFailed(("There is nothing to cleanup in machine graphics item!"));
}

void UIGSelectorItemMachine::addItem(UIGSelectorItem*, int)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGSelectorItemMachine::removeItem(UIGSelectorItem*)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

QList<UIGSelectorItem*> UIGSelectorItemMachine::items(UIGSelectorItemType) const
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
    return QList<UIGSelectorItem*>();
}

bool UIGSelectorItemMachine::hasItems(UIGSelectorItemType) const
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
    return false;
}

void UIGSelectorItemMachine::clearItems(UIGSelectorItemType)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGSelectorItemMachine::updateSizeHint()
{
    updateGeometry();
}

void UIGSelectorItemMachine::updateLayout()
{
    AssertMsgFailed(("There is nothing to layout in machine graphics item!"));
}

QSizeF UIGSelectorItemMachine::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* This calculation is suitable only for Qt::MaximumSize and Qt::MinimumSize hints: */
    if (which == Qt::MaximumSize || which == Qt::MinimumSize)
    {
        /* First of all, we have to prepare few variables: */
        int iMachineItemMargin = data(MachineItemData_MachineItemMargin).toInt();
        int iMachineItemMajorSpacing = data(MachineItemData_MachineItemMajorSpacing).toInt();
        int iMachineItemMinorSpacing = data(MachineItemData_MachineItemMinorSpacing).toInt();
        int iMachineItemTextSpacing = data(MachineItemData_MachineItemTextSpacing).toInt();
        QSize machinePixmapSize = data(MachineItemData_MachinePixmapSize).toSize();
        QSize machineNameSize = data(MachineItemData_MachineNameSize).toSize();
        QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
        QSize machineStatePixmapSize = data(MachineItemData_MachineStatePixmapSize).toSize();
        QSize machineStateTextSize = data(MachineItemData_MachineStateTextSize).toSize();

        /* Calculating proposed height: */
        int iProposedHeight = 0;
        {
            /* Simple machine item have 2 margins - top and bottom: */
            iProposedHeight += 2 * iMachineItemMargin;
            /* And machine item content to take into account: */
            int iFirstLineHeight = qMax(machineNameSize.height(), snapshotNameSize.height());
            int iSecondLineHeight = qMax(machineStatePixmapSize.height(), machineStateTextSize.height());
            int iSecondColumnHeight = iFirstLineHeight +
                                      iMachineItemTextSpacing +
                                      iSecondLineHeight;
            iProposedHeight += qMax(machinePixmapSize.height(), iSecondColumnHeight);
        }

        /* If Qt::MaximumSize was requested: */
        if (which == Qt::MaximumSize)
        {
            /* We are interested only in height calculation,
             * getting width from the base-class hint: */
            QSize maximumSize = UIGSelectorItem::sizeHint(Qt::MaximumSize, constraint).toSize();
            return QSize(maximumSize.width(), iProposedHeight);
        }

        /* Calculating proposed width: */
        int iProposedWidth = 0;
        {
            /* Simple machine item have 2 margins - left and right: */
            iProposedWidth += 2 * iMachineItemMargin;
            /* And machine item content to take into account: */
            int iFirstLineWidth = machineNameSize.width() +
                                  iMachineItemMinorSpacing +
                                  snapshotNameSize.width();
            int iSecondLineWidth = machineStatePixmapSize.width() +
                                   iMachineItemMinorSpacing +
                                   machineStateTextSize.width();
            int iSecondColumnWidth = qMax(iFirstLineWidth, iSecondLineWidth);
            int iMachineItemWidth = machinePixmapSize.width() +
                                    iMachineItemMajorSpacing +
                                    iSecondColumnWidth;
            iProposedWidth += iMachineItemWidth;
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

    /* Call to base-class: */
    return UIGSelectorItem::sizeHint(which, constraint);
}

QPixmap UIGSelectorItemMachine::toPixmap()
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

bool UIGSelectorItemMachine::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const
{
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should try to cast mime to known classes: */
    if (pMimeData->hasFormat(UIGSelectorItemMachine::className()))
    {
        /* Make sure passed item id is not ours: */
        const UIGSelectorItemMimeData *pCastedMimeData = qobject_cast<const UIGSelectorItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGSelectorItemMimeData!"));
        UIGSelectorItem *pItem = pCastedMimeData->item();
        return pItem->toMachineItem()->id() != id();
    }
    /* That was invalid mime: */
    return false;
}

void UIGSelectorItemMachine::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho, DragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Make sure this handler called by this item (not by children): */
    AssertMsg(!pFromWho && where == DragToken_Off, ("Machine graphics item do NOT support children!"));
    Q_UNUSED(pFromWho);
    Q_UNUSED(where);
    if (pMime->hasFormat(UIGSelectorItemMachine::className()))
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

                /* Group passed item with current item into the new group: */
                UIGSelectorItemGroup *pNewGroupItem = new UIGSelectorItemGroup(parentItem(), "New group");
                new UIGSelectorItemMachine(pNewGroupItem, this);
                new UIGSelectorItemMachine(pNewGroupItem, pItem->toMachineItem());

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }
                /* Delete this item: */
                delete this;

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
}

void UIGSelectorItemMachine::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != DragToken_Off)
    {
        setDragTokenPlace(DragToken_Off);
        update();
    }
}

QMimeData* UIGSelectorItemMachine::createMimeData()
{
    return new UIGSelectorItemMimeData(this);
}

void UIGSelectorItemMachine::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget * /* pWidget = 0 */)
{
    /* Initialize some necessary variables: */
    QRect fullRect = pOption->rect;
    int iMachineItemMargin = data(MachineItemData_MachineItemMargin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MachineItemMajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MachineItemMinorSpacing).toInt();
    int iMachineItemTextSpacing = data(MachineItemData_MachineItemTextSpacing).toInt();
    QSize machinePixmapSize = data(MachineItemData_MachinePixmapSize).toSize();
    QSize machineNameSize = data(MachineItemData_MachineNameSize).toSize();
    QString strSnapshotName = data(MachineItemData_SnapshotName).toString();
    QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
    QSize machineStatePixmapSize = data(MachineItemData_MachineStatePixmapSize).toSize();
    QSize machineStateTextSize = data(MachineItemData_MachineStateTextSize).toSize();

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
                    dragTokenPlace());

    /* Paint pixmap: */
    paintPixmap(/* Painter: */
                pPainter,
                /* Rectangle to paint in: */
                QRect(fullRect.topLeft() +
                      QPoint(iMachineItemMargin, iMachineItemMargin),
                      machinePixmapSize),
                /* Pixmap to paint: */
                data(MachineItemData_MachinePixmap).value<QIcon>().pixmap(machinePixmapSize));

    /* Paint name: */
    paintText(/* Painter: */
              pPainter,
              /* Rectangle to paint in: */
              QRect(fullRect.topLeft() +
                    QPoint(iMachineItemMargin, iMachineItemMargin) +
                    QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0),
                    machineNameSize),
              /* Font to paint text: */
              data(MachineItemData_MachineNameFont).value<QFont>(),
              /* Text to paint: */
              data(MachineItemData_MachineName).toString());

    /* Paint snapshot name (if necessary): */
    if (!strSnapshotName.isEmpty())
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

    /* Paint state pixmap: */
    paintPixmap(/* Painter: */
                pPainter,
                /* Rectangle to paint in: */
                QRect(fullRect.topLeft() +
                      QPoint(iMachineItemMargin, iMachineItemMargin) +
                      QPoint(machinePixmapSize.width() + iMachineItemMajorSpacing, 0) +
                      QPoint(0, machineNameSize.height() + iMachineItemTextSpacing),
                      machineStatePixmapSize),
                /* Pixmap to paint: */
                data(MachineItemData_MachineStatePixmap).value<QIcon>().pixmap(machineStatePixmapSize));

    /* Paint state text: */
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
              data(MachineItemData_MachineStateTextFont).value<QFont>(),
              /* Text to paint: */
              data(MachineItemData_MachineStateText).toString());

    /* Paint focus (if necessary): */
    if (model()->focusItem() == this)
        paintFocus(/* Painter: */
                   pPainter,
                   /* Rectangle to paint in: */
                   fullRect);
}

