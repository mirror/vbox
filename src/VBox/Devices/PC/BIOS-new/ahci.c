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

//@todo!!!! save/restore high bits of EAX/ECX and whatever else may be needed.

#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "ebda.h"
#include "inlines.h"
#include "pciutil.h"
#include "vds.h"

#define VBOX_AHCI_DEBUG         1

#if VBOX_AHCI_DEBUG
# define VBOXAHCI_DEBUG(...)        BX_INFO(__VA_ARGS__)
#else
# define VBOXAHCI_DEBUG(...)
#endif

/* Number of S/G table entries in EDDS. */
#define NUM_EDDS_SG         16


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
    uint8_t         cur_port;
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

    u8Port = ahci->cur_port;

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

    u8Port = ahci->cur_port;

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

        ahci->cur_port = 0xff;
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

    ahci->cur_port = u8Port;
}

/**
 * Read sectors from an attached AHCI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the 
 *                      EBDA).
 */
int ahci_read_sectors(bio_dsk_t __far *bios_dsk)
{
    uint32_t        lba;
    void __far      *buffer;
    uint16_t        n_sect;
    uint16_t        ahci_seg;
    uint16_t        io_base;
    uint16_t        device_id;
    uint8_t         port;
    uint8_t         u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    device_id = bios_dsk->drqp.dev_id - BX_MAX_ATA_DEVICES - BX_MAX_SCSI_DEVICES;
    if (device_id > BX_MAX_AHCI_DEVICES)
        BX_PANIC("ahci_read_sectors: device_id out of range %d\n", device_id);

    ahci_seg = bios_dsk->ahci_seg;
    io_base  = read_word(ahci_seg, (uint16_t)&AhciData->iobase);
    port     = bios_dsk->ahcidev[device_id].port;

    lba    = bios_dsk->drqp.lba;
    buffer = bios_dsk->drqp.buffer;
    n_sect = bios_dsk->drqp.nsect;

    u8SectCount = (uint8_t)(n_sect & 0xff);
    u8SectCountExp = (uint8_t)((n_sect >> 8) & 0xff);
    u8Sect = (uint8_t)(lba & 0xff);
    u8SectExp = (uint8_t)((lba >> 24) & 0xff);
    u8CylLow = (uint8_t)((lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (uint8_t)((lba >> 16) & 0xff);
    u8CylHighExp = 0;

    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(ahci_seg, io_base, port);
    ahci_cmd_data(ahci_seg, io_base, AHCI_CMD_READ_DMA_EXT, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect, 0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, buffer, n_sect * 512, 0);
#ifdef DMA_WORKAROUND
    rep_movsw(buffer, buffer, n_sect * 512 / 2);
#endif
    return 0;   //@todo!!
}

/**
 * Write sectors to an attached AHCI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the 
 *                      EBDA).
 */
int ahci_write_sectors(bio_dsk_t __far *bios_dsk)
{
    uint32_t        lba;
    void __far      *buffer;
    uint16_t        n_sect;
    uint16_t        ahci_seg;
    uint16_t        io_base;
    uint16_t        device_id;
    uint8_t         port;
    uint8_t         u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    device_id = bios_dsk->drqp.dev_id - BX_MAX_ATA_DEVICES - BX_MAX_SCSI_DEVICES;
    if (device_id > BX_MAX_AHCI_DEVICES)
        BX_PANIC("ahci_write_sectors: device_id out of range %d\n", device_id);

    ahci_seg = bios_dsk->ahci_seg;
    io_base  = read_word(ahci_seg, (uint16_t)&AhciData->iobase);
    port     = bios_dsk->ahcidev[device_id].port;

    lba    = bios_dsk->drqp.lba;
    buffer = bios_dsk->drqp.buffer;
    n_sect = bios_dsk->drqp.nsect;

    u8SectCount = (uint8_t)(n_sect & 0xff);
    u8SectCountExp = (uint8_t)((n_sect >> 8) & 0xff);
    u8Sect = (uint8_t)(lba & 0xff);
    u8SectExp = (uint8_t)((lba >> 24) & 0xff);
    u8CylLow = (uint8_t)((lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (uint8_t)((lba >> 16) & 0xff);
    u8CylHighExp = 0;

    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(ahci_seg, io_base, port);
    ahci_cmd_data(ahci_seg, io_base, AHCI_CMD_WRITE_DMA_EXT, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect, 0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, buffer, n_sect * 512, 0);
    return 0;   //@todo!!
}

static void ahci_port_detect_device(uint16_t ahci_seg, uint16_t u16IoBase, uint8_t u8Port)
{
    uint32_t            val;
    bio_dsk_t __far     *bios_dsk;

    ahci_port_init(ahci_seg, u16IoBase, u8Port);

    bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;

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
        uint8_t     hdcount, hdcount_ahci, hd_index;

        hdcount_ahci = bios_dsk->ahci_hdcount;

        VBOXAHCI_DEBUG("AHCI: Device detected on port %d\n", u8Port);

        if (hdcount_ahci < BX_MAX_AHCI_DEVICES)
        {
            /* Device detected, enable FIS receive. */
            ahci_ctrl_set_bits(u16IoBase, AHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                               AHCI_REG_PORT_CMD_FRE);

            /* Check signature to determine device type. */
            VBOXAHCI_PORT_READ_REG(u16IoBase, u8Port, AHCI_REG_PORT_SIG, val);
            if (val == 0x101L)
            {
                uint32_t    cSectors;
                uint8_t     abBuffer[0x0200];
                uint8_t     fRemovable;
                uint16_t    cCylinders, cHeads, cSectorsPerTrack;
                uint8_t     idxCmosChsBase;

                VBOXAHCI_DEBUG("AHCI: Detected hard disk\n");

                /* Identify device. */
                ahci_cmd_data(ahci_seg, u16IoBase, ATA_CMD_IDENTIFY_DEVICE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &abBuffer, sizeof(abBuffer), 0);

                /* Calculate index into the generic disk table. */
                hd_index = hdcount_ahci + BX_MAX_ATA_DEVICES + BX_MAX_SCSI_DEVICES;

                fRemovable       = *(abBuffer+0) & 0x80 ? 1 : 0;
                cCylinders       = *(uint16_t *)(abBuffer+(1*2));   // word 1
                cHeads           = *(uint16_t *)(abBuffer+(3*2));   // word 3
                cSectorsPerTrack = *(uint16_t *)(abBuffer+(6*2));   // word 6
                cSectors         = *(uint32_t *)(abBuffer+(60*2));  // word 60 and word 61

                /** @todo update sectors to be a 64 bit number (also lba...). */
                if (cSectors == 268435455)
                    cSectors = *(uint16_t *)(abBuffer+(100*2)); // words 100 to 103 (someday)

                VBOXAHCI_DEBUG("AHCI: %ld sectors\n", cSectors);

                bios_dsk->ahcidev[hdcount_ahci].port = u8Port;
                bios_dsk->devices[hd_index].type        = ATA_TYPE_AHCI;
                bios_dsk->devices[hd_index].device      = ATA_DEVICE_HD;
                bios_dsk->devices[hd_index].removable   = fRemovable;
                bios_dsk->devices[hd_index].lock        = 0;
                bios_dsk->devices[hd_index].blksize     = 512;
                bios_dsk->devices[hd_index].translation = ATA_TRANSLATION_LBA;
                bios_dsk->devices[hd_index].sectors     = cSectors;

                bios_dsk->devices[hd_index].pchs.heads     = cHeads;
                bios_dsk->devices[hd_index].pchs.cylinders = cCylinders;
                bios_dsk->devices[hd_index].pchs.spt       = cSectorsPerTrack;

                /* Get logical CHS geometry. */
                switch (hdcount_ahci)
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
                               hdcount_ahci, cCylinders, cHeads, cSectorsPerTrack);

                bios_dsk->devices[hd_index].lchs.heads     = cHeads;
                bios_dsk->devices[hd_index].lchs.cylinders = cCylinders;
                bios_dsk->devices[hd_index].lchs.spt       = cSectorsPerTrack;

                /* Store the id of the disk in the ata hdidmap. */
                hdcount = bios_dsk->hdcount;
                bios_dsk->hdidmap[hdcount] = hdcount_ahci + BX_MAX_ATA_DEVICES + BX_MAX_SCSI_DEVICES;
                hdcount++;
                bios_dsk->hdcount = hdcount;

                /* Update hdcount in the BDA. */
                hdcount = read_byte(0x40, 0x75);
                hdcount++;
                write_byte(0x40, 0x75, hdcount);
                
            }
            else if (val == 0xeb140101)
            {
                VBOXAHCI_DEBUG("AHCI: Detected ATAPI device\n");
            }
            else
                VBOXAHCI_DEBUG("AHCI: Ignoring unknown device\n");

            hdcount_ahci++;
            bios_dsk->ahci_hdcount = hdcount_ahci;
        }
        else
            VBOXAHCI_DEBUG("AHCI: Reached maximum device count, skipping\n");
    }
}

/**
 * Allocates 1K of conventional memory.
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
                   ahci_seg, sizeof(ahci_t), (uint16_t)&EbdaData->bdisk.ahci_seg, sizeof(ebda_data_t));

    write_word(ebda_seg, (uint16_t)&EbdaData->bdisk.ahci_seg, ahci_seg);
    write_byte(ebda_seg, (uint16_t)&EbdaData->bdisk.ahci_hdcount, 0);
    write_byte(ahci_seg, (uint16_t)&AhciData->cur_port, 0xff);
    write_word(ahci_seg, (uint16_t)&AhciData->iobase, u16IoBase);

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
