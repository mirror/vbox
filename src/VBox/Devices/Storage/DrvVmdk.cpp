/** @file
 *
 * VBox storage devices:
 * VBox VMDK container implementation
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
 * Block driver for the VMDK format
 *
 * Copyright (c) 2004 Fabrice Bellard
 * Copyright (c) 2005 Filip Navara
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VBOXHDD
#include <VBox/pdmdrv.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include "Builtins.h"

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/** The Sector size.
 * Currently we support only 512 bytes sectors.
 */
#define VMDK_GEOMETRY_SECTOR_SIZE    (512)
/**  512 = 2^^9 */
#define VMDK_GEOMETRY_SECTOR_SHIFT   (9)

#define VMDK3_MAGIC (('C' << 24) | ('O' << 16) | ('W' << 8) | 'D')
#define VMDK4_MAGIC (('K' << 24) | ('D' << 16) | ('M' << 8) | 'V')

#pragma pack(1)
typedef struct {
    uint32_t version;
    uint32_t flags;
    uint32_t disk_sectors;
    uint32_t granularity;
    uint32_t l1dir_offset;
    uint32_t l1dir_size;
    uint32_t file_sectors;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors_per_track;
} VMDK3Header;
#pragma pack()

#pragma pack(1)
typedef struct {
    uint32_t version;
    uint32_t flags;
    int64_t capacity;
    int64_t granularity;
    int64_t desc_offset;
    int64_t desc_size;
    int32_t num_gtes_per_gte;
    int64_t rgd_offset;
    int64_t gd_offset;
    int64_t grain_offset;
    char filler[1];
    char check_bytes[4];
} VMDK4Header;
#pragma pack()

#define L2_CACHE_SIZE 16

typedef struct BDRVVmdkState {
    /** File handle. */
    RTFILE                  File;

    bool                    fReadOnly;

    uint64_t                total_sectors;

    int64_t l1_table_offset;
    int64_t l1_backup_table_offset;
    uint32_t *l1_table;
    uint32_t *l1_backup_table;
    unsigned int l1_size;
    uint32_t l1_entry_sectors;

    unsigned int l2_size;
    uint32_t *l2_cache;
    uint32_t l2_cache_offsets[L2_CACHE_SIZE];
    uint32_t l2_cache_counts[L2_CACHE_SIZE];

    unsigned int cluster_sectors;
} BDRVVmdkState;


#define VMDKDISK_SIGNATURE          0x8013ABCD


/**
 * Harddisk geometry.
 */
#pragma pack(1)
typedef struct VMDKDISKGEOMETRY
{
    /** Cylinders. */
    uint32_t    cCylinders;
    /** Heads. */
    uint32_t    cHeads;
    /** Sectors per track. */
    uint32_t    cSectors;
    /** Sector size. (bytes per sector) */
    uint32_t    cbSector;
} VMDKDISKGEOMETRY, *PVMDKDISKGEOMETRY;
#pragma pack()

/**
 * VMDK HDD Container main structure, private part.
 */
typedef struct VMDKDISK
{
    uint32_t        u32Signature;

    BDRVVmdkState   VmdkState;

    /** Hard disk geometry. */
    VMDKDISKGEOMETRY Geometry;

    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
} VMDKDISK, *PVMDKDISK;


/** Converts a pointer to VDIDISK::IMedia to a PVMDKDISK. */
#define PDMIMEDIA_2_VMDKDISK(pInterface) ( (PVMDKDISK)((uintptr_t)pInterface - RT_OFFSETOF(VMDKDISK, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVMDKDISK. */
#define PDMIBASE_2_VMDKDISK(pInterface)  ( PDMINS2DATA(PDMIBASE_2_DRVINS(pInterface), PVMDKDISK) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  drvVmdkConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
static DECLCALLBACK(void) drvVmdkDestruct(PPDMDRVINS pDrvIns);
static DECLCALLBACK(int)  drvVmdkRead(PPDMIMEDIA pInterface,
                                  uint64_t off, void *pvBuf, size_t cbRead);
