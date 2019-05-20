/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders FSD - Simple Testcase.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/nt/nt-and-windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        HANDLE hFile = CreateFileA(argv[i], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL /*pSecAttr*/, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
#if 1
            DWORD cbFileHi = 0;
            DWORD cbFileLo = GetFileSize(hFile, &cbFileHi);
            LONG offFileHi = cbFileHi;
            if (SetFilePointer(hFile, cbFileLo + 1, &offFileHi, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
                fprintf(stderr, "%s: error: %s: SetFilePointer() -> %u\n", argv[0], argv[i], GetLastError());
#else
            //if (SetFilePointer(hFile, 0, &offWhateverHi, FILE_END) == INVALID_SET_FILE_POINTER)
            //    fprintf(stderr, "%s: error: %s: SetFilePointer() -> %u\n", argv[0], argv[i], GetLastError());
#endif


            uint8_t         abBuf[64];
            IO_STATUS_BLOCK const IosVirgin = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            IO_STATUS_BLOCK Ios  = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            NTSTATUS        rcNt = NtReadFile(hFile, NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                                              &Ios, abBuf, (ULONG)sizeof(abBuf), NULL /*poffFile*/, NULL /*Key*/);
            Sleep(2);
            if (rcNt == STATUS_END_OF_FILE && Ios.Status == IosVirgin.Status && Ios.Information == IosVirgin.Information)
                fprintf(stderr, "%s: info: %s: PASSED\n", argv[0], argv[i]);
            else
                fprintf(stderr, "%s: info: %s: FAILED - rcNt=%#x (expected %#x) Ios.Status=%#x (expected %#x [untouched]), Info=%p (expected %p)\n",
                        argv[0], argv[i], rcNt, STATUS_END_OF_FILE, Ios.Status, IosVirgin.Status, Ios.Information, IosVirgin.Information);

            CloseHandle(hFile);
        }
        else
            fprintf(stderr, "%s: error: %s: CreateFileA() -> %u\n", argv[0], argv[i], GetLastError());
    }
    return 0;
}

