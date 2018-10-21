/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserView class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAccessibleWidget>
# include <QScrollBar>

/* GUI includes: */
# include "UIChooser.h"
# include "UIChooserItem.h"
# include "UIChooserModel.h"
# include "UIChooserView.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleWidget extension used as an accessibility interface for Chooser-view. */
class UIAccessibilityInterfaceForUIChooserView : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating Chooser-view accessibility interface: */
        if (pObject && strClassname == QLatin1String("UIChooserView"))
            return new UIAccessibilityInterfaceForUIChooserView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    UIAccessibilityInterfaceForUIChooserView(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
    {}

    /** Returns the number of children. */
    virtual int childCount() const /* override */
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), 0);

        /* Return the number of children: */
        return view()->chooser()->model()->root()->items().size();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const /* override */
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), 0);
        /* Make sure index is valid: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(view()->chooser()->model()->root()->items().at(iIndex));
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), QString());

        /* Return view tool-tip: */
        Q_UNUSED(enmTextRole);
        return view()->toolTip();
    }

private:

    /** Returns corresponding Chooser-view. */
    UIChooserView *view() const { return qobject_cast<UIChooserView*>(widget()); }
};


UIChooserView::UIChooserView(UIChooser *pParent)
    : QIWithRetranslateUI<QIGraphicsView>(pParent)
    , m_pChooser(pParent)
    , m_iMinimumWidthHint(0)
    , m_iMinimumHeightHint(0)
{
    /* Prepare: */
    prepare();
}

void UIChooserView::sltFocusChanged()
{
    /* Make sure focus-item set: */
    const UIChooserItem *pFocusItem = chooser() && chooser()->model()
                                    ? chooser()->model()->focusItem()
                                    : 0;
    if (!pFocusItem)
        return;

    const QSize viewSize = viewport()->size();
    QRectF geo = pFocusItem->geometry();
    geo &= QRectF(geo.topLeft(), viewSize);
    ensureVisible(geo, 0, 0);
}

void UIChooserView::sltMinimumWidthHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumWidthHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumWidthHint = iHint;

    /* Set minimum view width according passed width-hint: */
    setMinimumWidth(2 * frameWidth() + m_iMinimumWidthHint + verticalScrollBar()->sizeHint().width());

    /* Update scene-rect: */
    updateSceneRect();
}

void UIChooserView::sltMinimumHeightHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumHeightHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumHeightHint = iHint;

    /* Update scene-rect: */
    updateSceneRect();
}

void UIChooserView::retranslateUi()
{
    /* Translate this: */
#if 0 /* we will leave that for accessibility needs. */
    setToolTip(tr("Contains a tree of Virtual Machines and their groups"));
#endif  /* to be integrated to accessibility interface. */
}

void UIChooserView::prepare()
{
    /* Install Chooser-view accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUIChooserView::pFactory);

    /* Prepare palette: */
    preparePalette();

    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Update scene-rect: */
    updateSceneRect();

    /* Apply language settings: */
    retranslateUi();
}

void UIChooserView::preparePalette()
{
    /* Setup palette: */
    QPalette pal = qApp->palette();
    const QColor bodyColor = pal.color(QPalette::Active, QPalette::Midlight).darker(110);
    pal.setColor(QPalette::Base, bodyColor);
    setPalette(pal);
}

void UIChooserView::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QIGraphicsView>::resizeEvent(pEvent);
    /* Notify listeners: */
    emit sigResized();
}

void UIChooserView::updateSceneRect()
{
    setSceneRect(0, 0, m_iMinimumWidthHint, m_iMinimumHeightHint);
}
