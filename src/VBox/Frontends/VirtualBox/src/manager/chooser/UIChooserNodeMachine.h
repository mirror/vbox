/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeMachine class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserNode.h"
#include "UIVirtualMachineItem.h"


/** UIChooserNode subclass used as interface for invisible tree-view machine nodes. */
class UIChooserNodeMachine : public UIChooserNode, public UIVirtualMachineItem
{
    Q_OBJECT;

public:

    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  fFavorite   Brings whether the node is favorite.
      * @param  comMachine  Brings COM machine object. */
    UIChooserNodeMachine(UIChooserNode *pParent,
                         bool fFavorite,
                         const CMachine &comMachine);

    /** Returns RTTI node type. */
    virtual UIChooserItemType type() const /* override */ { return UIChooserItemType_Machine; }

    /** Returns item name. */
    virtual QString name() const /* override */;
    /** Returns item full-name. */
    virtual QString fullName() const /* override */;
    /** Returns item description. */
    virtual QString description() const /* override */;
    /** Returns item definition. */
    virtual QString definition() const /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h */
