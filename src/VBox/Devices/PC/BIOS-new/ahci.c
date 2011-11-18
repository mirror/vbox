/* $Id$ */
/** @file
 * AHCI host adapter driver to boot from SATA disks.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
/**
 * Parts are based on the int13_harddisk code in rombios.c
 */

//@todo!!!! save/restore high bits of EAX/ECX and whatever else may be needed.

#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "ebda.h"
#include "inlines.h"
#include "pciutil.h"
#include "vds.h"

#define VBOX_AHCI_DEBUG         1
#define VBOX_AHCI_INT13_DEBUG   0

#if VBOX_AHCI_DEBUG
# define VBOXAHCI_DEBUG(...)        BX_INFO(__VA_ARGS__)
#else
# define VBOXAHCI_DEBUG(...)
#endif

#if VBOX_AHCI_INT13_DEBUG
# define VBOXAHCI_INT13_DEBUG(...)  BX_INFO(__VA_ARGS__)
#else
# define VBOXAHCI_INT13_DEBUG(...)
#endif

#define AHCI_MAX_STORAGE_DEVICES 4

/* Number of S/G table entries in EDDS. */
#define NUM_EDDS_SG         16


/**
 * AHCI device data.
 */
typedef struct
{
    uint8_t     type;         // Detected type of ata (ata/atapi/none/unknown/scsi)
    uint8_t     device;       // Detected type of attached devices (hd/cd/none)
    uint8_t     removable;    // Removable device flag
    uint8_t     lock;         // Locks for removable devices
    uint16_t    blksize;      // block size
    chs_t       lchs;         // Logical CHS
    chs_t       pchs;         // Physical CHS
    uint32_t    cSectors;     // Total sectors count
    uint8_t     port;         // Port this device is on.
} ahci_device_t;

/**
 * AHCI PRDT structure.
 */
typedef struct
{
    uint32_t    phys_addr;
    uint32_t    something;
    uint32_t    reserved;
    uint32_t    len;
} ahci_prdt;

/**
 * AHCI controller data.
 */
typedef struct
{
    /** The AHCI command list as defined by chapter 4.2.2 of the Intel AHCI spec.
     *  Because the BIOS doesn't support NCQ only the first command header is defined
     *  to save memory. - Must be aligned on a 1K boundary.
     */
    uint32_t        aCmdHdr[0x8];
    /** Align the next structure on a 128 byte boundary. */
    uint8_t         abAlignment1[0x60];
    /** The command table of one request as defined by chapter 4.2.3 of the Intel AHCI spec.
     *  Must be aligned on 128 byte boundary.
     */
    uint8_t         abCmd[0x80];
    /** Physical Region Descriptor Table (PRDT) array. In other
     *  words, a scatter/gather descriptor list.
     */
    ahci_prdt       aPrdt[16];
    /** Memory for the received command FIS area as specified by chapter 4.2.1
     *  of the Intel AHCI spec. This area is normally 256 bytes big but to save memory
     *  only the first 96 bytes are used because it is assumed that the controller
     *  never writes to the UFIS or reserved area. - Must be aligned on a 256byte boundary.
     */
    uint8_t         abFisRecv[0x60];
    /** Base I/O port for the index/data register pair. */
    uint16_t        iobase;
    /** Current port which uses the memory to communicate with the controller. */
    uint8_t         port;
    /** AHCI device information. */
    ahci_device_t   aDevices[AHCI_MAX_STORAGE_DEVICES];
    /** Index of the next unoccupied device slot. */
    uint8_t         cDevices;
    /** Map between (bios hd id - 0x80) and ahci devices. */
    uint8_t         cHardDisks;
    uint8_t         aHdIdMap[AHCI_MAX_STORAGE_DEVICES];
    /** Map between (bios cd id - 0xE0) and ahci_devices. */
    uint8_t         cCdDrives;
    uint8_t         aCdIdMap[AHCI_MAX_STORAGE_DEVICES];
    /** Number of harddisks detected before the AHCI driver started detection. */
    uint8_t         cHardDisksOld;
    /** int13 handler to call if given device is not from AHCI. */
    uint16_t        pfnInt13Old;
    /** VDS EDDS DMA buffer descriptor structure. */
    vds_edds        edds;
    vds_sg          edds_more_sg[NUM_EDDS_SG - 1];
} ahci_t;

#define AhciData ((ahci_t *) 0)

/** PCI configuration fields. */
#define PCI_CONFIG_CAP                  0x34

#define PCI_CAP_ID_SATACR               0x12
#define VBOX_AHCI_NO_DEVICE 0xffff

#define RT_BIT_32(bit) ((uint32_t)(1L << (bit)))

/** Global register set. */
#define AHCI_HBA_SIZE 0x100

//@todo: what are the casts good for?
#define AHCI_REG_CAP ((uint32_t)0x00)
#define AHCI_REG_GHC ((uint32_t)0x04)
# define AHCI_GHC_AE RT_BIT_32(31)
# define AHCI_GHC_IR RT_BIT_32(1)
# define AHCI_GHC_HR RT_BIT_32(0)
#define AHCI_REG_IS  ((uint32_t)0x08)
#define AHCI_REG_PI  ((uint32_t)0x0c)
#define AHCI_REG_VS  ((uint32_t)0x10)

/** Per port register set. */
#define AHCI_PORT_SIZE     0x80

#define AHCI_REG_PORT_CLB  0x00
#define AHCI_REG_PORT_CLBU 0x04
#define AHCI_REG_PORT_FB   0x08
#define AHCI_REG_PORT_FBU  0x0c
#define AHCI_REG_PORT_IS   0x10
# define AHCI_REG_PORT_IS_DHRS RT_BIT_32(0)
#define AHCI_REG_PORT_IE   0x14
#define AHCI_REG_PORT_CMD  0x18
# define AHCI_REG_PORT_CMD_ST  RT_BIT_32(0)
# define AHCI_REG_PORT_CMD_FRE RT_BIT_32(4)
# define AHCI_REG_PORT_CMD_FR  RT_BIT_32(14)
# define AHCI_REG_PORT_CMD_CR  RT_BIT_32(15)
#define AHCI_REG_PORT_TFD  0x20
#define AHCI_REG_PORT_SIG  0x24
#define AHCI_REG_PORT_SSTS 0x28
#define AHCI_REG_PORT_SCTL 0x2c
#define AHCI_REG_PORT_SERR 0x30
#define AHCI_REG_PORT_SACT 0x34
#define AHCI_REG_PORT_CI   0x38

