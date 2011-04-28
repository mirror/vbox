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

#define VBOX_AHCI_DEBUG 1 /* temporary */

#ifdef VBOX_AHCI_DEBUG
# define VBOXAHCI_DEBUG(a...) BX_INFO(a)
#else
# define VBOXAHCI_DEBUG(a...)
#endif

#define RT_BIT_32(bit) ((Bit32u)(1 << (bit)))

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
    VBOX_AHCI_WRITE_REG((iobase), VBOXAHCI_PORT_REG((port), (reg)), val)

/** Reads from the given port register. */
#define VBOXAHCI_PORT_READ_REG(iobase, port, reg, val) \
    VBOX_AHCI_READ_REG((iobase), VBOXAHCI_PORT_REG((port), (reg)), val)

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
    uint8_t u8Val;

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

    push eax
    push edx

    ; Read from the register
    mov eax, _ahci_ctrl_is_bit_set.u32Reg + 2[bp]
    mov dx, _ahci_ctrl_is_bit_set.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_IDX
    out dx, eax

    mov dx, _ahci_ctrl_is_bit_set.u16IoBase + 2[bp]
    add dx, #VBOXAHCI_REG_DATA
    in eax, dx

    ; Check for set bits
    test eax, _ahci_ctrl_is_bit_set.u32Mask + 2[bp]
    je ahci_ctrl_is_bit_set_not_set
    mov al, #1                                       ; At least one of the bits is set
    jmp ahci_ctrl_is_bit_set_done

ahci_ctrl_is_bit_set_not_set:
    mov al, #0                                       ; No bit is set

ahci_ctrl_is_bit_set_done:
    pop edx
    pop eax

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

static int ahci_port_init(u16IoBase, u8Port)
    Bit16u u16IoBase;
    Bit8u u8Port;
{

    /* Put the port into an idle state. */
    ahci_ctrl_clear_bits(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                         AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST);

    while (ahci_ctrl_is_bit_set(u16IoBase, VBOXAHCI_PORT_REG(u8Port, AHCI_REG_PORT_CMD),
                                AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_ST | AHCI_REG_PORT_CMD_FRE | AHCI_REG_PORT_CMD_CR) == 1)
    {
        VBOXAHCI_DEBUG("AHCI: Waiting for the port to idle\n");
    }

    /*
     * Port idles, set up memory for commands and received FIS and program the
     * address registers.
     */

    return -1;
}

static int ahci_ctrl_init(u16IoBase)
    Bit16u u16IoBase;
{
    int rc = 0;
    Bit8u i, cPorts;
    Bit32u val;

    VBOXAHCI_READ_REG(u16IoBase, AHCI_REG_VS, val);
    VBOXAHCI_DEBUG("AHCI: Controller has version: 0x%x (major) 0x%x (minor)\n",
                   ahci_ctrl_extract_bits(val, 0xffff0000, 16),
                   ahci_ctrl_extract_bits(val, 0x0000ffff,  0));

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
            rc = ahci_port_init(u16IoBase, i);
            cPorts--;
            if (cPorts == 0)
                break;
        }
        i++;
    }

    return rc;
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

