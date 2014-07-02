/* $Id$ */
/** @file
 * IPRT Testcase - RTSha*, RTMd5, RTCrc*.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/sha.h>
#include <iprt/md2.h>
#include <iprt/md5.h>
#include <iprt/crc.h>

#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>


static int Error(const char *pszFormat, ...)
{
    char szName[RTPATH_MAX];
    if (!RTProcGetExecutablePath(szName, sizeof(szName)))
        strcpy(szName, "tstRTDigest");

    RTStrmPrintf(g_pStdErr, "%s: error: ", RTPathFilename(szName));
    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);
    va_end(va);

    return 1;
}


static int MyReadFile(RTFILE hFile, void *pvBuf, size_t cbToRead, size_t *pcbRead, uint64_t *pcbMaxLeft)
{
    int rc = VINF_SUCCESS;
    if (*pcbMaxLeft > 0)
    {
        if (cbToRead > *pcbMaxLeft)
            cbToRead = (size_t)*pcbMaxLeft;
        rc = RTFileRead(hFile, pvBuf, cbToRead, pcbRead);
        if (RT_SUCCESS(rc))
            *pcbMaxLeft -= *pcbRead;
    }
    else
        *pcbRead = 0;
    return rc;
}



int main(int argc, char **argv)
{
     RTR3InitExe(argc, &argv, 0);

     enum
     {
         kDigestType_NotSpecified,
         kDigestType_CRC32,
         kDigestType_CRC64,
         kDigestType_MD2,
         kDigestType_MD5,
         kDigestType_SHA1,
         kDigestType_SHA256,
         kDigestType_SHA512
     } enmDigestType = kDigestType_NotSpecified;
     const char *pszDigestType = "NotSpecified";

     enum
     {
         kMethod_Full,
         kMethod_Block,
         kMethod_File
     } enmMethod = kMethod_Block;

     uint64_t   offStart    = 0;
     uint64_t   cbMax       = UINT64_MAX;
     bool       fTestcase   = false;

     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--type",   't', RTGETOPT_REQ_STRING },
         { "--method", 'm', RTGETOPT_REQ_STRING },
         { "--help",   'h', RTGETOPT_REQ_NOTHING },
         { "--length", 'l', RTGETOPT_REQ_UINT64 },
         { "--offset", 'o', RTGETOPT_REQ_UINT64 },
         { "--testcase", 'x', RTGETOPT_REQ_NOTHING },
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
     while ((ch = RTGetOpt(&GetState, &ValueUnion)))
     {
         switch (ch)
         {
             case 't':
                 if (!RTStrICmp(ValueUnion.psz, "crc32"))
                 {
                     pszDigestType = "CRC32";
                     enmDigestType = kDigestType_CRC32;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "crc64"))
                 {
                     pszDigestType = "CRC64";
                     enmDigestType = kDigestType_CRC64;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "md2"))
                 {
                     pszDigestType = "MD2";
                     enmDigestType = kDigestType_MD2;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "md5"))
                 {
                     pszDigestType = "MD5";
                     enmDigestType = kDigestType_MD5;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "sha1"))
                 {
                     pszDigestType = "SHA-1";
                     enmDigestType = kDigestType_SHA1;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "sha256"))
                 {
                     pszDigestType = "SHA-256";
                     enmDigestType = kDigestType_SHA256;
                 }
                 else if (!RTStrICmp(ValueUnion.psz, "sha512"))
                 {
                     pszDigestType = "SHA-512";
                     enmDigestType = kDigestType_SHA512;
                 }
                 else
                 {
                     Error("Invalid digest type: %s\n", ValueUnion.psz);
                     return 1;
                 }
                 break;

             case 'm':
                 if (!RTStrICmp(ValueUnion.psz, "full"))
                     enmMethod = kMethod_Full;
                 else if (!RTStrICmp(ValueUnion.psz, "block"))
                     enmMethod = kMethod_Block;
                 else if (!RTStrICmp(ValueUnion.psz, "file"))
                     enmMethod = kMethod_File;
                 else
                 {
                     Error("Invalid digest method: %s\n", ValueUnion.psz);
                     return 1;
                 }
                 break;

             case 'l':
                 cbMax = ValueUnion.u64;
                 break;

             case 'o':
                 offStart = ValueUnion.u64;
                 break;

             case 'x':
                 fTestcase = true;
                 break;

             case 'h':
                 RTPrintf("usage: tstRTDigest -t <digest-type> [-o <offset>] [-l <length>] [-x] file [file2 [..]]\n");
                 return 1;

             case VINF_GETOPT_NOT_OPTION:
             {
                 if (enmDigestType == kDigestType_NotSpecified)
                     return Error("No digest type was specified\n");

                 switch (enmMethod)
                 {
                     case kMethod_Full:
                         return Error("Full file method is not implemented\n");

                     case kMethod_File:
                         if (offStart != 0 || cbMax != UINT64_MAX)
                             return Error("The -l and -o options do not work with the 'file' method.");
                         switch (enmDigestType)
                         {
                             case kDigestType_SHA1:
                             {
                                 char *pszDigest;
                                 int rc = RTSha1DigestFromFile(ValueUnion.psz, &pszDigest, NULL, NULL);
                                 if (RT_FAILURE(rc))
                                     return Error("RTSha1Digest(%s,) -> %Rrc\n", ValueUnion.psz, rc);
                                 RTPrintf("%s  %s\n", pszDigest, ValueUnion.psz);
                                 RTStrFree(pszDigest);
                                 break;
                             }

                             case kDigestType_SHA256:
                             {
                                 char *pszDigest;
                                 int rc = RTSha256DigestFromFile(ValueUnion.psz, &pszDigest, NULL, NULL);
                                 if (RT_FAILURE(rc))
                                     return Error("RTSha256Digest(%s,) -> %Rrc\n", ValueUnion.psz, rc);
                                 RTPrintf("%s  %s\n", pszDigest, ValueUnion.psz);
                                 RTStrFree(pszDigest);
                                 break;
                             }
                             default:
                                 return Error("The file method isn't implemented for this digest\n");
                         }
                         break;

                     case kMethod_Block:
                     {
                         RTFILE hFile;
                         int rc = RTFileOpen(&hFile, ValueUnion.psz, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                         if (RT_FAILURE(rc))
                             return Error("RTFileOpen(,%s,) -> %Rrc\n", ValueUnion.psz, rc);
                         if (offStart != 0)
                         {
                             rc = RTFileSeek(hFile, offStart, RTFILE_SEEK_BEGIN, NULL);
                             if (RT_FAILURE(rc))
                                 return Error("RTFileSeek(%s,%ull) -> %Rrc\n", ValueUnion.psz, offStart, rc);
                         }

                         uint64_t cbMaxLeft = cbMax;
                         size_t  cbRead;
                         uint8_t abBuf[_64K];
                         char   *pszDigest = (char *)&abBuf[0];
                         switch (enmDigestType)
                         {
                             case kDigestType_CRC32:
                             {
                                 uint32_t uCRC32 = RTCrc32Start();
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     uCRC32 = RTCrc32Process(uCRC32, abBuf, cbRead);
                                 }
                                 uCRC32 = RTCrc32Finish(uCRC32);
                                 RTStrPrintf(pszDigest, sizeof(abBuf), "%08RX32", uCRC32);
                                 break;
                             }

                             case kDigestType_CRC64:
                             {
                                 uint64_t uCRC64 = RTCrc64Start();
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     uCRC64 = RTCrc64Process(uCRC64, abBuf, cbRead);
                                 }
                                 uCRC64 = RTCrc64Finish(uCRC64);
                                 RTStrPrintf(pszDigest, sizeof(abBuf), "%016RX64", uCRC64);
                                 break;
                             }

                             case kDigestType_MD2:
                             {
                                 RTMD2CONTEXT Ctx;
                                 RTMd2Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTMd2Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTMD2_HASH_SIZE];
                                 RTMd2Final(&Ctx, abDigest);
                                 RTMd2ToString(abDigest, pszDigest, sizeof(abBuf));
                                 break;
                             }

                             case kDigestType_MD5:
                             {
                                 RTMD5CONTEXT Ctx;
                                 RTMd5Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTMd5Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTMD5HASHSIZE];
                                 RTMd5Final(abDigest, &Ctx);
                                 RTMd5ToString(abDigest, pszDigest, sizeof(abBuf));
                                 break;
                             }

                             case kDigestType_SHA1:
                             {
                                 RTSHA1CONTEXT Ctx;
                                 RTSha1Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha1Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA1_HASH_SIZE];
                                 RTSha1Final(&Ctx, abDigest);
                                 RTSha1ToString(abDigest, pszDigest, sizeof(abBuf));
                                 break;
                             }

                             case kDigestType_SHA256:
                             {
                                 RTSHA256CONTEXT Ctx;
                                 RTSha256Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha256Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA256_HASH_SIZE];
                                 RTSha256Final(&Ctx, abDigest);
                                 RTSha256ToString(abDigest, pszDigest, sizeof(abBuf));
                                 break;
                             }

                             case kDigestType_SHA512:
                             {
                                 RTSHA512CONTEXT Ctx;
                                 RTSha512Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = MyReadFile(hFile, abBuf, sizeof(abBuf), &cbRead, &cbMaxLeft);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha512Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA512_HASH_SIZE];
                                 RTSha512Final(&Ctx, abDigest);
                                 RTSha512ToString(abDigest, pszDigest, sizeof(abBuf));
                                 break;
                             }

                             default:
                                 return Error("Internal error #1\n");
                         }
                         RTFileClose(hFile);
                         if (RT_FAILURE(rc) && rc != VERR_EOF)
                         {
                             RTPrintf("Partial: %s  %s\n", pszDigest, ValueUnion.psz);
                             return Error("RTFileRead(%s) -> %Rrc\n", ValueUnion.psz, rc);
                         }

                         if (!fTestcase)
                             RTPrintf("%s  %s\n", pszDigest, ValueUnion.psz);
                         else if (offStart)
                             RTPrintf("        { &g_abRandom72KB[%#4llx], %5llu, \"%s\", \"%s %llu bytes @%llu\" },\n",
                                      offStart, cbMax - cbMaxLeft, pszDigest, pszDigestType, offStart, cbMax - cbMaxLeft);
                         else
                             RTPrintf("        { &g_abRandom72KB[0],     %5llu, \"%s\", \"%s %llu bytes\" },\n",
                                      cbMax - cbMaxLeft, pszDigest, pszDigestType, cbMax - cbMaxLeft);
                         break;
                     }

                     default:
                         return Error("Internal error #2\n");
                 }
                 break;
             }

             default:
                return RTGetOptPrintError(ch, &ValueUnion);
         }
     }

     return 0;
}