/** Returns the absolute register offset from a given port and port register. */
#define AHCI_PORT_REG(port, reg)    ((uint32_t)(AHCI_HBA_SIZE + (port) * AHCI_PORT_SIZE + (reg)))

#define AHCI_REG_IDX   0
#define AHCI_REG_DATA  4

/** Writes the given value to a AHCI register. */
#define AHCI_WRITE_REG(iobase, reg, val)                \
    outpd((iobase) + AHCI_REG_IDX, (uint32_t)(reg));    \
    outpd((iobase) + AHCI_REG_DATA, (uint32_t)(val))

/** Reads from a AHCI register. */
#define AHCI_READ_REG(iobase, reg, val)                 \
    outpd((iobase) + AHCI_REG_IDX, (uint32_t)(reg));    \
    (val) = inpd((iobase) + AHCI_REG_DATA)

/** Writes to the given port register. */
#define VBOXAHCI_PORT_WRITE_REG(iobase, port, reg, val)     \
    AHCI_WRITE_REG((iobase), AHCI_PORT_REG((port), (reg)), val)

/** Reads from the given port register. */
#define VBOXAHCI_PORT_READ_REG(iobase, port, reg, val)      \
    AHCI_READ_REG((iobase), AHCI_PORT_REG((port), (reg)), val)

#define ATA_CMD_IDENTIFY_DEVICE     0xEC
#define AHCI_CMD_READ_DMA_EXT       0x25
#define AHCI_CMD_WRITE_DMA_EXT      0x35


/* Warning: Destroys high bits of EAX. */
uint32_t inpd(uint16_t port);
#pragma aux inpd =      \
    ".386"              \
    "in     eax, dx"    \
    "mov    dx, ax"     \
    "shr    eax, 16"    \
    "xchg   ax, dx"     \
    parm [dx] value [dx ax] modify nomemory;

void outpd(uint16_t port, uint32_t val);
#pragma aux outpd =     \
    ".386"              \
    "xchg   ax, cx"     \
    "shl    eax, 16"    \
    "mov    ax, cx"     \
    "out    dx, eax"    \
    parm [dx] [cx ax] modify nomemory;


/**
 * Sets a given set of bits in a register.
 */
static void ahci_ctrl_set_bits(uint16_t iobase, uint32_t reg, uint32_t mask)
{
    outpd(iobase + AHCI_REG_IDX, reg);
    outpd(iobase + AHCI_REG_DATA, inpd(iobase + AHCI_REG_DATA) | mask);
}

/**
 * Clears a given set of bits in a register.
 */
static void ahci_ctrl_clear_bits(uint16_t iobase, uint32_t reg, uint32_t mask)
{
    outpd(iobase + AHCI_REG_IDX, reg);
    outpd(iobase + AHCI_REG_DATA, inpd(iobase + AHCI_REG_DATA) & ~mask);
}

/**
 * Returns whether at least one of the bits in the given mask is set
 * for a register.
 */
static uint8_t ahci_ctrl_is_bit_set(uint16_t iobase, uint32_t reg, uint32_t mask)
{
    outpd(iobase + AHCI_REG_IDX, reg);
    return (inpd(iobase + AHCI_REG_DATA) & mask) != 0;
}

/**
 * Extracts a range of bits from a register and shifts them
 * to the right.
 */
static uint16_t ahci_ctrl_extract_bits(uint32_t val, uint32_t mask, uint8_t shift)
{
    return (val & mask) >> shift;
}

/**
 * Converts a segment:offset pair into a 32bit physical address.
 */
static uint32_t ahci_addr_to_phys(void __far *ptr)
{
    return ((uint32_t)FP_SEG(ptr) << 4) + FP_OFF(ptr);
}

/**
 * Issues a command to the SATA controller and waits for completion.
 */
static void ahci_port_cmd_sync(uint16_t ahci_seg, uint16_t u16IoBase, bx_bool fWrite, 
                               bx_bool fAtapi, uint8_t cFisDWords, uint16_t cbData)
{
    uint8_t         u8Port;
    ahci_t __far    *ahci = ahci_seg :> 0;

    u8Port = ahci->port;

    if (u8Port != 0xff)
    {
        uint32_t    u32Val;

        /* Prepare the command header. */
        u32Val = (1L << 16) | RT_BIT_32(7);
        if (fWrite)
            u32Val |= RT_BIT_32(6);

        if (fAtapi)
            u32Val |= RT_BIT_32(5);

        u32Val |= cFisDWords;

        ahci->aCmdHdr[0] = u32Val;
        ahci->aCmdHdr[1] = cbData;
        ahci->aCmdHdr[2] = ahci_addr_to_phys(&ahci->abCmd[0]);

        /* Enable Command and FIS receive engine. */
        ahci_ctrl_set_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                           AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

        /* Queue command. */
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CI, 0x1);

        /* Wait for a D2H FIS. */
        VBOXAHCI_DEBUG("AHCI: Waiting for D2H FIS\n");
        while (ahci_ctrl_is_bit_set(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_IS),
                                    AHCI_REG_PORT_IS_DHRS) == 0)
        {
            // This is where we'd need some kind of a yield functionality...
        }

        ahci_ctrl_set_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_IS),
                           AHCI_REG_PORT_IS_DHRS); /* Acknowledge received D2H FIS. */

        /* Disable command engine. */
        ahci_ctrl_clear_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                             AHCI_REG_PORT_CMD_ST);

        /** @todo: Examine status. */
    }
    else
        VBOXAHCI_DEBUG("AHCI: Invalid port given\n");
}

/**
 * Issue command to device.
 */
