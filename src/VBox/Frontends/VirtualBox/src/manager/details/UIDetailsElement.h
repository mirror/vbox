/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement class declaration.
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

#ifndef ___UIDetailsElement_h___
#define ___UIDetailsElement_h___

/* Qt includes: */
#include <QIcon>

/* GUI includes: */
#include "UIDetailsItem.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QPropertyAnimation;
class QStateMachine;
class QTextLayout;
class UIDetailsSet;
class UIGraphicsRotatorButton;
class UIGraphicsTextPane;
class UITextTableLine;
class CMachine;

/* Typedefs: */
typedef QList<UITextTableLine> UITextTable;

/** UIDetailsItem extension implementing element item. */
class UIDetailsElement : public UIDetailsItem
{
    Q_OBJECT;
    Q_PROPERTY(int animatedValue READ animatedValue WRITE setAnimatedValue);
    Q_PROPERTY(int additionalHeight READ additionalHeight WRITE setAdditionalHeight);

signals:

    /** @name Item stuff.
      * @{ */
        /** Notifies about hover enter. */
        void sigHoverEnter();
        /** Notifies about hover leave. */
        void sigHoverLeave();

        /** Notifies about @a enmType element @a fToggled. */
        void sigToggleElement(DetailsElementType enmType, bool fToggled);
        /** Notifies about element toggle finished. */
        void sigToggleElementFinished();

        /** Notifies about element link clicked.
          * @param  strCategory  Brings the link category.
          * @param  strControl   Brings the wanted settings control.
          * @param  strId        Brings the ID. */
        void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);
    /** @} */

public:

    /** RTTI item type. */
    enum { Type = UIDetailsItemType_Element };

    /** Constructs element item, passing pParent to the base-class.
      * @param  enmType  Brings element type.
      * @param  fOpened  Brings whether element is opened. */
    UIDetailsElement(UIDetailsSet *pParent, DetailsElementType enmType, bool fOpened);
    /** Destructs element item. */
    virtual ~UIDetailsElement() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Returns element type. */
        DetailsElementType elementType() const { return m_enmType; }

        /** Defines the @a text table as the passed one. */
        void setText(const UITextTable &text);
        /** Returns the reference to the text table. */
        UITextTable &text() const;

        /** Closes group in @a fAnimated way if requested. */
        void close(bool fAnimated = true);
        /** Returns whether group is closed. */
        bool isClosed() const { return m_fClosed; }

        /** Opens group in @a fAnimated way if requested. */
        void open(bool fAnimated = true);
        /** Returns whether group is opened. */
        bool isOpened() const { return !m_fClosed; }

        /** Marks animation finished. */
        void markAnimationFinished();

        /** Updates element appearance. */
        virtual void updateAppearance();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const /* override */;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const /* override */;
    /** @} */

protected:

    /** Data field types. */
    enum ElementData
    {
        /* Hints: */
        ElementData_Margin,
        ElementData_Spacing
    };

    /** @name Event-handling stuff.
      * @{ */
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;

        /** This event handler is delivered after the widget has been resized. */
        virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

        /** Handles hover enter @a event. */
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;
        /** Handles hover leave @a event. */
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;

        /** Handles mouse press @a event. */
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;
        /** Handles mouse double-click @a event. */
        virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns RTTI item type. */
        virtual int type() const /* override */ { return Type; }

        /** Returns the description of the item. */
        virtual QString description() const /* override */;

        /** Returns cached machine reference. */
        const CMachine &machine();

        /** Defines element @a strName. */
        void setName(const QString &strName);

        /** Defines @a iAdditionalHeight during toggle animation. */
        void setAdditionalHeight(int iAdditionalHeight);
        /** Returns additional height during toggle animation. */
        int additionalHeight() const { return m_iAdditionalHeight; }
        /** Returns toggle button instance. */
        UIGraphicsRotatorButton *button() const { return m_pButton; }
        /** Returns whether toggle animation is running. */
        bool isAnimationRunning() const { return m_fAnimationRunning; }

        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds child @a pItem. */
        virtual void addItem(UIDetailsItem *pItem) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIDetailsItem *pItem) /* override */;

        /** Returns children items of certain @a enmType. */
        virtual QList<UIDetailsItem*> items(UIDetailsItemType enmType) const /* override */;
        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIDetailsItemType enmType) const /* override */;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIDetailsItemType enmType) /* override */;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        virtual void updateLayout() /* override */;

        /** Returns minimum width-hint for @a fClosed element. */
        virtual int minimumHeightHintForElement(bool fClosed) const;

        /** Returns minimum header width. */
        int minimumHeaderWidth() const { return m_iMinimumHeaderWidth; }
        /** Returns minimum header height. */
        int minimumHeaderHeight() const { return m_iMinimumHeaderHeight; }
    /** @} */

