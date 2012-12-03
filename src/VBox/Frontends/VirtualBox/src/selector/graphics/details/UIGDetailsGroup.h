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
class UIVMItem;

/* Details group
 * for graphics details model/view architecture: */
class UIGDetailsGroup : public UIGDetailsItem
{
    Q_OBJECT;

signals:

    /* Notifier: Build stuff: */
    void sigBuildStep(QString strStepId, int iStepNumber);

public:

    /* Graphics-item type: */
    enum { Type = UIGDetailsItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGDetailsGroup();
    ~UIGDetailsGroup();

    /* API: Build stuff: */
    void buildGroup(const QList<UIVMItem*> &machineItems);
    void rebuildGroup();
    void stopBuildingGroup();

private slots:

    /* Handler: Build stuff: */
    void sltBuildStep(QString strStepId, int iStepNumber);

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

    /* Hidden API: Children stuff: */
    void addItem(UIGDetailsItem *pItem);
    void removeItem(UIGDetailsItem *pItem);
    QList<UIGDetailsItem*> items(UIGDetailsItemType type = UIGDetailsItemType_Set) const;
    bool hasItems(UIGDetailsItemType type = UIGDetailsItemType_Set) const;
    void clearItems(UIGDetailsItemType type = UIGDetailsItemType_Set);

    /* Helpers: Prepare stuff: */
    void prepareConnections();
    void loadSettings();

    /* Helpers: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    void updateLayout();

    /* Variables: */
    QList<UIGDetailsItem*> m_items;
    QList<UIVMItem*> m_machineItems;
    UIBuildStep *m_pBuildStep;
    QString m_strGroupId;
    QStringList m_settings;

    /* Friends: */
    friend class UIGDetailsModel;
};

#endif /* __UIGDetailsGroup_h__ */

