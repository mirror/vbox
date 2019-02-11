/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsScrollBar class declaration.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollBar_h
#define FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollBar_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declarations: */
class QGraphicsScene;
class UIGraphicsButton;
class UIGraphicsScrollBarToken;

/** QIGraphicsWidget subclass providing GUI with graphics scroll-bar. */
class UIGraphicsScrollBar : public QIGraphicsWidget
{
    Q_OBJECT;
    Q_PROPERTY(int animatedValue READ animatedValue WRITE setAnimatedValue);

signals:

    /** Notifies listeners about hover enter. */
    void sigHoverEnter();
    /** Notifies listeners about hover leave. */
    void sigHoverLeave();

    /** Notifies listeners about @a iValue has changed. */
    void sigValueChanged(int iValue);

public:

    /** Constructs graphics scroll-bar of requested @a enmOrientation, embedding it directly to passed @a pScene. */
    UIGraphicsScrollBar(Qt::Orientation enmOrientation, QGraphicsScene *pScene);

    /** Constructs graphics scroll-bar of requested @a enmOrientation passing @a pParent to the base-class. */
    UIGraphicsScrollBar(Qt::Orientation enmOrientation, QIGraphicsWidget *pParent = 0);

    /** Returns minimum size-hint. */
    virtual QSizeF minimumSizeHint() const /* override */;

    /** Defines scrolling @a iStep. */
    void setStep(int iStep);
    /** Returns scrolling step. */
    int step() const;

    /** Defines @a iMinimum scroll-bar value. */
    void setMinimum(int iMinimum);
    /** Returns minimum scroll-bar value. */
    int minimum() const;

    /** Defines @a iMaximum scroll-bar value. */
    void setMaximum(int iMaximum);
    /** Returns minimum scroll-bar value. */
    int maximum() const;

    /** Defines current scroll-bar @a iValue. */
    void setValue(int iValue);
    /** Returns current scroll-bar value. */
    int value() const;

protected:

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

    /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
    virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;

    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

    /** Handles hover enter @a pEvent. */
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;
    /** Handles hover leave @a pEvent. */
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;

    /** Handles timer @a pEvent. */
    virtual void timerEvent(QTimerEvent *pEvent) /* override */;

private slots:

    /** Handles button 1 click signal. */
    void sltButton1Clicked();
    /** Handles button 2 click signal. */
    void sltButton2Clicked();

    /** Handles token being moved to cpecified @a pos. */
    void sltTokenMoved(const QPointF &pos);

    /** Handles default state leaving. */
    void sltStateLeftDefault();
    /** Handles hovered state leaving. */
    void sltStateLeftHovered();
    /** Handles default state entering. */
    void sltStateEnteredDefault();
    /** Handles hovered state entering. */
    void sltStateEnteredHovered();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares buttons. */
    void prepareButtons();
    /** Prepares token. */
    void prepareToken();
    /** Prepares animation. */
    void prepareAnimation();

    /** Updates scroll-bar extent value. */
    void updateExtent();
    /** Layout widgets. */
    void layoutWidgets();
    /** Layout buttons. */
    void layoutButtons();
    /** Layout token. */
    void layoutToken();

    /** Returns actual token position. */
    QPoint actualTokenPosition() const;

    /** Paints background using specified @a pPainter and certain @a rectangle. */
    void paintBackground(QPainter *pPainter, const QRect &rectangle) const;

    /** Defines item's animated @a iValue. */
    void setAnimatedValue(int iValue) { m_iAnimatedValue = iValue; update(); }
    /** Returns item's animated value. */
    int animatedValue() const { return m_iAnimatedValue; }

    /** Holds the orientation. */
    const Qt::Orientation  m_enmOrientation;

    /** Holds the scroll-bar extent. */
    int  m_iExtent;

    /** Holds the scrolling step. */
    int  m_iStep;

    /** Holds the minimum scroll-bar value. */
    int  m_iMinimum;
    /** Holds the maximum scroll-bar value. */
    int  m_iMaximum;
    /** Holds the current scroll-bar value. */
    int  m_iValue;

    /** Holds the 1st arrow button instance. */
    UIGraphicsButton         *m_pButton1;
    /** Holds the 2nd arrow button instance. */
    UIGraphicsButton         *m_pButton2;
    /** Holds the scroll-bar token instance. */
    UIGraphicsScrollBarToken *m_pToken;

    /** Holds whether item is hovered. */
    bool  m_fHovered;
    /** Holds the hover-on timer id. */
    int   m_iHoverOnTimerId;
    /** Holds the hover-off timer id. */
    int   m_iHoverOffTimerId;
    /** Holds the animated value. */
    int   m_iAnimatedValue;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollBar_h */
