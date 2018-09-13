/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsItem class declaration.
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

#ifndef ___UIToolsItem_h___
#define ___UIToolsItem_h___

/* Qt includes: */
#include <QIcon>
#include <QMimeData>
#include <QPixmap>
#include <QRectF>
#include <QString>

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "UITools.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Forward declaration: */
class QPropertyAnimation;
class QGraphicsScene;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QStateMachine;
class UIActionPool;
class UIToolsItemGroup;
class UIToolsItemGlobal;
class UIToolsItemMachine;
class UIToolsModel;

/** QIGraphicsWidget extension used as interface
  * for graphics Tools-model/view architecture. */
class UIToolsItem : public QIGraphicsWidget
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
        /** Notifies listeners about minimum width @a iHint change. */
        void sigMinimumWidthHintChanged(int iHint);
        /** Notifies listeners about minimum height @a iHint change. */
        void sigMinimumHeightHintChanged(int iHint);
    /** @} */

public:

    /** Constructs item on the basis of passed arguments.
      * @param  pScene   Brings the scene reference to add item to.
      * @param  icon     Brings the item icon.
      * @param  strName  Brings the item name. */
    UIToolsItem(QGraphicsScene *pScene,
                UIToolsClass enmClass, UIToolsType enmType,
                const QIcon &icon, const QString &strName);
    /** Destructs item. */
    virtual ~UIToolsItem() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Returns model reference. */
        UIToolsModel *model() const;

        /** Reconfigures icon with new @a enmClass, @a enmType, @a icon and @a strName. */
        void reconfigure(UIToolsClass enmClass, UIToolsType enmType,
                         const QIcon &icon, const QString &strName);

        /** Returns item class. */
        UIToolsClass itemClass() const;
        /** Returns item type. */
        UIToolsType itemType() const;
        /** Returns item icon. */
        const QIcon &icon() const;
        /** Returns item name. */
        const QString &name() const;

        /** Defines whether item is @a fHovered. */
        void setHovered(bool fHovered);
        /** Returns whether item is hovered. */
        bool isHovered() const;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates geometry. */
        virtual void updateGeometry() /* override */;

        /** Returns minimum width-hint. */
        int minimumWidthHint() const;
        /** Returns minimum height-hint. */
        int minimumHeightHint() const;

        /** Returns size-hint.
          * @param  enmWhich    Brings size-hint type.
          * @param  constraint  Brings size constraint. */
        virtual QSizeF sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint = QSizeF()) const /* override */;
    /** @} */

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

        /** Handles hover enter @a event. */
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;
        /** Handles hover leave @a event. */
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;

        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;
    /** @} */

private slots:

    /** @name Item stuff.
      * @{ */
        /** Handles top-level window remaps. */
        void sltHandleWindowRemapped();
    /** @} */

private:

    /** Data field types. */
    enum ToolsItemData
    {
        /* Layout hints: */
        ToolsItemData_Margin,
        ToolsItemData_Spacing,
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares hover animation. */
        void prepareHoverAnimation();
        /** Prepares connections. */
        void prepareConnections();

        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;

        /** Defines item's default animation @a iValue. */
        void setDefaultValue(int iValue) { m_iDefaultValue = iValue; update(); }
        /** Returns item's default animation value. */
        int defaultValue() const { return m_iDefaultValue; }

        /** Defines item's hovered animation @a iValue. */
        void setHoveredValue(int iValue) { m_iHoveredValue = iValue; update(); }
        /** Returns item's hovered animation value. */
        int hoveredValue() const { return m_iHoveredValue; }

        /** Defines item's animated @a iValue. */
        void setAnimatedValue(int iValue) { m_iAnimatedValue = iValue; update(); }
        /** Returns item's animated value. */
        int animatedValue() const { return m_iAnimatedValue; }
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Defines previous @a geometry. */
        void setPreviousGeometry(const QRectF &geometry) { m_previousGeometry = geometry; }
        /** Returns previous geometry. */
        const QRectF &previousGeometry() const { return m_previousGeometry; }

        /** Updates pixmap. */
        void updatePixmap();
        /** Updates minimum name size. */
        void updateMinimumNameSize();
        /** Updates maximum name width. */
        void updateMaximumNameWidth();
        /** Updates visible name. */
        void updateVisibleName();

        /** Returns monospace text width of line containing @a iCount of chars calculated on the basis of certain @a font and @a pPaintDevice. */
        static int textWidthMonospace(const QFont &font, QPaintDevice *pPaintDevice, int iCount);
        /** Compresses @a strText to @a iWidth on the basis of certain @a font and @a pPaintDevice. */
        static QString compressText(const QFont &font, QPaintDevice *pPaintDevice, QString strText, int iWidth);
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter.
          * @param  rectangle  Brings the rectangle to fill with background. */
        void paintBackground(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints frame using using passed @a pPainter.
          * @param  rectangle  Brings the rectangle to stroke with frame. */
        void paintFrame(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints tool info using using passed @a pPainter.
          * @param  rectangle  Brings the rectangle to limit painting with. */
        void paintToolInfo(QPainter *pPainter, const QRect &rectangle) const;

        /** Paints @a pixmap using passed @a pPainter.
          * @param  pOptions  Brings the options set with painting data. */
        static void paintPixmap(QPainter *pPainter, const QPoint &point, const QPixmap &pixmap);

        /** Paints @a strText using passed @a pPainter.
          * @param  point         Brings upper-left corner pixmap should be mapped to.
          * @param  font          Brings the text font.
          * @param  pPaintDevice  Brings the paint-device reference to initilize painting from. */
        static void paintText(QPainter *pPainter, QPoint point,
                              const QFont &font, QPaintDevice *pPaintDevice,
                              const QString &strText);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds the item parent. */
        QGraphicsScene *m_pScene;
        /** Holds the item class. */
        UIToolsClass    m_enmClass;
        /** Holds the item type. */
        UIToolsType     m_enmType;
        /** Holds the item icon. */
        QIcon           m_icon;
        /** Holds the item name. */
        QString         m_strName;

        /** Holds the item pixmap. */
        QPixmap  m_pixmap;
        /** Holds the item visible name. */
        QString  m_strVisibleName;

        /** Holds name font. */
        QFont  m_nameFont;

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

        /** Holds minimum hover lightness tone. */
        int  m_iHoverLightnessMin;
        /** Holds maximum hover lightness tone. */
        int  m_iHoverLightnessMax;
        /** Holds minimum highlight lightness tone. */
        int  m_iHighlightLightnessMin;
        /** Holds maximum highlight lightness tone. */
        int  m_iHighlightLightnessMax;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds previous geometry. */
        QRectF  m_previousGeometry;

        /** Holds previous minimum width hint. */
        int  m_iPreviousMinimumWidthHint;
        /** Holds previous minimum height hint. */
        int  m_iPreviousMinimumHeightHint;

        /** Holds the pixmap size. */
        QSize  m_pixmapSize;
        /** Holds minimum name size. */
        QSize  m_minimumNameSize;

        /** Holds maximum name width. */
        int  m_iMaximumNameWidth;
    /** @} */
};

#endif /* !___UIToolsItem_h___ */
