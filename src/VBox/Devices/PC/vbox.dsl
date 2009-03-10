// $Id$
/// @file
//
// VirtualBox ACPI
//
// Copyright (C) 2006-2007 Sun Microsystems, Inc.
//
// This file is part of VirtualBox Open Source Edition (OSE), as
// available from http://www.virtualbox.org. This file is free software;
// you can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
// distribution. VirtualBox OSE is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY of any kind.
//

DefinitionBlock ("DSDT.aml", "DSDT", 1, "VBOX  ", "VBOXBIOS", 2)
{
    // Declare debugging ports withing SystemIO
    OperationRegion(DBG0, SystemIO, 0x3000, 4)

    // Writes to this field Will dump hex char
    Field (DBG0, ByteAcc, NoLock, Preserve)
    {
        DHE1, 8,
    }

    // Writes to this field Will dump hex word
    Field (DBG0, WordAcc, NoLock, Preserve)
    {
        DHE2, 16,
    }

    // Writes to this field Will dump hex double word
    Field (DBG0, DWordAcc, NoLock, Preserve)
    {
        DHE4, 32,
    }

    // Writes to this field will dump ascii char
    Field (DBG0, ByteAcc, NoLock, Preserve)
    {
        Offset (1),
        DCHR, 8
    }

    // Shortcuts
    Method(HEX, 1)
    {
        Store (Arg0, DHE1)
    }

    Method(HEX2, 1)
    {
        Store (Arg0, DHE2)
    }

    Method(HEX4, 1)
    {
        Store (Arg0, DHE4)
    }

    // Code from Microsoft sample
    // http://www.microsoft.com/whdc/system/pnppwr/powermgmt/_OSI-method.mspx

    //
    // SLEN(Str) - Returns the length of Str (excluding NULL).
    //
    Method(SLEN, 1)
    {
        //
        // Note: The caller must make sure that the argument is a string object.
        //
        Store(Arg0, Local0)
        Return(Sizeof(Local0))
    }

    Method(S2BF, 1)
    {
        //
        // Note: The caller must make sure that the argument is a string object.
        //
        // Local0 contains length of string + NULL.
        //
        Store(Arg0, Local0)
        Add(SLEN(Local0), One, Local0)
        //
        // Convert the string object into a buffer object.
        //
        Name(BUFF, Buffer(Local0) {})
        Store(Arg0, BUFF)
        Return(BUFF)
    }

    // Convert ASCII string to buffer and store it's contents (char by
    // char) into DCHR (thus possibly writing the string to console)
    Method (\DBG, 1, NotSerialized)
    {
        Store(Arg0, Local0)
        Store(S2BF (Local0), Local1)
        Store(SizeOf (Local1), Local0)
        Decrement (Local0)
        Store(Zero, Local2)
        While (Local0)
        {
            Decrement (Local0)
            Store (DerefOf (Index (Local1, Local2)), DCHR)
            Increment (Local2)
        }
    }

    Name(PICM, 0)
    Method(_PIC, 1)
    {
        DBG ("Pic mode: ")
        HEX4 (Arg0)
        Store (Arg0, PICM)
    }

    // Processor object
    // #1463: Showing the CPU can make the guest do bad things on it like SpeedStep.
    // In this case, XP SP2 contains this buggy Intelppm.sys driver which wants to mess
    // with SpeedStep if it finds a CPU object and when it finds out that it can't, it
    // tries to unload and crashes (MS probably never tested this code path).
    // So we enable this ACPI object only for certain guests, which do need it,
    // if by accident Windows guest seen enabled CPU object, just boot from latest
    // known good configuration, as it remembers state, even if ACPI object gets disabled.
    Scope (\_PR)
    {
       Processor (CPU0, /* Name */
                   0x00, /* Id */
                   0x0,  /* Processor IO ports range start */
                   0x0   /* Processor IO ports range length */
                   )
        {
           Method (_STA) { Return(\_SB.UCP0) }
        }
        Processor (CPU1, /* Name */
                   0x01, /* Id */
                   0x0,  /* Processor IO ports range start */
                   0x0   /* Processor IO ports range length */
                   )
        {
           Method (_STA) { Return(\_SB.UCP1) }
        }
        Processor (CPU2, /* Name */
                   0x02, /* Id */
                   0x0,  /* Processor IO ports range start */
                   0x0   /* Processor IO ports range length */
                   )
        {
           Method (_STA) { Return(\_SB.UCP2) }
        }
        Processor (CPU3, /* Name */
                   0x03, /* Id */
                   0x0,  /* Processor IO ports range start */
                   0x0   /* Processor IO ports range length */
                   )
        {
           Method (_STA) { Return(\_SB.UCP3) }
        }
    }

    Scope (\_SB)
    {
        OperationRegion (SYSI, SystemIO, 0x4048, 0x08)
        Field (SYSI, DwordAcc, NoLock, Preserve)
        {
            IDX0, 32,
            DAT0, 32,
        }

        IndexField (IDX0, DAT0, DwordAcc, NoLock, Preserve)
        {
            MEML,  32,
            UIOA,  32,
            UHPT,  32,
            USMC,  32,
            UFDC,  32,
            // @todo: maybe make it bitmask instead?
            UCP0,  32,
            UCP1,  32,
            UCP2,  32,
            UCP3,  32,
            MEMH,  32,
            Offset (0x80),
            ININ, 32,
            Offset (0x200),
            VAIN, 32,
        }

        Method (_INI, 0, NotSerialized)
        {
            Store (0xbadc0de, VAIN)
            DBG ("MEML: ")
            HEX4 (MEML)
            DBG ("UIOA: ")
            HEX4 (UIOA)
            DBG ("UHPT: ")
            HEX4 (UHPT)
            DBG ("USMC: ")
            HEX4 (USMC)
            DBG ("UFDC: ")
            HEX4 (UFDC)
            DBG ("UCP0: ")
            HEX4 (UCP0)
            DBG ("MEMH: ")
            HEX4 (MEMH)
        }

        // PCI PIC IRQ Routing table
        // Must match pci.c:pci_slot_get_pirq
        Name (PR00, Package ()
        {
            Package (0x04) {0x0002FFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x0002FFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x0002FFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x0002FFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x0003FFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x0003FFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x0003FFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x0003FFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x0004FFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x0004FFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x0004FFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x0004FFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x0005FFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x0005FFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x0005FFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x0005FFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x0006FFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x0006FFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x0006FFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x0006FFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x0007FFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x0007FFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x0007FFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x0007FFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x0008FFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x0008FFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x0008FFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x0008FFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x0009FFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x0009FFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x0009FFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x0009FFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x000AFFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x000AFFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x000AFFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x000AFFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x000BFFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x000BFFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x000BFFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x000BFFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x000CFFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x000CFFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x000CFFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x000CFFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x000DFFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x000DFFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x000DFFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x000DFFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x000EFFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x000EFFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x000EFFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x000EFFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x000FFFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x000FFFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x000FFFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x000FFFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x0010FFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x0010FFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x0010FFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x0010FFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x0011FFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x0011FFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x0011FFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x0011FFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x0012FFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x0012FFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x0012FFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x0012FFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x0013FFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x0013FFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x0013FFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x0013FFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x0014FFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x0014FFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x0014FFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x0014FFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x0015FFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x0015FFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x0015FFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x0015FFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x0016FFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x0016FFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x0016FFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x0016FFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x0017FFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x0017FFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x0017FFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x0017FFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x0018FFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x0018FFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x0018FFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x0018FFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x0019FFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x0019FFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x0019FFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x0019FFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x001AFFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x001AFFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x001AFFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x001AFFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x001BFFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x001BFFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x001BFFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x001BFFFF, 0x03, LNKB, 0x00,},

            Package (0x04) {0x001CFFFF, 0x00, LNKD, 0x00,},
            Package (0x04) {0x001CFFFF, 0x01, LNKA, 0x00,},
            Package (0x04) {0x001CFFFF, 0x02, LNKB, 0x00,},
            Package (0x04) {0x001CFFFF, 0x03, LNKC, 0x00,},

            Package (0x04) {0x001DFFFF, 0x00, LNKA, 0x00,},
            Package (0x04) {0x001DFFFF, 0x01, LNKB, 0x00,},
            Package (0x04) {0x001DFFFF, 0x02, LNKC, 0x00,},
            Package (0x04) {0x001DFFFF, 0x03, LNKD, 0x00,},

            Package (0x04) {0x001EFFFF, 0x00, LNKB, 0x00,},
            Package (0x04) {0x001EFFFF, 0x01, LNKC, 0x00,},
            Package (0x04) {0x001EFFFF, 0x02, LNKD, 0x00,},
            Package (0x04) {0x001EFFFF, 0x03, LNKA, 0x00,},

            Package (0x04) {0x001FFFFF, 0x00, LNKC, 0x00,},
            Package (0x04) {0x001FFFFF, 0x01, LNKD, 0x00,},
            Package (0x04) {0x001FFFFF, 0x02, LNKA, 0x00,},
            Package (0x04) {0x001FFFFF, 0x03, LNKB, 0x00,}
        })

        // PCI I/O APIC IRQ Routing table
        // Must match pci.c:pci_slot_get_acpi_pirq
        Name (PR01, Package ()
        {
            Package (0x04) {0x0002FFFF, 0x00, 0x00, 0x12,},
            Package (0x04) {0x0002FFFF, 0x01, 0x00, 0x13,},
            Package (0x04) {0x0002FFFF, 0x02, 0x00, 0x14,},
            Package (0x04) {0x0002FFFF, 0x03, 0x00, 0x15,},

            Package (0x04) {0x0003FFFF, 0x00, 0x00, 0x13,},
            Package (0x04) {0x0003FFFF, 0x01, 0x00, 0x14,},
            Package (0x04) {0x0003FFFF, 0x02, 0x00, 0x15,},
            Package (0x04) {0x0003FFFF, 0x03, 0x00, 0x16,},

            Package (0x04) {0x0004FFFF, 0x00, 0x00, 0x14,},
            Package (0x04) {0x0004FFFF, 0x01, 0x00, 0x15,},
            Package (0x04) {0x0004FFFF, 0x02, 0x00, 0x16,},
            Package (0x04) {0x0004FFFF, 0x03, 0x00, 0x17,},

            Package (0x04) {0x0005FFFF, 0x00, 0x00, 0x15,},
            Package (0x04) {0x0005FFFF, 0x01, 0x00, 0x16,},
            Package (0x04) {0x0005FFFF, 0x02, 0x00, 0x17,},
            Package (0x04) {0x0005FFFF, 0x03, 0x00, 0x10,},

            Package (0x04) {0x0006FFFF, 0x00, 0x00, 0x16,},
            Package (0x04) {0x0006FFFF, 0x01, 0x00, 0x17,},
            Package (0x04) {0x0006FFFF, 0x02, 0x00, 0x10,},
            Package (0x04) {0x0006FFFF, 0x03, 0x00, 0x11,},

            Package (0x04) {0x0007FFFF, 0x00, 0x00, 0x17,},
            Package (0x04) {0x0007FFFF, 0x01, 0x00, 0x10,},
            Package (0x04) {0x0007FFFF, 0x02, 0x00, 0x11,},
            Package (0x04) {0x0007FFFF, 0x03, 0x00, 0x12,},

            Package (0x04) {0x0008FFFF, 0x00, 0x00, 0x10,},
            Package (0x04) {0x0008FFFF, 0x01, 0x00, 0x11,},
            Package (0x04) {0x0008FFFF, 0x02, 0x00, 0x12,},
            Package (0x04) {0x0008FFFF, 0x03, 0x00, 0x13,},

            Package (0x04) {0x0009FFFF, 0x00, 0x00, 0x11,},
            Package (0x04) {0x0009FFFF, 0x01, 0x00, 0x12,},
            Package (0x04) {0x0009FFFF, 0x02, 0x00, 0x13,},
            Package (0x04) {0x0009FFFF, 0x03, 0x00, 0x14,},

            Package (0x04) {0x000AFFFF, 0x00, 0x00, 0x12,},
            Package (0x04) {0x000AFFFF, 0x01, 0x00, 0x13,},
            Package (0x04) {0x000AFFFF, 0x02, 0x00, 0x14,},
            Package (0x04) {0x000AFFFF, 0x03, 0x00, 0x15,},

            Package (0x04) {0x000BFFFF, 0x00, 0x00, 0x13,},
            Package (0x04) {0x000BFFFF, 0x01, 0x00, 0x14,},
            Package (0x04) {0x000BFFFF, 0x02, 0x00, 0x15,},
            Package (0x04) {0x000BFFFF, 0x03, 0x00, 0x16,},

            Package (0x04) {0x000CFFFF, 0x00, 0x00, 0x14,},
            Package (0x04) {0x000CFFFF, 0x01, 0x00, 0x15,},
            Package (0x04) {0x000CFFFF, 0x02, 0x00, 0x16,},
            Package (0x04) {0x000CFFFF, 0x03, 0x00, 0x17,},

            Package (0x04) {0x000DFFFF, 0x00, 0x00, 0x15,},
            Package (0x04) {0x000DFFFF, 0x01, 0x00, 0x16,},
            Package (0x04) {0x000DFFFF, 0x02, 0x00, 0x17,},
            Package (0x04) {0x000DFFFF, 0x03, 0x00, 0x10,},

            Package (0x04) {0x000EFFFF, 0x00, 0x00, 0x16,},
            Package (0x04) {0x000EFFFF, 0x01, 0x00, 0x17,},
            Package (0x04) {0x000EFFFF, 0x02, 0x00, 0x10,},
            Package (0x04) {0x000EFFFF, 0x03, 0x00, 0x11,},

            Package (0x04) {0x000FFFFF, 0x00, 0x00, 0x17,},
            Package (0x04) {0x000FFFFF, 0x01, 0x00, 0x10,},
            Package (0x04) {0x000FFFFF, 0x02, 0x00, 0x11,},
            Package (0x04) {0x000FFFFF, 0x03, 0x00, 0x12,},

            Package (0x04) {0x0010FFFF, 0x00, 0x00, 0x10,},
            Package (0x04) {0x0010FFFF, 0x01, 0x00, 0x11,},
            Package (0x04) {0x0010FFFF, 0x02, 0x00, 0x12,},
            Package (0x04) {0x0010FFFF, 0x03, 0x00, 0x13,},

            Package (0x04) {0x0011FFFF, 0x00, 0x00, 0x11,},
            Package (0x04) {0x0011FFFF, 0x01, 0x00, 0x12,},
            Package (0x04) {0x0011FFFF, 0x02, 0x00, 0x13,},
            Package (0x04) {0x0011FFFF, 0x03, 0x00, 0x14,},

            Package (0x04) {0x0012FFFF, 0x00, 0x00, 0x12,},
            Package (0x04) {0x0012FFFF, 0x01, 0x00, 0x13,},
            Package (0x04) {0x0012FFFF, 0x02, 0x00, 0x14,},
            Package (0x04) {0x0012FFFF, 0x03, 0x00, 0x15,},

            Package (0x04) {0x0013FFFF, 0x00, 0x00, 0x13,},
            Package (0x04) {0x0013FFFF, 0x01, 0x00, 0x14,},
            Package (0x04) {0x0013FFFF, 0x02, 0x00, 0x15,},
            Package (0x04) {0x0013FFFF, 0x03, 0x00, 0x16,},

            Package (0x04) {0x0014FFFF, 0x00, 0x00, 0x14,},
            Package (0x04) {0x0014FFFF, 0x01, 0x00, 0x15,},
            Package (0x04) {0x0014FFFF, 0x02, 0x00, 0x16,},
            Package (0x04) {0x0014FFFF, 0x03, 0x00, 0x17,},

            Package (0x04) {0x0015FFFF, 0x00, 0x00, 0x15,},
            Package (0x04) {0x0015FFFF, 0x01, 0x00, 0x16,},
            Package (0x04) {0x0015FFFF, 0x02, 0x00, 0x17,},
            Package (0x04) {0x0015FFFF, 0x03, 0x00, 0x10,},

            Package (0x04) {0x0016FFFF, 0x00, 0x00, 0x16,},
            Package (0x04) {0x0016FFFF, 0x01, 0x00, 0x17,},
            Package (0x04) {0x0016FFFF, 0x02, 0x00, 0x10,},
            Package (0x04) {0x0016FFFF, 0x03, 0x00, 0x11,},

            Package (0x04) {0x0017FFFF, 0x00, 0x00, 0x17,},
            Package (0x04) {0x0017FFFF, 0x01, 0x00, 0x10,},
            Package (0x04) {0x0017FFFF, 0x02, 0x00, 0x11,},
            Package (0x04) {0x0017FFFF, 0x03, 0x00, 0x12,},

            Package (0x04) {0x0018FFFF, 0x00, 0x00, 0x10,},
            Package (0x04) {0x0018FFFF, 0x01, 0x00, 0x11,},
            Package (0x04) {0x0018FFFF, 0x02, 0x00, 0x12,},
            Package (0x04) {0x0018FFFF, 0x03, 0x00, 0x13,},

            Package (0x04) {0x0019FFFF, 0x00, 0x00, 0x11,},
            Package (0x04) {0x0019FFFF, 0x01, 0x00, 0x12,},
            Package (0x04) {0x0019FFFF, 0x02, 0x00, 0x13,},
            Package (0x04) {0x0019FFFF, 0x03, 0x00, 0x14,},

            Package (0x04) {0x001AFFFF, 0x00, 0x00, 0x12,},
            Package (0x04) {0x001AFFFF, 0x01, 0x00, 0x13,},
            Package (0x04) {0x001AFFFF, 0x02, 0x00, 0x14,},
            Package (0x04) {0x001AFFFF, 0x03, 0x00, 0x15,},

            Package (0x04) {0x001BFFFF, 0x00, 0x00, 0x13,},
            Package (0x04) {0x001BFFFF, 0x01, 0x00, 0x14,},
            Package (0x04) {0x001BFFFF, 0x02, 0x00, 0x15,},
            Package (0x04) {0x001BFFFF, 0x03, 0x00, 0x16,},

            Package (0x04) {0x001CFFFF, 0x00, 0x00, 0x14,},
            Package (0x04) {0x001CFFFF, 0x01, 0x00, 0x15,},
            Package (0x04) {0x001CFFFF, 0x02, 0x00, 0x16,},
            Package (0x04) {0x001CFFFF, 0x03, 0x00, 0x17,},

            Package (0x04) {0x001DFFFF, 0x00, 0x00, 0x15,},
            Package (0x04) {0x001DFFFF, 0x01, 0x00, 0x16,},
            Package (0x04) {0x001DFFFF, 0x02, 0x00, 0x17,},
            Package (0x04) {0x001DFFFF, 0x03, 0x00, 0x10,},

            Package (0x04) {0x001EFFFF, 0x00, 0x00, 0x16,},
            Package (0x04) {0x001EFFFF, 0x01, 0x00, 0x17,},
            Package (0x04) {0x001EFFFF, 0x02, 0x00, 0x10,},
            Package (0x04) {0x001EFFFF, 0x03, 0x00, 0x11,},

            Package (0x04) {0x001FFFFF, 0x00, 0x00, 0x17,},
            Package (0x04) {0x001FFFFF, 0x01, 0x00, 0x10,},
            Package (0x04) {0x001FFFFF, 0x02, 0x00, 0x11,},
            Package (0x04) {0x001FFFFF, 0x03, 0x00, 0x12,}
        })

        // Possible resource settings for PCI link A
        Name (PRSA, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared) {5,9,10,11}
        })

        // Possible resource settings for PCI link B
        Name (PRSB, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared) {5,9,10,11}
        })

        // Possible resource settings for PCI link C
        Name (PRSC, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared) {5,9,10,11}
        })

        // Possible resource settings for PCI link D
        Name (PRSD, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared) {5,9,10,11}
        })

        // PCI bus 0
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03"))
            Name (_ADR, 0x00) // address
            Name (_BBN, 0x00) // base bus adddress
            Name (_UID, 0x00)

            // Method that returns routing table
            Method (_PRT, 0, NotSerialized)
            {
                if (LEqual (LAnd (PICM, UIOA), Zero)) {
                    DBG ("RETURNING PIC\n")
                    Store (0x00, \_SB.PCI0.SBRG.APDE)
                    Store (0x00, \_SB.PCI0.SBRG.APAD)
                    Return (PR00)
                }
                else {
                    DBG ("RETURNING APIC\n")
                    Store (0xbe, \_SB.PCI0.SBRG.APDE)
                    Store (0xef, \_SB.PCI0.SBRG.APAD)
                    Return (PR01)
                }
            }

            Device (SBRG)
            {
                // Address of the PIIX3 (device 1 function 0)
                Name (_ADR, 0x00010000)
                OperationRegion (PCIC, PCI_Config, 0x00, 0xff)

                Field (PCIC, ByteAcc, NoLock, Preserve)
                {
                    Offset (0xad),
                    APAD,   8,
                    Offset (0xde),
                    APDE,   8,
                }

                // Keyboard device
                Device (PS2K)
                {
                    Name (_HID, EisaId ("PNP0303"))
                    Method (_STA, 0, NotSerialized)
                    {
                        Return (0x0F)
                    }

                    Name (_CRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x0060, 0x0060, 0x00, 0x01)
                        IO (Decode16, 0x0064, 0x0064, 0x00, 0x01)
                        IRQNoFlags () {1}
                    })
                }

                // DMA Controller
                Device (DMAC)
                {
                    Name (_HID, EisaId ("PNP0200"))
                    Name (_CRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x0000, 0x0000, 0x01, 0x10)
                        IO (Decode16, 0x0080, 0x0080, 0x01, 0x10)
                        IO (Decode16, 0x00C0, 0x00C0, 0x01, 0x20)
                        DMA (Compatibility, BusMaster, Transfer8_16) {4}
                    })
                }

                // Floppy disk controller
                Device (FDC0)
                {
                    Name (_HID, EisaId ("PNP0700"))

                    Method (_STA, 0, NotSerialized)
                    {
                        Return (UFDC)
                    }

                    // Current resource settings
                    Name (_CRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x03F0, 0x03F0, 0x01, 0x06)
                        IO (Decode16, 0x03F7, 0x03F7, 0x01, 0x01)
                        IRQNoFlags () {6}
                        DMA (Compatibility, NotBusMaster, Transfer8) {2}
                    })

                    // Possible resource settings
                    Name (_PRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x03F0, 0x03F0, 0x01, 0x06)
                        IO (Decode16, 0x03F7, 0x03F7, 0x01, 0x01)
                        IRQNoFlags () {6}
                        DMA (Compatibility, NotBusMaster, Transfer8) {2}
                    })

                }

                // Mouse device
                Device (PS2M)
                {
                    Name (_HID, EisaId ("PNP0F03"))
                    Method (_STA, 0, NotSerialized)
                    {
                        Return (0x0F)
                    }

                    Name (_CRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x0060, 0x0060, 0x00, 0x01)
                        IO (Decode16, 0x0064, 0x0064, 0x00, 0x01)
                        IRQNoFlags () {12}
                    })
                }

                // Parallel port
                Device (LPT)
                {
                    Name (_HID, EisaId ("PNP0400"))
                    Method (_STA, 0, NotSerialized)
                    {
                        Return (0x0F)
                    }
                    Name (_CRS, ResourceTemplate ()
                    {
                        IO (Decode16, 0x0378, 0x0378, 0x08, 0x08)
                        IO (Decode16, 0x0778, 0x0778, 0x08, 0x08)
                        IRQNoFlags () {7}
                    })
                }
            }

            // Control method battery
            Device (BAT0)
            {
                Name (_HID, EisaId ("PNP0C0A"))
                Name (_UID, 0x00)

                Scope (\_GPE)
                {
                    // GPE bit 0 handler
                    // GPE.0 must be set and SCI raised when
                    // battery info changed and _BIF must be
                    // re-evaluated
                    Method (_L00, 0, NotSerialized)
                    {
                            Notify (\_SB.PCI0.BAT0, 0x81)
                    }
                }

                OperationRegion (CBAT, SystemIO, 0x4040, 0x08)
                Field (CBAT, DwordAcc, NoLock, Preserve)
                {
                    IDX0, 32,
                    DAT0, 32,
                }

                IndexField (IDX0, DAT0, DwordAcc, NoLock, Preserve)
                {
                    STAT, 32,
                    PRAT, 32,
                    RCAP, 32,
                    PVOL, 32,

                    UNIT, 32,
                    DCAP, 32,
                    LFCP, 32,
                    BTEC, 32,
                    DVOL, 32,
                    DWRN, 32,
                    DLOW, 32,
                    GRN1, 32,
                    GRN2, 32,

                    BSTA, 32,
                    APSR, 32,
                }

                Method (_STA, 0, NotSerialized)
                {
                    return (BSTA)
                }

                Name (PBIF, Package ()
                {
                    0x01,       // Power unit, 1 - mA
                    0x7fffffff, // Design capacity
                    0x7fffffff, // Last full charge capacity
                    0x00,       // Battery technology
                    0xffffffff, // Design voltage
                    0x00,       // Design capacity of Warning
                    0x00,       // Design capacity of Low
                    0x04,       // Battery capacity granularity 1
                    0x04,       // Battery capacity granularity 2
                    "1",        // Model number
                    "0",        // Serial number
                    "VBOX",     // Battery type
                    "innotek"   // OEM Information
                })

                Name (PBST, Package () {
                    0,          // Battery state
                    0x7fffffff, // Battery present rate
                    0x7fffffff, // Battery remaining capacity
                    0x7fffffff  // Battery present voltage
                })

                // Battery information
                Method (_BIF, 0, NotSerialized)
                {
                    Store (UNIT, Index (PBIF, 0,))
                    Store (DCAP, Index (PBIF, 1,))
                    Store (LFCP, Index (PBIF, 2,))
                    Store (BTEC, Index (PBIF, 3,))
                    Store (DVOL, Index (PBIF, 4,))
                    Store (DWRN, Index (PBIF, 5,))
                    Store (DLOW, Index (PBIF, 6,))
                    Store (GRN1, Index (PBIF, 7,))
                    Store (GRN2, Index (PBIF, 8,))

                    DBG ("_BIF:\n")
                    HEX4 (DerefOf (Index (PBIF, 0,)))
                    HEX4 (DerefOf (Index (PBIF, 1,)))
                    HEX4 (DerefOf (Index (PBIF, 2,)))
                    HEX4 (DerefOf (Index (PBIF, 3,)))
                    HEX4 (DerefOf (Index (PBIF, 4,)))
                    HEX4 (DerefOf (Index (PBIF, 5,)))
                    HEX4 (DerefOf (Index (PBIF, 6,)))
                    HEX4 (DerefOf (Index (PBIF, 7,)))
                    HEX4 (DerefOf (Index (PBIF, 8,)))

                    return (PBIF)
                }

                // Battery status
                Method (_BST, 0, NotSerialized)
                {
                    Store (STAT, Index (PBST, 0,))
                    Store (PRAT, Index (PBST, 1,))
                    Store (RCAP, Index (PBST, 2,))
                    Store (PVOL, Index (PBST, 3,))
/*
                    DBG ("_BST:\n")
                    HEX4 (DerefOf (Index (PBST, 0,)))
                    HEX4 (DerefOf (Index (PBST, 1,)))
                    HEX4 (DerefOf (Index (PBST, 2,)))
                    HEX4 (DerefOf (Index (PBST, 3,)))
*/
                    return (PBST)
                }
            }

            Device (AC)
            {
                Name (_HID, "ACPI0003")
                Name (_UID, 0x00)
                Name (_PCL, Package (0x01)
                {
                    \_SB
                })

                Method (_PSR, 0, NotSerialized)
                {
                    // DBG ("_PSR:\n")
                    // HEX4 (\_SB.PCI0.BAT0.APSR)
                    return (\_SB.PCI0.BAT0.APSR)
                }

                Method (_STA, 0, NotSerialized)
                {
                    return (0x0f)
                }
            }
        }
    }

    Scope (\_SB)
    {
        Scope (PCI0)
        {
            // PCI0 current resource settings
            Name (CRS, ResourceTemplate ()
            {
                WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                               0x0000,
                               0x0000,
                               0x00FF,
                               0x0000,
                               0x0100)
                IO (Decode16, 0x0CF8, 0x0CF8, 0x01, 0x08)
                WordIO (ResourceProducer, MinFixed, MaxFixed,
                        PosDecode, EntireRange,
                        0x0000,
                        0x0000,
                        0x0CF7,
                        0x0000,
                        0x0CF8)
                WordIO (ResourceProducer, MinFixed, MaxFixed,
                        PosDecode, EntireRange,
                        0x0000,
                        0x0D00,
                        0xFFFF,
                        0x0000,
                        0xF300)

                /* Taken from ACPI faq (with some modifications) */
                DwordMemory( // descriptor for video RAM behind ISA bus
                     ResourceProducer,        // bit 0 of general flags is 0
                     PosDecode,
                     MinFixed,                // Range is fixed
                     MaxFixed,                // Range is Fixed
                     Cacheable,
                     ReadWrite,
                     0x00000000,              // Granularity
                     0x000a0000,              // Min
                     0x000bffff,              // Max
                     0x00000000,              // Translation
                     0x00020000               // Range Length
                     )

                DwordMemory( // Consumed-and-produced resource
                             // (all of low memory space)
                     ResourceProducer,        // bit 0 of general flags is 0
                     PosDecode,               // positive Decode
                     MinFixed,                // Range is fixed
                     MaxFixed,                // Range is fixed
                     Cacheable,
                     ReadWrite,
                     0x00000000,              // Granularity
                     0x00000000,              // Min (calculated dynamically)

                     0xffdfffff,              // Max = 4GB - 2MB
                     0x00000000,              // Translation
                     0xdfdfffff,              // Range Length (calculated
                                              // dynamically)
                     ,                        // Optional field left blank
                     ,                        // Optional field left blank
                     MEM3                     // Name declaration for this
                                              //  descriptor
                     )
            })

            Name (TOM, ResourceTemplate ()      // Memory above 4GB (aka high), appended when needed.
            {
                QWORDMemory(
                    ResourceProducer,           // bit 0 of general flags is 0
                    PosDecode,                  // positive Decode
                    MinFixed,                   // Range is fixed
                    MaxFixed,                   // Range is fixed
                    Cacheable,
                    ReadWrite,
                    0x0000000000000000,         // _GRA: Granularity.
                    0 /*0x0000000100000000*/,   // _MIN: Min address, 4GB.
                    0 /*0x00000fffffffffff*/,   // _MAX: Max possible address, 16TB.
                    0x0000000000000000,         // _TRA: Translation
                    0x0000000000000000,         // _LEN: Range length (calculated dynamically)
                    ,                           // ResourceSourceIndex: Optional field left blank
                    ,                           // ResourceSource:      Optional field left blank
                    MEM4                        // Name declaration for this descriptor.
                    )
            })

            Method (_CRS, 0, NotSerialized)
            {
                CreateDwordField (CRS, \_SB.PCI0.MEM3._MIN, RAMT)
                CreateDwordField (CRS, \_SB.PCI0.MEM3._LEN, RAMR)
                CreateQwordField (TOM, \_SB.PCI0.MEM4._LEN, TM4L)
                CreateQwordField (TOM, \_SB.PCI0.MEM4._LEN, TM4N)
                CreateQwordField (TOM, \_SB.PCI0.MEM4._LEN, TM4X)

                Store (MEML, RAMT)
                Subtract (0xffe00000, RAMT, RAMR)

				If (LNotEqual (MEMH, 0x00000000))
                {
                    //
                    // Update the TOM resource template and append it to CRS.
                    // This way old < 4GB guest doesn't see anything different.
                    // (MEMH is the memory above 4GB specified in 64KB units.)
                    //
                    // Note: ACPI v2 doesn't do 32-bit integers. IASL may fail on
                    //       seeing 64-bit constants and the code probably wont work.
                    //
                    Store (1, TM4N)
                    ShiftLeft (TM4N, 32, TM4N)

                    Store (0x00000fff, TM4X)
                    ShiftLeft (TM4X, 32, TM4X)
                    Or (TM4X, 0xffffffff, TM4X)

                    Store (MEMH, TM4L)
                    ShiftLeft (TM4L, 16, TM4L)

                    ConcatenateResTemplate (CRS, TOM, Local2)
                    Return (Local2)
                }

                Return (CRS)
            }
        }
    }

    Scope (\_SB)
    {
         // High Precision Event Timer
        Device(HPET) {
            Name(_HID,  EISAID("PNP0103"))
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                    Return(UHPT)
            }
            Name(_CRS, ResourceTemplate() {
                DWordMemory(
                    ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                    NonCacheable, ReadWrite,
                    0x00000000,
                    0xFED00000,
                    0xFED003FF,
                    0x00000000,
                    0x00000400 /* 1K memory: FED00000 - FED003FF */
                )
            })
        }

       // System Management Controller
       Device (SMC)
       {
            Name (_HID, EisaId ("APP0001"))
            Name (_CID, "smc-napa")

            Method (_STA, 0, NotSerialized)
            {
                Return (USMC)
            }
            Name (_CRS, ResourceTemplate ()
            {
                IO (Decode16,
                    0x0300,             // Range Minimum
                    0x0300,             // Range Maximum
                    0x01,               // Alignment
                    0x20,               // Length
                    )
                // This line seriously confuses Windows ACPI driver, so not even try to
                // enable SMC for Windows guests
                IRQNoFlags () {8}
            })
        }

        // Fields within PIIX3 configuration[0x60..0x63] with
        // IRQ mappings
        Field (\_SB.PCI0.SBRG.PCIC, ByteAcc, NoLock, Preserve)
        {
            Offset (0x60),
            PIRA,   8,
            PIRB,   8,
            PIRC,   8,
            PIRD,   8
        }

        Name (BUFA, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared) {15}
        })
        CreateWordField (BUFA, 0x01, ICRS)

        // Generic status of IRQ routing entry
        Method (LSTA, 1, NotSerialized)
        {
            And (Arg0, 0x80, Local0)
//            DBG ("LSTA: ")
//            HEX (Arg0)
            If (Local0)
            {
                Return (0x09)
            }
            Else
            {
                Return (0x0B)
            }
        }

        // Generic "current resource settings" for routing entry
        Method (LCRS, 1, NotSerialized)
        {
            And (Arg0, 0x0F, Local0)
            ShiftLeft (0x01, Local0, ICRS)
//            DBG ("LCRS: ")
//            HEX (ICRS)
            Return (BUFA)
        }

        // Generic "set resource settings" for routing entry
        Method (LSRS, 1, NotSerialized)
        {
            CreateWordField (Arg0, 0x01, ISRS)
            FindSetRightBit (ISRS, Local0)
            Return (Decrement (Local0))
        }

        // Generic "disable" for routing entry
        Method (LDIS, 1, NotSerialized)
        {
            Return (Or (Arg0, 0x80))
        }

        // Link A
        Device (LNKA)
        {
            Name (_HID, EisaId ("PNP0C0F"))
            Name (_UID, 0x01)

            // Status
            Method (_STA, 0, NotSerialized)
            {
                DBG ("LNKA._STA\n")
                Return (LSTA (PIRA))
            }

            // Possible resource settings
            Method (_PRS, 0, NotSerialized)
            {
                DBG ("LNKA._PRS\n")
                Return (PRSA)
            }

            // Disable
            Method (_DIS, 0, NotSerialized)
            {
                DBG ("LNKA._DIS\n")
                Store (LDIS (PIRA), PIRA)
            }

            // Current resource settings
            Method (_CRS, 0, NotSerialized)
            {
                DBG ("LNKA._CRS\n")
                Return (LCRS (PIRA))
            }

            // Set resource settings
            Method (_SRS, 1, NotSerialized)
            {
                DBG ("LNKA._SRS: ")
                HEX (LSRS (Arg0))
                Store (LSRS (Arg0), PIRA)
            }
        }

        // Link B
        Device (LNKB)
        {
            Name (_HID, EisaId ("PNP0C0F"))
            Name (_UID, 0x02)
            Method (_STA, 0, NotSerialized)
            {
                // DBG ("LNKB._STA\n")
                Return (LSTA (PIRB))
            }

            Method (_PRS, 0, NotSerialized)
            {
                // DBG ("LNKB._PRS\n")
                Return (PRSB)
            }

            Method (_DIS, 0, NotSerialized)
            {
                // DBG ("LNKB._DIS\n")
                Store (LDIS (PIRB), PIRB)
            }

            Method (_CRS, 0, NotSerialized)
            {
                // DBG ("LNKB._CRS\n")
                Return (LCRS (PIRB))
            }

            Method (_SRS, 1, NotSerialized)
            {
                DBG ("LNKB._SRS: ")
                HEX (LSRS (Arg0))
                Store (LSRS (Arg0), PIRB)
            }
        }

        // Link C
        Device (LNKC)
        {
            Name (_HID, EisaId ("PNP0C0F"))
            Name (_UID, 0x03)
            Method (_STA, 0, NotSerialized)
            {
                // DBG ("LNKC._STA\n")
                Return (LSTA (PIRC))
            }

            Method (_PRS, 0, NotSerialized)
            {
                // DBG ("LNKC._PRS\n")
                Return (PRSC)
            }

            Method (_DIS, 0, NotSerialized)
            {
                // DBG ("LNKC._DIS\n")
                Store (LDIS (PIRC), PIRC)
            }

            Method (_CRS, 0, NotSerialized)
            {
                // DBG ("LNKC._CRS\n")
                Return (LCRS (PIRC))
            }

            Method (_SRS, 1, NotSerialized)
            {
                DBG ("LNKC._SRS: ")
                HEX (LSRS (Arg0))
                Store (LSRS (Arg0), PIRC)
            }
        }

        // Link D
        Device (LNKD)
        {
            Name (_HID, EisaId ("PNP0C0F"))
            Name (_UID, 0x04)
            Method (_STA, 0, NotSerialized)
            {
                // DBG ("LNKD._STA\n")
                Return (LSTA (PIRD))
            }

            Method (_PRS, 0, NotSerialized)
            {
                // DBG ("LNKD._PRS\n")
                Return (PRSD)
            }

            Method (_DIS, 0, NotSerialized)
            {
                // DBG ("LNKD._DIS\n")
                Store (LDIS (PIRA), PIRD)
            }

            Method (_CRS, 0, NotSerialized)
            {
                // DBG ("LNKD._CRS\n")
                Return (LCRS (PIRD))
            }

            Method (_SRS, 1, NotSerialized)
            {
                DBG ("LNKD._SRS: ")
                HEX (LSRS (Arg0))
                Store (LSRS (Arg0), PIRD)
            }
        }
    }

    // Sx states
    Name (_S0, Package (2) {
        0x00,
        0x00,
    })

    Name (_S5, Package (2) {
        0x05,
        0x05,
    })

    Method (_PTS, 1, NotSerialized)
    {
        DBG ("Prepare to sleep: ")
        HEX (Arg0)
    }
}

/*
 * Local Variables:
 * comment-start: "//"
 * End:
 */
