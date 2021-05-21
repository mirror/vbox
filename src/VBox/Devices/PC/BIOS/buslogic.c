/* $Id$ */
/** @file
 * BusLogic SCSI host adapter driver to boot from disks.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "ebda.h"
#include "inlines.h"
#include "pciutil.h"
#include "vds.h"
#include "scsi.h"

//#define DEBUG_BUSLOGIC 1
#if DEBUG_BUSLOGIC
# define DBG_BUSLOGIC(...)        BX_INFO(__VA_ARGS__)
#else
# define DBG_BUSLOGIC(...)
#endif

#define BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND        0x02
#define BUSLOGICCOMMAND_INITIALIZE_EXTENDED_MAILBOX    0x81
#define BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT 0x25


#define RT_BIT(bit) (1 << (bit))

/** Register offsets in the I/O port space. */
#define BUSLOGIC_REGISTER_CONTROL   0 /**< Writeonly */
/** Fields for the control register. */
# define BL_CTRL_RSBUS  RT_BIT(4)   /* Reset SCSI Bus. */
# define BL_CTRL_RINT   RT_BIT(5)   /* Reset Interrupt. */
# define BL_CTRL_RSOFT  RT_BIT(6)   /* Soft Reset. */
# define BL_CTRL_RHARD  RT_BIT(7)   /* Hard Reset. */

#define BUSLOGIC_REGISTER_STATUS    0 /**< Readonly */
/** Fields for the status register. */
# define BL_STAT_CMDINV RT_BIT(0)   /* Command Invalid. */
# define BL_STAT_DIRRDY RT_BIT(2)   /* Data In Register Ready. */
# define BL_STAT_CPRBSY RT_BIT(3)   /* Command/Parameter Out Register Busy. */
# define BL_STAT_HARDY  RT_BIT(4)   /* Host Adapter Ready. */
# define BL_STAT_INREQ  RT_BIT(5)   /* Initialization Required. */
# define BL_STAT_DFAIL  RT_BIT(6)   /* Diagnostic Failure. */
# define BL_STAT_DACT   RT_BIT(7)   /* Diagnistic Active. */

#define BUSLOGIC_REGISTER_COMMAND   1 /**< Writeonly */
#define BUSLOGIC_REGISTER_DATAIN    1 /**< Readonly */
#define BUSLOGIC_REGISTER_INTERRUPT 2 /**< Readonly */
/** Fields for the interrupt register. */
# define BL_INTR_IMBL   RT_BIT(0)   /* Incoming Mailbox Loaded. */
# define BL_INTR_OMBR   RT_BIT(1)   /* Outgoing Mailbox Available. */
# define BL_INTR_CMDC   RT_BIT(2)   /* Command Complete. */
# define BL_INTR_RSTS   RT_BIT(3)   /* SCSI Bus Reset State. */
# define BL_INTR_INTV   RT_BIT(7)   /* Interrupt Valid. */

/** Structure for the INITIALIZE EXTENDED MAILBOX request. */
#pragma pack(1)
typedef struct ReqInitExtMbx
{
    /** Number of mailboxes in guest memory. */
    uint8_t  cMailbox;
    /** Physical address of the first mailbox. */
    uint32_t uMailboxBaseAddress;
} ReqInitExtMbx;
#pragma pack()

/**
 * Structure of a mailbox in guest memory.
 * The incoming and outgoing mailbox have the same size
 * but the incoming one has some more fields defined which
 * are marked as reserved in the outgoing one.
 * The last field is also different from the type.
 * For outgoing mailboxes it is the action and
 * for incoming ones the completion status code for the task.
 * We use one structure for both types.
 */
