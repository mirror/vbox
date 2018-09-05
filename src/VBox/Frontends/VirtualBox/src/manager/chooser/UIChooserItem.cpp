/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItem class definition.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAccessibleObject>
# include <QApplication>
# include <QStyle>
# include <QPainter>
# include <QGraphicsScene>
# include <QStyleOptionFocusRect>
# include <QGraphicsSceneMouseEvent>
# include <QStateMachine>
# include <QPropertyAnimation>
# include <QSignalTransition>
# include <QDrag>

/* GUI includes: */
# include "UIChooser.h"
# include "UIChooserItem.h"
# include "UIChooserView.h"
# include "UIChooserModel.h"
# include "UIChooserItemGroup.h"
# include "UIChooserItemGlobal.h"
# include "UIChooserItemMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleObject extension used as an accessibility interface for Chooser-view items. */
class UIAccessibilityInterfaceForUIChooserItem : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating Chooser-view accessibility interface: */
        if (pObject && strClassname == QLatin1String("UIChooserItem"))
            return new UIAccessibilityInterfaceForUIChooserItem(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    UIAccessibilityInterfaceForUIChooserItem(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Return the parent: */
        return QAccessible::queryAccessibleInterface(item()->model()->chooser()->view());
    }

    /** Returns the number of children. */
    virtual int childCount() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Return the number of group children: */
        if (item()->type() == UIChooserItemType_Group)
            return item()->items().size();

        /* Zero by default: */
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);
        /* Make sure index is valid: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(item()->items().at(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const /* override */
    {
        /* Search for corresponding child: */
        for (int i = 0; i < childCount(); ++i)
            if (child(i) == pChild)
                return i;

        /* -1 by default: */
        return -1;
    }

    /** Returns the rect. */
    virtual QRect rect() const /* override */
    {
        /* Now goes the mapping: */
        const QSize   itemSize         = item()->size().toSize();
        const QPointF itemPosInScene   = item()->mapToScene(QPointF(0, 0));
        const QPoint  itemPosInView    = item()->model()->chooser()->view()->mapFromScene(itemPosInScene);
        const QPoint  itemPosInScreen  = item()->model()->chooser()->view()->mapToGlobal(itemPosInView);
        const QRect   itemRectInScreen = QRect(itemPosInScreen, itemSize);
        return itemRectInScreen;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QString());

        switch (enmTextRole)
        {
            case QAccessible::Name:        return item()->name();
            case QAccessible::Description: return item()->description();
            default: break;
        }

        /* Null-string by default: */
        return QString();
    }

    /** Returns the role. */
    virtual QAccessible::Role role() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QAccessible::NoRole);

        /* Return the role of group: */
        if (item()->type() == UIChooserItemType_Group)
            return QAccessible::List;

        /* ListItem by default: */
        return QAccessible::ListItem;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QAccessible::State());

        /* Compose the state: */
        QAccessible::State state;
        state.focusable = true;
        state.selectable = true;

        /* Compose the state of current item: */
        if (item() && item() == item()->model()->currentItem())
        {
            state.active = true;
            state.focused = true;
            state.selected = true;
        }

        /* Compose the state of group: */
        if (item()->type() == UIChooserItemType_Group)
        {
            state.expandable = true;
            if (!item()->toGroupItem()->isClosed())
                state.expanded = true;
        }

        /* Return the state: */
        return state;
    }

private:

    /** Returns corresponding Chooser-view item. */
    UIChooserItem *item() const { return qobject_cast<UIChooserItem*>(object()); }
};


/*********************************************************************************************************************************
*   Class UIChooserItem implementation.                                                                                          *
*********************************************************************************************************************************/

