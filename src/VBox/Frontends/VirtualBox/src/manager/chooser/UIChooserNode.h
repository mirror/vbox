/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserNode class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserNode_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserNode_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QString>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIChooserDefs.h"
#include "UIChooserItem.h"

/* Forward declaration: */
class UIChooserNodeGroup;
class UIChooserNodeGlobal;
class UIChooserNodeMachine;


/** QObject subclass used as interface for invisible tree-view nodes.
  * These nodes can be of three types (group, global and machine node).
  * They can be used to compose a tree of nodes loaded from VBox setting. */
class UIChooserNode : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

public:

    /** Constructs chooser node passing @a pParent to the base-class.
      * @param  fFavorite  Brings whether the node is favorite. */
    UIChooserNode(UIChooserNode *pParent = 0, bool fFavorite = false);
    /** Destructs chooser node. */
    virtual ~UIChooserNode() /* override */;

    /** Returns RTTI node type. */
    virtual UIChooserItemType type() const = 0;

    /** Casts node to group one. */
    UIChooserNodeGroup *toGroupNode();
    /** Casts node to global one. */
    UIChooserNodeGlobal *toGlobalNode();
    /** Casts node to machine one. */
    UIChooserNodeMachine *toMachineNode();

    /** Returns parent node reference. */
    UIChooserNode *parentNode() const {  return m_pParent; }
    /** Returns whether node is of root kind. */
    bool isRoot() const { return !m_pParent; }

    /** Returns whether the node is favorite. */
    bool isFavorite() const { return m_fFavorite; }
    /** Defines whether the node is @a fFavorite. */
    void setFavorite(bool fFavorite) { m_fFavorite = fFavorite; }

    /** Returns node name. */
    virtual QString name() const = 0;
    /** Returns full node name. */
    virtual QString fullName() const = 0;
    /** Returns item description. */
    virtual QString description() const = 0;
    /** Returns item definition. */
    virtual QString definition() const = 0;

    /** Returns whether there are children of certain @a enmType. */
    virtual bool hasNodes(UIChooserItemType enmType = UIChooserItemType_Any) const = 0;
    /** Returns a list of nodes of certain @a enmType. */
    virtual QList<UIChooserNode*> nodes(UIChooserItemType enmType = UIChooserItemType_Any) const = 0;

    /** Adds passed @a pNode to specified @a iPosition. */
    virtual void addNode(UIChooserNode *pNode, int iPosition) = 0;
    /** Removes passed @a pNode. */
    virtual void removeNode(UIChooserNode *pNode) = 0;

    /** Removes all children with specified @a uId recursively. */
    virtual void removeAllNodes(const QUuid &uId) = 0;
    /** Updates all children with specified @a uId recursively. */
    virtual void updateAllNodes(const QUuid &uId) = 0;

    /** Returns node position. */
    int position();
    /** Returns position of specified node inside this one. */
    virtual int positionOf(UIChooserNode *pNode) = 0;

    /** Defines linked @a pItem. */
    void setItem(UIChooserItem *pItem) { m_pItem = pItem; }
    /** Returns linked item. */
    UIChooserItem *item() const { return m_pItem.data(); }

    /** Performs search wrt. @a strSearchTerm and @a iItemSearchFlags and updates @a matchedItems. For an empty
      * @a strSearchTerm all items are added wrt. node type from @a iItemSearchFlags. */
    virtual void searchForNodes(const QString &strSearchTerm, int iItemSearchFlags, QList<UIChooserNode*> &matchedItems) = 0;

    /** Returns if node is disabled. */
    bool isDisabled() const;
    /** Sets the disabled flag. */
    void setDisabled(bool fDisabled);

protected:

    /** Holds the parent node reference. */
    UIChooserNode  *m_pParent;
    /** Holds whether the node is favorite. */
    bool            m_fFavorite;

    /** Holds the linked item reference. */
    QPointer<UIChooserItem>  m_pItem;

    /** Holds item description. */
    QString  m_strDescription;

    /** Holds the flag to indicate whether the node is disabled or not. */
    bool m_fDisabled;

};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserNode_h */