typedef struct Mailbox32
{
    /** Physical address of the CCB structure in the guest memory. */
    volatile uint32_t u32PhysAddrCCB;
    /** Type specific data. */
    union
    {
        /** For outgoing mailboxes. */
        struct
        {
            /** Reserved */
            uint8_t uReserved[3];
            /** Action code. */
            uint8_t uActionCode;
        } out;
        /** For incoming mailboxes. */
        struct
        {
            /** The host adapter status after finishing the request. */
            volatile uint8_t  uHostAdapterStatus;
            /** The status of the device which executed the request after executing it. */
            volatile uint8_t  uTargetDeviceStatus;
            /** Reserved. */
            volatile uint8_t  uReserved;
            /** The completion status code of the request. */
            volatile uint8_t uCompletionCode;
        } in;
    } u;
} Mailbox32, *PMailbox32;

/**
 * Action codes for outgoing mailboxes.
 */
enum BUSLOGIC_MAILBOX_OUTGOING_ACTION
{
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_FREE = 0x00,
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_START_COMMAND = 0x01,
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_ABORT_COMMAND = 0x02
};

/**
 * Completion codes for incoming mailboxes.
 */
enum BUSLOGIC_MAILBOX_INCOMING_COMPLETION
{
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_FREE = 0x00,
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITHOUT_ERROR = 0x01,
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_ABORTED = 0x02,
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_ABORTED_NOT_FOUND = 0x03,
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITH_ERROR = 0x04,
    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_INVALID_CCB = 0x05
};

/**
 * Host adapter status for incoming mailboxes.
 */
enum BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS
{
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_CMD_COMPLETED = 0x00,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_LINKED_CMD_COMPLETED = 0x0a,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_LINKED_CMD_COMPLETED_WITH_FLAG = 0x0b,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_DATA_UNDERUN = 0x0c,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_SCSI_SELECTION_TIMEOUT = 0x11,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_DATA_OVERRUN = 0x12,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_UNEXPECTED_BUS_FREE = 0x13,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_INVALID_BUS_PHASE_REQUESTED = 0x14,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_INVALID_OUTGOING_MAILBOX_ACTION_CODE = 0x15,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_INVALID_COMMAND_OPERATION_CODE = 0x16,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_LINKED_CCB_HAS_INVALID_LUN = 0x17,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_INVALID_COMMAND_PARAMETER = 0x1a,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_AUTO_REQUEST_SENSE_FAILED = 0x1b,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_TAGGED_QUEUING_MESSAGE_REJECTED = 0x1c,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_UNSUPPORTED_MESSAGE_RECEIVED = 0x1d,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_HOST_ADAPTER_HARDWARE_FAILED = 0x20,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_TARGET_FAILED_RESPONSE_TO_ATN = 0x21,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_HOST_ADAPTER_ASSERTED_RST = 0x22,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_OTHER_DEVICE_ASSERTED_RST = 0x23,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_TARGET_DEVICE_RECONNECTED_IMPROPERLY = 0x24,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_HOST_ADAPTER_ASSERTED_BUS_DEVICE_RESET = 0x25,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_ABORT_QUEUE_GENERATED = 0x26,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_HOST_ADAPTER_SOFTWARE_ERROR = 0x27,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_HOST_ADAPTER_HARDWARE_TIMEOUT_ERROR = 0x30,
    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_SCSI_PARITY_ERROR_DETECTED = 0x34
};

/**
 * Device status codes for incoming mailboxes.
 */
enum BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS
{
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_OPERATION_GOOD = 0x00,
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_CHECK_CONDITION = 0x02,
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_DEVICE_BUSY = 0x08
};

/**
 * Opcode types for CCB.
 */
enum BUSLOGIC_CCB_OPCODE
{
    BUSLOGIC_CCB_OPCODE_INITIATOR_CCB = 0x00,
    BUSLOGIC_CCB_OPCODE_TARGET_CCB = 0x01,
    BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_SCATTER_GATHER = 0x02,
    BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_RESIDUAL_DATA_LENGTH = 0x03,
    BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_RESIDUAL_SCATTER_GATHER = 0x04,
    BUSLOGIC_CCB_OPCODE_BUS_DEVICE_RESET = 0x81
};

