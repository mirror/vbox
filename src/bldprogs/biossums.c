/* $Id$ */
/** @file
 * Tool for modifying a BIOS image to write the BIOS checksum.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


static unsigned char abBios[64*1024];

int main(int argc, char **argv)
{
    FILE          *pIn, *pOut;
    size_t        cbIn, cbOut;
    unsigned int  i;
    unsigned char u8Sum;

    if (argc != 3)
    {
        printf("Input file name and output file name required.\n");
        exit(-1);
    }

    pIn = fopen(argv[1], "rb");
    if (!pIn)
    {
        printf("Error opening '%s' for reading (%s).\n", argv[1], strerror(errno));
        exit(-1);
    }
    
    pOut = fopen(argv[2], "wb");
    if (!pOut)
    {
        printf("Error opening '%s' for writing (%s).\n", argv[2], strerror(errno));
        exit(-1);
    }

    /* safety precaution */
    memset(abBios, 0, sizeof(abBios));

    cbIn = fread(abBios, 1, sizeof(abBios), pIn);
    if (ferror(pIn))
    {
        printf("Error reading from '%s' (%s).\n", argv[1], strerror(errno));
        fclose(pIn);
        exit(-1);
    }
    fclose(pIn);

    /* align size to page size */
    if ((cbIn % 4096) != 0)
        cbIn = (cbIn + 4095) & ~4095;

    /* set the size */
    abBios[2] = (unsigned char)(cbIn / 512);

    /* calculate the checksum */
    u8Sum = 0;
    for (i = 0; i < cbIn - 1; i++)
        u8Sum += abBios[i];

    /* set the checksum */
    abBios[i] = -u8Sum;

    cbOut = fwrite(abBios, 1, cbIn, pOut);
    if (ferror(pOut))
    {
        printf("Error writing to '%s' (%s).\n", argv[2], strerror(errno));
        fclose(pOut);
        exit(-1);
    }

    fclose(pOut);

    return 0;
}
