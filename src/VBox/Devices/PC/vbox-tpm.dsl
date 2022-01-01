/* $Id$ */
/** @file
 * VirtualBox ACPI - TPM ACPI device.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

DefinitionBlock ("SSDT.aml", "SSDT", 1, "VBOX  ", "VBOXTPMT", 2)
{
    Scope (\_SB)
    {
        Device (TPM)
        {
            Method (_HID, 0, NotSerialized)
            {
                If (LEqual(IFID, One))
                {
                    Return ("PNP0C31")
                }
                Else
                {
                    Return ("MSFT0101")
                }
            }

            Method (_CID, 0, NotSerialized)
            {
                If (LEqual(IFID, One))
                {
                    Return ("PNP0C31")
                }
                Else
                {
                    Return ("MSFT0101")
                }
            }

            Method (_STR, 0, NotSerialized)
            {
                If (LEqual(IFID, One))
                {
                    Return (Unicode ("TPM 1.2 Device"))
                }
                Else
                {
                    Return (Unicode ("TPM 2.0 Device"))
                }
            }

            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

            OperationRegion (TPMR, SystemMemory, 0xFED40000, 0x5000)
            Field(TPMR, AnyAcc, NoLock, Preserve)
            {
                Offset(0x30),
                IFID,       1,
            }

            Name(RES, ResourceTemplate()
            {
                Memory32Fixed (ReadWrite, 0xfed40000, 0x5000, REG1)
            })

            Method (_CRS, 0, Serialized)
            {
               Return (RES)
            }
        }
    }
}

/*
 * Local Variables:
 * comment-start: "//"
 * End:
 */

