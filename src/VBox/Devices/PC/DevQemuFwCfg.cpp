/* $Id$ */
/** @file
 * DevQemuFwCfg - QEMU firmware configuration compatible device.
 */

/*
 * Copyright (C) 2020-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/** @page pg_qemufwcfg   The QEMU firmware configuration Device.
 *
 * The QEMU firmware configuration device is a custom device emulation
 * to convey information about the VM to the guests firmware (UEFI for example).
 * In the case of VirtualBox it is used to directly load a compatible kernel
 * and initrd image like Linux from the host into the guest and boot it. This allows
 * efficiently testing/debugging of multiple Linux kernels without having to install
 * a guest OS. On VirtualBox the EFI firmware supports this interface, the BIOS is
 * currently unsupported (and probably never will be).
 *
 * @section sec_qemufwcfg_config    Configuration
 *
 * To use this interface for a particular VM the following extra data needs to be
 * set besides enabling the EFI firmware:
 *
 *     VBoxManage setextradata <VM name> "VBoxInternal/Devices/qemu-fw-cfg/0/Config/KernelImage" /path/to/kernel
 *     VBoxManage setextradata <VM name> "VBoxInternal/Devices/qemu-fw-cfg/0/Config/InitrdImage" /path/to/initrd
 *     VBoxManage setextradata <VM name> "VBoxInternal/Devices/qemu-fw-cfg/0/Config/CmdLine"     "<cmd line string>"
 *
 * The only mandatory item is the KernelImage one, the others are optional if the
 * kernel is configured to not require it. The InitrdImage item accepts a path to a directory as well.
 * If a directory is encountered, the CPIO initrd image is created on the fly and passed to the guest.
 * If the kernel is not an EFI compatible executable (CONFIG_EFI_STUB=y for Linux) a dedicated setup image might be required
 * which can be set with:
 *
 *     VBoxManage setextradata <VM name> "VBoxInternal/Devices/qemu-fw-cfg/0/Config/SetupImage" /path/to/setup_image
 *
 * @section sec_qemufwcfg_dma    DMA
 *
 * The QEMU firmware configuration device supports an optional DMA interface to speed up transferring the data into the guest.
 * It currently is not enabled by default but needs to be enabled with:
 *
 *     VBoxManage setextradata <VM name> "VBoxInternal/Devices/qemu-fw-cfg/0/Config/DmaEnabled" 1
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_QEMUFWCFG
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/zero.h>
#include <iprt/zip.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Start of the I/O port region. */
#define QEMU_FW_CFG_IO_PORT_START                   0x510
/** Number of I/O ports reserved for this device. */
#define QEMU_FW_CFG_IO_PORT_SIZE                    12
/** Offset of the config item selector register from the start. */
#define QEMU_FW_CFG_OFF_SELECTOR                    0
/** Offset of the data port from the start. */
#define QEMU_FW_CFG_OFF_DATA                        1
/** Offset of the high 32bit of the DMA address. */
#define QEMU_FW_CFG_OFF_DMA_HIGH                    4
/** Offset of the low 32bit of the DMA address. */
#define QEMU_FW_CFG_OFF_DMA_LOW                     8


/** @name MMIO register offsets.
 * @{ */
/** Data register offset. */
#define QEU_FW_CFG_MMIO_OFF_DATA                    0
/** Selector register offset. */
#define QEU_FW_CFG_MMIO_OFF_SELECTOR                8
/** DMA base address register offset. */
#define QEU_FW_CFG_MMIO_OFF_DMA                     16
/** @} */


/** Set if legacy interface is supported (always set).*/
#define QEMU_FW_CFG_VERSION_LEGACY                  RT_BIT_32(0)
/** Set if DMA is supported.*/
#define QEMU_FW_CFG_VERSION_DMA                     RT_BIT_32(1)


/** Error happened during the DMA access. */
#define QEMU_FW_CFG_DMA_ERROR                       RT_BIT_32(0)
/** Read requested. */
#define QEMU_FW_CFG_DMA_READ                        RT_BIT_32(1)
/** Skipping bytes requested. */
#define QEMU_FW_CFG_DMA_SKIP                        RT_BIT_32(2)
/** The config item is selected. */
#define QEMU_FW_CFG_DMA_SELECT                      RT_BIT_32(3)
/** Write requested. */
#define QEMU_FW_CFG_DMA_WRITE                       RT_BIT_32(4)
/** Extracts the selected config item. */
#define QEMU_FW_CFG_DMA_GET_CFG_ITEM(a_Control)     ((uint16_t)((a_Control) >> 16))


/** @name Known config items.
 * @{ */
#define QEMU_FW_CFG_ITEM_SIGNATURE                  UINT16_C(0x0000)
#define QEMU_FW_CFG_ITEM_VERSION                    UINT16_C(0x0001)
#define QEMU_FW_CFG_ITEM_SYSTEM_UUID                UINT16_C(0x0002)
#define QEMU_FW_CFG_ITEM_RAM_SIZE                   UINT16_C(0x0003)
#define QEMU_FW_CFG_ITEM_GRAPHICS_ENABLED           UINT16_C(0x0004)
#define QEMU_FW_CFG_ITEM_SMP_CPU_COUNT              UINT16_C(0x0005)
#define QEMU_FW_CFG_ITEM_MACHINE_ID                 UINT16_C(0x0006)
#define QEMU_FW_CFG_ITEM_KERNEL_ADDRESS             UINT16_C(0x0007)
#define QEMU_FW_CFG_ITEM_KERNEL_SIZE                UINT16_C(0x0008)
#define QEMU_FW_CFG_ITEM_KERNEL_CMD_LINE            UINT16_C(0x0009)
#define QEMU_FW_CFG_ITEM_INITRD_ADDRESS             UINT16_C(0x000a)
#define QEMU_FW_CFG_ITEM_INITRD_SIZE                UINT16_C(0x000b)
#define QEMU_FW_CFG_ITEM_BOOT_DEVICE                UINT16_C(0x000c)
#define QEMU_FW_CFG_ITEM_NUMA_DATA                  UINT16_C(0x000d)
#define QEMU_FW_CFG_ITEM_BOOT_MENU                  UINT16_C(0x000e)
#define QEMU_FW_CFG_ITEM_MAX_CPU_COUNT              UINT16_C(0x000f)
#define QEMU_FW_CFG_ITEM_KERNEL_ENTRY               UINT16_C(0x0010)
#define QEMU_FW_CFG_ITEM_KERNEL_DATA                UINT16_C(0x0011)
#define QEMU_FW_CFG_ITEM_INITRD_DATA                UINT16_C(0x0012)
#define QEMU_FW_CFG_ITEM_CMD_LINE_ADDRESS           UINT16_C(0x0013)
#define QEMU_FW_CFG_ITEM_CMD_LINE_SIZE              UINT16_C(0x0014)
#define QEMU_FW_CFG_ITEM_CMD_LINE_DATA              UINT16_C(0x0015)
#define QEMU_FW_CFG_ITEM_KERNEL_SETUP_ADDRESS       UINT16_C(0x0016)
#define QEMU_FW_CFG_ITEM_KERNEL_SETUP_SIZE          UINT16_C(0x0017)
#define QEMU_FW_CFG_ITEM_KERNEL_SETUP_DATA          UINT16_C(0x0018)
#define QEMU_FW_CFG_ITEM_FILE_DIR                   UINT16_C(0x0019)
/** @} */

