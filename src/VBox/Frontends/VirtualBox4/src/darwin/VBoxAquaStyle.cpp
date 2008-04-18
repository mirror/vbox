/** $Id: $ */
/** @file
 * Qt GUI - VBox Variation on the QAquaStyle.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxDefs.h"
#include "VBoxAquaStyle.h"
#include <qapplication.h>
#include <qtoolbutton.h>
#include <qpushbutton.h>
#include <qpainter.h>
#include <qpixmap.h>


VBoxAquaStyle *VBoxAquaStyle::sInstance;

/**
 * Create a VBoxAquaStyle wrapper overloading the specified QAquaStyle instance.
 * @param   parent  QAquaStyle instance.
 */
VBoxAquaStyle::VBoxAquaStyle(QStyle *parent)
    : QStyle(),
      mparent(parent)
{
}

/**
 * Create a VBoxAquaStyle wrapper overloading the specified QAquaStyle instance.
 * @param   parent  QAquaStyle instance.
 */
VBoxAquaStyle::VBoxAquaStyle(QStyle &parent)
    : QStyle(),
      mparent(&parent)
{
}

/**
 * The main purpose here is to make sure the global 
 * instance doesn't die on us.
 */
VBoxAquaStyle::~VBoxAquaStyle()
{
    //fprintf(stderr, "~VBoxAquaStyle\n");
    if (this == sInstance)
        sInstance = NULL;
    mparent = NULL;
}

/**
 * Get the global VBoxAquaStyle instance.
 */
/* static */VBoxAquaStyle &VBoxAquaStyle::VBoxAquaStyle::instance()
{
    if (!sInstance)
        sInstance = new VBoxAquaStyle(QApplication::style());
    return *sInstance;
}

// The QStyle bits we modify the behaviour of.

void VBoxAquaStyle::drawComplexControl( ComplexControl control, QPainter *p, const QWidget *widget,
                                        const QRect &r, const QColorGroup &cg, SFlags how/* = Style_Default*/,
                                        SCFlags sub/* = (uint)SC_All*/, SCFlags subActive/* = SC_None*/,
                                        const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "drawComplexControl %p %x", widget, control);
    if ( control == CC_ToolButton )
    {
        /*
         * This is for the ugly tool bar buttons.
         * We just drop the frame unless they are pressed down.
         */
        QToolButton *toolbutton = (QToolButton *)widget;
        //if (toolbutton->isDown()) fprintf(stderr, " down");
        if (!toolbutton->isDown())
        {
            if (toolbutton->hasFocus() && !toolbutton->focusProxy())
            {
                QRect fr = toolbutton->rect();
                fr.addCoords(3, 3, -3, -3);
                drawPrimitive(PE_FocusRect, p, fr, cg);
            }
            //fprintf(stderr, "\n");
            return;
        }
    }

    /* fallback */
    mparent->drawComplexControl( control, p, widget, r, cg, how, sub, subActive, foo );
    //fprintf(stderr, "\n");
}


// The QStyle proxying

void VBoxAquaStyle::polish( QWidget *w )
{
    //fprintf(stderr, "polish %p\n", w);
    mparent->polish( w );
}

void VBoxAquaStyle::unPolish( QWidget *w )
{
    //fprintf(stderr, "unPolish %p\n", w);
    mparent->unPolish( w );
}

void VBoxAquaStyle::polish( QApplication *app )
{
    mparent->polish( app );
}

void VBoxAquaStyle::unPolish( QApplication *app )
{
    mparent->unPolish( app );
}

void VBoxAquaStyle::polish( QPalette &p )
{
    mparent->polish( p );
}

void VBoxAquaStyle::polishPopupMenu( QPopupMenu *m )
{
    mparent->polishPopupMenu( m );
}

QRect VBoxAquaStyle::itemRect( QPainter *p, const QRect &r, int flags, bool enabled,
                                       const QPixmap *pixmap, const QString &text, int len/* = -1*/ ) const
{
    return mparent->itemRect( p, r, flags, enabled, pixmap, text, len );
}

void VBoxAquaStyle::drawItem( QPainter *p, const QRect &r, int flags, const QColorGroup &g, bool enabled,
                              const QPixmap *pixmap, const QString &text, int len/* = -1*/, const QColor *penColor/* = 0*/ ) const
{
    //fprintf(stderr, "drawItem %p\n", pixmap);
    mparent->drawItem( p, r, flags, g, enabled, pixmap, text, len, penColor );
}

void VBoxAquaStyle::drawPrimitive( PrimitiveElement pe, QPainter *p, const QRect &r, const QColorGroup &cg, 
                                   SFlags flags/* = Style_Default*/, const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "drawPrimitive %x\n", pe);
    mparent->drawPrimitive( pe, p, r, cg, flags, foo);
}

void VBoxAquaStyle::drawControl( ControlElement element, QPainter *p, const QWidget *widget, const QRect &r,
                                 const QColorGroup &cg, SFlags how/* = Style_Default*/, const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "drawControl %p %x\n", widget, element);
    mparent->drawControl( element, p, widget, r, cg, how, foo );
}

void VBoxAquaStyle::drawControlMask( ControlElement element, QPainter *p, const QWidget *widget,
                                     const QRect &r, const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "drawControlMask %p %x\n", widget, element);
    mparent->drawControlMask( element, p, widget, r, foo);
}

QRect VBoxAquaStyle::subRect( SubRect r, const QWidget *widget ) const
{
    //fprintf(stderr, "subRect %p", widget);
    QRect rct = mparent->subRect( r, widget );
    //fprintf(stderr, " rct(%d,%d,%d,%d)\n", rct.topLeft().x(), rct.topLeft().y(), rct.bottomRight().x(),rct.bottomRight().y());
    return rct;
}

int VBoxAquaStyle::pixelMetric( PixelMetric metric, const QWidget *widget/* = 0*/ ) const
{
    return mparent->pixelMetric( metric, widget );
}

QSize VBoxAquaStyle::sizeFromContents( ContentsType contents, const QWidget *widget, const QSize &contentsSize,
                                       const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    return mparent->sizeFromContents( contents, widget, contentsSize, foo );
}

int VBoxAquaStyle::styleHint( StyleHint stylehint, const QWidget *widget/* = 0*/, 
                              const QStyleOption &foo/* = QStyleOption::Default*/,
                              QStyleHintReturn* returnData/* = 0*/) const
{
    return mparent->styleHint( stylehint, widget, foo, returnData );
}

QPixmap VBoxAquaStyle::stylePixmap( StylePixmap stylepixmap, const QWidget *widget/* = 0*/,
                                    const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    return mparent->stylePixmap( stylepixmap, widget, foo );
}


void VBoxAquaStyle::drawComplexControlMask( ComplexControl control, QPainter *p, const QWidget *widget, const QRect &r,
                                            const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "drawComplexControl %p %x\n", widget, control);
    mparent->drawComplexControlMask( control, p, widget, r, foo );
}

QRect VBoxAquaStyle::querySubControlMetrics( ComplexControl control, const QWidget *widget,
                                             SubControl sc, const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "querySubControlMetrics %p %x\n", widget, control);
    return mparent->querySubControlMetrics( control, widget, sc, foo );
}

QStyle::SubControl VBoxAquaStyle::querySubControl( ComplexControl control, const QWidget *widget, const QPoint &pos,
                                           const QStyleOption &foo/* = QStyleOption::Default*/ ) const
{
    //fprintf(stderr, "querySubControl %p %x\n", widget, control);
    return mparent->querySubControl( control, widget, pos, foo );
}