//@todo: don't chop up arguments into bytes?!
static void ahci_cmd_data(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Cmd, uint8_t u8Feat, uint8_t u8Device, uint8_t u8CylHigh, uint8_t u8CylLow, uint8_t u8Sect,
                          uint8_t u8FeatExp, uint8_t u8CylHighExp, uint8_t u8CylLowExp, uint8_t u8SectExp, uint8_t u8SectCount, uint8_t u8SectCountExp,
                          void __far *buf, uint16_t cbData, bx_bool fWrite)
{
    ahci_t __far    *ahci = ahci_seg :> 0;

    _fmemset(&ahci->abCmd[0], 0, sizeof(ahci->abCmd));

    /* Prepare the FIS. */
    ahci->abCmd[0]  = 0x27;     /* FIS type H2D. */
    ahci->abCmd[1]  = 1 << 7;   /* Command update. */
    ahci->abCmd[2]  = u8Cmd;
    ahci->abCmd[3]  = u8Feat;

    ahci->abCmd[4]  = u8Sect;
    ahci->abCmd[5]  = u8CylLow;
    ahci->abCmd[6]  = u8CylHigh;
    ahci->abCmd[7]  = u8Device;

    ahci->abCmd[8]  = u8SectExp;
    ahci->abCmd[9]  = u8CylLowExp;
    ahci->abCmd[10] = u8CylHighExp;
    ahci->abCmd[11] = u8FeatExp;

    ahci->abCmd[12] = u8SectCount;
    ahci->abCmd[13] = u8SectCountExp;

    /* Lock memory needed for DMA. */
    ahci->edds.num_avail = NUM_EDDS_SG;
    vds_build_sg_list( &ahci->edds, buf, cbData );

    /* Set up the PRDT. */
    ahci->aPrdt[0].phys_addr = ahci->edds.u.sg[0].phys_addr;
    ahci->aPrdt[0].len       = ahci->edds.u.sg[0].size - 1;

    ahci_port_cmd_sync(ahci_seg, u16IoBase, fWrite, 0, 5, cbData);

    /* Unlock the buffer again. */
    vds_free_sg_list( &ahci->edds );
}

/**
 * Deinits the curent active port.
 */
static void ahci_port_deinit_current(uint16_t ahci_seg, uint16_t u16IoBase)
{
    uint8_t         u8Port;
    ahci_t __far    *ahci = ahci_seg :> 0;

    u8Port = ahci->port;

    if (u8Port != 0xff)
    {
        /* Put the port into an idle state. */
        ahci_ctrl_clear_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                             AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

        while (ahci_ctrl_is_bit_set(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                                    AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST | AHCI_REG_PORT_CMD_FR | AHCI_REG_PORT_CMD_CR) == 1)
        {
            VBOXAHCI_DEBUG("AHCI: Waiting for the port to idle\n");
        }

        /*
         * Port idles, set up memory for commands and received FIS and program the
         * address registers.
         */
        //@todo: merge memsets?
        _fmemset(&ahci->aCmdHdr[0], 0, sizeof(ahci->aCmdHdr));
        _fmemset(&ahci->abCmd[0], 0, sizeof(ahci->abCmd));
        _fmemset(&ahci->abFisRecv[0], 0, sizeof(ahci->abFisRecv));

        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FB, 0);
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FBU, 0);

        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLB, 0);
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLBU, 0);

        /* Disable all interrupts. */
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IE, 0);

        ahci->port = 0xff;
    }
}

/**
 * Brings a port into a minimal state to make device detection possible
 * or to queue requests.
 */
static void ahci_port_init(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Port)
{
    uint32_t        u32PhysAddr;
    ahci_t __far    *ahci = ahci_seg :> 0;

    /* Deinit any other port first. */
    ahci_port_deinit_current(ahci_seg, u16IoBase);

    /* Put the port into an idle state. */
    ahci_ctrl_clear_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                         AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

    while (ahci_ctrl_is_bit_set(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                                AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST | AHCI_REG_PORT_CMD_FR | AHCI_REG_PORT_CMD_CR) == 1)
    {
        VBOXAHCI_DEBUG("AHCI: Waiting for the port to idle\n");
    }

    /*
     * Port idles, set up memory for commands and received FIS and program the
     * address registers.
     */
    //@todo: just one memset?
    _fmemset(&ahci->aCmdHdr[0], 0, sizeof(ahci->aCmdHdr));
    _fmemset(&ahci->abCmd[0], 0, sizeof(ahci->abCmd));
    _fmemset(&ahci->abFisRecv[0], 0, sizeof(ahci->abFisRecv));

    u32PhysAddr = ahci_addr_to_phys(&ahci->abFisRecv);
    VBOXAHCI_DEBUG("AHCI: FIS receive area %lx from %x:%x\n", u32PhysAddr, ahci_seg, &AhciData->abFisRecv);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FB, u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FBU, 0);

    u32PhysAddr = ahci_addr_to_phys(&ahci->aCmdHdr);
    VBOXAHCI_DEBUG("AHCI: CMD list area %lx\n", u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLB, u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLBU, 0);

    /* Disable all interrupts. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IE, 0);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IS, 0xffffffff);
    /* Clear all errors. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SERR, 0xffffffff);

    ahci->port = u8Port;
}

/**
 * Write data to the device.
 */
