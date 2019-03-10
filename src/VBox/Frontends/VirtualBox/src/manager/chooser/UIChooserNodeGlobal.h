/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGlobal class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGlobal_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGlobal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserNode.h"


/** UIChooserNode subclass used as interface for invisible tree-view global nodes. */
class UIChooserNodeGlobal : public UIChooserNode
{
    Q_OBJECT;

public:

    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  fFavorite  Brings whether the node is favorite.
      * @param  strTip     Brings the dummy tip. */
    UIChooserNodeGlobal(UIChooserNode *pParent,
                        bool fFavorite,
                        const QString &strTip);

    /** Returns RTTI node type. */
    virtual UIChooserItemType type() const /* override */ { return UIChooserItemType_Global; }

    /** Returns node name. */
    virtual QString name() const /* override */;
    /** Returns full node name. */
    virtual QString fullName() const /* override */;
    /** Returns item description. */
    virtual QString description() const /* override */;
    /** Returns item definition. */
    virtual QString definition() const /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Holds the node name. */
    QString  m_strName;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGlobal_h */
