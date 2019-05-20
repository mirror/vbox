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

#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <old> <new>\n", argv[0]);
        return 2;
    }

    SetLastError(0);
    if (MoveFileExA(argv[1], argv[2], 0 /*fFlags*/))
        printf("%s: successfully renamed to: %s\n", argv[1], argv[2]);
    else
    {
        fprintf(stderr, "%s: MoveFileExA(,%s,0) failed: %u\n", argv[1], argv[2], GetLastError());
        return 1;
    }
    return 0;
}

