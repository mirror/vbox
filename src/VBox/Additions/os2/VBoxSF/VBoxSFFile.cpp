/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folders, the file level IFS EPs.
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



DECLASM(int)
FS32_OPENCREATE(PCDFSI pcdfsi, PVBOXSFCD pcdfsd, PCSZ pszName, USHORT iCurDirEnd,
                PSFFSI psffsi, PVBOXSFFSD psffsd, ULONG ulOpenMode, USHORT usOpenFlag,
                PUSHORT pusAction, USHORT usAttr, PBYTE pcEABuf, PUSHORT pfgenflag)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_CLOSE(ULONG type, ULONG IOflag, PSFFSI psffsi, PVBOXSFFSD psffsd)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_COMMIT(ULONG type, ULONG IOflag, PSFFSI psffsi, PVBOXSFFSD psffsd)
{
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_CHGFILEPTRL(PSFFSI psffsi, PVBOXSFFSD psffsd, LONGLONG off, ULONG ulMethod, ULONG IOflag)
{
    return ERROR_NOT_SUPPORTED;
}


/** Forwards the call to FS32_CHGFILEPTRL. */
extern "C" APIRET APIENTRY
FS32_CHGFILEPTR(PSFFSI psffsi, PVBOXSFFSD psffsd, LONG off, ULONG ulMethod, ULONG IOflag)
{
    return FS32_CHGFILEPTRL(psffsi, psffsd, off, ulMethod, IOflag);
}

DECLASM(int)
FS32_FILEINFO(ULONG flag, PSFFSI psffsi, PVBOXSFFSD psffsd, ULONG level,
              PBYTE pData, ULONG cbData, ULONG IOflag)
{
    return ERROR_NOT_SUPPORTED;
}

DECLASM(int)
FS32_NEWSIZEL(PSFFSI psffsi, PVBOXSFFSD psffsd, LONGLONG cbFile, ULONG IOflag)
{
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_READ(PSFFSI psffsi, PVBOXSFFSD psffsd, PVOID pvData, PULONG pcb, ULONG IOflag)
{
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_WRITE(PSFFSI psffsi, PVBOXSFFSD psffsd, PVOID pvData, PULONG pcb, ULONG IOflag)
{
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_READFILEATCACHE(PSFFSI psffsi, PVBOXSFFSD psffsd, ULONG IOflag, LONGLONG off, ULONG pcb, KernCacheList_t **ppCacheList)
{
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_RETURNFILECACHE(KernCacheList_t *pCacheList)
{
    return ERROR_NOT_SUPPORTED;
}


/* oddments */

DECLASM(int)
FS32_CANCELLOCKREQUESTL(PSFFSI psffsi, PVBOXSFFSD psffsd, struct filelockl *pLockRange)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_CANCELLOCKREQUEST(PSFFSI psffsi, PVBOXSFFSD psffsd, struct filelock *pLockRange)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FILELOCKSL(PSFFSI psffsi, PVBOXSFFSD psffsd, struct filelockl *pUnLockRange,
                struct filelockl *pLockRange, ULONG timeout, ULONG flags)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FILELOCKS(PSFFSI psffsi, PVBOXSFFSD psffsd, struct filelock *pUnLockRange,
               struct filelock *pLockRange, ULONG timeout, ULONG flags)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_IOCTL(PSFFSI psffsi, PVBOXSFFSD psffsd, USHORT cat, USHORT func,
           PVOID pParm, USHORT lenParm, PUSHORT plenParmIO,
           PVOID pData, USHORT lenData, PUSHORT plenDataIO)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_FILEIO(PSFFSI psffsi, PVBOXSFFSD psffsd, PBYTE pCmdList, USHORT cbCmdList,
            PUSHORT poError, USHORT IOflag)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_NMPIPE(PSFFSI psffsi, PVBOXSFFSD psffsd, USHORT OpType, union npoper *pOpRec,
            PBYTE pData, PCSZ pszName)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_OPENPAGEFILE(PULONG pFlag, PULONG pcMaxReq, PCSZ pszName, PSFFSI psffsi, PVBOXSFFSD psffsd,
                  USHORT ulOpenMode, USHORT usOpenFlag, USHORT usAttr, ULONG Reserved)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_SETSWAP(PSFFSI psffsi, PVBOXSFFSD psffsd)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_ALLOCATEPAGESPACE(PSFFSI psffsi, PVBOXSFFSD psffsd, ULONG cb, USHORT cbWantContig)
{
    return ERROR_NOT_SUPPORTED;
}


DECLASM(int)
FS32_DOPAGEIO(PSFFSI psffsi, PVBOXSFFSD psffsd, struct PageCmdHeader *pList)
{
    return ERROR_NOT_SUPPORTED;
}