static DECLCALLBACK(int)  drvVmdkWrite(PPDMIMEDIA pInterface,
                                   uint64_t off, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(int)  drvVmdkFlush(PPDMIMEDIA pInterface);
static DECLCALLBACK(uint64_t) drvVmdkGetSize(PPDMIMEDIA pInterface);
static DECLCALLBACK(int)  drvVmdkBiosGetGeometry(PPDMIMEDIA pInterface, uint32_t *pcCylinders,
                                             uint32_t *pcHeads, uint32_t *pcSectors);
static DECLCALLBACK(int)  drvVmdkBiosSetGeometry(PPDMIMEDIA pInterface, uint32_t cCylinders,
                                             uint32_t cHeads, uint32_t cSectors);
static DECLCALLBACK(int)  drvVmdkGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid);
static DECLCALLBACK(bool) drvVmdkIsReadOnly(PPDMIMEDIA pInterface);
static DECLCALLBACK(int)  drvVmdkBiosGetTranslation(PPDMIMEDIA pInterface,
                                                PPDMBIOSTRANSLATION penmTranslation);
static DECLCALLBACK(int)  drvVmdkBiosSetTranslation(PPDMIMEDIA pInterface,
                                                PDMBIOSTRANSLATION enmTranslation);
static DECLCALLBACK(void *) drvVmdkQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);

#if 0
static int vmdk_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    uint32_t magic;

    if (buf_size < 4)
        return 0;
    magic = RT_BE2H_U32(*(uint32_t *)buf);
    if (magic == VMDK3_MAGIC ||
        magic == VMDK4_MAGIC)
        return 100;
    else
        return 0;
}
#endif