/** The size of the directory entry buffer we're using. */
#define QEMUFWCFG_DIRENTRY_BUF_SIZE (sizeof(RTDIRENTRYEX) + RTPATH_MAX)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * QEMU firmware config DMA descriptor.
 */
typedef struct QEMUFWDMADESC
{
    /** Control field. */
    uint32_t                    u32Ctrl;
    /** Length of the transfer in bytes. */
    uint32_t                    u32Length;
    /** Address of the buffer to transfer from/to. */
    uint64_t                    u64GCPhysBuf;
} QEMUFWDMADESC;
AssertCompileSize(QEMUFWDMADESC, 2 * 4 + 8);
/** Pointer to a QEMU firmware config DMA descriptor. */
typedef QEMUFWDMADESC *PQEMUFWDMADESC;
/** Pointer to a const QEMU firmware config DMA descriptor. */
typedef const QEMUFWDMADESC *PCQEMUFWDMADESC;


/** Pointer to a const configuration item descriptor. */
typedef const struct QEMUFWCFGITEM *PCQEMUFWCFGITEM;

/**
 * QEMU firmware config instance data structure.
 */
typedef struct DEVQEMUFWCFG
{
    /** Pointer back to the device instance. */
    PPDMDEVINS                  pDevIns;
    /** The configuration handle. */
    PCFGMNODE                   pCfg;
    /** Pointer to the currently selected item. */
    PCQEMUFWCFGITEM             pCfgItem;
    /** Offset of the next byte to read from the start of the data item. */
    uint32_t                    offCfgItemNext;
    /** How many bytes are left for transfer. */
    uint32_t                    cbCfgItemLeft;
    /** Version register. */
    uint32_t                    u32Version;
    /** Guest physical address of the DMA descriptor. */
    RTGCPHYS                    GCPhysDma;
    /** VFS file of the on-the-fly created initramfs. */
    RTVFSFILE                   hVfsFileInitrd;

    /** Scratch buffer for config item specific data. */
    union
    {
        uint8_t                 u8;
        uint16_t                u16;
        uint32_t                u32;
        uint64_t                u64;
        /** VFS file handle. */
        RTVFSFILE               hVfsFile;
        /** Byte view. */
        uint8_t                 ab[8];
    } u;
} DEVQEMUFWCFG;
/** Pointer to the QEMU firmware config device instance. */
typedef DEVQEMUFWCFG *PDEVQEMUFWCFG;


/**
 * A supported configuration item descriptor.
 */
typedef struct QEMUFWCFGITEM
{
    /** The config tiem value. */
    uint16_t                    uCfgItem;
    /** Name of the item. */
    const char                  *pszItem;
    /** Optional CFGM key to lookup the content. */
    const char                  *pszCfgmKey;
    /**
     * Setup callback for when the guest writes the selector.
     *
     * @returns VBox status code.
     * @param   pThis           The QEMU fw config device instance.
     * @param   pItem           Pointer to the selected item.
     * @param   pcbItem         Where to store the size of the item on success.
     */
    DECLCALLBACKMEMBER(int, pfnSetup, (PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem));
    /**
     * Read callback to return the data.
     *
     * @returns VBox status code.
     * @param   pThis           The QEMU fw config device instance.
     * @param   pItem           Pointer to the selected item.
     * @param   off             Where to start reading from.
     * @param   pvBuf           Where to store the read data.
     * @param   cbToRead        How much to read.
     * @param   pcbRead         Where to store the amount of bytes read.
     */
    DECLCALLBACKMEMBER(int, pfnRead, (PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t off, void *pvBuf,
                                      uint32_t cbToRead, uint32_t *pcbRead));

    /**
     * Cleans up any allocated resources when the item is de-selected.
     *
     * @returns nothing.
     * @param   pThis           The QEMU fw config device instance.
     * @param   pItem           Pointer to the selected item.
     */
    DECLCALLBACKMEMBER(void, pfnCleanup, (PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem));
} QEMUFWCFGITEM;
/** Pointer to a configuration item descriptor. */
typedef QEMUFWCFGITEM *PQEMUFWCFGITEM;



