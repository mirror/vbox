/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGroup class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGroup_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGroup_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserNode.h"


/** UIChooserNode subclass used as interface for invisible tree-view group nodes. */
class UIChooserNodeGroup : public UIChooserNode
{
    Q_OBJECT;

public:

    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  fFavorite  Brings whether the node is favorite.
      * @param  strName    Brings current node name.
      * @param  fOpened    Brings whether this group node is opened. */
    UIChooserNodeGroup(UIChooserNode *pParent,
                       bool fFavorite,
                       const QString &strName,
                       bool fOpened);

    /** Returns RTTI node type. */
    virtual UIChooserItemType type() const /* override */ { return UIChooserItemType_Group; }

    /** Returns item name. */
    virtual QString name() const /* override */;
    /** Returns item full-name. */
    virtual QString fullName() const /* override */;
    /** Returns item description. */
    virtual QString description() const /* override */;
    /** Returns item definition. */
    virtual QString definition() const /* override */;

    /** Defines node @a strName. */
    void setName(const QString &strName);

    /** Returns whether this group node is opened. */
    bool isOpened() const { return m_fOpened; }
    /** Returns whether this group node is closed. */
    bool isClosed() const { return !m_fOpened; }

    /** Opens this group node. */
    void open() { m_fOpened = true; }
    /** Closes this group node. */
    void close() { m_fOpened = false; }

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Holds the node name. */
    QString  m_strName;
    /** Holds whether node is opened. */
    bool     m_fOpened;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGroup_h */
