/* $Id$ */
/** @file
 * Crypto thread locking functions which make use of the IPRT.
 */

/*
 * Copyright (C) 2016-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <openssl/crypto.h>
#include "internal/cryptlib.h"

#if defined(OPENSSL_THREADS)

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/errcore.h>
#include <iprt/log.h>
#include <iprt/process.h>

/*
 * Of course it's wrong to use a critical section to implement a read/write
 * lock. But as the OpenSSL interface is too simple (there is only read_lock()/
 * write_lock() and only unspecified unlock() and the Windows implementatio
 * (threads_win.c) uses {Enter,Leave}CriticalSection we do that here as well.
 */
CRYPTO_RWLOCK *CRYPTO_THREAD_lock_new(void)
{
    RTCRITSECT *pCritSect = (RTCRITSECT*)OPENSSL_zalloc(sizeof(RTCRITSECT));
    if (pCritSect)
    {
        int rc = RTCritSectInitEx(pCritSect, 0, NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE, NULL);
        if (RT_SUCCESS(rc))
            return (CRYPTO_RWLOCK*)pCritSect;
            OPENSSL_free(pCritSect);
    }
    return NULL;
}

int CRYPTO_THREAD_read_lock(CRYPTO_RWLOCK *lock)
{
    PRTCRITSECT pCritSect = (PRTCRITSECT)lock;
    int rc = RTCritSectEnter(pCritSect);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

int CRYPTO_THREAD_write_lock(CRYPTO_RWLOCK *lock)
{
    PRTCRITSECT pCritSect = (PRTCRITSECT)lock;
    int rc = RTCritSectEnter(pCritSect);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

int CRYPTO_THREAD_unlock(CRYPTO_RWLOCK *lock)
{
    PRTCRITSECT pCritSect = (PRTCRITSECT)lock;
    int rc = RTCritSectLeave(pCritSect);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

void CRYPTO_THREAD_lock_free(CRYPTO_RWLOCK *lock)
{
    if (lock)
    {
        PRTCRITSECT pCritSect = (PRTCRITSECT)lock;
        int rc = RTCritSectDelete(pCritSect);
        AssertRC(rc);
        OPENSSL_free(lock);
    }
}

int CRYPTO_THREAD_init_local(CRYPTO_THREAD_LOCAL *key, void (*cleanup)(void *))
{
    int rc = RTTlsAllocEx(key, (PFNRTTLSDTOR)cleanup); /* ASSUMES default calling convention is __cdecl, or close enough to it. */
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

void *CRYPTO_THREAD_get_local(CRYPTO_THREAD_LOCAL *key)
{
    return RTTlsGet(*key);
}

int CRYPTO_THREAD_set_local(CRYPTO_THREAD_LOCAL *key, void *val)
{
    int rc = RTTlsSet(*key, val);
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

int CRYPTO_THREAD_cleanup_local(CRYPTO_THREAD_LOCAL *key)
{
    int rc = RTTlsFree(*key);
    if (RT_FAILURE(rc))
        return 0;

    return 1;
}

CRYPTO_THREAD_ID CRYPTO_THREAD_get_current_id(void)
{
    return RTThreadSelf();
}

int CRYPTO_THREAD_compare_id(CRYPTO_THREAD_ID a, CRYPTO_THREAD_ID b)
{
    return (a == b);
}

int CRYPTO_THREAD_run_once(CRYPTO_ONCE *once, void (*init)(void))
{
/** @todo Implement function */
/*    if (*once != 0)
        return 1;

    init();
    *once = 1;
*/
    return 1;
}

int CRYPTO_atomic_add(int *val, int amount, int *ret, CRYPTO_RWLOCK *lock)
{
    *ret = ASMAtomicAddS32((int32_t volatile*)val, amount) + amount;
    return 1;
}

int CRYPTO_atomic_or(uint64_t *val, uint64_t op, uint64_t *ret,
                     CRYPTO_RWLOCK *lock)
{
    uint64_t u64RetOld = ASMAtomicUoReadU64(val);
    uint64_t u64New;
    do
        u64New = u64RetOld | op;
    while (!ASMAtomicCmpXchgExU64(val, u64New, u64RetOld, &u64RetOld));
    *ret = u64RetOld;

    return 1;
}

int CRYPTO_atomic_load(uint64_t *val, uint64_t *ret, CRYPTO_RWLOCK *lock)
{
    *ret = ASMAtomicReadU64((uint64_t volatile *)val);
    return 1;
}

#endif

int openssl_init_fork_handlers(void)
{
    return 0;
}

int openssl_get_fork_id(void)
{
     return (int)RTProcSelf();
}
