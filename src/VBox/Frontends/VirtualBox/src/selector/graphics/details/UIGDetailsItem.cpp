/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsItem class definition
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QPainter>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>

/* GUI includes: */
#include "UIGDetailsGroup.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsElement.h"
#include "UIGDetailsModel.h"

UIGDetailsItem::UIGDetailsItem(UIGDetailsItem *pParent)
    : QIGraphicsWidget(pParent)
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

UIGDetailsGroup* UIGDetailsItem::toGroup()
{
    UIGDetailsGroup *pItem = qgraphicsitem_cast<UIGDetailsGroup*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGDetailsGroup!"));
    return pItem;
}

UIGDetailsSet* UIGDetailsItem::toSet()
{
    UIGDetailsSet *pItem = qgraphicsitem_cast<UIGDetailsSet*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGDetailsSet!"));
    return pItem;
}

UIGDetailsElement* UIGDetailsItem::toElement()
{
    UIGDetailsElement *pItem = qgraphicsitem_cast<UIGDetailsElement*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGDetailsElement!"));
    return pItem;
}

UIGDetailsModel* UIGDetailsItem::model() const
{
    UIGDetailsModel *pModel = qobject_cast<UIGDetailsModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

UIGDetailsItem* UIGDetailsItem::parentItem() const
{
    return m_pParent;
}

void UIGDetailsItem::updateGeometry()
{
    /* Call to base-class: */
    QIGraphicsWidget::updateGeometry();

    /* Do the same for the parent: */
    if (parentItem())
        parentItem()->updateGeometry();
}

QSizeF UIGDetailsItem::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize or Qt::PreferredSize requested: */
    if (which == Qt::MinimumSize || which == Qt::PreferredSize)
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    /* Call to base-class: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGDetailsItem::sltBuildStep(QString, int)
{
    AssertMsgFailed(("This item doesn't support building!"));
}

/* static */
void UIGDetailsItem::configurePainterShape(QPainter *pPainter,
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
void UIGDetailsItem::paintFrameRect(QPainter *pPainter, const QRect &rect, int iRadius)
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
void UIGDetailsItem::paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap)
{
    pPainter->drawPixmap(rect, pixmap);
}

/* static */
void UIGDetailsItem::paintText(QPainter *pPainter, const QRect &rect, const QFont &font,
                               const QString &strText, const QColor &color)
{
    pPainter->save();
    pPainter->setFont(font);
    pPainter->setPen(color);
    pPainter->drawText(rect, strText);
    pPainter->restore();
}

UIBuildStep::UIBuildStep(QObject *pParent, QObject *pBuildObject, const QString &strStepId, int iStepNumber)
    : QObject(pParent)
    , m_strStepId(strStepId)
    , m_iStepNumber(iStepNumber)
{
    /* Prepare connections: */
    connect(pBuildObject, SIGNAL(sigBuildDone()), this, SLOT(sltStepDone()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigStepDone(QString, int)), pParent, SLOT(sltBuildStep(QString, int)), Qt::QueuedConnection);
}

void UIBuildStep::sltStepDone()
{
    emit sigStepDone(m_strStepId, m_iStepNumber);
}

