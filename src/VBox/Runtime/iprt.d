/* $Id$ */
/** @file
 * IPRT - Static dtrace probes.
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

provider iprt
{
    probe critsect__entered(void *pvCritSect, const char *pszLaterNm, int32_t cLockers, uint32_t cNestings);
    probe critsect__leaving(void *pvCritSect, const char *pszLaterNm, int32_t cLockers, uint32_t cNestings);
    probe critsect__waiting(void *pvCritSect, const char *pszLaterNm, int32_t cLockers, void *pvNativeThreadOwner);
    probe critsect__busy(   void *pvCritSect, const char *pszLaterNm, int32_t cLockers, void *pvNativeThreadOwner);

    probe critsectrw__excl_entered(void *pvCritSect, const char *pszLaterNm, uint32_t cNestings,
                                   uint32_t cWaitingReaders, uint32_t cWriters);
    probe critsectrw__excl_leaving(void *pvCritSect, const char *pszLaterNm, uint32_t cNestings,
                                   uint32_t cWaitingReaders, uint32_t cWriters);
    probe critsectrw__excl_waiting(void *pvCritSect, const char *pszLaterNm, bool a_fWriteMode, uint32_t a_cWaitingReaders,
                                   uint32_t a_cReaders, uint32_t a_cWriters, void *a_pvNativeOwnerThread);
    probe critsectrw__excl_busy(   void *pvCritSect, const char *pszLaterNm, bool a_fWriteMode, uint32_t a_cWaitingReaders,
                                   uint32_t a_cReaders, uint32_t a_cWriters, void *a_pvNativeOwnerThread);
    probe critsectrw__excl_entered_shared(void *pvCritSect, const char *pszLaterNm, uint32_t cNestings,
                                          uint32_t cWaitingReaders, uint32_t cWriters);
    probe critsectrw__excl_leaving_shared(void *pvCritSect, const char *pszLaterNm, uint32_t cNestings,
                                          uint32_t cWaitingReaders, uint32_t cWriters);
    probe critsectrw__shared_entered(void *pvCritSect, const char *pszLaterNm, uint32_t cReaders, uint32_t cNestings);
    probe critsectrw__shared_leaving(void *pvCritSect, const char *pszLaterNm, uint32_t cReaders, uint32_t cNestings);
    probe critsectrw__shared_waiting(void *pvCritSect, const char *pszLaterNm, void *pvNativeThreadOwner,
                                     uint32_t cWaitingReaders, uint32_t cWriters);
    probe critsectrw__shared_busy(   void *pvCritSect, const char *pszLaterNm, void *pvNativeThreadOwner,
                                     uint32_t cWaitingReaders, uint32_t cWriters);

};

#pragma D attributes Evolving/Evolving/Common provider iprt provider
#pragma D attributes Private/Private/Unknown  provider iprt module
#pragma D attributes Private/Private/Unknown  provider iprt function
#pragma D attributes Evolving/Evolving/Common provider iprt name
#pragma D attributes Evolving/Evolving/Common provider iprt args

