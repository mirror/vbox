/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMedium class declaration
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMedium_h__
#define __UIMedium_h__

/* Qt includes: */
#include <QPixmap>
#include <QLinkedList>

/* GUI includes: */
#include "UIMediumDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Other VBox includes: */
#include "iprt/cpp/utils.h"

/**
 * Cache used to override some attributes in the user-friendly "don't show diffs" mode.
 */
struct NoDiffsCache
{
    NoDiffsCache() : isSet(false), state(KMediumState_NotCreated) {}
    NoDiffsCache& operator=(const NoDiffsCache &other)
    {
        isSet = other.isSet;
        state = other.state;
        result = other.result;
        toolTip = other.toolTip;
        return *this;
    }

    bool isSet : 1;

    KMediumState state;
    COMResult result;
    QString toolTip;
};

/**
 * Media descriptor for the GUI.
 *
 * Maintains the results of the last state (accessibility) check and precomposes
 * string parameters such as location, size which can be used in various GUI
 * controls.
 *
 * Many getter methods take the boolean @a aNoDiffs argument. Unless explicitly
 * stated otherwise, this argument, when set to @c true, will cause the
 * corresponding property of this object's root medium to be returned instead of
 * its own one. This is useful when hard disk media is represented in the
 * user-friendly "don't show diffs" mode. For non-hard disk media, the value of
 * this argument is irrelevant because the root object for such medium is
 * the medium itself.
 *
 * Note that this class "abuses" the KMediumState_NotCreated state value to
 * indicate that the accessibility check of the given medium (see
 * #blockAndQueryState()) has not been done yet and therefore some parameters
 * such as #size() are meaningless because they can be read only from the
 * accessible medium. The real KMediumState_NotCreated state is not necessary
 * because this class is only used with created (existing) media.
 */
class UIMedium
{
public:

    /**
     * Creates a null medium descriptor which is not associated with any medium.
     * The state field is set to KMediumState_NotCreated.
     */
    UIMedium()
        : m_type(UIMediumType_Invalid)
        , m_state(KMediumState_NotCreated)
        , m_fReadOnly(false)
        , m_fUsedInSnapshots(false)
        , m_pParent(0) { refresh(); }

    /**
     * Creates a media descriptor associated with the given medium.
     *
     * The state field remain KMediumState_NotCreated until #blockAndQueryState()
     * is called. All precomposed strings are filled up by implicitly calling
     * #refresh(), see the #refresh() details for more info.
     *
     * One of the hardDisk, dvdImage, or floppyImage members is assigned from
     * medium according to type. @a pParent must be always NULL for non-hard
     * disk media.
     */
    UIMedium(const CMedium &medium, UIMediumType type, UIMedium *pParent = 0)
        : m_medium(medium)
        , m_type(type)
        , m_state(KMediumState_NotCreated)
        , m_fReadOnly(false)
        , m_fUsedInSnapshots(false)
        , m_pParent(pParent) { refresh(); }

    /**
     * Similar to the other non-null constructor but sets the media state to
     * @a state. Suitable when the media state is known such as right after
     * creation.
     */
    UIMedium(const CMedium &medium, UIMediumType type, KMediumState state)
        : m_medium(medium)
        , m_type(type)
        , m_state(state)
        , m_fReadOnly(false)
        , m_fUsedInSnapshots(false)
        , m_pParent(0) { refresh(); }

    UIMedium& operator=(const UIMedium &other);

    void blockAndQueryState();
    void refresh();

    bool isHidden() const { return m_fHidden | m_fAttachedToHiddenMachinesOnly; }

    const CMedium &medium() const { return m_medium; }

    UIMediumType type() const { return m_type; }

    /**
     * Media state. In "don't show diffs" mode, this is the worst state (in
     * terms of inaccessibility) detected on the given hard disk chain.
     *
     * @param fNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
     */
    KMediumState state(bool fNoDiffs = false) const
    {
        unconst (this)->checkNoDiffs(fNoDiffs);
        return fNoDiffs ? m_noDiffs.state : m_state;
    }

    QString lastAccessError() const { return m_strLastAccessError; }

    /**
     * Result of the last blockAndQueryState() call. Will indicate an error and
     * contain a proper error info if the last state check fails. In "don't show
     * diffs" mode, this is the worst result (in terms of inaccessibility)
     * detected on the given hard disk chain.
     *
     * @param fNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
     */
    const COMResult &result(bool fNoDiffs = false) const
    {
        unconst(this)->checkNoDiffs(fNoDiffs);
        return fNoDiffs ? m_noDiffs.result : m_result;
    }