/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for the signature configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupSignature(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    RT_NOREF(pThis, pItem);
    uint8_t abSig[] = { 'Q', 'E', 'M', 'U' };
    memcpy(&pThis->u.ab[0], &abSig[0], sizeof(abSig));
    *pcbItem = sizeof(abSig);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for the version configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupVersion(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    RT_NOREF(pThis, pItem);
    memcpy(&pThis->u.ab[0], &pThis->u32Version, sizeof(pThis->u32Version));
    *pcbItem = sizeof(pThis->u32Version);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for the file directory configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupFileDir(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    RT_NOREF(pThis, pItem);
    memset(&pThis->u.ab[0], 0, sizeof(uint32_t)); /** @todo Implement */
    *pcbItem = sizeof(uint32_t);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for the size config item belonging to a VFS file type configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupCfgmFileSz(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    PCPDMDEVHLPR3 pHlp  = pThis->pDevIns->pHlpR3;

    int rc = VINF_SUCCESS;
    RTVFSFILE hVfsFile = NIL_RTVFSFILE;
    if (   pItem->uCfgItem == QEMU_FW_CFG_ITEM_INITRD_SIZE
        && pThis->hVfsFileInitrd != NIL_RTVFSFILE)
    {
        RTVfsFileRetain(pThis->hVfsFileInitrd);
        hVfsFile = pThis->hVfsFileInitrd;
    }
    else
    {
        /* Query the path from the CFGM key. */
        char *pszFilePath = NULL;
        rc = pHlp->pfnCFGMQueryStringAlloc(pThis->pCfg, pItem->pszCfgmKey, &pszFilePath);
        if (RT_SUCCESS(rc))
        {
            rc = RTVfsFileOpenNormal(pszFilePath, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN, &hVfsFile);
            if (RT_FAILURE(rc))
                LogRel(("QemuFwCfg: Failed to open file \"%s\" -> %Rrc\n", pszFilePath, rc));
            PDMDevHlpMMHeapFree(pThis->pDevIns, pszFilePath);
        }
        else
            LogRel(("QemuFwCfg: Failed to query \"%s\" -> %Rrc\n", pItem->pszCfgmKey, rc));
    }

    if (RT_SUCCESS(rc))
    {
        uint64_t cbFile = 0;
        rc = RTVfsFileQuerySize(hVfsFile, &cbFile);
        if (RT_SUCCESS(rc))
        {
            if (cbFile < _4G)
            {
                pThis->u.u32 = (uint32_t)cbFile;
                *pcbItem = sizeof(uint32_t);
            }
            else
            {
                rc = VERR_BUFFER_OVERFLOW;
                LogRel(("QemuFwCfg: File exceeds 4G limit (%llu)\n", cbFile));
            }
        }
        else
            LogRel(("QemuFwCfg: Failed to query file size from -> %Rrc\n", rc));
        RTVfsFileRelease(hVfsFile);
    }

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for the size config item belonging to a string type configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupCfgmStrSz(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    PCPDMDEVHLPR3 pHlp  = pThis->pDevIns->pHlpR3;

    /* Query the string from the CFGM key. */
    char sz[_4K];
    int rc = pHlp->pfnCFGMQueryString(pThis->pCfg, pItem->pszCfgmKey, &sz[0], sizeof(sz));
    if (RT_SUCCESS(rc))
    {
        pThis->u.u32 = (uint32_t)strlen(&sz[0]) + 1;
        *pcbItem = sizeof(uint32_t);
    }
    else
        LogRel(("QemuFwCfg: Failed to query \"%s\" -> %Rrc\n", pItem->pszCfgmKey, rc));

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for a string type configuration item gathered from CFGM.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupCfgmStr(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    PCPDMDEVHLPR3 pHlp  = pThis->pDevIns->pHlpR3;

    /* Query the string from the CFGM key. */
    char sz[_4K];
    int rc = pHlp->pfnCFGMQueryString(pThis->pCfg, pItem->pszCfgmKey, &sz[0], sizeof(sz));
    if (RT_SUCCESS(rc))
        *pcbItem = (uint32_t)strlen(&sz[0]) + 1;
    else
        LogRel(("QemuFwCfg: Failed to query \"%s\" -> %Rrc\n", pItem->pszCfgmKey, rc));

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnSetup, Sets up the data for a VFS file type configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3SetupCfgmFile(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t *pcbItem)
{
    PCPDMDEVHLPR3 pHlp  = pThis->pDevIns->pHlpR3;

    int rc = VINF_SUCCESS;
    if (   pItem->uCfgItem == QEMU_FW_CFG_ITEM_INITRD_DATA
        && pThis->hVfsFileInitrd != NIL_RTVFSFILE)
    {
        RTVfsFileRetain(pThis->hVfsFileInitrd);
        pThis->u.hVfsFile = pThis->hVfsFileInitrd;
    }
    else
    {
        /* Query the path from the CFGM key. */
        char *pszFilePath = NULL;
        rc = pHlp->pfnCFGMQueryStringAlloc(pThis->pCfg, pItem->pszCfgmKey, &pszFilePath);
        if (RT_SUCCESS(rc))
        {
            rc = RTVfsFileOpenNormal(pszFilePath, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN, &pThis->u.hVfsFile);
            if (RT_FAILURE(rc))
                LogRel(("QemuFwCfg: Failed to open file \"%s\" -> %Rrc\n", pszFilePath, rc));
            PDMDevHlpMMHeapFree(pThis->pDevIns, pszFilePath);
        }
        else
            LogRel(("QemuFwCfg: Failed to query \"%s\" -> %Rrc\n", pItem->pszCfgmKey, rc));
    }

    if (RT_SUCCESS(rc))
    {
        uint64_t cbFile = 0;
        rc = RTVfsFileQuerySize(pThis->u.hVfsFile, &cbFile);
        if (RT_SUCCESS(rc))
        {
            if (cbFile < _4G)
                *pcbItem = (uint32_t)cbFile;
            else
            {
                rc = VERR_BUFFER_OVERFLOW;
                LogRel(("QemuFwCfg: File exceeds 4G limit (%llu)\n", cbFile));
            }
        }
        else
            LogRel(("QemuFwCfg: Failed to query file size from -> %Rrc\n", rc));
    }

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnRead, Reads data from a configuration item having its data stored in the scratch buffer.}
 */
static DECLCALLBACK(int) qemuFwCfgR3ReadSimple(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t off, void *pvBuf,
                                               uint32_t cbToRead, uint32_t *pcbRead)
{
    RT_NOREF(pThis, pItem);
    memcpy(pvBuf, &pThis->u.ab[off], cbToRead);
    *pcbRead = cbToRead;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnRead, Reads data from a VFS file type configuration item.}
 */
static DECLCALLBACK(int) qemuFwCfgR3ReadVfsFile(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t off, void *pvBuf,
                                                uint32_t cbToRead, uint32_t *pcbRead)
{
    RT_NOREF(pItem);
    size_t cbRead = 0;
    int rc = RTVfsFileReadAt(pThis->u.hVfsFile, off, pvBuf, cbToRead, &cbRead);
    if (RT_SUCCESS(rc))
        *pcbRead = (uint32_t)cbRead;

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnRead, Reads a string item gathered from CFGM.}
 */
static DECLCALLBACK(int) qemuFwCfgR3ReadStr(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem, uint32_t off, void *pvBuf,
                                            uint32_t cbToRead, uint32_t *pcbRead)
{
    PCPDMDEVHLPR3 pHlp  = pThis->pDevIns->pHlpR3;

    /* Query the string from the CFGM key. */
    char sz[_4K];
    int rc = pHlp->pfnCFGMQueryString(pThis->pCfg, pItem->pszCfgmKey, &sz[0], sizeof(sz));
    if (RT_SUCCESS(rc))
    {
        uint32_t cch = (uint32_t)strlen(sz) + 1;
        if (off < cch)
        {
            uint32_t cbRead = RT_MIN(cbToRead, off - cch);
            memcpy(pvBuf, &sz[off], cbRead);
            *pcbRead = cbRead;
        }
        else
            rc = VERR_BUFFER_OVERFLOW;
    }
    else
        LogRel(("QemuFwCfg: Failed to query \"%s\" -> %Rrc\n", pItem->pszCfgmKey, rc));

    return rc;
}


/**
 * @interface_method_impl{QEMUFWCFGITEM,pfnCleanup, Cleans up a VFS file type configuration item.}
 */
static DECLCALLBACK(void) qemuFwCfgR3CleanupVfsFile(PDEVQEMUFWCFG pThis, PCQEMUFWCFGITEM pItem)
{
    RT_NOREF(pItem);
    RTVfsFileRelease(pThis->u.hVfsFile);
    pThis->u.hVfsFile = NIL_RTVFSFILE;
}


/**
 * Supported config items.
 */
static const QEMUFWCFGITEM g_aQemuFwCfgItems[] =
{
    /** u16Selector                         pszItem         pszCfgmKey           pfnSetup                    pfnRead                         pfnCleanup */
    { QEMU_FW_CFG_ITEM_SIGNATURE,           "Signature",    NULL,                qemuFwCfgR3SetupSignature,  qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_VERSION,             "Version",      NULL,                qemuFwCfgR3SetupVersion,    qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_KERNEL_SIZE,         "KrnlSz",       "KernelImage",       qemuFwCfgR3SetupCfgmFileSz, qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_KERNEL_DATA,         "KrnlDat",      "KernelImage",       qemuFwCfgR3SetupCfgmFile,   qemuFwCfgR3ReadVfsFile,         qemuFwCfgR3CleanupVfsFile },
    { QEMU_FW_CFG_ITEM_INITRD_SIZE,         "InitrdSz",     "InitrdImage",       qemuFwCfgR3SetupCfgmFileSz, qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_KERNEL_DATA,         "InitrdDat",    "InitrdImage",       qemuFwCfgR3SetupCfgmFile,   qemuFwCfgR3ReadVfsFile,         qemuFwCfgR3CleanupVfsFile },
    { QEMU_FW_CFG_ITEM_KERNEL_SETUP_SIZE,   "SetupSz",      "SetupImage",        qemuFwCfgR3SetupCfgmFileSz, qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_KERNEL_SETUP_DATA,   "SetupDat",     "SetupImage",        qemuFwCfgR3SetupCfgmFile,   qemuFwCfgR3ReadVfsFile,         qemuFwCfgR3CleanupVfsFile },
    { QEMU_FW_CFG_ITEM_CMD_LINE_SIZE,       "CmdLineSz",    "CmdLine",           qemuFwCfgR3SetupCfgmStrSz,  qemuFwCfgR3ReadSimple,          NULL                      },
    { QEMU_FW_CFG_ITEM_CMD_LINE_DATA,       "CmdLineDat",   "CmdLine",           qemuFwCfgR3SetupCfgmStr,    qemuFwCfgR3ReadStr,             NULL                      },
    { QEMU_FW_CFG_ITEM_FILE_DIR,            "FileDir",      NULL,                qemuFwCfgR3SetupFileDir,    qemuFwCfgR3ReadSimple,          NULL                      }
};


/**
 * Resets the currently selected item.
 *
 * @returns nothing.
 * @param   pThis               The QEMU fw config device instance.
 */
static void qemuFwCfgR3ItemReset(PDEVQEMUFWCFG pThis)
{
    if (   pThis->pCfgItem
        && pThis->pCfgItem->pfnCleanup)
        pThis->pCfgItem->pfnCleanup(pThis, pThis->pCfgItem);

    pThis->pCfgItem       = NULL;
    pThis->offCfgItemNext = 0;
    pThis->cbCfgItemLeft  = 0;
}


/**
 * Selects the given config item.
 *
 * @returns VBox status code.
 * @param   pThis               The QEMU fw config device instance.
 * @param   uCfgItem            The configuration item to select.
 */
static int qemuFwCfgItemSelect(PDEVQEMUFWCFG pThis, uint16_t uCfgItem)
{
    LogFlowFunc(("uCfgItem=%#x\n", uCfgItem));

    qemuFwCfgR3ItemReset(pThis);

    for (uint32_t i = 0; i < RT_ELEMENTS(g_aQemuFwCfgItems); i++)
    {
        PCQEMUFWCFGITEM pCfgItem = &g_aQemuFwCfgItems[i];

        if (pCfgItem->uCfgItem == uCfgItem)
        {
            uint32_t cbItem = 0;
            int rc = pCfgItem->pfnSetup(pThis, pCfgItem, &cbItem);
            if (RT_SUCCESS(rc))
            {
                pThis->pCfgItem      = pCfgItem;
                pThis->cbCfgItemLeft = cbItem;
                return VINF_SUCCESS;
            }

            return rc;
        }
    }

    return VERR_NOT_FOUND;
}


/**
 * Processes a DMA transfer.
 *
 * @returns nothing.
 * @param   pThis               The QEMU fw config device instance.
 * @param   GCPhysDma           The guest physical address of the DMA descriptor.
 */
static void qemuFwCfgDmaXfer(PDEVQEMUFWCFG pThis, RTGCPHYS GCPhysDma)
{
    QEMUFWDMADESC DmaDesc; RT_ZERO(DmaDesc);

    LogFlowFunc(("pThis=%p GCPhysDma=%RGp\n", pThis, GCPhysDma));

    PDMDevHlpPhysReadMeta(pThis->pDevIns, GCPhysDma, &DmaDesc, sizeof(DmaDesc));

    /* Convert from big endianess to host endianess. */
    DmaDesc.u32Ctrl      = RT_BE2H_U32(DmaDesc.u32Ctrl);
    DmaDesc.u32Length    = RT_BE2H_U32(DmaDesc.u32Length);
    DmaDesc.u64GCPhysBuf = RT_BE2H_U64(DmaDesc.u64GCPhysBuf);

    LogFlowFunc(("u32Ctrl=%#x u32Length=%u u64GCPhysBuf=%llx\n",
                 DmaDesc.u32Ctrl, DmaDesc.u32Length, DmaDesc.u64GCPhysBuf));

    /* If the select bit is set a select is performed. */
    int rc = VINF_SUCCESS;
    if (DmaDesc.u32Ctrl & QEMU_FW_CFG_DMA_SELECT)
        rc = qemuFwCfgItemSelect(pThis, QEMU_FW_CFG_DMA_GET_CFG_ITEM(DmaDesc.u32Ctrl));

    if (RT_SUCCESS(rc))
    {
        /* We don't support any writes right now. */
        if (DmaDesc.u32Ctrl & QEMU_FW_CFG_DMA_WRITE)
            rc = VERR_INVALID_PARAMETER;
        else if (   !pThis->pCfgItem
                 || !pThis->cbCfgItemLeft)
        {
            if (DmaDesc.u32Ctrl & QEMU_FW_CFG_DMA_READ)
            {
                /* Item is not supported, just zero out the indicated area. */
                RTGCPHYS GCPhysCur = DmaDesc.u64GCPhysBuf;
                uint32_t cbLeft = DmaDesc.u32Length;

                while (   RT_SUCCESS(rc)
                       && cbLeft)
                {
                    uint32_t cbZero = RT_MIN(_64K, cbLeft);

                    PDMDevHlpPhysWriteMeta(pThis->pDevIns, GCPhysCur, &g_abRTZero64K[0], cbZero);

                    cbLeft    -= cbZero;
                    GCPhysCur += cbZero;
                }
            }
            /* else: Assume Skip */
        }
        else
        {
            /* Read or skip. */
            RTGCPHYS GCPhysCur = DmaDesc.u64GCPhysBuf;
            uint32_t cbLeft = RT_MIN(DmaDesc.u32Length, pThis->cbCfgItemLeft);

            while (   RT_SUCCESS(rc)
                   && cbLeft)
            {
                uint8_t abTmp[_1K];
                uint32_t cbThisRead = RT_MIN(sizeof(abTmp), cbLeft);
                uint32_t cbRead;

                rc = pThis->pCfgItem->pfnRead(pThis, pThis->pCfgItem, pThis->offCfgItemNext, &abTmp[0],
                                               cbThisRead, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    if (DmaDesc.u32Ctrl & QEMU_FW_CFG_DMA_READ)
                        PDMDevHlpPhysWriteMeta(pThis->pDevIns, GCPhysCur, &abTmp[0], cbRead);
                    /* else: Assume Skip */

                    cbLeft    -= cbRead;
                    GCPhysCur += cbRead;

                    pThis->offCfgItemNext += cbRead;
                    pThis->cbCfgItemLeft  -= cbRead;
                }
            }
        }
    }

    LogFlowFunc(("pThis=%p GCPhysDma=%RGp -> %Rrc\n", pThis, GCPhysDma, rc));

    /* Write back the control field. */
    uint32_t u32Resp = RT_SUCCESS(rc) ? 0 : RT_H2BE_U32(QEMU_FW_CFG_DMA_ERROR);
    PDMDevHlpPhysWriteMeta(pThis->pDevIns, GCPhysDma, &u32Resp, sizeof(u32Resp));
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT, QEMU firmware configuration write.}
 */
static DECLCALLBACK(VBOXSTRICTRC) qemuFwCfgIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);
    NOREF(pvUser);

    LogFlowFunc(("offPort=%RTiop u32=%#x cb=%u\n", offPort, u32, cb));

    switch (offPort)
    {
        case QEMU_FW_CFG_OFF_SELECTOR:
        {
            if (cb == 2)
                qemuFwCfgItemSelect(pThis, (uint16_t)u32);
            break;
        }
        case QEMU_FW_CFG_OFF_DATA: /* Readonly, ignore */
            break;
        case QEMU_FW_CFG_OFF_DMA_HIGH:
        {
            if (cb == 4)
                pThis->GCPhysDma = ((RTGCPHYS)RT_BE2H_U32(u32)) << 32;
            break;
        }
        case QEMU_FW_CFG_OFF_DMA_LOW:
        {
            if (cb == 4)
            {
                pThis->GCPhysDma |= ((RTGCPHYS)RT_BE2H_U32(u32));
                qemuFwCfgDmaXfer(pThis, pThis->GCPhysDma);
                pThis->GCPhysDma = 0;
            }
            break;
        }
        default:
            rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d u32=%#x\n", offPort, cb, u32);
            break;
    }

    LogFlowFunc((" -> rc=%Rrc\n", rc));
    return rc;
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWIN, QEMU firmware configuration read.}
 */
static DECLCALLBACK(VBOXSTRICTRC) qemuFwCfgIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);
    NOREF(pvUser);

    *pu32 = 0;

    LogFlowFunc(("offPort=%RTiop cb=%u\n", offPort, cb));

    switch (offPort)
    {
        /* Selector (Writeonly, ignore). */
        case QEMU_FW_CFG_OFF_SELECTOR:
            break;
        case QEMU_FW_CFG_OFF_DATA:
        {
            if (cb == 1)
            {
                if (   pThis->cbCfgItemLeft
                    && pThis->pCfgItem)
                {
                    uint8_t bRead = 0;
                    uint32_t cbRead = 0;
                    int rc2 = pThis->pCfgItem->pfnRead(pThis, pThis->pCfgItem, pThis->offCfgItemNext, &bRead,
                                                       sizeof(bRead), &cbRead);
                    if (   RT_SUCCESS(rc2)
                        && cbRead == sizeof(bRead))
                    {
                        pThis->offCfgItemNext += cbRead;
                        pThis->cbCfgItemLeft  -= cbRead;
                        *pu32 = bRead;
                    }
                }
            }
            else
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d\n", offPort, cb);
            break;
        }

        default:
            rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d\n", offPort, cb);
            break;
    }

    LogFlowFunc(("offPort=%RTiop cb=%u -> rc=%Rrc u32=%#x\n", offPort, cb, rc, *pu32));

    return rc;
}


/* -=-=-=-=-=- MMIO callbacks -=-=-=-=-=- */


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) qemuFwCfgMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);
    RT_NOREF(pvUser);

    LogFlowFunc(("%RGp cb=%u\n", off, cb));

    AssertReturn(cb <= sizeof(uint64_t), VERR_INVALID_PARAMETER);

    VBOXSTRICTRC rc = VINF_SUCCESS;
    switch (off)
    {
        case QEU_FW_CFG_MMIO_OFF_DATA:
        {
            if (   pThis->cbCfgItemLeft
                && pThis->pCfgItem)
            {
                uint32_t cbRead = 0;
                int rc2 = pThis->pCfgItem->pfnRead(pThis, pThis->pCfgItem, pThis->offCfgItemNext, pv,
                                                   cb, &cbRead);
                if (RT_SUCCESS(rc2))
                {
                    pThis->offCfgItemNext += cbRead;
                    pThis->cbCfgItemLeft  -= cbRead;
                }
            }
            else
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "offMmio=%#x cb=%d\n", off, cb);
            break;
        }
        case QEU_FW_CFG_MMIO_OFF_SELECTOR:
        case QEU_FW_CFG_MMIO_OFF_DMA:
            /* Writeonly, ignore. */
            break;
        default:
            rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "offMmio=%#x cb=%d\n", off, cb);
            break;
    }

    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) qemuFwCfgMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    int rc = VINF_SUCCESS;
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);
    RT_NOREF(pvUser);

    LogFlowFunc(("off=%RGp cb=%u\n", off, cb));

    switch (off)
    {
        case QEU_FW_CFG_MMIO_OFF_DATA: /* Readonly, ignore */
            break;
        case QEU_FW_CFG_MMIO_OFF_SELECTOR:
        {
            if (cb == sizeof(uint16_t))
                qemuFwCfgItemSelect(pThis, RT_BE2H_U16(*(uint16_t *)pv));
            else
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "offMmio=%#x cb=%d\n", off, cb);
            break;
        }
        case QEU_FW_CFG_MMIO_OFF_DMA:
        {
            if (cb == sizeof(uint64_t))
            {
                pThis->GCPhysDma |= ((RTGCPHYS)RT_BE2H_U64(*(uint64_t *)pv));
                qemuFwCfgDmaXfer(pThis, pThis->GCPhysDma);
                pThis->GCPhysDma = 0;
            }
            else
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "offMmio=%#x cb=%d\n", off, cb);
            break;
        }
        default:
            rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "offMmio=%#x cb=%d\n", off, cb);
            break;
    }

    LogFlowFunc((" -> rc=%Rrc\n", rc));
    return rc;
}


