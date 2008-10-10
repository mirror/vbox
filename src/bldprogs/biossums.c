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

typedef unsigned char uint8_t;

static uint8_t abBios[64*1024];

/**
 * Calculate the checksum.
 */
static uint8_t calculateChecksum(uint8_t *pb, size_t cb, unsigned int iChecksum)
{
    uint8_t       u8Sum = 0;
    unsigned int  i;

    for (i = 0; i < cb; i++)
        if (i != iChecksum)
            u8Sum += pb[i];

    return -u8Sum;
}

/**
 * @param   pb        Where to search for the signature
 * @param   cb        Size of the search area
 * @param   pbHeader  Pointer to the start of the signature
 * @returns           0 if signature was not found, 1 if found or
 *                    2 if more than one signature was found */
static int searchHeader(uint8_t *pb, size_t cb, const char *pszHeader, uint8_t **pbHeader)
{
    int          fFound = 0;
    unsigned int i;
    size_t       cbSignature = strlen(pszHeader);

    for (i = 0; i < cb; i += 16)
        if (!memcmp(pb + i, pszHeader, cbSignature))
        {
            if (fFound++)
                return 2;
            *pbHeader = pb + i;
        }

    return fFound;
}

int main(int argc, char **argv)
{
    FILE    *pIn, *pOut;
    size_t  cbIn, cbOut;
    int     fAdapterBios = 0;

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

    fAdapterBios = abBios[0] == 0x55 && abBios[1] == 0xaa;

    /* align size to page size */
    if ((cbIn % 4096) != 0)
        cbIn = (cbIn + 4095) & ~4095;

    if (!fAdapterBios && cbIn != 64*1024)
    {
        printf("Size of system BIOS is not 64KB!\n");
        fclose(pOut);
        exit(-1);
    }

    if (fAdapterBios)
    {
        /* adapter BIOS */
        
        /* set the length indicator */
        abBios[2] = (uint8_t)(cbIn / 512);
    }
    else
    {
        /* system BIOS */
        size_t  cbChecksum;
        uint8_t u8Checksum;
        uint8_t *pbHeader;

        /* Set the BIOS32 header checksum. */
        switch (searchHeader(abBios, cbIn, "_32_", &pbHeader))
        {
            case 0:
                printf("No BIOS32 header not found!\n");
                exit(-1);
            case 2:
                printf("More than one BIOS32 header found!\n");
                exit(-1);
            case 1:
                cbChecksum = (size_t)pbHeader[9] * 16;
                u8Checksum = calculateChecksum(pbHeader, cbChecksum, 10);
                pbHeader[10] = u8Checksum;
                break;
        }

        /* Set the PIR header checksum according to PCI IRQ Routing table
         * specification version 1.0, Microsoft Corporation, 1996 */
        switch (searchHeader(abBios, cbIn, "$PIR", &pbHeader))
        {
            case 0:
                printf("No PCI IRQ routing table found!\n");
                exit(-1);
            case 2:
                printf("More than one PCI IRQ routing table found!\n");
                exit(-1);
            case 1:
                cbChecksum = (size_t)pbHeader[6] + (size_t)pbHeader[7] * 256;
                u8Checksum = calculateChecksum(pbHeader, cbChecksum, 31);
                pbHeader[31] = u8Checksum;
                break;
        }

        /* Set the SMBIOS header checksum according to System Management BIOS
         * Reference Specification Version 2.5, DSP0134. */
        switch (searchHeader(abBios, cbIn, "_SM_", &pbHeader))
        {
            case 0:
                printf("No SMBIOS header found!\n");
                exit(-1);
            case 2:
                printf("More than one SMBIOS header found!\n");
                exit(-1);
            case 1:
                /* at first fix the DMI header starting at SMBIOS header offset 16 */
                u8Checksum = calculateChecksum(pbHeader+16, 15, 5);
                pbHeader[21] = u8Checksum;

                /* now fix the checksum of the whole SMBIOS header */
                cbChecksum = (size_t)pbHeader[5];
                u8Checksum = calculateChecksum(pbHeader, cbChecksum, 4);
                pbHeader[4] = u8Checksum;
                break;
        }
    }

    /* set the BIOS checksum */
    abBios[cbIn-1] = calculateChecksum(abBios, cbIn, cbIn - 1);

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
