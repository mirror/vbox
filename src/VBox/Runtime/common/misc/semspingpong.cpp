/* $Id$ */
/** @file
 * innotek Portable Runtime - Thread Ping-Pong Construct.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>



/**
 * Validates a pPP handle passed to one of the PP functions.
 *
 * @returns true if valid, false if invalid.
 * @param   pPP     Pointe to the ping-pong structure.
 */
inline bool rtsemPPValid(PRTPINGPONG  pPP)
{
    if (!VALID_PTR(pPP))
        return false;

    RTPINGPONGSPEAKER enmSpeaker = pPP->enmSpeaker;
    if (    enmSpeaker != RTPINGPONGSPEAKER_PING
        &&  enmSpeaker != RTPINGPONGSPEAKER_PONG
        &&  enmSpeaker != RTPINGPONGSPEAKER_PONG_SIGNALED
        &&  enmSpeaker != RTPINGPONGSPEAKER_PING_SIGNALED)
        return false;

    return true;
}

/**
 * Init a Ping-Pong construct.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure which needs initialization.
 */
RTR3DECL(int) RTSemPingPongInit(PRTPINGPONG pPP)
{
    /*
     * Init the structure.
     */
    pPP->enmSpeaker = RTPINGPONGSPEAKER_PING;

    int rc = RTSemEventCreate(&pPP->Ping);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pPP->Pong);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        RTSemEventDestroy(pPP->Ping);
    }

    return rc;
}


/**
 * Destroys a Ping-Pong construct.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure which is to be destroyed.
 *                      (I.e. put into uninitialized state.)
 */
RTR3DECL(int) RTSemPingPongDestroy(PRTPINGPONG pPP)
{
    /*
     * Validate input
     */
    if (!rtsemPPValid(pPP))
    {
        AssertMsgFailed(("Invalid input %p\n", pPP));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Invalidate the ping pong handle and destroy the event semaphores.
     */
    ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_UNINITIALIZE);
    int rc = RTSemEventDestroy(pPP->Ping);
    int rc2 = RTSemEventDestroy(pPP->Pong);
    AssertRC(rc);
    AssertRC(rc2);

    return VINF_SUCCESS;
}


/**
 * Signals the pong thread in a ping-pong construct. (I.e. sends ping.)
 * This is called by the ping thread.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure to ping.
 */
RTR3DECL(int) RTSemPing(PRTPINGPONG pPP)
{
    /*
     * Validate input
     */
    if (!rtsemPPValid(pPP))
    {
        AssertMsgFailed(("Invalid input %p\n", pPP));
        return VERR_INVALID_PARAMETER;
    }

    if (pPP->enmSpeaker != RTPINGPONGSPEAKER_PING)
    {
        AssertMsgFailed(("Speaking out of turn!\n"));
        return VERR_SEM_OUT_OF_TURN;
    }

    /*
     * Signal the other thread.
     */
    ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PONG_SIGNALED);
    int rc = RTSemEventSignal(pPP->Pong);
    if (RT_SUCCESS(rc))
        return rc;

    /* restore the state. */
    AssertMsgFailed(("Failed to signal pong sem %x. rc=%d\n",  pPP->Pong,  rc));
    ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PING);
    return rc;
}


/**
 * Signals the ping thread in a ping-pong construct. (I.e. sends pong.)
 * This is called by the pong thread.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure to pong.
 */
RTR3DECL(int) RTSemPong(PRTPINGPONG pPP)
{
    /*
     * Validate input
     */
    if (!rtsemPPValid(pPP))
    {
        AssertMsgFailed(("Invalid input %p\n", pPP));
        return VERR_INVALID_PARAMETER;
    }

    if (pPP->enmSpeaker != RTPINGPONGSPEAKER_PONG)
    {
        AssertMsgFailed(("Speaking out of turn!\n"));
        return VERR_SEM_OUT_OF_TURN;
    }

    /*
     * Signal the other thread.
     */
    ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PING_SIGNALED);
    int rc = RTSemEventSignal(pPP->Ping);
    if (RT_SUCCESS(rc))
        return rc;

    /* restore the state. */
    AssertMsgFailed(("Failed to signal ping sem %x. rc=%d\n",  pPP->Ping,  rc));
    ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PONG);
    return rc;
}


/**
 * Wait function for the ping thread.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   pPP         Pointer to the ping-pong structure to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTR3DECL(int) RTSemPingWait(PRTPINGPONG pPP, unsigned cMillies)
{
    /*
     * Validate input
     */
    if (!rtsemPPValid(pPP))
    {
        AssertMsgFailed(("Invalid input %p\n", pPP));
        return VERR_INVALID_PARAMETER;
    }

    if (    pPP->enmSpeaker != RTPINGPONGSPEAKER_PONG
        &&  pPP->enmSpeaker != RTPINGPONGSPEAKER_PONG_SIGNALED
        &&  pPP->enmSpeaker != RTPINGPONGSPEAKER_PING_SIGNALED)
    {
        AssertMsgFailed(("Listening out of turn! enmSpeaker=%d\n", pPP->enmSpeaker));
        return VERR_SEM_OUT_OF_TURN;
    }

    /*
     * Wait.
     */
    int rc = RTSemEventWait(pPP->Ping, cMillies);
    if (RT_SUCCESS(rc))
        ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PING);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Wait function for the pong thread.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   pPP         Pointer to the ping-pong structure to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTR3DECL(int) RTSemPongWait(PRTPINGPONG pPP, unsigned cMillies)
{
    /*
     * Validate input
     */
    if (!rtsemPPValid(pPP))
    {
        AssertMsgFailed(("Invalid input %p\n", pPP));
        return VERR_INVALID_PARAMETER;
    }

    if (    pPP->enmSpeaker != RTPINGPONGSPEAKER_PING
        &&  pPP->enmSpeaker != RTPINGPONGSPEAKER_PING_SIGNALED
        &&  pPP->enmSpeaker != RTPINGPONGSPEAKER_PONG_SIGNALED)
    {
        AssertMsgFailed(("Listening out of turn! enmSpeaker=%d\n", pPP->enmSpeaker));
        return VERR_SEM_OUT_OF_TURN;
    }

    /*
     * Wait.
     */
    int rc = RTSemEventWait(pPP->Pong, cMillies);
    if (RT_SUCCESS(rc))
        ASMAtomicXchgSize(&pPP->enmSpeaker, RTPINGPONGSPEAKER_PONG);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}

