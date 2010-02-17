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

#define GENERATE_CPU_OBJECT(id, sck, sckuid, cpu, cpuuid)                  \
    Device (sck)                                                           \
    {                                                                      \
        Name (_HID, "ACPI0004")                                            \
        Name (_UID, sckuid)                                                \
                                                                           \
        Method (_STA, 0, NotSerialized)                                    \
        {                                                                  \
            IF (CPCK(id))                                                  \
            {                                                              \
                Return (0xF)                                               \
            }                                                              \
            Else                                                           \
            {                                                              \
                Return (0x0)                                               \
            }                                                              \
        }                                                                  \
                                                                           \
        Processor (cpu, /* Name */                                         \
                   id,  /* Id */                                           \
                   0x0, /* Processor IO ports range start */               \
                   0x0  /* Processor IO ports range length */              \
                   )                                                       \
        {                                                                  \
            Name (_HID, "ACPI0007")                                        \
            Name (_UID, cpuuid)                                            \
            Name (_PXM, 0x00)                                              \
                                                                           \
            Method(_MAT, 0)                                                \
            {                                                              \
                IF (CPCK(id))                                              \
                {                                                          \
                    Name (APIC, Buffer (8) {0x00, 0x08, id, id, 0x01})     \
                    Return(APIC)                                           \
                }                                                          \
                Else                                                       \
                {                                                          \
                    Return (0x00)                                          \
                }                                                          \
            }                                                              \
            Method(_STA) /* Used for device presence detection */          \
            {                                                              \
                IF (CPCK(id))                                              \
                {                                                          \
                    Return (0xF)                                           \
                }                                                          \
                Else                                                       \
                {                                                          \
                    Return (0x0)                                           \
                }                                                          \
            }                                                              \
            Method(_EJ0, 1)                                                \
            {                                                              \
                Store(id, \_SB.CPUL) /* Unlock the CPU */                  \
                Return                                                     \
            }                                                              \
        }                                                                  \
    }                                                                      \

        GENERATE_CPU_OBJECT(0x00, SCK0, "SCKCPU0", CPU0, "SCK0-CPU0")
        GENERATE_CPU_OBJECT(0x01, SCK1, "SCKCPU1", CPU1, "SCK1-CPU0")
        GENERATE_CPU_OBJECT(0x02, SCK2, "SCKCPU2", CPU2, "SCK2-CPU0")
        GENERATE_CPU_OBJECT(0x03, SCK3, "SCKCPU3", CPU3, "SCK3-CPU0")
        GENERATE_CPU_OBJECT(0x04, SCK4, "SCKCPU4", CPU4, "SCK4-CPU0")
        GENERATE_CPU_OBJECT(0x05, SCK5, "SCKCPU5", CPU5, "SCK5-CPU0")
        GENERATE_CPU_OBJECT(0x06, SCK6, "SCKCPU6", CPU6, "SCK6-CPU0")
        GENERATE_CPU_OBJECT(0x07, SCK7, "SCKCPU7", CPU7, "SCK7-CPU0")
        GENERATE_CPU_OBJECT(0x08, SCK8, "SCKCPU8", CPU8, "SCK8-CPU0")
        GENERATE_CPU_OBJECT(0x09, SCK9, "SCKCPU9", CPU9, "SCK9-CPU0")
        GENERATE_CPU_OBJECT(0x0a, SCKA, "SCKCPUA", CPUA, "SCKA-CPU0")
        GENERATE_CPU_OBJECT(0x0b, SCKB, "SCKCPUB", CPUB, "SCKB-CPU0")
        GENERATE_CPU_OBJECT(0x0c, SCKC, "SCKCPUC", CPUC, "SCKC-CPU0")
        GENERATE_CPU_OBJECT(0x0d, SCKD, "SCKCPUD", CPUD, "SCKD-CPU0")
        GENERATE_CPU_OBJECT(0x0e, SCKE, "SCKCPUE", CPUE, "SCKE-CPU0")
        GENERATE_CPU_OBJECT(0x0f, SCKF, "SCKCPUF", CPUF, "SCKF-CPU0")
        GENERATE_CPU_OBJECT(0x10, SCKG, "SCKCPUG", CPUG, "SCKG-CPU0")
        GENERATE_CPU_OBJECT(0x11, SCKH, "SCKCPUH", CPUH, "SCKH-CPU0")
        GENERATE_CPU_OBJECT(0x12, SCKI, "SCKCPUI", CPUI, "SCKI-CPU0")
        GENERATE_CPU_OBJECT(0x13, SCKJ, "SCKCPUJ", CPUJ, "SCKJ-CPU0")
        GENERATE_CPU_OBJECT(0x14, SCKK, "SCKCPUK", CPUK, "SCKK-CPU0")
        GENERATE_CPU_OBJECT(0x15, SCKL, "SCKCPUL", CPUL, "SCKL-CPU0")
        GENERATE_CPU_OBJECT(0x16, SCKM, "SCKCPUM", CPUM, "SCKM-CPU0")
        GENERATE_CPU_OBJECT(0x17, SCKN, "SCKCPUN", CPUN, "SCKN-CPU0")
        GENERATE_CPU_OBJECT(0x18, SCKO, "SCKCPUO", CPUO, "SCKO-CPU0")
        GENERATE_CPU_OBJECT(0x19, SCKP, "SCKCPUP", CPUP, "SCKP-CPU0")
        GENERATE_CPU_OBJECT(0x1a, SCKQ, "SCKCPUQ", CPUQ, "SCKQ-CPU0")
        GENERATE_CPU_OBJECT(0x1b, SCKR, "SCKCPUR", CPUR, "SCKR-CPU0")
        GENERATE_CPU_OBJECT(0x1c, SCKS, "SCKCPUS", CPUS, "SCKS-CPU0")
        GENERATE_CPU_OBJECT(0x1d, SCKT, "SCKCPUT", CPUT, "SCKT-CPU0")
        GENERATE_CPU_OBJECT(0x1e, SCKU, "SCKCPUU", CPUU, "SCKU-CPU0")
        GENERATE_CPU_OBJECT(0x1f, SCKV, "SCKCPUV", CPUV, "SCKV-CPU0")

