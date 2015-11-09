/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationItem class definition.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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
# include <QApplication>
# include <QPainter>
# include <QGraphicsScene>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UIGInformationGroup.h"
# include "UIGInformationSet.h"
# include "UIGInformationElement.h"
# include "UIGInformationModel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGInformationItem::UIGInformationItem(UIGInformationItem *pParent)
    : QIWithRetranslateUI4<QIGraphicsWidget>(pParent)
    , m_pParent(pParent)
{
    /* Basic item setup: */
    setOwnedByLayout(false);
    setAcceptDrops(false);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    /* Non-root item? */
    if (parentItem())
    {
        /* Non-root item setup: */
        setAcceptHoverEvents(true);
    }

    /* Setup connections: */
    connect(this, SIGNAL(sigBuildStep(QString, int)),
            this, SLOT(sltBuildStep(QString, int)), Qt::QueuedConnection);
}

UIGInformationGroup* UIGInformationItem::toGroup()
{
    UIGInformationGroup *pItem = qgraphicsitem_cast<UIGInformationGroup*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGInformationGroup!"));
    return pItem;
}

UIGInformationSet* UIGInformationItem::toSet()
{
    UIGInformationSet *pItem = qgraphicsitem_cast<UIGInformationSet*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGInformationSet!"));
    return pItem;
}

UIGInformationElement* UIGInformationItem::toElement()
{
    UIGInformationElement *pItem = qgraphicsitem_cast<UIGInformationElement*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGInformationElement!"));
    return pItem;
}

UIGInformationModel* UIGInformationItem::model() const
{
    UIGInformationModel *pModel = qobject_cast<UIGInformationModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

UIGInformationItem* UIGInformationItem::parentItem() const
{
    return m_pParent;
}

void UIGInformationItem::updateGeometry()
{
    /* Call to base-class: */
    QIGraphicsWidget::updateGeometry();

    /* Do the same for the parent: */
    if (parentItem())
        parentItem()->updateGeometry();
}

QSizeF UIGInformationItem::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize or Qt::PreferredSize requested: */
    if (which == Qt::MinimumSize || which == Qt::PreferredSize)
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    /* Call to base-class: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGInformationItem::sltBuildStep(QString, int)
{
    AssertMsgFailed(("This item doesn't support building!"));
}

/* static */
void UIGInformationItem::configurePainterShape(QPainter *pPainter,
                                           const QStyleOptionGraphicsItem *pOption,
                                           int iRadius)
{
    /* Rounded corners? */
    if (iRadius)
    {
        /* Setup clipping: */
        QPainterPath roundedPath;
        roundedPath.addRoundedRect(pOption->rect, iRadius, iRadius);
        pPainter->setRenderHint(QPainter::Antialiasing);
        pPainter->setClipPath(roundedPath);
    }
}

/* static */
void UIGInformationItem::paintFrameRect(QPainter *pPainter, const QRect &rect, int iRadius)
{
    pPainter->save();
    QPalette pal = QApplication::palette();
    QColor base = pal.color(QPalette::Active, QPalette::Window);
    pPainter->setPen(base.darker(160));
    if (iRadius)
        pPainter->drawRoundedRect(rect, iRadius, iRadius);
    else
        pPainter->drawRect(rect);
    pPainter->restore();
}

/* static */
void UIGInformationItem::paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap)
{
    pPainter->drawPixmap(rect, pixmap);
}

/* static */
void UIGInformationItem::paintText(QPainter *pPainter, QPoint point,
                               const QFont &font, QPaintDevice *pPaintDevice,
                               const QString &strText, const QColor &color)
{
    /* Prepare variables: */
    QFontMetrics fm(font, pPaintDevice);
    point += QPoint(0, fm.ascent());

    /* Draw text: */
    pPainter->save();
    pPainter->setFont(font);
    pPainter->setPen(color);
    pPainter->drawText(point, strText);
    pPainter->restore();
}

UIInformationBuildStep::UIInformationBuildStep(QObject *pParent, QObject *pBuildObject, const QString &strStepId, int iStepNumber)
    : QObject(pParent)
    , m_strStepId(strStepId)
    , m_iStepNumber(iStepNumber)
{
    /* Prepare connections: */
    connect(pBuildObject, SIGNAL(sigBuildDone()), this, SLOT(sltStepDone()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigStepDone(QString, int)), pParent, SLOT(sltBuildStep(QString, int)), Qt::QueuedConnection);
}

void UIInformationBuildStep::sltStepDone()
{
    emit sigStepDone(m_strStepId, m_iStepNumber);
}