/**
 * Sets up the initrd memory file and CPIO filesystem stream for writing.
 *
 * @returns VBox status code.
 * @param   pThis               The QEMU fw config device instance.
 * @param   phVfsFss            Where to return the filesystem stream handle.
 */
static int qemuFwCfgCreateOutputArchive(PDEVQEMUFWCFG pThis, PRTVFSFSSTREAM phVfsFss)
{
    int rc = RTVfsMemFileCreate(NIL_RTVFSIOSTREAM, 0 /*cbEstimate*/, &pThis->hVfsFileInitrd);
    if (RT_SUCCESS(rc))
    {
        RTVFSFSSTREAM hVfsFss;
        RTVFSIOSTREAM hVfsIosOut = RTVfsFileToIoStream(pThis->hVfsFileInitrd);
        rc = RTZipTarFsStreamToIoStream(hVfsIosOut, RTZIPTARFORMAT_CPIO_ASCII_NEW, 0 /*fFlags*/, &hVfsFss);
        if (RT_SUCCESS(rc))
        {
            rc = RTZipTarFsStreamSetOwner(hVfsFss, 0, "root");
            if (RT_SUCCESS(rc))
                rc = RTZipTarFsStreamSetGroup(hVfsFss, 0, "root");

            if (RT_SUCCESS(rc))
                *phVfsFss = hVfsFss;
            else
            {
                RTVfsFsStrmRelease(hVfsFss);
                *phVfsFss = NIL_RTVFSFSSTREAM;
            }
        }
        RTVfsIoStrmRelease(hVfsIosOut);
    }

    return rc;
}