    QString id() const { return m_strId; }
    QString name(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strName : m_strName; }
    QString location(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strLocation : m_strLocation; }

    QString size(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strSize : m_strSize; }
    QString logicalSize(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strLogicalSize : m_strLogicalSize; }

    QString hardDiskFormat(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strHardDiskFormat : m_strHardDiskFormat; }
    QString hardDiskType(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strHardDiskType : m_strHardDiskType; }

    QString storageDetails() const { return m_strStorageDetails; }

    QString usage(bool fNoDiffs = false) const { return fNoDiffs ? root().m_strUsage : m_strUsage; }
    QString tip() const { return m_strToolTip; }

    const NoDiffsCache& cache() const { return m_noDiffs; }

    /**
     * Returns @c true if this medium is read-only (either because it is
     * Immutable or because it has child hard disks). Read-only media can only
     * be attached indirectly.
     */
    bool isReadOnly() const { return m_fReadOnly; }

    /**
     * Returns @c true if this medium is attached to any VM (in the current
     * state or in a snapshot) in which case #usage() will contain a string with
     * comma-separated VM names (with snapshot names, if any, in parenthesis).
     */
    bool isUsed() const { return !m_strUsage.isNull(); }

    /**
     * Returns @c true if this medium is attached to any VM in any snapshot.
     */
    bool isUsedInSnapshots() const { return m_fUsedInSnapshots; }

    /**
     * Returns @c true if this medium corresponds to real host drive.
     */
    bool isHostDrive() const { return m_fHostDrive; }

    /**
     * Returns @c true if this medium is attached to the given machine in the current state.
     */
    bool isAttachedInCurStateTo(const QString &strMachineId) const { return m_curStateMachineIds.indexOf(strMachineId) >= 0; }

    /**
     * Returns a vector of IDs of all machines this medium is attached
     * to in their current state (i.e. excluding snapshots).
     */
    const QList <QString> &curStateMachineIds() const { return m_curStateMachineIds; }

    /**
     * Returns a parent medium. For non-hard disk media, this is always NULL.
     */
    UIMedium* parent() const { return m_pParent; }

    UIMedium& root() const;

    QString toolTip(bool fNoDiffs = false, bool fCheckRO = false, bool fNullAllowed = false) const;
    QPixmap icon(bool fNoDiffs = false, bool fCheckRO = false) const;

    /** Shortcut to <tt>#toolTip(fNoDiffs, true)</tt>. */
    QString toolTipCheckRO(bool fNoDiffs = false, bool fNullAllowed = false) const { return toolTip(fNoDiffs, true, fNullAllowed); }

    /** Shortcut to <tt>#icon(fNoDiffs, true)</tt>. */
    QPixmap iconCheckRO(bool fNoDiffs = false) const { return icon(fNoDiffs, true); }

    QString details(bool fNoDiffs = false, bool fPredictDiff = false, bool aUseHTML = false) const;

    /** Shortcut to <tt>#details(fNoDiffs, fPredictDiff, true)</tt>. */
    QString detailsHTML(bool fNoDiffs = false, bool fPredictDiff = false) const { return details(fNoDiffs, fPredictDiff, true); }

    /** Returns @c true if this media descriptor is a null object. */
    bool isNull() const { return m_medium.isNull(); }

private:

    void checkNoDiffs(bool fNoDiffs);

    CMedium m_medium;

    UIMediumType m_type;

    KMediumState m_state;
    QString m_strLastAccessError;
    COMResult m_result;

    QString m_strId;
    QString m_strName;
    QString m_strLocation;

    QString m_strSize;
    QString m_strLogicalSize;

    QString m_strHardDiskFormat;
    QString m_strHardDiskType;

    QString m_strStorageDetails;

    QString m_strUsage;
    QString m_strToolTip;

    bool m_fReadOnly                     : 1;
    bool m_fUsedInSnapshots              : 1;
    bool m_fHostDrive                    : 1;
    bool m_fHidden                       : 1;
    bool m_fAttachedToHiddenMachinesOnly : 1;

    QList<QString> m_curStateMachineIds;

    UIMedium *m_pParent;

    NoDiffsCache m_noDiffs;

    static QString m_sstrTable;
    static QString m_sstrRow;
};

typedef QLinkedList<UIMedium> VBoxMediaList;

#endif /* __UIMedium_h__ */
