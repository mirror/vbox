/* $Id$ */

/** @file
 *
 * Context ID makeup/extraction test cases.
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

#define LOG_ENABLED
#define LOG_GROUP LOG_GROUP_MAIN
#define LOG_INSTANCE NULL
#include <VBox/log.h>

#include "../include/GuestCtrlImplPrivate.h"

using namespace com;

#include <iprt/env.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/test.h>

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstMakeup", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTTestIPrintf(RTTESTLVL_DEBUG, "Initializing COM...\n");
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
    {
        RTTestFailed(hTest, "Failed to initialize COM (%Rhrc)!\n", hrc);
        return RTEXITCODE_FAILURE;
    }

    /* Don't let the assertions trigger here
     * -- we rely on the return values in the test(s) below. */
    RTAssertSetQuiet(true);

    uint32_t uContextMax = UINT32_MAX;
    RTTestIPrintf(RTTESTLVL_DEBUG, "Max context is: %RU32\n", uContextMax);

    /* Do 64 tests total. */
    for (int t = 0; t < 64 && !RTTestErrorCount(hTest); t++)
    {
        uint32_t s = RTRandU32Ex(0, VBOX_GUESTCTRL_MAX_SESSIONS);
        uint32_t p = RTRandU32Ex(0, VBOX_GUESTCTRL_MAX_PROCESSES);
        uint32_t c = RTRandU32Ex(0, VBOX_GUESTCTRL_MAX_CONTEXTS);

        uint64_t uContextID = VBOX_GUESTCTRL_CONTEXTID_MAKE(s, p, c);
        RTTestIPrintf(RTTESTLVL_DEBUG, "ContextID (%d,%d,%d) = %RU32\n", s, p, c, uContextID);
        if (s != VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID))
        {
            RTTestFailed(hTest, "%d,%d,%d: Session is %d, expected %d -> %RU64\n",
                         s, p, c, VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID), s, uContextID);
        }
        else if (p != VBOX_GUESTCTRL_CONTEXTID_GET_PROCESS(uContextID))
        {
            RTTestFailed(hTest, "%d,%d,%d: Process is %d, expected %d -> %RU64\n",
                         s, p, c, VBOX_GUESTCTRL_CONTEXTID_GET_PROCESS(uContextID), p, uContextID);
        }
        if (c != VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(uContextID))
        {
            RTTestFailed(hTest, "%d,%d,%d: Count is %d, expected %d -> %RU64\n",
                         s, p, c, VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(uContextID), c, uContextID);
        }
        if (uContextID > UINT32_MAX)
        {
            RTTestFailed(hTest, "%d,%d,%d: Value overflow; does not fit anymore: %RU64\n",
                         s, p, c, uContextID);
        }
    }

#if 0
    #define VBOX_GUESTCTRL_CONTEXTID_MAKE(uSession, uProcess, uCount) \
    (  (uint32_t)((uSession) &   0xff) << 24 \
     | (uint32_t)((uProcess) &   0xff) << 16 \
     | (uint32_t)((uCount)   & 0xffff)       \
    )
/** Gets the session ID out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID) \
    (uContextID) >> 24)
/** Gets the process ID out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_PROCESS(uContextID) \
    (uContextID) >> 16)
/** Gets the conext count of a process out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(uContextID) \
    ((uContextID) && 0xffff)
#endif

    RTTestIPrintf(RTTESTLVL_DEBUG, "Shutting down COM...\n");
    com::Shutdown();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

