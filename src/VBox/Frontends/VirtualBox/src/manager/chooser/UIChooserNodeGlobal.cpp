/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGlobal class implementation.
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
#include "UIChooserNodeGlobal.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIChooserNodeGlobal::UIChooserNodeGlobal(UIChooserNode *pParent,
                                         bool fFavorite,
                                         const QString &)
    : UIChooserNode(pParent, fFavorite)
{
    retranslateUi();
}

QString UIChooserNodeGlobal::name() const
{
    return m_strName;
}

QString UIChooserNodeGlobal::fullName() const
{
    return name();
}

QString UIChooserNodeGlobal::description() const
{
    return name();
}

QString UIChooserNodeGlobal::definition() const
{
    return QString("n=%1").arg("GLOBAL");
}

void UIChooserNodeGlobal::retranslateUi()
{
    /* Translate name: */
    m_strName = tr("Tools");

    /* Update global-item: */
    if (item())
        item()->updateItem();
}