/**
 * Data transfer direction.
 */
enum BUSLOGIC_CCB_DIRECTION
{
    BUSLOGIC_CCB_DIRECTION_UNKNOWN = 0x00,
    BUSLOGIC_CCB_DIRECTION_IN      = 0x01,
    BUSLOGIC_CCB_DIRECTION_OUT     = 0x02,
    BUSLOGIC_CCB_DIRECTION_NO_DATA = 0x03
};

/**
 * The command control block for a SCSI request.
 */
typedef struct CCB32
{
    /** Opcode. */
    uint8_t       uOpcode;
    /** Reserved */
    unsigned char uReserved1 :      3;
    /** Data direction for the request. */
    unsigned char uDataDirection :  2;
    /** Whether the request is tag queued. */
    unsigned char fTagQueued :      1;
    /** Queue tag mode. */
    unsigned char uQueueTag :       2;
    /** Length of the SCSI CDB. */
    uint8_t       cbCDB;
    /** Sense data length. */
    uint8_t       cbSenseData;
    /** Data length. */
    uint32_t      cbData;
    /** Data pointer.
     *  This points to the data region or a scatter gather list based on the opcode.
     */
    uint32_t      u32PhysAddrData;
    /** Reserved. */
    uint8_t       uReserved2[2];
    /** Host adapter status. */
    uint8_t       uHostAdapterStatus;
    /** Device adapter status. */
    uint8_t       uDeviceStatus;
    /** The device the request is sent to. */
    uint8_t       uTargetId;
    /**The LUN in the device. */
    unsigned char uLogicalUnit : 5;
    /** Legacy tag. */
    unsigned char fLegacyTagEnable : 1;
    /** Legacy queue tag. */
    unsigned char uLegacyQueueTag : 2;
    /** The SCSI CDB.  (A CDB can be 12 bytes long.) */
    uint8_t       abCDB[12];
    /** Reserved. */
    uint8_t       uReserved3[6];
    /** Sense data pointer. */
    uint32_t      u32PhysAddrSenseData;
} CCB32, *PCCB32;

/** 32-bit scatter-gather list entry. */
typedef struct SGE32
{
    uint32_t   cbSegment;
    uint32_t   u32PhysAddrSegmentBase;
} SGE32, *PSGE32;


/**
 * BusLogic-SCSI controller data.
 */
typedef struct
{
    /** Outgoing mailbox - must come first because of alignment reasons. */
    Mailbox32        MbxOut32;
    /** Incoming mailbox. */
    Mailbox32        MbxIn32;
    /** Command control block. */
    CCB32            Ccb32;
    /** List of scatter gather entries. */
    SGE32            aSge[3];
    /** I/O base of device. */
    uint16_t         u16IoBase;
    /** The sink buf. */
    void __far       *pvSinkBuf;
} buslogic_t;

/* The BusLogic specific data must fit into 1KB (statically allocated). */
ct_assert(sizeof(buslogic_t) <= 1024);

/**
 * Converts a segment:offset pair into a 32bit physical address.
 */
static uint32_t buslogic_addr_to_phys(void __far *ptr)
{
    return ((uint32_t)FP_SEG(ptr) << 4) + FP_OFF(ptr);
}

static int buslogic_cmd(buslogic_t __far *buslogic, uint8_t uCmd, uint8_t __far *pbReq, uint16_t cbReq,
                        uint8_t __far *pbReply, uint16_t cbReply)
{
    uint16_t i;

    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_COMMAND, uCmd);
    for (i = 0; i < cbReq; i++)
        outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_COMMAND, *pbReq++);

    /* Wait for the HBA to finish processing the command. */
    if (cbReply)
    {
        while (!(inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_STATUS) & BL_STAT_DIRRDY));
        for (i = 0; i < cbReply; i++)
            *pbReply++ = inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_DATAIN);
    }

    while (!(inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_STATUS) & BL_STAT_HARDY));

    /* Clear interrupt status. */
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_CONTROL, BL_CTRL_RINT);

    return 0;
}