#undef GENERATE_CPU_OBJECT
    }

    Scope (\_GPE)
    {

#define CHECK_CPU(cpu, sck)    \
    IF (CPCK(cpu))             \
    {                          \
        Notify (\_SB.sck, 0x1) \
    }                          \

        // GPE bit 1 handler
        // GPE.1 must be set and SCI raised when
        // processor info changed and CPU must be
        // re-evaluated
        Method (_L01, 0, NotSerialized)
        {
            /*Store(\_SB.CPET, Local0)*/
            /*Store(\_SB.CPEV, Local1)*/

            CHECK_CPU(0x01, SCK1)
            CHECK_CPU(0x02, SCK2)
            CHECK_CPU(0x03, SCK3)
            CHECK_CPU(0x04, SCK4)
            CHECK_CPU(0x05, SCK5)
            CHECK_CPU(0x06, SCK6)
            CHECK_CPU(0x07, SCK7)
            CHECK_CPU(0x08, SCK8)
            CHECK_CPU(0x09, SCK9)
            CHECK_CPU(0x0a, SCKA)
            CHECK_CPU(0x0b, SCKB)
            CHECK_CPU(0x0c, SCKC)
            CHECK_CPU(0x0d, SCKD)
            CHECK_CPU(0x0e, SCKE)
            CHECK_CPU(0x0f, SCKF)
            CHECK_CPU(0x10, SCKG)
            CHECK_CPU(0x11, SCKH)
            CHECK_CPU(0x12, SCKI)
            CHECK_CPU(0x13, SCKJ)
            CHECK_CPU(0x14, SCKK)
            CHECK_CPU(0x15, SCKL)
            CHECK_CPU(0x16, SCKM)
            CHECK_CPU(0x17, SCKN)
            CHECK_CPU(0x18, SCKO)
            CHECK_CPU(0x19, SCKP)
            CHECK_CPU(0x1a, SCKQ)
            CHECK_CPU(0x1b, SCKR)
            CHECK_CPU(0x1c, SCKS)
            CHECK_CPU(0x1d, SCKT)
            CHECK_CPU(0x1e, SCKU)
            CHECK_CPU(0x1f, SCKV)
        }

#undef CHECK_CPU
    }

}

/*
 * Local Variables:
 * comment-start: "//"
 * End:
 */