UIChooserItem::UIChooserItem(UIChooserItem *pParent, bool fTemporary)
    : m_pParent(pParent)
    , m_fTemporary(fTemporary)
    , m_fRoot(!pParent)
    , m_fHovered(false)
    , m_pHoveringMachine(0)
    , m_pHoveringAnimationForward(0)
    , m_pHoveringAnimationBackward(0)
    , m_iAnimationDuration(400)
    , m_iDefaultValue(100)
    , m_iHoveredValue(90)
    , m_iAnimatedValue(m_iDefaultValue)
    , m_iPreviousMinimumWidthHint(0)
    , m_iPreviousMinimumHeightHint(0)
    , m_enmDragTokenPlace(DragToken_Off)
    , m_iDragTokenDarkness(110)
{
    /* Install Chooser-view item accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUIChooserItem::pFactory);

    /* Basic item setup: */
    setOwnedByLayout(false);
    setAcceptDrops(true);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setAcceptHoverEvents(!isRoot());

    /* Non-root item? */
    if (!isRoot())
    {
        /* Create hovering animation machine: */
        m_pHoveringMachine = new QStateMachine(this);
        if (m_pHoveringMachine)
        {
            /* Create 'default' state: */
            QState *pStateDefault = new QState(m_pHoveringMachine);
            /* Create 'hovered' state: */
            QState *pStateHovered = new QState(m_pHoveringMachine);

            /* Configure 'default' state: */
            if (pStateDefault)
            {
                /* When we entering default state => we assigning animatedValue to m_iDefaultValue: */
                pStateDefault->assignProperty(this, "animatedValue", m_iDefaultValue);

                /* Add state transitions: */
                QSignalTransition *pDefaultToHovered = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHovered);
                if (pDefaultToHovered)
                {
                    /* Create forward animation: */
                    m_pHoveringAnimationForward = new QPropertyAnimation(this, "animatedValue", this);
                    if (m_pHoveringAnimationForward)
                    {
                        m_pHoveringAnimationForward->setDuration(m_iAnimationDuration);
                        m_pHoveringAnimationForward->setStartValue(m_iDefaultValue);
                        m_pHoveringAnimationForward->setEndValue(m_iHoveredValue);

                        /* Add to transition: */
                        pDefaultToHovered->addAnimation(m_pHoveringAnimationForward);
                    }
                }
            }

            /* Configure 'hovered' state: */
            if (pStateHovered)
            {
                /* When we entering hovered state => we assigning animatedValue to m_iHoveredValue: */
                pStateHovered->assignProperty(this, "animatedValue", m_iHoveredValue);

                /* Add state transitions: */
                QSignalTransition *pHoveredToDefault = pStateHovered->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
                if (pHoveredToDefault)
                {
                    /* Create backward animation: */
                    m_pHoveringAnimationBackward = new QPropertyAnimation(this, "animatedValue", this);
                    if (m_pHoveringAnimationBackward)
                    {
                        m_pHoveringAnimationBackward->setDuration(m_iAnimationDuration);
                        m_pHoveringAnimationBackward->setStartValue(m_iHoveredValue);
                        m_pHoveringAnimationBackward->setEndValue(m_iDefaultValue);

                        /* Add to transition: */
                        pHoveredToDefault->addAnimation(m_pHoveringAnimationBackward);
                    }
                }
            }

            /* Initial state is 'default': */
            m_pHoveringMachine->setInitialState(pStateDefault);
            /* Start state-machine: */
            m_pHoveringMachine->start();
        }
    }
}

UIChooserItemGroup *UIChooserItem::toGroupItem()
{
    UIChooserItemGroup *pItem = qgraphicsitem_cast<UIChooserItemGroup*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIChooserItemGroup!"));
    return pItem;
}

UIChooserItemGlobal *UIChooserItem::toGlobalItem()
{
    UIChooserItemGlobal *pItem = qgraphicsitem_cast<UIChooserItemGlobal*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIChooserItemGlobal!"));
    return pItem;
}

UIChooserItemMachine *UIChooserItem::toMachineItem()
{
    UIChooserItemMachine *pItem = qgraphicsitem_cast<UIChooserItemMachine*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIChooserItemMachine!"));
    return pItem;
}

