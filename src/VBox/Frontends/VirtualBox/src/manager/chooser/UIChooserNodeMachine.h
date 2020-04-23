/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNodeMachine class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserNode.h"
#include "UIManagerDefs.h"

/* Forward declarations: */
class UIVirtualMachineItem;
class CCloudMachine;
class CMachine;


/** UIChooserNode subclass used as interface for invisible tree-view machine nodes. */
class UIChooserNodeMachine : public UIChooserNode
{
    Q_OBJECT;

public:

    /** Constructs chooser node for local VM passing @a pParent to the base-class.
      * @param  fFavorite   Brings whether the node is favorite.
      * @param  iPosition   Brings initial node position.
      * @param  comMachine  Brings COM machine object. */
    UIChooserNodeMachine(UIChooserNode *pParent,
                         bool fFavorite,
                         int iPosition,
                         const CMachine &comMachine);
    /** Constructs chooser node for real cloud VM passing @a pParent to the base-class.
      * @param  fFavorite        Brings whether the node is favorite.
      * @param  iPosition        Brings initial node position.
      * @param  comCloudMachine  Brings COM cloud machine object. */
    UIChooserNodeMachine(UIChooserNode *pParent,
                         bool fFavorite,
                         int iPosition,
                         const CCloudMachine &comCloudMachine);
    /** Constructs chooser node for fake cloud VM passing @a pParent to the base-class.
      * @param  fFavorite  Brings whether the node is favorite.
      * @param  iPosition  Brings the initial node position. */
    UIChooserNodeMachine(UIChooserNode *pParent,
                         bool fFavorite,
                         int iPosition);
    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  pCopyFrom  Brings the node to copy data from.
      * @param  iPosition  Brings the initial node position. */
    UIChooserNodeMachine(UIChooserNode *pParent,
                         UIChooserNodeMachine *pCopyFrom,
                         int iPosition);
    /** Destructs chooser node. */
    virtual ~UIChooserNodeMachine() /* override */;

    /** Returns RTTI node type. */
    virtual UIChooserNodeType type() const /* override */ { return UIChooserNodeType_Machine; }

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

    /** Returns position of specified node inside this one. */
    virtual int positionOf(UIChooserNode *pNode) /* override */;

    /** Checks if this instance matches to search  wrt. @a strSearchTerm and @a iItemSearchFlags and updates @a matchedItems. */
    virtual void searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems) /* override */;

    /** Performs sorting of children nodes. */
    virtual void sortNodes() /* override */;

    /** Returns virtual machine cache instance. */
    UIVirtualMachineItem *cache() const;
    /** Returns virtual machine cache instance. */
    UIVirtualMachineItemType cacheType() const;

    /** Returns node machine id. */
    QUuid id() const;
    /** Returns whether node accessible. */
    bool accessible() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles machine state change. */
    void sltHandleStateChange();

private:

    /** Holds virtual machine cache instance. */
    UIVirtualMachineItem *m_pCache;
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNodeMachine_h */
