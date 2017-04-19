/* $Id$ */
/** @file
 * VBox Qt GUI - Header with definitions and functions related to settings configuration.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISettingsDefs_h___
#define ___UISettingsDefs_h___

/* Qt includes: */
#include <QMap>
#include <QPair>
#include <QString>

/* COM includes: */
#include "COMEnums.h"


/** Settings configuration namespace. */
namespace UISettingsDefs
{
    /** Configuration access levels. */
    enum ConfigurationAccessLevel
    {
        /** Configuration is not accessible. */
        ConfigurationAccessLevel_Null,
        /** Configuration is accessible fully. */
        ConfigurationAccessLevel_Full,
        /** Configuration is accessible partially, machine is in @a powered_off state. */
        ConfigurationAccessLevel_Partial_PoweredOff,
        /** Configuration is accessible partially, machine is in @a saved state. */
        ConfigurationAccessLevel_Partial_Saved,
        /** Configuration is accessible partially, machine is in @a running state. */
        ConfigurationAccessLevel_Partial_Running,
    };

    /** Determines configuration access level for passed @a sessionState and @a machineState. */
    ConfigurationAccessLevel configurationAccessLevel(KSessionState sessionState, KMachineState machineState);
}


/** Template organizing settings object cache: */
template <class CacheData> class UISettingsCache
{
public:

    /** Constructs empty object cache. */
    UISettingsCache() { m_value = qMakePair(CacheData(), CacheData()); }

    /** Destructs cache object. */
    virtual ~UISettingsCache() { /* Makes MSC happy */ }

    /** Returns the NON-modifiable REFERENCE to the initial cached data. */
    const CacheData &base() const { return m_value.first; }
    /** Returns the NON-modifiable REFERENCE to the current cached data. */
    const CacheData &data() const { return m_value.second; }
    /** Returns the modifiable REFERENCE to the current cached data. */
    CacheData &data() { return m_value.second; }

    /** Returns whether the cached object was removed.
      * We assume that cached object was removed if
      * initial data was set but current data was NOT set. */
    virtual bool wasRemoved() const { return base() != CacheData() && data() == CacheData(); }

    /** Returns whether the cached object was created.
      * We assume that cached object was created if
      * initial data was NOT set but current data was set. */
    virtual bool wasCreated() const { return base() == CacheData() && data() != CacheData(); }

    /** Returns whether the cached object was updated.
      * We assume that cached object was updated if
      * current and initial data were both set and not equal to each other. */
    virtual bool wasUpdated() const { return base() != CacheData() && data() != CacheData() && data() != base(); }

    /** Returns whether the cached object was changed.
      * We assume that cached object was changed if
      * it was 1. removed, 2. created or 3. updated. */
    virtual bool wasChanged() const { return wasRemoved() || wasCreated() || wasUpdated(); }

    /** Defines initial cached object data. */
    void cacheInitialData(const CacheData &initialData) { m_value.first = initialData; }
    /** Defines current cached object data: */
    void cacheCurrentData(const CacheData &currentData) { m_value.second = currentData; }

    /** Resets the initial and the current object data to be both empty. */
    void clear() { m_value.first = CacheData(); m_value.second = CacheData(); }

private:

    /** Holds the cached object data. */
    QPair<CacheData, CacheData> m_value;
};


/** Template organizing settings object cache with children. */
template <class ParentCacheData, class ChildCacheData> class UISettingsCachePool : public UISettingsCache<ParentCacheData>
{
public:

    /** Children map. */
    typedef QMap<QString, ChildCacheData> UISettingsCacheChildMap;
    /** Children map iterator. */
    typedef QMapIterator<QString, ChildCacheData> UISettingsCacheChildIterator;

    /** Constructs empty object cache. */
    UISettingsCachePool() : UISettingsCache<ParentCacheData>() {}

    /** Returns children count. */
    int childCount() const { return m_children.size(); }
    /** Returns the modifiable REFERENCE to the child cached data. */
    ChildCacheData& child(const QString &strChildKey) { return m_children[strChildKey]; }
    /** Wraps method above to return the modifiable REFERENCE to the child cached data. */
    ChildCacheData& child(int iIndex) { return child(indexToKey(iIndex)); }
    /** Returns the NON-modifiable COPY to the child cached data. */
    const ChildCacheData child(const QString &strChildKey) const { return m_children[strChildKey]; }
    /** Wraps method above to return the NON-modifiable COPY to the child cached data. */
    const ChildCacheData child(int iIndex) const { return child(indexToKey(iIndex)); }

    /** Returns whether the cache was updated.
      * We assume that cache object was updated if current and
      * initial data were both set and not equal to each other.
      * Takes into account all the children. */
    bool wasUpdated() const
    {
        /* First of all, cache object is considered to be updated if parent data was updated: */
        bool fWasUpdated = UISettingsCache<ParentCacheData>::wasUpdated();
        /* If parent data was NOT updated but also was NOT created or removed too
         * (e.j. was NOT changed at all), we have to check children too: */
        if (!fWasUpdated && !UISettingsCache<ParentCacheData>::wasRemoved() && !UISettingsCache<ParentCacheData>::wasCreated())
        {
            for (int iChildIndex = 0; !fWasUpdated && iChildIndex < childCount(); ++iChildIndex)
                if (child(iChildIndex).wasChanged())
                    fWasUpdated = true;
        }
        return fWasUpdated;
    }

    /** Resets the initial and the current one data to be both empty.
      * Removes all the children. */
    void clear()
    {
        UISettingsCache<ParentCacheData>::clear();
        m_children.clear();
    }

private:

    /** Returns QString representation of passed @a iIndex. */
    QString indexToKey(int iIndex) const
    {
        UISettingsCacheChildIterator childIterator(m_children);
        for (int iChildIndex = 0; childIterator.hasNext(); ++iChildIndex)
        {
            childIterator.next();
            if (iChildIndex == iIndex)
                return childIterator.key();
        }
        return QString("%1").arg(iIndex, 8 /* up to 8 digits */, 10 /* base */, QChar('0') /* filler */);
    }

    /** Holds the children. */
    UISettingsCacheChildMap m_children;
};

#endif /* !___UISettingsDefs_h___ */

