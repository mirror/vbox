/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationItem class declaration.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGInformationItem_h__
#define __UIGInformationItem_h__

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "QIWithRetranslateUI.h"

/* Forward declaration: */
class UIGInformationModel;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class UIGInformationGroup;
class UIGInformationSet;
class UIGInformationElement;

/* UIGInformationItem types: */
enum UIGInformationItemType
{
    UIGInformationItemType_Any     = QGraphicsItem::UserType,
    UIGInformationItemType_Group   = QGraphicsItem::UserType + 1,
    UIGInformationItemType_Set     = QGraphicsItem::UserType + 2,
    UIGInformationItemType_Element = QGraphicsItem::UserType + 3,
    UIGInformationItemType_Preview = QGraphicsItem::UserType + 10
};

/* Details item interface
 * for graphics details model/view architecture: */
class UIGInformationItem : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;

signals:

    /* Notifiers: Build stuff: */
    void sigBuildStep(QString strStepId, int iStepNumber);
    void sigBuildDone();

public:

    /* Constructor: */
    UIGInformationItem(UIGInformationItem *pParent);

    /* API: Cast stuff: */
    UIGInformationGroup* toGroup();
    UIGInformationSet* toSet();
    UIGInformationElement* toElement();

    /* API: Model stuff: */
    UIGInformationModel* model() const;

    /* API: Parent stuff: */
    UIGInformationItem* parentItem() const;

    /* API: Children stuff: */
    virtual void addItem(UIGInformationItem *pItem) = 0;
    virtual void removeItem(UIGInformationItem *pItem) = 0;
    virtual QList<UIGInformationItem*> items(UIGInformationItemType type = UIGInformationItemType_Any) const = 0;
    virtual bool hasItems(UIGInformationItemType type = UIGInformationItemType_Any) const = 0;
    virtual void clearItems(UIGInformationItemType type = UIGInformationItemType_Any) = 0;

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
    UIGInformationItem *m_pParent;
};

/* Allows to build item content synchronously: */
class UIInformationBuildStep : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Build stuff: */
    void sigStepDone(QString strStepId, int iStepNumber);

public:

    /* Constructor: */
    UIInformationBuildStep(QObject *pParent, QObject *pBuildObject, const QString &strStepId, int iStepNumber);

private slots:

    /* Handler: Build stuff: */
    void sltStepDone();

private:

    /* Variables: */
    QString m_strStepId;
    int m_iStepNumber;
};

#endif /* __UIGInformationItem_h__ */