static void ahci_cmd_data_out(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Port, uint8_t u8Cmd, uint32_t u32Lba, uint16_t u16Sectors, uint16_t SegData, uint16_t OffData)
{
    uint8_t u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    u8SectCount = (uint8_t)(u16Sectors & 0xff);
    u8SectCountExp = (uint8_t)((u16Sectors >> 8) & 0xff);;
    u8Sect = (uint8_t)(u32Lba & 0xff);
    u8SectExp = (uint8_t)((u32Lba >> 24) & 0xff);
    u8CylLow = (uint8_t)((u32Lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (uint8_t)((u32Lba >> 16) & 0xff);
    u8CylHighExp = 0;
    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(ahci_seg, u16IoBase, u8Port);
    ahci_cmd_data(ahci_seg, u16IoBase, u8Cmd, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect,0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, SegData :> OffData, u16Sectors * 512, 1);
}


/**
 * Read data from the device.
 */
static void ahci_cmd_data_in(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Port, uint8_t u8Cmd, 
                             uint32_t u32Lba, uint16_t u16Sectors, uint16_t SegData, uint16_t OffData)
{
    uint8_t     u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    u8SectCount = (uint8_t)(u16Sectors & 0xff);
    u8SectCountExp = (uint8_t)((u16Sectors >> 8) & 0xff);;
    u8Sect = (uint8_t)(u32Lba & 0xff);
    u8SectExp = (uint8_t)((u32Lba >> 24) & 0xff);
    u8CylLow = (uint8_t)((u32Lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (uint8_t)((u32Lba >> 16) & 0xff);
    u8CylHighExp = 0;

    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(ahci_seg, u16IoBase, u8Port);
    ahci_cmd_data(ahci_seg, u16IoBase, u8Cmd, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect, 0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, SegData :> OffData, u16Sectors * 512, 0);
#ifdef DMA_WORKAROUND
    rep_movsw(SegData :> OffData, SegData :> OffData, u16Sectors * 512 / 2);
#endif
}

static void ahci_port_detect_device(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Port)
{
    uint32_t    val;

    ahci_port_init(ahci_seg, u16IoBase, u8Port);

    /* Reset connection. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SCTL, 0x01);
    /*
     * According to the spec we should wait at least 1msec until the reset
     * is cleared but this is a virtual controller so we don't have to.
     */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SCTL, 0);

    /* Check if there is a device on the port. */
    VBOXAHCI_PORT_READ_REG(u16IoBase, u8Port, AHCI_REG_PORT_SSTS, val);
    if (ahci_ctrl_extract_bits(val, 0xfL, 0) == 0x3L)
    {
        uint8_t     idxDevice;

        idxDevice = read_byte(ahci_seg, (uint16_t)&AhciData->cDevices);
        VBOXAHCI_DEBUG("AHCI: Device detected on port %d\n", u8Port);

        if (idxDevice < AHCI_MAX_STORAGE_DEVICES)
        {
            /* Device detected, enable FIS receive. */
            ahci_ctrl_set_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                               AHCI_REG_PORT_CMD_FRE);

            /* Check signature to determine device type. */
            VBOXAHCI_PORT_READ_REG(u16IoBase, u8Port, AHCI_REG_PORT_SIG, val);
            if (val == 0x101L)
            {
                uint8_t     idxHdCurr;
                uint32_t    cSectors;
                uint8_t     abBuffer[0x0200];
                uint8_t     fRemovable;
                uint16_t    cCylinders, cHeads, cSectorsPerTrack;
                uint8_t     cHardDisksOld;
                uint8_t     idxCmosChsBase;

                idxHdCurr = read_byte(ahci_seg, (uint16_t)&AhciData->cHardDisks);
                VBOXAHCI_DEBUG("AHCI: Detected hard disk\n");

                /* Identify device. */
                ahci_cmd_data(ahci_seg, u16IoBase, ATA_CMD_IDENTIFY_DEVICE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &abBuffer, sizeof(abBuffer), 0);

                write_byte(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].port, u8Port);

                fRemovable       = *(abBuffer+0) & 0x80 ? 1 : 0;
                cCylinders       = *(uint16_t *)(abBuffer+(1*2));   // word 1
                cHeads           = *(uint16_t *)(abBuffer+(3*2));   // word 3
                cSectorsPerTrack = *(uint16_t *)(abBuffer+(6*2));   // word 6
                cSectors         = *(uint32_t *)(abBuffer+(60*2));  // word 60 and word 61

                /** @todo update sectors to be a 64 bit number (also lba...). */
                if (cSectors == 268435455)
                    cSectors = *(uint16_t *)(abBuffer+(100*2)); // words 100 to 103 (someday)

                VBOXAHCI_DEBUG("AHCI: %ld sectors\n", cSectors);

                write_byte(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].device,ATA_DEVICE_HD);
                write_byte(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].removable, fRemovable);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].blksize, 512);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.heads, cHeads);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.cylinders, cCylinders);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.spt, cSectorsPerTrack);
                write_dword(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].cSectors, cSectors);

                /* Get logical CHS geometry. */
                switch (idxDevice)
                {
                    case 0:
                        idxCmosChsBase = 0x40;
                        break;
                    case 1:
                        idxCmosChsBase = 0x48;
                        break;
                    case 2:
                        idxCmosChsBase = 0x50;
                        break;
                    case 3:
                        idxCmosChsBase = 0x58;
                        break;
                    default:
                        idxCmosChsBase = 0;
                }
                if (idxCmosChsBase != 0)
                {
                    cCylinders = inb_cmos(idxCmosChsBase) + (inb_cmos(idxCmosChsBase+1) << 8);
                    cHeads = inb_cmos(idxCmosChsBase+2);
                    cSectorsPerTrack = inb_cmos(idxCmosChsBase+7);
                }
                else
                {
                    cCylinders = 0;
                    cHeads = 0;
                    cSectorsPerTrack = 0;
                }
                VBOXAHCI_DEBUG("AHCI: Dev %d LCHS=%d/%d/%d\n", 
                               idxDevice, cCylinders, cHeads, cSectorsPerTrack);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.heads, cHeads);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.cylinders, cCylinders);
                write_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.spt, cSectorsPerTrack);

                write_byte(ahci_seg, (uint16_t)&AhciData->aHdIdMap[idxHdCurr], idxDevice);
                idxHdCurr++;
                write_byte(ahci_seg, (uint16_t)&AhciData->cHardDisks, idxHdCurr);

                /* Update hdcount in the BDA. */
                cHardDisksOld = read_byte(0x40, 0x75);
                cHardDisksOld++;
                write_byte(0x40, 0x75, cHardDisksOld);
            }
            else if (val == 0xeb140101)
            {
                VBOXAHCI_DEBUG("AHCI: Detected ATAPI device\n");
            }
            else
                VBOXAHCI_DEBUG("AHCI: Ignoring unknown device\n");

            idxDevice++;
            write_byte(ahci_seg, (uint16_t)&AhciData->cDevices, idxDevice);
        }
        else
            VBOXAHCI_DEBUG("AHCI: Reached maximum device count, skipping\n");
    }
}

#define SET_DISK_RET_STATUS(status) write_byte(0x0040, 0x0074, status)

/**
 * Int 13 handler.
 */
