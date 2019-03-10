/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGroup class implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeGlobal.h"
#include "UIChooserNodeMachine.h"


UIChooserNodeGroup::UIChooserNodeGroup(UIChooserNode *pParent,
                                       bool fFavorite,
                                       const QString &strName,
                                       bool fOpened)
    : UIChooserNode(pParent, fFavorite)
    , m_strName(strName)
    , m_fOpened(fOpened)
{
    retranslateUi();
}

QString UIChooserNodeGroup::name() const
{
    return m_strName;
}

QString UIChooserNodeGroup::fullName() const
{
    /* Return "/" for root item: */
    if (isRoot())
        return "/";
    /* Get full parent name, append with '/' if not yet appended: */
    QString strFullParentName = parentNode()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIChooserNodeGroup::description() const
{
    return m_strDescription;
}

QString UIChooserNodeGroup::definition() const
{
    return QString("g=%1").arg(name());
}

void UIChooserNodeGroup::setName(const QString &strName)
{
    /* Make sure something changed: */
    if (m_strName == strName)
        return;

    /* Save name: */
    m_strName = strName;

    /* Update group-item: */
    if (item())
        item()->updateItem();
}

void UIChooserNodeGroup::retranslateUi()
{
    /* Update description: */
    m_strDescription = tr("Virtual Machine group");

    /* Update group-item: */
    if (item())
        item()->updateItem();
}
