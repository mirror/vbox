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

#define AHCI_MAX_STORAGE_DEVICES 4

/**
 * AHCI device data.
 */
typedef struct
{
    Bit8u  type;         // Detected type of ata (ata/atapi/none/unknown/scsi)
    Bit8u  device;       // Detected type of attached devices (hd/cd/none)
    Bit8u  removable;    // Removable device flag
    Bit8u  lock;         // Locks for removable devices
    Bit16u blksize;      // block size
    chs_t  lchs;         // Logical CHS
    chs_t  pchs;         // Physical CHS
    Bit32u cSectors;     // Total sectors count
    Bit8u  port;         // Port this device is on.
} ahci_device_t;

/**
 * AHCI controller data.
 */
typedef struct
{
    /** The AHCI command list as defined by chapter 4.2.2 of the Intel AHCI spec.
     *  Because the BIOS doesn't support NCQ only the first command header is defined
     *  to save memory. - Must be aligned on a 1K boundary.
     */
    Bit32u        abCmdHdr[0x8];
    /** Align the next structure on a 128 byte boundary. */
    Bit8u         abAlignment1[0x60];
    /** The command table of one request as defined by chapter 4.2.3 of the Intel AHCI spec.
     *  Must be aligned on 128 byte boundary.
     */
    Bit8u         abCmd[0x90];
    /** Alignment */
    Bit8u         abAlignment2[0xF0];
    /** Memory for the received command FIS area as specified by chapter 4.2.1
     *  of the Intel AHCI spec. This area is normally 256 bytes big but to save memory
     *  only the first 96 bytes are used because it is assumed that the controller
     *  never writes to the UFIS or reserved area. - Must be aligned on a 256byte boundary.
     */
    Bit8u         abFisRecv[0x60];
    /** Base I/O port for the index/data register pair. */
    Bit16u        iobase;
    /** Current port which uses the memory to communicate with the controller. */
    Bit8u         port;
    /** AHCI device information. */
    ahci_device_t aDevices[AHCI_MAX_STORAGE_DEVICES];
    /** Index of the next unoccupied device slot. */
    Bit8u         cDevices;
    /** Map between (bios hd id - 0x80) and ahci devices. */
    Bit8u         cHardDisks;
    Bit8u         aHdIdMap[AHCI_MAX_STORAGE_DEVICES];
    /** Map between (bios cd id - 0xE0) and ahci_devices. */
    Bit8u         cCdDrives;
    Bit8u         aCdIdMap[AHCI_MAX_STORAGE_DEVICES];
    /** int13 handler to call if given device is not from AHCI. */
    Bit16u        pfnInt13Old;
    /** Number of harddisks detected before the AHCI driver started detection. */
    Bit8u         cHardDisksOld;
} ahci_t;

#define AhciData ((ahci_t *) 0)

/** Supported methods of the PCI BIOS. */
#define PCIBIOS_ID                      0xb1
#define PCIBIOS_PCI_BIOS_PRESENT        0x01
#define PCIBIOS_FIND_PCI_DEVICE         0x02
#define PCIBIOS_FIND_CLASS_CODE         0x03
#define PCIBIOS_GENERATE_SPECIAL_CYCLE  0x06
#define PCIBIOS_READ_CONFIG_BYTE        0x08
#define PCIBIOS_READ_CONFIG_WORD        0x09
#define PCIBIOS_READ_CONFIG_DWORD       0x0a
#define PCIBIOS_WRITE_CONFIG_BYTE       0x0b
#define PCIBIOS_WRITE_CONFIG_WORD       0x0c
#define PCIBIOS_WRITE_CONFIG_DWORD      0x0d
#define PCIBIOS_GET_IRQ_ROUTING_OPTIONS 0x0e
#define PCIBIOS_SET_PCI_IRQ             0x0f

/** Status codes. */
#define SUCCESSFUL                      0x00
#define FUNC_NOT_SUPPORTED              0x81
#define BAD_VENDOR_ID                   0x83
#define DEVICE_NOT_FOUND                0x86
#define BAD_REGISTER_NUMBER             0x87
#define SET_FAILED                      0x88
#define BUFFER_TOO_SMALL                0x89

/** PCI configuration fields. */
#define PCI_CONFIG_CAP                  0x34

#define PCI_CAP_ID_SATACR               0x12
#define VBOX_AHCI_NO_DEVICE 0xffff

//#define VBOX_AHCI_DEBUG 1
//#define VBOX_AHCI_INT13_DEBUG 1

#ifdef VBOX_AHCI_DEBUG
# define VBOXAHCI_DEBUG(a...) BX_INFO(a)
#else
# define VBOXAHCI_DEBUG(a...)
#endif

#ifdef VBOX_AHCI_INT13_DEBUG
# define VBOXAHCI_INT13_DEBUG(a...) BX_INFO(a)
#else
# define VBOXAHCI_INT13_DEBUG(a...)
#endif

#define RT_BIT_32(bit) ((Bit32u)(1L << (bit)))

/** Global register set. */
#define AHCI_HBA_SIZE 0x100

#define AHCI_REG_CAP ((Bit32u)0x00)
#define AHCI_REG_GHC ((Bit32u)0x04)
# define AHCI_GHC_AE RT_BIT_32(31)
# define AHCI_GHC_IR RT_BIT_32(1)
# define AHCI_GHC_HR RT_BIT_32(0)
#define AHCI_REG_IS  ((Bit32u)0x08)
#define AHCI_REG_PI  ((Bit32u)0x0c)
#define AHCI_REG_VS  ((Bit32u)0x10)

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
#define VBOXAHCI_PORT_REG(port, reg) ((Bit32u)(AHCI_HBA_SIZE + (port) * AHCI_PORT_SIZE + (reg)))

#define VBOXAHCI_REG_IDX   0
#define VBOXAHCI_REG_DATA  4

/** Writes the given value to a AHCI register. */
#define VBOXAHCI_WRITE_REG(iobase, reg, val) \
    outl((iobase) + VBOXAHCI_REG_IDX, (Bit32u)(reg)); \
    outl((iobase) + VBOXAHCI_REG_DATA, (Bit32u)(val))

/** Reads from a AHCI register. */
#define VBOXAHCI_READ_REG(iobase, reg, val) \
    outl((iobase) + VBOXAHCI_REG_IDX, (Bit32u)(reg)); \
    (val) = inl((iobase) + VBOXAHCI_REG_DATA)

/** Writes to the given port register. */
#define VBOXAHCI_PORT_WRITE_REG(iobase, port, reg, val) \
    VBOXAHCI_WRITE_REG((iobase), VBOXAHCI_PORT_REG((port), (reg)), val)

/** Reads from the given port register. */
#define VBOXAHCI_PORT_READ_REG(iobase, port, reg, val) \
    VBOXAHCI_READ_REG((iobase), VBOXAHCI_PORT_REG((port), (reg)), val)

