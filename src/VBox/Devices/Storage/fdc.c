#ifdef VBOX
/** @file
 *
 * VBox storage devices:
 * Floppy disk controller
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU Floppy disk emulator (Intel 82078)
 *
 * Copyright (c) 2003 Jocelyn Mayer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_FDC
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>

#include "Builtins.h"
#include "../vl_vbox.h"

#define MAX_FD 2

#define PDMIBASE_2_FDRIVE(pInterface) \
    ((fdrive_t *)((uintptr_t)(pInterface) - RT_OFFSETOF(fdrive_t, IBase)))

#define PDMIMOUNTNOTIFY_2_FDRIVE(p)  \
    ((fdrive_t *)((uintptr_t)(p) - RT_OFFSETOF(fdrive_t, IMountNotify)))

#endif /* VBOX */

#ifndef VBOX
#include "vl.h"
#endif /* !VBOX */

/********************************************************/
/* debug Floppy devices */
/* #define DEBUG_FLOPPY */

#ifndef VBOX
    #ifdef DEBUG_FLOPPY
    #define FLOPPY_DPRINTF(fmt, args...) \
        do { printf("FLOPPY: " fmt , ##args); } while (0)
    #endif
#else /* !VBOX */
    # ifdef LOG_ENABLED
        static void FLOPPY_DPRINTF (const char *fmt, ...)
        {
            if (LogIsEnabled ()) {
                va_list args;
                va_start (args, fmt);
                RTLogLogger (NULL, NULL, "floppy: %N", fmt, &args); /* %N - nested va_list * type formatting call. */
                va_end (args);
            }
        }
    # else
      DECLINLINE(void) FLOPPY_DPRINTF(const char *pszFmt, ...) {}
    # endif
#endif /* !VBOX */

#ifndef VBOX
#define FLOPPY_ERROR(fmt, args...) \
    do { printf("FLOPPY ERROR: %s: " fmt, __func__ , ##args); } while (0)
#else /* VBOX */
#   define FLOPPY_ERROR RTLogPrintf
#endif /* VBOX */

#ifdef VBOX
typedef struct fdctrl_t fdctrl_t;
#endif /* VBOX */

/********************************************************/
/* Floppy drive emulation                               */

/* Will always be a fixed parameter for us */
#define FD_SECTOR_LEN 512
#define FD_SECTOR_SC  2   /* Sector size code */

/* Floppy disk drive emulation */
typedef enum fdisk_type_t {
    FDRIVE_DISK_288   = 0x01, /* 2.88 MB disk           */
    FDRIVE_DISK_144   = 0x02, /* 1.44 MB disk           */
    FDRIVE_DISK_720   = 0x03, /* 720 kB disk            */
    FDRIVE_DISK_USER  = 0x04, /* User defined geometry  */
    FDRIVE_DISK_NONE  = 0x05  /* No disk                */
} fdisk_type_t;

typedef enum fdrive_type_t {
    FDRIVE_DRV_144  = 0x00,   /* 1.44 MB 3"5 drive      */
    FDRIVE_DRV_288  = 0x01,   /* 2.88 MB 3"5 drive      */
    FDRIVE_DRV_120  = 0x02,   /* 1.2  MB 5"25 drive     */
    FDRIVE_DRV_NONE = 0x03    /* No drive connected     */
} fdrive_type_t;

typedef enum fdrive_flags_t {
    FDRIVE_MOTOR_ON   = 0x01, /* motor on/off           */
    FDRIVE_REVALIDATE = 0x02  /* Revalidated            */
} fdrive_flags_t;

typedef enum fdisk_flags_t {
    FDISK_DBL_SIDES  = 0x01
} fdisk_flags_t;

typedef struct fdrive_t {
#ifndef VBOX
    BlockDriverState *bs;
#else /* VBOX */
    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached driver's block interface. */
    R3PTRTYPE(PPDMIBLOCK)           pDrvBlock;
    /** Pointer to the attached driver's block bios interface. */
    R3PTRTYPE(PPDMIBLOCKBIOS)       pDrvBlockBios;
    /** Pointer to the attached driver's mount interface.
     * This is NULL if the driver isn't a removable unit. */
    R3PTRTYPE(PPDMIMOUNT)           pDrvMount;
    /** The base interface. */
    PDMIBASE                        IBase;
    /** The block port interface. */
    PDMIBLOCKPORT                   IPort;
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY                 IMountNotify;
    /** The LUN #. */
    RTUINT                          iLUN;
    /** The LED for this LUN. */
    PDMLED                          Led;
    /** The Diskette present/missing flag. */
    bool                            fMediaPresent;
#endif
    /* Drive status */
    fdrive_type_t drive;
    fdrive_flags_t drflags;
    uint8_t perpendicular;    /* 2.88 MB access mode    */
    /* Position */
    uint8_t head;
    uint8_t track;
    uint8_t sect;
    /* Last operation status */
    uint8_t dir;              /* Direction              */
    uint8_t rw;               /* Read/write             */
    /* Media */
    fdisk_flags_t flags;
    uint8_t last_sect;        /* Nb sector per track    */
    uint8_t max_track;        /* Nb of tracks           */
    uint16_t bps;             /* Bytes per sector       */
    uint8_t ro;               /* Is read-only           */
} fdrive_t;

#ifndef VBOX
static void fd_init (fdrive_t *drv, BlockDriverState *bs)
{
    /* Drive */
    drv->bs = bs;
    drv->drive = FDRIVE_DRV_NONE;
    drv->drflags = 0;
    drv->perpendicular = 0;
    /* Disk */
    drv->last_sect = 0;
    drv->max_track = 0;
}
#endif

static int _fd_sector (uint8_t head, uint8_t track,
                        uint8_t sect, uint8_t last_sect)
{
    return (((track * 2) + head) * last_sect) + sect - 1;
}

/* Returns current position, in sectors, for given drive */
static int fd_sector (fdrive_t *drv)
{
    return _fd_sector(drv->head, drv->track, drv->sect, drv->last_sect);
}

static int fd_seek (fdrive_t *drv, uint8_t head, uint8_t track, uint8_t sect,
                    int enable_seek)
{
    uint32_t sector;
    int ret;

    if (track > drv->max_track ||
        (head != 0 && (drv->flags & FDISK_DBL_SIDES) == 0)) {
        FLOPPY_DPRINTF("try to read %d %02x %02x (max=%d %d %02x %02x)\n",
                       head, track, sect, 1,
                       (drv->flags & FDISK_DBL_SIDES) == 0 ? 0 : 1,
                       drv->max_track, drv->last_sect);
        return 2;
    }
    if (sect > drv->last_sect) {
        FLOPPY_DPRINTF("try to read %d %02x %02x (max=%d %d %02x %02x)\n",
                       head, track, sect, 1,
                       (drv->flags & FDISK_DBL_SIDES) == 0 ? 0 : 1,
                       drv->max_track, drv->last_sect);
        return 3;
    }
    sector = _fd_sector(head, track, sect, drv->last_sect);
    ret = 0;
    if (sector != fd_sector(drv)) {
#if 0
        if (!enable_seek) {
            FLOPPY_ERROR("no implicit seek %d %02x %02x (max=%d %02x %02x)\n",
                         head, track, sect, 1, drv->max_track, drv->last_sect);
            return 4;
        }
#endif
        drv->head = head;
        if (drv->track != track)
            ret = 1;
        drv->track = track;
        drv->sect = sect;
    }

    return ret;
}

/* Set drive back to track 0 */
static void fd_recalibrate (fdrive_t *drv)
{
    FLOPPY_DPRINTF("recalibrate\n");
    drv->head = 0;
    drv->track = 0;
    drv->sect = 1;
    drv->dir = 1;
    drv->rw = 0;
}

/* Recognize floppy formats */
typedef struct fd_format_t {
    fdrive_type_t drive;
    fdisk_type_t  disk;
    uint8_t last_sect;
    uint8_t max_track;
    uint8_t max_head;
    const char *str;
} fd_format_t;

static fd_format_t fd_formats[] = {
    /* First entry is default format */
    /* 1.44 MB 3"1/2 floppy disks */
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 18, 80, 1, "1.44 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 20, 80, 1,  "1.6 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 21, 80, 1, "1.68 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 21, 82, 1, "1.72 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 21, 83, 1, "1.74 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 22, 80, 1, "1.76 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 23, 80, 1, "1.84 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_144, 24, 80, 1, "1.92 MB 3\"1/2", },
    /* 2.88 MB 3"1/2 floppy disks */
    { FDRIVE_DRV_288, FDRIVE_DISK_288, 36, 80, 1, "2.88 MB 3\"1/2", },
    { FDRIVE_DRV_288, FDRIVE_DISK_288, 39, 80, 1, "3.12 MB 3\"1/2", },
    { FDRIVE_DRV_288, FDRIVE_DISK_288, 40, 80, 1,  "3.2 MB 3\"1/2", },
    { FDRIVE_DRV_288, FDRIVE_DISK_288, 44, 80, 1, "3.52 MB 3\"1/2", },
    { FDRIVE_DRV_288, FDRIVE_DISK_288, 48, 80, 1, "3.84 MB 3\"1/2", },
    /* 720 kB 3"1/2 floppy disks */
    { FDRIVE_DRV_144, FDRIVE_DISK_720,  9, 80, 1,  "720 kB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_720, 10, 80, 1,  "800 kB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_720, 10, 82, 1,  "820 kB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_720, 10, 83, 1,  "830 kB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_720, 13, 80, 1, "1.04 MB 3\"1/2", },
    { FDRIVE_DRV_144, FDRIVE_DISK_720, 14, 80, 1, "1.12 MB 3\"1/2", },
    /* 1.2 MB 5"1/4 floppy disks */
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 15, 80, 1,  "1.2 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 18, 80, 1, "1.44 MB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 18, 82, 1, "1.48 MB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 18, 83, 1, "1.49 MB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 20, 80, 1,  "1.6 MB 5\"1/4", },
    /* 720 kB 5"1/4 floppy disks */
    { FDRIVE_DRV_120, FDRIVE_DISK_288,  9, 80, 1,  "720 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 11, 80, 1,  "880 kB 5\"1/4", },
    /* 360 kB 5"1/4 floppy disks */
    { FDRIVE_DRV_120, FDRIVE_DISK_288,  9, 40, 1,  "360 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288,  9, 40, 0,  "180 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 10, 41, 1,  "410 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288, 10, 42, 1,  "420 kB 5\"1/4", },
    /* 320 kB 5"1/4 floppy disks */
    { FDRIVE_DRV_120, FDRIVE_DISK_288,  8, 40, 1,  "320 kB 5\"1/4", },
    { FDRIVE_DRV_120, FDRIVE_DISK_288,  8, 40, 0,  "160 kB 5\"1/4", },
    /* 360 kB must match 5"1/4 better than 3"1/2... */
    { FDRIVE_DRV_144, FDRIVE_DISK_720,  9, 80, 0,  "360 kB 3\"1/2", },
    /* end */
    { FDRIVE_DRV_NONE, FDRIVE_DISK_NONE, -1, -1, 0, NULL, },
};

/* Revalidate a disk drive after a disk change */
static void fd_revalidate (fdrive_t *drv)
{
    fd_format_t *parse;
    int64_t nb_sectors, size;
    int i, first_match, match;
    int nb_heads, max_track, last_sect, ro;

    FLOPPY_DPRINTF("revalidate\n");
    drv->drflags &= ~FDRIVE_REVALIDATE;
#ifndef VBOX
    if (drv->bs != NULL && bdrv_is_inserted(drv->bs)) {
        ro = bdrv_is_read_only(drv->bs);
        bdrv_get_geometry_hint(drv->bs, &nb_heads, &max_track, &last_sect);
#else /* VBOX */
        /** @todo */ /** @todo r=bird: todo what exactly? */
    if (drv->pDrvBlock
        && drv->pDrvMount
        && drv->pDrvMount->pfnIsMounted (drv->pDrvMount)) {
        ro = drv->pDrvBlock->pfnIsReadOnly (drv->pDrvBlock);
        nb_heads = 0;
        max_track = 0;
        last_sect = 0;
#endif /* VBOX */
        if (nb_heads != 0 && max_track != 0 && last_sect != 0) {
            FLOPPY_DPRINTF("User defined disk (%d %d %d)",
                           nb_heads - 1, max_track, last_sect);
        } else {
#ifndef VBOX
            bdrv_get_geometry(drv->bs, &nb_sectors);
#else /* VBOX */
            /* @todo */ /** @todo r=bird: todo what exactly?!?!? */
            {
                uint64_t size =  drv->pDrvBlock->pfnGetSize (drv->pDrvBlock);
                nb_sectors = size / 512;
            }
#endif /* VBOX */
            match = -1;
            first_match = -1;
            for (i = 0;; i++) {
                parse = &fd_formats[i];
                if (parse->drive == FDRIVE_DRV_NONE)
                    break;
                if (drv->drive == parse->drive ||
                    drv->drive == FDRIVE_DRV_NONE) {
                    size = (parse->max_head + 1) * parse->max_track *
                        parse->last_sect;
                    if (nb_sectors == size) {
                        match = i;
                        break;
                    }
                    if (first_match == -1)
                        first_match = i;
                }
            }
            if (match == -1) {
                if (first_match == -1)
                    match = 1;
                else
                    match = first_match;
                parse = &fd_formats[match];
            }
            nb_heads = parse->max_head + 1;
            max_track = parse->max_track;
            last_sect = parse->last_sect;
            drv->drive = parse->drive;
            FLOPPY_DPRINTF("%s floppy disk (%d h %d t %d s) %s\n", parse->str,
                           nb_heads, max_track, last_sect, ro ? "ro" : "rw");
        }
        if (nb_heads == 1) {
            drv->flags &= ~FDISK_DBL_SIDES;
        } else {
            drv->flags |= FDISK_DBL_SIDES;
        }
        drv->max_track = max_track;
        drv->last_sect = last_sect;
        drv->ro = ro;
#ifdef VBOX
        drv->fMediaPresent = true;
#endif
    } else {
        FLOPPY_DPRINTF("No disk in drive\n");
        drv->last_sect = 0;
        drv->max_track = 0;
        drv->flags &= ~FDISK_DBL_SIDES;
#ifdef VBOX
        drv->fMediaPresent = false;
#endif
    }
    drv->drflags |= FDRIVE_REVALIDATE;
}

/* Motor control */
static void fd_start (fdrive_t *drv)
{
    drv->drflags |= FDRIVE_MOTOR_ON;
}

static void fd_stop (fdrive_t *drv)
{
    drv->drflags &= ~FDRIVE_MOTOR_ON;
}

/* Re-initialise a drives (motor off, repositioned) */
static void fd_reset (fdrive_t *drv)
{
    fd_stop(drv);
    fd_recalibrate(drv);
}

/********************************************************/
/* Intel 82078 floppy disk controller emulation          */

#define target_ulong uint32_t
static void fdctrl_reset (fdctrl_t *fdctrl, int do_irq);
static void fdctrl_reset_fifo (fdctrl_t *fdctrl);
#ifndef VBOX
static int fdctrl_transfer_handler (void *opaque, int nchan, int dma_pos, int dma_len);
#else /* VBOX: */
static DECLCALLBACK(uint32_t) fdctrl_transfer_handler (PPDMDEVINS pDevIns,
                                                       void *opaque,
                                                       unsigned nchan,
                                                       uint32_t dma_pos,
                                                       uint32_t dma_len);
#endif /* VBOX */
static void fdctrl_raise_irq (fdctrl_t *fdctrl, uint8_t status);
static void fdctrl_result_timer(void *opaque);

static uint32_t fdctrl_read_statusB (fdctrl_t *fdctrl);
static uint32_t fdctrl_read_dor (fdctrl_t *fdctrl);
static void fdctrl_write_dor (fdctrl_t *fdctrl, uint32_t value);
static uint32_t fdctrl_read_tape (fdctrl_t *fdctrl);
static void fdctrl_write_tape (fdctrl_t *fdctrl, uint32_t value);
static uint32_t fdctrl_read_main_status (fdctrl_t *fdctrl);
static void fdctrl_write_rate (fdctrl_t *fdctrl, uint32_t value);
static uint32_t fdctrl_read_data (fdctrl_t *fdctrl);
static void fdctrl_write_data (fdctrl_t *fdctrl, uint32_t value);
static uint32_t fdctrl_read_dir (fdctrl_t *fdctrl);

enum {
    FD_CTRL_ACTIVE = 0x01, /* XXX: suppress that */
    FD_CTRL_RESET  = 0x02,
    FD_CTRL_SLEEP  = 0x04, /* XXX: suppress that */
    FD_CTRL_BUSY   = 0x08, /* dma transfer in progress */
    FD_CTRL_INTR   = 0x10
};

enum {
    FD_DIR_WRITE   = 0,
    FD_DIR_READ    = 1,
    FD_DIR_SCANE   = 2,
    FD_DIR_SCANL   = 3,
    FD_DIR_SCANH   = 4
};

enum {
    FD_STATE_CMD    = 0x00,
    FD_STATE_STATUS = 0x01,
    FD_STATE_DATA   = 0x02,
    FD_STATE_STATE  = 0x03,
    FD_STATE_MULTI  = 0x10,
    FD_STATE_SEEK   = 0x20,
    FD_STATE_FORMAT = 0x40
};

#define FD_STATE(state) ((state) & FD_STATE_STATE)
#define FD_SET_STATE(state, new_state) \
do { (state) = ((state) & ~FD_STATE_STATE) | (new_state); } while (0)
#define FD_MULTI_TRACK(state) ((state) & FD_STATE_MULTI)
#define FD_DID_SEEK(state) ((state) & FD_STATE_SEEK)
#define FD_FORMAT_CMD(state) ((state) & FD_STATE_FORMAT)

struct fdctrl_t {
#ifndef VBOX
    fdctrl_t *fdctrl;
#endif
    /* Controller's identification */
    uint8_t version;
    /* HW */
#ifndef VBOX
    int irq_lvl;
    int dma_chann;
#else
    uint8_t irq_lvl;
    uint8_t dma_chann;
#endif
    uint32_t io_base;
    /* Controller state */
    QEMUTimer *result_timer;
    uint8_t state;
    uint8_t dma_en;
    uint8_t cur_drv;
    uint8_t bootsel;
    /* Command FIFO */
    uint8_t fifo[FD_SECTOR_LEN];
    uint32_t data_pos;
    uint32_t data_len;
    uint8_t data_state;
    uint8_t data_dir;
    uint8_t int_status;
    uint8_t eot; /* last wanted sector */
    /* States kept only to be returned back */
    /* Timers state */
    uint8_t timer0;
    uint8_t timer1;
    /* precompensation */
    uint8_t precomp_trk;
    uint8_t config;
    uint8_t lock;
    /* Power down config (also with status regB access mode */
    uint8_t pwrd;
    /* Floppy drives */
    fdrive_t drives[2];
#ifdef VBOX
    /** Pointer to device instance. */
    PPDMDEVINS pDevIns;

    /** Status Port - The base interface. */
    PDMIBASE IBaseStatus;
    /** Status Port - The Leds interface. */
    PDMILEDPORTS ILeds;
    /** Status Port - The Partner of ILeds. */
    PPDMILEDCONNECTORS pLedsConnector;
#endif
};

static uint32_t fdctrl_read (void *opaque, uint32_t reg)
{
    fdctrl_t *fdctrl = opaque;
    uint32_t retval;

    switch (reg & 0x07) {
    case 0x01:
        retval = fdctrl_read_statusB(fdctrl);
        break;
    case 0x02:
        retval = fdctrl_read_dor(fdctrl);
        break;
    case 0x03:
        retval = fdctrl_read_tape(fdctrl);
        break;
    case 0x04:
        retval = fdctrl_read_main_status(fdctrl);
        break;
    case 0x05:
        retval = fdctrl_read_data(fdctrl);
        break;
    case 0x07:
        retval = fdctrl_read_dir(fdctrl);
        break;
    default:
        retval = (uint32_t)(-1);
        break;
    }
    FLOPPY_DPRINTF("read reg%d: 0x%02x\n", reg & 7, retval);

    return retval;
}

static void fdctrl_write (void *opaque, uint32_t reg, uint32_t value)
{
    fdctrl_t *fdctrl = opaque;

    FLOPPY_DPRINTF("write reg%d: 0x%02x\n", reg & 7, value);

    switch (reg & 0x07) {
    case 0x02:
        fdctrl_write_dor(fdctrl, value);
        break;
    case 0x03:
        fdctrl_write_tape(fdctrl, value);
        break;
    case 0x04:
        fdctrl_write_rate(fdctrl, value);
        break;
    case 0x05:
        fdctrl_write_data(fdctrl, value);
        break;
    default:
        break;
    }
}

#ifndef VBOX
static void fd_change_cb (void *opaque)
{
    fdrive_t *drv = opaque;

    FLOPPY_DPRINTF("disk change\n");
    fd_revalidate(drv);
#if 0
    fd_recalibrate(drv);
    fdctrl_reset_fifo(drv->fdctrl);
    fdctrl_raise_irq(drv->fdctrl, 0x20);
#endif
}

#else  /* VBOX */
/**
 * Called when a media is mounted.
 *
 * @param   pInterface      Pointer to the interface structure
 *                          containing the called function pointer.
 */
static DECLCALLBACK(void) fdMountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    fdrive_t *drv = PDMIMOUNTNOTIFY_2_FDRIVE (pInterface);
    LogFlow(("fdMountNotify:\n"));
    fd_revalidate (drv);
}

/**
 * Called when a media is unmounted
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) fdUnmountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    fdrive_t *drv = PDMIMOUNTNOTIFY_2_FDRIVE (pInterface);
    LogFlow(("fdUnmountNotify:\n"));
    fd_revalidate (drv);
}
#endif

#ifndef VBOX
fdctrl_t *fdctrl_init (int irq_lvl, int dma_chann, int mem_mapped,
                       uint32_t io_base,
                       BlockDriverState **fds)
{
    fdctrl_t *fdctrl;
/* //    int io_mem; */
    int i;

    FLOPPY_DPRINTF("init controller\n");
    fdctrl = qemu_mallocz(sizeof(fdctrl_t));
    if (!fdctrl)
        return NULL;
    fdctrl->result_timer = qemu_new_timer(vm_clock,
                                          fdctrl_result_timer, fdctrl);

    fdctrl->version = 0x90; /* Intel 82078 controller */
    fdctrl->irq_lvl = irq_lvl;
    fdctrl->dma_chann = dma_chann;
    fdctrl->io_base = io_base;
    fdctrl->config = 0x60; /* Implicit seek, polling & FIFO enabled */
    if (fdctrl->dma_chann != -1) {
        fdctrl->dma_en = 1;
        DMA_register_channel(dma_chann, &fdctrl_transfer_handler, fdctrl);
    } else {
        fdctrl->dma_en = 0;
    }
    for (i = 0; i < 2; i++) {
        fd_init(&fdctrl->drives[i], fds[i]);
        if (fds[i]) {
            bdrv_set_change_cb(fds[i],
                               &fd_change_cb, &fdctrl->drives[i]);
        }
    }
    fdctrl_reset(fdctrl, 0);
    fdctrl->state = FD_CTRL_ACTIVE;
    if (mem_mapped) {
        FLOPPY_ERROR("memory mapped floppy not supported by now !\n");
#if 0
        io_mem = cpu_register_io_memory(0, fdctrl_mem_read, fdctrl_mem_write);
        cpu_register_physical_memory(base, 0x08, io_mem);
#endif
    } else {
        register_ioport_read(io_base + 0x01, 5, 1, &fdctrl_read, fdctrl);
        register_ioport_read(io_base + 0x07, 1, 1, &fdctrl_read, fdctrl);
        register_ioport_write(io_base + 0x01, 5, 1, &fdctrl_write, fdctrl);
        register_ioport_write(io_base + 0x07, 1, 1, &fdctrl_write, fdctrl);
    }
    for (i = 0; i < 2; i++) {
        fd_revalidate(&fdctrl->drives[i]);
    }

    return fdctrl;
}

/* XXX: may change if moved to bdrv */
int fdctrl_get_drive_type(fdctrl_t *fdctrl, int drive_num)
{
    return fdctrl->drives[drive_num].drive;
}
#endif

/* Change IRQ state */
static void fdctrl_reset_irq (fdctrl_t *fdctrl)
{
    FLOPPY_DPRINTF("Reset interrupt\n");
#ifdef VBOX
    PDMDevHlpISASetIrq (fdctrl->pDevIns, fdctrl->irq_lvl, 0);
#else
    pic_set_irq(fdctrl->irq_lvl, 0);
#endif
    fdctrl->state &= ~FD_CTRL_INTR;
}

static void fdctrl_raise_irq (fdctrl_t *fdctrl, uint8_t status)
{
    if (~(fdctrl->state & FD_CTRL_INTR)) {
#ifdef VBOX
        PDMDevHlpISASetIrq (fdctrl->pDevIns, fdctrl->irq_lvl, 1);
#else
        pic_set_irq(fdctrl->irq_lvl, 1);
#endif
        fdctrl->state |= FD_CTRL_INTR;
    }
    FLOPPY_DPRINTF("Set interrupt status to 0x%02x\n", status);
    fdctrl->int_status = status;
}

/* Reset controller */
static void fdctrl_reset (fdctrl_t *fdctrl, int do_irq)
{
    int i;

    FLOPPY_DPRINTF("reset controller\n");
    fdctrl_reset_irq(fdctrl);
    /* Initialise controller */
    fdctrl->cur_drv = 0;
    /* FIFO state */
    fdctrl->data_pos = 0;
    fdctrl->data_len = 0;
    fdctrl->data_state = FD_STATE_CMD;
    fdctrl->data_dir = FD_DIR_WRITE;
    for (i = 0; i < MAX_FD; i++)
        fd_reset(&fdctrl->drives[i]);
    fdctrl_reset_fifo(fdctrl);
    if (do_irq)
        fdctrl_raise_irq(fdctrl, 0xc0);
}

static inline fdrive_t *drv0 (fdctrl_t *fdctrl)
{
    return &fdctrl->drives[fdctrl->bootsel];
}

static inline fdrive_t *drv1 (fdctrl_t *fdctrl)
{
    return &fdctrl->drives[1 - fdctrl->bootsel];
}

static fdrive_t *get_cur_drv (fdctrl_t *fdctrl)
{
    return fdctrl->cur_drv == 0 ? drv0(fdctrl) : drv1(fdctrl);
}

/* Status B register : 0x01 (read-only) */
static uint32_t fdctrl_read_statusB (fdctrl_t *fdctrl)
{
    FLOPPY_DPRINTF("status register: 0x00\n");
    return 0;
}

/* Digital output register : 0x02 */
static uint32_t fdctrl_read_dor (fdctrl_t *fdctrl)
{
    uint32_t retval = 0;

    /* Drive motors state indicators */
#ifndef VBOX
    if (drv0(fdctrl)->drflags & FDRIVE_MOTOR_ON)
        retval |= 1 << 5;
    if (drv1(fdctrl)->drflags & FDRIVE_MOTOR_ON)
        retval |= 1 << 4;
#else
    /* bit4: 0 = drive 0 motor off/1 = on */
    if (drv0(fdctrl)->drflags & FDRIVE_MOTOR_ON)
        retval |= RT_BIT(4);
    /* bit5: 0 = drive 1 motor off/1 = on */
    if (drv1(fdctrl)->drflags & FDRIVE_MOTOR_ON)
        retval |= RT_BIT(5);
#endif
    /* DMA enable */
    retval |= fdctrl->dma_en << 3;
    /* Reset indicator */
    retval |= (fdctrl->state & FD_CTRL_RESET) == 0 ? 0x04 : 0;
    /* Selected drive */
    retval |= fdctrl->cur_drv;
    FLOPPY_DPRINTF("digital output register: 0x%02x\n", retval);

    return retval;
}

static void fdctrl_write_dor (fdctrl_t *fdctrl, uint32_t value)
{
    /* Reset mode */
    if (fdctrl->state & FD_CTRL_RESET) {
        if (!(value & 0x04)) {
            FLOPPY_DPRINTF("Floppy controller in RESET state !\n");
            return;
        }
    }
    FLOPPY_DPRINTF("digital output register set to 0x%02x\n", value);
    /* Drive motors state indicators */
    if (value & 0x20)
        fd_start(drv1(fdctrl));
    else
        fd_stop(drv1(fdctrl));
    if (value & 0x10)
        fd_start(drv0(fdctrl));
    else
        fd_stop(drv0(fdctrl));
    /* DMA enable */
#if 0
    if (fdctrl->dma_chann != -1)
        fdctrl->dma_en = 1 - ((value >> 3) & 1);
#endif
    /* Reset */
    if (!(value & 0x04)) {
        if (!(fdctrl->state & FD_CTRL_RESET)) {
            FLOPPY_DPRINTF("controller enter RESET state\n");
            fdctrl->state |= FD_CTRL_RESET;
        }
    } else {
        if (fdctrl->state & FD_CTRL_RESET) {
            FLOPPY_DPRINTF("controller out of RESET state\n");
            fdctrl_reset(fdctrl, 1);
            fdctrl->state &= ~(FD_CTRL_RESET | FD_CTRL_SLEEP);
        }
    }
    /* Selected drive */
    fdctrl->cur_drv = value & 1;
}

/* Tape drive register : 0x03 */
static uint32_t fdctrl_read_tape (fdctrl_t *fdctrl)
{
    uint32_t retval = 0;

    /* Disk boot selection indicator */
    retval |= fdctrl->bootsel << 2;
    /* Tape indicators: never allowed */
    FLOPPY_DPRINTF("tape drive register: 0x%02x\n", retval);

    return retval;
}

static void fdctrl_write_tape (fdctrl_t *fdctrl, uint32_t value)
{
    /* Reset mode */
    if (fdctrl->state & FD_CTRL_RESET) {
        FLOPPY_DPRINTF("Floppy controller in RESET state !\n");
        return;
    }
    FLOPPY_DPRINTF("tape drive register set to 0x%02x\n", value);
    /* Disk boot selection indicator */
    fdctrl->bootsel = (value >> 2) & 1;
    /* Tape indicators: never allow */
}

/* Main status register : 0x04 (read) */
static uint32_t fdctrl_read_main_status (fdctrl_t *fdctrl)
{
    uint32_t retval = 0;

    fdctrl->state &= ~(FD_CTRL_SLEEP | FD_CTRL_RESET);
    if (!(fdctrl->state & FD_CTRL_BUSY)) {
        /* Data transfer allowed */
        retval |= 0x80;
        /* Data transfer direction indicator */
        if (fdctrl->data_dir == FD_DIR_READ)
            retval |= 0x40;
    }
    /* Should handle 0x20 for SPECIFY command */
    /* Command busy indicator */
    if (FD_STATE(fdctrl->data_state) == FD_STATE_DATA ||
        FD_STATE(fdctrl->data_state) == FD_STATE_STATUS)
        retval |= 0x10;
    FLOPPY_DPRINTF("main status register: 0x%02x\n", retval);

    return retval;
}

/* Data select rate register : 0x04 (write) */
static void fdctrl_write_rate (fdctrl_t *fdctrl, uint32_t value)
{
    /* Reset mode */
    if (fdctrl->state & FD_CTRL_RESET) {
            FLOPPY_DPRINTF("Floppy controller in RESET state !\n");
            return;
        }
    FLOPPY_DPRINTF("select rate register set to 0x%02x\n", value);
    /* Reset: autoclear */
    if (value & 0x80) {
        fdctrl->state |= FD_CTRL_RESET;
        fdctrl_reset(fdctrl, 1);
        fdctrl->state &= ~FD_CTRL_RESET;
    }
    if (value & 0x40) {
        fdctrl->state |= FD_CTRL_SLEEP;
        fdctrl_reset(fdctrl, 1);
    }
/* //        fdctrl.precomp = (value >> 2) & 0x07; */
}

/* Digital input register : 0x07 (read-only) */
static uint32_t fdctrl_read_dir (fdctrl_t *fdctrl)
{
    uint32_t retval = 0;

#ifndef VBOX
    if (drv0(fdctrl)->drflags & FDRIVE_REVALIDATE ||
        drv1(fdctrl)->drflags & FDRIVE_REVALIDATE)
#else
    fdrive_t *cur_drv = get_cur_drv(fdctrl);
    if (    drv0(fdctrl)->drflags & FDRIVE_REVALIDATE
        ||  drv1(fdctrl)->drflags & FDRIVE_REVALIDATE
        ||  !cur_drv->fMediaPresent)
#endif
        retval |= 0x80;
    if (retval != 0)
        FLOPPY_DPRINTF("Floppy digital input register: 0x%02x\n", retval);
    drv0(fdctrl)->drflags &= ~FDRIVE_REVALIDATE;
    drv1(fdctrl)->drflags &= ~FDRIVE_REVALIDATE;

    return retval;
}

/* FIFO state control */
static void fdctrl_reset_fifo (fdctrl_t *fdctrl)
{
    fdctrl->data_dir = FD_DIR_WRITE;
    fdctrl->data_pos = 0;
    FD_SET_STATE(fdctrl->data_state, FD_STATE_CMD);
}

/* Set FIFO status for the host to read */
static void fdctrl_set_fifo (fdctrl_t *fdctrl, int fifo_len, int do_irq)
{
    fdctrl->data_dir = FD_DIR_READ;
    fdctrl->data_len = fifo_len;
    fdctrl->data_pos = 0;
    FD_SET_STATE(fdctrl->data_state, FD_STATE_STATUS);
    if (do_irq)
        fdctrl_raise_irq(fdctrl, 0x00);
}

/* Set an error: unimplemented/unknown command */
static void fdctrl_unimplemented (fdctrl_t *fdctrl)
{
#if 0
    fdrive_t *cur_drv;

    cur_drv = get_cur_drv(fdctrl);
    fdctrl->fifo[0] = 0x60 | (cur_drv->head << 2) | fdctrl->cur_drv;
    fdctrl->fifo[1] = 0x00;
    fdctrl->fifo[2] = 0x00;
    fdctrl_set_fifo(fdctrl, 3, 1);
#else
    /* //    fdctrl_reset_fifo(fdctrl); */
    fdctrl->fifo[0] = 0x80;
    fdctrl_set_fifo(fdctrl, 1, 0);
#endif
}

/* Callback for transfer end (stop or abort) */
static void fdctrl_stop_transfer (fdctrl_t *fdctrl, uint8_t status0,
                                  uint8_t status1, uint8_t status2)
{
    fdrive_t *cur_drv;

    cur_drv = get_cur_drv(fdctrl);
    FLOPPY_DPRINTF("transfer status: %02x %02x %02x (%02x)\n",
                   status0, status1, status2,
                   status0 | (cur_drv->head << 2) | fdctrl->cur_drv);
    fdctrl->fifo[0] = status0 | (cur_drv->head << 2) | fdctrl->cur_drv;
    fdctrl->fifo[1] = status1;
    fdctrl->fifo[2] = status2;
    fdctrl->fifo[3] = cur_drv->track;
    fdctrl->fifo[4] = cur_drv->head;
    fdctrl->fifo[5] = cur_drv->sect;
    fdctrl->fifo[6] = FD_SECTOR_SC;
    fdctrl->data_dir = FD_DIR_READ;
    if (fdctrl->state & FD_CTRL_BUSY) {
#ifdef VBOX
        PDMDevHlpDMASetDREQ (fdctrl->pDevIns, fdctrl->dma_chann, 0);
#else
        DMA_release_DREQ(fdctrl->dma_chann);
#endif
        fdctrl->state &= ~FD_CTRL_BUSY;
    }
    fdctrl_set_fifo(fdctrl, 7, 1);
}

/* Prepare a data transfer (either DMA or FIFO) */
static void fdctrl_start_transfer (fdctrl_t *fdctrl, int direction)
{
    fdrive_t *cur_drv;
    uint8_t kh, kt, ks;
    int did_seek;

    fdctrl->cur_drv = fdctrl->fifo[1] & 1;
    cur_drv = get_cur_drv(fdctrl);
    kt = fdctrl->fifo[2];
    kh = fdctrl->fifo[3];
    ks = fdctrl->fifo[4];
    FLOPPY_DPRINTF("Start transfer at %d %d %02x %02x (%d)\n",
                   fdctrl->cur_drv, kh, kt, ks,
                   _fd_sector(kh, kt, ks, cur_drv->last_sect));
    did_seek = 0;
    switch (fd_seek(cur_drv, kh, kt, ks, fdctrl->config & 0x40)) {
    case 2:
        /* sect too big */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 3:
        /* track too big */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x80, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 4:
        /* No seek enabled */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 1:
        did_seek = 1;
        break;
    default:
        break;
    }
    /* Set the FIFO state */
    fdctrl->data_dir = direction;
    fdctrl->data_pos = 0;
    FD_SET_STATE(fdctrl->data_state, FD_STATE_DATA); /* FIFO ready for data */
    if (fdctrl->fifo[0] & 0x80)
        fdctrl->data_state |= FD_STATE_MULTI;
    else
        fdctrl->data_state &= ~FD_STATE_MULTI;
    if (did_seek)
        fdctrl->data_state |= FD_STATE_SEEK;
    else
        fdctrl->data_state &= ~FD_STATE_SEEK;
    if (fdctrl->fifo[5] == 00) {
        fdctrl->data_len = fdctrl->fifo[8];
    } else {
        int tmp;
        fdctrl->data_len = 128 << fdctrl->fifo[5];
        tmp = (cur_drv->last_sect - ks + 1);
        if (fdctrl->fifo[0] & 0x80)
            tmp += cur_drv->last_sect;
        fdctrl->data_len *= tmp;
    }
    fdctrl->eot = fdctrl->fifo[6];
    if (fdctrl->dma_en) {
        int dma_mode;
        /* DMA transfer are enabled. Check if DMA channel is well programmed */
#ifndef VBOX
        dma_mode = DMA_get_channel_mode(fdctrl->dma_chann);
#else
        dma_mode = PDMDevHlpDMAGetChannelMode (fdctrl->pDevIns, fdctrl->dma_chann);
#endif
        dma_mode = (dma_mode >> 2) & 3;
        FLOPPY_DPRINTF("dma_mode=%d direction=%d (%d - %d)\n",
                       dma_mode, direction,
                       (128 << fdctrl->fifo[5]) *
                       (cur_drv->last_sect - ks + 1), fdctrl->data_len);
        if (((direction == FD_DIR_SCANE || direction == FD_DIR_SCANL ||
              direction == FD_DIR_SCANH) && dma_mode == 0) ||
            (direction == FD_DIR_WRITE && dma_mode == 2) ||
            (direction == FD_DIR_READ && dma_mode == 1)) {
            /* No access is allowed until DMA transfer has completed */
            fdctrl->state |= FD_CTRL_BUSY;
            /* Now, we just have to wait for the DMA controller to
             * recall us...
             */
#ifndef VBOX
            DMA_hold_DREQ(fdctrl->dma_chann);
            DMA_schedule(fdctrl->dma_chann);
#else
            PDMDevHlpDMASetDREQ (fdctrl->pDevIns, fdctrl->dma_chann, 1);
            PDMDevHlpDMASchedule (fdctrl->pDevIns);
#endif
            return;
        } else {
            FLOPPY_ERROR("dma_mode=%d direction=%d\n", dma_mode, direction);
        }
    }
    FLOPPY_DPRINTF("start non-DMA transfer\n");
    /* IO based transfer: calculate len */
    fdctrl_raise_irq(fdctrl, 0x00);

    return;
}

/* Prepare a transfer of deleted data */
static void fdctrl_start_transfer_del (fdctrl_t *fdctrl, int direction)
{
    /* We don't handle deleted data,
     * so we don't return *ANYTHING*
     */
    fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
}

#if 0
#include <stdio.h>
#include <stdlib.h>

static FILE * f;
static void dump (void *p, size_t len)
{
    size_t n;

    if (!f) {
        f = fopen ("dump", "wb");
        if (!f)
            exit (0);
    }

    n = fwrite (p, len, 1, f);
    if (n != 1) {
        AssertMsgFailed (("failed to dump\n"));
        exit (0);
    }
}
#else
#define dump(a,b) do { } while (0)
#endif

/* handlers for DMA transfers */
#ifdef VBOX
static DECLCALLBACK(uint32_t) fdctrl_transfer_handler (PPDMDEVINS pDevIns,
                                                       void *opaque,
                                                       unsigned nchan,
                                                       uint32_t dma_pos,
                                                       uint32_t dma_len)
#else
static int fdctrl_transfer_handler (void *opaque, int nchan,
                                    int dma_pos, int dma_len)
#endif
{
    fdctrl_t *fdctrl;
    fdrive_t *cur_drv;
#ifdef VBOX
    int rc;
    uint32_t len, start_pos, rel_pos;
#else
    int len, start_pos, rel_pos;
#endif
    uint8_t status0 = 0x00, status1 = 0x00, status2 = 0x00;

    fdctrl = opaque;
    if (!(fdctrl->state & FD_CTRL_BUSY)) {
        FLOPPY_DPRINTF("Not in DMA transfer mode !\n");
        return 0;
    }
    cur_drv = get_cur_drv(fdctrl);
    if (fdctrl->data_dir == FD_DIR_SCANE || fdctrl->data_dir == FD_DIR_SCANL ||
        fdctrl->data_dir == FD_DIR_SCANH)
        status2 = 0x04;
    if (dma_len > fdctrl->data_len)
        dma_len = fdctrl->data_len;
#ifndef VBOX
    if (cur_drv->bs == NULL) {
#else  /* !VBOX */
    if (cur_drv->pDrvBlock == NULL) {
#endif
        if (fdctrl->data_dir == FD_DIR_WRITE)
            fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
        else
            fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        len = 0;
        goto transfer_error;
    }
    rel_pos = fdctrl->data_pos % FD_SECTOR_LEN;
    for (start_pos = fdctrl->data_pos; fdctrl->data_pos < dma_len;) {
        len = dma_len - fdctrl->data_pos;
        if (len + rel_pos > FD_SECTOR_LEN)
            len = FD_SECTOR_LEN - rel_pos;
        FLOPPY_DPRINTF("copy %d bytes (%d %d %d) %d pos %d %02x "
                       "(%d-0x%08x 0x%08x)\n", len, dma_len, fdctrl->data_pos,
                       fdctrl->data_len, fdctrl->cur_drv, cur_drv->head,
                       cur_drv->track, cur_drv->sect, fd_sector(cur_drv),
                       fd_sector(cur_drv) * 512);
        if (fdctrl->data_dir != FD_DIR_WRITE ||
            len < FD_SECTOR_LEN || rel_pos != 0) {
            /* READ & SCAN commands and realign to a sector for WRITE */
#ifndef VBOX
            if (bdrv_read(cur_drv->bs, fd_sector(cur_drv),
                          fdctrl->fifo, 1) < 0) {
                FLOPPY_DPRINTF("Floppy: error getting sector %d\n",
                               fd_sector(cur_drv));
                /* Sure, image size is too small... */
                memset(fdctrl->fifo, 0, FD_SECTOR_LEN);
            }
#else
            cur_drv->Led.Asserted.s.fReading
                = cur_drv->Led.Actual.s.fReading = 1;

            rc = cur_drv->pDrvBlock->pfnRead (
                cur_drv->pDrvBlock,
                fd_sector (cur_drv) * 512,
                fdctrl->fifo,
                1 * 512
                );

            cur_drv->Led.Actual.s.fReading = 0;

            if (VBOX_FAILURE (rc)) {
                AssertMsgFailed (
                    ("Floppy: error getting sector %d, rc = %Vrc",
                     fd_sector (cur_drv), rc));
                memset(fdctrl->fifo, 0, FD_SECTOR_LEN);
            }
#endif
        }
        switch (fdctrl->data_dir) {
        case FD_DIR_READ:
            /* READ commands */
#ifdef VBOX
            {
                uint32_t read;
                int rc = PDMDevHlpDMAWriteMemory(fdctrl->pDevIns, nchan,
                                                 fdctrl->fifo + rel_pos,
                                                 fdctrl->data_pos,
                                                 len, &read);
                dump (fdctrl->fifo + rel_pos, len);
                AssertMsgRC (rc, ("DMAWriteMemory -> %Vrc\n", rc));
            }
#else
            DMA_write_memory (nchan, fdctrl->fifo + rel_pos,
                              fdctrl->data_pos, len);
#endif
/*          cpu_physical_memory_write(addr + fdctrl->data_pos, */
/*                                    fdctrl->fifo + rel_pos, len); */
            break;
        case FD_DIR_WRITE:
            /* WRITE commands */
#ifdef VBOX
            {
                uint32_t written;
                int rc = PDMDevHlpDMAReadMemory(fdctrl->pDevIns, nchan,
                                                fdctrl->fifo + rel_pos,
                                                fdctrl->data_pos,
                                                len, &written);
                AssertMsgRC (rc, ("DMAReadMemory -> %Vrc\n", rc));
            }
#else
            DMA_read_memory (nchan, fdctrl->fifo + rel_pos,
                             fdctrl->data_pos, len);
#endif
/*             cpu_physical_memory_read(addr + fdctrl->data_pos, */
/*                                   fdctrl->fifo + rel_pos, len); */
#ifndef VBOX
            if (bdrv_write(cur_drv->bs, fd_sector(cur_drv),
                           fdctrl->fifo, 1) < 0) {
                FLOPPY_ERROR("writting sector %d\n", fd_sector(cur_drv));
                fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
                goto transfer_error;
            }
#else  /* VBOX */
            cur_drv->Led.Asserted.s.fWriting
                = cur_drv->Led.Actual.s.fWriting = 1;

            rc = cur_drv->pDrvBlock->pfnWrite (
                cur_drv->pDrvBlock,
                fd_sector (cur_drv) * 512,
                fdctrl->fifo,
                1 * 512
                );

            cur_drv->Led.Actual.s.fWriting = 0;

            if (VBOX_FAILURE (rc)) {
                AssertMsgFailed (("Floppy: error writing sector %d. rc=%Vrc",
                                  fd_sector (cur_drv), rc));
                fdctrl_stop_transfer (fdctrl, 0x60, 0x00, 0x00);
                goto transfer_error;
            }
#endif
            break;
        default:
            /* SCAN commands */
            {
                uint8_t tmpbuf[FD_SECTOR_LEN];
                int ret;
#ifdef VBOX
                int rc;
                uint32_t read;

                rc = PDMDevHlpDMAReadMemory (fdctrl->pDevIns, nchan, tmpbuf,
                                             fdctrl->data_pos, len, &read);
                AssertMsg (VBOX_SUCCESS (rc), ("DMAReadMemory -> %Vrc\n", rc));
#else
                DMA_read_memory (nchan, tmpbuf, fdctrl->data_pos, len);
#endif
/*                 cpu_physical_memory_read(addr + fdctrl->data_pos, */
/*                                          tmpbuf, len); */
                ret = memcmp(tmpbuf, fdctrl->fifo + rel_pos, len);
                if (ret == 0) {
                    status2 = 0x08;
                    goto end_transfer;
                }
                if ((ret < 0 && fdctrl->data_dir == FD_DIR_SCANL) ||
                    (ret > 0 && fdctrl->data_dir == FD_DIR_SCANH)) {
                    status2 = 0x00;
                    goto end_transfer;
                }
            }
            break;
        }
        fdctrl->data_pos += len;
        rel_pos = fdctrl->data_pos % FD_SECTOR_LEN;
        if (rel_pos == 0) {
            /* Seek to next sector */
            FLOPPY_DPRINTF("seek to next sector (%d %02x %02x => %d) (%d)\n",
                           cur_drv->head, cur_drv->track, cur_drv->sect,
                           fd_sector(cur_drv),
                           fdctrl->data_pos - len);
            /* XXX: cur_drv->sect >= cur_drv->last_sect should be an
               error in fact */
            if (cur_drv->sect >= cur_drv->last_sect ||
                cur_drv->sect == fdctrl->eot) {
                cur_drv->sect = 1;
                if (FD_MULTI_TRACK(fdctrl->data_state)) {
                    if (cur_drv->head == 0 &&
                        (cur_drv->flags & FDISK_DBL_SIDES) != 0) {
                        cur_drv->head = 1;
                    } else {
                        cur_drv->head = 0;
                        cur_drv->track++;
                        if ((cur_drv->flags & FDISK_DBL_SIDES) == 0)
                            break;
                    }
                } else {
                    cur_drv->track++;
                    break;
                }
                FLOPPY_DPRINTF("seek to next track (%d %02x %02x => %d)\n",
                               cur_drv->head, cur_drv->track,
                               cur_drv->sect, fd_sector(cur_drv));
            } else {
                cur_drv->sect++;
            }
        }
    }
end_transfer:
    len = fdctrl->data_pos - start_pos;
    FLOPPY_DPRINTF("end transfer %d %d %d\n",
                   fdctrl->data_pos, len, fdctrl->data_len);
    if (fdctrl->data_dir == FD_DIR_SCANE ||
        fdctrl->data_dir == FD_DIR_SCANL ||
        fdctrl->data_dir == FD_DIR_SCANH)
        status2 = 0x08;
    if (FD_DID_SEEK(fdctrl->data_state))
        status0 |= 0x20;
    fdctrl->data_len -= len;
    /* if (fdctrl->data_len == 0) */
    fdctrl_stop_transfer(fdctrl, status0, status1, status2);
transfer_error:

    return len;
}

#if 0
{
    fdctrl_t *fdctrl;
    fdrive_t *cur_drv;
    int len, start_pos, rel_pos;
    uint8_t status0 = 0x00, status1 = 0x00, status2 = 0x00;

    fdctrl = opaque;
    if (!(fdctrl->state & FD_CTRL_BUSY)) {
        FLOPPY_DPRINTF("Not in DMA transfer mode !\n");
        return 0;
    }
    cur_drv = get_cur_drv(fdctrl);
    if (fdctrl->data_dir == FD_DIR_SCANE || fdctrl->data_dir == FD_DIR_SCANL ||
        fdctrl->data_dir == FD_DIR_SCANH)
        status2 = 0x04;
    if (size > fdctrl->data_len)
        size = fdctrl->data_len;
#ifndef VBOX
    if (cur_drv->bs == NULL) {
#else
    if (cur_drv->pDrvBase == NULL) {
#endif
        if (fdctrl->data_dir == FD_DIR_WRITE)
            fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
        else
            fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        len = 0;
        goto transfer_error;
    }
    rel_pos = fdctrl->data_pos % FD_SECTOR_LEN;
    for (start_pos = fdctrl->data_pos; fdctrl->data_pos < size;) {
        len = size - fdctrl->data_pos;
        if (len + rel_pos > FD_SECTOR_LEN)
            len = FD_SECTOR_LEN - rel_pos;
        FLOPPY_DPRINTF("copy %d bytes (%d %d %d) %d pos %d %02x %02x "
                       "(%d-0x%08x 0x%08x)\n", len, size, fdctrl->data_pos,
                       fdctrl->data_len, fdctrl->cur_drv, cur_drv->head,
                       cur_drv->track, cur_drv->sect, fd_sector(cur_drv),
                       fd_sector(cur_drv) * 512, addr);
        if (fdctrl->data_dir != FD_DIR_WRITE ||
            len < FD_SECTOR_LEN || rel_pos != 0) {
            /* READ & SCAN commands and realign to a sector for WRITE */
#ifndef VBOX
            if (bdrv_read(cur_drv->bs, fd_sector(cur_drv),
                          fdctrl->fifo, 1) < 0) {
                FLOPPY_DPRINTF("Floppy: error getting sector %d\n",
                               fd_sector(cur_drv));
                /* Sure, image size is too small... */
                memset(fdctrl->fifo, 0, FD_SECTOR_LEN);
            }
#else
            int rc;

            cur_drv->Led.Asserted.s.fReading
                = cur_drv->Led.Actual.s.fReading = 1;

            rc = cur_drv->pDrvBlock->pfnRead (
                cur_drv->pDrvBlock,
                fd_sector (cur_drv) * 512,
                fdctrl->fifo,
                1 * 512
                );

            cur_drv->Led.Actual.s.fReading = 0;

            if (VBOX_FAILURE (rc)) {
                AssertMsgFailed (
                    ("Floppy: error getting sector %d. rc=%Vrc\n",
                     fd_sector(cur_drv), rc));
                /* Sure, image size is too small... */
                memset(fdctrl->fifo, 0, FD_SECTOR_LEN);
            }
            /* *p_fd_activity = 1; */
#endif
        }
        switch (fdctrl->data_dir) {
        case FD_DIR_READ:
            /* READ commands */
#ifdef VBOX
            PDMDevHlpPhysWrite (fdctrl->pDevIns, addr + fdctrl->data_pos,
                                fdctrl->fifo + rel_pos, len);
#else
            cpu_physical_memory_write(addr + fdctrl->data_pos,
                                      fdctrl->fifo + rel_pos, len);
#endif
            break;
        case FD_DIR_WRITE:
            /* WRITE commands */
#ifdef VBOX
            {
                int rc;

                PDMDevHlpPhysRead (fdctrl->pDevIns, addr + fdctrl->data_pos,
                                   fdctrl->fifo + rel_pos, len);

                cur_drv->Led.Asserted.s.fWriting
                    = cur_drv->Led.Actual.s.fWriting = 1;

                rc = cur_drv->pDrvBlock->pfnWrite (
                    cur_drv->pDrvBlock,
                    fd_sector (cur_drv) * 512,
                    fdctrl->fifo,
                    1 * 512
                    );

                cur_drv->Led.Actual.s.fWriting = 0;

                if (VBOX_FAILURE (rc)) {
                    AssertMsgFailed (
                        ("Floppy: error writting sector %d. rc=%Vrc\n",
                         fd_sector(cur_drv), rc));
                    fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
                    goto transfer_error;
                }
            }
            /* *p_fd_activity = 2; */
#else  /* !VBOX */
            cpu_physical_memory_read(addr + fdctrl->data_pos,
                                     fdctrl->fifo + rel_pos, len);
            if (bdrv_write(cur_drv->bs, fd_sector(cur_drv),
                           fdctrl->fifo, 1) < 0) {
                FLOPPY_ERROR("writting sector %d\n", fd_sector(cur_drv));
                fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
                goto transfer_error;
            }
#endif
            break;
        default:
            /* SCAN commands */
            {
                uint8_t tmpbuf[FD_SECTOR_LEN];
                int ret;
#ifdef VBOX
                PDMDevHlpPhysRead (fdctrl->pDevIns, addr + fdctrl->data_pos,
                                   tmpbuf, len);
#else
                cpu_physical_memory_read(addr + fdctrl->data_pos,
                                         tmpbuf, len);
#endif
                ret = memcmp(tmpbuf, fdctrl->fifo + rel_pos, len);
                if (ret == 0) {
                    status2 = 0x08;
                    goto end_transfer;
                }
                if ((ret < 0 && fdctrl->data_dir == FD_DIR_SCANL) ||
                    (ret > 0 && fdctrl->data_dir == FD_DIR_SCANH)) {
                    status2 = 0x00;
                    goto end_transfer;
                }
            }
            break;
        }
        fdctrl->data_pos += len;
        rel_pos = fdctrl->data_pos % FD_SECTOR_LEN;
        if (rel_pos == 0) {
            /* Seek to next sector */
            FLOPPY_DPRINTF("seek to next sector (%d %02x %02x => %d) (%d)\n",
                           cur_drv->head, cur_drv->track, cur_drv->sect,
                           fd_sector(cur_drv),
                           fdctrl->data_pos - size);
            /* XXX: cur_drv->sect >= cur_drv->last_sect should be an
               error in fact */
            if (cur_drv->sect >= cur_drv->last_sect ||
                cur_drv->sect == fdctrl->eot) {
                cur_drv->sect = 1;
                if (FD_MULTI_TRACK(fdctrl->data_state)) {
                    if (cur_drv->head == 0 &&
                        (cur_drv->flags & FDISK_DBL_SIDES) != 0) {
                        cur_drv->head = 1;
                    } else {
                        cur_drv->head = 0;
                        cur_drv->track++;
                        if ((cur_drv->flags & FDISK_DBL_SIDES) == 0)
                            break;
                    }
                } else {
                    cur_drv->track++;
                    break;
                }
                FLOPPY_DPRINTF("seek to next track (%d %02x %02x => %d)\n",
                               cur_drv->head, cur_drv->track,
                               cur_drv->sect, fd_sector(cur_drv));
            } else {
                cur_drv->sect++;
            }
        }
    }
end_transfer:
    len = fdctrl->data_pos - start_pos;
    FLOPPY_DPRINTF("end transfer %d %d %d\n",
                   fdctrl->data_pos, len, fdctrl->data_len);
    if (fdctrl->data_dir == FD_DIR_SCANE ||
        fdctrl->data_dir == FD_DIR_SCANL ||
        fdctrl->data_dir == FD_DIR_SCANH)
        status2 = 0x08;
    if (FD_DID_SEEK(fdctrl->data_state))
        status0 |= 0x20;
    fdctrl->data_len -= len;
    /* //    if (fdctrl->data_len == 0) */
    fdctrl_stop_transfer(fdctrl, status0, status1, status2);
transfer_error:

    return len;
}
#endif

/* Data register : 0x05 */
static uint32_t fdctrl_read_data (fdctrl_t *fdctrl)
{
    fdrive_t *cur_drv;
    uint32_t retval = 0;
    int pos, len;
#ifdef VBOX
    int rc;
#endif

    cur_drv = get_cur_drv(fdctrl);
    fdctrl->state &= ~FD_CTRL_SLEEP;
    if (FD_STATE(fdctrl->data_state) == FD_STATE_CMD) {
        FLOPPY_ERROR("can't read data in CMD state\n");
        return 0;
    }
    pos = fdctrl->data_pos;
    if (FD_STATE(fdctrl->data_state) == FD_STATE_DATA) {
        pos %= FD_SECTOR_LEN;
        if (pos == 0) {
            len = fdctrl->data_len - fdctrl->data_pos;
            if (len > FD_SECTOR_LEN)
                len = FD_SECTOR_LEN;
#ifdef VBOX
            cur_drv->Led.Asserted.s.fReading
                = cur_drv->Led.Actual.s.fReading = 1;

            rc = cur_drv->pDrvBlock->pfnRead (
                cur_drv->pDrvBlock,
                fd_sector (cur_drv) * 512,
                fdctrl->fifo,
                len * 512
                );

            cur_drv->Led.Actual.s.fReading = 0;

            if (VBOX_FAILURE (rc)) {
                AssertMsgFailed (
                    ("Floppy: Failure to read sector %d. rc=%Vrc",
                     fd_sector (cur_drv), rc));
            }
#else
            bdrv_read(cur_drv->bs, fd_sector(cur_drv),
                      fdctrl->fifo, len);
#endif
        }
    }
    retval = fdctrl->fifo[pos];
    if (++fdctrl->data_pos == fdctrl->data_len) {
        fdctrl->data_pos = 0;
        /* Switch from transfer mode to status mode
         * then from status mode to command mode
         */
        if (FD_STATE(fdctrl->data_state) == FD_STATE_DATA) {
            fdctrl_stop_transfer(fdctrl, 0x20, 0x00, 0x00);
        } else {
            fdctrl_reset_fifo(fdctrl);
            fdctrl_reset_irq(fdctrl);
        }
    }
    FLOPPY_DPRINTF("data register: 0x%02x\n", retval);

    return retval;
}

static void fdctrl_format_sector (fdctrl_t *fdctrl)
{
    fdrive_t *cur_drv;
    uint8_t kh, kt, ks;
    int did_seek;
#ifdef VBOX
    int ok = 0, rc;
#endif

    fdctrl->cur_drv = fdctrl->fifo[1] & 1;
    cur_drv = get_cur_drv(fdctrl);
    kt = fdctrl->fifo[6];
    kh = fdctrl->fifo[7];
    ks = fdctrl->fifo[8];
    FLOPPY_DPRINTF("format sector at %d %d %02x %02x (%d)\n",
                   fdctrl->cur_drv, kh, kt, ks,
                   _fd_sector(kh, kt, ks, cur_drv->last_sect));
    did_seek = 0;
    switch (fd_seek(cur_drv, kh, kt, ks, fdctrl->config & 0x40)) {
    case 2:
        /* sect too big */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 3:
        /* track too big */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x80, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 4:
        /* No seek enabled */
        fdctrl_stop_transfer(fdctrl, 0x40, 0x00, 0x00);
        fdctrl->fifo[3] = kt;
        fdctrl->fifo[4] = kh;
        fdctrl->fifo[5] = ks;
        return;
    case 1:
        did_seek = 1;
        fdctrl->data_state |= FD_STATE_SEEK;
        break;
    default:
        break;
    }
    memset(fdctrl->fifo, 0, FD_SECTOR_LEN);
#ifdef VBOX
    /* *p_fd_activity = 2; */
    if (cur_drv->pDrvBlock) {
        cur_drv->Led.Asserted.s.fWriting = cur_drv->Led.Actual.s.fWriting = 1;

        rc = cur_drv->pDrvBlock->pfnWrite (
            cur_drv->pDrvBlock,
            fd_sector (cur_drv) * 512,
            fdctrl->fifo,
            1 * 512
            );

        cur_drv->Led.Actual.s.fWriting = 0;

        if (VBOX_FAILURE (rc)) {
            AssertMsgFailed (("Floppy: error formating sector %d. rc=%Vrc\n",
                              fd_sector (cur_drv), rc));
            fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
        }
        else {
            ok = 1;
        }
    }
    if (ok) {
#else
    if (cur_drv->bs == NULL ||
        bdrv_write(cur_drv->bs, fd_sector(cur_drv), fdctrl->fifo, 1) < 0) {
        FLOPPY_ERROR("formating sector %d\n", fd_sector(cur_drv));
        fdctrl_stop_transfer(fdctrl, 0x60, 0x00, 0x00);
    } else {
#endif
        if (cur_drv->sect == cur_drv->last_sect) {
            fdctrl->data_state &= ~FD_STATE_FORMAT;
            /* Last sector done */
            if (FD_DID_SEEK(fdctrl->data_state))
                fdctrl_stop_transfer(fdctrl, 0x20, 0x00, 0x00);
            else
                fdctrl_stop_transfer(fdctrl, 0x00, 0x00, 0x00);
        } else {
            /* More to do */
            fdctrl->data_pos = 0;
            fdctrl->data_len = 4;
        }
    }
}

static void fdctrl_write_data (fdctrl_t *fdctrl, uint32_t value)
{
    fdrive_t *cur_drv;

    cur_drv = get_cur_drv(fdctrl);
    /* Reset mode */
    if (fdctrl->state & FD_CTRL_RESET) {
        FLOPPY_DPRINTF("Floppy controller in RESET state !\n");
        return;
    }
    fdctrl->state &= ~FD_CTRL_SLEEP;
    if (FD_STATE(fdctrl->data_state) == FD_STATE_STATUS) {
        FLOPPY_ERROR("can't write data in status mode\n");
        return;
    }
    /* Is it write command time ? */
    if (FD_STATE(fdctrl->data_state) == FD_STATE_DATA) {
        /* FIFO data write */
        fdctrl->fifo[fdctrl->data_pos++] = value;
        if (fdctrl->data_pos % FD_SECTOR_LEN == (FD_SECTOR_LEN - 1) ||
            fdctrl->data_pos == fdctrl->data_len) {
#ifdef VBOX
            int rc;
            /* *p_fd_activity = 2; */
            cur_drv->Led.Asserted.s.fWriting
                = cur_drv->Led.Actual.s.fWriting = 1;

            rc = cur_drv->pDrvBlock->pfnWrite (
                cur_drv->pDrvBlock,
                fd_sector (cur_drv) * 512,
                fdctrl->fifo,
                FD_SECTOR_LEN * 512
                );

            cur_drv->Led.Actual.s.fWriting = 0;

            if (VBOX_FAILURE (rc)) {
                AssertMsgFailed (
                    ("Floppy: failed to write sector %d. rc=%Vrc\n",
                     fd_sector (cur_drv), rc));
            }
#else
            bdrv_write(cur_drv->bs, fd_sector(cur_drv),
                       fdctrl->fifo, FD_SECTOR_LEN);
#endif
        }
        /* Switch from transfer mode to status mode
         * then from status mode to command mode
         */
        if (FD_STATE(fdctrl->data_state) == FD_STATE_DATA)
            fdctrl_stop_transfer(fdctrl, 0x20, 0x00, 0x00);
        return;
    }
    if (fdctrl->data_pos == 0) {
        /* Command */
        switch (value & 0x5F) {
        case 0x46:
            /* READ variants */
            FLOPPY_DPRINTF("READ command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x4C:
            /* READ_DELETED variants */
            FLOPPY_DPRINTF("READ_DELETED command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x50:
            /* SCAN_EQUAL variants */
            FLOPPY_DPRINTF("SCAN_EQUAL command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x56:
            /* VERIFY variants */
            FLOPPY_DPRINTF("VERIFY command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x59:
            /* SCAN_LOW_OR_EQUAL variants */
            FLOPPY_DPRINTF("SCAN_LOW_OR_EQUAL command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x5D:
            /* SCAN_HIGH_OR_EQUAL variants */
            FLOPPY_DPRINTF("SCAN_HIGH_OR_EQUAL command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        default:
            break;
        }
        switch (value & 0x7F) {
        case 0x45:
            /* WRITE variants */
            FLOPPY_DPRINTF("WRITE command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x49:
            /* WRITE_DELETED variants */
            FLOPPY_DPRINTF("WRITE_DELETED command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        default:
            break;
        }
        switch (value) {
        case 0x03:
            /* SPECIFY */
            FLOPPY_DPRINTF("SPECIFY command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 3;
            goto enqueue;
        case 0x04:
            /* SENSE_DRIVE_STATUS */
            FLOPPY_DPRINTF("SENSE_DRIVE_STATUS command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 2;
            goto enqueue;
        case 0x07:
            /* RECALIBRATE */
            FLOPPY_DPRINTF("RECALIBRATE command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 2;
            goto enqueue;
        case 0x08:
            /* SENSE_INTERRUPT_STATUS */
            FLOPPY_DPRINTF("SENSE_INTERRUPT_STATUS command (%02x)\n",
                           fdctrl->int_status);
            /* No parameters cmd: returns status if no interrupt */
#if 0
            fdctrl->fifo[0] =
                fdctrl->int_status | (cur_drv->head << 2) | fdctrl->cur_drv;
#else
            /* XXX: int_status handling is broken for read/write
               commands, so we do this hack. It should be suppressed
               ASAP */
            fdctrl->fifo[0] =
                0x20 | (cur_drv->head << 2) | fdctrl->cur_drv;
#endif
            fdctrl->fifo[1] = cur_drv->track;
            fdctrl_set_fifo(fdctrl, 2, 0);
            fdctrl_reset_irq(fdctrl);
            fdctrl->int_status = 0xC0;
            return;
        case 0x0E:
            /* DUMPREG */
            FLOPPY_DPRINTF("DUMPREG command\n");
            /* Drives position */
            fdctrl->fifo[0] = drv0(fdctrl)->track;
            fdctrl->fifo[1] = drv1(fdctrl)->track;
            fdctrl->fifo[2] = 0;
            fdctrl->fifo[3] = 0;
            /* timers */
            fdctrl->fifo[4] = fdctrl->timer0;
            fdctrl->fifo[5] = (fdctrl->timer1 << 1) | fdctrl->dma_en;
            fdctrl->fifo[6] = cur_drv->last_sect;
            fdctrl->fifo[7] = (fdctrl->lock << 7) |
                    (cur_drv->perpendicular << 2);
            fdctrl->fifo[8] = fdctrl->config;
            fdctrl->fifo[9] = fdctrl->precomp_trk;
            fdctrl_set_fifo(fdctrl, 10, 0);
            return;
        case 0x0F:
            /* SEEK */
            FLOPPY_DPRINTF("SEEK command\n");
            /* 2 parameters cmd */
            fdctrl->data_len = 3;
            goto enqueue;
        case 0x10:
            /* VERSION */
            FLOPPY_DPRINTF("VERSION command\n");
            /* No parameters cmd */
            /* Controller's version */
            fdctrl->fifo[0] = fdctrl->version;
            fdctrl_set_fifo(fdctrl, 1, 1);
            return;
        case 0x12:
            /* PERPENDICULAR_MODE */
            FLOPPY_DPRINTF("PERPENDICULAR_MODE command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 2;
            goto enqueue;
        case 0x13:
            /* CONFIGURE */
            FLOPPY_DPRINTF("CONFIGURE command\n");
            /* 3 parameters cmd */
            fdctrl->data_len = 4;
            goto enqueue;
        case 0x14:
            /* UNLOCK */
            FLOPPY_DPRINTF("UNLOCK command\n");
            /* No parameters cmd */
            fdctrl->lock = 0;
            fdctrl->fifo[0] = 0;
            fdctrl_set_fifo(fdctrl, 1, 0);
            return;
        case 0x17:
            /* POWERDOWN_MODE */
            FLOPPY_DPRINTF("POWERDOWN_MODE command\n");
            /* 2 parameters cmd */
            fdctrl->data_len = 3;
            goto enqueue;
        case 0x18:
            /* PART_ID */
            FLOPPY_DPRINTF("PART_ID command\n");
            /* No parameters cmd */
            fdctrl->fifo[0] = 0x41; /* Stepping 1 */
            fdctrl_set_fifo(fdctrl, 1, 0);
            return;
        case 0x2C:
            /* SAVE */
            FLOPPY_DPRINTF("SAVE command\n");
            /* No parameters cmd */
            fdctrl->fifo[0] = 0;
            fdctrl->fifo[1] = 0;
            /* Drives position */
            fdctrl->fifo[2] = drv0(fdctrl)->track;
            fdctrl->fifo[3] = drv1(fdctrl)->track;
            fdctrl->fifo[4] = 0;
            fdctrl->fifo[5] = 0;
            /* timers */
            fdctrl->fifo[6] = fdctrl->timer0;
            fdctrl->fifo[7] = fdctrl->timer1;
            fdctrl->fifo[8] = cur_drv->last_sect;
            fdctrl->fifo[9] = (fdctrl->lock << 7) |
                    (cur_drv->perpendicular << 2);
            fdctrl->fifo[10] = fdctrl->config;
            fdctrl->fifo[11] = fdctrl->precomp_trk;
            fdctrl->fifo[12] = fdctrl->pwrd;
            fdctrl->fifo[13] = 0;
            fdctrl->fifo[14] = 0;
            fdctrl_set_fifo(fdctrl, 15, 1);
            return;
        case 0x33:
            /* OPTION */
            FLOPPY_DPRINTF("OPTION command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 2;
            goto enqueue;
        case 0x42:
            /* READ_TRACK */
            FLOPPY_DPRINTF("READ_TRACK command\n");
            /* 8 parameters cmd */
            fdctrl->data_len = 9;
            goto enqueue;
        case 0x4A:
            /* READ_ID */
            FLOPPY_DPRINTF("READ_ID command\n");
            /* 1 parameter cmd */
            fdctrl->data_len = 2;
            goto enqueue;
        case 0x4C:
            /* RESTORE */
            FLOPPY_DPRINTF("RESTORE command\n");
            /* 17 parameters cmd */
            fdctrl->data_len = 18;
            goto enqueue;
        case 0x4D:
            /* FORMAT_TRACK */
            FLOPPY_DPRINTF("FORMAT_TRACK command\n");
            /* 5 parameters cmd */
            fdctrl->data_len = 6;
            goto enqueue;
        case 0x8E:
            /* DRIVE_SPECIFICATION_COMMAND */
            FLOPPY_DPRINTF("DRIVE_SPECIFICATION_COMMAND command\n");
            /* 5 parameters cmd */
            fdctrl->data_len = 6;
            goto enqueue;
        case 0x8F:
            /* RELATIVE_SEEK_OUT */
            FLOPPY_DPRINTF("RELATIVE_SEEK_OUT command\n");
            /* 2 parameters cmd */
            fdctrl->data_len = 3;
            goto enqueue;
        case 0x94:
            /* LOCK */
            FLOPPY_DPRINTF("LOCK command\n");
            /* No parameters cmd */
            fdctrl->lock = 1;
            fdctrl->fifo[0] = 0x10;
            fdctrl_set_fifo(fdctrl, 1, 1);
            return;
        case 0xCD:
            /* FORMAT_AND_WRITE */
            FLOPPY_DPRINTF("FORMAT_AND_WRITE command\n");
            /* 10 parameters cmd */
            fdctrl->data_len = 11;
            goto enqueue;
        case 0xCF:
            /* RELATIVE_SEEK_IN */
            FLOPPY_DPRINTF("RELATIVE_SEEK_IN command\n");
            /* 2 parameters cmd */
            fdctrl->data_len = 3;
            goto enqueue;
        default:
            /* Unknown command */
            FLOPPY_ERROR("unknown command: 0x%02x\n", value);
            fdctrl_unimplemented(fdctrl);
            return;
        }
    }
enqueue:
    FLOPPY_DPRINTF("%s: %02x\n", __func__, value);
    fdctrl->fifo[fdctrl->data_pos] = value;
    if (++fdctrl->data_pos == fdctrl->data_len) {
        /* We now have all parameters
         * and will be able to treat the command
         */
        if (fdctrl->data_state & FD_STATE_FORMAT) {
            fdctrl_format_sector(fdctrl);
            return;
        }
        switch (fdctrl->fifo[0] & 0x1F) {
        case 0x06:
        {
            /* READ variants */
            FLOPPY_DPRINTF("treat READ command\n");
            fdctrl_start_transfer(fdctrl, FD_DIR_READ);
            return;
        }
        case 0x0C:
            /* READ_DELETED variants */
/* //            FLOPPY_DPRINTF("treat READ_DELETED command\n"); */
            FLOPPY_ERROR("treat READ_DELETED command\n");
            fdctrl_start_transfer_del(fdctrl, FD_DIR_READ);
            return;
        case 0x16:
            /* VERIFY variants */
/* //            FLOPPY_DPRINTF("treat VERIFY command\n"); */
            FLOPPY_ERROR("treat VERIFY command\n");
            fdctrl_stop_transfer(fdctrl, 0x20, 0x00, 0x00);
            return;
        case 0x10:
            /* SCAN_EQUAL variants */
/* //            FLOPPY_DPRINTF("treat SCAN_EQUAL command\n"); */
            FLOPPY_ERROR("treat SCAN_EQUAL command\n");
            fdctrl_start_transfer(fdctrl, FD_DIR_SCANE);
            return;
        case 0x19:
            /* SCAN_LOW_OR_EQUAL variants */
/* //            FLOPPY_DPRINTF("treat SCAN_LOW_OR_EQUAL command\n"); */
            FLOPPY_ERROR("treat SCAN_LOW_OR_EQUAL command\n");
            fdctrl_start_transfer(fdctrl, FD_DIR_SCANL);
            return;
        case 0x1D:
            /* SCAN_HIGH_OR_EQUAL variants */
/* //            FLOPPY_DPRINTF("treat SCAN_HIGH_OR_EQUAL command\n"); */
            FLOPPY_ERROR("treat SCAN_HIGH_OR_EQUAL command\n");
            fdctrl_start_transfer(fdctrl, FD_DIR_SCANH);
            return;
        default:
            break;
        }
        switch (fdctrl->fifo[0] & 0x3F) {
        case 0x05:
            /* WRITE variants */
            FLOPPY_DPRINTF("treat WRITE command (%02x)\n", fdctrl->fifo[0]);
            fdctrl_start_transfer(fdctrl, FD_DIR_WRITE);
            return;
        case 0x09:
            /* WRITE_DELETED variants */
/* //            FLOPPY_DPRINTF("treat WRITE_DELETED command\n"); */
            FLOPPY_ERROR("treat WRITE_DELETED command\n");
            fdctrl_start_transfer_del(fdctrl, FD_DIR_WRITE);
            return;
        default:
            break;
        }
        switch (fdctrl->fifo[0]) {
        case 0x03:
            /* SPECIFY */
            FLOPPY_DPRINTF("treat SPECIFY command\n");
            fdctrl->timer0 = (fdctrl->fifo[1] >> 4) & 0xF;
            fdctrl->timer1 = fdctrl->fifo[2] >> 1;
            fdctrl->dma_en = 1 - (fdctrl->fifo[2] & 1) ;
            /* No result back */
            fdctrl_reset_fifo(fdctrl);
            break;
        case 0x04:
            /* SENSE_DRIVE_STATUS */
            FLOPPY_DPRINTF("treat SENSE_DRIVE_STATUS command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            cur_drv->head = (fdctrl->fifo[1] >> 2) & 1;
            /* 1 Byte status back */
            fdctrl->fifo[0] = (cur_drv->ro << 6) |
                (cur_drv->track == 0 ? 0x10 : 0x00) |
                (cur_drv->head << 2) |
                fdctrl->cur_drv |
                0x28;
            fdctrl_set_fifo(fdctrl, 1, 0);
            break;
        case 0x07:
            /* RECALIBRATE */
            FLOPPY_DPRINTF("treat RECALIBRATE command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            fd_recalibrate(cur_drv);
            fdctrl_reset_fifo(fdctrl);
            /* Raise Interrupt */
            fdctrl_raise_irq(fdctrl, 0x20);
            break;
        case 0x0F:
            /* SEEK */
            FLOPPY_DPRINTF("treat SEEK command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            fd_start(cur_drv);
            if (fdctrl->fifo[2] <= cur_drv->track)
                cur_drv->dir = 1;
            else
                cur_drv->dir = 0;
            fdctrl_reset_fifo(fdctrl);
#ifndef VBOX
            if (fdctrl->fifo[2] > cur_drv->max_track) {
#else
            if (    fdctrl->fifo[2] > cur_drv->max_track
                &&  cur_drv->fMediaPresent) {
#endif
                fdctrl_raise_irq(fdctrl, 0x60);
            } else {
                cur_drv->track = fdctrl->fifo[2];
                /* Raise Interrupt */
                fdctrl_raise_irq(fdctrl, 0x20);
            }
            break;
        case 0x12:
            /* PERPENDICULAR_MODE */
            FLOPPY_DPRINTF("treat PERPENDICULAR_MODE command\n");
            if (fdctrl->fifo[1] & 0x80)
                cur_drv->perpendicular = fdctrl->fifo[1] & 0x7;
            /* No result back */
            fdctrl_reset_fifo(fdctrl);
            break;
        case 0x13:
            /* CONFIGURE */
            FLOPPY_DPRINTF("treat CONFIGURE command\n");
            fdctrl->config = fdctrl->fifo[2];
            fdctrl->precomp_trk =  fdctrl->fifo[3];
            /* No result back */
            fdctrl_reset_fifo(fdctrl);
            break;
        case 0x17:
            /* POWERDOWN_MODE */
            FLOPPY_DPRINTF("treat POWERDOWN_MODE command\n");
            fdctrl->pwrd = fdctrl->fifo[1];
            fdctrl->fifo[0] = fdctrl->fifo[1];
            fdctrl_set_fifo(fdctrl, 1, 1);
            break;
        case 0x33:
            /* OPTION */
            FLOPPY_DPRINTF("treat OPTION command\n");
            /* No result back */
            fdctrl_reset_fifo(fdctrl);
            break;
        case 0x42:
            /* READ_TRACK */
/* //            FLOPPY_DPRINTF("treat READ_TRACK command\n"); */
            FLOPPY_ERROR("treat READ_TRACK command\n");
            fdctrl_start_transfer(fdctrl, FD_DIR_READ);
            break;
        case 0x4A:
                /* READ_ID */
            FLOPPY_DPRINTF("treat READ_ID command\n");
            /* XXX: should set main status register to busy */
            cur_drv->head = (fdctrl->fifo[1] >> 2) & 1;
#ifdef VBOX
            TMTimerSetMillies(fdctrl->result_timer, 1000 / 50);
#else
            qemu_mod_timer(fdctrl->result_timer,
                           qemu_get_clock(vm_clock) + (ticks_per_sec / 50));
#endif
            break;
        case 0x4C:
            /* RESTORE */
            FLOPPY_DPRINTF("treat RESTORE command\n");
            /* Drives position */
            drv0(fdctrl)->track = fdctrl->fifo[3];
            drv1(fdctrl)->track = fdctrl->fifo[4];
            /* timers */
            fdctrl->timer0 = fdctrl->fifo[7];
            fdctrl->timer1 = fdctrl->fifo[8];
            cur_drv->last_sect = fdctrl->fifo[9];
            fdctrl->lock = fdctrl->fifo[10] >> 7;
            cur_drv->perpendicular = (fdctrl->fifo[10] >> 2) & 0xF;
            fdctrl->config = fdctrl->fifo[11];
            fdctrl->precomp_trk = fdctrl->fifo[12];
            fdctrl->pwrd = fdctrl->fifo[13];
            fdctrl_reset_fifo(fdctrl);
            break;
        case 0x4D:
            /* FORMAT_TRACK */
            FLOPPY_DPRINTF("treat FORMAT_TRACK command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            fdctrl->data_state |= FD_STATE_FORMAT;
            if (fdctrl->fifo[0] & 0x80)
                fdctrl->data_state |= FD_STATE_MULTI;
            else
                fdctrl->data_state &= ~FD_STATE_MULTI;
            fdctrl->data_state &= ~FD_STATE_SEEK;
            cur_drv->bps =
                fdctrl->fifo[2] > 7 ? 16384 : 128 << fdctrl->fifo[2];
#if 0
            cur_drv->last_sect =
                cur_drv->flags & FDISK_DBL_SIDES ? fdctrl->fifo[3] :
                fdctrl->fifo[3] / 2;
#else
            cur_drv->last_sect = fdctrl->fifo[3];
#endif
            /* Bochs BIOS is buggy and don't send format informations
             * for each sector. So, pretend all's done right now...
             */
            fdctrl->data_state &= ~FD_STATE_FORMAT;
            fdctrl_stop_transfer(fdctrl, 0x00, 0x00, 0x00);
            break;
        case 0x8E:
            /* DRIVE_SPECIFICATION_COMMAND */
            FLOPPY_DPRINTF("treat DRIVE_SPECIFICATION_COMMAND command\n");
            if (fdctrl->fifo[fdctrl->data_pos - 1] & 0x80) {
                /* Command parameters done */
                if (fdctrl->fifo[fdctrl->data_pos - 1] & 0x40) {
                    fdctrl->fifo[0] = fdctrl->fifo[1];
                    fdctrl->fifo[2] = 0;
                    fdctrl->fifo[3] = 0;
                    fdctrl_set_fifo(fdctrl, 4, 1);
                } else {
                    fdctrl_reset_fifo(fdctrl);
                }
            } else if (fdctrl->data_len > 7) {
                /* ERROR */
                fdctrl->fifo[0] = 0x80 |
                    (cur_drv->head << 2) | fdctrl->cur_drv;
                fdctrl_set_fifo(fdctrl, 1, 1);
            }
            break;
        case 0x8F:
            /* RELATIVE_SEEK_OUT */
            FLOPPY_DPRINTF("treat RELATIVE_SEEK_OUT command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            fd_start(cur_drv);
                cur_drv->dir = 0;
            if (fdctrl->fifo[2] + cur_drv->track >= cur_drv->max_track) {
                cur_drv->track = cur_drv->max_track - 1;
            } else {
                cur_drv->track += fdctrl->fifo[2];
            }
            fdctrl_reset_fifo(fdctrl);
            fdctrl_raise_irq(fdctrl, 0x20);
            break;
        case 0xCD:
            /* FORMAT_AND_WRITE */
/* //                FLOPPY_DPRINTF("treat FORMAT_AND_WRITE command\n"); */
            FLOPPY_ERROR("treat FORMAT_AND_WRITE command\n");
            fdctrl_unimplemented(fdctrl);
            break;
        case 0xCF:
                /* RELATIVE_SEEK_IN */
            FLOPPY_DPRINTF("treat RELATIVE_SEEK_IN command\n");
            fdctrl->cur_drv = fdctrl->fifo[1] & 1;
            cur_drv = get_cur_drv(fdctrl);
            fd_start(cur_drv);
                cur_drv->dir = 1;
            if (fdctrl->fifo[2] > cur_drv->track) {
                cur_drv->track = 0;
            } else {
                cur_drv->track -= fdctrl->fifo[2];
            }
            fdctrl_reset_fifo(fdctrl);
            /* Raise Interrupt */
            fdctrl_raise_irq(fdctrl, 0x20);
            break;
        }
    }
}

static void fdctrl_result_timer(void *opaque)
{
    fdctrl_t *fdctrl = opaque;
    fdctrl_stop_transfer(fdctrl, 0x00, 0x00, 0x00);
}


#ifdef VBOX
static DECLCALLBACK(void) fdc_timer (PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    fdctrl_t *fdctrl = PDMINS2DATA (pDevIns, fdctrl_t *);
    fdctrl_result_timer (fdctrl);
}

static DECLCALLBACK(int) fdc_io_write (PPDMDEVINS pDevIns,
                                       void *pvUser,
                                       RTIOPORT Port,
                                       uint32_t u32,
                                       unsigned cb)
{
    if (cb == 1) {
        fdctrl_write (pvUser, Port, u32);
    }
    else {
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) fdc_io_read (PPDMDEVINS pDevIns,
                                      void *pvUser,
                                      RTIOPORT Port,
                                      uint32_t *pu32,
                                      unsigned cb)
{
    if (cb == 1) {
        *pu32 = fdctrl_read (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}

static DECLCALLBACK(int) SaveExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    fdctrl_t *s = PDMINS2DATA (pDevIns, fdctrl_t *);
    QEMUFile *f = pSSMHandle;
    unsigned int i;

    qemu_put_8s (f, &s->version);
    qemu_put_8s (f, &s->irq_lvl);
    qemu_put_8s (f, &s->dma_chann);
    qemu_put_be32s (f, &s->io_base);
    qemu_put_8s (f, &s->state);
    qemu_put_8s (f, &s->dma_en);
    qemu_put_8s (f, &s->cur_drv);
    qemu_put_8s (f, &s->bootsel);
    qemu_put_buffer (f, s->fifo, FD_SECTOR_LEN);
    qemu_put_be32s (f, &s->data_pos);
    qemu_put_be32s (f, &s->data_len);
    qemu_put_8s (f, &s->data_state);
    qemu_put_8s (f, &s->data_dir);
    qemu_put_8s (f, &s->int_status);
    qemu_put_8s (f, &s->eot);
    qemu_put_8s (f, &s->timer0);
    qemu_put_8s (f, &s->timer1);
    qemu_put_8s (f, &s->precomp_trk);
    qemu_put_8s (f, &s->config);
    qemu_put_8s (f, &s->lock);
    qemu_put_8s (f, &s->pwrd);

    for (i = 0; i < sizeof (s->drives) / sizeof (s->drives[0]); ++i) {
        fdrive_t *d = &s->drives[i];

        SSMR3PutMem (pSSMHandle, &d->Led, sizeof (d->Led));
        qemu_put_be32s (f, &d->drive);
        qemu_put_be32s (f, &d->drflags);
        qemu_put_8s (f, &d->perpendicular);
        qemu_put_8s (f, &d->head);
        qemu_put_8s (f, &d->track);
        qemu_put_8s (f, &d->sect);
        qemu_put_8s (f, &d->dir);
        qemu_put_8s (f, &d->rw);
        qemu_put_be32s (f, &d->flags);
        qemu_put_8s (f, &d->last_sect);
        qemu_put_8s (f, &d->max_track);
        qemu_put_be16s (f, &d->bps);
        qemu_put_8s (f, &d->ro);
    }
    return TMR3TimerSave (s->result_timer, pSSMHandle);
}

static DECLCALLBACK(int) LoadExec (PPDMDEVINS pDevIns,
                                   PSSMHANDLE pSSMHandle,
                                   uint32_t u32Version)
{
    fdctrl_t *s = PDMINS2DATA (pDevIns, fdctrl_t *);
    QEMUFile *f = pSSMHandle;
    unsigned int i;

    if (u32Version != 1) {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    qemu_get_8s (f, &s->version);
    qemu_get_8s (f, &s->irq_lvl);
    qemu_get_8s (f, &s->dma_chann);
    qemu_get_be32s (f, &s->io_base);
    qemu_get_8s (f, &s->state);
    qemu_get_8s (f, &s->dma_en);
    qemu_get_8s (f, &s->cur_drv);
    qemu_get_8s (f, &s->bootsel);
    qemu_get_buffer (f, s->fifo, FD_SECTOR_LEN);
    qemu_get_be32s (f, &s->data_pos);
    qemu_get_be32s (f, &s->data_len);
    qemu_get_8s (f, &s->data_state);
    qemu_get_8s (f, &s->data_dir);
    qemu_get_8s (f, &s->int_status);
    qemu_get_8s (f, &s->eot);
    qemu_get_8s (f, &s->timer0);
    qemu_get_8s (f, &s->timer1);
    qemu_get_8s (f, &s->precomp_trk);
    qemu_get_8s (f, &s->config);
    qemu_get_8s (f, &s->lock);
    qemu_get_8s (f, &s->pwrd);

    for (i = 0; i < sizeof (s->drives) / sizeof (s->drives[0]); ++i) {
        fdrive_t *d = &s->drives[i];

        SSMR3GetMem (pSSMHandle, &d->Led, sizeof (d->Led));
        qemu_get_be32s (f, &d->drive);
        qemu_get_be32s (f, &d->drflags);
        qemu_get_8s (f, &d->perpendicular);
        qemu_get_8s (f, &d->head);
        qemu_get_8s (f, &d->track);
        qemu_get_8s (f, &d->sect);
        qemu_get_8s (f, &d->dir);
        qemu_get_8s (f, &d->rw);
        qemu_get_be32s (f, &d->flags);
        qemu_get_8s (f, &d->last_sect);
        qemu_get_8s (f, &d->max_track);
        qemu_get_be16s (f, &d->bps);
        qemu_get_8s (f, &d->ro);
    }
    return TMR3TimerLoad (s->result_timer, pSSMHandle);
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to IDEState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) fdQueryInterface (PPDMIBASE pInterface,
                                              PDMINTERFACE enmInterface)
{
    fdrive_t *drv = PDMIBASE_2_FDRIVE(pInterface);
    switch (enmInterface) {
    case PDMINTERFACE_BASE:
        return &drv->IBase;
    case PDMINTERFACE_BLOCK_PORT:
        return &drv->IPort;
    case PDMINTERFACE_MOUNT_NOTIFY:
        return &drv->IMountNotify;
    default:
        return NULL;
    }
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) fdcStatusQueryStatusLed (PPDMILEDPORTS pInterface,
                                                  unsigned iLUN,
                                                  PPDMLED *ppLed)
{
    fdctrl_t *fdctrl = (fdctrl_t *)
        ((uintptr_t )pInterface - RT_OFFSETOF (fdctrl_t, ILeds));
    if (iLUN < ELEMENTS(fdctrl->drives)) {
        *ppLed = &fdctrl->drives[iLUN].Led;
        Assert ((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/**
 * Queries an interface to the status LUN.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to IDEState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) fdcStatusQueryInterface (PPDMIBASE pInterface,
                                                     PDMINTERFACE enmInterface)
{
    fdctrl_t *fdctrl = (fdctrl_t *)
        ((uintptr_t)pInterface - RT_OFFSETOF (fdctrl_t, IBaseStatus));
    switch (enmInterface) {
    case PDMINTERFACE_BASE:
        return &fdctrl->IBaseStatus;
    case PDMINTERFACE_LED_PORTS:
        return &fdctrl->ILeds;
    default:
        return NULL;
    }
}


/**
 * Configure a drive.
 *
 * @returns VBox status code.
 * @param   drv         The drive in question.
 * @param   pDevIns     The driver instance.
 */
static int fdConfig (fdrive_t *drv, PPDMDEVINS pDevIns)
{
    static const char *descs[] = {"Floppy Drive A:", "Floppy Drive B"};
    int rc;

    /*
     * Reset the LED just to be on the safe side.
     */
    Assert (ELEMENTS(descs) > drv->iLUN);
    Assert (drv->Led.u32Magic == PDMLED_MAGIC);
    drv->Led.Actual.u32 = 0;
    drv->Led.Asserted.u32 = 0;

    /*
     * Try attach the block device and get the interfaces.
     */
    rc = PDMDevHlpDriverAttach (pDevIns, drv->iLUN, &drv->IBase, &drv->pDrvBase, descs[drv->iLUN]);
    if (VBOX_SUCCESS (rc))
    {
        drv->pDrvBlock = drv->pDrvBase->pfnQueryInterface (
            drv->pDrvBase,
            PDMINTERFACE_BLOCK
            );
        if (drv->pDrvBlock) {
            drv->pDrvBlockBios = drv->pDrvBase->pfnQueryInterface (
                drv->pDrvBase,
                PDMINTERFACE_BLOCK_BIOS
                );
            if (drv->pDrvBlockBios) {
                drv->pDrvMount = drv->pDrvBase->pfnQueryInterface (
                    drv->pDrvBase,
                    PDMINTERFACE_MOUNT
                    );
                if (drv->pDrvMount) {
                    /*
                     * Init the state.
                     */
                    drv->drive = FDRIVE_DRV_NONE;
                    drv->drflags = 0;
                    drv->perpendicular = 0;
                    /* Disk */
                    drv->last_sect = 0;
                    drv->max_track = 0;
                    drv->fMediaPresent = false;
                }
                else {
                    AssertMsgFailed (("Configuration error: LUN#%d without mountable interface!\n", drv->iLUN));
                    rc = VERR_PDM_MISSING_INTERFACE;
                }

            }
            else {
                AssertMsgFailed (("Configuration error: LUN#%d hasn't a block BIOS interface!\n", drv->iLUN));
                rc = VERR_PDM_MISSING_INTERFACE;
            }

        }
        else {
            AssertMsgFailed (("Configuration error: LUN#%d hasn't a block interface!\n", drv->iLUN));
            rc = VERR_PDM_MISSING_INTERFACE;
        }
    }
    else {
        AssertMsg (rc == VERR_PDM_NO_ATTACHED_DRIVER,
                   ("Failed to attach LUN#%d. rc=%Vrc\n", drv->iLUN, rc));
        switch (rc)
        {
            case VERR_ACCESS_DENIED:
                /* Error already catched by DrvHostBase */
                return rc;
            default:
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("The floppy controller cannot attach to the floppy drive"));
        }
    }

    if (VBOX_FAILURE (rc)) {
        drv->pDrvBase = NULL;
        drv->pDrvBlock = NULL;
        drv->pDrvBlockBios = NULL;
        drv->pDrvMount = NULL;
    }
    LogFlow (("fdConfig: returns %Vrc\n", rc));
    return rc;
}


/**
 * Attach command.
 *
 * This is called when we change block driver for a floppy drive.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(int)  fdcAttach (PPDMDEVINS pDevIns,
                                     unsigned iLUN)
{
    fdctrl_t *fdctrl = PDMINS2DATA (pDevIns, fdctrl_t *);
    fdrive_t *drv;
    int rc;
    LogFlow (("ideDetach: iLUN=%u\n", iLUN));

    /*
     * Validate.
     */
    if (iLUN > 2) {
        AssertMsgFailed (("Configuration error: cannot attach or detach any but the first two LUNs - iLUN=%u\n",
                          iLUN));
        return VERR_PDM_DEVINS_NO_ATTACH;
    }

    /*
     * Locate the drive and stuff.
     */
    drv = &fdctrl->drives[iLUN];

    /* the usual paranoia */
    AssertRelease (!drv->pDrvBase);
    AssertRelease (!drv->pDrvBlock);
    AssertRelease (!drv->pDrvBlockBios);
    AssertRelease (!drv->pDrvMount);

    rc = fdConfig (drv, pDevIns);
    AssertMsg (rc != VERR_PDM_NO_ATTACHED_DRIVER,
               ("Configuration error: failed to configure drive %d, rc=%Vrc\n", rc));
    if (VBOX_SUCCESS(rc)) {
        fd_revalidate (drv);
    }

    LogFlow (("floppyAttach: returns %Vrc\n", rc));
    return rc;
}


/**
 * Detach notification.
 *
 * The floppy drive has been temporarily 'unplugged'.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(void) fdcDetach (PPDMDEVINS pDevIns,
                                     unsigned iLUN)
{
    fdctrl_t *fdctrl = PDMINS2DATA (pDevIns, fdctrl_t *);
    LogFlow (("ideDetach: iLUN=%u\n", iLUN));

    switch (iLUN) {
    case 0:
    case 1: {
        fdrive_t *drv = &fdctrl->drives[iLUN];
        drv->pDrvBase = NULL;
        drv->pDrvBlock = NULL;
        drv->pDrvBlockBios = NULL;
        drv->pDrvMount = NULL;
        break;
    }

    default:
        AssertMsgFailed (("Cannot detach LUN#%d!\n", iLUN));
        break;
    }
}


/**
 * Handle reset.
 *
 * I haven't check the specs on what's supposed to happen on reset, but we
 * should get any 'FATAL: floppy recal:f07 ctrl not ready' when resetting
 * at wrong time like we do if this was all void.
 *
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(void) fdcReset (PPDMDEVINS pDevIns)
{
    fdctrl_t *fdctrl = PDMINS2DATA (pDevIns, fdctrl_t *);
    int i;
    LogFlow (("fdcReset:\n"));

    fdctrl_reset(fdctrl, 0);
    fdctrl->state = FD_CTRL_ACTIVE;

    for (i = 0; i < ELEMENTS(fdctrl->drives); i++) {
        fd_revalidate(&fdctrl->drives[i]);
    }
}


/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) fdcConstruct (PPDMDEVINS pDevIns,
                                       int iInstance,
                                       PCFGMNODE pCfgHandle)
{
    int            rc;
    fdctrl_t       *fdctrl = PDMINS2DATA(pDevIns, fdctrl_t*);
    int            i;
    bool           mem_mapped;
    uint16_t       io_base;
    uint8_t        irq_lvl, dma_chann;
    PPDMIBASE      pBase;

    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0DMA\0MemMapped\0IOBase\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Read the configuration.
     */
    rc = CFGMR3QueryU8 (pCfgHandle, "IRQ", &irq_lvl);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        irq_lvl = 6;
    else if (VBOX_FAILURE (rc))
    {
        AssertMsgFailed (("Configuration error: Failed to read U8 IRQ, rc=%Vrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU8 (pCfgHandle, "DMA", &dma_chann);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        dma_chann = 2;
    else if (VBOX_FAILURE (rc))
    {
        AssertMsgFailed (("Configuration error: Failed to read U8 DMA, rc=%Vrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU16 (pCfgHandle, "IOBase", &io_base);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        io_base = 0x3f0;
    else if (VBOX_FAILURE (rc))
    {
        AssertMsgFailed (("Configuration error: Failed to read U16 IOBase, rc=%Vrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryBool (pCfgHandle, "MemMapped", &mem_mapped);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        mem_mapped = false;
    else if (VBOX_FAILURE (rc))
    {
        AssertMsgFailed (("Configuration error: Failed to read bool value MemMapped rc=%Vrc\n", rc));
        return rc;
    }

    /*
     * Initialize data.
     */
    LogFlow(("fdcConstruct: irq_lvl=%d dma_chann=%d io_base=%#x\n", irq_lvl, dma_chann, io_base));
    fdctrl->pDevIns   = pDevIns;
    fdctrl->version   = 0x90;   /* Intel 82078 controller */
    fdctrl->irq_lvl   = irq_lvl;
    fdctrl->dma_chann = dma_chann;
    fdctrl->io_base   = io_base;
    fdctrl->config    = 0x60;   /* Implicit seek, polling & FIFO enabled */

    fdctrl->IBaseStatus.pfnQueryInterface = fdcStatusQueryInterface;
    fdctrl->ILeds.pfnQueryStatusLed = fdcStatusQueryStatusLed;

    for (i = 0; i < ELEMENTS(fdctrl->drives); ++i)
    {
        fdrive_t *drv = &fdctrl->drives[i];

        drv->drive = FDRIVE_DRV_NONE;
        drv->iLUN = i;

        drv->IBase.pfnQueryInterface = fdQueryInterface;
        drv->IMountNotify.pfnMountNotify = fdMountNotify;
        drv->IMountNotify.pfnUnmountNotify = fdUnmountNotify;
        drv->Led.u32Magic = PDMLED_MAGIC;
    }

    /*
     * Create the FDC timer.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, fdc_timer, "FDC Timer", &fdctrl->result_timer);
    if (VBOX_FAILURE (rc))
        return rc;

    /*
     * Register DMA channel.
     */
    if (fdctrl->dma_chann != 0xff)
    {
        fdctrl->dma_en = 1;
        rc = PDMDevHlpDMARegister (pDevIns, dma_chann, &fdctrl_transfer_handler, fdctrl);
        if (VBOX_FAILURE (rc))
            return rc;
    }
    else
        fdctrl->dma_en = 0;

    /*
     * IO / MMIO.
     */
    if (mem_mapped)
    {
        AssertMsgFailed (("Memory mapped floppy not support by now\n"));
        return VERR_NOT_SUPPORTED;
#if 0
        FLOPPY_ERROR("memory mapped floppy not supported by now !\n");
        io_mem = cpu_register_io_memory(0, fdctrl_mem_read, fdctrl_mem_write);
        cpu_register_physical_memory(base, 0x08, io_mem);
#endif
    }
    else
    {
        rc = PDMDevHlpIOPortRegister (pDevIns, io_base + 0x1, 5, fdctrl,
                                      fdc_io_write, fdc_io_read, NULL, NULL, "FDC#1");
        if (VBOX_FAILURE (rc))
            return rc;

        rc = PDMDevHlpIOPortRegister (pDevIns, io_base + 0x7, 1, fdctrl,
                                      fdc_io_write, fdc_io_read, NULL, NULL, "FDC#2");
        if (VBOX_FAILURE (rc))
            return rc;
    }

    /*
     * Register the saved state data unit.
     */
    rc = PDMDevHlpSSMRegister (pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1, sizeof(*fdctrl),
                               NULL, SaveExec, NULL, NULL, LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Attach the status port (optional).
     */
    rc = PDMDevHlpDriverAttach (pDevIns, PDM_STATUS_LUN, &fdctrl->IBaseStatus, &pBase, "Status Port");
    if (VBOX_SUCCESS (rc)) {
        fdctrl->pLedsConnector =
            pBase->pfnQueryInterface (pBase, PDMINTERFACE_LED_CONNECTORS);
    }
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER) {
        AssertMsgFailed (("Failed to attach to status driver. rc=%Vrc\n",
                          rc));
        return rc;
    }

    /*
     * Initialize drives.
     */
    for (i = 0; i < ELEMENTS(fdctrl->drives); i++)
    {
        fdrive_t *drv = &fdctrl->drives[i];
        rc = fdConfig (drv, pDevIns);
        if (    VBOX_FAILURE (rc)
            &&  rc != VERR_PDM_NO_ATTACHED_DRIVER)
        {
            AssertMsgFailed (("Configuration error: failed to configure drive %d, rc=%Vrc\n", rc));
            return rc;
        }
    }

    fdctrl_reset(fdctrl, 0);
    fdctrl->state = FD_CTRL_ACTIVE;

    for (i = 0; i < ELEMENTS(fdctrl->drives); i++)
        fd_revalidate(&fdctrl->drives[i]);

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceFloppyController =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "i82078",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Floppy drive controller (Intel 82078)",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(fdctrl_t),
    /* pfnConstruct */
    fdcConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    fdcReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    fdcAttach,
    /* pfnDetach */
    fdcDetach,
    /* pfnQueryInterface. */
    NULL
};

#endif /* VBOX */

/*
 * Local Variables:
 *  mode: c
 *  c-file-style: "k&r"
 *  indent-tabs-mode: nil
 * End:
 */

