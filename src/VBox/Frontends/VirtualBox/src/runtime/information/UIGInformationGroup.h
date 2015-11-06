/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationGroup class declaration.
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

#ifndef __UIGInformationGroup_h__
#define __UIGInformationGroup_h__

/* GUI includes: */
#include "UIGInformationItem.h"

/* Forward declarations: */
class UIVMItem;
class QGraphicsScene;

/* Details group
 * for graphics details model/view architecture: */
class UIGInformationGroup : public UIGInformationItem
{
    Q_OBJECT;

signals:

    /* Notifiers: Size-hint stuff: */
    void sigMinimumWidthHintChanged(int iMinimumWidthHint);
    void sigMinimumHeightHintChanged(int iMinimumHeightHint);

public:

    /* Graphics-item type: */
    enum { Type = UIGInformationItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGInformationGroup(QGraphicsScene *pParent);
    ~UIGInformationGroup();

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
    void addItem(UIGInformationItem *pItem);
    void removeItem(UIGInformationItem *pItem);
    QList<UIGInformationItem*> items(UIGInformationItemType type = UIGInformationItemType_Set) const;
    bool hasItems(UIGInformationItemType type = UIGInformationItemType_Set) const;
    void clearItems(UIGInformationItemType type = UIGInformationItemType_Set);

    /* Helpers: Prepare stuff: */
    void prepareConnections();

    /* Helpers: Layout stuff: */
    void updateGeometry();
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    void updateLayout();

    /* Variables: */
    int m_iPreviousMinimumWidthHint;
    int m_iPreviousMinimumHeightHint;
    QList<UIGInformationItem*> m_items;
    QList<UIVMItem*> m_machineItems;
    UIInformationBuildStep *m_pBuildStep;
    QString m_strGroupId;

    /* Friends: */
    friend class UIGInformationModel;
};

#endif /* __UIGInformationGroup_h__ */