static int vmdk_open(PVMDKDISK pDisk, const char *filename, bool fReadOnly)
{
    uint32_t magic, i;
    int l1_size;

    BDRVVmdkState *s = &pDisk->VmdkState;

    /*
     * Open the image.
     */
    s->fReadOnly = fReadOnly;
    int rc = RTFileOpen(&s->File,
                        filename,
                        fReadOnly
                        ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                        : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (VBOX_FAILURE(rc))
    {
        if (!fReadOnly)
        {
            /* Try to open image for reading only. */
            rc = RTFileOpen(&s->File,
                            filename,
                            RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
            if (VBOX_SUCCESS(rc))
                s->fReadOnly = true;
        }
        if (VBOX_FAILURE(rc))
            return rc;
    }
    rc = RTFileRead(s->File, &magic, sizeof(magic), NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        goto fail;

    magic = RT_BE2H_U32(magic);
    if (magic == VMDK3_MAGIC)
    {
        VMDK3Header header;
        rc = RTFileRead(s->File, &header, sizeof(header), NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            goto fail;
        s->cluster_sectors = RT_LE2H_U32(header.granularity);
        s->l2_size = 1 << 9;
        s->l1_size = 1 << 6;
        s->total_sectors = RT_LE2H_U32(header.disk_sectors);
        s->l1_table_offset = RT_LE2H_U32(header.l1dir_offset) << 9;
        s->l1_backup_table_offset = 0;
        s->l1_entry_sectors = s->l2_size * s->cluster_sectors;

        /* fill in the geometry structure */
        pDisk->Geometry.cCylinders = RT_LE2H_U32(header.cylinders);
        pDisk->Geometry.cHeads = RT_LE2H_U32(header.heads);
        pDisk->Geometry.cSectors = RT_LE2H_U32(header.sectors_per_track);
        pDisk->Geometry.cbSector = VMDK_GEOMETRY_SECTOR_SIZE;
    }
    else if (magic == VMDK4_MAGIC)
    {
        VMDK4Header header;

        rc = RTFileRead(s->File, &header, sizeof(header), NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            goto fail;
        s->total_sectors = RT_LE2H_U64(header.capacity);
        s->cluster_sectors = RT_LE2H_U64(header.granularity);
        s->l2_size = RT_LE2H_U32(header.num_gtes_per_gte);
        s->l1_entry_sectors = s->l2_size * s->cluster_sectors;
        if (s->l1_entry_sectors <= 0)
        {
            rc = VERR_VDI_INVALID_HEADER;
            goto fail;
        }
        s->l1_size = (s->total_sectors + s->l1_entry_sectors - 1)
            / s->l1_entry_sectors;
        s->l1_table_offset = RT_LE2H_U64(header.rgd_offset) << 9;
        s->l1_backup_table_offset = RT_LE2H_U64(header.gd_offset) << 9;

        /* fill in the geometry structure */
        /// @todo we should read these properties from the DDB section
        //  of the Disk DescriptorFile. So, the below values are just a
        //  quick hack.
        pDisk->Geometry.cCylinders = RT_MIN((RT_LE2H_U64(header.capacity) /
                                      (16 * 63)), 16383);
        pDisk->Geometry.cHeads = 16;
        pDisk->Geometry.cSectors = 63;
        pDisk->Geometry.cbSector = VMDK_GEOMETRY_SECTOR_SIZE;
    }
    else
    {
        rc = VERR_VDI_INVALID_HEADER;
        goto fail;
    }
    /* read the L1 table */
    l1_size = s->l1_size * sizeof(uint32_t);
    s->l1_table = (uint32_t *)RTMemAllocZ(l1_size);
    if (!s->l1_table)
    {
        rc = VERR_NO_MEMORY;
        goto fail;
    }
    rc = RTFileSeek(s->File, s->l1_table_offset, RTFILE_SEEK_BEGIN, NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        goto fail;
    rc = RTFileRead(s->File, s->l1_table, l1_size, NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        goto fail;
    for(i = 0; i < s->l1_size; i++) {
        s->l1_table[i] = RT_LE2H_U32(s->l1_table[i]);
    }

    if (s->l1_backup_table_offset) {
        s->l1_backup_table = (uint32_t *)RTMemAllocZ(l1_size);
        if (!s->l1_backup_table)
        {
            rc = VERR_NO_MEMORY;
            goto fail;
        }
        rc = RTFileSeek(s->File, s->l1_backup_table_offset, RTFILE_SEEK_BEGIN, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            goto fail;
        rc = RTFileRead(s->File, s->l1_backup_table, l1_size, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            goto fail;
        for(i = 0; i < s->l1_size; i++) {
            s->l1_backup_table[i] = RT_LE2H_U32(s->l1_backup_table[i]);
        }
    }

    s->l2_cache = (uint32_t *)RTMemAllocZ(s->l2_size * L2_CACHE_SIZE * sizeof(uint32_t));
    if (!s->l2_cache)
    {
        rc = VERR_NO_MEMORY;
        goto fail;
    }

    return VINF_SUCCESS;

 fail:
    Log(("vmdk_open failed with %Vrc\n", rc));
    if (s->l1_backup_table)
        RTMemFree(s->l1_backup_table);
    if (s->l1_table)
        RTMemFree(s->l1_table);
    if (s->l2_cache)
        RTMemFree(s->l2_cache);
    RTFileClose(s->File);
    return rc;
}

static uint64_t get_cluster_offset(BDRVVmdkState *s,
                                   uint64_t offset, int allocate)
{
    unsigned int l1_index, l2_offset, l2_index;
    int min_index, i, j;
    uint32_t min_count, *l2_table, tmp;
    uint64_t cluster_offset;
    int rc;

    l1_index = (offset >> 9) / s->l1_entry_sectors;
    if (l1_index >= s->l1_size)
        return 0;
    l2_offset = s->l1_table[l1_index];
    if (!l2_offset)
        return 0;
    for(i = 0; i < L2_CACHE_SIZE; i++) {
        if (l2_offset == s->l2_cache_offsets[i]) {
            /* increment the hit count */
            if (++s->l2_cache_counts[i] == 0xffffffff) {
                for(j = 0; j < L2_CACHE_SIZE; j++) {
                    s->l2_cache_counts[j] >>= 1;
                }
            }
            l2_table = s->l2_cache + (i * s->l2_size);
            goto found;
        }
    }
    /* not found: load a new entry in the least used one */
    min_index = 0;
    min_count = 0xffffffff;
    for(i = 0; i < L2_CACHE_SIZE; i++) {
        if (s->l2_cache_counts[i] < min_count) {
            min_count = s->l2_cache_counts[i];
            min_index = i;
        }
    }
    l2_table = s->l2_cache + (min_index * s->l2_size);
    rc = RTFileSeek(s->File, (int64_t)l2_offset * VMDK_GEOMETRY_SECTOR_SIZE, RTFILE_SEEK_BEGIN, NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        return 0;
    rc = RTFileRead(s->File, l2_table, s->l2_size * sizeof(uint32_t), NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        return 0;
    s->l2_cache_offsets[min_index] = l2_offset;
    s->l2_cache_counts[min_index] = 1;
 found:
    l2_index = ((offset >> 9) / s->cluster_sectors) % s->l2_size;
    cluster_offset = RT_LE2H_U32(l2_table[l2_index]);
    if (!cluster_offset) {
        if (!allocate)
            return 0;
        rc = RTFileSeek(s->File, 0, RTFILE_SEEK_END, &cluster_offset);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return 0;
        rc = RTFileSetSize(s->File, cluster_offset + (s->cluster_sectors << 9));
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return 0;
        cluster_offset >>= 9;
        /* update L2 table */
        tmp = RT_H2LE_U32(cluster_offset);
        l2_table[l2_index] = tmp;
        rc = RTFileSeek(s->File, ((int64_t)l2_offset * VMDK_GEOMETRY_SECTOR_SIZE) + (l2_index * sizeof(tmp)), RTFILE_SEEK_BEGIN, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return 0;
        rc = RTFileWrite(s->File, &tmp, sizeof(tmp), NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return 0;
        /* update backup L2 table */
        if (s->l1_backup_table_offset != 0) {
            l2_offset = s->l1_backup_table[l1_index];

            rc = RTFileSeek(s->File, ((int64_t)l2_offset * VMDK_GEOMETRY_SECTOR_SIZE) + (l2_index * sizeof(tmp)), RTFILE_SEEK_BEGIN, NULL);
            AssertRC(rc);
            if (VBOX_FAILURE(rc))
                return 0;
            rc = RTFileWrite(s->File, &tmp, sizeof(tmp), NULL);
            AssertRC(rc);
            if (VBOX_FAILURE(rc))
                return 0;
        }
    }
    cluster_offset <<= 9;
    return cluster_offset;
}

#if 0
static int vmdk_is_allocated(BDRVVmdkState *s, int64_t sector_num,
                             int nb_sectors, int *pnum)
{
    int index_in_cluster, n;
    uint64_t cluster_offset;

    cluster_offset = get_cluster_offset(s, sector_num << 9, 0);
    index_in_cluster = sector_num % s->cluster_sectors;
    n = s->cluster_sectors - index_in_cluster;
    if (n > nb_sectors)
        n = nb_sectors;
    *pnum = n;
    return (cluster_offset != 0);
}
#endif

static int vmdk_read(BDRVVmdkState *s, int64_t sector_num,
                    uint8_t *buf, int nb_sectors)
{
    int index_in_cluster, n;
    uint64_t cluster_offset;

    while (nb_sectors > 0) {
        cluster_offset = get_cluster_offset(s, sector_num << 9, 0);
        index_in_cluster = sector_num % s->cluster_sectors;
        n = s->cluster_sectors - index_in_cluster;
        if (n > nb_sectors)
            n = nb_sectors;
        if (!cluster_offset) {
            memset(buf, 0, VMDK_GEOMETRY_SECTOR_SIZE * n);
        } else {
            int rc = RTFileSeek(s->File, cluster_offset + index_in_cluster * VMDK_GEOMETRY_SECTOR_SIZE, RTFILE_SEEK_BEGIN, NULL);
            AssertRC(rc);
            if (VBOX_FAILURE(rc))
                return rc;

            rc = RTFileRead(s->File, buf, n * VMDK_GEOMETRY_SECTOR_SIZE, NULL);
            AssertRC(rc);
            if (VBOX_FAILURE(rc))
                return rc;
        }
        nb_sectors -= n;
        sector_num += n;
        buf += n * VMDK_GEOMETRY_SECTOR_SIZE;
    }
    return VINF_SUCCESS;
}

static int vmdk_write(BDRVVmdkState *s, int64_t sector_num,
                     const uint8_t *buf, int nb_sectors)
{
    int index_in_cluster, n;
    uint64_t cluster_offset;

    while (nb_sectors > 0) {
        index_in_cluster = sector_num & (s->cluster_sectors - 1);
        n = s->cluster_sectors - index_in_cluster;
        if (n > nb_sectors)
            n = nb_sectors;
        cluster_offset = get_cluster_offset(s, sector_num << 9, 1);
        if (!cluster_offset)
            return VERR_IO_SECTOR_NOT_FOUND;

        int rc = RTFileSeek(s->File, cluster_offset + index_in_cluster * VMDK_GEOMETRY_SECTOR_SIZE, RTFILE_SEEK_BEGIN, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return rc;

        rc = RTFileWrite(s->File, buf, n * VMDK_GEOMETRY_SECTOR_SIZE, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
            return rc;

        nb_sectors -= n;
        sector_num += n;
        buf += n * VMDK_GEOMETRY_SECTOR_SIZE;
    }
    return VINF_SUCCESS;
}

static void vmdk_close(BDRVVmdkState *s)
{
    RTMemFree(s->l1_table);
    RTMemFree(s->l2_cache);
    RTFileClose(s->File);
}

static void vmdk_flush(BDRVVmdkState *s)
{
    RTFileFlush(s->File);
}


/**
 * Get read/write mode of VMDK HDD.
 *
 * @returns Disk ReadOnly status.
 * @returns true if no one VMDK image is opened in HDD container.
 */
IDER3DECL(bool) VMDKDiskIsReadOnly(PVMDKDISK pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    LogFlow(("VmdkDiskIsReadOnly: returns %u\n", pDisk->VmdkState.fReadOnly));
    return pDisk->VmdkState.fReadOnly;
}


/**
 * Get disk size of VMDK HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no one VMDK image is opened in HDD container.
 */
IDER3DECL(uint64_t) VMDKDiskGetSize(PVMDKDISK pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    return pDisk->VmdkState.total_sectors * VMDK_GEOMETRY_SECTOR_SIZE;
}

/**
 * Get block size of VMDK HDD container.
 *
 * @returns VDI image block size in bytes.
 * @returns 0 if no one VMDK image is opened in HDD container.
 */
IDER3DECL(unsigned) VMDKDiskGetBlockSize(PVMDKDISK pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    return VMDK_GEOMETRY_SECTOR_SIZE;
}

/**
 * Get virtual disk geometry stored in image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   pcCylinders     Where to store the number of cylinders. NULL is ok.
 * @param   pcHeads         Where to store the number of heads. NULL is ok.
 * @param   pcSectors       Where to store the number of sectors. NULL is ok.
 */
IDER3DECL(int) VMDKDiskGetGeometry(PVMDKDISK pDisk, unsigned *pcCylinders, unsigned *pcHeads, unsigned *pcSectors)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    PVMDKDISKGEOMETRY pGeometry = &pDisk->Geometry;

    LogFlow(("VDIDiskGetGeometry: C/H/S = %u/%u/%u\n",
             pGeometry->cCylinders, pGeometry->cHeads, pGeometry->cSectors));

    int rc = VINF_SUCCESS;

    if (    pGeometry->cCylinders > 0
        &&  pGeometry->cHeads > 0
        &&  pGeometry->cSectors > 0)
    {
        if (pcCylinders)
            *pcCylinders = pDisk->Geometry.cCylinders;
        if (pcHeads)
            *pcHeads = pDisk->Geometry.cHeads;
        if (pcSectors)
            *pcSectors = pDisk->Geometry.cSectors;
    }
    else
        rc = VERR_VDI_GEOMETRY_NOT_SET;

    LogFlow(("VDIDiskGetGeometry: returns %Vrc\n", rc));
    return rc;
}

/**
 * Store virtual disk geometry into base image file of HDD container.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   cCylinders      Number of cylinders.
 * @param   cHeads          Number of heads.
 * @param   cSectors        Number of sectors.
 */
IDER3DECL(int) VMDKDiskSetGeometry(PVMDKDISK pDisk, unsigned cCylinders, unsigned cHeads, unsigned cSectors)
{
    LogFlow(("VMDKDiskSetGeometry: C/H/S = %u/%u/%u\n", cCylinders, cHeads, cSectors));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    pDisk->Geometry.cCylinders = cCylinders;
    pDisk->Geometry.cHeads     = cHeads;
    pDisk->Geometry.cSectors   = cSectors;
    pDisk->Geometry.cbSector   = VMDK_GEOMETRY_SECTOR_SIZE;

    /** @todo Update header information in base image file. */
    return VINF_SUCCESS;
}

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images is opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 */
IDER3DECL(int) VMDKDiskGetImagesCount(PVMDKDISK pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VMDKDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    return 1;
}

/*******************************************************************************
*   PDM interface                                                              *
*******************************************************************************/

/**
 * Construct a VBox HDD media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvVmdkConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    LogFlow(("drvVmdkConstruct:\n"));
    PVMDKDISK pData = PDMINS2DATA(pDrvIns, PVMDKDISK);

    /*
     * Init the static parts.
     */
    pDrvIns->IBase.pfnQueryInterface    = drvVmdkQueryInterface;
    pData->pDrvIns = pDrvIns;

    pData->u32Signature = VMDKDISK_SIGNATURE;

    pData->Geometry.cCylinders = 0;
    pData->Geometry.cHeads = 0;
    pData->Geometry.cSectors = 0;
    pData->Geometry.cbSector = 0;

    /* IMedia */
    pData->IMedia.pfnRead               = drvVmdkRead;
    pData->IMedia.pfnWrite              = drvVmdkWrite;
    pData->IMedia.pfnFlush              = drvVmdkFlush;
    pData->IMedia.pfnGetSize            = drvVmdkGetSize;
    pData->IMedia.pfnGetUuid            = drvVmdkGetUuid;
    pData->IMedia.pfnIsReadOnly         = drvVmdkIsReadOnly;
    pData->IMedia.pfnBiosGetGeometry    = drvVmdkBiosGetGeometry;
    pData->IMedia.pfnBiosSetGeometry    = drvVmdkBiosSetGeometry;
    pData->IMedia.pfnBiosGetTranslation = drvVmdkBiosGetTranslation;
    pData->IMedia.pfnBiosSetTranslation = drvVmdkBiosSetTranslation;

    /*
     * Validate and read top level configuration.
     */
    char *pszName;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "Path", &pszName);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("VHDD: Configuration error: Querying \"Path\" as string failed"));

    /** True if the media is readonly. */
    bool fReadOnly;
    rc = CFGMR3QueryBool(pCfgHandle, "ReadOnly", &fReadOnly);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fReadOnly = false;
    else if (VBOX_FAILURE(rc))
    {
        MMR3HeapFree(pszName);
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("VHDD: Configuration error: Querying \"ReadOnly\" as boolean failed"));
    }

    /*
     * Open the image.
     */
    rc = vmdk_open(pData, pszName, fReadOnly);
    if (VBOX_SUCCESS(rc))
        Log(("drvVmdkConstruct: Opened '%s' in %s mode\n", pszName, VMDKDiskIsReadOnly(pData) ? "read-only" : "read-write"));
    else
        AssertMsgFailed(("Failed to open image '%s' rc=%Vrc\n", pszName, rc));

    MMR3HeapFree(pszName);
    pszName = NULL;

    return rc;
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvVmdkDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvVmdkDestruct:\n"));
    PVMDKDISK pData = PDMINS2DATA(pDrvIns, PVMDKDISK);
    vmdk_close(&pData->VmdkState);
}

/**
 * When the VM has been suspended we'll change the image mode to read-only
 * so that main and others can read the VDIs. This is important when
 * saving state and so forth.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvVmdkSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvVmdkSuspend:\n"));
    PVMDKDISK pData = PDMINS2DATA(pDrvIns, PVMDKDISK);
    if (!VMDKDiskIsReadOnly(pData))
    {
        /** @todo does this even make sense? the vdi method locks the whole file, but don't we close it afterwards?? */
        //int rc = vmdkChangeImageMode(pData, true);
        //AssertRC(rc);
    }
}

/**
 * Before the VM resumes we'll have to undo the read-only mode change
 * done in drvVmdkSuspend.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvVmdkResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvVmdkSuspend:\n"));
    PVMDKDISK pData = PDMINS2DATA(pDrvIns, PVMDKDISK);
    if (!VMDKDiskIsReadOnly(pData))
    {
        /** @todo does this even make sense? the vdi method locks the whole file, but don't we close it afterwards?? */
        //int rc = vmdkChangeImageMode(pData, false);
        //AssertRC(rc);
    }
}


/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvVmdkGetSize(PPDMIMEDIA pInterface)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    uint64_t cb = VMDKDiskGetSize(pData);
    LogFlow(("drvVmdkGetSize: returns %#llx (%llu)\n", cb, cb));
    return cb;
}

/**
 * Get stored media geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosGetGeometry for details.
 */
static DECLCALLBACK(int) drvVmdkBiosGetGeometry(PPDMIMEDIA pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    int rc = VMDKDiskGetGeometry(pData, pcCylinders, pcHeads, pcSectors);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("drvVmdkBiosGetGeometry: returns VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    Log(("drvVmdkBiosGetGeometry: The Bios geometry data was not available.\n"));
    return VERR_PDM_GEOMETRY_NOT_SET;
}

/**
 * Set stored media geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosSetGeometry for details.
 */
static DECLCALLBACK(int) drvVmdkBiosSetGeometry(PPDMIMEDIA pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    int rc = VMDKDiskSetGeometry(pData, cCylinders, cHeads, cSectors);
    LogFlow(("drvVmdkBiosSetGeometry: returns %Vrc (%d,%d,%d)\n", rc, cCylinders, cHeads, cSectors));
    return rc;
}

/**
 * Read bits.
 *
 * @see PDMIMEDIA::pfnRead for details.
 */
static DECLCALLBACK(int) drvVmdkRead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    LogFlow(("drvVmdkRead: off=%#llx pvBuf=%p cbRead=%d\n", off, pvBuf, cbRead));
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    int rc = vmdk_read(&pData->VmdkState, off >> VMDK_GEOMETRY_SECTOR_SHIFT, (uint8_t *)pvBuf, cbRead >> VMDK_GEOMETRY_SECTOR_SHIFT);
    if (VBOX_SUCCESS(rc))
        Log2(("drvVmdkRead: off=%#llx pvBuf=%p cbRead=%d\n"
              "%.*Vhxd\n",
              off, pvBuf, cbRead, cbRead, pvBuf));
    LogFlow(("drvVmdkRead: returns %Vrc\n", rc));
    return rc;
}

/**
 * Write bits.
 *
 * @see PDMIMEDIA::pfnWrite for details.
 */
static DECLCALLBACK(int) drvVmdkWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    LogFlow(("drvVmdkWrite: off=%#llx pvBuf=%p cbWrite=%d\n", off, pvBuf, cbWrite));
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    Log2(("drvVmdkWrite: off=%#llx pvBuf=%p cbWrite=%d\n"
          "%.*Vhxd\n",
          off, pvBuf, cbWrite, cbWrite, pvBuf));
    int rc = vmdk_write(&pData->VmdkState, off >> VMDK_GEOMETRY_SECTOR_SHIFT, (const uint8_t *)pvBuf, cbWrite >> VMDK_GEOMETRY_SECTOR_SHIFT);
    LogFlow(("drvVmdkWrite: returns %Vrc\n", rc));
    return rc;
}

/**
 * Flush bits to media.
 *
 * @see PDMIMEDIA::pfnFlush for details.
 */
static DECLCALLBACK(int) drvVmdkFlush(PPDMIMEDIA pInterface)
{
    LogFlow(("drvVmdkFlush:\n"));
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    vmdk_flush(&pData->VmdkState);
    int rc = VINF_SUCCESS;
    LogFlow(("drvVmdkFlush: returns %Vrc\n", rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvVmdkGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    /** @todo */
    int rc = VINF_SUCCESS;
    NOREF(pData);
    LogFlow(("drvVmdkGetUuid: returns %Vrc ({%Vuuid})\n", rc, pUuid));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvVmdkIsReadOnly(PPDMIMEDIA pInterface)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    LogFlow(("drvVmdkIsReadOnly: returns %d\n", VMDKDiskIsReadOnly(pData)));
    return VMDKDiskIsReadOnly(pData);
}

/** @copydoc PDMIMEDIA::pfnBiosGetTranslation */
static DECLCALLBACK(int) drvVmdkBiosGetTranslation(PPDMIMEDIA pInterface,
                                               PPDMBIOSTRANSLATION penmTranslation)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    int rc = VINF_SUCCESS;
    NOREF(pData);
    *penmTranslation = PDMBIOSTRANSLATION_AUTO; /** @todo */
    LogFlow(("drvVmdkBiosGetTranslation: returns %Vrc (%d)\n", rc, *penmTranslation));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosSetTranslation */
static DECLCALLBACK(int) drvVmdkBiosSetTranslation(PPDMIMEDIA pInterface,
                                               PDMBIOSTRANSLATION enmTranslation)
{
    PVMDKDISK pData = PDMIMEDIA_2_VMDKDISK(pInterface);
    /** @todo */
    int rc = VINF_SUCCESS;
    NOREF(pData);
    LogFlow(("drvVmdkBiosSetTranslation: returns %Vrc (%d)\n", rc, enmTranslation));
    return rc;
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvVmdkQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PVMDKDISK pData = PDMINS2DATA(pDrvIns, PVMDKDISK);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MEDIA:
            return &pData->IMedia;
        default:
            return NULL;
    }
}


/**
 * VMDK driver registration record.
 */
const PDMDRVREG g_DrvVmdkHDD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "VmdkHDD",
    /* pszDescription */
    "VMDK media driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(VMDKDISK),
    /* pfnConstruct */
    drvVmdkConstruct,
    /* pfnDestruct */
    drvVmdkDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvVmdkSuspend,
    /* pfnResume */
    drvVmdkResume,
    /* pfnDetach */
    NULL
};