void BIOSCALL ahci_int13(volatile uint16_t RET, volatile uint16_t ES, volatile uint16_t DS, volatile uint16_t DI, 
                         volatile uint16_t SI, volatile uint16_t BP, volatile uint16_t SP, volatile uint16_t BX,
                         volatile uint16_t DX, volatile uint16_t CX, volatile uint16_t AX, volatile uint16_t IPIRET,
                         volatile uint16_t CSIRET, volatile uint16_t FLAGSIRET, volatile uint16_t IP,
                         volatile uint16_t CS, volatile uint16_t FLAGS)
{
    uint16_t    ebda_seg;
    uint16_t    ahci_seg, u16IoBase;
    uint8_t     idxDevice;
    uint8_t     old_disks;
    uint8_t     u8Port;

    uint32_t    lba;
    uint16_t    cylinder, head, sector;
    uint16_t    segment, offset;
    uint16_t    npc, nph, npspt, nlc, nlh, nlspt;
    uint16_t    count;
//    uint16_t    size;
    uint8_t     status;

    VBOXAHCI_INT13_DEBUG("ahci_int13 AX=%x CX=%x DX=%x BX=%x ES=%x SP=%x BP=%x SI=%x DI=%x IP=%x CS=%x FLAGS=%x\n",
                         AX, CX, DX, BX, ES, SP, BP, SI, DI, IP, CS, FLAGS);

    ebda_seg  = read_word(0x0040, 0x000E);
    ahci_seg  = read_word(ebda_seg, (uint16_t)&EbdaData->ahci_seg);
    u16IoBase = read_word(ahci_seg, (uint16_t)&AhciData->iobase);
    old_disks = read_byte(ahci_seg, (uint16_t)&AhciData->cHardDisksOld);
    VBOXAHCI_INT13_DEBUG("ahci_int13: ahci_seg=%x u16IoBase=%x old_disks=%d\n", ahci_seg, u16IoBase, old_disks);

    /* Check if the device is controlled by us first. */
    if (   (GET_DL() < 0x80)
        || (GET_DL() < 0x80 + old_disks)
        || ((GET_DL() & 0xe0) != 0x80) /* No CD-ROM drives supported for now */)
    {
        VBOXAHCI_INT13_DEBUG("ahci_int13: device not controlled by us, forwarding to old handler (%d)\n", old_disks);
        /* Fill the iret frame to jump to the old handler. */
        IPIRET    = read_word(ahci_seg, (uint16_t)&AhciData->pfnInt13Old);
        CSIRET    = 0xf000;
        FLAGSIRET = FLAGS;
        RET       = 1;
        return;
    }

    //@todo: pre-init aHdIdMap!
    idxDevice = read_byte(ahci_seg, (uint16_t)&AhciData->aHdIdMap[GET_DL() - 0x80 - old_disks]);

    if (idxDevice >= AHCI_MAX_STORAGE_DEVICES)
    {
        VBOXAHCI_INT13_DEBUG("ahci_int13: function %02x, unmapped device for ELDL=%02x\n", GET_AH(), GET_DL());
        goto ahci_int13_fail;
    }

    u8Port = read_byte(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].port);

    switch (GET_AH())
    {
        case 0x00: /* disk controller reset */
        {
            /** @todo: not really important I think. */
            goto ahci_int13_success;
            break;
        }
        case 0x01: /* read disk status */
        {
            status = read_byte(0x0040, 0x0074);
            SET_AH(status);
            SET_DISK_RET_STATUS(0);
            /* set CF if error status read */
            if (status)
                goto ahci_int13_fail_nostatus;
            else
                goto ahci_int13_success_noah;
            break;
        }
        case 0x02: // read disk sectors
        case 0x03: // write disk sectors
        case 0x04: // verify disk sectors
        {
            count       = GET_AL();
            cylinder    = GET_CH();
            cylinder   |= ( ((uint16_t) GET_CL()) << 2) & 0x300;
            sector      = (GET_CL() & 0x3f);
            head        = GET_DH();

            segment = ES;
            offset  = BX;

            if ((count > 128) || (count == 0))
            {
                BX_INFO("ahci_int13: function %02x, count out of range!\n",GET_AH());
                goto ahci_int13_fail;
            }

            nlc   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.cylinders);
            nlh   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.heads);
            nlspt = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.spt);

            // sanity check on cyl heads, sec
            if( (cylinder >= nlc) || (head >= nlh) || (sector > nlspt ))
            {
              BX_INFO("ahci_int13: function %02x, disk %02x, idx %02x, parameters out of range %04x/%04x/%04x!\n",
                      GET_AH(), GET_DL(), idxDevice, cylinder, head, sector);
              goto ahci_int13_fail;
            }

            // FIXME verify
            if ( GET_AH() == 0x04 )
                goto ahci_int13_success;

            lba = ((((uint32_t)cylinder * (uint32_t)nlh) + (uint32_t)head) * (uint32_t)nlspt) + (uint32_t)sector - 1;

            status = 0;
            if ( GET_AH() == 0x02 )
                ahci_cmd_data_in(ahci_seg, u16IoBase, u8Port, AHCI_CMD_READ_DMA_EXT, lba, count, segment, offset);
            else
                ahci_cmd_data_out(ahci_seg, u16IoBase, u8Port, AHCI_CMD_WRITE_DMA_EXT, lba, count, segment, offset);

            // Set nb of sector transferred
            SET_AL(read_word(ebda_seg, (uint16_t)&EbdaData->bdisk.drqp.trsfsectors));

            if (status != 0)
            {
                BX_INFO("ahci_int13: function %02x, error %02x !\n",GET_AH(),status);
                SET_AH(0x0c);
                goto ahci_int13_fail_noah;
            }

            goto ahci_int13_success;
            break;
        }
        case 0x05: /* format disk track */
            BX_INFO("format disk track called\n");
            goto ahci_int13_success;
            break;
        case 0x08: /* read disk drive parameters */
        {
            // Get logical geometry from table
            nlc   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.cylinders);
            nlh   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.heads);
            nlspt = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].lchs.spt);

            count = read_byte(ahci_seg, (uint16_t)&AhciData->cHardDisks); /** @todo correct? */
            /* Maximum cylinder number is just one less than the number of cylinders. */
            nlc = nlc - 1; /* 0 based , last sector not used */
            SET_AL(0);
            SET_CH(nlc & 0xff);
            SET_CL(((nlc >> 2) & 0xc0) | (nlspt & 0x3f));
            SET_DH(nlh - 1);
            SET_DL(count); /* FIXME returns 0, 1, or n hard drives */
            // FIXME should set ES & DI
            goto ahci_int13_success;
            break;
        }
        case 0x10: /* check drive ready */
        {
            /** @todo */
            goto ahci_int13_success;
            break;
        }
        case 0x15: /* read disk drive size */
        {
            // Get physical geometry from table
            npc   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.cylinders);
            nph   = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.heads);
            npspt = read_word(ahci_seg, (uint16_t)&AhciData->aDevices[idxDevice].pchs.spt);

            // Compute sector count seen by int13
            lba = (uint32_t)npc * (uint32_t)nph * (uint32_t)npspt;
            CX = lba >> 16;
            DX = lba & 0xffff;

            SET_AH(3);  // hard disk accessible
            goto ahci_int13_success_noah;
            break;
        }
