/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItem class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserItem_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserItem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QGraphicsEffect>
#include <QMimeData>
#include <QRectF>
#include <QString>
#include <QUuid>

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "QIWithRetranslateUI.h"
#include "UIChooserDefs.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Forward declaration: */
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneDragDropEvent;
class QPropertyAnimation;
class QStateMachine;
class UIActionPool;
class UIChooserItemGroup;
class UIChooserItemGlobal;
class UIChooserItemMachine;
class UIChooserModel;
class UIChooserNode;

/** A simple QGraphicsEffect extension to mark disabled UIChooserItems. Applies blur and gray scale filters. */
class UIChooserDisabledItemEffect : public QGraphicsEffect
{
    Q_OBJECT;

public:

    UIChooserDisabledItemEffect(int iBlurRadius, QObject *pParent = 0);

protected:

    virtual void draw(QPainter *painter);
    int m_iBlurRadius;
};


/** QIGraphicsWidget extension used as interface
  * for graphics chooser model/view architecture. */
class UIChooserItem : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;
    Q_PROPERTY(int animatedValue READ animatedValue WRITE setAnimatedValue);

signals:

    /** @name Item stuff.
      * @{ */
        /** Notifies listeners about hover enter. */
        void sigHoverEnter();
        /** Notifies listeners about hover leave. */
        void sigHoverLeave();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Notifies listeners about @a iMinimumWidthHint change. */
        void sigMinimumWidthHintChanged(int iMinimumWidthHint);
    /** @} */

public:

    /** Constructs item passing @a pParent to the base-class.
      * @param  pNode          Brings the node this item is built for.
      * @param  iDefaultValue  Brings default value for hovering animation.
      * @param  iHoveredValue  Brings hovered value for hovering animation. */
    UIChooserItem(UIChooserItem *pParent, UIChooserNode *pNode,
                  int iDefaultValue = 100, int iHoveredValue = 90);

    /** @name Item stuff.
      * @{ */
        /** Returns parent reference. */
        UIChooserItem *parentItem() const {  return m_pParent; }
        /** Returns node reference. */
        UIChooserNode *node() const { return m_pNode; }

        /** Casts item to group one. */
        UIChooserItemGroup *toGroupItem();
        /** Casts item to global one. */
        UIChooserItemGlobal *toGlobalItem();
        /** Casts item to machine one. */
        UIChooserItemMachine *toMachineItem();

        /** Returns model reference. */
        UIChooserModel *model() const;
        /** Returns action-pool reference. */
        UIActionPool *actionPool() const;

        /** Returns whether item is root. */
        bool isRoot() const;

        /** Returns item name. */
        QString name() const;
        /** Returns item full-name. */
        QString fullName() const;
        /** Returns item description. */
        QString description() const;
        /** Returns item definition. */
        QString definition() const;

        /** Returns whether item is favorite. */
        bool isFavorite() const;
        /** Defines whether item is @a fFavorite. */
        virtual void setFavorite(bool fFavorite);

        /** Returns item position. */
        int position() const;

        /** Returns a level of item. */
        int level() const;
        /** Defines a @a iLevel of item. */
        void setLevel(int iLevel);

        /** Returns whether item is hovered. */
        bool isHovered() const;
        /** Defines whether item is @a fHovered. */
        void setHovered(bool fHovered);

        /** Starts item editing. */
        virtual void startEditing() = 0;

        /** Updates item. */
        virtual void updateItem() = 0;
        /** Updates item tool-tip. */
        virtual void updateToolTip() = 0;

        /** Installs event-filter for @a pSource object.
          * @note  Base-class implementation does nothing. */
        virtual void installEventFilterHelper(QObject *pSource) { Q_UNUSED(pSource); }
        /** Enables the visual effect for disabled item. */
        void disableEnableItem(bool fDisabled);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns children items of certain @a enmType. */
        virtual QList<UIChooserItem*> items(UIChooserItemType enmType = UIChooserItemType_Any) const = 0;

        /** Adds possible @a fFavorite child @a pItem to certain @a iPosition. */
        virtual void addItem(UIChooserItem *pItem, bool fFavorite, int iPosition) = 0;
        /** Removes child @a pItem. */
        virtual void removeItem(UIChooserItem *pItem) = 0;

        /** Searches for a first child item answering to specified @a strSearchTag and @a iItemSearchFlags. */
        virtual UIChooserItem *searchForItem(const QString &strSearchTag, int iItemSearchFlags) = 0;

        /** Searches for a first machine child item. */
        virtual UIChooserItem *firstMachineItem() = 0;

        /** Sorts children items. */
        virtual void sortItems() = 0;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates geometry. */
        virtual void updateGeometry() /* override */;

        /** Updates layout. */
        virtual void updateLayout() = 0;

        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const = 0;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const = 0;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Makes sure item is visible. */
        virtual void makeSureItsVisible();
        /** Makes sure passed child @a pItem is visible. */
        virtual void makeSureItemIsVisible(UIChooserItem *pItem) { Q_UNUSED(pItem); }

        /** Returns pixmap item representation. */
        virtual QPixmap toPixmap() = 0;

        /** Returns whether item drop is allowed.
          * @param  pEvent    Brings information about drop event.
          * @param  enmPlace  Brings the place of drag token to the drop moment. */
        virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, UIChooserItemDragToken enmPlace = UIChooserItemDragToken_Off) const = 0;
        /** Processes item drop.
          * @param  pEvent    Brings information about drop event.
          * @param  pFromWho  Brings the item according to which we choose drop position.
          * @param  enmPlace  Brings the place of drag token to the drop moment (according to item mentioned above). */
        virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho = 0, UIChooserItemDragToken enmPlace = UIChooserItemDragToken_Off) = 0;
        /** Reset drag token. */
        virtual void resetDragToken() = 0;

        /** Returns drag token place. */
        UIChooserItemDragToken dragTokenPlace() const;
        /** Defines drag token @a enmPlace. */
        void setDragTokenPlace(UIChooserItemDragToken enmPlace);
    /** @} */

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles hover enter @a event. */
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;
        /** Handles hover leave @a event. */
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;

        /** Handles mouse press @a event. */
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;
        /** Handles mouse move @a event. */
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

        /** Handles drag move @a event. */
        virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *pEvent) /* override */;
        /** Handles drag leave @a event. */
        virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *pEvent) /* override */;
        /** Handles drop @a event. */
        virtual void dropEvent(QGraphicsSceneDragDropEvent *pEvent) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns item's default animation value. */
        int defaultValue() const { return m_iDefaultValue; }
        /** Defines item's default animation @a iValue. */
        void setDefaultValue(int iValue) { m_iDefaultValue = iValue; update(); }

        /** Returns item's hovered animation value. */
        int hoveredValue() const { return m_iHoveredValue; }
        /** Defines item's hovered animation @a iValue. */
        void setHoveredValue(int iValue) { m_iHoveredValue = iValue; update(); }

        /** Returns item's animated value. */
        int animatedValue() const { return m_iAnimatedValue; }
        /** Defines item's animated @a iValue. */
        void setAnimatedValue(int iValue) { m_iAnimatedValue = iValue; update(); }
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Returns previous geometry. */
        QRectF previousGeometry() const { return m_previousGeometry; }
        /** Defines previous @a geometry. */
        void setPreviousGeometry(const QRectF &geometry) { m_previousGeometry = geometry; }

        /** Returns @a strText size calculated on the basis of certain @a font and @a pPaintDevice. */
        static QSize textSize(const QFont &font, QPaintDevice *pPaintDevice, const QString &strText);
        /** Returns a width of line containing @a iCount of chars calculated on the basis of certain @a font and @a pPaintDevice. */
        static int textWidth(const QFont &font, QPaintDevice *pPaintDevice, int iCount);
        /** Compresses @a strText to @a iWidth on the basis of certain @a font and @a pPaintDevice. */
        static QString compressText(const QFont &font, QPaintDevice *pPaintDevice, QString strText, int iWidth);
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns D&D mime data. */
        virtual QMimeData *createMimeData() = 0;

        /** Returns drag token darkness. */
        int dragTokenDarkness() const { return m_iDragTokenDarkness; }
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints frame @a rectangle using passed @a pPainter.
          * @param  fIsSelected  Brings whether this rectangle should be filled.
          * @param  iRadius      Brings the radius of rounded corners. */
        static void paintFrameRect(QPainter *pPainter, bool fIsSelected, int iRadius,
                                   const QRect &rectangle);
        /** Paints @a pixmap using passed @a pPainter putting its upper-left corner to specified @a point. */
        static void paintPixmap(QPainter *pPainter, const QPoint &point,
                                const QPixmap &pixmap);
        /** Paints @a strText using passed @a pPainter putting its upper-left corner to specified @a point.
          * @param  font          Brings the text font.
          * @param  pPaintDevice  Brings the paint-device reference to initilize painting from. */
        static void paintText(QPainter *pPainter, QPoint point,
                              const QFont &font, QPaintDevice *pPaintDevice,
                              const QString &strText);
        /** Paints flat button @a rectangle using passed @a pPainter moving light focus according to passed @a cursorPosition. */
        static void paintFlatButton(QPainter *pPainter, const QRect &rectangle, const QPoint &cursorPosition);
    /** @} */

