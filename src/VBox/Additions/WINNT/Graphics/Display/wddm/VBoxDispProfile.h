/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxDispProfile_h__
#define ___VBoxDispProfile_h__
#include <iprt/ctype.h>
#include <iprt/time.h>

#include "VBoxDispD3DCmn.h"

#define VBOXDISPPROFILE_MAX_SETSIZE 512
#define VBOXDISPPROFILE_GET_TIME_NANO() RTTimeNanoTS()
#define VBOXDISPPROFILE_GET_TIME_MILLI() RTTimeMilliTS()
#define VBOXDISPPROFILE_DUMP(_m) do {\
        vboxVDbgDoMpPrintF _m; \
    } while (0)

class VBoxDispProfileEntry
{
public:
    VBoxDispProfileEntry() :
        m_cCalls(0),
        m_cTime(0),
        m_pName(NULL)
    {}

    VBoxDispProfileEntry(const char *pName) :
        m_cCalls(0),
        m_cTime(0),
        m_pName(pName)
    {}

    void step(uint64_t cTime)
    {
        ++m_cCalls;
        m_cTime+= cTime;
    }

    void reset()
    {
        m_cCalls = 0;
        m_cTime = 0;
    }

    void dump(const PVBOXWDDMDISP_DEVICE pDevice) const
    {
//        VBOXDISPPROFILE_DUMP((pDevice, "Entry '%s': calls(%d), time: nanos(%I64u), micros(%I64u), millis(%I64u)\n",
//                m_pName, m_cCalls,
//                m_cTime, m_cTime/1000, m_cTime/1000000));
        VBOXDISPPROFILE_DUMP((pDevice, "%s\t%d\t%I64u\t%I64u\t%I64u\n",
                m_pName, m_cCalls,
                m_cTime, m_cTime/1000, m_cTime/1000000));
    }
private:
    uint32_t m_cCalls;
    uint64_t m_cTime;
    const char * m_pName;
};

class VBoxDispProfileSet
{
public:
    VBoxDispProfileSet(const char *pName) :
        m_cEntries(0),
        m_pName(pName)
    {}

    VBoxDispProfileEntry * alloc(const char *pName)
    {
        if (m_cEntries < RT_ELEMENTS(m_Entries))
        {
            VBoxDispProfileEntry * entry = &m_Entries[m_cEntries];
            ++m_cEntries;
            *entry = VBoxDispProfileEntry(pName);
            return entry;
        }
        return NULL;
    }

    void resetEntries()
    {
        for (uint32_t i = 0; i < m_cEntries; ++i)
        {
            m_Entries[i].reset();
        }
    }

    void dump(const PVBOXWDDMDISP_DEVICE pDevice)
    {
        VBOXDISPPROFILE_DUMP((pDevice, ">>>> Start of VBox Disp Dump '%s': num entries(%d) >>>>>\n", m_pName, m_cEntries));
        VBOXDISPPROFILE_DUMP((pDevice, "Name\tCalls\tNanos\tMicros\tMillis\n"));
        for (uint32_t i = 0; i < m_cEntries; ++i)
        {
            m_Entries[i].dump(pDevice);
        }
        VBOXDISPPROFILE_DUMP((pDevice, "<<<< Endi of VBox Disp Dump '%s' <<<<<\n", m_pName));
    }

private:
    VBoxDispProfileEntry m_Entries[VBOXDISPPROFILE_MAX_SETSIZE];
    uint32_t m_cEntries;
    const char * m_pName;
};

class VBoxDispProfileScopeLogger
{
public:
    VBoxDispProfileScopeLogger(VBoxDispProfileEntry *pEntry) :
        m_pEntry(pEntry),
        m_bDisable(FALSE)
    {
        m_cTime = VBOXDISPPROFILE_GET_TIME_NANO();
    }

    ~VBoxDispProfileScopeLogger()
    {
        if (!m_bDisable)
        {
            uint64_t cNewTime = VBOXDISPPROFILE_GET_TIME_NANO();
            m_pEntry->step(cNewTime - m_cTime);
        }
    }

    void disable()
    {
        m_bDisable = TRUE;
    }
private:
    VBoxDispProfileEntry *m_pEntry;
    uint64_t m_cTime;
    BOOL m_bDisable;
};

#define VBOXDISPPROFILE_FUNCTION_LOGGER_DISABLE_CURRENT() do { \
        __vboxDispProfileLogger.disable();\
    } while (0)

#define VBOXDISPPROFILE_FUNCTION_LOGGER_DEFINE(_p)  \
        static VBoxDispProfileEntry * __pVBoxDispProfileEntry = NULL; \
        if (!__pVBoxDispProfileEntry) { __pVBoxDispProfileEntry = _p.alloc(__FUNCTION__); } \
        VBoxDispProfileScopeLogger __vboxDispProfileLogger(__pVBoxDispProfileEntry);


#define VBOXDISPPROFILE_FUNCTION_PROLOGUE(_p) \
        VBOXDISPPROFILE_FUNCTION_LOGGER_DEFINE(_p)


#endif /* #ifndef ___VBoxDispProfile_h__ */