#if 0
        case 0x41: // IBM/MS installation check
        {
            BX=0xaa55;     // install check
            SET_AH(0x30);  // EDD 3.0
            CX=0x0007;     // ext disk access and edd, removable supported
            goto ahci_int13_success_noah;
            break;
        }
        case 0x42: // IBM/MS extended read
        case 0x43: // IBM/MS extended write
        case 0x44: // IBM/MS verify
        case 0x47: // IBM/MS extended seek
        {
            count=read_word(DS, SI+(uint16_t)&Int13Ext->count);
            segment=read_word(DS, SI+(uint16_t)&Int13Ext->segment);
            offset=read_word(DS, SI+(uint16_t)&Int13Ext->offset);

            // Can't use 64 bits lba
            lba=read_dword(DS, SI+(uint16_t)&Int13Ext->lba2);
            if (lba != 0L)
            {
                BX_PANIC("ahci_int13: function %02x. Can't use 64bits lba\n",GET_AH());
                goto ahci_int13_fail;
            }

            // Get 32 bits lba and check
            lba=read_dword(DS, SI+(uint16_t)&Int13Ext->lba1);

            if (lba >= read_word(ahci_seg, &AhciData->aDevices[idxDevice].cSectors) )
            {
                BX_INFO("ahci_int13: function %02x. LBA out of range\n",GET_AH());
                goto ahci_int13_fail;
            }

            // If verify or seek
            if (( GET_AH() == 0x44 ) || ( GET_AH() == 0x47 ))
                goto ahci_int13_success;

            // Execute the command
            if ( GET_AH() == 0x42 )
            {
                ...
            }
            else
            {
                ...
            }

            count=read_word(ebda_seg, &EbdaData->bdisk.drqp.trsfsectors);
            write_word(DS, SI+(uint16_t)&Int13Ext->count, count);

            if (status != 0)
            {
                BX_INFO("ahci_int13: function %02x, error %02x !\n",GET_AH(),status);
                SET_AH(0x0c);
                goto ahci_int13_fail_noah;
            }
            goto ahci_int13_success;
            break;
        }
        case 0x45: // IBM/MS lock/unlock drive
        case 0x49: // IBM/MS extended media change
            goto ahci_int13_success;    // Always success for HD
            break;
        case 0x46: // IBM/MS eject media
            SET_AH(0xb2);          // Volume Not Removable
            goto ahci_int13_fail_noah;  // Always fail for HD
            break;

        case 0x48: // IBM/MS get drive parameters
            size=read_word(DS,SI+(uint16_t)&Int13DPT->size);

            // Buffer is too small
            if(size < 0x1a)
                goto ahci_int13_fail;

            // EDD 1.x
            if(size >= 0x1a)
            {
                uint16_t   blksize;

                npc     = read_word(ahci_seg, &AhciData->aDevices[idxDevice].pchs.cylinders);
                nph     = read_word(ahci_seg, &AhciData->aDevices[idxDevice].pchs.heads);
                npspt   = read_word(ahci_seg, &AhciData->aDevices[idxDevice].pchs.spt);
                lba     = read_dword(ahci_seg, &AhciData->aDevices[idxDevice].cSectors);
                blksize = read_word(ahci_seg, &AhciData->aDevices[idxDevice].blksize);

                write_word(DS, SI+(uint16_t)&Int13DPT->size, 0x1a);
                write_word(DS, SI+(uint16_t)&Int13DPT->infos, 0x02); // geometry is valid
                write_dword(DS, SI+(uint16_t)&Int13DPT->cylinders, (uint32_t)npc);
                write_dword(DS, SI+(uint16_t)&Int13DPT->heads, (uint32_t)nph);
                write_dword(DS, SI+(uint16_t)&Int13DPT->spt, (uint32_t)npspt);
                write_dword(DS, SI+(uint16_t)&Int13DPT->sector_count1, lba);  // FIXME should be Bit64
                write_dword(DS, SI+(uint16_t)&Int13DPT->sector_count2, 0L);
                write_word(DS, SI+(uint16_t)&Int13DPT->blksize, blksize);
            }

