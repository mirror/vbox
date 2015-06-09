/** @file
 * BIOS POST routines. Used only during initialization.
 */

/*
 * Copyright (C) 2004-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "inlines.h"

#if DEBUG_POST
#  define DPRINT(...) BX_DEBUG(__VA_ARGS__)
#else
#  define DPRINT(...)
#endif

/* In general, checksumming ROMs in a VM just wastes time. */
//#define CHECKSUM_ROMS

/* The format of a ROM is as follows:
 *
 *     ------------------------------
 *   0 | AA55h signature (word)     |
 *     ------------------------------
 *   2 | Size in 512B blocks (byte) |
 *     ------------------------------
 *   3 | Start of executable code   |
 *     |          .......           |
 * end |                            |
 *     ------------------------------
 */

typedef struct rom_hdr_tag {
    uint16_t    signature;
    uint8_t     num_blks;
    uint8_t     code;
} rom_hdr;


/* Calculate the checksum of a ROM. Note that the ROM might be
 * larger than 64K.
 */
static inline uint8_t rom_checksum(uint8_t __far *rom, uint8_t blocks)
{
    uint8_t     sum = 0;

#ifdef CHECKSUM_ROMS
    while (blocks--) {
        int     i;

        for (i = 0; i < 512; ++i)
            sum += rom[i];
        /* Add 512 bytes (32 paragraphs) to segment. */
        rom = MK_FP(FP_SEG(rom) + (512 >> 4), 0);
    }
#endif
    return sum;
}

/* Scan for ROMs in the given range and execute their POST code. */
void rom_scan(uint16_t start_seg, uint16_t end_seg)
{
    rom_hdr __far   *rom;
    uint8_t         rom_blks;

    DPRINT("Scanning for ROMs in %04X-%04X range\n", start_seg, end_seg);

    while (start_seg < end_seg) {
        rom = MK_FP(start_seg, 0);
        /* Check for the ROM signature. */
        if (rom->signature == 0xAA55) {
            DPRINT("Found ROM at segment %04X\n", start_seg);
            if (!rom_checksum((void __far *)rom, rom->num_blks)) {
                void (__far * rom_init)(void);

                /* Checksum good, initialize ROM. */
                rom_init = (void __far *)&rom->code;
                rom_init();
                int_disable();
                DPRINT("ROM initialized\n");

                /* Continue scanning past the end of this ROM. */
                rom_blks   = (rom->num_blks + 3) & ~3;  /* 4 blocks = 2K */
                start_seg += rom_blks / 4;
            }
        } else {
            /* Scanning is done in 2K steps. */
            start_seg += 2048 >> 4;
        }
    }
}
