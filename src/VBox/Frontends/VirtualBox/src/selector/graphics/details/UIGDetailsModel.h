/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsModel class declaration
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

#ifndef __UIGDetailsModel_h__
#define __UIGDetailsModel_h__

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QTransform>
#include <QMap>
#include <QSet>

/* GUI includes: */
#include "UIDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsSceneContextMenuEvent;
class UIGDetailsGroup;
class UIVMItem;
class UIGDetailsElementAnimationCallback;
class UIGDetailsItem;

/* Graphics details-model: */
class UIGDetailsModel : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Root-item stuff: */
    void sigRootItemResized(const QSizeF &size, int iMinimumWidth);

    /* Notifier: Link processing stuff: */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

public:

    /* Constructor/destructor: */
    UIGDetailsModel(QObject *pParent);
    ~UIGDetailsModel();

    /* API: Scene stuff: */
    QGraphicsScene* scene() const;
    QPaintDevice* paintDevice() const;
    QGraphicsItem* itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

    /* API: Layout stuff: */
    void updateLayout();

    /* API: Current-item(s) stuff: */
    void setItems(const QList<UIVMItem*> &items);

private slots:

    /* Handler: Details-view stuff: */
    void sltHandleViewResized();

    /* Handlers: Element-items stuff: */
    void sltToggleElements(DetailsElementType type, bool fToggled);
    void sltToggleAnimationFinished(DetailsElementType type, bool fToggled);
    void sltElementTypeToggled();

    /* Handlers: Chooser stuff: */
    void sltHandleSlidingStarted();
    void sltHandleToggleStarted();
    void sltHandleToggleFinished();

private:

    /* Data enumerator: */
    enum DetailsModelData
    {
        /* Layout hints: */
        DetailsModelData_Margin
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Prepare stuff: */
    void prepareScene();
    void prepareRoot();

    /* Helpers: Cleanup stuff: */
    void cleanupRoot();
    void cleanupScene();

    /* Handler: Event-filter: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);

    /* Handler: Context-menu stuff: */
    bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);

    /* Variables: */
    QGraphicsScene *m_pScene;
    UIGDetailsGroup *m_pRoot;
    UIGDetailsElementAnimationCallback *m_pAnimationCallback;
};

/* Details-element animation callback: */
class UIGDetailsElementAnimationCallback : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Complete stuff: */
    void sigAllAnimationFinished(DetailsElementType type, bool fToggled);

public:

    /* Constructor: */
    UIGDetailsElementAnimationCallback(QObject *pParent, DetailsElementType type, bool fToggled);

    /* API: Notifiers stuff: */
    void addNotifier(UIGDetailsItem *pItem);

private slots:

    /* Handler: Progress stuff: */
    void sltAnimationFinished();

private:

    /* Variables: */
    QList<UIGDetailsItem*> m_notifiers;
    DetailsElementType m_type;
    bool m_fToggled;
};

#endif /* __UIGDetailsModel_h__ */

