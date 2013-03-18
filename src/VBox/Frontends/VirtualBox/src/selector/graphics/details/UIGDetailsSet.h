/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsSet class declaration
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

#ifndef __UIGDetailsSet_h__
#define __UIGDetailsSet_h__

/* GUI includes: */
#include "UIGDetailsItem.h"
#include "UIDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class UIVMItem;

/* Details set
 * for graphics details model/view architecture: */
class UIGDetailsSet : public UIGDetailsItem
{
    Q_OBJECT;

public:

    /* Graphics-item type: */
    enum { Type = UIGDetailsItemType_Set };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGDetailsSet(UIGDetailsItem *pParent);
    ~UIGDetailsSet();

    /* API: Build stuff: */
    void buildSet(UIVMItem *pMachineItem, bool fFullSet, const QStringList &settings);

    /* API: Machine stuff: */
    const CMachine& machine() const { return m_machine; }
    bool elementNameHoverable() const { return m_fElementNameHoverable; }
    bool hasDetails() const { return m_fHasDetails; }

private slots:

    /* Handler: Build stuff: */
    void sltBuildStep(QString strStepId, int iStepNumber);

    /* Handlers: Global event stuff: */
    void sltMachineStateChange(QString strId);
    void sltMachineAttributesChange(QString strId);

    /* Handler: Update stuff: */
    void sltUpdateAppearance();

private:

    /* Data enumerator: */
    enum SetItemData
    {
        /* Layout hints: */
        SetData_Margin,
        SetData_Spacing
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Hidden API: Children stuff: */
    void addItem(UIGDetailsItem *pItem);
    void removeItem(UIGDetailsItem *pItem);
    QList<UIGDetailsItem*> items(UIGDetailsItemType type = UIGDetailsItemType_Element) const;
    bool hasItems(UIGDetailsItemType type = UIGDetailsItemType_Element) const;
    void clearItems(UIGDetailsItemType type = UIGDetailsItemType_Element);
    UIGDetailsElement* element(DetailsElementType elementType) const;

    /* Helpers: Prepare stuff: */
    void prepareSet();
    void prepareConnections();

    /* Helpers: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    void updateLayout();

    /* Helpers: Build stuff: */
    void rebuildSet();
    UIGDetailsElement* createElement(DetailsElementType elementType, bool fOpen);

    /* Main variables: */
    CMachine m_machine;
    QMap<int, UIGDetailsItem*> m_elements;
    bool m_fElementNameHoverable;
    bool m_fHasDetails;

    /* Prepare variables: */
    bool m_fFullSet;
    UIBuildStep *m_pBuildStep;
    int m_iLastStepNumber;
    QString m_strSetId;
    QStringList m_settings;
};

#endif /* __UIGDetailsSet_h__ */

