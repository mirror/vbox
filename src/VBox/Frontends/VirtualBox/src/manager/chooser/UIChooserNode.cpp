/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNode class definition.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include "UIChooserNode.h"
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeGlobal.h"
#include "UIChooserNodeMachine.h"


UIChooserNode::UIChooserNode(UIChooserNode *pParent /* = 0 */, bool fFavorite /* = false */)
    : QIWithRetranslateUI3<QObject>(pParent)
    , m_pParent(pParent)
    , m_fFavorite(fFavorite)
    , m_fDisabled(false)
{
}

UIChooserNode::~UIChooserNode()
{
    if (!m_pItem.isNull())
        delete m_pItem.data();
}

UIChooserNodeGroup *UIChooserNode::toGroupNode()
{
    return static_cast<UIChooserNodeGroup*>(this);
}

UIChooserNodeGlobal *UIChooserNode::toGlobalNode()
{
    return static_cast<UIChooserNodeGlobal*>(this);
}

UIChooserNodeMachine *UIChooserNode::toMachineNode()
{
    return static_cast<UIChooserNodeMachine*>(this);
}

int UIChooserNode::position()
{
    return parentNode() ? parentNode()->positionOf(this) : 0;
}

bool UIChooserNode::isDisabled() const
{
    return m_fDisabled;
}

void UIChooserNode::setDisabled(bool fDisabled)
{
    if (fDisabled == m_fDisabled)
        return;
    m_fDisabled = fDisabled;
    if (m_pItem)
        m_pItem->disableEnableItem(m_fDisabled);
}
