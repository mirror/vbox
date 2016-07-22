/* $Id$ */
/** @file
 * VBox Qt GUI - QIListView, QIItemDelegate class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QtGlobal>
# ifdef VBOX_WS_MAC
#  include <QApplication>
#  include <QPainter>
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#ifdef VBOX_WS_MAC
# if QT_VERSION < 0x050000
#  include <qmacstyle_mac.h>
# endif /* QT_VERSION < 0x050000 */
#endif /* VBOX_WS_MAC */

/* GUI includes: */
#include "QIListView.h"


QIListView::QIListView (QWidget *aParent /* = 0 */)
    :QListView (aParent)
{
#ifdef VBOX_WS_MAC
    /* Track if the application lost the focus */
    connect (QCoreApplication::instance(), SIGNAL (focusChanged (QWidget *, QWidget *)),
             this, SLOT (focusChanged (QWidget *, QWidget *)));
    /* 1 pixel line frame */
    setMidLineWidth (1);
    setLineWidth (0);
    setFrameShape (QFrame::Box);
    focusChanged (NULL, qApp->focusWidget());
# if QT_VERSION < 0x050000
    /* Nesty hack to disable the focus rect on the list view. This interface
     * may change at any time! */
    static_cast<QMacStyle *> (style())->setFocusRectPolicy (this, QMacStyle::FocusDisabled);
# endif /* QT_VERSION < 0x050000 */
#endif /* VBOX_WS_MAC */
}

void QIListView::focusChanged (QWidget * /* aOld */, QWidget *aNow)
{
#ifdef VBOX_WS_MAC
    QColor bgColor (212, 221, 229);
    if (aNow == NULL)
        bgColor.setRgb (232, 232, 232);
    QPalette pal = viewport()->palette();
    pal.setColor (QPalette::Base, bgColor);
    viewport()->setPalette (pal);
    viewport()->setAutoFillBackground(true);
#else /* !VBOX_WS_MAC */
    Q_UNUSED (aNow);
#endif /* !VBOX_WS_MAC */
}

/* QIItemDelegate class */

void QIItemDelegate::drawBackground (QPainter *aPainter, const QStyleOptionViewItem &aOption,
                                     const QModelIndex &aIndex) const
{
#ifdef VBOX_WS_MAC
    Q_UNUSED (aIndex);
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
    else
    {
        /* Color for items and no focus on the application at all */
        QColor bgColor (212, 221, 229);
        if (qApp->focusWidget() == NULL)
            bgColor.setRgb (232, 232, 232);
        aPainter->fillRect(aOption.rect, bgColor);
    }
#else /* !VBOX_WS_MAC */
    QItemDelegate::drawBackground (aPainter, aOption, aIndex);
#endif /* !VBOX_WS_MAC */
}

