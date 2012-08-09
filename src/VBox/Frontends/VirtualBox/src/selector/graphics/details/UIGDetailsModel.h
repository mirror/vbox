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

/* Graphics selector model: */
class UIGDetailsModel : public QObject
{
    Q_OBJECT;

signals:

    /* Notify listeners about root-item height changed: */
    void sigRootItemResized(const QSizeF &size, int iMinimumWidth);

    /* Link-click stuff: */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

public:

    /* Constructor/destructor: */
    UIGDetailsModel(QObject *pParent);
    ~UIGDetailsModel();

    /* API: Scene getter: */
    QGraphicsScene* scene() const;

    /* API: Item positioning stuff: */
    QGraphicsItem* itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

    /* API: Group/machine stuff: */
    void setItems(const QList<UIVMItem*> &items);

    /* API: Layout stuff: */
    void updateLayout();

private slots:

    /* Handler: View-resize: */
    void sltHandleViewResized();

    /* Handler: Toggle stuff: */
    void sltToggleElements(DetailsElementType type, bool fToggled);

    /* Handler: Context-menu stuff: */
    void sltElementTypeToggled();

    /* Handler: Sliding started in chooser: */
    void sltHandleSlidingStarted();

    /* Handlers: Togle stuff in chooser: */
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

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);

    /* Handler: Context menu stuff: */
    bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);

    /* Variables: */
    QGraphicsScene *m_pScene;
    UIGDetailsGroup *m_pRoot;
};

#endif /* __UIGDetailsModel_h__ */

