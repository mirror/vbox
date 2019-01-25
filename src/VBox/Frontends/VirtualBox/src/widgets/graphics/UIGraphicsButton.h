/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsButton class declaration.
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

#ifndef FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsButton_h
#define FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsButton_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declarations: */
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class QPropertyAnimation;

/* Graphics-button representation: */
class UIGraphicsButton : public QIGraphicsWidget
{
    Q_OBJECT;

signals:

    /* Notify listeners about button was clicked: */
    void sigButtonClicked();

public:

    /** Click policy. */
    enum ClickPolicy { ClickPolicy_OnRelease, ClickPolicy_OnPress };

    /* Constructor: */
    UIGraphicsButton(QIGraphicsWidget *pParent, const QIcon &icon);

    /* API: Parent stuff: */
    void setParentSelected(bool fParentSelected);

    /** Defines icon scale @a dIndex. */
    void setIconScaleIndex(double dIndex);
    /** Returns icon scale index. */
    double iconScaleIndex() const;

    /** Defines click @a enmPolicy. */
    void setClickPolicy(ClickPolicy enmPolicy);
    /** Returns click policy. */
    ClickPolicy clickPolicy() const;

protected:

    /* Data enumerator: */
    enum GraphicsButton
    {
        GraphicsButton_Margin,
        GraphicsButton_IconSize,
        GraphicsButton_Icon
    };

    /* Data provider: */
    virtual QVariant data(int iKey) const;

    /* Size hint stuff: */
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Paint stuff: */
    virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

    /* Mouse handlers: */
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *pEvent);

    /** Handles timer @a pEvent. */
    virtual void timerEvent(QTimerEvent *pEvent) /* override */;

    /* Helpers: Update stuff: */
    virtual void refresh();

private:

    /* Variables: */
    QIcon m_icon;
    bool m_fParentSelected;

    /** Holds the click policy. */
    ClickPolicy  m_enmClickPolicy;

    /** Holds the delay timer ID. */
    int  m_iDelayId;
    /** Holds the repeat timer ID. */
    int  m_iRepeatId;

    /** Holds the icon scale index. */
    double  m_dIconScaleIndex;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsButton_h */

