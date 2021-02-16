/* $Id$ */
/** @file
 * TM - Common Inlined functions.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_TMInline_h
#define VMM_INCLUDED_SRC_include_TMInline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/**
 * Used to unlink a timer from the active list.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pQueue      The timer queue.
 * @param   pTimer      The timer that needs linking.
 *
 * @remarks Called while owning the relevant queue lock.
 */
DECL_FORCE_INLINE(void) tmTimerQueueUnlinkActive(PVMCC pVM, PTMTIMERQUEUE pQueue, PTMTIMER pTimer)
{
#ifdef VBOX_STRICT
    TMTIMERSTATE const enmState = pTimer->enmState;
    Assert(  pTimer->enmClock == TMCLOCK_VIRTUAL_SYNC
           ? enmState == TMTIMERSTATE_ACTIVE
           : enmState == TMTIMERSTATE_PENDING_SCHEDULE || enmState == TMTIMERSTATE_PENDING_STOP_SCHEDULE);
#endif
    RT_NOREF(pVM);

    const PTMTIMER pPrev = TMTIMER_GET_PREV(pTimer);
    const PTMTIMER pNext = TMTIMER_GET_NEXT(pTimer);
    if (pPrev)
        TMTIMER_SET_NEXT(pPrev, pNext);
    else
    {
        TMTIMER_SET_HEAD(pQueue, pNext);
        pQueue->u64Expire = pNext ? pNext->u64Expire : INT64_MAX;
        DBGFTRACE_U64_TAG(pVM, pQueue->u64Expire, "tmTimerQueueUnlinkActive");
    }
    if (pNext)
        TMTIMER_SET_PREV(pNext, pPrev);
    pTimer->offNext = 0;
    pTimer->offPrev = 0;
}


/** @def TMTIMER_HANDLE_TO_PTR_RETURN_EX
 * Converts a timer handle to a timer pointer, returning @a a_rcRet if the
 * handle is invalid.
 *
 * @param   a_pVM       The cross context VM structure.
 * @param   a_hTimer    The timer handle to translate.
 * @param   a_rcRet     What to return on failure.
 * @param   a_pTimerVar The timer variable to assign the resulting pointer to.
 */
#ifdef IN_RING3
# define TMTIMER_HANDLE_TO_PTR_RETURN_EX(a_pVM, a_hTimer, a_rcRet, a_pTimerVar) do { \
        RT_NOREF(a_pVM); \
        (a_pTimerVar) = (PTMTIMER)hTimer; \
        AssertPtrReturn((a_pTimerVar), a_rcRet); \
        AssertReturn((a_pTimerVar)->hSelf == a_hTimer, a_rcRet); \
    } while (0)
#else
# define TMTIMER_HANDLE_TO_PTR_RETURN_EX(a_pVM, a_hTimer, a_rcRet, a_pTimerVar) do { \
        (a_pTimerVar) = (PTMTIMER)MMHyperR3ToCC(pVM, (RTR3PTR)hTimer); \
        AssertPtrReturn((a_pTimerVar), a_rcRet); \
        AssertReturn((a_pTimerVar)->hSelf == a_hTimer, a_rcRet); \
        Assert((a_pTimerVar)->fFlags & TMTIMER_FLAGS_RING0); \
    } while (0)
#endif

/** @def TMTIMER_HANDLE_TO_PTR_RETURN
 * Converts a timer handle to a timer pointer, returning VERR_INVALID_HANDLE if
 * invalid.
 *
 * @param   a_pVM       The cross context VM structure.
 * @param   a_hTimer    The timer handle to translate.
 * @param   a_pTimerVar The timer variable to assign the resulting pointer to.
 */
#define TMTIMER_HANDLE_TO_PTR_RETURN(a_pVM, a_hTimer, a_pTimerVar) \
        TMTIMER_HANDLE_TO_PTR_RETURN_EX(a_pVM, a_hTimer, VERR_INVALID_HANDLE, a_pTimerVar)

/** @def TMTIMER_HANDLE_TO_PTR_RETURN_VOID
 * Converts a timer handle to a timer pointer, returning VERR_INVALID_HANDLE if
 * invalid.
 *
 * @param   a_pVM       The cross context VM structure.
 * @param   a_hTimer    The timer handle to translate.
 * @param   a_pTimerVar The timer variable to assign the resulting pointer to.
 */
#ifdef IN_RING3
# define TMTIMER_HANDLE_TO_PTR_RETURN_VOID(a_pVM, a_hTimer, a_pTimerVar) do { \
        RT_NOREF(a_pVM); \
        (a_pTimerVar) = (PTMTIMER)hTimer; \
        AssertPtrReturnVoid((a_pTimerVar)); \
        AssertReturnVoid((a_pTimerVar)->hSelf == a_hTimer); \
    } while (0)
#else
# define TMTIMER_HANDLE_TO_PTR_RETURN_VOID(a_pVM, a_hTimer, a_pTimerVar) do { \
        (a_pTimerVar) = (PTMTIMER)MMHyperR3ToCC(pVM, (RTR3PTR)hTimer); \
        AssertPtrReturnVoid((a_pTimerVar)); \
        AssertReturnVoid((a_pTimerVar)->hSelf == a_hTimer); \
        Assert((a_pTimerVar)->fFlags & TMTIMER_FLAGS_RING0); \
    } while (0)
#endif


#endif /* !VMM_INCLUDED_SRC_include_TMInline_h */