int buslogic_scsi_cmd_data_out(void __far *pvHba, uint8_t idTgt, uint8_t __far *aCDB,
                               uint8_t cbCDB, uint8_t __far *buffer, uint32_t length)
{
    buslogic_t __far *buslogic = (buslogic_t __far *)pvHba;
    int i;

    buslogic->MbxIn32.u.in.uCompletionCode = BUSLOGIC_MAILBOX_INCOMING_COMPLETION_FREE;

    _fmemset(&buslogic->Ccb32, 0, sizeof(buslogic->Ccb32));

    buslogic->Ccb32.uOpcode         = BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_SCATTER_GATHER;
    buslogic->Ccb32.uDataDirection  = BUSLOGIC_CCB_DIRECTION_OUT;
    buslogic->Ccb32.cbCDB           = cbCDB;
    buslogic->Ccb32.cbSenseData     = 0;
    buslogic->Ccb32.cbData          = sizeof(buslogic->aSge[0]);
    buslogic->Ccb32.u32PhysAddrData = buslogic_addr_to_phys(&buslogic->aSge[0]);
    buslogic->Ccb32.uTargetId       = idTgt;
    for (i = 0; i < cbCDB; i++)
        buslogic->Ccb32.abCDB[i] = aCDB[i];

    buslogic->aSge[0].cbSegment = length;
    buslogic->aSge[0].u32PhysAddrSegmentBase = buslogic_addr_to_phys(buffer);

    /* Send it off. */
    buslogic->MbxOut32.u32PhysAddrCCB    = buslogic_addr_to_phys(&buslogic->Ccb32);
    buslogic->MbxOut32.u.out.uActionCode = BUSLOGIC_MAILBOX_OUTGOING_ACTION_START_COMMAND;
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_COMMAND, BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND);

    /* Wait for it to finish. */
    while (!(inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_INTERRUPT) & BL_INTR_IMBL));

    /* Clear interrupt status. */
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_CONTROL, BL_CTRL_RINT);

    /* Check mailbox status. */
    if (buslogic->MbxIn32.u.in.uCompletionCode != BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITHOUT_ERROR)
        return 4;

    return 0;
}

int buslogic_scsi_cmd_data_in(void __far *pvHba, uint8_t idTgt, uint8_t __far *aCDB,
                            uint8_t cbCDB, uint8_t __far *buffer, uint32_t length, uint16_t skip_b,
                            uint16_t skip_a)
{
    buslogic_t __far *buslogic = (buslogic_t __far *)pvHba;
    int i;
    uint8_t idxSge = 0;

    buslogic->MbxIn32.u.in.uCompletionCode = BUSLOGIC_MAILBOX_INCOMING_COMPLETION_FREE;

    _fmemset(&buslogic->Ccb32, 0, sizeof(buslogic->Ccb32));

    buslogic->Ccb32.uOpcode         = BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_SCATTER_GATHER;
    buslogic->Ccb32.uDataDirection  = BUSLOGIC_CCB_DIRECTION_IN;
    buslogic->Ccb32.cbCDB           = cbCDB;
    buslogic->Ccb32.cbSenseData     = 0;
    buslogic->Ccb32.u32PhysAddrData = buslogic_addr_to_phys(&buslogic->aSge[0]);
    buslogic->Ccb32.uTargetId       = idTgt;
    for (i = 0; i < cbCDB; i++)
        buslogic->Ccb32.abCDB[i] = aCDB[i];

    /* Prepend a sinkhole if data is skipped upfront. */
    if (skip_b)
    {
        buslogic->aSge[idxSge].cbSegment = skip_b;
        buslogic->aSge[idxSge].u32PhysAddrSegmentBase = buslogic_addr_to_phys(buslogic->pvSinkBuf);
        idxSge++;
    }

    buslogic->aSge[idxSge].cbSegment = length;
    buslogic->aSge[idxSge].u32PhysAddrSegmentBase = buslogic_addr_to_phys(buffer);
    idxSge++;

    /* Append a sinkhole if data is skipped at the end. */
    if (skip_a)
    {
        buslogic->aSge[idxSge].cbSegment = skip_a;
        buslogic->aSge[idxSge].u32PhysAddrSegmentBase = buslogic_addr_to_phys(buslogic->pvSinkBuf);
        idxSge++;
    }

    buslogic->Ccb32.cbData = idxSge * sizeof(buslogic->aSge[0]);

    /* Send it off. */
    buslogic->MbxOut32.u32PhysAddrCCB    = buslogic_addr_to_phys(&buslogic->Ccb32);
    buslogic->MbxOut32.u.out.uActionCode = BUSLOGIC_MAILBOX_OUTGOING_ACTION_START_COMMAND;
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_COMMAND, BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND);

    /* Wait for it to finish. */
    while (!(inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_INTERRUPT) & BL_INTR_IMBL));

    /* Clear interrupt status. */
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_CONTROL, BL_CTRL_RINT);

    /* Check mailbox status. */
    if (buslogic->MbxIn32.u.in.uCompletionCode != BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITHOUT_ERROR)
        return 4;

    return 0;
}

