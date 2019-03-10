/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeMachine class implementation.
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

/* Qt includes */
//#include <QUuid>

/* GUI includes: */
#include "UIChooserNodeMachine.h"


UIChooserNodeMachine::UIChooserNodeMachine(UIChooserNode *pParent,
                                           bool fFavorite,
                                           const CMachine &comMachine)
    : UIChooserNode(pParent, fFavorite)
    , UIVirtualMachineItem(comMachine)
{
    retranslateUi();
}

QString UIChooserNodeMachine::name() const
{
    return UIVirtualMachineItem::name();
}

QString UIChooserNodeMachine::fullName() const
{
    /* Get full parent name, append with '/' if not yet appended: */
    AssertReturn(parentNode(), name());
    QString strFullParentName = parentNode()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIChooserNodeMachine::description() const
{
    return m_strDescription;
}

QString UIChooserNodeMachine::definition() const
{
    return QString("m=%1").arg(name());
}

void UIChooserNodeMachine::retranslateUi()
{
    /* Update description: */
    m_strDescription = tr("Virtual Machine");

    /* Update machine-item: */
    if (item())
        item()->updateItem();
}
