/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGlobal class declaration.
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
      * @param  iPosition  Brings the initial node position.
      * @param  strTip     Brings the dummy tip. */
    UIChooserNodeGlobal(UIChooserNode *pParent,
                        bool fFavorite,
                        int iPosition,
                        const QString &strTip);
    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  pCopyFrom  Brings the node to copy data from.
      * @param  iPosition  Brings the initial node position. */
    UIChooserNodeGlobal(UIChooserNode *pParent,
                        UIChooserNodeGlobal *pCopyFrom,
                        int iPosition);
    /** Destructs chooser node. */
    virtual ~UIChooserNodeGlobal() /* override */;

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

    /** Returns whether there are children of certain @a enmType. */
    virtual bool hasNodes(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;
    /** Returns a list of nodes of certain @a enmType. */
    virtual QList<UIChooserNode*> nodes(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;

    /** Adds passed @a pNode to specified @a iPosition. */
    virtual void addNode(UIChooserNode *pNode, int iPosition) /* override */;
    /** Removes passed @a pNode. */
    virtual void removeNode(UIChooserNode *pNode) /* override */;

    /** Removes all children with specified @a uId recursively. */
    virtual void removeAllNodes(const QUuid &uId) /* override */;
    /** Updates all children with specified @a uId recursively. */
    virtual void updateAllNodes(const QUuid &uId) /* override */;

    /** Returns whether this node is a cloud node itself
      * or contains at least one cloud VM node child. */
    virtual bool hasAtLeastOneCloudNode() const /* override */;

    /** Returns position of specified node inside this one. */
    virtual int positionOf(UIChooserNode *pNode) /* override */;

    /** Updates the @a matchedItems wrt. @strSearchTerm and @a iItemSearchFlags. */
    virtual void searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems) /* override */;

    /** Performs sorting of children nodes. */
    virtual void sortNodes() /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Holds the node name. */
    QString  m_strName;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGlobal_h */
