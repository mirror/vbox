/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationSet class declaration.
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

#ifndef __UIGInformationSet_h__
#define __UIGInformationSet_h__

/* GUI includes: */
#include "UIGInformationItem.h"
#include "UIExtraDataDefs.h"
#include "UISettingsDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class UIVMItem;

/* Using declarations: */
using namespace UISettingsDefs;

/* Details set
 * for graphics details model/view architecture: */
class UIGInformationSet : public UIGInformationItem
{
    Q_OBJECT;

public:

    /* Graphics-item type: */
    enum { Type = UIGInformationItemType_Set };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGInformationSet(UIGInformationItem *pParent);
    ~UIGInformationSet();

    /* API: Build stuff: */
    void buildSet(UIVMItem *pMachineItem, bool fFullSet, const QMap<InformationElementType, bool> &settings);

    /* API: Machine stuff: */
    const CMachine& machine() const { return m_machine; }
    bool hasDetails() const { return m_fHasDetails; }

    /** Returns configuration access level. */
    ConfigurationAccessLevel configurationAccessLevel() const { return m_configurationAccessLevel; }

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
    void addItem(UIGInformationItem *pItem);
    void removeItem(UIGInformationItem *pItem);
    QList<UIGInformationItem*> items(UIGInformationItemType type = UIGInformationItemType_Element) const;
    bool hasItems(UIGInformationItemType type = UIGInformationItemType_Element) const;
    void clearItems(UIGInformationItemType type = UIGInformationItemType_Element);
    UIGInformationElement* element(InformationElementType elementType) const;

    /* Helpers: Prepare stuff: */
    void prepareSet();
    void prepareConnections();

    /* Helpers: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    void updateLayout();

    /* Helpers: Build stuff: */
    void rebuildSet();
    UIGInformationElement* createElement(InformationElementType elementType, bool fOpen);

    /** Machine-item this set built for. */
    UIVMItem *m_pMachineItem;

    /* Main variables: */
    CMachine m_machine;
    QMap<int, UIGInformationItem*> m_elements;
    bool m_fHasDetails;

    /** Holds configuration access level. */
    ConfigurationAccessLevel m_configurationAccessLevel;

    /* Prepare variables: */
    bool m_fFullSet;
    UIInformationBuildStep *m_pBuildStep;
    int m_iLastStepNumber;
    QString m_strSetId;
    QMap<InformationElementType, bool> m_settings;
};

#endif /* __UIGInformationSet_h__ */