/**
 * Archives a file.
 *
 * @returns VBox status code.
 * @param   hVfsFss         The TAR filesystem stream handle.
 * @param   pszSrc          The file path or VFS spec.
 * @param   pszDst          The name to archive the file under.
 * @param   pErrInfo        Error info buffer (saves stack space).
 */
static int qemuFwCfgInitrdArchiveFile(RTVFSFSSTREAM hVfsFss, const char *pszSrc,
                                      const char *pszDst, PRTERRINFOSTATIC pErrInfo)
{
    /* Open the file. */
    uint32_t        offError;
    RTVFSIOSTREAM   hVfsIosSrc;
    int rc = RTVfsChainOpenIoStream(pszSrc, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                    &hVfsIosSrc, &offError, RTErrInfoInitStatic(pErrInfo));
    if (RT_FAILURE(rc))
        return rc;

    /* I/O stream to base object. */
    RTVFSOBJ hVfsObjSrc = RTVfsObjFromIoStream(hVfsIosSrc);
    if (hVfsObjSrc != NIL_RTVFSOBJ)
    {
        /*
         * Add it to the stream.
         */
        rc = RTVfsFsStrmAdd(hVfsFss, pszDst, hVfsObjSrc, 0 /*fFlags*/);
        RTVfsIoStrmRelease(hVfsIosSrc);
        RTVfsObjRelease(hVfsObjSrc);
        return rc;
    }
    RTVfsIoStrmRelease(hVfsIosSrc);
    return VERR_INVALID_POINTER;
}