private:

    /** @name Item stuff.
      * @{ */
        /** Holds the item's parent item. */
        UIChooserItem *m_pParent;
        /** Holds the node this item is built for. */
        UIChooserNode *m_pNode;

        /** Holds the item level according to root. */
        int  m_iLevel;

        /** Holds whether item is hovered. */
        bool                m_fHovered;
        /** Holds the hovering animation machine instance. */
        QStateMachine      *m_pHoveringMachine;
        /** Holds the forward hovering animation instance. */
        QPropertyAnimation *m_pHoveringAnimationForward;
        /** Holds the backward hovering animation instance. */
        QPropertyAnimation *m_pHoveringAnimationBackward;
        /** Holds the animation duration. */
        int                 m_iAnimationDuration;
        /** Holds the default animation value. */
        int                 m_iDefaultValue;
        /** Holds the hovered animation value. */
        int                 m_iHoveredValue;
        /** Holds the animated value. */
        int                 m_iAnimatedValue;
        /** Holds the pointer to blur effect instance. */
        UIChooserDisabledItemEffect *m_pDisabledEffect;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds previous geometry. */
        QRectF  m_previousGeometry;

        /** Holds previous minimum width hint. */
        int  m_iPreviousMinimumWidthHint;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Holds drag token place. */
        UIChooserItemDragToken  m_enmDragTokenPlace;

        /** Holds drag token darkness. */
        int  m_iDragTokenDarkness;
    /** @} */
};


/** QMimeData for graphics item interface. */
class UIChooserItemMimeData : public QMimeData
{
    Q_OBJECT;

public:

    /** Constructs mime-data on the basis of passed @a pItem. */
    UIChooserItemMimeData(UIChooserItem *pItem);

    /** Returns cached item. */
    UIChooserItem *item() const { return m_pItem; }

    /** Constructs mime-data on the basis of passed @a pItem. */
    virtual bool hasFormat(const QString &strMimeType) const /* override */;

private:

    /** Holds the cached item. */
    UIChooserItem *m_pItem;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserItem_h */