#define ATA_CMD_IDENTIFY_DEVICE 0xEC
#define AHCI_CMD_READ_DMA_EXT 0x25
#define AHCI_CMD_WRITE_DMA_EXT 0x35

/**
 * Returns the bus/device/function of a PCI device with
 * the given classcode.
 *
 * @returns bus/device/fn in one 16bit integer where
 *          where the upper byte contains the bus number
 *          and lower one the device and function number.
 *          VBOX_AHCI_NO_DEVICE if no device was found.
 * @param   u16Class    The classcode to search for.
 */
Bit16u ahci_pci_find_classcode(u16Class)
    Bit32u u16Class;
{
    Bit16u u16BusDevFn;

ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_FIND_CLASS_CODE
    mov ecx, _ahci_pci_find_classcode.u16Class + 2[bp]
    mov si, #0                                    ; First controller
    int 0x1a

    ; Return from PCIBIOS
    cmp ah, #SUCCESSFUL
    jne ahci_pci_find_classcode_not_found

    mov _ahci_pci_find_classcode.u16BusDevFn + 2[bp], bx
    jmp ahci_pci_find_classcode_done

ahci_pci_find_classcode_not_found:
    mov _ahci_pci_find_classcode.u16BusDevFn + 2[bp], #VBOX_AHCI_NO_DEVICE

ahci_pci_find_classcode_done:
    pop bp
ASM_END

    return u16BusDevFn;
}

Bit8u ahci_pci_read_config_byte(u8Bus, u8DevFn, u8Reg)
    Bit8u u8Bus, u8DevFn, u8Reg;
{
    Bit8u u8Val;

    u8Val = 0;

ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_READ_CONFIG_BYTE
    mov bh, _ahci_pci_read_config_byte.u8Bus + 2[bp]
    mov bl, _ahci_pci_read_config_byte.u8DevFn + 2[bp]
    mov di, _ahci_pci_read_config_byte.u8Reg + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    cmp ah, #SUCCESSFUL
    jne ahci_pci_read_config_byte_done

    mov _ahci_pci_read_config_byte.u8Val + 2[bp], cl

ahci_pci_read_config_byte_done:
    pop bp
ASM_END

    return u8Val;
}

Bit16u ahci_pci_read_config_word(u8Bus, u8DevFn, u8Reg)
    Bit8u u8Bus, u8DevFn, u8Reg;
{
    Bit16u u16Val;

    u16Val = 0;

ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_READ_CONFIG_WORD
    mov bh, _ahci_pci_read_config_word.u8Bus + 2[bp]
    mov bl, _ahci_pci_read_config_word.u8DevFn + 2[bp]
    mov di, _ahci_pci_read_config_word.u8Reg + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    cmp ah, #SUCCESSFUL
    jne ahci_pci_read_config_word_done

    mov _ahci_pci_read_config_word.u16Val + 2[bp], cx

ahci_pci_read_config_word_done:
    pop bp
ASM_END

    return u16Val;
}

Bit32u ahci_pci_read_config_dword(u8Bus, u8DevFn, u8Reg)
    Bit8u u8Bus, u8DevFn, u8Reg;
{
    Bit32u u32Val;

    u32Val = 0;

ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_READ_CONFIG_DWORD
    mov bh, _ahci_pci_read_config_dword.u8Bus + 2[bp]
    mov bl, _ahci_pci_read_config_dword.u8DevFn + 2[bp]
    mov di, _ahci_pci_read_config_dword.u8Reg + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    cmp ah, #SUCCESSFUL
    jne ahci_pci_read_config_dword_done

    mov _ahci_pci_read_config_dword.u32Val + 2[bp], ecx

ahci_pci_read_config_dword_done:
    pop bp
ASM_END

    return u32Val;
}

#if 0 /* Disabled to save space because they are not needed. Might become useful in the future. */
/**
 * Returns the bus/device/function of a PCI device with
 * the given vendor and device id.
 *
 * @returns bus/device/fn in one 16bit integer where
 *          where the upper byte contains the bus number
 *          and lower one the device and function number.
 *          VBOX_AHCI_NO_DEVICE if no device was found.
 * @param   u16Vendor    The vendor ID.
 * @param   u16Device    The device ID.
 */
Bit16u ahci_pci_find_device(u16Vendor, u16Device)
    Bit16u u16Vendor;
    Bit16u u16Device;
{
    Bit16u u16BusDevFn;

ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_FIND_PCI_DEVICE
    mov cx, _ahci_pci_find_device.u16Device + 2[bp]
    mov dx, _ahci_pci_find_device.u16Vendor + 2[bp]
    mov si, #0                                    ; First controller
    int 0x1a

    ; Return from PCIBIOS
    cmp ah, #SUCCESSFUL
    jne ahci_pci_find_device_not_found

    mov _ahci_pci_find_device.u16BusDevFn + 2[bp], bx
    jmp ahci_pci_find_device_done

ahci_pci_find_device_not_found:
    mov _ahci_pci_find_device.u16BusDevFn + 2[bp], #VBOX_AHCI_NO_DEVICE

ahci_pci_find_device_done:
    pop bp
ASM_END

    return u16BusDevFn;
}

void ahci_pci_write_config_byte(u8Bus, u8DevFn, u8Reg, u8Val)
    Bit8u u8Bus, u8DevFn, u8Reg, u8Val;
{
ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_WRITE_CONFIG_BYTE
    mov bh, _ahci_pci_write_config_byte.u8Bus + 2[bp]
    mov bl, _ahci_pci_write_config_byte.u8DevFn + 2[bp]
    mov di, _ahci_pci_write_config_byte.u8Reg + 2[bp]
    mov cl, _ahci_pci_write_config_byte.u8Val + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    pop bp
ASM_END
}

void ahci_pci_write_config_word(u8Bus, u8DevFn, u8Reg, u16Val)
    Bit8u u8Bus, u8DevFn, u8Reg;
    Bit16u u16Val;
{
ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_WRITE_CONFIG_WORD
    mov bh, _ahci_pci_write_config_word.u8Bus + 2[bp]
    mov bl, _ahci_pci_write_config_word.u8DevFn + 2[bp]
    mov di, _ahci_pci_write_config_word.u8Reg + 2[bp]
    mov cx, _ahci_pci_write_config_word.u16Val + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    pop bp
ASM_END
}

void ahci_pci_write_config_dword(u8Bus, u8DevFn, u8Reg, u32Val)
    Bit8u u8Bus, u8DevFn, u8Reg;
    Bit32u u32Val;
{
ASM_START
    push bp
    mov  bp, sp

    mov ah, #PCIBIOS_ID
    mov al, #PCIBIOS_WRITE_CONFIG_WORD
    mov bh, _ahci_pci_write_config_dword.u8Bus + 2[bp]
    mov bl, _ahci_pci_write_config_dword.u8DevFn + 2[bp]
    mov di, _ahci_pci_write_config_dword.u8Reg + 2[bp]
    mov cx, _ahci_pci_write_config_dword.u32Val + 2[bp]
    int 0x1a

    ; Return from PCIBIOS
    pop bp
ASM_END
}
#endif /* 0 */

