/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Internal tests for the chacha module. EVP tests would exercise
 * complete 32-byte blocks. This test goes per byte...
 */

#include <string.h>
#include <openssl/opensslconf.h>
#include "testutil.h"
#include "crypto/chacha.h"

static const unsigned int key[] = {
    0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c,
    0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c
};

static const unsigned int ivp[] = {
    0x00000000, 0x00000000, 0x03020100, 0x07060504
};

static const unsigned char ref[] = {
    0xf7, 0x98, 0xa1, 0x89, 0xf1, 0x95, 0xe6, 0x69,
    0x82, 0x10, 0x5f, 0xfb, 0x64, 0x0b, 0xb7, 0x75,
    0x7f, 0x57, 0x9d, 0xa3, 0x16, 0x02, 0xfc, 0x93,
    0xec, 0x01, 0xac, 0x56, 0xf8, 0x5a, 0xc3, 0xc1,
    0x34, 0xa4, 0x54, 0x7b, 0x73, 0x3b, 0x46, 0x41,
    0x30, 0x42, 0xc9, 0x44, 0x00, 0x49, 0x17, 0x69,
    0x05, 0xd3, 0xbe, 0x59, 0xea, 0x1c, 0x53, 0xf1,
    0x59, 0x16, 0x15, 0x5c, 0x2b, 0xe8, 0x24, 0x1a,
    0x38, 0x00, 0x8b, 0x9a, 0x26, 0xbc, 0x35, 0x94,
    0x1e, 0x24, 0x44, 0x17, 0x7c, 0x8a, 0xde, 0x66,
    0x89, 0xde, 0x95, 0x26, 0x49, 0x86, 0xd9, 0x58,
    0x89, 0xfb, 0x60, 0xe8, 0x46, 0x29, 0xc9, 0xbd,
    0x9a, 0x5a, 0xcb, 0x1c, 0xc1, 0x18, 0xbe, 0x56,
    0x3e, 0xb9, 0xb3, 0xa4, 0xa4, 0x72, 0xf8, 0x2e,
    0x09, 0xa7, 0xe7, 0x78, 0x49, 0x2b, 0x56, 0x2e,
    0xf7, 0x13, 0x0e, 0x88, 0xdf, 0xe0, 0x31, 0xc7,
    0x9d, 0xb9, 0xd4, 0xf7, 0xc7, 0xa8, 0x99, 0x15,
    0x1b, 0x9a, 0x47, 0x50, 0x32, 0xb6, 0x3f, 0xc3,
    0x85, 0x24, 0x5f, 0xe0, 0x54, 0xe3, 0xdd, 0x5a,
    0x97, 0xa5, 0xf5, 0x76, 0xfe, 0x06, 0x40, 0x25,
    0xd3, 0xce, 0x04, 0x2c, 0x56, 0x6a, 0xb2, 0xc5,
    0x07, 0xb1, 0x38, 0xdb, 0x85, 0x3e, 0x3d, 0x69,
    0x59, 0x66, 0x09, 0x96, 0x54, 0x6c, 0xc9, 0xc4,
    0xa6, 0xea, 0xfd, 0xc7, 0x77, 0xc0, 0x40, 0xd7,
    0x0e, 0xaf, 0x46, 0xf7, 0x6d, 0xad, 0x39, 0x79,
    0xe5, 0xc5, 0x36, 0x0c, 0x33, 0x17, 0x16, 0x6a,
    0x1c, 0x89, 0x4c, 0x94, 0xa3, 0x71, 0x87, 0x6a,
    0x94, 0xdf, 0x76, 0x28, 0xfe, 0x4e, 0xaa, 0xf2,
    0xcc, 0xb2, 0x7d, 0x5a, 0xaa, 0xe0, 0xad, 0x7a,
    0xd0, 0xf9, 0xd4, 0xb6, 0xad, 0x3b, 0x54, 0x09,
    0x87, 0x46, 0xd4, 0x52, 0x4d, 0x38, 0x40, 0x7a,
    0x6d, 0xeb, 0x3a, 0xb7, 0x8f, 0xab, 0x78, 0xc9,
    0x42, 0x13, 0x66, 0x8b, 0xbb, 0xd3, 0x94, 0xc5,
    0xde, 0x93, 0xb8, 0x53, 0x17, 0x8a, 0xdd, 0xd6,
    0xb9, 0x7f, 0x9f, 0xa1, 0xec, 0x3e, 0x56, 0xc0,
    0x0c, 0x9d, 0xdf, 0xf0, 0xa4, 0x4a, 0x20, 0x42,
    0x41, 0x17, 0x5a, 0x4c, 0xab, 0x0f, 0x96, 0x1b,
    0xa5, 0x3e, 0xde, 0x9b, 0xdf, 0x96, 0x0b, 0x94,
    0xf9, 0x82, 0x9b, 0x1f, 0x34, 0x14, 0x72, 0x64,
    0x29, 0xb3, 0x62, 0xc5, 0xb5, 0x38, 0xe3, 0x91,
    0x52, 0x0f, 0x48, 0x9b, 0x7e, 0xd8, 0xd2, 0x0a,
    0xe3, 0xfd, 0x49, 0xe9, 0xe2, 0x59, 0xe4, 0x43,
    0x97, 0x51, 0x4d, 0x61, 0x8c, 0x96, 0xc4, 0x84,
    0x6b, 0xe3, 0xc6, 0x80, 0xbd, 0xc1, 0x1c, 0x71,
    0xdc, 0xbb, 0xe2, 0x9c, 0xcf, 0x80, 0xd6, 0x2a,
    0x09, 0x38, 0xfa, 0x54, 0x93, 0x91, 0xe6, 0xea,
    0x57, 0xec, 0xbe, 0x26, 0x06, 0x79, 0x0e, 0xc1,
    0x5d, 0x22, 0x24, 0xae, 0x30, 0x7c, 0x14, 0x42,
    0x26, 0xb7, 0xc4, 0xe8, 0xc2, 0xf9, 0x7d, 0x2a,
    0x1d, 0x67, 0x85, 0x2d, 0x29, 0xbe, 0xba, 0x11,
    0x0e, 0xdd, 0x44, 0x51, 0x97, 0x01, 0x20, 0x62,
    0xa3, 0x93, 0xa9, 0xc9, 0x28, 0x03, 0xad, 0x3b,
    0x4f, 0x31, 0xd7, 0xbc, 0x60, 0x33, 0xcc, 0xf7,
    0x93, 0x2c, 0xfe, 0xd3, 0xf0, 0x19, 0x04, 0x4d,
    0x25, 0x90, 0x59, 0x16, 0x77, 0x72, 0x86, 0xf8,
    0x2f, 0x9a, 0x4c, 0xc1, 0xff, 0xe4, 0x30, 0xff,
    0xd1, 0xdc, 0xfc, 0x27, 0xde, 0xed, 0x32, 0x7b,
    0x9f, 0x96, 0x30, 0xd2, 0xfa, 0x96, 0x9f, 0xb6,
    0xf0, 0x60, 0x3c, 0xd1, 0x9d, 0xd9, 0xa9, 0x51,
    0x9e, 0x67, 0x3b, 0xcf, 0xcd, 0x90, 0x14, 0x12,
    0x52, 0x91, 0xa4, 0x46, 0x69, 0xef, 0x72, 0x85,
    0xe7, 0x4e, 0xd3, 0x72, 0x9b, 0x67, 0x7f, 0x80,
    0x1c, 0x3c, 0xdf, 0x05, 0x8c, 0x50, 0x96, 0x31,
    0x68, 0xb4, 0x96, 0x04, 0x37, 0x16, 0xc7, 0x30,
    0x7c, 0xd9, 0xe0, 0xcd, 0xd1, 0x37, 0xfc, 0xcb,
    0x0f, 0x05, 0xb4, 0x7c, 0xdb, 0xb9, 0x5c, 0x5f,
    0x54, 0x83, 0x16, 0x22, 0xc3, 0x65, 0x2a, 0x32,
    0xb2, 0x53, 0x1f, 0xe3, 0x26, 0xbc, 0xd6, 0xe2,
    0xbb, 0xf5, 0x6a, 0x19, 0x4f, 0xa1, 0x96, 0xfb,
    0xd1, 0xa5, 0x49, 0x52, 0x11, 0x0f, 0x51, 0xc7,
    0x34, 0x33, 0x86, 0x5f, 0x76, 0x64, 0xb8, 0x36,
    0x68, 0x5e, 0x36, 0x64, 0xb3, 0xd8, 0x44, 0x4a,
    0xf8, 0x9a, 0x24, 0x28, 0x05, 0xe1, 0x8c, 0x97,
    0x5f, 0x11, 0x46, 0x32, 0x49, 0x96, 0xfd, 0xe1,
    0x70, 0x07, 0xcf, 0x3e, 0x6e, 0x8f, 0x4e, 0x76,
    0x40, 0x22, 0x53, 0x3e, 0xdb, 0xfe, 0x07, 0xd4,
    0x73, 0x3e, 0x48, 0xbb, 0x37, 0x2d, 0x75, 0xb0,
    0xef, 0x48, 0xec, 0x98, 0x3e, 0xb7, 0x85, 0x32,
    0x16, 0x1c, 0xc5, 0x29, 0xe5, 0xab, 0xb8, 0x98,
    0x37, 0xdf, 0xcc, 0xa6, 0x26, 0x1d, 0xbb, 0x37,
    0xc7, 0xc5, 0xe6, 0xa8, 0x74, 0x78, 0xbf, 0x41,
    0xee, 0x85, 0xa5, 0x18, 0xc0, 0xf4, 0xef, 0xa9,
    0xbd, 0xe8, 0x28, 0xc5, 0xa7, 0x1b, 0x8e, 0x46,
    0x59, 0x7b, 0x63, 0x4a, 0xfd, 0x20, 0x4d, 0x3c,
    0x50, 0x13, 0x34, 0x23, 0x9c, 0x34, 0x14, 0x28,
    0x5e, 0xd7, 0x2d, 0x3a, 0x91, 0x69, 0xea, 0xbb,
    0xd4, 0xdc, 0x25, 0xd5, 0x2b, 0xb7, 0x51, 0x6d,
    0x3b, 0xa7, 0x12, 0xd7, 0x5a, 0xd8, 0xc0, 0xae,
    0x5d, 0x49, 0x3c, 0x19, 0xe3, 0x8a, 0x77, 0x93,
    0x9e, 0x7a, 0x05, 0x8d, 0x71, 0x3e, 0x9c, 0xcc,
    0xca, 0x58, 0x04, 0x5f, 0x43, 0x6b, 0x43, 0x4b,
    0x1c, 0x80, 0xd3, 0x65, 0x47, 0x24, 0x06, 0xe3,
    0x92, 0x95, 0x19, 0x87, 0xdb, 0x69, 0x05, 0xc8,
    0x0d, 0x43, 0x1d, 0xa1, 0x84, 0x51, 0x13, 0x5b,
    0xe7, 0xe8, 0x2b, 0xca, 0xb3, 0x58, 0xcb, 0x39,
    0x71, 0xe6, 0x14, 0x05, 0xb2, 0xff, 0x17, 0x98,
    0x0d, 0x6e, 0x7e, 0x67, 0xe8, 0x61, 0xe2, 0x82,
    0x01, 0xc1, 0xee, 0x30, 0xb4, 0x41, 0x04, 0x0f,
    0xd0, 0x68, 0x78, 0xd6, 0x50, 0x42, 0xc9, 0x55,
    0x82, 0xa4, 0x31, 0x82, 0x07, 0xbf, 0xc7, 0x00,
    0xbe, 0x0c, 0xe3, 0x28, 0x89, 0xae, 0xc2, 0xff,
    0xe5, 0x08, 0x5e, 0x89, 0x67, 0x91, 0x0d, 0x87,
    0x9f, 0xa0, 0xe8, 0xc0, 0xff, 0x85, 0xfd, 0xc5,
    0x10, 0xb9, 0xff, 0x2f, 0xbf, 0x87, 0xcf, 0xcb,
    0x29, 0x57, 0x7d, 0x68, 0x09, 0x9e, 0x04, 0xff,
    0xa0, 0x5f, 0x75, 0x2a, 0x73, 0xd3, 0x77, 0xc7,
    0x0d, 0x3a, 0x8b, 0xc2, 0xda, 0x80, 0xe6, 0xe7,
    0x80, 0xec, 0x05, 0x71, 0x82, 0xc3, 0x3a, 0xd1,
    0xde, 0x38, 0x72, 0x52, 0x25, 0x8a, 0x1e, 0x18,
    0xe6, 0xfa, 0xd9, 0x10, 0x32, 0x7c, 0xe7, 0xf4,
    0x2f, 0xd1, 0xe1, 0xe0, 0x51, 0x5f, 0x95, 0x86,
    0xe2, 0xf2, 0xef, 0xcb, 0x9f, 0x47, 0x2b, 0x1d,
    0xbd, 0xba, 0xc3, 0x54, 0xa4, 0x16, 0x21, 0x51,
    0xe9, 0xd9, 0x2c, 0x79, 0xfb, 0x08, 0xbb, 0x4d,
    0xdc, 0x56, 0xf1, 0x94, 0x48, 0xc0, 0x17, 0x5a,
    0x46, 0xe2, 0xe6, 0xc4, 0x91, 0xfe, 0xc7, 0x14,
    0x19, 0xaa, 0x43, 0xa3, 0x49, 0xbe, 0xa7, 0x68,
    0xa9, 0x2c, 0x75, 0xde, 0x68, 0xfd, 0x95, 0x91,
    0xe6, 0x80, 0x67, 0xf3, 0x19, 0x70, 0x94, 0xd3,
    0xfb, 0x87, 0xed, 0x81, 0x78, 0x5e, 0xa0, 0x75,
    0xe4, 0xb6, 0x5e, 0x3e, 0x4c, 0x78, 0xf8, 0x1d,
    0xa9, 0xb7, 0x51, 0xc5, 0xef, 0xe0, 0x24, 0x15,
    0x23, 0x01, 0xc4, 0x8e, 0x63, 0x24, 0x5b, 0x55,
    0x6c, 0x4c, 0x67, 0xaf, 0xf8, 0x57, 0xe5, 0xea,
    0x15, 0xa9, 0x08, 0xd8, 0x3a, 0x1d, 0x97, 0x04,
    0xf8, 0xe5, 0x5e, 0x73, 0x52, 0xb2, 0x0b, 0x69,
    0x4b, 0xf9, 0x97, 0x02, 0x98, 0xe6, 0xb5, 0xaa,
    0xd3, 0x3e, 0xa2, 0x15, 0x5d, 0x10, 0x5d, 0x4e
};

static int test_cha_cha_internal(int n)
{
    unsigned char buf[sizeof(ref)];
    unsigned int i = n + 1, j;

    memset(buf, 0, i);
    memcpy(buf + i, ref + i, sizeof(ref) - i);

    ChaCha20_ctr32(buf, buf, i, key, ivp);

    /*
     * Idea behind checking for whole sizeof(ref) is that if
     * ChaCha20_ctr32 oversteps i-th byte, then we'd know
     */
    for (j = 0; j < sizeof(ref); j++)
        if (!TEST_uchar_eq(buf[j], ref[j])) {
            TEST_info("%d failed at %u (%02x)\n", i, j, buf[j]);
            return 0;
        }
    return 1;
}

int setup_tests(void)
{
#ifdef CPUID_OBJ
    OPENSSL_cpuid_setup();
#endif

    ADD_ALL_TESTS(test_cha_cha_internal, sizeof(ref));
    return 1;
}
