/* $Id$ */
/** @file
 * VBox Qt GUI - Update routine related declarations.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h
#define FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QDate>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIVersion.h"

/* COM includes: */
#include "COMEnums.h"


/** Update period types. */
enum UpdatePeriodType
{
    UpdatePeriodType_Never  = -1,
    UpdatePeriodType_1Day   =  0,
    UpdatePeriodType_2Days  =  1,
    UpdatePeriodType_3Days  =  2,
    UpdatePeriodType_4Days  =  3,
    UpdatePeriodType_5Days  =  4,
    UpdatePeriodType_6Days  =  5,
    UpdatePeriodType_1Week  =  6,
    UpdatePeriodType_2Weeks =  7,
    UpdatePeriodType_3Weeks =  8,
    UpdatePeriodType_1Month =  9
};


/** Structure to store retranslated period type values. */
struct VBoxUpdateDay
{
    VBoxUpdateDay(const QString &strVal, const QString &strKey)
        : val(strVal)
        , key(strKey)
    {}

    bool operator==(const VBoxUpdateDay &other) const
    {
        return    val == other.val
               || key == other.key;
    }

    QString  val;
    QString  key;
};
typedef QList<VBoxUpdateDay> VBoxUpdateDayList;


/** Class used to encode/decode update data. */
class SHARED_LIBRARY_STUFF VBoxUpdateData
{
public:

    /** Populates a set of update options. */
    static void populate();
    /** Returns a list of update options. */
    static QStringList list();

    /** Constructs update description on the basis of passed @a strData. */
    VBoxUpdateData(const QString &strData = QString());
    /** Constructs update description on the basis of passed @a fCheckEnabled, @a enmUpdatePeriod and @a enmUpdateChannel. */
    VBoxUpdateData(bool fCheckEnabled, UpdatePeriodType enmUpdatePeriod, KUpdateChannel enmUpdateChannel);

    /** Returns whether check is enabled. */
    bool isCheckEnabled() const;
    /** Returns whether check is required. */
    bool isCheckRequired() const;

    /** Returns update data. */
    QString data() const;

    /** Returns update period. */
    UpdatePeriodType updatePeriod() const;
    /** Returns update date. */
    QDate date() const;
    /** Returns update date as string. */
    QString dateToString() const;
    /** Returns update channel. */
    KUpdateChannel updateChannel() const;
    /** Returns update channel name. */
    QString updateChannelName() const;
    /** Returns version. */
    UIVersion version() const;

    /** Returns whether this item equals to @a another one. */
    bool isEqual(const VBoxUpdateData &another) const;
    /** Returns whether this item equals to @a another one. */
    bool operator==(const VBoxUpdateData &another) const;
    /** Returns whether this item isn't equal to @a another one. */
    bool operator!=(const VBoxUpdateData &another) const;

    /** Converts passed @a enmUpdateChannel to internal QString value.
      * @note This isn't a member of UIConverter since it's used for
      *       legacy extra-data settings saving routine only. */
    static QString updateChannelToInternalString(KUpdateChannel enmUpdateChannel);
    /** Converts passed @a strUpdateChannel to KUpdateChannel value.
      * @note This isn't a member of UIConverter since it's used for
      *       legacy extra-data settings saving routine only. */
    static KUpdateChannel updateChannelFromInternalString(const QString &strUpdateChannel);

private:

    /** Holds the populated list of update period options. */
    static VBoxUpdateDayList s_days;

    /** Holds the update data. */
    QString  m_strData;

    /** Holds whether check is enabled. */
    bool  m_fCheckEnabled;
    /** Holds whether it's need to check for update. */
    bool  m_fCheckRequired;

    /** Holds the update period. */
    UpdatePeriodType      m_enmUpdatePeriod;
    /** Holds the update date. */
    QDate           m_date;
    /** Holds the update channel. */
    KUpdateChannel  m_enmUpdateChannel;
    /** Holds the update version. */
    UIVersion       m_version;
};


#endif /* !FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h */
