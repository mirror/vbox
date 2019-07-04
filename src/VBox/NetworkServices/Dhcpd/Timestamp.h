/* $Id$ */
/** @file
 * DHCP server - timestamps
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Dhcpd_TimeStamp_h
#define VBOX_INCLUDED_SRC_Dhcpd_TimeStamp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/time.h>


/**
 * Timestamp API uses unsigned time, but we need to be able to refer
 * to events in the past.  Hide the ugly convertions.
 *
 * @todo r=bird: Unnecessary mixing of RTTimeNanoTS and RTTimeNow.
 */
class Timestamp
{
    int64_t m_ns;

public:
    Timestamp()
      : m_ns(0)
    {}

    Timestamp(uint64_t ns)
      : m_ns(static_cast<int64_t>(ns))
    {}

    static Timestamp now()
    {
        return Timestamp(RTTimeNanoTS());
    }

    static Timestamp absSeconds(int64_t sec)
    {
        RTTIMESPEC delta;
        RTTimeNow(&delta);
        RTTimeSpecSubSeconds(&delta, sec);

        uint64_t stampNow = RTTimeNanoTS();
        return Timestamp(stampNow - RTTimeSpecGetNano(&delta));
    }

    Timestamp &addSeconds(int64_t cSecs)
    {
        m_ns += cSecs * RT_NS_1SEC;
        return *this;
    }

    Timestamp &subSeconds(int64_t cSecs)
    {
        m_ns -= cSecs * RT_NS_1SEC;
        return *this;
    }


    RTTIMESPEC *getAbsTimeSpec(RTTIMESPEC *pTime) const
    {
        RTTimeNow(pTime);

        uint64_t stampNow = RTTimeNanoTS();
        uint64_t delta = stampNow - m_ns;
        RTTimeSpecSubNano(pTime, delta);
        return pTime;
    }

    int64_t getAbsSeconds() const
    {
        RTTIMESPEC time;
        return RTTimeSpecGetSeconds(getAbsTimeSpec(&time));
    }

    size_t absStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput) const;

    friend bool operator<(const Timestamp &l, const Timestamp &r);
    friend bool operator>(const Timestamp &l, const Timestamp &r);
    friend bool operator<=(const Timestamp &l, const Timestamp &r);
    friend bool operator>=(const Timestamp &l, const Timestamp &r);
};


inline bool operator<(const Timestamp &l, const Timestamp &r) { return l.m_ns < r.m_ns; }
inline bool operator>(const Timestamp &l, const Timestamp &r) { return l.m_ns > r.m_ns; }
inline bool operator<=(const Timestamp &l, const Timestamp &r) { return l.m_ns <= r.m_ns; }
inline bool operator>=(const Timestamp &l, const Timestamp &r) { return l.m_ns >= r.m_ns; }

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_TimeStamp_h */
