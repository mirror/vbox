/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeGroup class declaration.
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
      * @param  iPosition  Brings the initial node position.
      * @param  strName    Brings current node name.
      * @param  fOpened    Brings whether this group node is opened. */
    UIChooserNodeGroup(UIChooserNode *pParent,
                       bool fFavorite,
                       int iPosition,
                       const QString &strName,
                       bool fOpened);
    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  pCopyFrom  Brings the node to copy data from.
      * @param  iPosition  Brings the initial node position. */
    UIChooserNodeGroup(UIChooserNode *pParent,
                       UIChooserNodeGroup *pCopyFrom,
                       int iPosition);
    /** Destructs chooser node removing it's children. */
    virtual ~UIChooserNodeGroup() /* override */;

    /** Returns RTTI node type. */
    virtual UIChooserNodeType type() const /* override */ { return UIChooserNodeType_Group; }

    /** Returns item name. */
    virtual QString name() const /* override */;
    /** Returns item full-name. */
    virtual QString fullName() const /* override */;
    /** Returns item description. */
    virtual QString description() const /* override */;
    /** Returns item definition. */
    virtual QString definition() const /* override */;

    /** Returns whether there are children of certain @a enmType. */
    virtual bool hasNodes(UIChooserNodeType enmType = UIChooserNodeType_Any) const /* override */;
    /** Returns a list of nodes of certain @a enmType. */
    virtual QList<UIChooserNode*> nodes(UIChooserNodeType enmType = UIChooserNodeType_Any) const /* override */;

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

    /** Recursively searches for a children wrt.  @a strSearchTerm and @a iItemSearchFlags and updates the @a matchedItems. */
    virtual void searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems) /* override */;

    /** Performs sorting of children nodes. */
    virtual void sortNodes() /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Copies children contents from @a pCopyFrom item. */
    void copyContents(UIChooserNodeGroup *pCopyFrom);

    /** Holds the node name. */
    QString  m_strName;
    /** Holds whether node is opened. */
    bool     m_fOpened;

    /** Holds group children. */
    QList<UIChooserNode*>  m_nodesGroup;
    /** Holds global children. */
    QList<UIChooserNode*>  m_nodesGlobal;
    /** Holds machine children. */
    QList<UIChooserNode*>  m_nodesMachine;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeGroup_h */
