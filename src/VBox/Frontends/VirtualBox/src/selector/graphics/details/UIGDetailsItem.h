/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsItem class declaration
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

#ifndef __UIGDetailsItem_h__
#define __UIGDetailsItem_h__

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declaration: */
class UIGDetailsModel;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class UIGDetailsGroup;
class UIGDetailsSet;
class UIGDetailsElement;

/* UIGDetailsItem types: */
enum UIGDetailsItemType
{
    UIGDetailsItemType_Any     = QGraphicsItem::UserType,
    UIGDetailsItemType_Group   = QGraphicsItem::UserType + 1,
    UIGDetailsItemType_Set     = QGraphicsItem::UserType + 2,
    UIGDetailsItemType_Element = QGraphicsItem::UserType + 3,
    UIGDetailsItemType_Preview = QGraphicsItem::UserType + 10
};

/* Details item interface
 * for graphics details model/view architecture: */
class UIGDetailsItem : public QIGraphicsWidget
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsItem(UIGDetailsItem *pParent);

    /* API: Cast stuff: */
    UIGDetailsGroup* toGroup();
    UIGDetailsSet* toSet();
    UIGDetailsElement* toElement();

    /* API: Model stuff: */
    UIGDetailsModel* model() const;

    /* API: Parent stuff: */
    UIGDetailsItem* parentItem() const;

    /* API: Children stuff: */
    virtual void addItem(UIGDetailsItem *pItem) = 0;
    virtual void removeItem(UIGDetailsItem *pItem) = 0;
    virtual QList<UIGDetailsItem*> items(UIGDetailsItemType type = UIGDetailsItemType_Any) const = 0;
    virtual bool hasItems(UIGDetailsItemType type = UIGDetailsItemType_Any) const = 0;
    virtual void clearItems(UIGDetailsItemType type = UIGDetailsItemType_Any) = 0;

    /* API: Layout stuff: */
    void updateSizeHint();
    virtual void updateLayout() = 0;

protected:

    /* Helpers: Paint stuff: */
    static void configurePainterShape(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, int iRadius);
    static void paintFrameRect(QPainter *pPainter, const QRect &rect, int iRadius);
    static void paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap);
    static void paintText(QPainter *pPainter, const QRect &rect, const QFont &font, const QString &strText, const QColor &color);

private:

    /* Variables: */
    UIGDetailsItem *m_pParent;
};

/* Allows to prepare item synchronously: */
class UIPrepareStep : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Prepare stuff: */
    void sigStepDone(QString strStepId);

public:

    /* Constructor: */
    UIPrepareStep(QObject *pParent, const QString &strStepId = QString());

private slots:

    /* Handlers: Prepare stuff: */
    void sltStepDone();

private:

    /* Variables: */
    QString m_strStepId;
};

#endif /* __UIGDetailsItem_h__ */

