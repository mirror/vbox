/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsScrollArea class declaration.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollArea_h
#define FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollArea_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declarations: */
class QGraphicsScene;
class UIGraphicsScrollBar;

/** QIGraphicsWidget subclass providing GUI with graphics scroll-area. */
class UIGraphicsScrollArea : public QIGraphicsWidget
{
    Q_OBJECT;

public:

    /** Constructs graphics scroll-area of requested @a enmOrientation, embedding it directly to passed @a pScene. */
    UIGraphicsScrollArea(Qt::Orientation enmOrientation, QGraphicsScene *pScene);

    /** Constructs graphics scroll-area of requested @a enmOrientation passing @a pParent to the base-class. */
    UIGraphicsScrollArea(Qt::Orientation enmOrientation, QIGraphicsWidget *pParent = 0);

    /** Returns minimum size-hint. */
    virtual QSizeF minimumSizeHint() const /* override */;

    /** Defines scroll-area @a pViewport. */
    void setViewport(QIGraphicsWidget *pViewport);
    /** Returns scroll-area viewport. */
    QIGraphicsWidget *viewport() const;

    /** Performs scrolling by @a iDelta pixels. */
    void scrollBy(int iDelta);

    /** Makes sure passed @a rect is visible. */
    void makeSureRectIsVisible(const QRectF &rect);

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

private slots:

    /** Handles scroll-bar @a iValue change. */
    void sltHandleScrollBarValueChange(int iValue);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares all. */
    void prepareWidgets();

    /** Layout widgets. */
    void layoutWidgets();
    /** Layout viewport. */
    void layoutViewport();
    /** Layout scroll-bar. */
    void layoutScrollBar();

    /** Holds the orientation. */
    Qt::Orientation  m_enmOrientation;
    /** Holds whether scroll-bar is in auto-hide mode. */
    bool             m_fAutoHideMode;

    /** Holds the scroll-bar instance. */
    UIGraphicsScrollBar *m_pScrollBar;
    /** Holds the viewport instance. */
    QIGraphicsWidget    *m_pViewport;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsScrollArea_h */
