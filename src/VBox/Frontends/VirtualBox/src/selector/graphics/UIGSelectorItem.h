/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItem class declaration
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

#ifndef __UIGSelectorItem_h__
#define __UIGSelectorItem_h__

/* Qt includes: */
#include <QMimeData>

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declaration: */
class UIGraphicsSelectorModel;
class UIGSelectorItemGroup;
class UIGSelectorItemMachine;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneDragDropEvent;
class QPropertyAnimation;

/* UIGSelectorItem types: */
enum UIGSelectorItemType
{
    UIGSelectorItemType_Any     = QGraphicsItem::UserType,
    UIGSelectorItemType_Group   = QGraphicsItem::UserType + 1,
    UIGSelectorItemType_Machine = QGraphicsItem::UserType + 2
};

/* Drag token placement: */
enum DragToken { DragToken_Off, DragToken_Up, DragToken_Down };

/* Graphics item interface
 * for graphics selector model/view architecture: */
class UIGSelectorItem : public QIGraphicsWidget
{
    Q_OBJECT;
    Q_PROPERTY(int gradient READ gradient WRITE setGradient);

public:

    /* Constructor/destructor: */
    UIGSelectorItem(UIGSelectorItem *pParent, int iPosition);
    ~UIGSelectorItem();

    /* API: Cast stuff: */
    UIGSelectorItemGroup* toGroupItem();
    UIGSelectorItemMachine* toMachineItem();

    /* API: Model stuff: */
    UIGraphicsSelectorModel* model() const;

    /* API: Parent stuff: */
    UIGSelectorItem* parentItem() const;

    /* API: Basic stuff: */
    virtual void startEditing() = 0;
    virtual QString name() const = 0;
    virtual void cleanup() = 0;

    /* API: Children stuff: */
    virtual void addItem(UIGSelectorItem *pItem, int iPosition) = 0;
    virtual void removeItem(UIGSelectorItem *pItem) = 0;
    virtual QList<UIGSelectorItem*> items(UIGSelectorItemType type) const = 0;
    virtual bool hasItems(UIGSelectorItemType type = UIGSelectorItemType_Any) const = 0;
    virtual void clearItems(UIGSelectorItemType type = UIGSelectorItemType_Any) = 0;

    /* API: Layout stuff: */
    virtual void updateSizeHint() = 0;
    virtual void updateLayout() = 0;
    virtual void setDesiredWidth(int iDesiredWidth);
    virtual void setDesiredHeight(int iDesiredHeight);
    virtual int desiredWidth() const;
    virtual int desiredHeight() const;

    /* API: Navigation stuff: */
    virtual void makeSureItsVisible();

    /* API: Drag and drop stuff: */
    virtual QPixmap toPixmap() = 0;
    virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where = DragToken_Off) const = 0;
    virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho = 0, DragToken where = DragToken_Off) = 0;
    virtual void resetDragToken() = 0;
    void setDragTokenPlace(DragToken where);
    DragToken dragTokenPlace() const;

protected:

    /* Hover-enter event: */
    void hoverEnterEvent(QGraphicsSceneHoverEvent *pEvent);
    /* Mouse-press event: */
    void mousePressEvent(QGraphicsSceneMouseEvent *pEvent);
    /* Mouse-move event: */
    void mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent);
    /* Drag-move event: */
    void dragMoveEvent(QGraphicsSceneDragDropEvent *pEvent);
    /* Drag-leave event: */
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent);
    /* Drop event: */
    void dropEvent(QGraphicsSceneDragDropEvent *pEvent);

    /* Static paint stuff: */
    static void paintBackground(QPainter *pPainter, const QRect &rect,
                                bool fHasParent, bool fIsSelected,
                                int iGradientDarkness, DragToken dragTokenWhere);
    static void paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap);
    static void paintText(QPainter *pPainter, const QRect &rect, const QFont &font, const QString &strText);
    static void paintFocus(QPainter *pPainter, const QRect &rect);

    /* Helpers: Drag and drop stuff: */
    virtual QMimeData* createMimeData() = 0;

    /* Gradient animation (property) stuff: */
    int gradient() const { return m_iInitialGradientDarkness; }
    void setGradient(int iGradient) { m_iInitialGradientDarkness = iGradient; update(); }

private:

    /* Variables: */
    UIGSelectorItem *m_pParent;
    int m_iDesiredWidth;
    int m_iDesiredHeight;
    DragToken m_dragTokenPlace;

    /* Gradient animation (property) stuff: */
    QPropertyAnimation *m_pAnimation;
    int m_iInitialGradientDarkness;
};

/* Mime-data for graphics item interface: */
class UIGSelectorItemMimeData : public QMimeData
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGSelectorItemMimeData(UIGSelectorItem *pItem);

    /* API: Format checker: */
    bool hasFormat(const QString &strMimeType) const;

    /* API: Item getter: */
    UIGSelectorItem* item() const;

private:

    /* Variables: */
    UIGSelectorItem *m_pItem;
};

#endif // __UIGSelectorItem_h__