#if 0 /* Disable EDD 2.X and 3.x for now, don't know if it is required by any OS loader yet */
            // EDD 2.x
            if(size >= 0x1e)
            {
                uint8_t     channel, dev, irq, mode, checksum, i, translation;
                uint16_t    iobase1, iobase2, options;

                translation = ATA_TRANSLATION_LBA;

                write_word(DS, SI+(uint16_t)&Int13DPT->size, 0x1e);

                write_word(DS, SI+(uint16_t)&Int13DPT->dpte_segment, ebda_seg);
                write_word(DS, SI+(uint16_t)&Int13DPT->dpte_offset, &EbdaData->bdisk.dpte);

                // Fill in dpte
                channel = device / 2;
                iobase1 = read_word(ebda_seg, &EbdaData->bdisk.channels[channel].iobase1);
                iobase2 = read_word(ebda_seg, &EbdaData->bdisk.channels[channel].iobase2);
                irq = read_byte(ebda_seg, &EbdaData->bdisk.channels[channel].irq);
                mode = read_byte(ebda_seg, &EbdaData->bdisk.devices[device].mode);
                translation = read_byte(ebda_seg, &EbdaData->bdisk.devices[device].translation);

                options  = (translation==ATA_TRANSLATION_NONE?0:1<<3); // chs translation
                options |= (1<<4); // lba translation
                options |= (mode==ATA_MODE_PIO32?1:0<<7);
                options |= (translation==ATA_TRANSLATION_LBA?1:0<<9);
                options |= (translation==ATA_TRANSLATION_RECHS?3:0<<9);

                write_word(ebda_seg, &EbdaData->bdisk.dpte.iobase1, iobase1);
                write_word(ebda_seg, &EbdaData->bdisk.dpte.iobase2, iobase2);
                //write_byte(ebda_seg, &EbdaData->bdisk.dpte.prefix, (0xe | /*(device % 2))<<4*/ );
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.unused, 0xcb );
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.irq, irq );
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.blkcount, 1 );
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.dma, 0 );
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.pio, 0 );
                write_word(ebda_seg, &EbdaData->bdisk.dpte.options, options);
                write_word(ebda_seg, &EbdaData->bdisk.dpte.reserved, 0);
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.revision, 0x11);

                checksum=0;
                for (i=0; i<15; i++)
                    checksum+=read_byte(ebda_seg, (&EbdaData->bdisk.dpte) + i);

                checksum = -checksum;
                write_byte(ebda_seg, &EbdaData->bdisk.dpte.checksum, checksum);
            }

            // EDD 3.x
            if(size >= 0x42)
            {
                uint8_t     channel, iface, checksum, i;
                uint16_t    iobase1;

                channel = device / 2;
                iface = read_byte(ebda_seg, &EbdaData->bdisk.channels[channel].iface);
                iobase1 = read_word(ebda_seg, &EbdaData->bdisk.channels[channel].iobase1);

                write_word(DS, SI+(uint16_t)&Int13DPT->size, 0x42);
                write_word(DS, SI+(uint16_t)&Int13DPT->key, 0xbedd);
                write_byte(DS, SI+(uint16_t)&Int13DPT->dpi_length, 0x24);
                write_byte(DS, SI+(uint16_t)&Int13DPT->reserved1, 0);
                write_word(DS, SI+(uint16_t)&Int13DPT->reserved2, 0);

                if (iface==ATA_IFACE_ISA) {
                  write_byte(DS, SI+(uint16_t)&Int13DPT->host_bus[0], 'I');
                  write_byte(DS, SI+(uint16_t)&Int13DPT->host_bus[1], 'S');
                  write_byte(DS, SI+(uint16_t)&Int13DPT->host_bus[2], 'A');
                  write_byte(DS, SI+(uint16_t)&Int13DPT->host_bus[3], 0);
                  }
                else {
                  // FIXME PCI
                  }
                write_byte(DS, SI+(uint16_t)&Int13DPT->iface_type[0], 'A');
                write_byte(DS, SI+(uint16_t)&Int13DPT->iface_type[1], 'T');
                write_byte(DS, SI+(uint16_t)&Int13DPT->iface_type[2], 'A');
                write_byte(DS, SI+(uint16_t)&Int13DPT->iface_type[3], 0);

                if (iface==ATA_IFACE_ISA) {
                  write_word(DS, SI+(uint16_t)&Int13DPT->iface_path[0], iobase1);
                  write_word(DS, SI+(uint16_t)&Int13DPT->iface_path[2], 0);
                  write_dword(DS, SI+(uint16_t)&Int13DPT->iface_path[4], 0L);
                  }
                else {
                  // FIXME PCI
                  }
                //write_byte(DS, SI+(uint16_t)&Int13DPT->device_path[0], device%2);
                write_byte(DS, SI+(uint16_t)&Int13DPT->device_path[1], 0);
                write_word(DS, SI+(uint16_t)&Int13DPT->device_path[2], 0);
                write_dword(DS, SI+(uint16_t)&Int13DPT->device_path[4], 0L);

                checksum=0;
                for (i=30; i<64; i++) checksum+=read_byte(DS, SI + i);
                checksum = -checksum;
                write_byte(DS, SI+(uint16_t)&Int13DPT->checksum, checksum);
            }
#endif
            goto ahci_int13_success;
            break;
        case 0x4e: // // IBM/MS set hardware configuration
            // DMA, prefetch, PIO maximum not supported
            switch (GET_AL())
            {
                case 0x01:
                case 0x03:
                case 0x04:
                case 0x06:
                    goto ahci_int13_success;
                    break;
                default :
                    goto ahci_int13_fail;
            }
            break;
#endif
        case 0x09: /* initialize drive parameters */
        case 0x0c: /* seek to specified cylinder */
        case 0x0d: /* alternate disk reset */
        case 0x11: /* recalibrate */
        case 0x14: /* controller internal diagnostic */
            BX_INFO("ahci_int13: function %02xh unimplemented, returns success\n", GET_AH());
            goto ahci_int13_success;
            break;

        case 0x0a: /* read disk sectors with ECC */
        case 0x0b: /* write disk sectors with ECC */
        case 0x18: // set media type for format
        case 0x50: // IBM/MS send packet command
        default:
            BX_INFO("ahci_int13: function %02xh unsupported, returns fail\n", GET_AH());
            goto ahci_int13_fail;
            break;
    }

    //@todo: this is really badly written, reuse the code more!
ahci_int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
ahci_int13_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
ahci_int13_fail_nostatus:
    VBOXAHCI_INT13_DEBUG("ahci_int13: done, AH=%02x\n", GET_AH());
    SET_CF();     // error occurred
    return;

ahci_int13_success:
    SET_AH(0x00); // no error
ahci_int13_success_noah:
    SET_DISK_RET_STATUS(0x00);
    VBOXAHCI_INT13_DEBUG("ahci_int13: done, AH=%02x\n", GET_AH());
    CLEAR_CF();   // no error
    return;
}

#undef SET_DISK_RET_STATUS

/* Defined in assembler code. */
extern void ahci_int13_handler(void);
#pragma aux ahci_int13_handler "*";

/**
 * Install the in13 interrupt handler
 * preserving the previous one.
 */
static void ahci_install_int_handler(uint16_t ahci_seg)
{

    uint16_t    pfnInt13Old;

    VBOXAHCI_DEBUG("AHCI: Hooking int 13h vector\n");

    /* Read the old interrupt handler. */
    pfnInt13Old = read_word(0x0000, 0x0013*4);
    write_word(ahci_seg, (uint16_t)&AhciData->pfnInt13Old, pfnInt13Old);

    /* Set our own */
    write_word(0x0000, 0x0013*4, (uint16_t)ahci_int13_handler);
}

/**
 * Allocates 1K from the base memory.
 */
static uint16_t ahci_mem_alloc(void)
{
    uint16_t    base_mem_kb;
    uint16_t    ahci_seg;

    base_mem_kb = read_word(0x00, 0x0413);

    VBOXAHCI_DEBUG("AHCI: %dK of base mem\n", base_mem_kb);

    if (base_mem_kb == 0)
        return 0;

    base_mem_kb--; /* Allocate one block. */
    ahci_seg = (((uint32_t)base_mem_kb * 1024) >> 4); /* Calculate start segment. */

    write_word(0x00, 0x0413, base_mem_kb);

    return ahci_seg;
}

/**
 * Initializes the AHCI HBA and detects attached devices.
 */