/**
 * Initializes the BusLogic SCSI HBA and detects attached devices.
 */
static int buslogic_scsi_hba_init(buslogic_t __far *buslogic)
{
    int rc;
    uint8_t bIrqOff = 0;
    ReqInitExtMbx       ReqInitMbx;

    /* Hard reset. */
    outb(buslogic->u16IoBase + BUSLOGIC_REGISTER_CONTROL, BL_CTRL_RHARD);
    while (!(inb(buslogic->u16IoBase + BUSLOGIC_REGISTER_STATUS) & BL_STAT_HARDY));

    /* Disable interrupts. */
    rc = buslogic_cmd(buslogic, BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT,
                      (unsigned char __far *)&bIrqOff, sizeof(bIrqOff),
                      NULL, 0);
    if (!rc)
    {
        /* Initialize mailbox. */
        ReqInitMbx.cMailbox = 1;
        ReqInitMbx.uMailboxBaseAddress = buslogic_addr_to_phys(&buslogic->MbxOut32);
        rc = buslogic_cmd(buslogic, BUSLOGICCOMMAND_INITIALIZE_EXTENDED_MAILBOX,
                          (unsigned char __far *)&ReqInitMbx, sizeof(ReqInitMbx),
                          NULL, 0);
    }

    return rc;
}

/**
 * Init the BusLogic SCSI driver and detect attached disks.
 */
int buslogic_scsi_init(void __far *pvHba, void __far *pvSinkBuf, uint8_t u8Bus, uint8_t u8DevFn)
{
    buslogic_t __far *buslogic = (buslogic_t __far *)pvHba;
    uint32_t u32Bar;

    DBG_BUSLOGIC("BusLogic SCSI HBA at Bus %u DevFn 0x%x (raw 0x%x)\n", u8Bus, u8DevFn);

    u32Bar = pci_read_config_dword(u8Bus, u8DevFn, 0x10);

    DBG_BUSLOGIC("BAR at 0x10 : 0x%x\n", u32Bar);

    if ((u32Bar & 0x01) != 0)
    {
        uint16_t u16IoBase = (u32Bar & 0xfff0);

        /* Enable PCI memory, I/O, bus mastering access in command register. */
        pci_write_config_word(u8Bus, u8DevFn, 4, 0x7);

        DBG_BUSLOGIC("I/O base: 0x%x\n", u16IoBase);
        buslogic->u16IoBase = u16IoBase;
        buslogic->pvSinkBuf = pvSinkBuf;
        return buslogic_scsi_hba_init(buslogic);
    }
    else
        DBG_BUSLOGIC("BAR is MMIO\n");

    return 1;
}
