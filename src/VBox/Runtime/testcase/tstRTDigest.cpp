/* $Id$ */
/** @file
 * IPRT Testcase - RTSha*, RTMd5, RTCrc*.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/sha.h>
#include <iprt/md5.h>
#include <iprt/crc32.h>
#include <iprt/crc64.h>

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
    if (!RTProcGetExecutableName(szName, sizeof(szName)))
        strcpy(szName, "tstRTDigest");

    RTStrmPrintf(g_pStdErr, "%s: error: ", RTPathFilename(szName));
    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);
    va_end(va);

    return 1;
}


int main(int argc, char **argv)
{
     RTR3Init();

     enum
     {
         kDigestType_NotSpecified,
         kDigestType_CRC32,
         kDigestType_CRC64,
         kDigestType_MD5,
         kDigestType_SHA1,
         kDigestType_SHA256,
         kDigestType_SHA512
     } enmDigestType = kDigestType_NotSpecified;

     enum
     {
         kMethod_Full,
         kMethod_Block,
         kMethod_File
     } enmMethod = kMethod_Block;

     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--type",   't', RTGETOPT_REQ_STRING },
         { "--method", 'm', RTGETOPT_REQ_STRING },
         { "--help",   'h', RTGETOPT_REQ_NOTHING },
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
     while ((ch = RTGetOpt(&GetState, &ValueUnion)))
     {
         switch (ch)
         {
             case 't':
                 if (!RTStrICmp(ValueUnion.psz, "crc32"))
                     enmDigestType = kDigestType_CRC32;
                 else if (!RTStrICmp(ValueUnion.psz, "crc64"))
                     enmDigestType = kDigestType_CRC64;
                 else if (!RTStrICmp(ValueUnion.psz, "md5"))
                     enmDigestType = kDigestType_MD5;
                 else if (!RTStrICmp(ValueUnion.psz, "sha1"))
                     enmDigestType = kDigestType_SHA1;
                 else if (!RTStrICmp(ValueUnion.psz, "sha256"))
                     enmDigestType = kDigestType_SHA256;
                 else if (!RTStrICmp(ValueUnion.psz, "sha512"))
                     enmDigestType = kDigestType_SHA512;
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

             case 'h':
                 RTPrintf("syntax: tstRTDigest -t <digest-type> file [file2 [..]]\n");
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
                         switch (enmDigestType)
                         {
                             case kDigestType_SHA1:
                             {
                                 char *pszDigest;
                                 int rc = RTSha1Digest(ValueUnion.psz, &pszDigest);
                                 if (RT_FAILURE(rc))
                                     return Error("RTSha1Digest(%s,) -> %Rrc\n", ValueUnion.psz, rc);
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
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
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
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     uCRC64 = RTCrc64Process(uCRC64, abBuf, cbRead);
                                 }
                                 uCRC64 = RTCrc64Finish(uCRC64);
                                 RTStrPrintf(pszDigest, sizeof(abBuf), "%016RX64", uCRC64);
                                 break;
                             }

                             case kDigestType_MD5:
                             {
                                 RTMD5CONTEXT Ctx;
                                 RTMd5Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTMd5Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTMD5HASHSIZE];
                                 RTMd5Final(abDigest, &Ctx);
                                 RTStrPrintf(pszDigest, sizeof(abBuf),
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                             abDigest[0], abDigest[1], abDigest[2], abDigest[3],
                                             abDigest[4], abDigest[5], abDigest[6], abDigest[7],
                                             abDigest[8], abDigest[9], abDigest[10], abDigest[11],
                                             abDigest[12], abDigest[13], abDigest[14], abDigest[15]);
                                 break;
                             }

                             case kDigestType_SHA1:
                             {
                                 RTSHA1CONTEXT Ctx;
                                 RTSha1Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha1Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA1_HASH_SIZE];
                                 RTSha1Final(&Ctx, abDigest);
                                 RTStrPrintf(pszDigest, sizeof(abBuf),
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                             abDigest[0],  abDigest[1],  abDigest[2],  abDigest[3],
                                             abDigest[4],  abDigest[5],  abDigest[6],  abDigest[7],
                                             abDigest[8],  abDigest[9],  abDigest[10], abDigest[11],
                                             abDigest[12], abDigest[13], abDigest[14], abDigest[15],
                                             abDigest[16], abDigest[17], abDigest[18], abDigest[19]);
                                 break;
                             }

                             case kDigestType_SHA256:
                             {
                                 RTSHA256CONTEXT Ctx;
                                 RTSha256Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha256Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA256_HASH_SIZE];
                                 RTSha256Final(&Ctx, abDigest);
                                 RTStrPrintf(pszDigest, sizeof(abBuf),
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                             abDigest[0],  abDigest[1],  abDigest[2],  abDigest[3],
                                             abDigest[4],  abDigest[5],  abDigest[6],  abDigest[7],
                                             abDigest[8],  abDigest[9],  abDigest[10], abDigest[11],
                                             abDigest[12], abDigest[13], abDigest[14], abDigest[15],
                                             abDigest[16], abDigest[17], abDigest[18], abDigest[19],
                                             abDigest[20], abDigest[21], abDigest[22], abDigest[23],
                                             abDigest[24], abDigest[25], abDigest[26], abDigest[27],
                                             abDigest[28], abDigest[29], abDigest[30], abDigest[31]);
                                 break;
                             }

                             case kDigestType_SHA512:
                             {
                                 RTSHA512CONTEXT Ctx;
                                 RTSha512Init(&Ctx);
                                 for (;;)
                                 {
                                     rc = RTFileRead(hFile, abBuf, sizeof(abBuf), &cbRead);
                                     if (RT_FAILURE(rc) || !cbRead)
                                         break;
                                     RTSha512Update(&Ctx, abBuf, cbRead);
                                 }
                                 uint8_t abDigest[RTSHA512_HASH_SIZE];
                                 RTSha512Final(&Ctx, abDigest);
                                 RTStrPrintf(pszDigest, sizeof(abBuf),
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                             abDigest[0],  abDigest[1],  abDigest[2],  abDigest[3],
                                             abDigest[4],  abDigest[5],  abDigest[6],  abDigest[7],
                                             abDigest[8],  abDigest[9],  abDigest[10], abDigest[11],
                                             abDigest[12], abDigest[13], abDigest[14], abDigest[15],
                                             abDigest[16], abDigest[17], abDigest[18], abDigest[19],
                                             abDigest[20], abDigest[21], abDigest[22], abDigest[23],
                                             abDigest[24], abDigest[25], abDigest[26], abDigest[27],
                                             abDigest[28], abDigest[29], abDigest[30], abDigest[31],
                                             abDigest[32], abDigest[33], abDigest[34], abDigest[35],
                                             abDigest[36], abDigest[37], abDigest[38], abDigest[39],
                                             abDigest[40], abDigest[41], abDigest[42], abDigest[43],
                                             abDigest[44], abDigest[45], abDigest[46], abDigest[47],
                                             abDigest[48], abDigest[49], abDigest[50], abDigest[51],
                                             abDigest[52], abDigest[53], abDigest[54], abDigest[55],
                                             abDigest[56], abDigest[57], abDigest[58], abDigest[59],
                                             abDigest[60], abDigest[61], abDigest[62], abDigest[63]);
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
                         RTPrintf("%s  %s\n", pszDigest, ValueUnion.psz);
                         break;
                     }

                     default:
                         return Error("Internal error #2\n");
                 }
                 break;
             }

             default:
                 if (ch > 0)
                 {
                     if (RT_C_IS_GRAPH(ch))
                         Error("unhandled option: -%c\n", ch);
                     else
                         Error("unhandled option: %i\n", ch);
                 }
                 else if (ch == VERR_GETOPT_UNKNOWN_OPTION)
                     Error("unknown option: %s\n", ValueUnion.psz);
                 else if (ValueUnion.pDef)
                     Error("%s: %Rrs\n", ValueUnion.pDef->pszLong, ch);
                 else
                     Error("%Rrs\n", ch);
                 return 1;
         }
     }

     return 0;
}