/**
 * Archives a symlink.
 *
 * @returns VBox status code.
 * @param   hVfsFss         The TAR filesystem stream handle.
 * @param   pszSrc          The file path or VFS spec.
 * @param   pszDst          The name to archive the file under.
 * @param   pErrInfo        Error info buffer (saves stack space).
 */
static int qemuFwCfgInitrdArchiveSymlink(RTVFSFSSTREAM hVfsFss, const char *pszSrc,
                                         const char *pszDst, PRTERRINFOSTATIC pErrInfo)
{
    /* Open the file. */
    uint32_t        offError;
    RTVFSOBJ        hVfsObjSrc;
    int rc = RTVfsChainOpenObj(pszSrc, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                               RTVFSOBJ_F_OPEN_SYMLINK | RTVFSOBJ_F_CREATE_NOTHING | RTPATH_F_ON_LINK,
                               &hVfsObjSrc, &offError, RTErrInfoInitStatic(pErrInfo));
    if (RT_FAILURE(rc))
        return rc;

    rc = RTVfsFsStrmAdd(hVfsFss, pszDst, hVfsObjSrc, 0 /*fFlags*/);
    RTVfsObjRelease(hVfsObjSrc);
    return rc;
}


/**
 * Sub-directory helper for creating archives.
 *
 * @returns VBox status code.
 * @param   hVfsFss         The TAR filesystem stream handle.
 * @param   pszSrc          The directory path or VFS spec.  We append to the
 *                          buffer as we decend.
 * @param   cchSrc          The length of the input.
 * @param   pszDst          The name to archive it the under.  We append to the
 *                          buffer as we decend.
 * @param   cchDst          The length of the input.
 * @param   pDirEntry       Directory entry to use for the directory to handle.
 * @param   pErrInfo        Error info buffer (saves stack space).
 */
