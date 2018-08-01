/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsItem class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIDetailsItem_h__
#define __UIDetailsItem_h__

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "QIWithRetranslateUI.h"

/* Forward declaration: */
class UIDetailsModel;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class UIDetailsGroup;
class UIDetailsSet;
class UIDetailsElement;

/* UIDetailsItem types: */
enum UIDetailsItemType
{
    UIDetailsItemType_Any     = QGraphicsItem::UserType,
    UIDetailsItemType_Group   = QGraphicsItem::UserType + 1,
    UIDetailsItemType_Set     = QGraphicsItem::UserType + 2,
    UIDetailsItemType_Element = QGraphicsItem::UserType + 3,
    UIDetailsItemType_Preview = QGraphicsItem::UserType + 10
};

/* Details item interface
 * for graphics details model/view architecture: */
class UIDetailsItem : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;

signals:

    /* Notifiers: Build stuff: */
    void sigBuildStep(QString strStepId, int iStepNumber);
    void sigBuildDone();

public:

    /* Constructor: */
    UIDetailsItem(UIDetailsItem *pParent);

    /* API: Cast stuff: */
    UIDetailsGroup* toGroup();
    UIDetailsSet* toSet();
    UIDetailsElement* toElement();

    /* API: Model stuff: */
    UIDetailsModel* model() const;

    /* API: Parent stuff: */
    UIDetailsItem* parentItem() const;

    /** Returns the description of the item. */
    virtual QString description() const = 0;

    /* API: Children stuff: */
    virtual void addItem(UIDetailsItem *pItem) = 0;
    virtual void removeItem(UIDetailsItem *pItem) = 0;
    virtual QList<UIDetailsItem*> items(UIDetailsItemType type = UIDetailsItemType_Any) const = 0;
    virtual bool hasItems(UIDetailsItemType type = UIDetailsItemType_Any) const = 0;
    virtual void clearItems(UIDetailsItemType type = UIDetailsItemType_Any) = 0;

    /* API: Layout stuff: */
    void updateGeometry();
    virtual int minimumWidthHint() const = 0;
    virtual int minimumHeightHint() const = 0;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    virtual void updateLayout() = 0;

protected slots:

    /* Handler: Build stuff: */
    virtual void sltBuildStep(QString strStepId, int iStepNumber);

protected:

    /* Helper: Translate stuff: */
    void retranslateUi() {}

    /* Helpers: Paint stuff: */
    static void configurePainterShape(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, int iRadius);
    static void paintFrameRect(QPainter *pPainter, const QRect &rect, int iRadius);
    static void paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap);
    static void paintText(QPainter *pPainter, QPoint point,
                          const QFont &font, QPaintDevice *pPaintDevice,
                          const QString &strText, const QColor &color);

private:

    /* Variables: */
    UIDetailsItem *m_pParent;
};

/* Allows to build item content synchronously: */
class UIPrepareStep : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Build stuff: */
    void sigStepDone(QString strStepId, int iStepNumber);

public:

    /* Constructor: */
    UIPrepareStep(QObject *pParent, QObject *pBuildObject, const QString &strStepId, int iStepNumber);

private slots:

    /* Handler: Build stuff: */
    void sltStepDone();

private:

    /* Variables: */
    QString m_strStepId;
    int m_iStepNumber;
};

#endif /* __UIDetailsItem_h__ */

