/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>

#ifndef VBOX

#define DEFINE_RUN_ONCE(init)                   \
    static int init(void);                     \
    int init##_ossl_ret_ = 0;                   \
    void init##_ossl_(void)                     \
    {                                           \
        init##_ossl_ret_ = init();              \
    }                                           \
    static int init(void)
#define DECLARE_RUN_ONCE(init)                  \
    extern int init##_ossl_ret_;                \
    void init##_ossl_(void);

#define DEFINE_RUN_ONCE_STATIC(init)            \
    static int init(void);                     \
    static int init##_ossl_ret_ = 0;            \
    static void init##_ossl_(void)              \
    {                                           \
        init##_ossl_ret_ = init();              \
    }                                           \
    static int init(void)

#else /* VBOX */

/*
 * PFNRTONCE returns an IPRT status code but the OpenSSL once functions
 * return 1 for success and 0 for failure. We need to translate between
 * these errors back (here) and force (in RUN_ONCE()).
 */
#undef DEFINE_RUN_ONCE   /* currently unused */
#undef DECLARE_RUN_ONCE  /* currently unused */
#define DEFINE_RUN_ONCE_STATIC(init)            \
    static int init(void);                                            \
    static int init##_ossl_ret_ = 0;                                  \
    static DECLCALLBACK(int) init##_ossl_(void *pvUser)               \
    {                                                                 \
        init##_ossl_ret_ = init();                                    \
        return init##_ossl_ret_ ? VINF_SUCCESS : VERR_INTERNAL_ERROR; \
    }                                                                 \
    static int init(void)

#endif /* VBOX */

/*
 * RUN_ONCE - use CRYPTO_THREAD_run_once, and check if the init succeeded
 * @once: pointer to static object of type CRYPTO_ONCE
 * @init: function name that was previously given to DEFINE_RUN_ONCE,
 *        DEFINE_RUN_ONCE_STATIC or DECLARE_RUN_ONCE.  This function
 *        must return 1 for success or 0 for failure.
 *
 * The return value is 1 on success (*) or 0 in case of error.
 *
 * (*) by convention, since the init function must return 1 on success.
 */
#ifndef VBOX
#define RUN_ONCE(once, init)                                            \
    (CRYPTO_THREAD_run_once(once, init##_ossl_) ? init##_ossl_ret_ : 0)
#else
#define RUN_ONCE(once, init)                                                 \
    (RT_SUCCESS_NP(RTOnce(once, init##_ossl_, NULL)) ? init##_ossl_ret_ : 0)
#endif