static int qemuFwCfgInitrdArchiveDirSub(RTVFSFSSTREAM hVfsFss,
                                        char *pszSrc, size_t cchSrc,
                                        char pszDst[RTPATH_MAX], size_t cchDst, PRTDIRENTRYEX pDirEntry,
                                        PRTERRINFOSTATIC pErrInfo)
{
    uint32_t offError;
    RTVFSDIR hVfsIoDir;
    int rc = RTVfsChainOpenDir(pszSrc, 0 /*fFlags*/,
                               &hVfsIoDir, &offError, RTErrInfoInitStatic(pErrInfo));
    if (RT_FAILURE(rc))
        return rc;

    /* Make sure we've got some room in the path, to save us extra work further down. */
    if (cchSrc + 3 >= RTPATH_MAX)
        return VERR_FILENAME_TOO_LONG;

    /* Ensure we've got a trailing slash (there is space for it see above). */
    if (!RTPATH_IS_SEP(pszSrc[cchSrc - 1]))
    {
        pszSrc[cchSrc++] = RTPATH_SLASH;
        pszSrc[cchSrc]   = '\0';
    }

    /* Ditto for destination. */
    if (cchDst + 3 >= RTPATH_MAX)
        return VERR_FILENAME_TOO_LONG;

    /* Don't try adding the root directory. */
    if (*pszDst != '\0')
    {
        RTVFSOBJ hVfsObjSrc = RTVfsObjFromDir(hVfsIoDir);
        rc = RTVfsFsStrmAdd(hVfsFss, pszDst, hVfsObjSrc, 0 /*fFlags*/);
        RTVfsObjRelease(hVfsObjSrc);
        if (RT_FAILURE(rc))
            return rc;

        if (!RTPATH_IS_SEP(pszDst[cchDst - 1]))
        {
            pszDst[cchDst++] = RTPATH_SLASH;
            pszDst[cchDst]   = '\0';
        }
    }

    /*
     * Process the files and subdirs.
     */
    for (;;)
    {
        size_t cbDirEntry = QEMUFWCFG_DIRENTRY_BUF_SIZE;
        rc = RTVfsDirReadEx(hVfsIoDir, pDirEntry, &cbDirEntry, RTFSOBJATTRADD_UNIX);
        if (RT_FAILURE(rc))
            break;

        /* Check length. */
        if (pDirEntry->cbName + cchSrc + 3 >= RTPATH_MAX)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_DIRECTORY:
            {
                if (RTDirEntryExIsStdDotLink(pDirEntry))
                    continue;

                memcpy(&pszSrc[cchSrc], pDirEntry->szName, pDirEntry->cbName + 1);
                memcpy(&pszDst[cchDst], pDirEntry->szName, pDirEntry->cbName + 1);
                rc = qemuFwCfgInitrdArchiveDirSub(hVfsFss, pszSrc, cchSrc + pDirEntry->cbName,
                                                  pszDst, cchDst + pDirEntry->cbName, pDirEntry, pErrInfo);
                break;
            }

            case RTFS_TYPE_FILE:
            {
                memcpy(&pszSrc[cchSrc], pDirEntry->szName, pDirEntry->cbName + 1);
                memcpy(&pszDst[cchDst], pDirEntry->szName, pDirEntry->cbName + 1);
                rc = qemuFwCfgInitrdArchiveFile(hVfsFss, pszSrc, pszDst, pErrInfo);
                break;
            }

            case RTFS_TYPE_SYMLINK:
            {
                memcpy(&pszSrc[cchSrc], pDirEntry->szName, pDirEntry->cbName + 1);
                memcpy(&pszDst[cchDst], pDirEntry->szName, pDirEntry->cbName + 1);
                rc = qemuFwCfgInitrdArchiveSymlink(hVfsFss, pszSrc, pszDst, pErrInfo);
                break;
            }

            default:
            {
                LogRel(("Warning: File system type %#x for '%s' not implemented yet, sorry! Skipping ...\n",
                        pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK, pDirEntry->szName));
                break;
            }
        }
    }

    if (rc == VERR_NO_MORE_FILES)
        rc = VINF_SUCCESS;

    RTVfsDirRelease(hVfsIoDir);
    return rc;
}


/**
 * Archives a directory recursively.
 *
 * @returns VBox status code.
 * @param   hVfsFss         The CPIO filesystem stream handle.
 * @param   pszSrc          The directory path or VFS spec.  We append to the
 *                          buffer as we decend.
 * @param   cchSrc          The length of the input.
 * @param   pszDst          The name to archive it the under.  We append to the
 *                          buffer as we decend.
 * @param   cchDst          The length of the input.
 * @param   pErrInfo        Error info buffer (saves stack space).
 */
static int qemuFwCfgInitrdArchiveDir(RTVFSFSSTREAM hVfsFss, char pszSrc[RTPATH_MAX], size_t cchSrc,
                                     char pszDst[RTPATH_MAX], size_t cchDst,
                                     PRTERRINFOSTATIC pErrInfo)
{
    RT_NOREF(cchSrc);

    char szSrcAbs[RTPATH_MAX];
    int rc = RTPathAbs(pszSrc, szSrcAbs, sizeof(szSrcAbs));
    if (RT_FAILURE(rc))
        return rc;

    union
    {
        uint8_t         abPadding[QEMUFWCFG_DIRENTRY_BUF_SIZE];
        RTDIRENTRYEX    DirEntry;
    } uBuf;

    return qemuFwCfgInitrdArchiveDirSub(hVfsFss, szSrcAbs, strlen(szSrcAbs), pszDst, cchDst, &uBuf.DirEntry, pErrInfo);
}