/**
 * Sets a given set of bits in a register.
 */
static void ahci_ctrl_set_bits(u16IoBase, u32Reg, u32Set)
    Bit16u u16IoBase;
    Bit32u u32Reg, u32Set;
{
ASM_START
    push bp
    mov bp, sp

    push eax
    push edx

    ; Read from the register
    mov eax, _ahci_ctrl_set_bits.u32Reg + 2[bp]
    mov dx, _ahci_ctrl_set_bits.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_IDX
    out dx, eax

    mov dx, _ahci_ctrl_set_bits.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_DATA
    in eax, dx

    ; Set the new bits and write the result to the register again
    or eax, _ahci_ctrl_set_bits.u32Set + 2[bp]
    out dx, eax

    pop edx
    pop eax

    pop bp
ASM_END
}

/**
 * Clears a given set of bits in a register.
 */
static void ahci_ctrl_clear_bits(u16IoBase, u32Reg, u32Clear)
    Bit16u u16IoBase;
    Bit32u u32Reg, u32Clear;
{
ASM_START
    push bp
    mov bp, sp

    push eax
    push ecx
    push edx

    ; Read from the register
    mov eax, _ahci_ctrl_clear_bits.u32Reg + 2[bp]
    mov dx, _ahci_ctrl_clear_bits.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_IDX
    out dx, eax

    mov dx, _ahci_ctrl_clear_bits.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_DATA
    in eax, dx

    ; Clear the bits and write the result to the register again
    mov ecx, _ahci_ctrl_clear_bits.u32Clear + 2[bp]
    not ecx
    and eax, ecx
    out dx, eax

    pop edx
    pop ecx
    pop eax

    pop bp
ASM_END
}

/**
 * Returns whether at least one of the bits in the given mask is set
 * for a register.
 */
static Bit8u ahci_ctrl_is_bit_set(u16IoBase, u32Reg, u32Mask)
    Bit16u u16IoBase;
    Bit32u u32Reg, u32Mask;
{
ASM_START
    push bp
    mov bp, sp

    push edx
    push eax

    ; Read from the register
    mov eax, _ahci_ctrl_is_bit_set.u32Reg + 2[bp]
    mov dx, _ahci_ctrl_is_bit_set.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_IDX
    out dx, eax

    mov dx, _ahci_ctrl_is_bit_set.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_DATA
    in eax, dx

    ; Check for set bits
    mov edx, _ahci_ctrl_is_bit_set.u32Mask + 2[bp]
    test eax, edx
    je ahci_ctrl_is_bit_set_not_set
    mov al, #1                                       ; At least one of the bits is set
    jmp ahci_ctrl_is_bit_set_done

ahci_ctrl_is_bit_set_not_set:
    mov al, #0                                       ; No bit is set

ahci_ctrl_is_bit_set_done:
    pop edx                                          ; restore upper 16 bits of eax
    and edx, #0xffff0000
    or  eax, edx
    pop edx

    pop bp
ASM_END
}

/**
 * Extracts a range of bits from a register and shifts them
 * to the right.
 */
static Bit16u ahci_ctrl_extract_bits(u32Reg, u32Mask, u8Shift)
    Bit32u u32Reg, u32Mask, u8Shift;
{
ASM_START
    push bp
    mov bp, sp

    push cx

    mov eax, _ahci_ctrl_extract_bits.u32Reg  + 2[bp]
    mov cl,  _ahci_ctrl_extract_bits.u8Shift + 2[bp]
    and eax, _ahci_ctrl_extract_bits.u32Mask + 2[bp]
    shr eax, cl

    pop cx

    pop bp
ASM_END
}

/**
 * Converts a segment:offset pair into a 32bit physical address.
 */
static Bit32u ahci_addr_to_phys(u16Segment, u16Offset)
    Bit16u u16Segment, u16Offset;
{
ASM_START
    push bp
    mov bp, sp

    push bx
    push eax

    xor eax, eax
    xor ebx, ebx
    mov ax, _ahci_addr_to_phys.u16Segment + 2[bp]
    shl eax, #4
    add bx, _ahci_addr_to_phys.u16Offset + 2[bp]
    add eax, ebx

    mov bx, ax
    shr eax, #16
    mov dx, ax

    pop eax
    mov ax, bx
    pop bx

    pop bp
ASM_END
}

/**
 * Issues a command to the SATA controller and waits for completion.
 */
static void ahci_port_cmd_sync(SegAhci, u16IoBase, fWrite, fAtapi, cFisDWords, cbData)
    Bit16u SegAhci;
    Bit16u u16IoBase;
    bx_bool fWrite;
    bx_bool fAtapi;
    Bit8u   cFisDWords;
    Bit16u  cbData;
{
    Bit8u u8Port;

    u8Port = read_byte(SegAhci, &AhciData->port);

    if (u8Port != 0xff)
    {
        Bit32u u32Val;

        /* Prepare the command header. */
        u32Val = (1L << 16) | RT_BIT_32(7);
        if (fWrite)
            u32Val |= RT_BIT_32(6);

        if (fAtapi)
            u32Val |= RT_BIT_32(5);

        u32Val |= cFisDWords;

        write_dword(SegAhci, &AhciData->abCmdHdr[0], u32Val);
        write_dword(SegAhci, &AhciData->abCmdHdr[1], (Bit32u)cbData);
        write_dword(SegAhci, &AhciData->abCmdHdr[2],
                    ahci_addr_to_phys(SegAhci, &AhciData->abCmd[0]));

        /* Enable Command and FIS receive engine. */
        ahci_ctrl_set_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                           AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

        /* Queue command. */
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CI, 0x1L);

        /* Wait for a D2H Fis. */
        while (ahci_ctrl_is_bit_set(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_IS),
                                    AHCI_REG_PORT_IS_DHRS) == 0)
        {
            VBOXAHCI_DEBUG("AHCI: Waiting for a D2H Fis\n");
        }

        ahci_ctrl_set_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_IS),
                           AHCI_REG_PORT_IS_DHRS); /* Acknowledge received D2H FIS. */

        /* Disable command engine. */
        ahci_ctrl_clear_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                             AHCI_REG_PORT_CMD_ST);

        /** @todo: Examine status. */
    }
    else
        VBOXAHCI_DEBUG("AHCI: Invalid port given\n");
}

/**
 * Issue command to device.
 */
