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

signals:

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

private slots:

    /** Handles button 1 click signal. */
    void sltButton1Clicked();
    /** Handles button 2 click signal. */
    void sltButton2Clicked();

    /** Handles token being moved to cpecified @a pos. */
    void sltTokenMoved(const QPointF &pos);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();

    /** Updates scroll-bar extent value. */
    void updateExtent();
    /** Layout widgets. */
    void layoutWidgets();
    /** Layout buttons. */
    void layoutButtons();
    /** Layout token. */
    void layoutToken();

    /** Paints background using specified @a pPainter and certain @a rectangle. */
    void paintBackground(QPainter *pPainter, const QRect &rectangle) const;

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
};

#endif /* !FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollBar_h */