private slots:

    /** @name Item stuff.
      * @{ */
        /** Handles top-level window remaps. */
        void sltHandleWindowRemapped();

        /** Handles toggle button click. */
        void sltToggleButtonClicked();
        /** Handles toggle start. */
        void sltElementToggleStart();
        /** Handles toggle finish. */
        void sltElementToggleFinish(bool fToggled);

        /** Handles children anchor clicks. */
        void sltHandleAnchorClicked(const QString &strAnchor);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Handles children geometry changes. */
        void sltUpdateGeometry() { updateGeometry(); }
    /** @} */

    /** @name Move to sub-class.
      * @{ */
        /** Handles mount storage medium requests. */
        void sltMountStorageMedium();
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares element. */
        void prepareElement();
        /** Prepares toggle button. */
        void prepareButton();
        /** Prepares text pane. */
        void prepareTextPane();
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Updates icon. */
        void updateIcon();

        /** Returns header darkness. */
        int headerDarkness() const { return m_iHeaderDarkness; }

        /** Defines animated @a iValue. */
        void setAnimatedValue(int iValue) { m_iAnimatedValue = iValue; update(); }
        /** Returns animated value. */
        int animatedValue() const { return m_iAnimatedValue; }

        /** Handles any kind of hover @a pEvent. */
        void handleHoverEvent(QGraphicsSceneHoverEvent *pEvent);
        /** Updates hovered link. */
        void updateNameHoverLink();

        /** Updates animation parameters. */
        void updateAnimationParameters();
        /** Updates toggle button visibility.  */
        void updateButtonVisibility();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates minimum header width. */
        void updateMinimumHeaderWidth();
        /** Updates minimum header height. */
        void updateMinimumHeaderHeight();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints decorations using specified @a pPainter and certain @a pOptions. */
        void paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions);
        /** Paints element info using specified @a pPainter and certain @a pOptions. */
        void paintElementInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions);
        /** Paints background using specified @a pPainter and certain @a pOptions. */
        void paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds the parent reference. */
        UIDetailsSet       *m_pSet;
        /** Holds the element type. */
        DetailsElementType  m_enmType;

        /** Holds the element pixmap. */
        QPixmap  m_pixmap;
        /** Holds the element name. */
        QString  m_strName;

        /** Holds the corner radius. */
        int  m_iCornerRadius;
        /** Holds the header darkness. */
        int  m_iHeaderDarkness;

        /** Holds the name font. */
        QFont  m_nameFont;
        /** Holds the text font. */
        QFont  m_textFont;

        /** Holds whether element is hovered. */
        bool                m_fHovered;
        /** Holds whether element name is hovered. */
        bool                m_fNameHovered;
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

        /** Holds the toggle button instance. */
        UIGraphicsRotatorButton *m_pButton;
        /** Holds whether element is closed. */
        bool  m_fClosed;
        /** Holds whether animation is running. */
        bool  m_fAnimationRunning;
        /** Holds the additional height. */
        int   m_iAdditionalHeight;

        /** Holds the graphics text pane instance. */
        UIGraphicsTextPane *m_pTextPane;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds the pixmap size. */
        QSize  m_pixmapSize;
        /** Holds the name size. */
        QSize  m_nameSize;
        /** Holds the button size. */
        QSize  m_buttonSize;

        /** Holds minimum header width. */
        int  m_iMinimumHeaderWidth;
        /** Holds minimum header height. */
        int  m_iMinimumHeaderHeight;
    /** @} */
};

#endif /* !___UIDetailsElement_h___ */