static void ahci_cmd_data(SegAhci, u16IoBase, u8Cmd, u8Feat, u8Device, u8CylHigh, u8CylLow, u8Sect,
                          u8FeatExp, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp,
                          SegData, OffData, cbData, fWrite)
    Bit16u SegAhci;
    Bit16u u16IoBase;
    Bit8u  u8Cmd, u8Feat, u8Device, u8CylHigh, u8CylLow, u8Sect, u8FeatExp,
           u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;
    Bit16u SegData, OffData, cbData;
    bx_bool fWrite;
{
    memsetb(SegAhci, &AhciData->abCmd[0], 0, sizeof(AhciData->abCmd));

    /* Prepare the FIS. */
    write_byte(SegAhci, &AhciData->abCmd[0], 0x27);   /* FIS type H2D. */
    write_byte(SegAhci, &AhciData->abCmd[1], 1 << 7); /* Command update. */
    write_byte(SegAhci, &AhciData->abCmd[2], u8Cmd);
    write_byte(SegAhci, &AhciData->abCmd[3], u8Feat);

    write_byte(SegAhci, &AhciData->abCmd[4], u8Sect);
    write_byte(SegAhci, &AhciData->abCmd[5], u8CylLow);
    write_byte(SegAhci, &AhciData->abCmd[6], u8CylHigh);
    write_byte(SegAhci, &AhciData->abCmd[7], u8Device);

    write_byte(SegAhci, &AhciData->abCmd[8], u8SectExp);
    write_byte(SegAhci, &AhciData->abCmd[9], u8CylLowExp);
    write_byte(SegAhci, &AhciData->abCmd[10], u8CylHighExp);
    write_byte(SegAhci, &AhciData->abCmd[11], u8FeatExp);

    write_byte(SegAhci, &AhciData->abCmd[12], u8SectCount);
    write_byte(SegAhci, &AhciData->abCmd[13], u8SectCountExp);

    /* Prepare PRDT. */
    write_dword(SegAhci, &AhciData->abCmd[0x80], ahci_addr_to_phys(SegData, OffData));
    write_dword(SegAhci, &AhciData->abCmd[0x8c], (Bit32u)(cbData - 1));

    ahci_port_cmd_sync(SegAhci, u16IoBase, fWrite, 0, 5, cbData);
}

/**
 * Deinits the curent active port.
 */
static void ahci_port_deinit_current(SegAhci, u16IoBase)
    Bit16u SegAhci;
    Bit16u u16IoBase;
{
    Bit8u u8Port;

    u8Port = read_byte(SegAhci, &AhciData->port);

    if (u8Port != 0xff)
    {
        /* Put the port into an idle state. */
        ahci_ctrl_clear_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                             AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

        while (ahci_ctrl_is_bit_set(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                                    AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST | AHCI_REG_PORT_CMD_FR | AHCI_REG_PORT_CMD_CR) == 1)
        {
            VBOXAHCI_DEBUG("AHCI: Waiting for the port to idle\n");
        }

        /*
         * Port idles, set up memory for commands and received FIS and program the
         * address registers.
         */
        memsetb(SegAhci, &AhciData->abFisRecv[0], 0, 0x60);
        memsetb(SegAhci, &AhciData->abCmdHdr[0], 0, 0x20);
        memsetb(SegAhci, &AhciData->abCmd[0], 0, 0x84);

        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FB, 0L);
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FBU, 0L);

        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLB, 0L);
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLBU, 0L);

        /* Disable all interrupts. */
        VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IE, 0L);

        write_byte(SegAhci, &AhciData->port, 0xff);
    }
}

/**
 * Brings a port into a minimal state to make device detection possible
 * or to queue requests.
 */
static void ahci_port_init(SegAhci, u16IoBase, u8Port)
    Bit16u SegAhci;
    Bit16u u16IoBase;
    Bit8u u8Port;
{
    Bit32u u32PhysAddr;

    /* Deinit any other port first. */
    ahci_port_deinit_current(SegAhci, u16IoBase);

    /* Put the port into an idle state. */
    ahci_ctrl_clear_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                         AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

    while (ahci_ctrl_is_bit_set(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                                AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST | AHCI_REG_PORT_CMD_FR | AHCI_REG_PORT_CMD_CR) == 1)
    {
        VBOXAHCI_DEBUG("AHCI: Waiting for the port to idle\n");
    }

    /*
     * Port idles, set up memory for commands and received FIS and program the
     * address registers.
     */
    memsetb(SegAhci, &AhciData->abFisRecv[0], 0, 0x60);
    memsetb(SegAhci, &AhciData->abCmdHdr[0], 0, 0x20);
    memsetb(SegAhci, &AhciData->abCmd[0], 0, 0x84);

    u32PhysAddr = ahci_addr_to_phys(SegAhci, &AhciData->abFisRecv);
    VBOXAHCI_DEBUG("AHCI: FIS receive area %lx from %x:%x\n", u32PhysAddr, SegAhci, &AhciData->abFisRecv);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FB, u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_FBU, 0L);

    u32PhysAddr = ahci_addr_to_phys(SegAhci, &AhciData->abCmdHdr);
    VBOXAHCI_DEBUG("AHCI: CMD list area %lx\n", u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLB, u32PhysAddr);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_CLBU, 0L);

    /* Disable all interrupts. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IE, 0L);
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_IS, 0xffffffffL);
    /* Clear all errors. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SERR, 0xffffffffL);

    write_byte(SegAhci, &AhciData->port, u8Port);
}

/**
 * Write data to the device.
 */