static int ahci_hba_init(uint16_t u16IoBase)
{
    uint8_t     i, cPorts;
    uint32_t    val;
    uint16_t    ebda_seg;
    uint16_t    ahci_seg;

    ebda_seg = read_word(0x0040, 0x000E);

    AHCI_READ_REG(u16IoBase, AHCI_REG_VS, val);
    VBOXAHCI_DEBUG("AHCI: Controller has version: 0x%x (major) 0x%x (minor)\n",
                   ahci_ctrl_extract_bits(val, 0xffff0000, 16),
                   ahci_ctrl_extract_bits(val, 0x0000ffff,  0));

    /* Allocate 1K of base memory. */
    ahci_seg = ahci_mem_alloc();
    if (ahci_seg == 0)
    {
        VBOXAHCI_DEBUG("AHCI: Could not allocate 1K of memory, can't boot from controller\n");
        return 0;
    }
    VBOXAHCI_DEBUG("AHCI: ahci_seg=%04x, size=%04x, pointer at EBDA:%04x (EBDA size=%04x)\n", 
                   ahci_seg, sizeof(ahci_t), (uint16_t)&EbdaData->ahci_seg, sizeof(ebda_data_t));

    write_word(ebda_seg, (uint16_t)&EbdaData->ahci_seg, ahci_seg);
    write_byte(ahci_seg, (uint16_t)&AhciData->port, 0xff);
    write_word(ahci_seg, (uint16_t)&AhciData->iobase, u16IoBase);
    write_byte(ahci_seg, (uint16_t)&AhciData->cHardDisksOld, read_byte(0x40, 0x75));

    /* Reset the controller. */
    ahci_ctrl_set_bits(u16IoBase, AHCI_REG_GHC, AHCI_GHC_HR);
    do
    {
        AHCI_READ_REG(u16IoBase, AHCI_REG_GHC, val);
    } while (val & AHCI_GHC_HR != 0);

    AHCI_READ_REG(u16IoBase, AHCI_REG_CAP, val);
    cPorts = ahci_ctrl_extract_bits(val, 0x1f, 0) + 1; /* Extract number of ports.*/

    VBOXAHCI_DEBUG("AHCI: HBA has %u ports\n", cPorts);

    /* Go through the ports. */
    i = 0;
    while (i < 32)
    {
        if (ahci_ctrl_is_bit_set(u16IoBase, AHCI_REG_PI, RT_BIT_32(i)) != 0)
        {
            VBOXAHCI_DEBUG("AHCI: Port %u is present\n", i);
            ahci_port_detect_device(ahci_seg, u16IoBase, i);
            cPorts--;
            if (cPorts == 0)
                break;
        }
        i++;
    }

    if (read_byte(ahci_seg, (uint16_t)&AhciData->cDevices) > 0)
    {
        /*
         * Init completed and there is at least one device present.
         * Install our int13 handler.
         */
        ahci_install_int_handler(ahci_seg);
    }

    return 0;
}

/**
 * Init the AHCI driver and detect attached disks.
 */
void BIOSCALL ahci_init(void)
{
    uint16_t    busdevfn;

    busdevfn = pci_find_classcode(0x00010601);
    if (busdevfn != VBOX_AHCI_NO_DEVICE)
    {
        uint8_t     u8Bus, u8DevFn;
        uint8_t     u8PciCapOff;

        u8Bus = (busdevfn & 0xff00) >> 8;
        u8DevFn = busdevfn & 0x00ff;

        VBOXAHCI_DEBUG("AHCI HBA at Bus %u DevFn 0x%x (raw 0x%x)\n", u8Bus, u8DevFn, busdevfn);

        /* Examine the capability list and search for the Serial ATA Capability Register. */
        u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, PCI_CONFIG_CAP);

        while (u8PciCapOff != 0)
        {
            uint8_t     u8PciCapId = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);

            VBOXAHCI_DEBUG("Capability ID 0x%x at 0x%x\n", u8PciCapId, u8PciCapOff);

            if (u8PciCapId == PCI_CAP_ID_SATACR)
                break;

            /* Go on to the next capability. */
            u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);
        }

        if (u8PciCapOff != 0)
        {
            uint8_t     u8Rev;

            VBOXAHCI_DEBUG("AHCI HBA with SATA Capability register at 0x%x\n", u8PciCapOff);

            /* Advance to the stuff behind the id and next capability pointer. */
            u8PciCapOff += 2;

            u8Rev = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);
            if (u8Rev == 0x10)
            {
                /* Read the SATACR1 register and get the bar and offset of the index/data pair register. */
                uint8_t     u8Bar = 0x00;
                uint16_t    u16Off = 0x00;
                uint16_t    u16BarOff = pci_read_config_word(u8Bus, u8DevFn, u8PciCapOff + 2);

                VBOXAHCI_DEBUG("SATACR1: 0x%x\n", u16BarOff);

                switch (u16BarOff & 0xf)
                {
                    case 0x04:
                        u8Bar = 0x10;
                        break;
                    case 0x05:
                        u8Bar = 0x14;
                        break;
                    case 0x06:
                        u8Bar = 0x18;
                        break;
                    case 0x07:
                        u8Bar = 0x1c;
                        break;
                    case 0x08:
                        u8Bar = 0x20;
                        break;
                    case 0x09:
                        u8Bar = 0x24;
                        break;
                    case 0x0f:
                    default:
                        /* Reserved or unsupported. */
                        VBOXAHCI_DEBUG("BAR 0x%x unsupported\n", u16BarOff & 0xf);
                }

                /* Get the offset inside the BAR from bits 4:15. */
                u16Off = (u16BarOff >> 4) * 4;

                if (u8Bar != 0x00)
                {
                    uint32_t    u32Bar = pci_read_config_dword(u8Bus, u8DevFn, u8Bar);

                    VBOXAHCI_DEBUG("BAR at 0x%x : 0x%x\n", u8Bar, u32Bar);

                    if ((u32Bar & 0x01) != 0)
                    {
                        int         rc;
                        uint16_t    u16AhciIoBase = (u32Bar & 0xfff0) + u16Off;

                        VBOXAHCI_DEBUG("I/O base: 0x%x\n", u16AhciIoBase);
                        rc = ahci_hba_init(u16AhciIoBase);
                    }
                    else
                        VBOXAHCI_DEBUG("BAR is MMIO\n");
                }
            }
            else
                VBOXAHCI_DEBUG("Invalid revision 0x%x\n", u8Rev);
        }
        else
            VBOXAHCI_DEBUG("AHCI HBA with no usable Index/Data register pair!\n");
    }
    else
        VBOXAHCI_DEBUG("No AHCI HBA!\n");
}
