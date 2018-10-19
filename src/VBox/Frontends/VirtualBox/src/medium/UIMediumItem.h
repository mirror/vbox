/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumItem class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumItem_h___
#define ___UIMediumItem_h___

/* GUI includes: */
#include "UIMedium.h"
#include "UIMediumDetailsWidget.h"
#include "QITreeWidget.h"

/** QITreeWidgetItem extension representing Media Manager item. */
class SHARED_LIBRARY_STUFF UIMediumItem : public QITreeWidgetItem, public UIDataMedium
{
public:

    /** Constructs top-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent tree reference. */
    UIMediumItem(const UIMedium &guiMedium, QITreeWidget *pParent);
    /** Constructs sub-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItem(const UIMedium &guiMedium, UIMediumItem *pParent);
    /** Constructs sub-level item under a QITreeWidgetItem.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItem(const UIMedium &guiMedium, QITreeWidgetItem *pParent);

    /** Copies UIMedium wrapped by <i>this</i> item. */
    //virtual bool copy();
    /** Moves UIMedium wrapped by <i>this</i> item. */
    virtual bool move();
    /** Removes UIMedium wrapped by <i>this</i> item. */
    virtual bool remove() = 0;
    /** Releases UIMedium wrapped by <i>this</i> item.
      * @param  fInduced  Brings whether this action is caused by other user's action,
      *                   not a direct order to release particularly selected medium. */
    virtual bool release(bool fInduced = false);

    /** Refreshes item fully. */
    void refreshAll();

    /** Returns UIMedium wrapped by <i>this</i> item. */
    const UIMedium &medium() const { return m_guiMedium; }
    /** Defines UIMedium wrapped by <i>this</i> item. */
    void setMedium(const UIMedium &guiMedium);

    /** Returns UIMediumDeviceType of the wrapped UIMedium. */
    UIMediumDeviceType mediumType() const { return m_guiMedium.type(); }

    /** Returns KMediumState of the wrapped UIMedium. */
    KMediumState state() const { return m_guiMedium.state(); }

    /** Returns QString <i>ID</i> of the wrapped UIMedium. */
    QUuid id() const { return m_guiMedium.id(); }
    /** Returns QString <i>location</i> of the wrapped UIMedium. */
    QString location() const { return m_guiMedium.location(); }

    /** Returns QString <i>hard-disk format</i> of the wrapped UIMedium. */
    QString hardDiskFormat() const { return m_guiMedium.hardDiskFormat(); }
    /** Returns QString <i>hard-disk type</i> of the wrapped UIMedium. */
    QString hardDiskType() const { return m_guiMedium.hardDiskType(); }

    /** Returns QString <i>storage details</i> of the wrapped UIMedium. */
    QString details() const { return m_guiMedium.storageDetails(); }
    /** Returns QString <i>encryption password ID</i> of the wrapped UIMedium. */
    QString encryptionPasswordID() const { return m_guiMedium.encryptionPasswordID(); }

    /** Returns QString <i>tool-tip</i> of the wrapped UIMedium. */
    QString toolTip() const { return m_guiMedium.toolTip(); }

    /** Returns a vector of IDs of all machines wrapped UIMedium is attached to. */
    const QList<QUuid> &machineIds() const { return m_guiMedium.machineIds(); }
    /** Returns QString <i>usage</i> of the wrapped UIMedium. */
    QString usage() const { return m_guiMedium.usage(); }
    /** Returns whether wrapped UIMedium is used. */
    bool isUsed() const { return m_guiMedium.isUsed(); }
    /** Returns whether wrapped UIMedium is used in snapshots. */
    bool isUsedInSnapshots() const { return m_guiMedium.isUsedInSnapshots(); }

    /** Returns whether <i>this</i> item is less than @a other one. */
    bool operator<(const QTreeWidgetItem &other) const;

protected:

    /** Release UIMedium wrapped by <i>this</i> item from virtual @a comMachine. */
    virtual bool releaseFrom(CMachine comMachine) = 0;

    /** Returns default text. */
    virtual QString defaultText() const /* override */;

private:

    /** Refreshes item information such as icon, text and tool-tip. */
    void refresh();

    /** Releases UIMedium wrapped by <i>this</i> item from virtual machine with @a uMachineId. */
    bool releaseFrom(const QUuid &uMachineId);

    /** Formats field text. */
    static QString formatFieldText(const QString &strText, bool fCompact = true, const QString &strElipsis = "middle");

    /** Holds the UIMedium wrapped by <i>this</i> item. */
    UIMedium m_guiMedium;
};


/** UIMediumItem extension representing hard-disk item. */
class SHARED_LIBRARY_STUFF UIMediumItemHD : public UIMediumItem
{
public:

    /** Constructs top-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent tree reference. */
    UIMediumItemHD(const UIMedium &guiMedium, QITreeWidget *pParent);
    /** Constructs sub-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItemHD(const UIMedium &guiMedium, UIMediumItem *pParent);
    /** Constructs sub-level item under a QITreeWidgetItem.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItemHD(const UIMedium &guiMedium, QITreeWidgetItem *pParent);

protected:

    /** Removes UIMedium wrapped by <i>this</i> item. */
    virtual bool remove() /* override */;
    /** Releases UIMedium wrapped by <i>this</i> item from virtual @a comMachine. */
    virtual bool releaseFrom(CMachine comMachine) /* override */;

private:

    /** Proposes user to remove CMedium storage wrapped by <i>this</i> item. */
    bool maybeRemoveStorage();
};

/** UIMediumItem extension representing optical-disk item. */
class SHARED_LIBRARY_STUFF UIMediumItemCD : public UIMediumItem
{
public:

    /** Constructs top-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent tree reference. */
    UIMediumItemCD(const UIMedium &guiMedium, QITreeWidget *pParent);
    /** Constructs sub-level item under a QITreeWidgetItem.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItemCD(const UIMedium &guiMedium, QITreeWidgetItem *pParent);

protected:

    /** Removes UIMedium wrapped by <i>this</i> item. */
    virtual bool remove() /* override */;
    /** Releases UIMedium wrapped by <i>this</i> item from virtual @a comMachine. */
    virtual bool releaseFrom(CMachine comMachine) /* override */;
};

/** UIMediumItem extension representing floppy-disk item. */
class SHARED_LIBRARY_STUFF UIMediumItemFD : public UIMediumItem
{
public:

    /** Constructs top-level item.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent tree reference. */
    UIMediumItemFD(const UIMedium &guiMedium, QITreeWidget *pParent);
    /** Constructs sub-level item under a QITreeWidgetItem.
      * @param  guiMedium  Brings the medium to wrap around.
      * @param  pParent    Brings the parent item reference. */
    UIMediumItemFD(const UIMedium &guiMedium, QITreeWidgetItem *pParent);

protected:

    /** Removes UIMedium wrapped by <i>this</i> item. */
    virtual bool remove() /* override */;
    /** Releases UIMedium wrapped by <i>this</i> item from virtual @a comMachine. */
    virtual bool releaseFrom(CMachine comMachine) /* override */;
};

#endif /* !___UIMediumItem_h___ */