static void ahci_cmd_data_out(SegAhci, u16IoBase, u8Port, u8Cmd, u32Lba, u16Sectors, SegData, OffData)
    Bit16u SegAhci, u16IoBase;
    Bit8u  u8Port, u8Cmd;
    Bit32u u32Lba;
    Bit16u u16Sectors;
    Bit16u SegData, OffData;
{
    Bit8u u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    u8SectCount = (Bit8u)(u16Sectors & 0xff);
    u8SectCountExp = (Bit8u)((u16Sectors >> 8) & 0xff);;
    u8Sect = (Bit8u)(u32Lba & 0xff);
    u8SectExp = (Bit8u)((u32Lba >> 24) & 0xff);
    u8CylLow = (Bit8u)((u32Lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (Bit8u)((u32Lba >> 16) & 0xff);
    u8CylHighExp = 0;
    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(SegAhci, u16IoBase, u8Port);
    ahci_cmd_data(SegAhci, u16IoBase, u8Cmd, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect,0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, SegData, OffData, u16Sectors * 512, 1);
}

/**
 * Read data from the device.
 */
static void ahci_cmd_data_in(SegAhci, u16IoBase, u8Port, u8Cmd, u32Lba, u16Sectors, SegData, OffData)
    Bit16u SegAhci, u16IoBase;
    Bit8u  u8Port, u8Cmd;
    Bit32u u32Lba;
    Bit16u u16Sectors;
    Bit16u SegData, OffData;
{
    Bit8u u8CylLow, u8CylHigh, u8Device, u8Sect, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount, u8SectCountExp;

    u8SectCount = (Bit8u)(u16Sectors & 0xff);
    u8SectCountExp = (Bit8u)((u16Sectors >> 8) & 0xff);;
    u8Sect = (Bit8u)(u32Lba & 0xff);
    u8SectExp = (Bit8u)((u32Lba >> 24) & 0xff);
    u8CylLow = (Bit8u)((u32Lba >> 8) & 0xff);
    u8CylLowExp = 0;
    u8CylHigh = (Bit8u)((u32Lba >> 16) & 0xff);
    u8CylHighExp = 0;

    u8Device = (1 << 6); /* LBA access */

    ahci_port_init(SegAhci, u16IoBase, u8Port);
    ahci_cmd_data(SegAhci, u16IoBase, u8Cmd, 0, u8Device, u8CylHigh, u8CylLow,
                  u8Sect, 0, u8CylHighExp, u8CylLowExp, u8SectExp, u8SectCount,
                  u8SectCountExp, SegData, OffData, u16Sectors * 512, 0);
}

static void ahci_port_detect_device(SegAhci, u16IoBase, u8Port)
    Bit16u SegAhci;
    Bit16u u16IoBase;
    Bit8u u8Port;
{
    Bit32u val;

    ahci_port_init(SegAhci, u16IoBase, u8Port);

    /* Reset connection. */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SCTL, 0x01L);
    /*
     * According to the spec we should wait at least 1msec until the reset
     * is cleared but this is a virtual controller so we don't have to.
     */
    VBOXAHCI_PORT_WRITE_REG(u16IoBase, u8Port, AHCI_REG_PORT_SCTL, 0x00L);

    /* Check if there is a device on the port. */
    VBOXAHCI_PORT_READ_REG(u16IoBase, u8Port, AHCI_REG_PORT_SSTS, val);
    if (ahci_ctrl_extract_bits(val, 0xfL, 0) == 0x3L)
    {
        Bit8u idxDevice;

        idxDevice = read_byte(SegAhci, &AhciData->cDevices);
        VBOXAHCI_DEBUG("AHCI: Device detected on port %d\n", u8Port);

        if (idxDevice < AHCI_MAX_STORAGE_DEVICES)
        {
            /* Device detected, enable FIS receive. */
            ahci_ctrl_set_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                               AHCI_REG_PORT_CMD_FRE);

            /* Check signature to determine device type. */
            VBOXAHCI_PORT_READ_REG(u16IoBase, u8Port, AHCI_REG_PORT_SIG, val);
            if (val == 0x101L)
            {
                Bit8u idxHdCurr;
                Bit32u cSectors;
                Bit8u  abBuffer[0x0200];
                Bit8u  fRemovable;
                Bit16u cCylinders, cHeads, cSectorsPerTrack;
                Bit8u  cHardDisksOld;
                Bit8u  idxCmosChsBase;

                idxHdCurr = read_byte(SegAhci, &AhciData->cHardDisks);
                VBOXAHCI_DEBUG("AHCI: Detected hard disk\n");

                /* Identify device. */
                ahci_cmd_data(SegAhci, u16IoBase, ATA_CMD_IDENTIFY_DEVICE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, get_SS(), abBuffer, sizeof(abBuffer), 0);

                write_byte(SegAhci, &AhciData->aDevices[idxDevice].port, u8Port);

                fRemovable       = (read_byte(get_SS(),abBuffer+0) & 0x80) ? 1 : 0;
                cCylinders       = read_word(get_SS(),abBuffer+(1*2)); // word 1
                cHeads           = read_word(get_SS(),abBuffer+(3*2)); // word 3
                cSectorsPerTrack = read_word(get_SS(),abBuffer+(6*2)); // word 6
                cSectors         = read_dword(get_SS(),abBuffer+(60*2)); // word 60 and word 61

                /** @todo update sectors to be a 64 bit number (also lba...). */
                if (cSectors == 268435455)
                    cSectors = read_dword(get_SS(),abBuffer+(100*2)); // words 100 to 103 (someday)

                VBOXAHCI_DEBUG("AHCI: %ld sectors\n", cSectors);

                write_byte(SegAhci, &AhciData->aDevices[idxDevice].device,ATA_DEVICE_HD);
                write_byte(SegAhci, &AhciData->aDevices[idxDevice].removable, fRemovable);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].blksize, 512);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.heads, cHeads);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.cylinders, cCylinders);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.spt, cSectorsPerTrack);
                write_dword(SegAhci, &AhciData->aDevices[idxDevice].cSectors, cSectors);

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
                write_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.heads, cHeads);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.cylinders, cCylinders);
                write_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.spt, cSectorsPerTrack);

                write_byte(SegAhci, &AhciData->aHdIdMap[idxHdCurr], idxDevice);
                idxHdCurr++;
                write_byte(SegAhci, &AhciData->cHardDisks, idxHdCurr);

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
                VBOXAHCI_DEBUG("AHCI: Unknown device ignoring\n");

            idxDevice++;
            write_byte(SegAhci, &AhciData->cDevices, idxDevice);
        }
        else
            VBOXAHCI_DEBUG("AHCI: Reached maximum device count, skipping\n");
    }
}

#define SET_DISK_RET_STATUS(status) write_byte(0x0040, 0x0074, status)

/**
 * Int 13 handler.
 */
