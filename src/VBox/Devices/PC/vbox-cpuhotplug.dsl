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
// General Public License (GPL) as published by the Free Software
// Foundation, in version 2 as it comes in the "COPYING" file of the
// VirtualBox OSE distribution. VirtualBox OSE is distributed in the
// hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
//
// Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
// Clara, CA 95054 USA or visit http://www.sun.com if you need
// additional information or have any questions.

DefinitionBlock ("SSDT-cpuhotplug.aml", "SSDT", 1, "VBOX  ", "VBOXCPUT", 2)
{
    External(\_SB.CPUC)
    External(\_SB.CPUL)

    // Method to check for the CPU status
    Method(CPCK, 1)
    {
        Store (Arg0, \_SB.CPUC)
        Return(LEqual(\_SB.CPUL, 0x01))
    }

    Scope (\_SB)
    {
        Device (SCK0)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU0")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x00))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU0, /* Name */
                       0x00, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK0-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x00))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x00, 0x00, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x00))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }
                Method(_EJ0, 1)
                {
                    Store(0x0, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK1)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU1")

            Method (_STA, 0, NotSerialized)
            {
                Return (0xF)
            }

            Processor (CPU1, /* Name */
                       0x01, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK1-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x01))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x01, 0x01, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x01))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK2)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU2")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x02))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU2, /* Name */
                       0x02, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK2-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x02))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x02, 0x02, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x02))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x2, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK3)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU3")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x03))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU3, /* Name */
                       0x03, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK3-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x03))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x03, 0x03, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x03))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x3, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK4)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU4")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x04))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU4, /* Name */
                       0x04, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK4-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x04))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x04, 0x04, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x04))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x4, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK5)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU5")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x05))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU5, /* Name */
                       0x05, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK5-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x05))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x05, 0x05, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x05))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x5, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK6)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU6")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x06))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU6, /* Name */
                       0x06, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK6-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x6))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x06, 0x06, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x06))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x6, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK7)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU7")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x07))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU7, /* Name */
                       0x07, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK7-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x07))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x07, 0x07, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x07))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x7, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK8)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU8")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x08))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU8, /* Name */
                       0x08, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK4-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x08))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x08, 0x08, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x08))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x8, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCK9)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPU9")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x09))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPU9, /* Name */
                       0x09, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCK9-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x09))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x09, 0x09, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x09))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x9, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKA)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUA")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0a))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUA, /* Name */
                       0x0a, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKA-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0a))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0a, 0x0a, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0a))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x0a, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKB)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUB")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0b))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUB, /* Name */
                       0x0b, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKB-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0b))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0b, 0x0b, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0b))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x0b, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKC)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUC")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0c))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUC, /* Name */
                       0x0c, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKC-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0c))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0c, 0x0c, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0c))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x0c, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKD)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUD")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0d))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUD, /* Name */
                       0x0d, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKD-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0d))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0d, 0x0d, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0d))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x0d, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKE)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUE")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0e))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUE, /* Name */
                       0x0e, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKE-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0e))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0e, 0x0e, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0e))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x0e, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKF)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUF")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x0f))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUF, /* Name */
                       0x0f, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKF-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x0f))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x0f, 0x0f, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x0f))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0xf, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKG)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUG")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x10))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUG, /* Name */
                       0x10, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKG-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x10))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x10, 0x10, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x10))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x10, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKH)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUH")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x11))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUH, /* Name */
                       0x11, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKH-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x11))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x11, 0x11, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x11))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x11, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKI)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUI")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x12))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUI, /* Name */
                       0x12, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKI-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x12))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x12, 0x12, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x12))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x12, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKJ)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUJ")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x13))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUJ, /* Name */
                       0x13, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKJ-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x13))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x13, 0x13, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x13))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x13, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKK)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUK")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x14))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUK, /* Name */
                       0x14, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKK-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x14))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x14, 0x14, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x14))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x14, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKL)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUL")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x15))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUL, /* Name */
                       0x15, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKL-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x15))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x15, 0x15, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x15))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x15, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKM)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUM")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x16))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUM, /* Name */
                       0x16, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKM-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x16))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x16, 0x16, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x16))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x16, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKN)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUN")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x17))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUN, /* Name */
                       0x17, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKN-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x17))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x17, 0x17, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x17))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x17, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKO)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUO")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x18))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUO, /* Name */
                       0x18, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKO-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x18))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x18, 0x18, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x18))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x18, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKP)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUP")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x19))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUP, /* Name */
                       0x19, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKP-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x19))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x19, 0x19, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x19))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x19, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKQ)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUQ")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1a))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUQ, /* Name */
                       0x1a, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKQ-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1a))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1a, 0x1a, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1a))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1a, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKR)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUR")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1b))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUR, /* Name */
                       0x1b, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKR-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1b))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1b, 0x1b, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1b))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1b, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKS)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUS")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1c))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUS, /* Name */
                       0x1c, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKS-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1c))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1c, 0x1c, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1c))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1c, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKT)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUT")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1d))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUT, /* Name */
                       0x1d, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKT-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1d))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1d, 0x1d, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1d))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1d, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKU)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUU")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1e))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUU, /* Name */
                       0x1e, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKU-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1e))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1e, 0x1e, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1e))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1e, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }

        Device (SCKV)
        {
            Name (_HID, "ACPI0004")
            Name (_UID, "SCKCPUV")

            Method (_STA, 0, NotSerialized)
            {
                IF (CPCK(0x1f))
                {
                    Return (0xF)
                }
                Else
                {
                    Return (0x0)
                }
            }

            Processor (CPUV, /* Name */
                       0x1f, /* Id */
                       0x0,  /* Processor IO ports range start */
                       0x0   /* Processor IO ports range length */
                       )
            {
                Name (_HID, "ACPI0007")
                Name (_UID, "SCKV-CPU0")
                Name (_PXM, 0x00)

                Method(_MAT, 0)
                {
                    IF (CPCK(0x1f))
                    {
                        Name (APIC, Buffer (8) {0x00, 0x08, 0x1f, 0x1f, 0x01})
                        Return(APIC)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method(_STA) // Used for device presence detection
                {
                    IF (CPCK(0x1f))
                    {
                        Return (0xF)
                    }
                    Else
                    {
                        Return (0x0)
                    }
                }

                Method(_EJ0, 1)
                {
                    Store(0x1f, \_SB.CPUL) // Unlock the CPU
                    Return
                }
            }
        }
    }

    Scope (\_GPE)
    {
        // GPE bit 1 handler
        // GPE.1 must be set and SCI raised when
        // processor info changed and CPU1 must be
        // re-evaluated
        Method (_L01, 0, NotSerialized)
        {
            // Eject notifications from ACPI are not supported so far by any guest
            //IF (And(\_SB.CPUD, 0x2))
            //{
            //    Notify(\_PR.SCK1, 0x3)
            //}

            IF (CPCK(0x01))
            {
                Notify (\_SB.SCK1, 0x1)
            }
            IF (CPCK(0x02))
            {
                Notify (\_SB.SCK2, 0x1)
            }
            IF (CPCK(0x03))
            {
                Notify (\_SB.SCK3, 0x1)
            }
            IF (CPCK(0x04))
            {
                Notify (\_SB.SCK4, 0x1)
            }
            IF (CPCK(0x05))
            {
                Notify (\_SB.SCK5, 0x1)
            }
            IF (CPCK(0x06))
            {
                Notify (\_SB.SCK6, 0x1)
            }
            IF (CPCK(0x07))
            {
                Notify (\_SB.SCK7, 0x1)
            }
            IF (CPCK(0x08))
            {
                Notify (\_SB.SCK8, 0x1)
            }
            IF (CPCK(0x09))
            {
                Notify (\_SB.SCK9, 0x1)
            }
            IF (CPCK(0x0a))
            {
                Notify (\_SB.SCKA, 0x1)
            }
            IF (CPCK(0x0b))
            {
                Notify (\_SB.SCKB, 0x1)
            }
            IF (CPCK(0x0c))
            {
                Notify (\_SB.SCKC, 0x1)
            }
            IF (CPCK(0x0d))
            {
                Notify (\_SB.SCKD, 0x1)
            }
            IF (CPCK(0x0e))
            {
                Notify (\_SB.SCKE, 0x1)
            }
            IF (CPCK(0x0f))
            {
                Notify (\_SB.SCKF, 0x1)
            }
            IF (CPCK(0x10))
            {
                Notify (\_SB.SCKG, 0x1)
            }
            IF (CPCK(0x11))
            {
                Notify (\_SB.SCKH, 0x1)
            }
            IF (CPCK(0x12))
            {
                Notify (\_SB.SCKI, 0x1)
            }
            IF (CPCK(0x13))
            {
                Notify (\_SB.SCKJ, 0x1)
            }
            IF (CPCK(0x14))
            {
                Notify (\_SB.SCKK, 0x1)
            }
            IF (CPCK(0x15))
            {
                Notify (\_SB.SCKL, 0x1)
            }
            IF (CPCK(0x16))
            {
                Notify (\_SB.SCKM, 0x1)
            }
            IF (CPCK(0x17))
            {
                Notify (\_SB.SCKN, 0x1)
            }
            IF (CPCK(0x18))
            {
                Notify (\_SB.SCKO, 0x1)
            }
            IF (CPCK(0x19))
            {
                Notify (\_SB.SCKP, 0x1)
            }
            IF (CPCK(0x1a))
            {
                Notify (\_SB.SCKQ, 0x1)
            }
            IF (CPCK(0x1b))
            {
                Notify (\_SB.SCKR, 0x1)
            }
            IF (CPCK(0x1c))
            {
                Notify (\_SB.SCKS, 0x1)
            }
            IF (CPCK(0x1d))
            {
                Notify (\_SB.SCKT, 0x1)
            }
            IF (CPCK(0x1e))
            {
                Notify (\_SB.SCKU, 0x1)
            }
            IF (CPCK(0x1f))
            {
                Notify (\_SB.SCKV, 0x1)
            }
        }
    }

}

/*
 * Local Variables:
 * comment-start: "//"
 * End:
 */
