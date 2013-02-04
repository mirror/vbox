/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Header with definitions and functions related to settings dialog
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISettingsDefs_h__
#define __UISettingsDefs_h__

/* Qt includes: */
#include <QPair>
#include <QMap>

/* COM includes: */
#include "COMEnums.h"

/* Settings dialog namespace: */
namespace UISettingsDefs
{
    /* Settings dialog types: */
    enum SettingsDialogType
    {
        SettingsDialogType_Wrong,
        SettingsDialogType_Offline,
        SettingsDialogType_Saved,
        SettingsDialogType_Online
    };

    /* Machine state => Settings dialog type converter: */
    SettingsDialogType determineSettingsDialogType(KSessionState sessionState, KMachineState machineState);
}

/* Template to operate settings cache item: */
template <class CacheData> class UISettingsCache
{
public:

    /* Creates empty cache item: */
    UISettingsCache() { m_value = qMakePair(CacheData(), CacheData()); }

    /* Returns the NON-modifiable REFERENCE to the initial cached data: */
    const CacheData& base() const { return m_value.first; }
    /* Returns the NON-modifiable REFERENCE to the current cached data: */
    const CacheData& data() const { return m_value.second; }

    /* We assume that old cache item was removed if
     * initial data was set but current data was NOT set.
     * Returns 'true' if that cache item was removed: */
    virtual bool wasRemoved() const { return base() != CacheData() && data() == CacheData(); }

    /* We assume that new cache item was created if
     * initial data was NOT set but current data was set.
     * Returns 'true' if that cache item was created: */
    virtual bool wasCreated() const { return base() == CacheData() && data() != CacheData(); }

    /* We assume that old cache item was updated if
     * current and initial data were both set and not equal to each other.
     * Returns 'true' if that cache item was updated: */
    virtual bool wasUpdated() const { return base() != CacheData() && data() != CacheData() && data() != base(); }

    /* We assume that old cache item was actually changed if
     * 1. this item was removed or
     * 2. this item was created or
     * 3. this item was updated.
     * Returns 'true' if that cache item was actually changed: */
    virtual bool wasChanged() const { return wasRemoved() || wasCreated() || wasUpdated(); }

    /* Set initial cache item data: */
    void cacheInitialData(const CacheData &initialData) { m_value.first = initialData; }
    /* Set current cache item data: */
    void cacheCurrentData(const CacheData &currentData) { m_value.second = currentData; }

    /* Reset the initial and the current data to be both empty: */
    void clear() { m_value.first = CacheData(); m_value.second = CacheData(); }

private:

    /* Data: */
    QPair<CacheData, CacheData> m_value;
};

/* Template to operate settings cache item with children: */
template <class ParentCacheData, class ChildCacheData> class UISettingsCachePool : public UISettingsCache<ParentCacheData>
{
public:

    /* Typedefs: */
    typedef QMap<QString, ChildCacheData> UISettingsCacheChildMap;
    typedef QMapIterator<QString, ChildCacheData> UISettingsCacheChildIterator;

    /* Creates empty cache item: */
    UISettingsCachePool() : UISettingsCache<ParentCacheData>() {}

    /* Returns the modifiable REFERENCE to the particular child cached data.
     * If child with such key or index is NOT present,
     * both those methods will create the new one to return: */
    ChildCacheData& child(const QString &strChildKey) { return m_children[strChildKey]; }
    ChildCacheData& child(int iIndex) { return child(indexToKey(iIndex)); }

    /* Returns the NON-modifiable COPY to the particular child cached data.
     * If child with such key or index is NOT present,
     * both those methods will create the new one to return: */
    const ChildCacheData child(const QString &strChildKey) const { return m_children[strChildKey]; }
    const ChildCacheData child(int iIndex) const { return child(indexToKey(iIndex)); }

    /* Children count: */
    int childCount() const { return m_children.size(); }

    /* We assume that old cache item was updated if
     * current and initial data were both set and not equal to each other.
     * Takes into account all the children.
     * Returns 'true' if that cache item was updated: */
    bool wasUpdated() const
    {
        /* First of all, cache item is considered to be updated if parent data was updated: */
        bool fWasUpdated = UISettingsCache<ParentCacheData>::wasUpdated();
        /* If parent data was NOT updated but also was NOT created or removed too (e.j. was NOT changed at all),
         * we have to check children too: */
        if (!fWasUpdated && !UISettingsCache<ParentCacheData>::wasRemoved() && !UISettingsCache<ParentCacheData>::wasCreated())
        {
            for (int iChildIndex = 0; !fWasUpdated && iChildIndex < childCount(); ++iChildIndex)
                if (child(iChildIndex).wasChanged())
                    fWasUpdated = true;
        }
        return fWasUpdated;
    }

    /* Reset the initial and the current data to be both empty.
     * Removes all the children: */
    void clear()
    {
        UISettingsCache<ParentCacheData>::clear();
        m_children.clear();
    }

private:

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

    /* Children: */
    UISettingsCacheChildMap m_children;
};

#endif /* __UISettingsDefs_h__ */