static void ahci_int13(RET, ES, DS, DI, SI, BP, SP, BX, DX, CX, AX, IPIRET, CSIRET, FLAGSIRET, IP, CS, FLAGS)
    Bit16u RET, ES, DS, AX, CX, DX, BX, SP, BP, SI, DI, IPIRET, CSIRET, FLAGSIRET, IP, CS, FLAGS;
{
    Bit16u ebda_seg;
    Bit16u SegAhci, u16IoBase;
    Bit8u  idxDevice;
    Bit8u  cHardDisksOld;
    Bit8u  u8Port;

    Bit32u lba;
    Bit16u cylinder, head, sector;
    Bit16u segment, offset;
    Bit16u npc, nph, npspt, nlc, nlh, nlspt;
    Bit16u size, count;
    Bit8u  status;

    VBOXAHCI_INT13_DEBUG("AHCI: ahci_int13 AX=%x CX=%x DX=%x BX=%x SP=%x BP=%x SI=%x DI=%x IP=%x CS=%x FLAGS=%x\n",
                         AX, CX, DX, BX, SP, BP, SI, DI, IP, CS, FLAGS);

    ebda_seg  = read_word(0x0040, 0x000E);
    SegAhci   = read_word(ebda_seg, &EbdaData->SegAhci);
    u16IoBase = read_word(SegAhci, &AhciData->iobase);
    VBOXAHCI_INT13_DEBUG("AHCI: ahci_int13: SegAhci=%x u16IoBase=%x\n", SegAhci, u16IoBase);

    cHardDisksOld = read_byte(SegAhci, &AhciData->cHardDisksOld);

    /* Check if the device is controlled by us first. */
    if (   (GET_DL() < 0x80)
        || (GET_DL() < 0x80 + cHardDisksOld)
        || ((GET_DL() & 0xe0) != 0x80) /* No CD-ROM drives supported for now */)
    {
        VBOXAHCI_INT13_DEBUG("AHCI: ahci_int13 device not controlled by us, forwarding to old handler (%d)\n", cHardDisksOld);
        /* Fill the iret frame to jump to the old handler. */
        IPIRET    = read_word(SegAhci, &AhciData->pfnInt13Old);
        CSIRET    = 0xf000;
        FLAGSIRET = FLAGS;
        RET       = 1;
        return;
    }

    idxDevice = read_byte(SegAhci, &AhciData->aHdIdMap[GET_DL()-0x80-cHardDisksOld]);

    if (idxDevice >= AHCI_MAX_STORAGE_DEVICES)
    {
        VBOXAHCI_INT13_DEBUG("AHCI: ahci_int13: function %02x, unmapped device for ELDL=%02x\n", GET_AH(), GET_DL());
        goto ahci_int13_fail;
    }

    u8Port = read_byte(SegAhci, &AhciData->aDevices[idxDevice].port);

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
            cylinder   |= ( ((Bit16u) GET_CL()) << 2) & 0x300;
            sector      = (GET_CL() & 0x3f);
            head        = GET_DH();

            segment = ES;
            offset  = BX;

            if ( (count > 128) || (count == 0) )
            {
                BX_INFO("int13_harddisk: function %02x, count out of range!\n",GET_AH());
                goto ahci_int13_fail;
            }

            nlc   = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.cylinders);
            nlh   = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.heads);
            nlspt = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.spt);

            // sanity check on cyl heads, sec
            if( (cylinder >= nlc) || (head >= nlh) || (sector > nlspt ))
            {
              BX_INFO("ahci_int13: function %02x, disk %02x, parameters out of range %04x/%04x/%04x!\n", GET_AH(), GET_DL(), cylinder, head, sector);
              goto ahci_int13_fail;
            }

            // FIXME verify
            if ( GET_AH() == 0x04 )
                goto ahci_int13_success;

            lba = ((((Bit32u)cylinder * (Bit32u)nlh) + (Bit32u)head) * (Bit32u)nlspt) + (Bit32u)sector - 1;

            status = 0;
            if ( GET_AH() == 0x02 )
                ahci_cmd_data_in(SegAhci, u16IoBase, u8Port, AHCI_CMD_READ_DMA_EXT, lba, count, segment, offset);
            else
                ahci_cmd_data_out(SegAhci, u16IoBase, u8Port, AHCI_CMD_WRITE_DMA_EXT, lba, count, segment, offset);

            // Set nb of sector transferred
            SET_AL(read_word(ebda_seg, &EbdaData->ata.trsfsectors));

            if (status != 0)
            {
                BX_INFO("int13_harddisk: function %02x, error %02x !\n",GET_AH(),status);
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
            nlc   = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.cylinders);
            nlh   = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.heads);
            nlspt = read_word(SegAhci, &AhciData->aDevices[idxDevice].lchs.spt);

            count = read_byte(SegAhci, &AhciData->cHardDisks); /** @todo correct? */
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
            npc   = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.cylinders);
            nph   = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.heads);
            npspt = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.spt);

            // Compute sector count seen by int13
            lba = (Bit32u)npc * (Bit32u)nph * (Bit32u)npspt;
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
            count=read_word(DS, SI+(Bit16u)&Int13Ext->count);
            segment=read_word(DS, SI+(Bit16u)&Int13Ext->segment);
            offset=read_word(DS, SI+(Bit16u)&Int13Ext->offset);

            // Can't use 64 bits lba
            lba=read_dword(DS, SI+(Bit16u)&Int13Ext->lba2);
            if (lba != 0L)
            {
                BX_PANIC("int13_harddisk: function %02x. Can't use 64bits lba\n",GET_AH());
                goto ahci_int13_fail;
            }

            // Get 32 bits lba and check
            lba=read_dword(DS, SI+(Bit16u)&Int13Ext->lba1);

            if (lba >= read_word(SegAhci, &AhciData->aDevices[idxDevice].cSectors) )
            {
                BX_INFO("int13_harddisk: function %02x. LBA out of range\n",GET_AH());
                goto ahci_int13_fail;
            }

            // If verify or seek
            if (( GET_AH() == 0x44 ) || ( GET_AH() == 0x47 ))
                goto ahci_int13_success;

            // Execute the command
            if ( GET_AH() == 0x42 )
            {
                if (lba + count >= 268435456)
                    status=ata_cmd_data_in(device, ATA_CMD_READ_SECTORS_EXT, count, 0, 0, 0, lba, segment, offset);
                else
                {
                    write_word(ebda_seg,&EbdaData->ata.devices[device].blksize,count * 0x200);
                    status=ata_cmd_data_in(device, ATA_CMD_READ_MULTIPLE, count, 0, 0, 0, lba, segment, offset);
                    write_word(ebda_seg,&EbdaData->ata.devices[device].blksize,0x200);
                }
            }
            else
            {
                if (lba + count >= 268435456)
                    status=ata_cmd_data_out(device, ATA_CMD_WRITE_SECTORS_EXT, count, 0, 0, 0, lba, segment, offset);
                else
                    status=ata_cmd_data_out(device, ATA_CMD_WRITE_SECTORS, count, 0, 0, 0, lba, segment, offset);
            }

            count=read_word(ebda_seg, &EbdaData->ata.trsfsectors);
            write_word(DS, SI+(Bit16u)&Int13Ext->count, count);

            if (status != 0)
            {
                BX_INFO("int13_harddisk: function %02x, error %02x !\n",GET_AH(),status);
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
            size=read_word(DS,SI+(Bit16u)&Int13DPT->size);

            // Buffer is too small
            if(size < 0x1a)
                goto ahci_int13_fail;

            // EDD 1.x
            if(size >= 0x1a)
            {
                Bit16u   blksize;

                npc     = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.cylinders);
                nph     = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.heads);
                npspt   = read_word(SegAhci, &AhciData->aDevices[idxDevice].pchs.spt);
                lba     = read_dword(SegAhci, &AhciData->aDevices[idxDevice].cSectors);
                blksize = read_word(SegAhci, &AhciData->aDevices[idxDevice].blksize);

                write_word(DS, SI+(Bit16u)&Int13DPT->size, 0x1a);
                write_word(DS, SI+(Bit16u)&Int13DPT->infos, 0x02); // geometry is valid
                write_dword(DS, SI+(Bit16u)&Int13DPT->cylinders, (Bit32u)npc);
                write_dword(DS, SI+(Bit16u)&Int13DPT->heads, (Bit32u)nph);
                write_dword(DS, SI+(Bit16u)&Int13DPT->spt, (Bit32u)npspt);
                write_dword(DS, SI+(Bit16u)&Int13DPT->sector_count1, lba);  // FIXME should be Bit64
                write_dword(DS, SI+(Bit16u)&Int13DPT->sector_count2, 0L);
                write_word(DS, SI+(Bit16u)&Int13DPT->blksize, blksize);
            }

