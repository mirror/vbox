/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationModel class declaration.
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

#ifndef __UIGInformationModel_h__
#define __UIGInformationModel_h__

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QMap>
#include <QSet>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsSceneContextMenuEvent;
class QGraphicsView;
class UIGInformationGroup;
class UIVMItem;
class UIGInformationElementAnimationCallback;
class UIGInformationItem;

/* Graphics details-model: */
class UIGInformationModel : public QObject
{
    Q_OBJECT;

signals:

    /* Notifiers: Root-item stuff: */
    void sigRootItemMinimumWidthHintChanged(int iRootItemMinimumWidthHint);
    void sigRootItemMinimumHeightHintChanged(int iRootItemMinimumHeightHint);

    /* Notifier: Link processing stuff: */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

public:

    /* Constructor/destructor: */
    UIGInformationModel(QObject *pParent);
    ~UIGInformationModel();

    /* API: Scene stuff: */
    QGraphicsScene* scene() const;
    QGraphicsView* paintDevice() const;
    QGraphicsItem* itemAt(const QPointF &position) const;

    /* API: Layout stuff: */
    void updateLayout();

    /* API: Current-item(s) stuff: */
    void setItems(const QList<UIVMItem*> &items);

    /** Returns information-window elements. */
    QMap<InformationElementType, bool> informationWindowElements();
    /** Defines information-window @a elements. */
    void setInformationWindowElements(const QMap<InformationElementType, bool> &elements);

private slots:

    /* Handler: Details-view stuff: */
    void sltHandleViewResize();

    /* Handlers: Element-items stuff: */
    void sltToggleElements(InformationElementType type, bool fToggled);
    void sltToggleAnimationFinished(InformationElementType type, bool fToggled);
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
    void loadSettings();

    /* Helpers: Cleanup stuff: */
    void saveSettings();
    void cleanupRoot();
    void cleanupScene();

    /* Handler: Event-filter: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);

    /* Handler: Context-menu stuff: */
    bool processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);

    /* Variables: */
    QGraphicsScene *m_pScene;
    UIGInformationGroup *m_pRoot;
    UIGInformationElementAnimationCallback *m_pAnimationCallback;
    /** Holds the details settings. */
    QMap<InformationElementType, bool> m_settings;
};

/* Details-element animation callback: */
class UIGInformationElementAnimationCallback : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Complete stuff: */
    void sigAllAnimationFinished(InformationElementType type, bool fToggled);

public:

    /* Constructor: */
    UIGInformationElementAnimationCallback(QObject *pParent, InformationElementType type, bool fToggled);

    /* API: Notifiers stuff: */
    void addNotifier(UIGInformationItem *pItem);

private slots:

    /* Handler: Progress stuff: */
    void sltAnimationFinished();

private:

    /* Variables: */
    QList<UIGInformationItem*> m_notifiers;
    InformationElementType m_type;
    bool m_fToggled;
};

#endif /* __UIGInformationModel_h__ */

