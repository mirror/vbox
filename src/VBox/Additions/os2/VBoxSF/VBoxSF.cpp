/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folders, the FS and FSD level IFS EPs
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxSFInternal.h"

#include <VBox/log.h>
#include <iprt/assert.h>



DECLASM(void)
FS32_EXIT(ULONG uid, ULONG pid, ULONG pdb)
{
}


DECLASM(int)
FS32_SHUTDOWN(ULONG type, ULONG reserved)
{
    return NO_ERROR;
}


DECLASM(int)
FS32_ATTACH(ULONG flag, PCSZ pszDev, PVBOXSFVP pvpfsd, PVBOXSFCD pcdfsd, PBYTE pszParm, PUSHORT pcbParm)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FLUSHBUF(USHORT hVPB, ULONG flag)
{
    return NO_ERROR;
}


DECLASM(int)
FS32_FSINFO(ULONG flag, USHORT hVPB, PBYTE pbData, USHORT cbData, ULONG level)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FSCTL(union argdat *pArgdat, ULONG iArgType, ULONG func,
           PVOID pParm, USHORT lenParm, PUSHORT plenParmIO,
           PVOID pData, USHORT lenData, PUSHORT plenDataIO)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_PROCESSNAME(PSZ pszName)
{
    return NO_ERROR;
}


DECLASM(int)
FS32_CHDIR(ULONG flag, PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszDir, USHORT iCurDirEnd)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_MKDIR(PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszName, USHORT iCurDirEnd,
           PBYTE pEABuf, ULONG flag)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_RMDIR(PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszName, USHORT iCurDirEnd)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_COPY(USHORT flag, PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszSrc, USHORT iSrcCurDirEnd,
          PCSZ pszDst, USHORT iDstCurDirEnd, USHORT nameType)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_MOVE(PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszSrc, USHORT iSrcCurDirEnd,
          PCSZ pszDst, USHORT iDstCurDirEnd, USHORT type)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_DELETE(PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszFile, USHORT iCurDirEnd)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FILEATTRIBUTE(ULONG flag, PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszName, USHORT iCurDirEnd, PUSHORT pAttr)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_PATHINFO(USHORT flag, PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszName, USHORT iCurDirEnt,
              USHORT level, PBYTE pData, USHORT cbData)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_MOUNT(USHORT flag, PVPFSI pvpfsi, PVBOXSFVP pvpfsd, USHORT hVPB, PCSZ pszBoot)
{
    return ERROR_NOT_SUPPORTED;
}