#if 0 /* Disable EDD 2.X and 3.x for now, don't know if it is required by any OS loader yet */
            // EDD 2.x
            if(size >= 0x1e)
            {
                Bit8u  channel, dev, irq, mode, checksum, i, translation;
                Bit16u iobase1, iobase2, options;

                translation = ATA_TRANSLATION_LBA;

                write_word(DS, SI+(Bit16u)&Int13DPT->size, 0x1e);

                write_word(DS, SI+(Bit16u)&Int13DPT->dpte_segment, ebda_seg);
                write_word(DS, SI+(Bit16u)&Int13DPT->dpte_offset, &EbdaData->ata.dpte);

                // Fill in dpte
                channel = device / 2;
                iobase1 = read_word(ebda_seg, &EbdaData->ata.channels[channel].iobase1);
                iobase2 = read_word(ebda_seg, &EbdaData->ata.channels[channel].iobase2);
                irq = read_byte(ebda_seg, &EbdaData->ata.channels[channel].irq);
                mode = read_byte(ebda_seg, &EbdaData->ata.devices[device].mode);
                translation = read_byte(ebda_seg, &EbdaData->ata.devices[device].translation);

                options  = (translation==ATA_TRANSLATION_NONE?0:1<<3); // chs translation
                options |= (1<<4); // lba translation
                options |= (mode==ATA_MODE_PIO32?1:0<<7);
                options |= (translation==ATA_TRANSLATION_LBA?1:0<<9);
                options |= (translation==ATA_TRANSLATION_RECHS?3:0<<9);

                write_word(ebda_seg, &EbdaData->ata.dpte.iobase1, iobase1);
                write_word(ebda_seg, &EbdaData->ata.dpte.iobase2, iobase2);
                //write_byte(ebda_seg, &EbdaData->ata.dpte.prefix, (0xe | /*(device % 2))<<4*/ );
                write_byte(ebda_seg, &EbdaData->ata.dpte.unused, 0xcb );
                write_byte(ebda_seg, &EbdaData->ata.dpte.irq, irq );
                write_byte(ebda_seg, &EbdaData->ata.dpte.blkcount, 1 );
                write_byte(ebda_seg, &EbdaData->ata.dpte.dma, 0 );
                write_byte(ebda_seg, &EbdaData->ata.dpte.pio, 0 );
                write_word(ebda_seg, &EbdaData->ata.dpte.options, options);
                write_word(ebda_seg, &EbdaData->ata.dpte.reserved, 0);
                write_byte(ebda_seg, &EbdaData->ata.dpte.revision, 0x11);

                checksum=0;
                for (i=0; i<15; i++)
                    checksum+=read_byte(ebda_seg, (&EbdaData->ata.dpte) + i);

                checksum = -checksum;
                write_byte(ebda_seg, &EbdaData->ata.dpte.checksum, checksum);
            }

            // EDD 3.x
            if(size >= 0x42)
            {
                Bit8u channel, iface, checksum, i;
                Bit16u iobase1;

                channel = device / 2;
                iface = read_byte(ebda_seg, &EbdaData->ata.channels[channel].iface);
                iobase1 = read_word(ebda_seg, &EbdaData->ata.channels[channel].iobase1);

                write_word(DS, SI+(Bit16u)&Int13DPT->size, 0x42);
                write_word(DS, SI+(Bit16u)&Int13DPT->key, 0xbedd);
                write_byte(DS, SI+(Bit16u)&Int13DPT->dpi_length, 0x24);
                write_byte(DS, SI+(Bit16u)&Int13DPT->reserved1, 0);
                write_word(DS, SI+(Bit16u)&Int13DPT->reserved2, 0);

                if (iface==ATA_IFACE_ISA) {
                  write_byte(DS, SI+(Bit16u)&Int13DPT->host_bus[0], 'I');
                  write_byte(DS, SI+(Bit16u)&Int13DPT->host_bus[1], 'S');
                  write_byte(DS, SI+(Bit16u)&Int13DPT->host_bus[2], 'A');
                  write_byte(DS, SI+(Bit16u)&Int13DPT->host_bus[3], 0);
                  }
                else {
                  // FIXME PCI
                  }
                write_byte(DS, SI+(Bit16u)&Int13DPT->iface_type[0], 'A');
                write_byte(DS, SI+(Bit16u)&Int13DPT->iface_type[1], 'T');
                write_byte(DS, SI+(Bit16u)&Int13DPT->iface_type[2], 'A');
                write_byte(DS, SI+(Bit16u)&Int13DPT->iface_type[3], 0);

                if (iface==ATA_IFACE_ISA) {
                  write_word(DS, SI+(Bit16u)&Int13DPT->iface_path[0], iobase1);
                  write_word(DS, SI+(Bit16u)&Int13DPT->iface_path[2], 0);
                  write_dword(DS, SI+(Bit16u)&Int13DPT->iface_path[4], 0L);
                  }
                else {
                  // FIXME PCI
                  }
                //write_byte(DS, SI+(Bit16u)&Int13DPT->device_path[0], device%2);
                write_byte(DS, SI+(Bit16u)&Int13DPT->device_path[1], 0);
                write_word(DS, SI+(Bit16u)&Int13DPT->device_path[2], 0);
                write_dword(DS, SI+(Bit16u)&Int13DPT->device_path[4], 0L);

                checksum=0;
                for (i=30; i<64; i++) checksum+=read_byte(DS, SI + i);
                checksum = -checksum;
                write_byte(DS, SI+(Bit16u)&Int13DPT->checksum, checksum);
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

ahci_int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
ahci_int13_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
ahci_int13_fail_nostatus:
    SET_CF();     // error occurred
    return;

ahci_int13_success:
    SET_AH(0x00); // no error
ahci_int13_success_noah:
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}

#undef SET_DISK_RET_STATUS

/**
 * Assembler part of the int 13 handler.
 */
ASM_START
ahci_int13_handler:
    ; Allocate space for an iret frame we have to use
    ; to call the old int 13 handler
    push #0x0000
    push #0x0000
    push #0x0000

    pusha                 ; Save 16bit values as arguments
    push ds
    push es
    mov ax, #0            ; Allocate room for the return value on the stack
    push ax
    call _ahci_int13
    pop ax
    pop es
    pop ds
    cmp ax, #0x0          ; Check if the handler processed the request
    je ahci_int13_out
    popa                  ; Restore the caller environment
    iret                  ; Call the old interrupt handler. Will not come back here.

ahci_int13_out:
    popa
    add sp, #6            ; Destroy the iret frame
    iret
ASM_END

/**
 * Install the in13 interrupt handler
 * preserving the previous one.
 */