/**
 * Creates an on the fly initramfs for a given root directory.
 *
 * @returns VBox status code.
 * @param   pThis               The QEMU fw config device instance.
 * @param   pszPath             The path to work on.
 */
static int qemuFwCfgInitrdCreate(PDEVQEMUFWCFG pThis, const char *pszPath)
{
    /*
     * First open the output file.
     */
    RTVFSFSSTREAM hVfsFss;
    int rc = qemuFwCfgCreateOutputArchive(pThis, &hVfsFss);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Construct/copy the source name.
     */
    char szSrc[RTPATH_MAX];
    rc = RTStrCopy(szSrc, sizeof(szSrc), pszPath);
    if (RT_SUCCESS(rc))
    {
        RTERRINFOSTATIC ErrInfo;
        char  szDst[RTPATH_MAX]; RT_ZERO(szDst);
        rc = qemuFwCfgInitrdArchiveDir(hVfsFss, szSrc, strlen(szSrc),
                                       szDst, strlen(szDst), &ErrInfo);
    }

    /*
     * Finalize the archive.
     */
    int rc2 = RTVfsFsStrmEnd(hVfsFss); AssertRC(rc2);
    RTVfsFsStrmRelease(hVfsFss);
    return rc;
}


/**
 * Checks whether creation of the initrd should be done on the fly.
 *
 * @returns VBox status code.
 * @param   pThis               The QEMU fw config device instance.
 */
static int qemuFwCfgInitrdMaybeCreate(PDEVQEMUFWCFG pThis)
{
    PPDMDEVINS pDevIns = pThis->pDevIns;
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    /* Query the path from the CFGM key. */
    char *pszFilePath = NULL;
    int rc = pHlp->pfnCFGMQueryStringAlloc(pThis->pCfg, "InitrdImage", &pszFilePath);
    if (RT_SUCCESS(rc))
    {
        if (RTDirExists(pszFilePath))
        {
            rc = qemuFwCfgInitrdCreate(pThis, pszFilePath);
            if (RT_FAILURE(rc))
                rc = PDMDEV_SET_ERROR(pDevIns, rc,
                                      N_("QemuFwCfg: failed to create the in memory initram filesystem"));
        }
        /*else: Probably a normal file. */
        PDMDevHlpMMHeapFree(pDevIns, pszFilePath);
    }
    else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
        rc = PDMDEV_SET_ERROR(pDevIns, rc,
                              N_("Configuration error: Querying \"InitrdImage\" as a string failed"));
    else
        rc = VINF_SUCCESS;

    return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) qemuFwCfgReset(PPDMDEVINS pDevIns)
{
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);

    qemuFwCfgR3ItemReset(pThis);
    pThis->GCPhysDma = 0;

    if (pThis->hVfsFileInitrd != NIL_RTVFSFILE)
        RTVfsFileRelease(pThis->hVfsFileInitrd);
    pThis->hVfsFileInitrd = NIL_RTVFSFILE;

    qemuFwCfgInitrdMaybeCreate(pThis); /* Ignores status code. */
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) qemuFwCfgDestruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);

    qemuFwCfgR3ItemReset(pThis);
    pThis->GCPhysDma = 0;

    if (pThis->hVfsFileInitrd != NIL_RTVFSFILE)
        RTVfsFileRelease(pThis->hVfsFileInitrd);
    pThis->hVfsFileInitrd = NIL_RTVFSFILE;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) qemuFwCfgConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVQEMUFWCFG pThis = PDMDEVINS_2_DATA(pDevIns, PDEVQEMUFWCFG);
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DmaEnabled"
                                           "|MmioBase"
                                           "|MmioSize"
                                           "|KernelImage"
                                           "|InitrdImage"
                                           "|SetupImage"
                                           "|CmdLine",
                                           "");

    bool fDmaEnabled = false;
    int rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "DmaEnabled", &fDmaEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"DmaEnabled\""));

    /*
     * Init the data.
     */
    pThis->pDevIns        = pDevIns;
    pThis->pCfg           = pCfg;
    pThis->u32Version     = QEMU_FW_CFG_VERSION_LEGACY | (fDmaEnabled ? QEMU_FW_CFG_VERSION_DMA : 0);
    pThis->GCPhysDma      = 0;
    pThis->hVfsFileInitrd = NIL_RTVFSFILE;

    RTGCPHYS GCPhysMmioBase = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "MmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"MmioBase\" value"));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        /*
         * Register standard I/O Ports
         */
        IOMIOPORTHANDLE hIoPorts;
        rc = PDMDevHlpIoPortCreateFlagsAndMap(pDevIns, QEMU_FW_CFG_IO_PORT_START, QEMU_FW_CFG_IO_PORT_SIZE, 0 /*fFlags*/,
                                              qemuFwCfgIoPortWrite, qemuFwCfgIoPortRead,
                                              "QEMU firmware configuration", NULL /*paExtDescs*/, &hIoPorts);
        AssertRCReturn(rc, rc);
    }
    else
    {
        uint32_t cbMmio = 0;
        rc = pHlp->pfnCFGMQueryU32(pCfg, "MmioSize", &cbMmio);
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc,
                                    N_("Configuration error: Failed to get the \"MmioSize\" value"));

        /*
         * Register and map the MMIO region.
         */
        IOMMMIOHANDLE hMmio;
        rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, cbMmio, qemuFwCfgMmioWrite, qemuFwCfgMmioRead,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU, "QemuFwCfg", &hMmio);
        AssertRCReturn(rc, rc);
    }

    qemuFwCfgR3ItemReset(pThis);

    rc = qemuFwCfgInitrdMaybeCreate(pThis);
    if (RT_FAILURE(rc))
        return rc; /* Error set. */

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceQemuFwCfg =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "qemu-fw-cfg",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_ARCH,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVQEMUFWCFG),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "QEMU Firmware Config compatible device",
#if defined(IN_RING3)
    /* .pszRCMod = */               "",
    /* .pszR0Mod = */               "",
    /* .pfnConstruct = */           qemuFwCfgConstruct,
    /* .pfnDestruct = */            qemuFwCfgDestruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               qemuFwCfgReset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           NULL,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

