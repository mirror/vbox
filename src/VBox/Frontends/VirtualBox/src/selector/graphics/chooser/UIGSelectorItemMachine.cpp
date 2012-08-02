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
QString UIGSelectorItemMachine::className() { return "UIGSelectorItemMachine"; }

UIGSelectorItemMachine::UIGSelectorItemMachine(UIGSelectorItem *pParent,
                                               const CMachine &machine,
                                               int iPosition /* = -1 */)
    : UIGSelectorItem(pParent)
    , UIVMItem(machine)
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(6)
{
    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);

    /* Prepare: */
    prepare();
}

UIGSelectorItemMachine::UIGSelectorItemMachine(UIGSelectorItem *pParent,
                                               UIGSelectorItemMachine *pCopyFrom,
                                               int iPosition /* = -1 */)
    : UIGSelectorItem(pParent)
    , UIVMItem(pCopyFrom->machine())
    , m_pToolBar(0)
    , m_pSettingsButton(0)
    , m_pStartButton(0)
    , m_pPauseButton(0)
    , m_pCloseButton(0)
    , m_iCornerRadius(6)
{
    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for machine item!"));
    parentItem()->addItem(this, iPosition);

    /* Prepare: */
    prepare();
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
        case MachineItemData_Name: return name();
        case MachineItemData_SnapshotName: return snapshotName();
        case MachineItemData_StateText: return gpConverter->toString(machineState());
        /* Sizes: */
        case MachineItemData_PixmapSize: return osIcon().availableSizes().at(0);
        case MachineItemData_StatePixmapSize: return machineStateIcon().availableSizes().at(0);
        case MachineItemData_NameSize:
        {
            QFontMetrics fm(data(MachineItemData_NameFont).value<QFont>());
            return QSize(fm.width(data(MachineItemData_Name).toString()), fm.height());
        }
        case MachineItemData_SnapshotNameSize:
        {
            QFontMetrics fm(data(MachineItemData_SnapshotNameFont).value<QFont>());
            return QSize(fm.width(QString("(%1)").arg(data(MachineItemData_SnapshotName).toString())), fm.height());
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

void UIGSelectorItemMachine::startEditing()
{
    AssertMsgFailed(("Machine graphics item do NOT support editing yet!"));
}

void UIGSelectorItemMachine::addItem(UIGSelectorItem*, int)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGSelectorItemMachine::removeItem(UIGSelectorItem*)
{
    AssertMsgFailed(("Machine graphics item do NOT support children!"));
}

void UIGSelectorItemMachine::moveItemTo(UIGSelectorItem*, int)
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

int UIGSelectorItemMachine::minimumWidthHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemMajorSpacing = data(MachineItemData_MajorSpacing).toInt();
    int iMachineItemMinorSpacing = data(MachineItemData_MinorSpacing).toInt();
    QSize machinePixmapSize = data(MachineItemData_PixmapSize).toSize();
    QSize machineNameSize = data(MachineItemData_NameSize).toSize();
    QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
    QSize machineStatePixmapSize = data(MachineItemData_StatePixmapSize).toSize();
    QSize machineStateTextSize = data(MachineItemData_StateTextSize).toSize();
    QSize toolBarSize = data(MachineItemData_ToolBarSize).toSize();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* We are taking into account only left margin,
     * tool-bar contains right one: */
    iProposedWidth += iMachineItemMargin;
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
                            iSecondColumnWidth +
                            iMachineItemMajorSpacing +
                            toolBarSize.width() + 1;
    iProposedWidth += iMachineItemWidth;

    /* Return result: */
    return iProposedWidth;
}

int UIGSelectorItemMachine::minimumHeightHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMachineItemMargin = data(MachineItemData_Margin).toInt();
    int iMachineItemTextSpacing = data(MachineItemData_TextSpacing).toInt();
    QSize machinePixmapSize = data(MachineItemData_PixmapSize).toSize();
    QSize machineNameSize = data(MachineItemData_NameSize).toSize();
    QSize snapshotNameSize = data(MachineItemData_SnapshotNameSize).toSize();
    QSize machineStatePixmapSize = data(MachineItemData_StatePixmapSize).toSize();
    QSize machineStateTextSize = data(MachineItemData_StateTextSize).toSize();
    QSize toolBarSize = data(MachineItemData_ToolBarSize).toSize();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Simple machine item have 2 margins - top and bottom: */
    iProposedHeight += 2 * iMachineItemMargin;
    /* And machine item content to take into account: */
    int iFirstLineHeight = qMax(machineNameSize.height(), snapshotNameSize.height());
    int iSecondLineHeight = qMax(machineStatePixmapSize.height(), machineStateTextSize.height());
    int iSecondColumnHeight = iFirstLineHeight +
                              iMachineItemTextSpacing +
                              iSecondLineHeight;
    QList<int> heights;
    heights << machinePixmapSize.height() << iSecondColumnHeight
            << (toolBarSize.height() - 2 * iMachineItemMargin + 2);
    int iMaxHeight = 0;
    foreach (int iHeight, heights)
        iMaxHeight = qMax(iMaxHeight, iHeight);
    iProposedHeight += iMaxHeight;

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIGSelectorItemMachine::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
    {
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
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
        /* Paint machine info: */
        paintMachineInfo(pPainter, pOption);
    }
}

void UIGSelectorItemMachine::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
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

void UIGSelectorItemMachine::paintMachineInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
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
    if (model()->focusItem() == this)
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

void UIGSelectorItemMachine::prepare()
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