static void ahci_install_int_handler(SegAhci)
    Bit16u SegAhci;
{

    Bit16u pfnInt13Old;

    VBOXAHCI_DEBUG("AHCI: Hooking int 13h vector\n");

    /* Read the old interrupt handler. */
    pfnInt13Old = read_word(0x0000, 0x0013*4);
    write_word(SegAhci, &AhciData->pfnInt13Old, pfnInt13Old);

    /* Set our own */
    ASM_START

    push bp
    mov bp, sp

    push ax
    SET_INT_VECTOR(0x13, #0xF000, #ahci_int13_handler)
    pop ax

    pop bp
    ASM_END
}

/**
 * Allocates 1K from the base memory.
 */
static Bit16u ahci_mem_alloc()
{
    Bit16u cBaseMem1K;
    Bit16u SegStart;

    cBaseMem1K = read_byte(0x00, 0x0413);

    VBOXAHCI_DEBUG("AHCI: %x K of base memory available\n", cBaseMem1K);

    if (cBaseMem1K == 0)
        return 0;

    cBaseMem1K--; /* Allocate one block. */
    SegStart = (Bit16u)(((Bit32u)cBaseMem1K * 1024) >> 4); /* Calculate start segment. */

    write_byte(0x00, 0x0413, cBaseMem1K);

    return SegStart;
}

/**
 * Initializes the SATA controller and detects attached devices.
 */
static int ahci_ctrl_init(u16IoBase)
    Bit16u u16IoBase;
{
    Bit8u i, cPorts;
    Bit32u val;
    Bit16u ebda_seg;
    Bit16u SegAhci;

    ebda_seg = read_word(0x0040, 0x000E);

    VBOXAHCI_READ_REG(u16IoBase, AHCI_REG_VS, val);
    VBOXAHCI_DEBUG("AHCI: Controller has version: 0x%x (major) 0x%x (minor)\n",
                   ahci_ctrl_extract_bits(val, 0xffff0000, 16),
                   ahci_ctrl_extract_bits(val, 0x0000ffff,  0));

    /* Allocate 1K of base memory. */
    SegAhci = ahci_mem_alloc();
    if (SegAhci == 0)
    {
        VBOXAHCI_DEBUG("AHCI: Could not allocate 1K of memory, can't boot from controller\n");
        return 0;
    }

    write_word(ebda_seg, &EbdaData->SegAhci, SegAhci);
    write_byte(SegAhci, &AhciData->port, 0xff);
    write_word(SegAhci, &AhciData->iobase, u16IoBase);
    write_byte(SegAhci, &AhciData->cHardDisksOld, read_byte(0x40, 0x75));

    /* Reset the controller. */
    ahci_ctrl_set_bits(u16IoBase, AHCI_REG_GHC, AHCI_GHC_HR);
    do
    {
        VBOXAHCI_READ_REG(u16IoBase, AHCI_REG_GHC, val);
    } while (val & AHCI_GHC_HR != 0);

    VBOXAHCI_READ_REG(u16IoBase, AHCI_REG_CAP, val);
    cPorts = ahci_ctrl_extract_bits(val, 0x1f, 0) + 1; /* Extract number of ports.*/

    VBOXAHCI_DEBUG("AHCI: Controller has %u ports\n", cPorts);

    /* Go through the ports. */
    i = 0;
    while (i < 32)
    {
        if (ahci_ctrl_is_bit_set(u16IoBase, AHCI_REG_PI, RT_BIT_32(i)) != 0)
        {
            VBOXAHCI_DEBUG("AHCI: Port %u is present\n", i);
            ahci_port_detect_device(SegAhci, u16IoBase, i);
            cPorts--;
            if (cPorts == 0)
                break;
        }
        i++;
    }

    if (read_byte(SegAhci, &AhciData->cDevices) > 0)
    {
        /*
         * Init completed and there is at least one device present.
         * Install our int13 handler.
         */
        ahci_install_int_handler(SegAhci);
    }

    return 0;
}

/**
 * Init the AHCI driver and detect attached disks.
 */
void ahci_init( )
{
    Bit16u ebda_seg;
    Bit16u busdevfn;

    ebda_seg = read_word(0x0040, 0x000E);

    busdevfn = ahci_pci_find_classcode(0x00010601);
    if (busdevfn != VBOX_AHCI_NO_DEVICE)
    {
        Bit8u u8Bus, u8DevFn;
        Bit8u u8PciCapOff;

        u8Bus = (busdevfn & 0xff00) >> 8;
        u8DevFn = busdevfn & 0x00ff;

        VBOXAHCI_DEBUG("Found AHCI controller at Bus %u DevFn 0x%x (raw 0x%x)\n", u8Bus, u8DevFn, busdevfn);

        /* Examine the capability list and search for the Serial ATA Capability Register. */
        u8PciCapOff = ahci_pci_read_config_byte(u8Bus, u8DevFn, PCI_CONFIG_CAP);

        while (u8PciCapOff != 0)
        {
            Bit8u u8PciCapId = ahci_pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);

            VBOXAHCI_DEBUG("Capability ID 0x%x at offset 0x%x found\n", u8PciCapId, u8PciCapOff);

            if (u8PciCapId == PCI_CAP_ID_SATACR)
                break;

            /* Go on to the next capability. */
            u8PciCapOff = ahci_pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);
        }

        if (u8PciCapOff != 0)
        {
            Bit8u u8Rev;

            VBOXAHCI_DEBUG("AHCI controller with SATA Capability register at offset 0x%x found\n", u8PciCapOff);

            /* Advance to the stuff behind the id and next capability pointer. */
            u8PciCapOff += 2;

            u8Rev = ahci_pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);
            if (u8Rev == 0x10)
            {
                /* Read the SATACR1 register and get the bar and offset of the index/data pair register. */
                Bit8u  u8Bar = 0x00;
                Bit16u u16Off = 0x00;
                Bit16u u16BarOff = ahci_pci_read_config_word(u8Bus, u8DevFn, u8PciCapOff + 2);

                VBOXAHCI_DEBUG("SATACR1 register contains 0x%x\n", u16BarOff);

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
                        VBOXAHCI_DEBUG("BAR location 0x%x is unsupported\n", u16BarOff & 0xf);
                }

                /* Get the offset inside the BAR from bits 4:15. */
                u16Off = (u16BarOff >> 4) * 4;

                if (u8Bar != 0x00)
                {
                    Bit32u u32Bar = ahci_pci_read_config_dword(u8Bus, u8DevFn, u8Bar);

                    VBOXAHCI_DEBUG("BAR at offset 0x%x contains 0x%x\n", u8Bar, u32Bar);

                    if ((u32Bar & 0x01) != 0)
                    {
                        int rc;
                        Bit16u u16AhciIoBase = (u32Bar & 0xfff0) + u16Off;

                        VBOXAHCI_DEBUG("I/O base is 0x%x\n", u16AhciIoBase);
                        rc = ahci_ctrl_init(u16AhciIoBase);
                    }
                    else
                        VBOXAHCI_DEBUG("BAR is MMIO\n");
                }
            }
            else
                VBOXAHCI_DEBUG("Invalid revision 0x%x\n", u8Rev);
        }
        else
            VBOXAHCI_DEBUG("AHCI controller without usable Index/Data register pair found\n");
    }
    else
        VBOXAHCI_DEBUG("No AHCI controller found\n");
}

