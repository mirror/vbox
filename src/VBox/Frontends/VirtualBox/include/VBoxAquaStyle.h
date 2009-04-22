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


#ifndef ___VBoxAquaStyle_h___
#define ___VBoxAquaStyle_h___

#include <qstyle.h>
//Added by qt3to4:
#include <QPixmap>
#include <Q3PopupMenu>

/**
 * The VirtualBox QAquaStyle overrider.
 *
 * The purpose of this class is to make some small adjustments to
 * the QAquaStyle so it looks and works better.
 *
 * Since the QAquaStyle class isn't exported we have to do all the
 * virtual method work our selves. This also means we doesn't have
 * access to any protected or QAquaStyle methods, too bad.
 */
class VBoxAquaStyle : public QStyle //QAquaStyle - not exported, sigh.
{
public:
    VBoxAquaStyle(QStyle *parent);
    VBoxAquaStyle(QStyle &parent);
    virtual ~VBoxAquaStyle();
    static VBoxAquaStyle &instance();

    // The QStyle implementation.
    virtual void polish( QWidget *w );
    virtual void unPolish( QWidget *w );
    virtual void polish( QApplication *app );
    virtual void unPolish( QApplication *app );
    virtual void polish( QPalette &p );
    virtual void polishPopupMenu( Q3PopupMenu *m );
    virtual QRect itemRect( QPainter *p, const QRect &r, int flags, bool enabled, const QPixmap *pixmap,
                            const QString &text, int len = -1 ) const;
    virtual void drawItem( QPainter *p, const QRect &r, int flags, const QColorGroup &g, bool enabled,
                           const QPixmap *pixmap, const QString &text, int len = -1, const QColor *penColor = 0 ) const;
    virtual void drawPrimitive( PrimitiveElement pe, QPainter *p, const QRect &r, const QColorGroup &cg,
                                SFlags flags = Style_Default, const QStyleOption &foo = QStyleOption::Default ) const;
    virtual void drawControl( ControlElement element, QPainter *p, const QWidget *widget, const QRect &r, const QColorGroup &cg,
                              SFlags how = Style_Default, const QStyleOption &foo = QStyleOption::Default ) const;
    virtual void drawControlMask( ControlElement element, QPainter *p, const QWidget *widget, const QRect &r,
                                  const QStyleOption &foo = QStyleOption::Default ) const;
    virtual QRect subRect( SubRect r, const QWidget *widget ) const;
    virtual int pixelMetric( PixelMetric metric, const QWidget *widget = 0 ) const;
    virtual QSize sizeFromContents( ContentsType contents, const QWidget *widget, const QSize &contentsSize,
                                    const QStyleOption &foo = QStyleOption::Default ) const;
    virtual int styleHint( StyleHint stylehint, const QWidget *widget = 0, const QStyleOption &foo = QStyleOption::Default,
                           QStyleHintReturn* returnData = 0 ) const;
    virtual QPixmap stylePixmap( StylePixmap stylepixmap, const QWidget *widget = 0, const QStyleOption &foo = QStyleOption::Default ) const;
    virtual void drawComplexControl( ComplexControl control, QPainter *p, const QWidget *widget, const QRect &r,
                                     const QColorGroup &cg, SFlags how = Style_Default, SCFlags sub = (uint)SC_All,
                                     SCFlags subActive = SC_None, const QStyleOption &foo = QStyleOption::Default ) const;
    virtual void drawComplexControlMask( ComplexControl control, QPainter *p, const QWidget *widget, const QRect &r,
                                         const QStyleOption &foo = QStyleOption::Default ) const;
    virtual QRect querySubControlMetrics( ComplexControl control, const QWidget *widget, SubControl sc,
                                          const QStyleOption &foo = QStyleOption::Default ) const;
    virtual SubControl querySubControl( ComplexControl control, const QWidget *widget, const QPoint &pos,
                                        const QStyleOption &foo = QStyleOption::Default ) const;

protected:
    /** The style we're overloading. */
    QStyle *mparent;

    /** The global instance. */
    static VBoxAquaStyle *sInstance;
};

#endif

