/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * QIListView, QIItemDelegate class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "QIListView.h"
#include "VBoxDefs.h"

#if MAC_LEOPARD_STYLE
/* Qt includes */
# include <QPainter>
# include <QApplication>
# include <qmacstyle_mac.h>
#endif /* MAC_LEOPARD_STYLE */


/* VBoxVMListView class */

QIListView::QIListView (QWidget *aParent /* = 0 */)
    :QListView (aParent)
{
    /* Track if the application lost the focus */
#if MAC_LEOPARD_STYLE
    connect (QCoreApplication::instance(), SIGNAL (focusChanged (QWidget *, QWidget *)),
             this, SLOT (focusChanged (QWidget *, QWidget *)));
    /* 1 pixel line frame */
    setMidLineWidth (1);
    setLineWidth (0);
    setFrameShape (QFrame::Box);
    focusChanged (NULL, this);
    /* Nesty hack to disable the focus rect on the list view. This interface
     * may change at any time! */
    static_cast<QMacStyle *> (style())->setFocusRectPolicy (this, QMacStyle::FocusDisabled);
#endif /* MAC_LEOPARD_STYLE */
}

void QIListView::focusChanged (QWidget * /* aOld */, QWidget *aNow)
{
#if MAC_LEOPARD_STYLE
    QColor bgColor (212, 221, 229);
    if (aNow == NULL)
        bgColor.setRgb (232, 232, 232);
    QPalette pal = viewport()->palette();
    pal.setColor (QPalette::Base, bgColor);
    viewport()->setPalette (pal);
    viewport()->setAutoFillBackground (true);
#else /* MAC_LEOPARD_STYLE */
    Q_UNUSED (aNow);
#endif /* MAC_LEOPARD_STYLE */
}

/* VBoxVMItemPainter class */

void QIItemDelegate::drawBackground (QPainter *aPainter, const QStyleOptionViewItem &aOption, 
                                        const QModelIndex &aIndex) const
{   
#if MAC_LEOPARD_STYLE
    NOREF (aIndex);
    /* Macify for Leopard */
    if (aOption.state & QStyle::State_Selected)
    {   
        /* Standard color for selected items and focus on the widget */
        QColor topLineColor (69, 128, 200);
        QColor topGradColor (92, 147, 214);
        QColor bottomGradColor (21, 83, 169);
        /* Color for selected items and no focus on the widget */
        if (QWidget *p = qobject_cast<QWidget *> (parent()))
            if (!p->hasFocus())
            {
                topLineColor.setRgb (145, 160, 192);
                topGradColor.setRgb (162, 177, 207);
                bottomGradColor.setRgb (110, 129, 169);
            }
        /* Color for selected items and no focus on the application at all */
        if (qApp->focusWidget() == NULL)
            {
                topLineColor.setRgb (151, 151, 151);
                topGradColor.setRgb (180, 180, 180);
                bottomGradColor.setRgb (137, 137, 137);
            }
        /* Paint the background */
        QRect r = aOption.rect;
        r.setTop (r.top() + 1);
        QLinearGradient linearGrad (QPointF(0, r.top()), QPointF(0, r.bottom()));
        linearGrad.setColorAt (0, topGradColor);
        linearGrad.setColorAt (1, bottomGradColor);
        aPainter->setPen (topLineColor);
        aPainter->drawLine (r.left(), r.top() - 1, r.right(), r.top() - 1);
        aPainter->fillRect (r, linearGrad);
    }
#else /* MAC_LEOPARD_STYLE */
    QItemDelegate::drawBackground (aPainter, aOption, aIndex);
#endif /* MAC_LEOPARD_STYLE */
}