UIChooserModel *UIChooserItem::model() const
{
    UIChooserModel *pModel = qobject_cast<UIChooserModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

UIActionPool *UIChooserItem::actionPool() const
{
    return model()->actionPool();
}

int UIChooserItem::level() const
{
    int iLevel = 0;
    UIChooserItem *pParentItem = parentItem();
    while (pParentItem && !pParentItem->isRoot())
    {
        pParentItem = pParentItem->parentItem();
        ++iLevel;
    }
    return iLevel;
}

void UIChooserItem::show()
{
    /* Call to base-class: */
    QIGraphicsWidget::show();
}

void UIChooserItem::hide()
{
    /* Call to base-class: */
    QIGraphicsWidget::hide();
}

void UIChooserItem::setRoot(bool fRoot)
{
    m_fRoot = fRoot;
    handleRootStatusChange();
}

bool UIChooserItem::isRoot() const
{
    return m_fRoot;
}

void UIChooserItem::setHovered(bool fHovered)
{
    m_fHovered = fHovered;
    if (m_fHovered)
        emit sigHoverEnter();
    else
        emit sigHoverLeave();
}

bool UIChooserItem::isHovered() const
{
    return m_fHovered;
}

void UIChooserItem::updateGeometry()
{
    /* Call to base-class: */
    QIGraphicsWidget::updateGeometry();

    /* Update parent's geometry: */
    if (parentItem())
        parentItem()->updateGeometry();

    /* Special handling for root-items: */
    if (isRoot())
    {
        /* Root-item should notify chooser-view if minimum-width-hint was changed: */
        int iMinimumWidthHint = minimumWidthHint();
        if (m_iPreviousMinimumWidthHint != iMinimumWidthHint)
        {
            /* Save new minimum-width-hint, notify listener: */
            m_iPreviousMinimumWidthHint = iMinimumWidthHint;
            emit sigMinimumWidthHintChanged(m_iPreviousMinimumWidthHint);
        }
        /* Root-item should notify chooser-view if minimum-height-hint was changed: */
        int iMinimumHeightHint = minimumHeightHint();
        if (m_iPreviousMinimumHeightHint != iMinimumHeightHint)
        {
            /* Save new minimum-height-hint, notify listener: */
            m_iPreviousMinimumHeightHint = iMinimumHeightHint;
            emit sigMinimumHeightHintChanged(m_iPreviousMinimumHeightHint);
        }
    }
}

void UIChooserItem::makeSureItsVisible()
{
    /* If item is not visible: */
    if (!isVisible())
    {
        /* Get parrent item, assert if can't: */
        if (UIChooserItemGroup *pParentItem = parentItem()->toGroupItem())
        {
            /* We should make parent visible: */
            pParentItem->makeSureItsVisible();
            /* And make sure its opened: */
            if (pParentItem->isClosed())
                pParentItem->open(false);
        }
    }
}

void UIChooserItem::setDragTokenPlace(DragToken enmPlace)
{
    /* Something changed? */
    if (m_enmDragTokenPlace != enmPlace)
    {
        m_enmDragTokenPlace = enmPlace;
        update();
    }
}

DragToken UIChooserItem::dragTokenPlace() const
{
    return m_enmDragTokenPlace;
}

void UIChooserItem::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
        update();
    }
}

void UIChooserItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_fHovered)
    {
        m_fHovered = false;
        emit sigHoverLeave();
        update();
    }
}

void UIChooserItem::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* By default, non-moveable and non-selectable items
     * can't grab mouse-press events which is required
     * to grab further mouse-move events which we wants... */
    if (isRoot())
        pEvent->ignore();
    else
        pEvent->accept();
}

void UIChooserItem::mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Make sure item is really dragged: */
    if (QLineF(pEvent->screenPos(),
               pEvent->buttonDownScreenPos(Qt::LeftButton)).length() <
        QApplication::startDragDistance())
        return;

    /* Initialize dragging: */
    QDrag *pDrag = new QDrag(pEvent->widget());
    model()->setCurrentDragObject(pDrag);
    pDrag->setPixmap(toPixmap());
    pDrag->setMimeData(createMimeData());
    pDrag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
}

void UIChooserItem::dragMoveEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Make sure we are non-root: */
    if (!isRoot())
    {
        /* Allow drag tokens only for the same item type as current: */
        bool fAllowDragToken = false;
        if ((type() == UIChooserItemType_Group &&
             pEvent->mimeData()->hasFormat(UIChooserItemGroup::className())) ||
            (type() == UIChooserItemType_Machine &&
             pEvent->mimeData()->hasFormat(UIChooserItemMachine::className())))
            fAllowDragToken = true;
        /* Do we need a drag-token? */
        if (fAllowDragToken)
        {
            QPoint p = pEvent->pos().toPoint();
            if (p.y() < 10)
                setDragTokenPlace(DragToken_Up);
            else if (p.y() > minimumSizeHint().toSize().height() - 10)
                setDragTokenPlace(DragToken_Down);
            else
                setDragTokenPlace(DragToken_Off);
        }
    }
    /* Check if drop is allowed: */
    pEvent->setAccepted(isDropAllowed(pEvent, dragTokenPlace()));
}

void UIChooserItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *)
{
    resetDragToken();
}

void UIChooserItem::dropEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Do we have token active? */
    switch (dragTokenPlace())
    {
        case DragToken_Off:
        {
            /* Its our drop, processing: */
            processDrop(pEvent);
            break;
        }
        default:
        {
            /* Its parent drop, passing: */
            parentItem()->processDrop(pEvent, this, dragTokenPlace());
            break;
        }
    }
}

void UIChooserItem::handleRootStatusChange()
{
    /* Reset minimum size hints for non-root items: */
    if (!isRoot())
    {
        m_iPreviousMinimumWidthHint = 0;
        m_iPreviousMinimumHeightHint = 0;
    }
}

/* static */
QSize UIChooserItem::textSize(const QFont &font, QPaintDevice *pPaintDevice, const QString &strText)
{
    /* Make sure text is not empty: */
    if (strText.isEmpty())
        return QSize(0, 0);

    /* Return text size, based on font-metrics: */
    QFontMetrics fm(font, pPaintDevice);
    return QSize(fm.width(strText), fm.height());
}

/* static */
int UIChooserItem::textWidth(const QFont &font, QPaintDevice *pPaintDevice, int iCount)
{
    /* Return text width: */
    QFontMetrics fm(font, pPaintDevice);
    QString strString;
    strString.fill('_', iCount);
    return fm.width(strString);
}

/* static */
QString UIChooserItem::compressText(const QFont &font, QPaintDevice *pPaintDevice, QString strText, int iWidth)
{
    /* Check if passed text is empty: */
    if (strText.isEmpty())
        return strText;

    /* Check if passed text feats maximum width: */
    QFontMetrics fm(font, pPaintDevice);
    if (fm.width(strText) <= iWidth)
        return strText;

    /* Truncate otherwise: */
    QString strEllipsis = QString("...");
    int iEllipsisWidth = fm.width(strEllipsis + " ");
    while (!strText.isEmpty() && fm.width(strText) + iEllipsisWidth > iWidth)
        strText.truncate(strText.size() - 1);
    return strText + strEllipsis;
}

/* static */
void UIChooserItem::paintFrameRect(QPainter *pPainter, bool fIsSelected, int iRadius,
                                   const QRect &rectangle)
{
    pPainter->save();
    QPalette pal = QApplication::palette();
    QColor base = pal.color(QPalette::Active, fIsSelected ? QPalette::Highlight : QPalette::Window);
    pPainter->setPen(base.darker(160));
    if (iRadius)
        pPainter->drawRoundedRect(rectangle, iRadius, iRadius);
    else
        pPainter->drawRect(rectangle);
    pPainter->restore();
}

/* static */
void UIChooserItem::paintPixmap(QPainter *pPainter, const QPoint &point,
                                const QPixmap &pixmap)
{
    pPainter->drawPixmap(point, pixmap);
}

/* static */
void UIChooserItem::paintText(QPainter *pPainter, QPoint point,
                              const QFont &font, QPaintDevice *pPaintDevice,
                              const QString &strText)
{
    /* Prepare variables: */
    QFontMetrics fm(font, pPaintDevice);
    point += QPoint(0, fm.ascent());

    /* Draw text: */
    pPainter->save();
    pPainter->setFont(font);
    pPainter->drawText(point, strText);
    pPainter->restore();
}


/*********************************************************************************************************************************
*   Class UIChooserItemMimeData implementation.                                                                                  *
*********************************************************************************************************************************/

UIChooserItemMimeData::UIChooserItemMimeData(UIChooserItem *pItem)
    : m_pItem(pItem)
{
}

bool UIChooserItemMimeData::hasFormat(const QString &strMimeType) const
{
    if (strMimeType == QString(m_pItem->metaObject()->className()))
        return true;
    return false;
}
