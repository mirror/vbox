/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItem class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIChooserItem_h__
#define __UIChooserItem_h__

/* Qt includes: */
#include <QMimeData>
#include <QString>

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "QIWithRetranslateUI.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Forward declaration: */
class UIActionPool;
class UIChooserModel;
class UIChooserItemGroup;
class UIChooserItemGlobal;
class UIChooserItemMachine;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneDragDropEvent;
class QStateMachine;
class QPropertyAnimation;

/* UIChooserItem types: */
enum UIChooserItemType
{
    UIChooserItemType_Any = QGraphicsItem::UserType,
    UIChooserItemType_Group,
    UIChooserItemType_Global,
    UIChooserItemType_Machine
};

/* Item search flags: */
enum UIChooserItemSearchFlag
{
    UIChooserItemSearchFlag_Machine   = RT_BIT(0),
    UIChooserItemSearchFlag_Global    = RT_BIT(1),
    UIChooserItemSearchFlag_Group     = RT_BIT(2),
    UIChooserItemSearchFlag_ExactName = RT_BIT(3)
};

/* Drag token placement: */
enum DragToken { DragToken_Off, DragToken_Up, DragToken_Down };

/* Graphics item interface
 * for graphics selector model/view architecture: */
class UIChooserItem : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;
    Q_PROPERTY(int animationDarkness READ animationDarkness WRITE setAnimationDarkness);

signals:

    /* Notifiers: Size-hint stuff: */
    void sigMinimumWidthHintChanged(int iMinimumWidthHint);
    void sigMinimumHeightHintChanged(int iMinimumHeightHint);

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

public:

    /* Constructor: */
    UIChooserItem(UIChooserItem *pParent, bool fTemporary);

    /* API: Cast stuff: */
    UIChooserItemGroup *toGroupItem();
    UIChooserItemGlobal *toGlobalItem();
    UIChooserItemMachine *toMachineItem();

    /* API: Model stuff: */
    UIChooserModel* model() const;

    /** Returns the action-pool reference. */
    UIActionPool* actionPool() const;

    /* API: Parent stuff: */
    UIChooserItem* parentItem() const;

    /* API: Basic stuff: */
    virtual void show();
    virtual void hide();
    virtual void startEditing() = 0;
    virtual void updateToolTip() = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString fullName() const = 0;
    virtual QString definition() const = 0;
    void setRoot(bool fRoot);
    bool isRoot() const;
    bool isHovered() const;
    void setHovered(bool fHovered);

    /* API: Children stuff: */
    virtual void addItem(UIChooserItem *pItem, int iPosition) = 0;
    virtual void removeItem(UIChooserItem *pItem) = 0;
    virtual void setItems(const QList<UIChooserItem*> &items, UIChooserItemType type) = 0;
    virtual QList<UIChooserItem*> items(UIChooserItemType type = UIChooserItemType_Any) const = 0;
    virtual bool hasItems(UIChooserItemType type = UIChooserItemType_Any) const = 0;
    virtual void clearItems(UIChooserItemType type = UIChooserItemType_Any) = 0;
    virtual void updateAll(const QString &strId) = 0;
    virtual void removeAll(const QString &strId) = 0;
    virtual UIChooserItem* searchForItem(const QString &strSearchTag, int iItemSearchFlags) = 0;
    virtual UIChooserItem* firstMachineItem() = 0;
    virtual void sortItems() = 0;

    /* API: Layout stuff: */
    void updateGeometry();
    virtual void updateLayout() = 0;
    virtual int minimumWidthHint() const = 0;
    virtual int minimumHeightHint() const = 0;

    /* API: Navigation stuff: */
    virtual void makeSureItsVisible();

    /* API: Drag and drop stuff: */
    virtual QPixmap toPixmap() = 0;
    virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where = DragToken_Off) const = 0;
    virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho = 0, DragToken where = DragToken_Off) = 0;
    virtual void resetDragToken() = 0;
    void setDragTokenPlace(DragToken where);
    DragToken dragTokenPlace() const;

    /* API: Toggle stuff: */
    bool isTemporary() const;

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

    /* Helper: Update stuff: */
    virtual void handleRootStatusChange();
    void setPreviousGeometry(const QRectF &previousGeometry) { m_previousGeometry = previousGeometry; }
    const QRectF& previousGeometry() const { return m_previousGeometry; }

    /* Static paint stuff: */
    static void configurePainterShape(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, int iRadius);
    static void paintFrameRect(QPainter *pPainter, const QRect &rect, bool fIsSelected, int iRadius);
    static void paintPixmap(QPainter *pPainter, const QPoint &point, const QPixmap &pixmap);
    static void paintText(QPainter *pPainter, QPoint point,
                          const QFont &font, QPaintDevice *pPaintDevice,
                          const QString &strText);

    /* Helpers: Drag and drop stuff: */
    virtual QMimeData* createMimeData() = 0;

    /* Hover stuff: */
    int defaultDarkness() const { return m_iDefaultDarkness; }
    int highlightDarkness() const { return m_iHighlightDarkness; }
    int animationDarkness() const { return m_iAnimationDarkness; }
    void setDefaultDarkness(int iDefaultDarkness) { m_iDefaultDarkness = iDefaultDarkness; update(); }
    void setHighlightDarkness(int iHighlightDarkness) { m_iHighlightDarkness = iHighlightDarkness; update(); }
    void setAnimationDarkness(int iAnimationDarkness) { m_iAnimationDarkness = iAnimationDarkness; update(); }

    /* Other color stuff: */
    int dragTokenDarkness() const { return m_iDragTokenDarkness; }

    /* Helpers: Text processing stuff: */
    static QSize textSize(const QFont &font, QPaintDevice *pPaintDevice, const QString &strText);
    static int textWidth(const QFont &font, QPaintDevice *pPaintDevice, int iCount);
    static QString compressText(const QFont &font, QPaintDevice *pPaintDevice, QString strText, int iWidth);

private:

    /* Variables: */
    bool m_fRoot;
    bool m_fTemporary;
    UIChooserItem *m_pParent;
    QRectF m_previousGeometry;
    int m_iPreviousMinimumWidthHint;
    int m_iPreviousMinimumHeightHint;
    DragToken m_dragTokenPlace;

    /* Highlight animation stuff: */
    bool m_fHovered;
    QStateMachine *m_pHighlightMachine;
    QPropertyAnimation *m_pForwardAnimation;
    QPropertyAnimation *m_pBackwardAnimation;
    int m_iAnimationDuration;
    int m_iDefaultDarkness;
    int m_iHighlightDarkness;
    int m_iAnimationDarkness;
    int m_iDragTokenDarkness;
};

/* Mime-data for graphics item interface: */
class UIChooserItemMimeData : public QMimeData
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIChooserItemMimeData(UIChooserItem *pItem);

    /* API: Format checker: */
    bool hasFormat(const QString &strMimeType) const;

    /* API: Item getter: */
    UIChooserItem* item() const;

private:

    /* Variables: */
    UIChooserItem *m_pItem;
};

#endif /* __UIChooserItem_h__ */

