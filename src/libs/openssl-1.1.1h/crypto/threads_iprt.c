/* $Id: threads_iprt.cpp 125764 2018-10-12 17:09:46Z michael $ */
/** @file
 * Crypto thread locking functions which make use of the IPRT.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <openssl/crypto.h>
#include "internal/cryptlib.h"

#if defined(OPENSSL_THREADS)

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
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
    int rc = RTTlsAllocEx(key, cleanup);
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

int CRYPTO_atomic_add(int *val, int amount, int *ret, CRYPTO_RWLOCK *lock)
{
    *ret = ASMAtomicAddS32((int32_t volatile*)val, amount) + amount;
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
