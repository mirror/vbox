/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsGroup class declaration
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

#ifndef __UIGDetailsGroup_h__
#define __UIGDetailsGroup_h__

/* GUI includes: */
#include "UIGDetailsItem.h"

/* Forward declarations: */
class QGraphicsLinearLayout;
class UIVMItem;

/* Details group
 * for graphics details model/view architecture: */
class UIGDetailsGroup : public UIGDetailsItem
{
    Q_OBJECT;

signals:

    /* Notifiers: Prepare stuff: */
    void sigStartFirstStep(QString strGroupId);

public:

    /* Graphics-item type: */
    enum { Type = UIGDetailsItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGDetailsGroup();
    ~UIGDetailsGroup();

    /* API: Prepare stuff: */
    void setItems(const QList<UIVMItem*> &items);
    void updateItems();
    void stopPopulatingItems();

    /* API: Children stuff: */
    void addItem(UIGDetailsItem *pItem);
    void removeItem(UIGDetailsItem *pItem);
    QList<UIGDetailsItem*> items(UIGDetailsItemType type = UIGDetailsItemType_Set) const;
    bool hasItems(UIGDetailsItemType type = UIGDetailsItemType_Set) const;
    void clearItems(UIGDetailsItemType type = UIGDetailsItemType_Set);

    /* API: Layout stuff: */
    void updateLayout();

private slots:

    /* Handlers: Prepare stuff: */
    void sltFirstStep(QString strGroupId);
    void sltNextStep(QString strGroupId);

private:

    /* Data enumerator: */
    enum GroupItemData
    {
        /* Layout hints: */
        GroupData_Margin,
        GroupData_Spacing
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Prepare stuff: */
    void loadSettings();
    void prepareLayout();
    void prepareSets(const QList<UIVMItem*> &items);
    void updateSets();
    void prepareSet(QString strGroupId);

    /* Main variables: */
    QGraphicsLinearLayout *m_pMainLayout;
    QGraphicsLinearLayout *m_pLayout;
    QList<UIGDetailsItem*> m_sets;

    /* Prepare variables: */
    QList<UIVMItem*> m_items;
    UIPrepareStep *m_pStep;
    int m_iStep;
    QString m_strGroupId;
    QStringList m_settings;
};

#endif /* __UIGDetailsGroup_h__ */

