/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItem class declaration
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

#ifndef __UIGChooserItem_h__
#define __UIGChooserItem_h__

/* Qt includes: */
#include <QMimeData>

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declaration: */
class UIGChooserModel;
class UIGChooserItemGroup;
class UIGChooserItemMachine;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneDragDropEvent;
class QStateMachine;
class QPropertyAnimation;

/* UIGChooserItem types: */
enum UIGChooserItemType
{
    UIGChooserItemType_Any     = QGraphicsItem::UserType,
    UIGChooserItemType_Group   = QGraphicsItem::UserType + 1,
    UIGChooserItemType_Machine = QGraphicsItem::UserType + 2
};

/* Drag token placement: */
enum DragToken { DragToken_Off, DragToken_Up, DragToken_Down };

/* Graphics item interface
 * for graphics selector model/view architecture: */
class UIGChooserItem : public QIGraphicsWidget
{
    Q_OBJECT;
    Q_PROPERTY(int gradient READ gradient WRITE setGradient);

signals:

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

public:

    /* Constructor: */
    UIGChooserItem(UIGChooserItem *pParent);

    /* API: Cast stuff: */
    UIGChooserItemGroup* toGroupItem();
    UIGChooserItemMachine* toMachineItem();

    /* API: Model stuff: */
    UIGChooserModel* model() const;

    /* API: Parent stuff: */
    UIGChooserItem* parentItem() const;

    /* API: Basic stuff: */
    virtual void show();
    virtual void hide();
    virtual void startEditing() = 0;
    virtual QString name() const = 0;
    void setRoot(bool fRoot);
    bool isRoot() const;
    bool isHovered() const { return m_fHovered; }
    void setHovered(bool fHovered) { m_fHovered = fHovered; }

    /* API: Children stuff: */
    virtual void addItem(UIGChooserItem *pItem, int iPosition) = 0;
    virtual void removeItem(UIGChooserItem *pItem) = 0;
    virtual void setItems(const QList<UIGChooserItem*> &items, UIGChooserItemType type) = 0;
    virtual QList<UIGChooserItem*> items(UIGChooserItemType type = UIGChooserItemType_Any) const = 0;
    virtual bool hasItems(UIGChooserItemType type = UIGChooserItemType_Any) const = 0;
    virtual void clearItems(UIGChooserItemType type = UIGChooserItemType_Any) = 0;

    /* API: Layout stuff: */
    virtual void updateSizeHint() = 0;
    virtual void updateLayout() = 0;
    virtual int minimumWidthHint() const = 0;
    virtual int minimumHeightHint() const = 0;

    /* API: Navigation stuff: */
    virtual void makeSureItsVisible();

    /* API: Drag and drop stuff: */
    virtual QPixmap toPixmap() = 0;
    virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where = DragToken_Off) const = 0;
    virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho = 0, DragToken where = DragToken_Off) = 0;
    virtual void resetDragToken() = 0;
    void setDragTokenPlace(DragToken where);
    DragToken dragTokenPlace() const;

protected:

    /* Hover-enter event: */
    void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent);
    /* Hover-leave event: */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent);
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
    static void configurePainterShape(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, int iRadius);
    static void paintFrameRect(QPainter *pPainter, const QRect &rect, bool fIsSelected, int iRadius);
    static void paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap);
    static void paintText(QPainter *pPainter, const QRect &rect, const QFont &font, const QString &strText);

    /* Helpers: Drag and drop stuff: */
    virtual QMimeData* createMimeData() = 0;

    /* Hover stuff: */
    int gradient() const { return m_iGradient; }
    void setGradient(int iGradient) { m_iGradient = iGradient; update(); }

    /* Helpers: Text compression stuff: */
    static int textWidth(const QFont &font, int iCount);
    static QString compressText(const QFont &font, QString strText, int iWidth);

private:

    /* Variables: */
    bool m_fRoot;
    UIGChooserItem *m_pParent;
    DragToken m_dragTokenPlace;

    /* Highlight animation stuff: */
    bool m_fHovered;
    QStateMachine *m_pHighlightMachine;
    QPropertyAnimation *m_pForwardAnimation;
    QPropertyAnimation *m_pBackwardAnimation;
    int m_iAnimationDuration;
    int m_iDefaultDarkness;
    int m_iHighlightDarkness;
    int m_iGradient;
};

/* Mime-data for graphics item interface: */
class UIGChooserItemMimeData : public QMimeData
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGChooserItemMimeData(UIGChooserItem *pItem);

    /* API: Format checker: */
    bool hasFormat(const QString &strMimeType) const;

    /* API: Item getter: */
    UIGChooserItem* item() const;

private:

    /* Variables: */
    UIGChooserItem *m_pItem;
};

#endif /* __UIGChooserItem_h__ */

