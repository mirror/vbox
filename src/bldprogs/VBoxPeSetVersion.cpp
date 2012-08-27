/* $Id$ */
/** @file
 * IPRT - Change the OS and SubSystem version to 4.0 (VS2010 trick).
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <Windows.h>
#include <stdio.h>
#include <string.h>


/** @todo Rewrite this so it can take options and print out error messages. */
int main(int argc, char **argv)
{
    if (argc != 2)
        return 30;
    FILE *pFile = fopen(argv[1], "r+b");
    if (!pFile)
        return 1;
    IMAGE_DOS_HEADER MzHdr;
    if (fread(&MzHdr, sizeof(MzHdr), 1, pFile) != 1)
        return 2;

    if (fseek(pFile, MzHdr.e_lfanew, SEEK_SET) != 0)
        return 3;

    IMAGE_NT_HEADERS32 NtHdrs;
    if (fread(&NtHdrs, sizeof(NtHdrs), 1, pFile) != 1)
        return 4;
    if (NtHdrs.Signature != IMAGE_NT_SIGNATURE)
        return 5;
    if (NtHdrs.FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return 6;
    if (NtHdrs.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return 7;

    if (NtHdrs.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return 7;

    IMAGE_NT_HEADERS32 NtHdrsNew = NtHdrs;
    if (NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion > 4)
    {
        NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = 4;
        NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = 0;
    }
    if (NtHdrsNew.OptionalHeader.MajorSubsystemVersion > 4)
    {
        NtHdrsNew.OptionalHeader.MajorSubsystemVersion = 4;
        NtHdrsNew.OptionalHeader.MinorSubsystemVersion = 0;
    }

    if (memcmp(&NtHdrsNew, &NtHdrs, sizeof(NtHdrs)))
    {
        /** @todo calc checksum. */
        NtHdrsNew.OptionalHeader.CheckSum = 0;

        if (fseek(pFile, MzHdr.e_lfanew, SEEK_SET) != 0)
            return 10;
        if (fwrite(&NtHdrsNew, sizeof(NtHdrsNew), 1, pFile) != 1)
            return 11;
    }

    if (fclose(pFile) != 0)
        return 29;
    return 0;
}

