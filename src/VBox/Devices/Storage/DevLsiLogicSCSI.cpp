/* $Id$ */
/** @file
 *
 * VBox storage devices:
 * LsiLogic LSI53c1030 SCSI controller.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DEV_LSILOGICSCSI
#include <VBox/pdmdev.h>
#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/param.h>
# include <iprt/alloc.h>
# include <iprt/cache.h>
#endif

#include "VBoxSCSI.h"

#include "../Builtins.h"

/* I/O port registered in the ISA compatible range to let the BIOS access
 * the controller.
 */
#define LSILOGIC_ISA_IO_PORT 0x340

#define LSILOGIC_PORTS_MAX 1
#define LSILOGIC_BUSES_MAX 1
#define LSILOGIC_DEVICES_PER_BUS_MAX 16

#define LSILOGIC_DEVICES_MAX (LSILOGIC_BUSES_MAX*LSILOGIC_DEVICES_PER_BUS_MAX)

#define LSILOGICSCSI_REQUEST_QUEUE_DEPTH_DEFAULT 1024
#define LSILOGICSCSI_REPLY_QUEUE_DEPTH_DEFAULT   128

#define LSILOGICSCSI_MAXIMUM_CHAIN_DEPTH 3

#define LSILOGIC_NR_OF_ALLOWED_BIGGER_LISTS 100

#define LSILOGICSCSI_PCI_VENDOR_ID            (0x1000)
#define LSILOGICSCSI_PCI_DEVICE_ID            (0x0030)
#define LSILOGICSCSI_PCI_REVISION_ID          (0x00)
#define LSILOGICSCSI_PCI_CLASS_CODE           (0x01)
#define LSILOGICSCSI_PCI_SUBSYSTEM_VENDOR_ID  (0x1000)
#define LSILOGICSCSI_PCI_SUBSYSTEM_ID         (0x8000)

#define LSILOGIC_SAVED_STATE_MINOR_VERSION 1

/**
 * A simple SG element for a 64bit adress.
 */
#pragma pack(1)
typedef struct MptSGEntrySimple64
{
    /** Length of the buffer this entry describes. */
    unsigned u24Length:          24;
    /** Flag whether this element is the end of the list. */
    unsigned fEndOfList:          1;
    /** Flag whether the address is 32bit or 64bits wide. */
    unsigned f64BitAddress:       1;
    /** Flag whether this buffer contains data to be transfered or is the destination. */
    unsigned fBufferContainsData: 1;
    /** Flag whether this is a local address or a system address. */
    unsigned fLocalAddress:       1;
    /** Element type. */
    unsigned u2ElementType:       2;
    /** Flag whether this is the last element of the buffer. */
    unsigned fEndOfBuffer:        1;
    /** Flag whether this is the last element of the current segment. */
    unsigned fLastElement:        1;
    /** Lower 32bits of the address of the data buffer. */
    unsigned u32DataBufferAddressLow: 32;
    /** Upper 32bits of the address of the data buffer. */
    unsigned u32DataBufferAddressHigh: 32;
} MptSGEntrySimple64, *PMptSGEntrySimple64;
#pragma pack()
AssertCompileSize(MptSGEntrySimple64, 12);

/**
 * A simple SG element for a 32bit adress.
 */
#pragma pack(1)
typedef struct MptSGEntrySimple32
{
    /** Length of the buffer this entry describes. */
    unsigned u24Length:          24;
    /** Flag whether this element is the end of the list. */
    unsigned fEndOfList:          1;
    /** Flag whether the address is 32bit or 64bits wide. */
    unsigned f64BitAddress:       1;
    /** Flag whether this buffer contains data to be transfered or is the destination. */
    unsigned fBufferContainsData: 1;
    /** Flag whether this is a local address or a system address. */
    unsigned fLocalAddress:       1;
    /** Element type. */
    unsigned u2ElementType:       2;
    /** Flag whether this is the last element of the buffer. */
    unsigned fEndOfBuffer:        1;
    /** Flag whether this is the last element of the current segment. */
    unsigned fLastElement:        1;
    /** Lower 32bits of the address of the data buffer. */
    unsigned u32DataBufferAddressLow: 32;
} MptSGEntrySimple32, *PMptSGEntrySimple32;
#pragma pack()
AssertCompileSize(MptSGEntrySimple32, 8);

/**
 * A chain SG element.
 */
#pragma pack(1)
typedef struct MptSGEntryChain
{
    /** Size of the segment. */
    unsigned u16Length: 16;
    /** Offset in 32bit words of the next chain element in the segment
     *  identified by this element. */
    unsigned u8NextChainOffset: 8;
    /** Reserved. */
    unsigned fReserved0:    1;
    /** Flag whether the address is 32bit or 64bits wide. */
    unsigned f64BitAddress: 1;
    /** Reserved. */
    unsigned fReserved1:    1;
    /** Flag whether this is a local address or a system address. */
    unsigned fLocalAddress: 1;
    /** Element type. */
    unsigned u2ElementType: 2;
    /** Flag whether this is the last element of the buffer. */
    unsigned u2Reserved2:   2;
    /** Lower 32bits of the address of the data buffer. */
    unsigned u32SegmentAddressLow: 32;
    /** Upper 32bits of the address of the data buffer. */
    unsigned u32SegmentAddressHigh: 32;
} MptSGEntryChain, *PMptSGEntryChain;
#pragma pack()
AssertCompileSize(MptSGEntryChain, 12);

typedef union MptSGEntryUnion
{
    MptSGEntrySimple64 Simple64;
    MptSGEntrySimple32 Simple32;
    MptSGEntryChain    Chain;
} MptSGEntryUnion, *PMptSGEntryUnion;

/**
 * MPT Fusion message header - Common for all message frames.
 * This is filled in by the guest.
 */
#pragma pack(1)
typedef struct MptMessageHdr
{
    /** Function dependent data. */
    uint16_t    u16FunctionDependent;
    /** Chain offset. */
    uint8_t     u8ChainOffset;
    /** The function code. */
    uint8_t     u8Function;
    /** Function dependent data. */
    uint8_t     au8FunctionDependent[3];
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context - Unique ID from the guest unmodified by the device. */
    uint32_t    u32MessageContext;
} MptMessageHdr, *PMptMessageHdr;
#pragma pack()
AssertCompileSize(MptMessageHdr, 12);

/** Defined function codes found in the message header. */
#define MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST        (0x00)
#define MPT_MESSAGE_HDR_FUNCTION_SCSI_TASK_MGMT         (0x01)
#define MPT_MESSAGE_HDR_FUNCTION_IOC_INIT               (0x02)
#define MPT_MESSAGE_HDR_FUNCTION_IOC_FACTS              (0x03)
#define MPT_MESSAGE_HDR_FUNCTION_CONFIG                 (0x04)
#define MPT_MESSAGE_HDR_FUNCTION_PORT_FACTS             (0x05)
#define MPT_MESSAGE_HDR_FUNCTION_PORT_ENABLE            (0x06)
#define MPT_MESSAGE_HDR_FUNCTION_EVENT_NOTIFICATION     (0x07)
#define MPT_MESSAGE_HDR_FUNCTION_EVENT_ACK              (0x08)
#define MPT_MESSAGE_HDR_FUNCTION_FW_DOWNLOAD            (0x09)
#define MPT_MESSAGE_HDR_FUNCTION_TARGET_CMD_BUFFER_POST (0x0A)
#define MPT_MESSAGE_HDR_FUNCTION_TARGET_ASSIST          (0x0B)
#define MPT_MESSAGE_HDR_FUNCTION_TARGET_STATUS_SEND     (0x0C)
#define MPT_MESSAGE_HDR_FUNCTION_TARGET_MODE_ABORT      (0x0D)

#ifdef DEBUG
/**
 * Function names
 */
static const char * const g_apszMPTFunctionNames[] =
{
    "SCSI I/O Request",
    "SCSI Task Management",
    "IOC Init",
    "IOC Facts",
    "Config",
    "Port Facts",
    "Port Enable",
    "Event Notification",
    "Event Ack",
    "Firmware Download"
};
#endif

/**
 * Default reply message.
 * Send from the device to the guest upon completion of a request.
 */
 #pragma pack(1)
typedef struct MptDefaultReplyMessage
{
    /** Function dependent data. */
    uint16_t    u16FunctionDependent;
    /** Length of the message in 32bit DWords. */
    uint8_t     u8MessageLength;
    /** Function which completed. */
    uint8_t     u8Function;
    /** Function dependent. */
    uint8_t     au8FunctionDependent[3];
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context given in the request. */
    uint32_t    u32MessageContext;
    /** Function dependent status code. */
    uint16_t    u16FunctionDependentStatus;
    /** Status of the IOC. */
    uint16_t    u16IOCStatus;
    /** Additional log info. */
    uint32_t    u32IOCLogInfo;
} MptDefaultReplyMessage, *PMptDefaultReplyMessage;
#pragma pack()
AssertCompileSize(MptDefaultReplyMessage, 20);

/**
 * IO controller init request.
 */
#pragma pack(1)
typedef struct MptIOCInitRequest
{
    /** Which system send this init request. */
    uint8_t     u8WhoInit;
    /** Reserved */
    uint8_t     u8Reserved;
    /** Chain offset in the SG list. */
    uint8_t     u8ChainOffset;
    /** Function to execute. */
    uint8_t     u8Function;
    /** Flags */
    uint8_t     u8Flags;
    /** Maximum number of devices the driver can handle. */
    uint8_t     u8MaxDevices;
    /** Maximum number of buses the driver can handle. */
    uint8_t     u8MaxBuses;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** Reply frame size. */
    uint16_t    u16ReplyFrameSize;
    /** Reserved */
    uint16_t    u16Reserved;
    /** Upper 32bit part of the 64bit address the message frames are in.
     *  That means all frames must be in the same 4GB segment. */
    uint32_t    u32HostMfaHighAddr;
    /** Upper 32bit of the sense buffer. */
    uint32_t    u32SenseBufferHighAddr;
} MptIOCInitRequest, *PMptIOCInitRequest;
#pragma pack()
AssertCompileSize(MptIOCInitRequest, 24);

/**
 * IO controller init reply.
 */
#pragma pack(1)
typedef struct MptIOCInitReply
{
    /** Which subsystem send this init request. */
    uint8_t     u8WhoInit;
    /** Reserved */
    uint8_t     u8Reserved;
    /** Message length */
    uint8_t     u8MessageLength;
    /** Function. */
    uint8_t     u8Function;
    /** Flags */
    uint8_t     u8Flags;
    /** Maximum number of devices the driver can handle. */
    uint8_t     u8MaxDevices;
    /** Maximum number of busses the driver can handle. */
    uint8_t     u8MaxBuses;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID */
    uint32_t    u32MessageContext;
    /** Reserved */
    uint16_t    u16Reserved;
    /** IO controller status. */
    uint16_t    u16IOCStatus;
    /** IO controller log information. */
    uint32_t    u32IOCLogInfo;
} MptIOCInitReply, *PMptIOCInitReply;
#pragma pack()
AssertCompileSize(MptIOCInitReply, 20);

/**
 * IO controller facts request.
 */
#pragma pack(1)
typedef struct MptIOCFactsRequest
{
    /** Reserved. */
    uint16_t    u16Reserved;
    /** Chain offset in SG list. */
    uint8_t     u8ChainOffset;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved */
    uint8_t     u8Reserved[3];
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
} MptIOCFactsRequest, *PMptIOCFactsRequest;
#pragma pack()
AssertCompileSize(MptIOCFactsRequest, 12);

/**
 * IO controller facts reply.
 */
#pragma pack(1)
typedef struct MptIOCFactsReply
{
    /** Message version. */
    uint16_t    u16MessageVersion;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved */
    uint16_t    u16Reserved1;
    /** IO controller number */
    uint8_t     u8IOCNumber;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** IO controller exceptions */
    uint16_t    u16IOCExceptions;
    /** IO controller status. */
    uint16_t    u16IOCStatus;
    /** IO controller log information. */
    uint32_t    u32IOCLogInfo;
    /** Maximum chain depth. */
    uint8_t     u8MaxChainDepth;
    /** The current value of the WhoInit field. */
    uint8_t     u8WhoInit;
    /** Block size. */
    uint8_t     u8BlockSize;
    /** Flags. */
    uint8_t     u8Flags;
    /** Depth of the reply queue. */
    uint16_t    u16ReplyQueueDepth;
    /** Size of a request frame. */
    uint16_t    u16RequestFrameSize;
    /** Reserved */
    uint16_t    u16Reserved2;
    /** Product ID. */
    uint16_t    u16ProductID;
    /** Current value of the high 32bit MFA address. */
    uint32_t    u32CurrentHostMFAHighAddr;
    /** Global credits - Number of entries allocated to queues */
    uint16_t    u16GlobalCredits;
    /** Number of ports on the IO controller */
    uint8_t     u8NumberOfPorts;
    /** Event state. */
    uint8_t     u8EventState;
    /** Current value of the high 32bit sense buffer address. */
    uint32_t    u32CurrentSenseBufferHighAddr;
    /** Current reply frame size. */
    uint16_t    u16CurReplyFrameSize;
    /** Maximum number of devices. */
    uint8_t     u8MaxDevices;
    /** Maximum number of buses. */
    uint8_t     u8MaxBuses;
    /** Size of the firmware image. */
    uint32_t    u32FwImageSize;
    /** Reserved. */
    uint32_t    u32Reserved;
    /** Firmware version */
    uint32_t    u32FWVersion;
} MptIOCFactsReply, *PMptIOCFactsReply;
#pragma pack()
AssertCompileSize(MptIOCFactsReply, 60);

/**
 * Port facts request
 */
#pragma pack(1)
typedef struct MptPortFactsRequest
{
    /** Reserved */
    uint16_t    u16Reserved1;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved */
    uint16_t    u16Reserved2;
    /** Port number to get facts for. */
    uint8_t     u8PortNumber;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
} MptPortFactsRequest, *PMptPortFactsRequest;
#pragma pack()
AssertCompileSize(MptPortFactsRequest, 12);

/**
 * Port facts reply.
 */
#pragma pack(1)
typedef struct MptPortFactsReply
{
    /** Reserved. */
    uint16_t    u16Reserved1;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved */
    uint16_t    u16Reserved2;
    /** Port number the facts are for. */
    uint8_t     u8PortNumber;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** Reserved. */
    uint16_t    u16Reserved3;
    /** IO controller status. */
    uint16_t    u16IOCStatus;
    /** IO controller log information. */
    uint32_t    u32IOCLogInfo;
    /** Reserved */
    uint8_t     u8Reserved;
    /** Port type */
    uint8_t     u8PortType;
    /** Maximum number of devices on this port. */
    uint16_t    u16MaxDevices;
    /** SCSI ID of this port on the attached bus. */
    uint16_t    u16PortSCSIID;
    /** Protocol flags. */
    uint16_t    u16ProtocolFlags;
    /** Maxmimum number of target command buffers which can be posted to this port at a time. */
    uint16_t    u16MaxPostedCmdBuffers;
    /** Maximum number of target IDs that remain persistent between power/reset cycles. */
    uint16_t    u16MaxPersistentIDs;
    /** Maximum number of LAN buckets. */
    uint16_t    u16MaxLANBuckets;
    /** Reserved. */
    uint16_t    u16Reserved4;
    /** Reserved. */
    uint32_t    u32Reserved;
} MptPortFactsReply, *PMptPortFactsReply;
#pragma pack()
AssertCompileSize(MptPortFactsReply, 40);

/**
 * Port Enable request.
 */
#pragma pack(1)
typedef struct MptPortEnableRequest
{
    /** Reserved. */
    uint16_t    u16Reserved1;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved. */
    uint16_t    u16Reserved2;
    /** Port number to enable. */
    uint8_t     u8PortNumber;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
} MptPortEnableRequest, *PMptPortEnableRequest;
#pragma pack()
AssertCompileSize(MptPortEnableRequest, 12);

/**
 * Port enable reply.
 */
#pragma pack(1)
typedef struct MptPortEnableReply
{
    /** Reserved. */
    uint16_t    u16Reserved1;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved */
    uint16_t    u16Reserved2;
    /** Port number which was enabled. */
    uint8_t     u8PortNumber;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** Reserved. */
    uint16_t    u16Reserved3;
    /** IO controller status */
    uint16_t    u16IOCStatus;
    /** IO controller log information. */
    uint32_t    u32IOCLogInfo;
} MptPortEnableReply, *PMptPortEnableReply;
#pragma pack()
AssertCompileSize(MptPortEnableReply, 20);

/**
 * Event notification request.
 */
#pragma pack(1)
typedef struct MptEventNotificationRequest
{
    /** Switch - Turns event notification on and off. */
    uint8_t     u8Switch;
    /** Reserved. */
    uint8_t     u8Reserved1;
    /** Chain offset. */
    uint8_t     u8ChainOffset;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved. */
    uint8_t     u8reserved2[3];
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
} MptEventNotificationRequest, *PMptEventNotificationRequest;
#pragma pack()
AssertCompileSize(MptEventNotificationRequest, 12);

/**
 * Event notification reply.
 */
#pragma pack(1)
typedef struct MptEventNotificationReply
{
    /** Event data length. */
    uint16_t    u16EventDataLength;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** Reserved. */
    uint16_t    u16Reserved1;
    /** Ack required. */
    uint8_t     u8AckRequired;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** Reserved. */
    uint16_t    u16Reserved2;
    /** IO controller status. */
    uint16_t    u16IOCStatus;
    /** IO controller log information. */
    uint32_t    u32IOCLogInfo;
    /** Notification event. */
    uint32_t    u32Event;
    /** Event context. */
    uint32_t    u32EventContext;
    /** Event data. */
    uint32_t    u32EventData;
} MptEventNotificationReply, *PMptEventNotificationReply;
#pragma pack()
AssertCompileSize(MptEventNotificationReply, 32);

#define MPT_EVENT_EVENT_CHANGE (0x0000000a)

/**
 * SCSI IO Request
 */
#pragma pack(1)
typedef struct MptSCSIIORequest
{
    /** Target ID */
    uint8_t     u8TargetID;
    /** Bus number */
    uint8_t     u8Bus;
    /** Chain offset */
    uint8_t     u8ChainOffset;
    /** Function number. */
    uint8_t     u8Function;
    /** CDB length. */
    uint8_t     u8CDBLength;
    /** Sense buffer length. */
    uint8_t     u8SenseBufferLength;
    /** Rserved */
    uint8_t     u8Reserved;
    /** Message flags. */
    uint8_t     u8MessageFlags;
    /** Message context ID. */
    uint32_t    u32MessageContext;
    /** LUN */
    uint8_t     au8LUN[8];
    /** Control values. */
    uint32_t    u32Control;
    /** The CDB. */
    uint8_t     au8CDB[16];
    /** Data length. */
    uint32_t    u32DataLength;
    /** Sense buffer low 32bit address. */
    uint32_t    u32SenseBufferLowAddress;
} MptSCSIIORequest, *PMptSCSIIORequest;
#pragma pack()
AssertCompileSize(MptSCSIIORequest, 48);

#define MPT_SCSIIO_REQUEST_CONTROL_TXDIR_GET(x) (((x) & 0x3000000) >> 24)
#define MPT_SCSIIO_REQUEST_CONTROL_TXDIR_NONE  (0x0)
#define MPT_SCSIIO_REQUEST_CONTROL_TXDIR_WRITE (0x1)
#define MPT_SCSIIO_REQUEST_CONTROL_TXDIR_READ  (0x2)

/**
 * SCSI IO error reply.
 */
#pragma pack(1)
typedef struct MptSCSIIOErrorReply
{
    /** Target ID */
    uint8_t     u8TargetID;
    /** Bus number */
    uint8_t     u8Bus;
    /** Message length. */
    uint8_t     u8MessageLength;
    /** Function number. */
    uint8_t     u8Function;
    /** CDB length */
    uint8_t     u8CDBLength;
    /** Sense buffer length */
    uint8_t     u8SenseBufferLength;
    /** Reserved */
    uint8_t     u8Reserved;
    /** Message flags */
    uint8_t     u8MessageFlags;
    /** Message context ID */
    uint32_t    u32MessageContext;
    /** SCSI status. */
    uint8_t     u8SCSIStatus;
    /** SCSI state */
    uint8_t     u8SCSIState;
    /** IO controller status */
    uint16_t    u16IOCStatus;
    /** IO controller log information */
    uint32_t    u32IOCLogInfo;
    /** Transfer count */
    uint32_t    u32TransferCount;
    /** Sense count */
    uint32_t    u32SenseCount;
    /** Response information */
    uint32_t    u32ResponseInfo;
} MptSCSIIOErrorReply, *PMptSCSIIOErrorReply;
#pragma pack()
AssertCompileSize(MptSCSIIOErrorReply, 32);

#define MPT_SCSI_IO_ERROR_SCSI_STATE_AUTOSENSE_VALID (0x01)
#define MPT_SCSI_IO_ERROR_SCSI_STATE_TERMINATED      (0x08)

/**
 * IOC status codes sepcific to the SCSI I/O error reply.
 */
#define MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_BUS      (0x0041)
#define MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_TARGETID (0x0042)
#define MPT_SCSI_IO_ERROR_IOCSTATUS_DEVICE_NOT_THERE (0x0043)

/**
 * SCSI task management request.
 */
#pragma pack(1)
typedef struct MptSCSITaskManagementRequest
{
    /** Target ID */
    uint8_t     u8TargetID;
    /** Bus number */
    uint8_t     u8Bus;
    /** Chain offset */
    uint8_t     u8ChainOffset;
    /** Function number */
    uint8_t     u8Function;
    /** Reserved */
    uint8_t     u8Reserved1;
    /** Task type */
    uint8_t     u8TaskType;
    /** Reserved */
    uint8_t     u8Reserved2;
    /** Message flags */
    uint8_t     u8MessageFlags;
    /** Message context ID */
    uint32_t    u32MessageContext;
    /** LUN */
    uint8_t     au8LUN[8];
    /** Reserved */
    uint8_t     auReserved[28];
    /** Task message context ID. */
    uint32_t    u32TaskMessageContext;
} MptSCSITaskManagementRequest, *PMptSCSITaskManagementRequest;
#pragma pack()
AssertCompileSize(MptSCSITaskManagementRequest, 52);

/**
 * SCSI task management reply.
 */
#pragma pack(1)
typedef struct MptSCSITaskManagementReply
{
    /** Target ID */
    uint8_t     u8TargetID;
    /** Bus number */
    uint8_t     u8Bus;
    /** Message length */
    uint8_t     u8MessageLength;
    /** Function number */
    uint8_t     u8Function;
    /** Reserved */
    uint8_t     u8Reserved1;
    /** Task type */
    uint8_t     u8TaskType;
    /** Reserved */
    uint8_t     u8Reserved2;
    /** Message flags */
    uint8_t     u8MessageFlags;
    /** Message context ID */
    uint32_t    u32MessageContext;
    /** Reserved */
    uint16_t    u16Reserved;
    /** IO controller status */
    uint16_t    u16IOCStatus;
    /** IO controller log information */
    uint32_t    u32IOCLogInfo;
    /** Termination count */
    uint32_t    u32TerminationCount;
} MptSCSITaskManagementReply, *PMptSCSITaskManagementReply;
#pragma pack()
AssertCompileSize(MptSCSITaskManagementReply, 24);

/**
 * Configuration request
 */
#pragma pack(1)
typedef struct MptConfigurationRequest
{
    /** Action code. */
    uint8_t    u8Action;
    /** Reserved. */
    uint8_t    u8Reserved1;
    /** Chain offset. */
    uint8_t    u8ChainOffset;
    /** Function number. */
    uint8_t    u8Function;
    /** Reserved. */
    uint8_t    u8Reserved2[3];
    /** Message flags. */
    uint8_t    u8MessageFlags;
    /** Message context ID. */
    uint32_t   u32MessageContext;
    /** Reserved. */
    uint8_t    u8Reserved3[8];
    /** Version number of the page. */
    uint8_t    u8PageVersion;
    /** Length of the page in 32bit Dwords. */
    uint8_t    u8PageLength;
    /** Page number to access. */
    uint8_t    u8PageNumber;
    /** Type of the page beeing accessed. */
    uint8_t    u8PageType;
    /** Page type dependent address. */
    union
    {
        /** 32bit view. */
        uint32_t u32PageAddress;
        struct
        {
            /** Port number to get the configuration page for. */
            uint8_t u8PortNumber;
            /** Reserved. */
            uint8_t u8Reserved[3];
        } MPIPortNumber;
        struct
        {
            /** Target ID to get the configuration page for. */
            uint8_t u8TargetID;
            /** Bus number to get the configuration page for. */
            uint8_t u8Bus;
            /** Reserved. */
            uint8_t u8Reserved[2];
        } BusAndTargetId;
    } u;
    MptSGEntrySimple64 SimpleSGElement;
} MptConfigurationRequest, *PMptConfigurationRequest;
#pragma pack()
AssertCompileSize(MptConfigurationRequest, 40);

/** Possible action codes. */
#define MPT_CONFIGURATION_REQUEST_ACTION_HEADER        (0x00)
#define MPT_CONFIGURATION_REQUEST_ACTION_READ_CURRENT  (0x01)
#define MPT_CONFIGURATION_REQUEST_ACTION_WRITE_CURRENT (0x02)
#define MPT_CONFIGURATION_REQUEST_ACTION_READ_DEFAULT  (0x03)
#define MPT_CONFIGURATION_REQUEST_ACTION_DEFAULT       (0x04)
#define MPT_CONFIGURATION_REQUEST_ACTION_READ_NVRAM    (0x05)
#define MPT_CONFIGURATION_REQUEST_ACTION_WRITE_NVRAM   (0x06)

/** Page type codes. */
#define MPT_CONFIGURATION_REQUEST_PAGE_TYPE_IO_UNIT    (0x00)
#define MPT_CONFIGURATION_REQUEST_PAGE_TYPE_IOC        (0x01)
#define MPT_CONFIGURATION_REQUEST_PAGE_TYPE_BIOS       (0x02)
#define MPT_CONFIGURATION_REQUEST_PAGE_TYPE_SCSI_PORT  (0x03)

/**
 * Configuration reply.
 */
#pragma pack(1)
typedef struct MptConfigurationReply
{
    /** Action code. */
    uint8_t    u8Action;
    /** Reserved. */
    uint8_t    u8Reserved;
    /** Message length. */
    uint8_t    u8MessageLength;
    /** Function number. */
    uint8_t    u8Function;
    /** Reserved. */
    uint8_t    u8Reserved2[3];
    /** Message flags. */
    uint8_t    u8MessageFlags;
    /** Message context ID. */
    uint32_t   u32MessageContext;
    /** Reserved. */
    uint16_t   u16Reserved;
    /** I/O controller status. */
    uint16_t   u16IOCStatus;
    /** I/O controller log information. */
    uint32_t   u32IOCLogInfo;
    /** Version number of the page. */
    uint8_t    u8PageVersion;
    /** Length of the page in 32bit Dwords. */
    uint8_t    u8PageLength;
    /** Page number to access. */
    uint8_t    u8PageNumber;
    /** Type of the page beeing accessed. */
    uint8_t    u8PageType;
} MptConfigurationReply, *PMptConfigurationReply;
#pragma pack()
AssertCompileSize(MptConfigurationReply, 24);

/** Additional I/O controller status codes for the configuration reply. */
#define MPT_IOCSTATUS_CONFIG_INVALID_ACTION (0x0020)
#define MPT_IOCSTATUS_CONFIG_INVALID_TYPE   (0x0021)
#define MPT_IOCSTATUS_CONFIG_INVALID_PAGE   (0x0022)
#define MPT_IOCSTATUS_CONFIG_INVALID_DATA   (0x0023)
#define MPT_IOCSTATUS_CONFIG_NO_DEFAULTS    (0x0024)
#define MPT_IOCSTATUS_CONFIG_CANT_COMMIT    (0x0025)

/**
 * Union of all possible request messages.
 */
typedef union MptRequestUnion
{
    MptMessageHdr                Header;
    MptIOCInitRequest            IOCInit;
    MptIOCFactsRequest           IOCFacts;
    MptPortFactsRequest          PortFacts;
    MptPortEnableRequest         PortEnable;
    MptEventNotificationRequest  EventNotification;
    MptSCSIIORequest             SCSIIO;
    MptSCSITaskManagementRequest SCSITaskManagement;
    MptConfigurationRequest      Configuration;
} MptRequestUnion, *PMptRequestUnion;

/**
 * Union of all possible reply messages.
 */
typedef union MptReplyUnion
{
    /** 16bit view. */
    uint16_t                   au16Reply[30];
    MptDefaultReplyMessage     Header;
    MptIOCInitReply            IOCInit;
    MptIOCFactsReply           IOCFacts;
    MptPortFactsReply          PortFacts;
    MptPortEnableReply         PortEnable;
    MptEventNotificationReply  EventNotification;
    MptSCSIIOErrorReply        SCSIIOError;
    MptSCSITaskManagementReply SCSITaskManagement;
    MptConfigurationReply      Configuration;
} MptReplyUnion, *PMptReplyUnion;


/**
 * Configuration Page attributes.
 */
#define MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY            (0x00)
#define MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE          (0x10)
#define MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT          (0x20)
#define MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY (0x30)

#define MPT_CONFIGURATION_PAGE_ATTRIBUTE_GET(u8PageType) ((u8PageType) & 0xf0)

/**
 * Configuration Page types.
 */
#define MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT                  (0x00)
#define MPT_CONFIGURATION_PAGE_TYPE_IOC                      (0x01)
#define MPT_CONFIGURATION_PAGE_TYPE_BIOS                     (0x02)
#define MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT            (0x03)
#define MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE          (0x04)
#define MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING            (0x09)

#define MPT_CONFIGURATION_PAGE_TYPE_GET(u8PageType) ((u8PageType) & 0x0f)

/**
 * Configuration Page header - Common to all pages.
 */
#pragma pack(1)
typedef struct MptConfigurationPageHeader
{
    /** Version of the page. */
    uint8_t     u8PageVersion;
    /** The length of the page in 32bit D-Words. */
    uint8_t     u8PageLength;
    /** Number of the page. */
    uint8_t     u8PageNumber;
    /** Type of the page. */
    uint8_t     u8PageType;
} MptConfigurationPageHeader, *PMptConfigurationPageHeader;
#pragma pack()
AssertCompileSize(MptConfigurationPageHeader, 4);

/**
 * Manufacturing page 0. - Readonly.
 */
#pragma pack(1)
typedef struct MptConfigurationPageManufacturing0
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[76];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Name of the chip. */
            uint8_t               abChipName[16];
            /** Chip revision. */
            uint8_t               abChipRevision[8];
            /** Board name. */
            uint8_t               abBoardName[16];
            /** Board assembly. */
            uint8_t               abBoardAssembly[16];
            /** Board tracer number. */
            uint8_t               abBoardTracerNumber[16];
        } fields;
    } u;
} MptConfigurationPageManufacturing0, *PMptConfigurationPageManufacturing0;
#pragma pack()
AssertCompileSize(MptConfigurationPageManufacturing0, 76);

/**
 * Manufacturing page 1. - Readonly Persistent.
 */
#pragma pack(1)
typedef struct MptConfigurationPageManufacturing1
{
    /** The omnipresent header. */
    MptConfigurationPageHeader    Header;
    /** VPD info - don't know what belongs here so all zero. */
    uint8_t                       abVPDInfo[256];
} MptConfigurationPageManufacturing1, *PMptConfigurationPageManufacturing1;
#pragma pack()
AssertCompileSize(MptConfigurationPageManufacturing1, 260);

/**
 * Manufacturing page 2. - Readonly.
 */
#pragma pack(1)
typedef struct MptConfigurationPageManufacturing2
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                        abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader Header;
            /** PCI Device ID. */
            uint16_t                   u16PCIDeviceID;
            /** PCI Revision ID. */
            uint8_t                    u8PCIRevisionID;
            /** Reserved. */
            uint8_t                    u8Reserved;
            /** Hardware specific settings... */
        } fields;
    } u;
} MptConfigurationPageManufacturing2, *PMptConfigurationPageManufacturing2;
#pragma pack()
AssertCompileSize(MptConfigurationPageManufacturing2, 8);

/**
 * Manufacturing page 3. - Readonly.
 */
#pragma pack(1)
typedef struct MptConfigurationPageManufacturing3
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** PCI Device ID. */
            uint16_t              u16PCIDeviceID;
            /** PCI Revision ID. */
            uint8_t               u8PCIRevisionID;
            /** Reserved. */
            uint8_t               u8Reserved;
            /** Chip specific settings... */
        } fields;
    } u;
} MptConfigurationPageManufacturing3, *PMptConfigurationPageManufacturing3;
#pragma pack()
AssertCompileSize(MptConfigurationPageManufacturing3, 8);

/**
 * Manufacturing page 4. - Readonly.
 */
#pragma pack(1)
typedef struct MptConfigurationPageManufacturing4
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[84];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Reserved. */
            uint32_t              u32Reserved;
            /** InfoOffset0. */
            uint8_t               u8InfoOffset0;
            /** Info size. */
            uint8_t               u8InfoSize0;
            /** InfoOffset1. */
            uint8_t               u8InfoOffset1;
            /** Info size. */
            uint8_t               u8InfoSize1;
            /** Size of the inquiry data. */
            uint8_t               u8InquirySize;
            /** Reserved. */
            uint8_t               abReserved[3];
            /** Inquiry data. */
            uint8_t               abInquiryData[56];
            /** IS volume settings. */
            uint32_t              u32ISVolumeSettings;
            /** IME volume settings. */
            uint32_t              u32IMEVolumeSettings;
            /** IM volume settings. */
            uint32_t              u32IMVolumeSettings;
        } fields;
    } u;
} MptConfigurationPageManufacturing4, *PMptConfigurationPageManufacturing4;
#pragma pack()
AssertCompileSize(MptConfigurationPageManufacturing4, 84);

/**
 * IO Unit page 0. - Readonly.
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOUnit0
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** A unique identifier. */
            uint64_t              u64UniqueIdentifier;
        } fields;
    } u;
} MptConfigurationPageIOUnit0, *PMptConfigurationPageIOUnit0;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOUnit0, 12);

/**
 * IO Unit page 1. - Read/Write.
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOUnit1
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Flag whether this is a single function PCI device. */
            unsigned              fSingleFunction:         1;
            /** Flag whether all possible paths to a device are mapped. */
            unsigned              fAllPathsMapped:         1;
            /** Reserved. */
            unsigned              u4Reserved:              4;
            /** Flag whether all RAID functionality is disabled. */
            unsigned              fIntegratedRAIDDisabled: 1;
            /** Flag whether 32bit PCI accesses are forced. */
            unsigned              f32BitAccessForced:      1;
            /** Reserved. */
            unsigned              abReserved:             24;
        } fields;
    } u;
} MptConfigurationPageIOUnit1, *PMptConfigurationPageIOUnit1;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOUnit1, 8);

/**
 * Adapter Ordering.
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOUnit2AdapterOrdering
{
    /** PCI bus number. */
    unsigned    u8PCIBusNumber:   8;
    /** PCI device and function number. */
    unsigned    u8PCIDevFn:       8;
    /** Flag whether the adapter is embedded. */
    unsigned    fAdapterEmbedded: 1;
    /** Flag whether the adapter is enabled. */
    unsigned    fAdapterEnabled:  1;
    /** Reserved. */
    unsigned    u6Reserved:       6;
    /** Reserved. */
    unsigned    u8Reserved:       8;
} MptConfigurationPageIOUnit2AdapterOrdering, *PMptConfigurationPageIOUnit2AdapterOrdering;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOUnit2AdapterOrdering, 4);

/**
 * IO Unit page 2. - Read/Write.
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOUnit2
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[28];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Reserved. */
            unsigned              fReserved:           1;
            /** Flag whether Pause on error is enabled. */
            unsigned              fPauseOnError:       1;
            /** Flag whether verbose mode is enabled. */
            unsigned              fVerboseModeEnabled: 1;
            /** Set to disable color video. */
            unsigned              fDisableColorVideo:  1;
            /** Flag whether int 40h is hooked. */
            unsigned              fNotHookInt40h:      1;
            /** Reserved. */
            unsigned              u3Reserved:          3;
            /** Reserved. */
            unsigned              abReserved:         24;
            /** BIOS version. */
            uint32_t              u32BIOSVersion;
            /** Adapter ordering. */
            MptConfigurationPageIOUnit2AdapterOrdering aAdapterOrder[4];
        } fields;
    } u;
} MptConfigurationPageIOUnit2, *PMptConfigurationPageIOUnit2;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOUnit2, 28);

/*
 * IO Unit page 3. - Read/Write.
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOUnit3
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Number of GPIO values. */
            uint8_t               u8GPIOCount;
            /** Reserved. */
            uint8_t               abReserved[3];
        } fields;
    } u;
} MptConfigurationPageIOUnit3, *PMptConfigurationPageIOUnit3;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOUnit3, 8);

/**
 * IOC page 0. - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC0
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[28];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Total ammount of NV memory in bytes. */
            uint32_t              u32TotalNVStore;
            /** Number of free bytes in the NV store. */
            uint32_t              u32FreeNVStore;
            /** PCI vendor ID. */
            uint16_t              u16VendorId;
            /** PCI device ID. */
            uint16_t              u16DeviceId;
            /** PCI revision ID. */
            uint8_t               u8RevisionId;
            /** Reserved. */
            uint8_t               abReserved[3];
            /** PCI class code. */
            uint32_t              u32ClassCode;
            /** Subsystem vendor Id. */
            uint16_t              u16SubsystemVendorId;
            /** Subsystem Id. */
            uint16_t              u16SubsystemId;
        } fields;
    } u;
} MptConfigurationPageIOC0, *PMptConfigurationPageIOC0;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC0, 28);

/**
 * IOC page 1. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC1
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[16];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Flag whether reply coalescing is enabled. */
            unsigned              fReplyCoalescingEnabled: 1;
            /** Reserved. */
            unsigned              u31Reserved:            31;
            /** Coalescing Timeout in microseconds. */
            unsigned              u32CoalescingTimeout:   32;
            /** Coalescing depth. */
            unsigned              u8CoalescingDepth:       8;
            /** Reserved. */
            unsigned              u8Reserved0:             8;
            unsigned              u8Reserved1:             8;
            unsigned              u8Reserved2:             8;
        } fields;
    } u;
} MptConfigurationPageIOC1, *PMptConfigurationPageIOC1;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC1, 16);

/**
 * IOC page 2. - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC2
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Flag whether striping is supported. */
            unsigned              fStripingSupported:            1;
            /** Flag whether enhanced mirroring is supported. */
            unsigned              fEnhancedMirroringSupported:   1;
            /** Flag whether mirroring is supported. */
            unsigned              fMirroringSupported:           1;
            /** Reserved. */
            unsigned              u26Reserved:                  26;
            /** Flag whether SES is supported. */
            unsigned              fSESSupported:                 1;
            /** Flag whether SAF-TE is supported. */
            unsigned              fSAFTESupported:               1;
            /** Flag whether cross channel volumes are supported. */
            unsigned              fCrossChannelVolumesSupported: 1;
            /** Number of active integrated RAID volumes. */
            unsigned              u8NumActiveVolumes:            8;
            /** Maximum number of integrated RAID volumes supported. */
            unsigned              u8MaxVolumes:                  8;
            /** Number of active integrated RAID physical disks. */
            unsigned              u8NumActivePhysDisks:          8;
            /** Maximum number of integrated RAID physical disks supported. */
            unsigned              u8MaxPhysDisks:                8;
            /** RAID volumes... - not supported. */
        } fields;
    } u;
} MptConfigurationPageIOC2, *PMptConfigurationPageIOC2;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC2, 12);

/**
 * IOC page 3. - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC3
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Number of active integrated RAID physical disks. */
            uint8_t               u8NumPhysDisks;
            /** Reserved. */
            uint8_t               abReserved[3];
        } fields;
    } u;
} MptConfigurationPageIOC3, *PMptConfigurationPageIOC3;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC3, 8);

/**
 * IOC page 4. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC4
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[8];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Number of SEP entries in this page. */
            uint8_t               u8ActiveSEP;
            /** Maximum number of SEp entries supported. */
            uint8_t               u8MaxSEP;
            /** Reserved. */
            uint16_t              u16Reserved;
            /** SEP entries... - not supported. */
        } fields;
    } u;
} MptConfigurationPageIOC4, *PMptConfigurationPageIOC4;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC4, 8);

/**
 * IOC page 6. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageIOC6
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[60];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            uint32_t                      u32CapabilitiesFlags;
            uint8_t                       u8MaxDrivesIS;
            uint8_t                       u8MaxDrivesIM;
            uint8_t                       u8MaxDrivesIME;
            uint8_t                       u8Reserved1;
            uint8_t                       u8MinDrivesIS;
            uint8_t                       u8MinDrivesIM;
            uint8_t                       u8MinDrivesIME;
            uint8_t                       u8Reserved2;
            uint8_t                       u8MaxGlobalHotSpares;
            uint8_t                       u8Reserved3;
            uint16_t                      u16Reserved4;
            uint32_t                      u32Reserved5;
            uint32_t                      u32SupportedStripeSizeMapIS;
            uint32_t                      u32SupportedStripeSizeMapIME;
            uint32_t                      u32Reserved6;
            uint8_t                       u8MetadataSize;
            uint8_t                       u8Reserved7;
            uint16_t                      u16Reserved8;
            uint16_t                      u16MaxBadBlockTableEntries;
            uint16_t                      u16Reserved9;
            uint16_t                      u16IRNvsramUsage;
            uint16_t                      u16Reserved10;
            uint32_t                      u32IRNvsramVersion;
            uint32_t                      u32Reserved11;
        } fields;
    } u;
} MptConfigurationPageIOC6, *PMptConfigurationPageIOC6;
#pragma pack()
AssertCompileSize(MptConfigurationPageIOC6, 60);

/**
 * SCSI-SPI port page 0. - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIPort0
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Flag whether this port is information unit trnafsers capable. */
            unsigned              fInformationUnitTransfersCapable: 1;
            /** Flag whether the port is DT (Dual Transfer) capable. */
            unsigned              fDTCapable:                       1;
            /** Flag whether the port is QAS (Quick Arbitrate and Select) capable. */
            unsigned              fQASCapable:                      1;
            /** Reserved. */
            unsigned              u5Reserved1:                      5;
            /** Minimum Synchronous transfer period. */
            unsigned              u8MinimumSynchronousTransferPeriod: 8;
            /** Maximum synchronous offset. */
            unsigned              u8MaximumSynchronousOffset:         8;
            /** Reserved. */
            unsigned              u5Reserved2:                      5;
            /** Flag whether indicating the width of the bus - 0 narrow and 1 for wide. */
            unsigned              fWide:                            1;
            /** Reserved */
            unsigned              fReserved:                        1;
            /** Flag whether the port is AIP (Asynchronous Information Protection) capable. */
            unsigned              fAIPCapable:                      1;
            /** Signaling Type. */
            unsigned              u2SignalingType:                  2;
            /** Reserved. */
            unsigned              u30Reserved:                     30;
        } fields;
    } u;
} MptConfigurationPageSCSISPIPort0, *PMptConfigurationPageSCSISPIPort0;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIPort0, 12);

/**
 * SCSI-SPI port page 1. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIPort1
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** The SCSI ID of the port. */
            uint8_t               u8SCSIID;
            /** Reserved. */
            uint8_t               u8Reserved;
            /** Port response IDs Bit mask field. */
            uint16_t              u16PortResponseIDsBitmask;
            /** Value for the on BUS timer. */
            uint32_t              u32OnBusTimerValue;
        } fields;
    } u;
} MptConfigurationPageSCSISPIPort1, *PMptConfigurationPageSCSISPIPort1;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIPort1, 12);

/**
 * Device settings for one device.
 */
#pragma pack(1)
typedef struct MptDeviceSettings
{
    /** Timeout for I/O in seconds. */
    unsigned    u8Timeout:             8;
    /** Minimum synchronous factor. */
    unsigned    u8SyncFactor:          8;
    /** Flag whether disconnect is enabled. */
    unsigned    fDisconnectEnable:     1;
    /** Flag whether Scan ID is enabled. */
    unsigned    fScanIDEnable:         1;
    /** Flag whether Scan LUNs is enabled. */
    unsigned    fScanLUNEnable:        1;
    /** Flag whether tagged queuing is enabled. */
    unsigned    fTaggedQueuingEnabled: 1;
    /** Flag whether wide is enabled. */
    unsigned    fWideDisable:          1;
    /** Flag whether this device is bootable. */
    unsigned    fBootChoice:           1;
    /** Reserved. */
    unsigned    u10Reserved:          10;
} MptDeviceSettings, *PMptDeviceSettings;
#pragma pack()
AssertCompileSize(MptDeviceSettings, 4);

/**
 * SCSI-SPI port page 2. - Read/Write for the BIOS
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIPort2
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[76];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Flag indicating the bus scan order. */
            unsigned              fBusScanOrderHighToLow:  1;
            /** Reserved. */
            unsigned              fReserved:               1;
            /** Flag whether SCSI Bus resets are avoided. */
            unsigned              fAvoidSCSIBusResets:     1;
            /** Flag whether alternate CHS is used. */
            unsigned              fAlternateCHS:           1;
            /** Flag whether termination is disabled. */
            unsigned              fTerminationDisabled:    1;
            /** Reserved. */
            unsigned              u27Reserved:            27;
            /** Host SCSI ID. */
            unsigned              u4HostSCSIID:            4;
            /** Initialize HBA. */
            unsigned              u2InitializeHBA:         2;
            /** Removeable media setting. */
            unsigned              u2RemovableMediaSetting: 2;
            /** Spinup delay. */
            unsigned              u4SpinupDelay:           4;
            /** Negotiating settings. */
            unsigned              u2NegotitatingSettings:  2;
            /** Reserved. */
            unsigned              u18Reserved:            18;
            /** Device Settings. */
            MptDeviceSettings     aDeviceSettings[16];
        } fields;
    } u;
} MptConfigurationPageSCSISPIPort2, *PMptConfigurationPageSCSISPIPort2;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIPort2, 76);

/**
 * SCSI-SPI device page 0. - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIDevice0
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Negotiated Parameters. */
            /** Information Units enabled. */
            unsigned              fInformationUnitsEnabled: 1;
            /** Dual Transfers Enabled. */
            unsigned              fDTEnabled:               1;
            /** QAS enabled. */
            unsigned              fQASEnabled:              1;
            /** Reserved. */
            unsigned              u5Reserved1:              5;
            /** Synchronous Transfer period. */
            unsigned              u8NegotiatedSynchronousTransferPeriod: 8;
            /** Synchronous offset. */
            unsigned              u8NegotiatedSynchronousOffset: 8;
            /** Reserved. */
            unsigned              u5Reserved2:              5;
            /** Width - 0 for narrow and 1 for wide. */
            unsigned              fWide:                    1;
            /** Reserved. */
            unsigned              fReserved:                1;
            /** AIP enabled. */
            unsigned              fAIPEnabled:              1;
            /** Flag whether negotiation occurred. */
            unsigned              fNegotationOccured:       1;
            /** Flag whether a SDTR message was rejected. */
            unsigned              fSDTRRejected:            1;
            /** Flag whether a WDTR message was rejected. */
            unsigned              fWDTRRejected:            1;
            /** Flag whether a PPR message was rejected. */
            unsigned              fPPRRejected:             1;
            /** Reserved. */
            unsigned              u28Reserved:             28;
        } fields;
    } u;
} MptConfigurationPageSCSISPIDevice0, *PMptConfigurationPageSCSISPIDevice0;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIDevice0, 12);

/**
 * SCSI-SPI device page 1. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIDevice1
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[16];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Requested Parameters. */
            /** Information Units enable. */
            bool                  fInformationUnitsEnable: 1;
            /** Dual Transfers Enable. */
            bool                  fDTEnable:               1;
            /** QAS enable. */
            bool                  fQASEnable:              1;
            /** Reserved. */
            unsigned              u5Reserved1:              5;
            /** Synchronous Transfer period. */
            unsigned              u8NegotiatedSynchronousTransferPeriod: 8;
            /** Synchronous offset. */
            unsigned              u8NegotiatedSynchronousOffset: 8;
            /** Reserved. */
            unsigned              u5Reserved2:              5;
            /** Width - 0 for narrow and 1 for wide. */
            bool                  fWide:                   1;
            /** Reserved. */
            bool                  fReserved1:              1;
            /** AIP enable. */
            bool                  fAIPEnable:              1;
            /** Reserved. */
            bool                  fReserved2:              1;
            /** WDTR disallowed. */
            bool                  fWDTRDisallowed:         1;
            /** SDTR disallowed. */
            bool                  fSDTRDisallowed:         1;
            /** Reserved. */
            unsigned              u29Reserved:            29;
        } fields;
    } u;
} MptConfigurationPageSCSISPIDevice1, *PMptConfigurationPageSCSISPIDevice1;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIDevice1, 16);

/**
 * SCSI-SPI device page 2. - Read/Write
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIDevice2
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[16];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Reserved. */
            unsigned              u4Reserved: 4;
            /** ISI enable. */
            unsigned              fISIEnable: 1;
            /** Secondary driver enable. */
            unsigned              fSecondaryDriverEnable:          1;
            /** Reserved. */
            unsigned              fReserved:                       1;
            /** Slew reate controler. */
            unsigned              u3SlewRateControler:             3;
            /** Primary drive strength controler. */
            unsigned              u3PrimaryDriveStrengthControl:   3;
            /** Secondary drive strength controler. */
            unsigned              u3SecondaryDriveStrengthControl: 3;
            /** Reserved. */
            unsigned              u12Reserved:                    12;
            /** XCLKH_ST. */
            unsigned              fXCLKH_ST:                       1;
            /** XCLKS_ST. */
            unsigned              fXCLKS_ST:                       1;
            /** XCLKH_DT. */
            unsigned              fXCLKH_DT:                       1;
            /** XCLKS_DT. */
            unsigned              fXCLKS_DT:                       1;
            /** Parity pipe select. */
            unsigned              u2ParityPipeSelect:              2;
            /** Reserved. */
            unsigned              u30Reserved:                    30;
            /** Data bit pipeline select. */
            unsigned              u32DataPipelineSelect:          32;
        } fields;
    } u;
} MptConfigurationPageSCSISPIDevice2, *PMptConfigurationPageSCSISPIDevice2;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIDevice2, 16);

/**
 * SCSI-SPI device page 3 (Revision G). - Readonly
 */
#pragma pack(1)
typedef struct MptConfigurationPageSCSISPIDevice3
{
    /** Union. */
    union
    {
        /** Byte view. */
        uint8_t                   abPageData[12];
        /** Field view. */
        struct
        {
            /** The omnipresent header. */
            MptConfigurationPageHeader    Header;
            /** Number of times the IOC rejected a message because it doesn't support the operation. */
            uint16_t                      u16MsgRejectCount;
            /** Number of times the SCSI bus entered an invalid operation state. */
            uint16_t                      u16PhaseErrorCount;
            /** Number of parity errors. */
            uint16_t                      u16ParityCount;
            /** Reserved. */
            uint16_t                      u16Reserved;
        } fields;
    } u;
} MptConfigurationPageSCSISPIDevice3, *PMptConfigurationPageSCSISPIDevice3;
#pragma pack()
AssertCompileSize(MptConfigurationPageSCSISPIDevice3, 12);

/**
 * Structure of all supported pages.
 */
typedef struct MptConfigurationPagesSupported
{
    MptConfigurationPageManufacturing0 ManufacturingPage0;
    MptConfigurationPageManufacturing1 ManufacturingPage1;
    MptConfigurationPageManufacturing2 ManufacturingPage2;
    MptConfigurationPageManufacturing3 ManufacturingPage3;
    MptConfigurationPageManufacturing4 ManufacturingPage4;
    MptConfigurationPageIOUnit0        IOUnitPage0;
    MptConfigurationPageIOUnit1        IOUnitPage1;
    MptConfigurationPageIOUnit2        IOUnitPage2;
    MptConfigurationPageIOUnit3        IOUnitPage3;
    MptConfigurationPageIOC0           IOCPage0;
    MptConfigurationPageIOC1           IOCPage1;
    MptConfigurationPageIOC2           IOCPage2;
    MptConfigurationPageIOC3           IOCPage3;
    MptConfigurationPageIOC4           IOCPage4;
    MptConfigurationPageIOC6           IOCPage6;
    struct
    {
        MptConfigurationPageSCSISPIPort0   SCSISPIPortPage0;
        MptConfigurationPageSCSISPIPort1   SCSISPIPortPage1;
        MptConfigurationPageSCSISPIPort2   SCSISPIPortPage2;
    } aPortPages[1]; /* Currently only one port supported. */
    struct
    {
        struct
        {
            MptConfigurationPageSCSISPIDevice0 SCSISPIDevicePage0;
            MptConfigurationPageSCSISPIDevice1 SCSISPIDevicePage1;
            MptConfigurationPageSCSISPIDevice2 SCSISPIDevicePage2;
            MptConfigurationPageSCSISPIDevice3 SCSISPIDevicePage3;
        } aDevicePages[LSILOGIC_DEVICES_MAX];
    } aBuses[1]; /* Only one bus at the moment. */
} MptConfigurationPagesSupported, *PMptConfigurationPagesSupported;

/**
 * Possible SG element types.
 */
enum MPTSGENTRYTYPE
{
    MPTSGENTRYTYPE_TRANSACTION_CONTEXT = 0x00,
    MPTSGENTRYTYPE_SIMPLE              = 0x01,
    MPTSGENTRYTYPE_CHAIN               = 0x03
};

/**
 * Reply data.
 */
typedef struct LSILOGICSCSIREPLY
{
    /** Lower 32 bits of the reply address in memory. */
    uint32_t      u32HostMFALowAddress;
    /** Full address of the reply in guest memory. */
    RTGCPHYS      GCPhysReplyAddress;
    /** Size of the reply. */
    uint32_t      cbReply;
    /** Different views to the reply depending on the request type. */
    MptReplyUnion Reply;
} LSILOGICSCSIREPLY, *PLSILOGICSCSIREPLY;

/*
 * State of a device attached to the buslogic host adapter.
 */
typedef struct LSILOGICDEVICE
{
    /** Pointer to the owning lsilogic device instance. - R3 pointer */
    R3PTRTYPE(struct LSILOGICSCSI *)  pLsiLogicR3;
    /** Pointer to the owning lsilogic device instance. - R0 pointer */
    R0PTRTYPE(struct LSILOGICSCSI *)  pLsiLogicR0;
    /** Pointer to the owning lsilogic device instance. - RC pointer */
    RCPTRTYPE(struct LSILOGICSCSI *)  pLsiLogicRC;

    /** LUN of the device. */
    RTUINT                        iLUN;
    /** Number of outstanding tasks on the port. */
    volatile uint32_t             cOutstandingRequests;

#if HC_ARCH_BITS == 64
    uint32_t                      Alignment0;
#endif

    /** Our base interace. */
    PDMIBASE                      IBase;
    /** SCSI port interface. */
    PDMISCSIPORT                  ISCSIPort;
    /** Led interface. */
    PDMILEDPORTS                  ILed;
    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)          pDrvBase;
    /** Pointer to the underlying SCSI connector interface. */
    R3PTRTYPE(PPDMISCSICONNECTOR) pDrvSCSIConnector;
    /** The status LED state for this device. */
    PDMLED                        Led;

} LSILOGICDEVICE, *PLSILOGICDEVICE;

/**
 * Defined states that the SCSI controller can have.
 */
typedef enum LSILOGICSTATE
{
    /** Reset state. */
    LSILOGICSTATE_RESET       = 0x00,
    /** Ready state. */
    LSILOGICSTATE_READY       = 0x01,
    /** Operational state. */
    LSILOGICSTATE_OPERATIONAL = 0x02,
    /** Fault state. */
    LSILOGICSTATE_FAULT       = 0x04,
    /** 32bit size hack */
    LSILOGICSTATE_32BIT_HACK  = 0x7fffffff
} LSILOGICSTATE;

/**
 * Which entity needs to initialize the controller
 * to get into the operational state.
 */
typedef enum LSILOGICWHOINIT
{
    /** Not initialized. */
    LSILOGICWHOINIT_NOT_INITIALIZED = 0x00,
    /** System BIOS. */
    LSILOGICWHOINIT_SYSTEM_BIOS     = 0x01,
    /** ROM Bios. */
    LSILOGICWHOINIT_ROM_BIOS        = 0x02,
    /** PCI Peer. */
    LSILOGICWHOINIT_PCI_PEER        = 0x03,
    /** Host driver. */
    LSILOGICWHOINIT_HOST_DRIVER     = 0x04,
    /** Manufacturing. */
    LSILOGICWHOINIT_MANUFACTURING   = 0x05,
    /** 32bit size hack. */
    LSILOGICWHOINIT_32BIT_HACK      = 0x7fffffff
} LSILOGICWHOINIT;


/**
 * IOC status codes.
 */
#define LSILOGIC_IOCSTATUS_SUCCESS                0x0000
#define LSILOGIC_IOCSTATUS_INVALID_FUNCTION       0x0001
#define LSILOGIC_IOCSTATUS_BUSY                   0x0002
#define LSILOGIC_IOCSTATUS_INVALID_SGL            0x0003
#define LSILOGIC_IOCSTATUS_INTERNAL_ERROR         0x0004
#define LSILOGIC_IOCSTATUS_RESERVED               0x0005
#define LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES 0x0006
#define LSILOGIC_IOCSTATUS_INVALID_FIELD          0x0007
#define LSILOGIC_IOCSTATUS_INVALID_STATE          0x0008
#define LSILOGIC_IOCSTATUS_OP_STATE_NOT_SUPPOTED  0x0009

/**
 * Device instance data for the emulated
 * SCSI controller.
 */
typedef struct LSILOGICSCSI
{
    /** PCI device structure. */
    PCIDEVICE            PciDev;
    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3         pDevInsR3;
    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0         pDevInsR0;
    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC         pDevInsRC;

    /** Flag whether the GC part of the device is enabled. */
    bool                 fGCEnabled;
    /** Flag whether the R0 part of the device is enabled. */
    bool                 fR0Enabled;

    /** The state the controller is currently in. */
    LSILOGICSTATE        enmState;
    /** Who needs to init the driver to get into operational state. */
    LSILOGICWHOINIT      enmWhoInit;
    /** Flag whether we are in doorbell function. */
    bool                 fDoorbellInProgress;
    /** Flag whether diagnostic access is enabled. */
    bool                 fDiagnosticEnabled;

    /** Flag whether a notification was send to R3. */
    bool                 fNotificationSend;

    /** Flag whether the guest enabled event notification from the IOC. */
    bool                 fEventNotificationEnabled;

#if HC_ARCH_BITS == 64
    uint32_t             Alignment0;
#endif

    /** Queue to send tasks to R3. - R3 ptr */
    R3PTRTYPE(PPDMQUEUE) pNotificationQueueR3;
    /** Queue to send tasks to R3. - R0 ptr */
    R0PTRTYPE(PPDMQUEUE) pNotificationQueueR0;
    /** Queue to send tasks to R3. - RC ptr */
    RCPTRTYPE(PPDMQUEUE) pNotificationQueueRC;

#if HC_ARCH_BITS == 64
    uint32_t             Alignment1;
#endif

    /** States for attached devices. */
    LSILOGICDEVICE       aDeviceStates[LSILOGIC_DEVICES_MAX];

    /** MMIO address the device is mapped to. */
    RTGCPHYS             GCPhysMMIOBase;
    /** I/O port address the device is mapped to. */
    RTIOPORT             IOPortBase;

    /** Interrupt mask. */
    volatile uint32_t    uInterruptMask;
    /** Interrupt status register. */
    volatile uint32_t    uInterruptStatus;

    /** Buffer for messages which are passed
     * through the doorbell using the
     * handshake method. */
    uint32_t              aMessage[sizeof(MptConfigurationRequest)];
    /** Actual position in the buffer. */
    uint32_t              iMessage;
    /** Size of the message which is given in the doorbell message in dwords. */
    uint32_t              cMessage;

    /** Reply buffer. */
    MptReplyUnion         ReplyBuffer;
    /** Next entry to read. */
    uint32_t              uNextReplyEntryRead;
    /** Size of the reply in the buffer in 16bit words. */
    uint32_t              cReplySize;

    /** The fault code of the I/O controller if we are in the fault state. */
    uint16_t              u16IOCFaultCode;

    /** Upper 32 bits of the moessage frame address to locate requests in guest memory. */
    uint32_t              u32HostMFAHighAddr;
    /** Upper 32 bits of the sense buffer address. */
    uint32_t              u32SenseBufferHighAddr;
    /** Maximum number of devices the driver reported he can handle. */
    uint8_t               cMaxDevices;
    /** Maximum number of buses the driver reported he can handle. */
    uint8_t               cMaxBuses;
    /** Current size of reply message frames in the guest. */
    uint16_t              cbReplyFrame;

    /** Next key to write in the sequence to get access
     *  to diagnostic memory. */
    uint32_t              iDiagnosticAccess;

    /** Number entries allocated for the reply queue. */
    uint32_t              cReplyQueueEntries;
    /** Number entries allocated for the outstanding request queue. */
    uint32_t              cRequestQueueEntries;

    /** Critical section protecting the reply post queue. */
    PDMCRITSECT           ReplyPostQueueCritSect;
    /** Critical section protecting the reply free queue. */
    PDMCRITSECT           ReplyFreeQueueCritSect;

#if HC_ARCH_BITS == 64
    uint32_t              Alignment2;
#endif

    /** Pointer to the start of the reply free queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseR3;
    /** Pointer to the start of the reply post queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pReplyPostQueueBaseR3;
    /** Pointer to the start of the request queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pRequestQueueBaseR3;

    /** Pointer to the start of the reply queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseR0;
    /** Pointer to the start of the reply queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pReplyPostQueueBaseR0;
    /** Pointer to the start of the request queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pRequestQueueBaseR0;

    /** Pointer to the start of the reply queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseRC;
    /** Pointer to the start of the reply queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pReplyPostQueueBaseRC;
    /** Pointer to the start of the request queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pRequestQueueBaseRC;

    /** Next free entry in the reply queue the guest can write a address to. */
    volatile uint32_t     uReplyFreeQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for reply frames from. */
    volatile uint32_t     uReplyFreeQueueNextAddressRead;

    /** Next free entry in the reply queue the guest can write a address to. */
    volatile uint32_t     uReplyPostQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for reply frames from. */
    volatile uint32_t     uReplyPostQueueNextAddressRead;

    /** Next free entry the guest can write a address to a request frame to. */
    volatile uint32_t     uRequestQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for request frames from. */
    volatile uint32_t     uRequestQueueNextAddressRead;

    /** Configuration pages. */
    MptConfigurationPagesSupported ConfigurationPages;

    /** BIOS emulation. */
    VBOXSCSI                       VBoxSCSI;

    /** Cache for allocated tasks. */
    R3PTRTYPE(PRTOBJCACHE)         pTaskCache;

    /** The base interface */
    PDMIBASE                       IBase;
    /** Status Port - Leds interface. */
    PDMILEDPORTS                   ILeds;
    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)  pLedsConnector;
} LSILOGISCSI, *PLSILOGICSCSI;

#define LSILOGIC_PCI_SPACE_IO_SIZE  256
#define LSILOGIC_PCI_SPACE_MEM_SIZE 128 * _1K

#define LSILOGIC_REG_DOORBELL 0x00
# define LSILOGIC_REG_DOORBELL_SET_STATE(enmState)     (((enmState) & 0x0f) << 28)
# define LSILOGIC_REG_DOORBELL_SET_USED(fUsed)         (((fUsed) ? 1 : 0) << 27)
# define LSILOGIC_REG_DOORBELL_SET_WHOINIT(enmWhoInit) (((enmWhoInit) & 0x07) << 24)
# define LSILOGIC_REG_DOORBELL_SET_FAULT_CODE(u16Code) (u16Code)
# define LSILOGIC_REG_DOORBELL_GET_FUNCTION(x)         (((x) & 0xff000000) >> 24)
# define LSILOGIC_REG_DOORBELL_GET_SIZE(x)             (((x) & 0x00ff0000) >> 16)

#define LSILOGIC_REG_WRITE_SEQUENCE    0x04
#define LSILOGIC_REG_HOST_DIAGNOSTIC   0x08
#define LSILOGIC_REG_TEST_BASE_ADDRESS 0x0c
#define LSILOGIC_REG_DIAG_RW_DATA      0x10
#define LSILOGIC_REG_DIAG_RW_ADDRESS   0x14

#define LSILOGIC_REG_HOST_INTR_STATUS  0x30
# define LSILOGIC_REG_HOST_INTR_STATUS_W_MASK (RT_BIT(3))
# define LSILOGIC_REG_HOST_INTR_STATUS_DOORBELL_STS    (RT_BIT(31))
# define LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR      (RT_BIT(3))
# define LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL (RT_BIT(0))

#define LSILOGIC_REG_HOST_INTR_MASK    0x34
# define LSILOGIC_REG_HOST_INTR_MASK_W_MASK (RT_BIT(0) | RT_BIT(3) | RT_BIT(8) | RT_BIT(9))
# define LSILOGIC_REG_HOST_INTR_MASK_IRQ_ROUTING (RT_BIT(8) | RT_BIT(9))
# define LSILOGIC_REG_HOST_INTR_MASK_DOORBELL RT_BIT(0)
# define LSILOGIC_REG_HOST_INTR_MASK_REPLY    RT_BIT(3)

#define LSILOGIC_REG_REQUEST_QUEUE     0x40
#define LSILOGIC_REG_REPLY_QUEUE       0x44

/* Functions which can be passed through the system doorbell. */
#define LSILOGIC_DOORBELL_FUNCTION_IOC_MSG_UNIT_RESET  0x40
#define LSILOGIC_DOORBELL_FUNCTION_IO_UNIT_RESET       0x41
#define LSILOGIC_DOORBELL_FUNCTION_HANDSHAKE           0x42
#define LSILOGIC_DOORBELL_FUNCTION_REPLY_FRAME_REMOVAL 0x43

/**
 * Scatter gather list entry data.
 */
typedef struct LSILOGICTASKSTATESGENTRY
{
    /** Flag whether the buffer in the list is from the guest or an
     *  allocated temporary buffer because the segments in the guest
     *  are not sector aligned.
     */
    bool     fGuestMemory;
    /** Flag whether the buffer contains data or is the destination for the transfer. */
    bool     fBufferContainsData;
    /** Pointer to the start of the buffer. */
    void    *pvBuf;
    /** Size of the buffer. */
    uint32_t cbBuf;
    /** Flag dependent data. */
    union
    {
        /** Data to handle direct mappings of guest buffers. */
        PGMPAGEMAPLOCK  PageLock;
        /** The segment in the guest which is not sector aligned. */
        RTGCPHYS        GCPhysAddrBufferUnaligned;
    } u;
} LSILOGICTASKSTATESGENTRY, *PLSILOGICTASKSTATESGENTRY;

/**
 * Task state object which holds all neccessary data while
 * processing the request from the guest.
 */
typedef struct LSILOGICTASKSTATE
{
    /** Target device. */
    PLSILOGICDEVICE            pTargetDevice;
    /** The message request from the guest. */
    MptRequestUnion            GuestRequest;
    /** Reply message if the request produces one. */
    MptReplyUnion              IOCReply;
    /** SCSI request structure for the SCSI driver. */
    PDMSCSIREQUEST             PDMScsiRequest;
    /** Address of the message request frame in guests memory.
     *  Used to read the S/G entries in the second step. */
    RTGCPHYS                   GCPhysMessageFrameAddr;
    /** Number of scatter gather list entries. */
    uint32_t                   cSGListEntries;
    /** How many entries would fit into the sg list. */
    uint32_t                   cSGListSize;
    /** How many times the list was too big. */
    uint32_t                   cSGListTooBig;
    /** Pointer to the first entry of the scatter gather list. */
    PPDMDATASEG                pSGListHead;
    /** How many entries would fit into the sg info list. */
    uint32_t                   cSGInfoSize;
    /** Number of entries for the information entries. */
    uint32_t                   cSGInfoEntries;
    /** How many times the list was too big. */
    uint32_t                   cSGInfoTooBig;
    /** Pointer to the first mapping information entry. */
    PLSILOGICTASKSTATESGENTRY  paSGEntries;
    /** Size of the temporary buffer for unaligned guest segments. */
    uint32_t                   cbBufferUnaligned;
    /** Pointer to the temporary buffer. */
    void                      *pvBufferUnaligned;
    /** Pointer to the sense buffer. */
    uint8_t                    abSenseBuffer[18];
    /** Flag whether the request was issued from the BIOS. */
    bool                       fBIOS;
} LSILOGICTASKSTATE, *PLSILOGICTASKSTATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) lsilogicIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                        RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) lsilogicIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) lsilogicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser,
                                     RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) lsilogicMMIORead(PPDMDEVINS pDevIns, void *pvUser,
                                    RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) lsilogicDiagnosticWrite(PPDMDEVINS pDevIns, void *pvUser,
                                           RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) lsilogicDiagnosticRead(PPDMDEVINS pDevIns, void *pvUser,
                                          RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
#ifdef IN_RING3
static void lsilogicInitializeConfigurationPages(PLSILOGICSCSI pLsiLogic);
static int lsilogicProcessConfigurationRequest(PLSILOGICSCSI pLsiLogic, PMptConfigurationRequest pConfigurationReq,
                                               PMptConfigurationReply pReply);
#endif
RT_C_DECLS_END

#define PDMIBASE_2_PLSILOGICDEVICE(pInterface)     ( (PLSILOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(LSILOGICDEVICE, IBase)) )
#define PDMISCSIPORT_2_PLSILOGICDEVICE(pInterface) ( (PLSILOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(LSILOGICDEVICE, ISCSIPort)) )
#define PDMILEDPORTS_2_PLSILOGICDEVICE(pInterface) ( (PLSILOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(LSILOGICDEVICE, ILed)) )
#define LSILOGIC_RTGCPHYS_FROM_U32(Hi, Lo)         ( (RTGCPHYS)RT_MAKE_U64(Lo, Hi) )
#define PDMIBASE_2_PLSILOGICSCSI(pInterface)       ( (PLSILOGICSCSI)((uintptr_t)(pInterface) - RT_OFFSETOF(LSILOGICSCSI, IBase)) )
#define PDMILEDPORTS_2_PLSILOGICSCSI(pInterface)   ( (PLSILOGICSCSI)((uintptr_t)(pInterface) - RT_OFFSETOF(LSILOGICSCSI, ILeds)) )

/** Key sequence the guest has to write to enable access
 * to diagnostic memory. */
static const uint8_t g_lsilogicDiagnosticAccess[] = {0x04, 0x0b, 0x02, 0x07, 0x0d};

/**
 * Updates the status of the interrupt pin of the device.
 *
 * @returns nothing.
 * @param   pThis    Pointer to the device instance data.
 */
static void lsilogicUpdateInterrupt(PLSILOGICSCSI pThis)
{
    uint32_t uIntSts;

    LogFlowFunc(("Updating interrupts\n"));

    /* Mask out doorbell status so that it does not affect interrupt updating. */
    uIntSts = (ASMAtomicReadU32(&pThis->uInterruptStatus) & ~LSILOGIC_REG_HOST_INTR_STATUS_DOORBELL_STS);
    /* Check maskable interrupts. */
    uIntSts &= ~(pThis->uInterruptMask & ~LSILOGIC_REG_HOST_INTR_MASK_IRQ_ROUTING);

    if (uIntSts)
    {
        LogFlowFunc(("Setting interrupt\n"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, 1);
    }
    else
    {
        LogFlowFunc(("Clearing interrupt\n"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, 0);
    }
}

/**
 * Sets a given interrupt status bit in the status register and
 * updates the interupt status.
 *
 * @returns nothing.
 * @param   pLsiLogic    Pointer to the device instance.
 * @param   uStatus      The status bit to set.
 */
DECLINLINE(void) lsilogicSetInterrupt(PLSILOGICSCSI pLsiLogic, uint32_t uStatus)
{
    ASMAtomicOrU32(&pLsiLogic->uInterruptStatus, uStatus);
    lsilogicUpdateInterrupt(pLsiLogic);
}

/**
 * Clears a given interrupt status bit in the status register and
 * updates the interupt status.
 *
 * @returns nothing.
 * @param   pLsiLogic    Pointer to the device instance.
 * @param   uStatus      The status bit to set.
 */
DECLINLINE(void) lsilogicClearInterrupt(PLSILOGICSCSI pLsiLogic, uint32_t uStatus)
{
    ASMAtomicAndU32(&pLsiLogic->uInterruptStatus, ~uStatus);
    lsilogicUpdateInterrupt(pLsiLogic);
}

/**
 * Sets the I/O controller into fault state and sets the fault code.
 *
 * @returns nothing
 * @param   pLsiLogic        Pointer to the controller device instance.
 * @param   uIOCFaultCode    Fault code to set.
 */
DECLINLINE(void) lsilogicSetIOCFaultCode(PLSILOGICSCSI pLsiLogic, uint16_t uIOCFaultCode)
{
    if (pLsiLogic->enmState != LSILOGICSTATE_FAULT)
    {
        Log(("%s: Setting I/O controller into FAULT state: uIOCFaultCode=%u\n", __FUNCTION__, uIOCFaultCode));
        pLsiLogic->enmState        = LSILOGICSTATE_FAULT;
        pLsiLogic->u16IOCFaultCode = uIOCFaultCode;
    }
    else
    {
        Log(("%s: We are already in FAULT state\n"));
    }
}

#ifdef IN_RING3
/**
 * Performs a hard reset on the controller.
 *
 * @returns VBox status code.
 * @param   pThis    Pointer to the device instance to initialize.
 */
static int lsilogicHardReset(PLSILOGICSCSI pThis)
{
    pThis->enmState = LSILOGICSTATE_RESET;

    /* The interrupts are masked out. */
    pThis->uInterruptMask |= LSILOGIC_REG_HOST_INTR_MASK_DOORBELL |
                             LSILOGIC_REG_HOST_INTR_MASK_REPLY;
    /* Reset interrupt states. */
    pThis->uInterruptStatus = 0;
    lsilogicUpdateInterrupt(pThis);

    /* Reset the queues. */
    pThis->uReplyFreeQueueNextEntryFreeWrite = 0;
    pThis->uReplyFreeQueueNextAddressRead = 0;
    pThis->uReplyPostQueueNextEntryFreeWrite = 0;
    pThis->uReplyPostQueueNextAddressRead = 0;
    pThis->uRequestQueueNextEntryFreeWrite = 0;
    pThis->uRequestQueueNextAddressRead = 0;

    /* Disable diagnostic access. */
    pThis->iDiagnosticAccess = 0;

    /* Set default values. */
    pThis->cMaxDevices  = LSILOGIC_DEVICES_MAX;
    pThis->cMaxBuses    = 1;
    pThis->cbReplyFrame = 128; /* @todo Figure out where it is needed. */
    /* @todo: Put stuff to reset here. */

    lsilogicInitializeConfigurationPages(pThis);

    /* Mark that we finished performing the reset. */
    pThis->enmState = LSILOGICSTATE_READY;
    return VINF_SUCCESS;
}

/**
 * Finishes a context reply.
 *
 * @returns nothing
 * @param   pLsiLogic            Pointer to the device instance
 * @param   u32MessageContext    The message context ID to post.
 */
static void lsilogicFinishContextReply(PLSILOGICSCSI pLsiLogic, uint32_t u32MessageContext)
{
    int rc;
    AssertMsg(!pLsiLogic->fDoorbellInProgress, ("We are in a doorbell function\n"));

    /* Write message context ID into reply post queue. */
    rc = PDMCritSectEnter(&pLsiLogic->ReplyPostQueueCritSect, VINF_SUCCESS);
    AssertRC(rc);

#if 0
    /* Check for a entry in the queue. */
    if (RT_UNLIKELY(pLsiLogic->uReplyPostQueueNextAddressRead != pLsiLogic->uReplyPostQueueNextEntryFreeWrite))
    {
        /* Set error code. */
        lsilogicSetIOCFaultCode(pLsiLogic, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
        PDMCritSectLeave(&pLsiLogic->ReplyPostQueueCritSect);
        return;
    }
#endif

    /* We have a context reply. */
    ASMAtomicWriteU32(&pLsiLogic->CTX_SUFF(pReplyPostQueueBase)[pLsiLogic->uReplyPostQueueNextEntryFreeWrite], u32MessageContext);
    ASMAtomicIncU32(&pLsiLogic->uReplyPostQueueNextEntryFreeWrite);
    pLsiLogic->uReplyPostQueueNextEntryFreeWrite %= pLsiLogic->cReplyQueueEntries;

    PDMCritSectLeave(&pLsiLogic->ReplyPostQueueCritSect);

    /* Set interrupt. */
    lsilogicSetInterrupt(pLsiLogic, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);
}
#endif /* IN_RING3 */

/**
 * Takes neccessary steps to finish a reply frame.
 *
 * @returns nothing
 * @param   pLsiLogic       Pointer to the device instance
 * @param   pReply          Pointer to the reply message.
 * @param   fForceReplyFifo Flag whether the use of the reply post fifo is forced.
 */
static void lsilogicFinishAddressReply(PLSILOGICSCSI pLsiLogic, PMptReplyUnion pReply, bool fForceReplyFifo)
{
    /*
     * If we are in a doorbell function we set the reply size now and
     * set the system doorbell status interrupt to notify the guest that
     * we are ready to send the reply.
     */
    if (pLsiLogic->fDoorbellInProgress && !fForceReplyFifo)
    {
        /* Set size of the reply in 16bit words. The size in the reply is in 32bit dwords. */
        pLsiLogic->cReplySize = pReply->Header.u8MessageLength * 2;
        Log(("%s: cReplySize=%u\n", __FUNCTION__, pLsiLogic->cReplySize));
        pLsiLogic->uNextReplyEntryRead = 0;
        lsilogicSetInterrupt(pLsiLogic, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
    }
    else
    {
        /*
         * The reply queues are only used if the request was fetched from the request queue.
         * Requests from the request queue are always transferred to R3. So it is not possible
         * that this case happens in R0 or GC.
         */
#ifdef IN_RING3
        int rc;
        /* Grab a free reply message from the queue. */
        rc = PDMCritSectEnter(&pLsiLogic->ReplyFreeQueueCritSect, VINF_SUCCESS);
        AssertRC(rc);

#if 0
        /* Check for a free reply frame. */
        if (RT_UNLIKELY(pLsiLogic->uReplyFreeQueueNextAddressRead != pLsiLogic->uReplyFreeQueueNextEntryFreeWrite))
        {
            /* Set error code. */
            lsilogicSetIOCFaultCode(pLsiLogic, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
            PDMCritSectLeave(&pLsiLogic->ReplyFreeQueueCritSect);
            return;
        }
#endif

        uint32_t u32ReplyFrameAddressLow = pLsiLogic->CTX_SUFF(pReplyFreeQueueBase)[pLsiLogic->uReplyFreeQueueNextAddressRead];

        pLsiLogic->uReplyFreeQueueNextAddressRead++;
        pLsiLogic->uReplyFreeQueueNextAddressRead %= pLsiLogic->cReplyQueueEntries;

        PDMCritSectLeave(&pLsiLogic->ReplyFreeQueueCritSect);

        /* Build 64bit physical address. */
        RTGCPHYS GCPhysReplyMessage = LSILOGIC_RTGCPHYS_FROM_U32(pLsiLogic->u32HostMFAHighAddr, u32ReplyFrameAddressLow);
        size_t cbReplyCopied = (pLsiLogic->cbReplyFrame < sizeof(MptReplyUnion)) ? pLsiLogic->cbReplyFrame : sizeof(MptReplyUnion);

        /* Write reply to guest memory. */
        PDMDevHlpPhysWrite(pLsiLogic->CTX_SUFF(pDevIns), GCPhysReplyMessage, pReply, cbReplyCopied);

        /* Write low 32bits of reply frame into post reply queue. */
        rc = PDMCritSectEnter(&pLsiLogic->ReplyPostQueueCritSect, VINF_SUCCESS);
        AssertRC(rc);

#if 0
        /* Check for a entry in the queue. */
        if (RT_UNLIKELY(pLsiLogic->uReplyPostQueueNextAddressRead != pLsiLogic->uReplyPostQueueNextEntryFreeWrite))
        {
            /* Set error code. */
            lsilogicSetIOCFaultCode(pLsiLogic, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
            PDMCritSectLeave(&pLsiLogic->ReplyPostQueueCritSect);
            return;
        }
#endif

        /* We have a address reply. Set the 31th bit to indicate that. */
        ASMAtomicWriteU32(&pLsiLogic->CTX_SUFF(pReplyPostQueueBase)[pLsiLogic->uReplyPostQueueNextEntryFreeWrite],
                          RT_BIT(31) | (u32ReplyFrameAddressLow >> 1));
        ASMAtomicIncU32(&pLsiLogic->uReplyPostQueueNextEntryFreeWrite);
        pLsiLogic->uReplyPostQueueNextEntryFreeWrite %= pLsiLogic->cReplyQueueEntries;

        PDMCritSectLeave(&pLsiLogic->ReplyPostQueueCritSect);

        if (fForceReplyFifo)
        {
            pLsiLogic->fDoorbellInProgress = false;
            lsilogicSetInterrupt(pLsiLogic, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
        }

        /* Set interrupt. */
        lsilogicSetInterrupt(pLsiLogic, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);
#else
        AssertMsgFailed(("This is not allowed to happen.\n"));
#endif
    }
}

#ifdef IN_RING3
/**
 * Processes a given Request from the guest
 *
 * @returns VBox status code.
 * @param   pLsiLogic    Pointer to the device instance.
 * @param   pMessageHdr  Pointer to the message header of the request.
 * @param   pReply       Pointer to the reply.
 */
static int lsilogicProcessMessageRequest(PLSILOGICSCSI pLsiLogic, PMptMessageHdr pMessageHdr, PMptReplyUnion pReply)
{
    int rc = VINF_SUCCESS;
    bool fForceReplyPostFifo = false;

#ifdef DEBUG
    if (pMessageHdr->u8Function < RT_ELEMENTS(g_apszMPTFunctionNames))
        Log(("Message request function: %s\n", g_apszMPTFunctionNames[pMessageHdr->u8Function]));
    else
        Log(("Message request function: <unknown>\n"));
#endif

    memset(pReply, 0, sizeof(MptReplyUnion));

    switch (pMessageHdr->u8Function)
    {
        case MPT_MESSAGE_HDR_FUNCTION_SCSI_TASK_MGMT:
        {
            PMptSCSITaskManagementRequest pTaskMgmtReq = (PMptSCSITaskManagementRequest)pMessageHdr;

            pReply->SCSITaskManagement.u8MessageLength     = 6;     /* 6 32bit dwords. */
            pReply->SCSITaskManagement.u8TaskType          = pTaskMgmtReq->u8TaskType;
            pReply->SCSITaskManagement.u32TerminationCount = 0;
            //AssertMsgFailed(("todo\n"));
            fForceReplyPostFifo = true;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_IOC_INIT:
        {
            /*
             * This request sets the I/O controller to the
             * operational state.
             */
            PMptIOCInitRequest pIOCInitReq = (PMptIOCInitRequest)pMessageHdr;

            /* Update configuration values. */
            pLsiLogic->enmWhoInit             = (LSILOGICWHOINIT)pIOCInitReq->u8WhoInit;
            pLsiLogic->cbReplyFrame           = pIOCInitReq->u16ReplyFrameSize;
            pLsiLogic->cMaxBuses              = pIOCInitReq->u8MaxBuses;
            pLsiLogic->cMaxDevices            = pIOCInitReq->u8MaxDevices;
            pLsiLogic->u32HostMFAHighAddr     = pIOCInitReq->u32HostMfaHighAddr;
            pLsiLogic->u32SenseBufferHighAddr = pIOCInitReq->u32SenseBufferHighAddr;

            if (pLsiLogic->enmState == LSILOGICSTATE_READY)
            {
                pLsiLogic->enmState = LSILOGICSTATE_OPERATIONAL;
            }

            /* Return reply. */
            pReply->IOCInit.u8MessageLength = 5;
            pReply->IOCInit.u8WhoInit       = pLsiLogic->enmWhoInit;
            pReply->IOCInit.u8MaxDevices    = pLsiLogic->cMaxDevices;
            pReply->IOCInit.u8MaxBuses      = pLsiLogic->cMaxBuses;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_IOC_FACTS:
        {
            pReply->IOCFacts.u8MessageLength      = 15;     /* 15 32bit dwords. */
            pReply->IOCFacts.u16MessageVersion    = 0x0102; /* Version from the specification. */
            pReply->IOCFacts.u8IOCNumber          = 0;      /* PCI function number. */
            pReply->IOCFacts.u16IOCExceptions     = 0;
            pReply->IOCFacts.u8MaxChainDepth      = LSILOGICSCSI_MAXIMUM_CHAIN_DEPTH;
            pReply->IOCFacts.u8WhoInit            = pLsiLogic->enmWhoInit;
            pReply->IOCFacts.u8BlockSize          = 12;     /* Block size in 32bit dwords. This is the largest request we can get (SCSI I/O). */
            pReply->IOCFacts.u8Flags              = 0;      /* Bit 0 is set if the guest must upload the FW prior to using the controller. Obviously not needed here. */
            pReply->IOCFacts.u16ReplyQueueDepth   = pLsiLogic->cReplyQueueEntries - 1; /* One entry is always free. */
            pReply->IOCFacts.u16RequestFrameSize  = 128;    /* @todo Figure out where it is needed. */
            pReply->IOCFacts.u16ProductID         = 0xcafe; /* Our own product ID :) */
            pReply->IOCFacts.u32CurrentHostMFAHighAddr = pLsiLogic->u32HostMFAHighAddr;
            pReply->IOCFacts.u16GlobalCredits     = pLsiLogic->cRequestQueueEntries - 1; /* One entry is always free. */
            pReply->IOCFacts.u8NumberOfPorts      = 1;
            pReply->IOCFacts.u8EventState         = 0; /* Event notifications not enabled. */
            pReply->IOCFacts.u32CurrentSenseBufferHighAddr = pLsiLogic->u32SenseBufferHighAddr;
            pReply->IOCFacts.u16CurReplyFrameSize = pLsiLogic->cbReplyFrame;
            pReply->IOCFacts.u8MaxDevices         = pLsiLogic->cMaxDevices;
            pReply->IOCFacts.u8MaxBuses           = pLsiLogic->cMaxBuses;
            pReply->IOCFacts.u32FwImageSize       = 0; /* No image needed. */
            pReply->IOCFacts.u32FWVersion         = 0;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_PORT_FACTS:
        {
            PMptPortFactsRequest pPortFactsReq = (PMptPortFactsRequest)pMessageHdr;

            pReply->PortFacts.u8MessageLength = 10;
            pReply->PortFacts.u8PortNumber    = pPortFactsReq->u8PortNumber;

            /* This controller only supports one bus with bus number 0. */
            if (pPortFactsReq->u8PortNumber != 0)
            {
                pReply->PortFacts.u8PortType = 0; /* Not existant. */
            }
            else
            {
                pReply->PortFacts.u8PortType             = 0x01; /* SCSI Port. */
                pReply->PortFacts.u16MaxDevices          = LSILOGIC_DEVICES_MAX;
                pReply->PortFacts.u16ProtocolFlags       = RT_BIT(3) | RT_BIT(0); /* SCSI initiator and LUN supported. */
                pReply->PortFacts.u16PortSCSIID          = 7; /* Default */
                pReply->PortFacts.u16MaxPersistentIDs    = 0;
                pReply->PortFacts.u16MaxPostedCmdBuffers = 0; /* Only applies for target mode which we dont support. */
                pReply->PortFacts.u16MaxLANBuckets       = 0; /* Only for the LAN controller. */
            }
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_PORT_ENABLE:
        {
            /*
             * The port enable request notifies the IOC to make the port available and perform
             * appropriate discovery on the associated link.
             */
            PMptPortEnableRequest pPortEnableReq = (PMptPortEnableRequest)pMessageHdr;

            pReply->PortEnable.u8MessageLength = 5;
            pReply->PortEnable.u8PortNumber    = pPortEnableReq->u8PortNumber;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_EVENT_NOTIFICATION:
        {
            PMptEventNotificationRequest pEventNotificationReq = (PMptEventNotificationRequest)pMessageHdr;

            if (pEventNotificationReq->u8Switch)
                pLsiLogic->fEventNotificationEnabled = true;
            else
                pLsiLogic->fEventNotificationEnabled = false;

            pReply->EventNotification.u16EventDataLength = 1; /* 1 32bit D-Word. */
            pReply->EventNotification.u8MessageLength    = 8;
            pReply->EventNotification.u8MessageFlags     = (1 << 7);
            pReply->EventNotification.u8AckRequired      = 0;
            pReply->EventNotification.u32Event           = MPT_EVENT_EVENT_CHANGE;
            pReply->EventNotification.u32EventContext    = 0;
            pReply->EventNotification.u32EventData       = pLsiLogic->fEventNotificationEnabled ? 1 : 0;

            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_EVENT_ACK:
        {
            AssertMsgFailed(("todo"));
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_CONFIG:
        {
            PMptConfigurationRequest pConfigurationReq = (PMptConfigurationRequest)pMessageHdr;

            rc = lsilogicProcessConfigurationRequest(pLsiLogic, pConfigurationReq, &pReply->Configuration);
            AssertRC(rc);
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST: /* Should be handled already. */
        default:
            AssertMsgFailed(("Invalid request function %#x\n", pMessageHdr->u8Function));
    }

    /* Copy common bits from request message frame to reply. */
    pReply->Header.u8Function        = pMessageHdr->u8Function;
    pReply->Header.u32MessageContext = pMessageHdr->u32MessageContext;

    lsilogicFinishAddressReply(pLsiLogic, pReply, fForceReplyPostFifo);
    return rc;
}
#endif

/**
 * Writes a value to a register at a given offset.
 *
 * @returns VBox status code.
 * @param   pThis    Pointer to the LsiLogic SCSI controller instance data.
 * @param   uOffset  Offset of the register to write.
 * @param   pv       Pointer to the value to write
 * @param   cb       Number of bytes to write.
 */
static int lsilogicRegisterWrite(PLSILOGICSCSI pThis, uint32_t uOffset, void *pv, unsigned cb)
{
    uint32_t u32 = *(uint32_t *)pv;
    AssertMsg(cb == 4, ("cb != 4 is %d\n", cb));

    LogFlowFunc(("pThis=%#p uOffset=%#x pv=%#p{%.*Rhxs} cb=%u\n", pThis, uOffset, pv, cb, pv, cb));

    switch (uOffset)
    {
        case LSILOGIC_REG_REPLY_QUEUE:
        {
            /* Add the entry to the reply free queue. */
            ASMAtomicWriteU32(&pThis->CTX_SUFF(pReplyFreeQueueBase)[pThis->uReplyFreeQueueNextEntryFreeWrite], u32);
            pThis->uReplyFreeQueueNextEntryFreeWrite++;
            pThis->uReplyFreeQueueNextEntryFreeWrite %= pThis->cReplyQueueEntries;
            break;
        }
        case LSILOGIC_REG_REQUEST_QUEUE:
        {
            ASMAtomicWriteU32(&pThis->CTX_SUFF(pRequestQueueBase)[pThis->uRequestQueueNextEntryFreeWrite], u32);
            pThis->uRequestQueueNextEntryFreeWrite++;
            pThis->uRequestQueueNextEntryFreeWrite %= pThis->cRequestQueueEntries;

            /* Send notification to R3 if there is not one send already. */
            if (!ASMAtomicXchgBool(&pThis->fNotificationSend, true))
            {
                PPDMQUEUEITEMCORE pNotificationItem = PDMQueueAlloc(pThis->CTX_SUFF(pNotificationQueue));
                AssertPtr(pNotificationItem);

                PDMQueueInsert(pThis->CTX_SUFF(pNotificationQueue), pNotificationItem);
            }
            break;
        }
        case LSILOGIC_REG_DOORBELL:
        {
            /*
             * When the guest writes to this register a real device would set the
             * doorbell status bit in the interrupt status register to indicate that the IOP
             * has still to process the message.
             * The guest needs to wait with posting new messages here until the bit is cleared.
             * Because the guest is not continuing execution while we are here we can skip this.
             */
            if (!pThis->fDoorbellInProgress)
            {
                uint32_t uFunction = LSILOGIC_REG_DOORBELL_GET_FUNCTION(u32);

                switch (uFunction)
                {
                    case LSILOGIC_DOORBELL_FUNCTION_IOC_MSG_UNIT_RESET:
                    {
                        pThis->enmState = LSILOGICSTATE_RESET;

                        /* Reset interrupt states. */
                        pThis->uInterruptMask   = 0;
                        pThis->uInterruptStatus = 0;
                        lsilogicUpdateInterrupt(pThis);

                        /* Reset the queues. */
                        pThis->uReplyFreeQueueNextEntryFreeWrite = 0;
                        pThis->uReplyFreeQueueNextAddressRead = 0;
                        pThis->uReplyPostQueueNextEntryFreeWrite = 0;
                        pThis->uReplyPostQueueNextAddressRead = 0;
                        pThis->uRequestQueueNextEntryFreeWrite = 0;
                        pThis->uRequestQueueNextAddressRead = 0;
                        pThis->enmState = LSILOGICSTATE_READY;
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_IO_UNIT_RESET:
                    {
                        AssertMsgFailed(("todo\n"));
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_HANDSHAKE:
                    {
                        pThis->cMessage = LSILOGIC_REG_DOORBELL_GET_SIZE(u32);
                        pThis->iMessage = 0;
                        AssertMsg(pThis->cMessage <= RT_ELEMENTS(pThis->aMessage),
                                  ("Message doesn't fit into the buffer, cMessage=%u", pThis->cMessage));
                        pThis->fDoorbellInProgress = true;
                        /* Update the interrupt status to notify the guest that a doorbell function was started. */
                        lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_REPLY_FRAME_REMOVAL:
                    {
                        AssertMsgFailed(("todo\n"));
                        break;
                    }
                    default:
                        AssertMsgFailed(("Unknown function %u to perform\n", uFunction));
                }
            }
            else
            {
                /*
                 * We are already performing a doorbell function.
                 * Get the remaining parameters.
                 */
                AssertMsg(pThis->iMessage < RT_ELEMENTS(pThis->aMessage), ("Message is too big to fit into the buffer\n"));
                /*
                 * If the last byte of the message is written, force a switch to R3 because some requests might force
                 * a reply through the FIFO which cannot be handled in GC or R0.
                 */
#ifndef IN_RING3
                if (pThis->iMessage == pThis->cMessage - 1)
                    return VINF_IOM_HC_MMIO_WRITE;
#endif
                pThis->aMessage[pThis->iMessage++] = u32;
#ifdef IN_RING3
                if (pThis->iMessage == pThis->cMessage)
                {
                    int rc = lsilogicProcessMessageRequest(pThis, (PMptMessageHdr)pThis->aMessage, &pThis->ReplyBuffer);
                    AssertRC(rc);
                }
#endif
            }
            break;
        }
        case LSILOGIC_REG_HOST_INTR_STATUS:
        {
            /*
             * Clear the bits the guest wants except the system doorbell interrupt and the IO controller
             * status bit.
             * The former bit is always cleared no matter what the guest writes to the register and
             * the latter one is read only.
             */
            pThis->uInterruptStatus = pThis->uInterruptStatus & ~LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL;

            /*
             * Check if there is still a doorbell function in progress. Set the
             * system doorbell interrupt bit again if it is.
             * We do not use lsilogicSetInterrupt here because the interrupt status
             * is updated afterwards anyway.
             */
            if (   (pThis->fDoorbellInProgress)
                && (pThis->cMessage == pThis->iMessage))
            {
                if (pThis->uNextReplyEntryRead == pThis->cReplySize)
                {
                    /* Reply finished. Reset doorbell in progress status. */
                    Log(("%s: Doorbell function finished\n", __FUNCTION__));
                    pThis->fDoorbellInProgress = false;
                }
                ASMAtomicOrU32(&pThis->uInterruptStatus, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
            }

            lsilogicUpdateInterrupt(pThis);
            break;
        }
        case LSILOGIC_REG_HOST_INTR_MASK:
        {
            pThis->uInterruptMask = (u32 & LSILOGIC_REG_HOST_INTR_MASK_W_MASK);
            lsilogicUpdateInterrupt(pThis);
            break;
        }
        case LSILOGIC_REG_WRITE_SEQUENCE:
        {
            if (pThis->fDiagnosticEnabled)
            {
                /* Any value will cause a reset and disabling access. */
                pThis->fDiagnosticEnabled = false;
                pThis->iDiagnosticAccess  = 0;
            }
            else if ((u32 & 0xf) == g_lsilogicDiagnosticAccess[pThis->iDiagnosticAccess])
            {
                pThis->iDiagnosticAccess++;
                if (pThis->iDiagnosticAccess == RT_ELEMENTS(g_lsilogicDiagnosticAccess))
                {
                    /*
                     * Key sequence successfully written. Enable access to diagnostic
                     * memory and register.
                     */
                    pThis->fDiagnosticEnabled = true;
                }
            }
            else
            {
                /* Wrong value written - reset to beginning. */
                pThis->iDiagnosticAccess = 0;
            }
            break;
        }
        default: /* Ignore. */
        {
            break;
        }
    }
    return VINF_SUCCESS;
}

/**
 * Reads the content of a register at a given offset.
 *
 * @returns VBox status code.
 * @param   pThis    Pointer to the LsiLogic SCSI controller instance data.
 * @param   uOffset  Offset of the register to read.
 * @param   pv       Where to store the content of the register.
 * @param   cb       Number of bytes to read.
 */
static int lsilogicRegisterRead(PLSILOGICSCSI pThis, uint32_t uOffset, void *pv, unsigned cb)
{
    uint32_t *pu32 = (uint32_t *)pv;
    AssertMsg(cb == 4, ("cb != 4 is %d\n", cb));

    switch (uOffset)
    {
        case LSILOGIC_REG_REPLY_QUEUE:
        {
            if (pThis->uReplyPostQueueNextEntryFreeWrite != pThis->uReplyPostQueueNextAddressRead)
            {
                *pu32 = pThis->CTX_SUFF(pReplyPostQueueBase)[pThis->uReplyPostQueueNextAddressRead];
                pThis->uReplyPostQueueNextAddressRead++;
                pThis->uReplyPostQueueNextAddressRead %= pThis->cReplyQueueEntries;
            }
            else
            {
                /* The reply post queue is empty. Reset interrupt. */
                *pu32 = UINT32_C(0xffffffff);
                lsilogicClearInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);
            }
            Log(("%s: Returning address %#x\n", __FUNCTION__, *pu32));
            break;
        }
        case LSILOGIC_REG_DOORBELL:
        {
            *pu32  = LSILOGIC_REG_DOORBELL_SET_STATE(pThis->enmState);
            *pu32 |= LSILOGIC_REG_DOORBELL_SET_USED(pThis->fDoorbellInProgress);
            *pu32 |= LSILOGIC_REG_DOORBELL_SET_WHOINIT(pThis->enmWhoInit);
            /*
             * If there is a doorbell function in progress we pass the return value
             * instead of the status code. We transfer 16bit of the reply
             * during one read.
             */
            if (pThis->fDoorbellInProgress)
            {
                /* Return next 16bit value. */
                *pu32 |= pThis->ReplyBuffer.au16Reply[pThis->uNextReplyEntryRead++];
            }
            else
            {
                /* We return the status code of the I/O controller. */
                *pu32 |= pThis->u16IOCFaultCode;
            }
            break;
        }
        case LSILOGIC_REG_HOST_INTR_STATUS:
        {
            *pu32 = pThis->uInterruptStatus;
            break;
        }
        case LSILOGIC_REG_HOST_INTR_MASK:
        {
            *pu32 = pThis->uInterruptMask;
            break;
        }
        case LSILOGIC_REG_HOST_DIAGNOSTIC:
        {
            //AssertMsgFailed(("todo\n"));
            break;
        }
        case LSILOGIC_REG_TEST_BASE_ADDRESS:
        {
            AssertMsgFailed(("todo\n"));
            break;
        }
        case LSILOGIC_REG_DIAG_RW_DATA:
        {
            AssertMsgFailed(("todo\n"));
            break;
        }
        case LSILOGIC_REG_DIAG_RW_ADDRESS:
        {
            AssertMsgFailed(("todo\n"));
            break;
        }
        default: /* Ignore. */
        {
            break;
        }
    }

    LogFlowFunc(("pThis=%#p uOffset=%#x pv=%#p{%.*Rhxs} cb=%u\n", pThis, uOffset, pv, cb, pv, cb));

    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) lsilogicIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                        RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t   uOffset = Port - pThis->IOPortBase;

    Assert(cb <= 4);

    int rc = lsilogicRegisterWrite(pThis, uOffset, &u32, cb);
    if (rc == VINF_IOM_HC_MMIO_WRITE)
        rc = VINF_IOM_HC_IOPORT_WRITE;

    return rc;
}

PDMBOTHCBDECL(int) lsilogicIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t   uOffset = Port - pThis->IOPortBase;

    Assert(cb <= 4);

    return lsilogicRegisterRead(pThis, uOffset, pu32, cb);
}

PDMBOTHCBDECL(int) lsilogicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser,
                                     RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t   uOffset = GCPhysAddr - pThis->GCPhysMMIOBase;

    return lsilogicRegisterWrite(pThis, uOffset, pv, cb);
}

PDMBOTHCBDECL(int) lsilogicMMIORead(PPDMDEVINS pDevIns, void *pvUser,
                                    RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t   uOffset = GCPhysAddr - pThis->GCPhysMMIOBase;

    return lsilogicRegisterRead(pThis, uOffset, pv, cb);
}

PDMBOTHCBDECL(int) lsilogicDiagnosticWrite(PPDMDEVINS pDevIns, void *pvUser,
                                           RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("pThis=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pThis, GCPhysAddr, pv, cb, pv, cb));

    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) lsilogicDiagnosticRead(PPDMDEVINS pDevIns, void *pvUser,
                                          RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("pThis=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pThis, GCPhysAddr, pv, cb, pv, cb));

    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * Copies a contigous buffer into the scatter gather list provided by the guest.
 *
 * @returns nothing
 * @param   pTaskState    Pointer to the task state which contains the SGL.
 * @param   pvBuf         Pointer to the buffer to copy.
 * @param   cbCopy        Number of bytes to copy.
 */
static void lsilogicScatterGatherListCopyFromBuffer(PLSILOGICTASKSTATE pTaskState, void *pvBuf, size_t cbCopy)
{
    unsigned cSGEntry = 0;
    PPDMDATASEG pSGEntry = &pTaskState->pSGListHead[cSGEntry];
    uint8_t *pu8Buf = (uint8_t *)pvBuf;

    while (cSGEntry < pTaskState->cSGListEntries)
    {
        size_t cbToCopy = (cbCopy < pSGEntry->cbSeg) ? cbCopy : pSGEntry->cbSeg;

        memcpy(pSGEntry->pvSeg, pu8Buf, cbToCopy);

        cbCopy -= cbToCopy;
        /* We finished. */
        if (!cbCopy)
            break;

        /* Advance the buffer. */
        pu8Buf += cbToCopy;

        /* Go to the next entry in the list. */
        pSGEntry++;
        cSGEntry++;
    }
}

/**
 * Copy a temporary buffer into a part of the guest scatter gather list
 * described by the given descriptor entry.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to write to which are unaligned.
 */
static void lsilogicCopyFromBufferIntoSGList(PPDMDEVINS pDevIns, PLSILOGICTASKSTATESGENTRY pSGInfo)
{
    RTGCPHYS GCPhysBuffer = pSGInfo->u.GCPhysAddrBufferUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    /* Copy into SG entry. */
    PDMDevHlpPhysWrite(pDevIns, GCPhysBuffer, pSGInfo->pvBuf, pSGInfo->cbBuf);

}

/**
 * Copy a part of the guest scatter gather list into a temporary buffer.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to read from which are unaligned.
 */
static void lsilogicCopyFromSGListIntoBuffer(PPDMDEVINS pDevIns, PLSILOGICTASKSTATESGENTRY pSGInfo)
{
    RTGCPHYS GCPhysBuffer = pSGInfo->u.GCPhysAddrBufferUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    /* Copy into temporary buffer. */
    PDMDevHlpPhysRead(pDevIns, GCPhysBuffer, pSGInfo->pvBuf, pSGInfo->cbBuf);
}

static int lsilogicScatterGatherListAllocate(PLSILOGICTASKSTATE pTaskState, uint32_t cSGList, uint32_t cSGInfo, uint32_t cbUnaligned)
{
    if (pTaskState->cSGListSize < cSGList)
    {
        /* The entries are not allocated yet or the number is too small. */
        if (pTaskState->cSGListSize)
            RTMemFree(pTaskState->pSGListHead);

        /* Allocate R3 scatter gather list. */
        pTaskState->pSGListHead = (PPDMDATASEG)RTMemAllocZ(cSGList * sizeof(PDMDATASEG));
        if (!pTaskState->pSGListHead)
            return VERR_NO_MEMORY;

        /* Reset usage statistics. */
        pTaskState->cSGListSize     = cSGList;
        pTaskState->cSGListEntries  = cSGList;
        pTaskState->cSGListTooBig = 0;
    }
    else if (pTaskState->cSGListSize > cSGList)
    {
        /*
         * The list is too big. Increment counter.
         * So that the destroying function can free
         * the list if it is too big too many times
         * in a row.
         */
        pTaskState->cSGListEntries = cSGList;
        pTaskState->cSGListTooBig++;
    }
    else
    {
        /*
         * Needed entries matches current size.
         * Reset counter.
         */
        pTaskState->cSGListEntries = cSGList;
        pTaskState->cSGListTooBig  = 0;
    }

    if (pTaskState->cSGInfoSize < cSGInfo)
    {
        /* The entries are not allocated yet or the number is too small. */
        if (pTaskState->cSGInfoSize)
            RTMemFree(pTaskState->paSGEntries);

        pTaskState->paSGEntries = (PLSILOGICTASKSTATESGENTRY)RTMemAllocZ(cSGInfo * sizeof(LSILOGICTASKSTATESGENTRY));
        if (!pTaskState->paSGEntries)
            return VERR_NO_MEMORY;

        /* Reset usage statistics. */
        pTaskState->cSGInfoSize = cSGInfo;
        pTaskState->cSGInfoEntries  = cSGInfo;
        pTaskState->cSGInfoTooBig = 0;
    }
    else if (pTaskState->cSGInfoSize > cSGInfo)
    {
        /*
         * The list is too big. Increment counter.
         * So that the destroying function can free
         * the list if it is too big too many times
         * in a row.
         */
        pTaskState->cSGInfoEntries = cSGInfo;
        pTaskState->cSGInfoTooBig++;
    }
    else
    {
        /*
         * Needed entries matches current size.
         * Reset counter.
         */
        pTaskState->cSGInfoEntries  = cSGInfo;
        pTaskState->cSGInfoTooBig = 0;
    }


    if (pTaskState->cbBufferUnaligned < cbUnaligned)
    {
        if (pTaskState->pvBufferUnaligned)
            RTMemFree(pTaskState->pvBufferUnaligned);

        Log(("%s: Allocating buffer for unaligned segments cbUnaligned=%u\n", __FUNCTION__, cbUnaligned));

        pTaskState->pvBufferUnaligned = RTMemAllocZ(cbUnaligned);
        if (!pTaskState->pvBufferUnaligned)
            return VERR_NO_MEMORY;

        pTaskState->cbBufferUnaligned = cbUnaligned;
    }

    /* Make debugging easier. */
#ifdef DEBUG
    memset(pTaskState->pSGListHead, 0, pTaskState->cSGListSize * sizeof(PDMDATASEG));
    memset(pTaskState->paSGEntries, 0, pTaskState->cSGInfoSize * sizeof(LSILOGICTASKSTATESGENTRY));
    if (pTaskState->pvBufferUnaligned)
        memset(pTaskState->pvBufferUnaligned, 0, pTaskState->cbBufferUnaligned);
#endif
    return VINF_SUCCESS;
}

/**
 * Destroy a scatter gather list.
 *
 * @returns nothing.
 * @param   pLsiLogic    Pointer to the LsiLogic SCSI controller.
 * @param   pTaskState   Pointer to the task state.
 */
static void lsilogicScatterGatherListDestroy(PLSILOGICSCSI pLsiLogic, PLSILOGICTASKSTATE pTaskState)
{
    PPDMDEVINS                pDevIns     = pLsiLogic->CTX_SUFF(pDevIns);
    PLSILOGICTASKSTATESGENTRY pSGInfoCurr = pTaskState->paSGEntries;

    for (unsigned i = 0; i < pTaskState->cSGInfoEntries; i++)
    {
        if (pSGInfoCurr->fGuestMemory)
        {
            /* Release the lock. */
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &pSGInfoCurr->u.PageLock);
        }
        else if (!pSGInfoCurr->fBufferContainsData)
        {
            /* Copy the data into the guest segments now. */
            lsilogicCopyFromBufferIntoSGList(pLsiLogic->CTX_SUFF(pDevIns), pSGInfoCurr);
        }

        pSGInfoCurr++;
    }

    /* Free allocated memory if the list was too big too many times. */
    if (pTaskState->cSGListTooBig >= LSILOGIC_NR_OF_ALLOWED_BIGGER_LISTS)
    {
        RTMemFree(pTaskState->pSGListHead);
        RTMemFree(pTaskState->paSGEntries);
        if (pTaskState->pvBufferUnaligned)
            RTMemFree(pTaskState->pvBufferUnaligned);
        pTaskState->cSGListSize = 0;
        pTaskState->cSGInfoSize = 0;
        pTaskState->cSGInfoEntries = 0;
        pTaskState->cSGListTooBig  = 0;
        pTaskState->pSGListHead = NULL;
        pTaskState->paSGEntries = NULL;
        pTaskState->pvBufferUnaligned = NULL;
        pTaskState->cbBufferUnaligned = 0;
    }
}

#ifdef DEBUG
/**
 * Dump an SG entry.
 *
 * @returns nothing.
 * @param   pSGEntry    Pointer to the SG entry to dump
 */
static void lsilogicDumpSGEntry(PMptSGEntryUnion pSGEntry)
{
    switch (pSGEntry->Simple32.u2ElementType)
    {
        case MPTSGENTRYTYPE_SIMPLE:
        {
            Log(("%s: Dumping info for SIMPLE SG entry:\n", __FUNCTION__));
            Log(("%s: u24Length=%u\n", __FUNCTION__, pSGEntry->Simple32.u24Length));
            Log(("%s: fEndOfList=%d\n", __FUNCTION__, pSGEntry->Simple32.fEndOfList));
            Log(("%s: f64BitAddress=%d\n", __FUNCTION__, pSGEntry->Simple32.f64BitAddress));
            Log(("%s: fBufferContainsData=%d\n", __FUNCTION__, pSGEntry->Simple32.fBufferContainsData));
            Log(("%s: fLocalAddress=%d\n", __FUNCTION__, pSGEntry->Simple32.fLocalAddress));
            Log(("%s: fEndOfBuffer=%d\n", __FUNCTION__, pSGEntry->Simple32.fEndOfBuffer));
            Log(("%s: fLastElement=%d\n", __FUNCTION__, pSGEntry->Simple32.fLastElement));
            Log(("%s: u32DataBufferAddressLow=%u\n", __FUNCTION__, pSGEntry->Simple32.u32DataBufferAddressLow));
            if (pSGEntry->Simple32.f64BitAddress)
            {
                Log(("%s: u32DataBufferAddressHigh=%u\n", __FUNCTION__, pSGEntry->Simple64.u32DataBufferAddressHigh));
                Log(("%s: GCDataBufferAddress=%RGp\n", __FUNCTION__,
                    ((uint64_t)pSGEntry->Simple64.u32DataBufferAddressHigh << 32) | pSGEntry->Simple64.u32DataBufferAddressLow));
            }
            else
                Log(("%s: GCDataBufferAddress=%RGp\n", __FUNCTION__, pSGEntry->Simple32.u32DataBufferAddressLow));

            break;
        }
        case MPTSGENTRYTYPE_CHAIN:
        {
            Log(("%s: Dumping info for CHAIN SG entry:\n", __FUNCTION__));
            Log(("%s: u16Length=%u\n", __FUNCTION__, pSGEntry->Chain.u16Length));
            Log(("%s: u8NExtChainOffset=%d\n", __FUNCTION__, pSGEntry->Chain.u8NextChainOffset));
            Log(("%s: f64BitAddress=%d\n", __FUNCTION__, pSGEntry->Chain.f64BitAddress));
            Log(("%s: fLocalAddress=%d\n", __FUNCTION__, pSGEntry->Chain.fLocalAddress));
            Log(("%s: u32SegmentAddressLow=%u\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressLow));
            Log(("%s: u32SegmentAddressHigh=%u\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressHigh));
            if (pSGEntry->Chain.f64BitAddress)
                Log(("%s: GCSegmentAddress=%RGp\n", __FUNCTION__,
                    ((uint64_t)pSGEntry->Chain.u32SegmentAddressHigh << 32) | pSGEntry->Chain.u32SegmentAddressLow));
            else
                Log(("%s: GCSegmentAddress=%RGp\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressLow));
            break;
        }
    }
}
#endif

/**
 * Create scatter gather list descriptors.
 *
 * @returns VBox status code.
 * @param   pLsiLogic      Pointer to the LsiLogic SCSI controller.
 * @param   pTaskState     Pointer to the task state.
 * @param   GCPhysSGLStart Guest physical address of the first SG entry.
 * @param   uChainOffset   Offset in bytes from the beginning of the SGL segment to the chain element.
 * @thread  EMT
 */
static int lsilogicScatterGatherListCreate(PLSILOGICSCSI pLsiLogic, PLSILOGICTASKSTATE pTaskState,
                                           RTGCPHYS GCPhysSGLStart, uint32_t uChainOffset)
{
    int                        rc           = VINF_SUCCESS;
    PPDMDEVINS                 pDevIns      = pLsiLogic->CTX_SUFF(pDevIns);
    PVM                        pVM          = PDMDevHlpGetVM(pDevIns);
    bool                       fUnaligned;     /* Flag whether the current buffer is unaligned. */
    uint32_t                   cbUnaligned;    /* Size of the unaligned buffers. */
    uint32_t                   cSGEntriesR3 = 0;
    uint32_t                   cSGInfo      = 0;
    uint32_t                   cbSegment    = 0;
    PLSILOGICTASKSTATESGENTRY  pSGInfoCurr  = NULL;
    uint8_t                   *pu8BufferUnalignedPos = NULL;
    uint8_t                   *pbBufferUnalignedSGInfoPos = NULL;
    uint32_t                   cbUnalignedComplete = 0;
    bool                       fDoMapping = false;
    bool                       fEndOfList;
    RTGCPHYS                   GCPhysSGEntryNext;
    RTGCPHYS                   GCPhysSegmentStart;
    uint32_t                   uChainOffsetNext;

    /*
     * Two passes - one to count needed scatter gather list entries and needed unaligned
     * buffers and one to actually map the SG list into R3.
     */
    for (int i = 0; i < 2; i++)
    {
        fUnaligned      = false;
        cbUnaligned     = 0;
        fEndOfList      = false;

        GCPhysSGEntryNext  = GCPhysSGLStart;
        uChainOffsetNext   = uChainOffset;
        GCPhysSegmentStart = GCPhysSGLStart;

        if (fDoMapping)
        {
            Log(("%s: cSGInfo=%u\n", __FUNCTION__, cSGInfo));

            /* The number of needed SG entries in R3 is known. Allocate needed memory. */
            rc = lsilogicScatterGatherListAllocate(pTaskState, cSGInfo, cSGInfo, cbUnalignedComplete);
            AssertMsgRC(rc, ("Failed to allocate scatter gather array rc=%Rrc\n", rc));

            /* We are now able to map the pages into R3. */
            pSGInfoCurr = pTaskState->paSGEntries;
            /* Initialize first segment to remove the need for additional if checks later in the code. */
            pSGInfoCurr->fGuestMemory= false;
            pu8BufferUnalignedPos = (uint8_t *)pTaskState->pvBufferUnaligned;
            pbBufferUnalignedSGInfoPos = pu8BufferUnalignedPos;
        }

        /* Go through the list until we reach the end. */
        while (!fEndOfList)
        {
            bool fEndOfSegment = false;

            while (!fEndOfSegment)
            {
                MptSGEntryUnion SGEntry;

                Log(("%s: Reading SG entry from %RGp\n", __FUNCTION__, GCPhysSGEntryNext));

                /* Read the entry. */
                PDMDevHlpPhysRead(pDevIns, GCPhysSGEntryNext, &SGEntry, sizeof(MptSGEntryUnion));

#ifdef DEBUG
                lsilogicDumpSGEntry(&SGEntry);
#endif

                AssertMsg(SGEntry.Simple32.u2ElementType == MPTSGENTRYTYPE_SIMPLE, ("Invalid SG entry type\n"));

                /* Check if this is a zero element. */
                if (   !SGEntry.Simple32.u24Length
                    && SGEntry.Simple32.fEndOfList
                    && SGEntry.Simple32.fEndOfBuffer)
                {
                    pTaskState->cSGListEntries = 0;
                    pTaskState->cSGInfoEntries = 0;
                    return VINF_SUCCESS;
                }

                uint32_t cbDataToTransfer     = SGEntry.Simple32.u24Length;
                bool     fBufferContainsData  = !!SGEntry.Simple32.fBufferContainsData;
                RTGCPHYS GCPhysAddrDataBuffer = SGEntry.Simple32.u32DataBufferAddressLow;

                if (SGEntry.Simple32.f64BitAddress)
                {
                    GCPhysAddrDataBuffer |= ((uint64_t)SGEntry.Simple64.u32DataBufferAddressHigh) << 32;
                    GCPhysSGEntryNext += sizeof(MptSGEntrySimple64);
                }
                else
                    GCPhysSGEntryNext += sizeof(MptSGEntrySimple32);

                if (fDoMapping)
                {
                    pSGInfoCurr->fGuestMemory = false;
                    pSGInfoCurr->fBufferContainsData = fBufferContainsData;
                    pSGInfoCurr->cbBuf = cbDataToTransfer;
                    pSGInfoCurr->pvBuf = pbBufferUnalignedSGInfoPos;
                    pbBufferUnalignedSGInfoPos += cbDataToTransfer;
                    pSGInfoCurr->u.GCPhysAddrBufferUnaligned = GCPhysAddrDataBuffer;
                    if (fBufferContainsData)
                        lsilogicCopyFromSGListIntoBuffer(pDevIns, pSGInfoCurr);
                    pSGInfoCurr++;
                }
                else
                {
                    cbUnalignedComplete += cbDataToTransfer;
                    cSGInfo++;
                }

                /* Check if we reached the end of the list. */
                if (SGEntry.Simple32.fEndOfList)
                {
                    /* We finished. */
                    fEndOfSegment = true;
                    fEndOfList = true;
                }
                else if (SGEntry.Simple32.fLastElement)
                {
                    fEndOfSegment = true;
                }
            } /* while (!fEndOfSegment) */

            /* Get next chain element. */
            if (uChainOffsetNext)
            {
                MptSGEntryChain SGEntryChain;

                PDMDevHlpPhysRead(pDevIns, GCPhysSegmentStart + uChainOffsetNext, &SGEntryChain, sizeof(MptSGEntryChain));

                AssertMsg(SGEntryChain.u2ElementType == MPTSGENTRYTYPE_CHAIN, ("Invalid SG entry type\n"));

               /* Set the next address now. */
                GCPhysSGEntryNext = SGEntryChain.u32SegmentAddressLow;
                if (SGEntryChain.f64BitAddress)
                    GCPhysSGEntryNext |= ((uint64_t)SGEntryChain.u32SegmentAddressHigh) << 32;

                GCPhysSegmentStart = GCPhysSGEntryNext;
                uChainOffsetNext   = SGEntryChain.u8NextChainOffset * sizeof(uint32_t);
            }

        } /* while (!fEndOfList) */

        fDoMapping = true;
        if (fUnaligned)
            cbUnalignedComplete += cbUnaligned;
    }

    uint32_t    cSGEntries;
    PPDMDATASEG pSGEntryCurr = pTaskState->pSGListHead;
    pSGInfoCurr              = pTaskState->paSGEntries;

    /* Initialize first entry. */
    pSGEntryCurr->pvSeg = pSGInfoCurr->pvBuf;
    pSGEntryCurr->cbSeg = pSGInfoCurr->cbBuf;
    pSGInfoCurr++;
    cSGEntries = 1;

    /* Construct the scatter gather list. */
    for (unsigned i = 0; i < (pTaskState->cSGInfoEntries-1); i++)
    {
        if (pSGEntryCurr->cbSeg % 512 != 0)
        {
            AssertMsg((uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg == pSGInfoCurr->pvBuf,
                      ("Buffer ist not sector aligned but the buffer addresses are not adjacent\n"));

            pSGEntryCurr->cbSeg += pSGInfoCurr->cbBuf;
        }
        else
        {
            if (((uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg) == pSGInfoCurr->pvBuf)
            {
                pSGEntryCurr->cbSeg += pSGInfoCurr->cbBuf;
            }
            else
            {
                pSGEntryCurr++;
                cSGEntries++;
                pSGEntryCurr->pvSeg = pSGInfoCurr->pvBuf;
                pSGEntryCurr->cbSeg = pSGInfoCurr->cbBuf;
            }
        }

        pSGInfoCurr++;
    }

    pTaskState->cSGListEntries = cSGEntries;

    return rc;
}

/*
 * Disabled because the sense buffer provided by the LsiLogic driver for Windows XP
 * crosses page boundaries.
 */
#if 0
/**
 * Free the sense buffer.
 *
 * @returns nothing.
 * @param   pTaskState   Pointer to the task state.
 */
static void lsilogicFreeGCSenseBuffer(PLSILOGICSCSI pLsiLogic, PLSILOGICTASKSTATE pTaskState)
{
    PVM pVM = PDMDevHlpGetVM(pLsiLogic->CTX_SUFF(pDevIns));

    PGMPhysReleasePageMappingLock(pVM, &pTaskState->PageLockSense);
    pTaskState->pbSenseBuffer = NULL;
}

/**
 * Map the sense buffer into R3.
 *
 * @returns VBox status code.
 * @param   pTaskState    Pointer to the task state.
 * @note Current assumption is that the sense buffer is not scattered and does not cross a page boundary.
 */
static int lsilogicMapGCSenseBufferIntoR3(PLSILOGICSCSI pLsiLogic, PLSILOGICTASKSTATE pTaskState)
{
    int rc = VINF_SUCCESS;
    PPDMDEVINS pDevIns = pLsiLogic->CTX_SUFF(pDevIns);
    RTGCPHYS GCPhysAddrSenseBuffer;

    GCPhysAddrSenseBuffer = pTaskState->GuestRequest.SCSIIO.u32SenseBufferLowAddress;
    GCPhysAddrSenseBuffer |= ((uint64_t)pLsiLogic->u32SenseBufferHighAddr << 32);

#ifdef RT_STRICT
    uint32_t cbSenseBuffer = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
#endif
    RTGCPHYS GCPhysAddrSenseBufferBase = PAGE_ADDRESS(GCPhysAddrSenseBuffer);

    AssertMsg(GCPhysAddrSenseBuffer >= GCPhysAddrSenseBufferBase,
              ("Impossible GCPhysAddrSenseBuffer < GCPhysAddrSenseBufferBase\n"));

    /* Sanity checks for the assumption. */
    AssertMsg(((GCPhysAddrSenseBuffer + cbSenseBuffer) <= (GCPhysAddrSenseBufferBase + PAGE_SIZE)),
              ("Sense buffer crosses page boundary\n"));

    rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrSenseBufferBase, (void **)&pTaskState->pbSenseBuffer, &pTaskState->PageLockSense);
    AssertMsgRC(rc, ("Mapping sense buffer failed rc=%Rrc\n", rc));

    /* Correct start address of the sense buffer. */
    pTaskState->pbSenseBuffer += (GCPhysAddrSenseBuffer - GCPhysAddrSenseBufferBase);

    return rc;
}
#endif

#ifdef DEBUG
static void lsilogicDumpSCSIIORequest(PMptSCSIIORequest pSCSIIORequest)
{
    Log(("%s: u8TargetID=%d\n", __FUNCTION__, pSCSIIORequest->u8TargetID));
    Log(("%s: u8Bus=%d\n", __FUNCTION__, pSCSIIORequest->u8Bus));
    Log(("%s: u8ChainOffset=%d\n", __FUNCTION__, pSCSIIORequest->u8ChainOffset));
    Log(("%s: u8Function=%d\n", __FUNCTION__, pSCSIIORequest->u8Function));
    Log(("%s: u8CDBLength=%d\n", __FUNCTION__, pSCSIIORequest->u8CDBLength));
    Log(("%s: u8SenseBufferLength=%d\n", __FUNCTION__, pSCSIIORequest->u8SenseBufferLength));
    Log(("%s: u8MessageFlags=%d\n", __FUNCTION__, pSCSIIORequest->u8MessageFlags));
    Log(("%s: u32MessageContext=%#x\n", __FUNCTION__, pSCSIIORequest->u32MessageContext));
    for (unsigned i = 0; i < RT_ELEMENTS(pSCSIIORequest->au8LUN); i++)
        Log(("%s: u8LUN[%d]=%d\n", __FUNCTION__, i, pSCSIIORequest->au8LUN[i]));
    Log(("%s: u32Control=%#x\n", __FUNCTION__, pSCSIIORequest->u32Control));
    for (unsigned i = 0; i < RT_ELEMENTS(pSCSIIORequest->au8CDB); i++)
        Log(("%s: u8CDB[%d]=%d\n", __FUNCTION__, i, pSCSIIORequest->au8CDB[i]));
    Log(("%s: u32DataLength=%#x\n", __FUNCTION__, pSCSIIORequest->u32DataLength));
    Log(("%s: u32SenseBufferLowAddress=%#x\n", __FUNCTION__, pSCSIIORequest->u32SenseBufferLowAddress));
}
#endif

/**
 * Processes a SCSI I/O request by setting up the request
 * and sending it to the underlying SCSI driver.
 * Steps needed to complete request are done in the
 * callback called by the driver below upon completion of
 * the request.
 *
 * @returns VBox status code.
 * @param   pLsiLogic    Pointer to the device instance which sends the request.
 * @param   pTaskState   Pointer to the task state data.
 */
static int lsilogicProcessSCSIIORequest(PLSILOGICSCSI pLsiLogic, PLSILOGICTASKSTATE pTaskState)
{
    int rc = VINF_SUCCESS;

#ifdef DEBUG
    lsilogicDumpSCSIIORequest(&pTaskState->GuestRequest.SCSIIO);
#endif

    pTaskState->fBIOS = false;

    uint32_t uChainOffset = pTaskState->GuestRequest.SCSIIO.u8ChainOffset;

    if (uChainOffset)
        uChainOffset = uChainOffset * sizeof(uint32_t) - sizeof(MptSCSIIORequest);

    /* Create Scatter gather list. */
    rc = lsilogicScatterGatherListCreate(pLsiLogic, pTaskState,
                                         pTaskState->GCPhysMessageFrameAddr + sizeof(MptSCSIIORequest),
                                         uChainOffset);
    AssertRC(rc);

#if 0
    /* Map sense buffer. */
    rc = lsilogicMapGCSenseBufferIntoR3(pLsiLogic, pTaskState);
    AssertRC(rc);
#endif

    if (RT_LIKELY(   (pTaskState->GuestRequest.SCSIIO.u8TargetID < LSILOGIC_DEVICES_MAX)
                  && (pTaskState->GuestRequest.SCSIIO.u8Bus == 0)))
    {
        PLSILOGICDEVICE pTargetDevice;
        pTargetDevice = &pLsiLogic->aDeviceStates[pTaskState->GuestRequest.SCSIIO.u8TargetID];

        if (pTargetDevice->pDrvBase)
        {
            /* Setup the SCSI request. */
            pTaskState->pTargetDevice                        = pTargetDevice;
            pTaskState->PDMScsiRequest.uLogicalUnit          = pTaskState->GuestRequest.SCSIIO.au8LUN[1];

            uint8_t uDataDirection = MPT_SCSIIO_REQUEST_CONTROL_TXDIR_GET(pTaskState->GuestRequest.SCSIIO.u32Control);

            if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_NONE)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_NONE;
            else if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_WRITE)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_TO_DEVICE;
            else if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_READ)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_FROM_DEVICE;

            pTaskState->PDMScsiRequest.cbCDB                 = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
            pTaskState->PDMScsiRequest.pbCDB                 = pTaskState->GuestRequest.SCSIIO.au8CDB;
            pTaskState->PDMScsiRequest.cbScatterGather       = pTaskState->GuestRequest.SCSIIO.u32DataLength;
            pTaskState->PDMScsiRequest.cScatterGatherEntries = pTaskState->cSGListEntries;
            pTaskState->PDMScsiRequest.paScatterGatherHead   = pTaskState->pSGListHead;
            pTaskState->PDMScsiRequest.cbSenseBuffer         = sizeof(pTaskState->abSenseBuffer);
            memset(pTaskState->abSenseBuffer, 0, pTaskState->PDMScsiRequest.cbSenseBuffer);
            pTaskState->PDMScsiRequest.pbSenseBuffer         = pTaskState->abSenseBuffer;
            pTaskState->PDMScsiRequest.pvUser                = pTaskState;

            ASMAtomicIncU32(&pTargetDevice->cOutstandingRequests);
            rc = pTargetDevice->pDrvSCSIConnector->pfnSCSIRequestSend(pTargetDevice->pDrvSCSIConnector, &pTaskState->PDMScsiRequest);
            AssertMsgRC(rc, ("Sending request to SCSI layer failed rc=%Rrc\n", rc));
            return VINF_SUCCESS;
        }
        else
        {
            /* Device is not present report SCSI selection timeout. */
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_DEVICE_NOT_THERE;
        }
    }
    else
    {
        /* Report out of bounds target ID or bus. */
        if (pTaskState->GuestRequest.SCSIIO.u8Bus != 0)
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_BUS;
        else
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_TARGETID;
    }

    /* The rest is equal to both errors. */
    pTaskState->IOCReply.SCSIIOError.u8TargetID = pTaskState->GuestRequest.SCSIIO.u8TargetID;
    pTaskState->IOCReply.SCSIIOError.u8Bus = pTaskState->GuestRequest.SCSIIO.u8Bus;
    pTaskState->IOCReply.SCSIIOError.u8MessageLength = sizeof(MptSCSIIOErrorReply) / 4;
    pTaskState->IOCReply.SCSIIOError.u8Function = pTaskState->GuestRequest.SCSIIO.u8Function;
    pTaskState->IOCReply.SCSIIOError.u8CDBLength = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
    pTaskState->IOCReply.SCSIIOError.u8SenseBufferLength = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
    pTaskState->IOCReply.SCSIIOError.u32MessageContext = pTaskState->GuestRequest.SCSIIO.u32MessageContext;
    pTaskState->IOCReply.SCSIIOError.u8SCSIStatus = SCSI_STATUS_OK;
    pTaskState->IOCReply.SCSIIOError.u8SCSIState  = MPT_SCSI_IO_ERROR_SCSI_STATE_TERMINATED;
    pTaskState->IOCReply.SCSIIOError.u32IOCLogInfo    = 0;
    pTaskState->IOCReply.SCSIIOError.u32TransferCount = 0;
    pTaskState->IOCReply.SCSIIOError.u32SenseCount    = 0;
    pTaskState->IOCReply.SCSIIOError.u32ResponseInfo  = 0;

    lsilogicFinishAddressReply(pLsiLogic, &pTaskState->IOCReply, false);
    RTCacheInsert(pLsiLogic->pTaskCache, pTaskState);

    return rc;
}

/**
 * Called upon completion of the request from the SCSI driver below.
 * This function frees all allocated ressources and notifies the guest
 * that the process finished by asserting an interrupt.
 *
 * @returns VBox status code.
 * @param   pInterface    Pointer to the interface the called funtion belongs to.
 * @param   pSCSIRequest  Pointer to the SCSI request which finished.
 */
static DECLCALLBACK(int) lsilogicDeviceSCSIRequestCompleted(PPDMISCSIPORT pInterface, PPDMSCSIREQUEST pSCSIRequest, int rcCompletion)
{
    PLSILOGICTASKSTATE pTaskState      = (PLSILOGICTASKSTATE)pSCSIRequest->pvUser;
    PLSILOGICDEVICE    pLsiLogicDevice = pTaskState->pTargetDevice;
    PLSILOGICSCSI      pLsiLogic       = pLsiLogicDevice->CTX_SUFF(pLsiLogic);

    ASMAtomicDecU32(&pLsiLogicDevice->cOutstandingRequests);

    if (RT_UNLIKELY(pTaskState->fBIOS))
    {
        int rc = vboxscsiRequestFinished(&pLsiLogic->VBoxSCSI, pSCSIRequest);
        AssertMsgRC(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc));
    }
    else
    {
#if 0
        lsilogicFreeGCSenseBuffer(pLsiLogic, pTaskState);
#else
        RTGCPHYS GCPhysAddrSenseBuffer;

        GCPhysAddrSenseBuffer = pTaskState->GuestRequest.SCSIIO.u32SenseBufferLowAddress;
        GCPhysAddrSenseBuffer |= ((uint64_t)pLsiLogic->u32SenseBufferHighAddr << 32);

        /* Copy the sense buffer over. */
        PDMDevHlpPhysWrite(pLsiLogic->CTX_SUFF(pDevIns), GCPhysAddrSenseBuffer, pTaskState->abSenseBuffer,
                             RT_UNLIKELY(pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength < pTaskState->PDMScsiRequest.cbSenseBuffer)
                           ? pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength
                           : pTaskState->PDMScsiRequest.cbSenseBuffer);
#endif
        lsilogicScatterGatherListDestroy(pLsiLogic, pTaskState);


        if (RT_LIKELY(rcCompletion == SCSI_STATUS_OK))
            lsilogicFinishContextReply(pLsiLogic, pTaskState->GuestRequest.SCSIIO.u32MessageContext);
        else
        {
            /* The SCSI target encountered an error during processing post a reply. */
            memset(&pTaskState->IOCReply, 0, sizeof(MptReplyUnion));
            pTaskState->IOCReply.SCSIIOError.u8TargetID = pTaskState->GuestRequest.SCSIIO.u8TargetID;
            pTaskState->IOCReply.SCSIIOError.u8Bus      = pTaskState->GuestRequest.SCSIIO.u8Bus;
            pTaskState->IOCReply.SCSIIOError.u8MessageLength = 8;
            pTaskState->IOCReply.SCSIIOError.u8Function  = pTaskState->GuestRequest.SCSIIO.u8Function;
            pTaskState->IOCReply.SCSIIOError.u8CDBLength = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
            pTaskState->IOCReply.SCSIIOError.u8SenseBufferLength = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
            pTaskState->IOCReply.SCSIIOError.u8MessageFlags = pTaskState->GuestRequest.SCSIIO.u8MessageFlags;
            pTaskState->IOCReply.SCSIIOError.u32MessageContext = pTaskState->GuestRequest.SCSIIO.u32MessageContext;
            pTaskState->IOCReply.SCSIIOError.u8SCSIStatus = rcCompletion;
            pTaskState->IOCReply.SCSIIOError.u8SCSIState  = MPT_SCSI_IO_ERROR_SCSI_STATE_AUTOSENSE_VALID;
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus  = 0;
            pTaskState->IOCReply.SCSIIOError.u32IOCLogInfo = 0;
            pTaskState->IOCReply.SCSIIOError.u32TransferCount = 0;
            pTaskState->IOCReply.SCSIIOError.u32SenseCount    = sizeof(pTaskState->abSenseBuffer);
            pTaskState->IOCReply.SCSIIOError.u32ResponseInfo  = 0;

            lsilogicFinishAddressReply(pLsiLogic, &pTaskState->IOCReply, true);
        }
    }

    RTCacheInsert(pLsiLogic->pTaskCache, pTaskState);

    return VINF_SUCCESS;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicConfigurationIOUnitPageGetFromNumber(PLSILOGICSCSI pLsiLogic, uint8_t u8PageNumber,
                                                        PMptConfigurationPageHeader *ppPageHeader,
                                                        uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertMsg(VALID_PTR(ppPageHeader) && VALID_PTR(ppbPageData), ("Invalid parameters\n"));

    switch(u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOUnitPage0.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOUnitPage0.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOUnitPage0);
            break;
        case 1:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOUnitPage1.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOUnitPage1.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOUnitPage1);
            break;
        case 2:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOUnitPage2.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOUnitPage2.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOUnitPage2);
            break;
        case 3:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOUnitPage3.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOUnitPage3.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOUnitPage3);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicConfigurationIOCPageGetFromNumber(PLSILOGICSCSI pLsiLogic, uint8_t u8PageNumber,
                                                     PMptConfigurationPageHeader *ppPageHeader,
                                                     uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertMsg(VALID_PTR(ppPageHeader) && VALID_PTR(ppbPageData), ("Invalid parameters\n"));

    switch(u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage0.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage0.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage0);
            break;
        case 1:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage1.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage1.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage1);
            break;
        case 2:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage2.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage2.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage2);
            break;
        case 3:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage3.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage3.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage3);
            break;
        case 4:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage4.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage4.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage4);
            break;
        case 6:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.IOCPage6.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.IOCPage6.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.IOCPage6);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicConfigurationManufacturingPageGetFromNumber(PLSILOGICSCSI pLsiLogic, uint8_t u8PageNumber,
                                                               PMptConfigurationPageHeader *ppPageHeader,
                                                               uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertMsg(VALID_PTR(ppPageHeader) && VALID_PTR(ppbPageData), ("Invalid parameters\n"));

    switch(u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.ManufacturingPage0.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.ManufacturingPage0.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.ManufacturingPage0);
            break;
        case 1:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.ManufacturingPage1.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.ManufacturingPage1.abVPDInfo;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.ManufacturingPage1);
            break;
        case 2:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.ManufacturingPage2.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.ManufacturingPage2.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.ManufacturingPage2);
            break;
        case 3:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.ManufacturingPage3.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.ManufacturingPage3.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.ManufacturingPage3);
            break;
        case 4:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.ManufacturingPage4.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.ManufacturingPage4.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.ManufacturingPage4);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicConfigurationSCSISPIPortPageGetFromNumber(PLSILOGICSCSI pLsiLogic, uint8_t u8Port,
                                                             uint8_t u8PageNumber,
                                                             PMptConfigurationPageHeader *ppPageHeader,
                                                             uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertMsg(VALID_PTR(ppPageHeader) && VALID_PTR(ppbPageData), ("Invalid parameters\n"));

    if (u8Port >= RT_ELEMENTS(pLsiLogic->ConfigurationPages.aPortPages))
        return VERR_NOT_FOUND;

    switch(u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage0.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage0.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage0);
            break;
        case 1:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage1.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage1.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage1);
            break;
        case 2:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage2.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage2.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aPortPages[u8Port].SCSISPIPortPage2);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicConfigurationSCSISPIDevicePageGetFromNumber(PLSILOGICSCSI pLsiLogic, uint8_t u8Bus,
                                                               uint8_t u8TargetID, uint8_t u8PageNumber,
                                                               PMptConfigurationPageHeader *ppPageHeader,
                                                               uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertMsg(VALID_PTR(ppPageHeader) && VALID_PTR(ppbPageData), ("Invalid parameters\n"));

    if (u8Bus >= RT_ELEMENTS(pLsiLogic->ConfigurationPages.aBuses))
        return VERR_NOT_FOUND;

    if (u8TargetID >= RT_ELEMENTS(pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages))
        return VERR_NOT_FOUND;

    switch(u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0);
            break;
        case 1:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1);
            break;
        case 2:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2);
            break;
        case 3:
            *ppPageHeader = &pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3.u.fields.Header;
            *ppbPageData  =  pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3.u.abPageData;
            *pcbPage      = sizeof(pLsiLogic->ConfigurationPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Processes a Configuration request.
 *
 * @returns VBox status code.
 * @param   pLsiLogic             Pointer to the device instance which sends the request.
 * @param   pConfigurationReq     Pointer to the request structure.
 * @param   pReply                Pointer to the reply message frame
 */
static int lsilogicProcessConfigurationRequest(PLSILOGICSCSI pLsiLogic, PMptConfigurationRequest pConfigurationReq,
                                               PMptConfigurationReply pReply)
{
    int rc = VINF_SUCCESS;
    uint8_t                     *pbPageData;
    PMptConfigurationPageHeader pPageHeader;
    uint8_t                     u8PageType, u8PageAttribute;
    size_t                      cbPage;

    LogFlowFunc(("pLsiLogic=%#p\n", pLsiLogic));

    u8PageType = MPT_CONFIGURATION_PAGE_TYPE_GET(pConfigurationReq->u8PageType);
    u8PageAttribute = MPT_CONFIGURATION_PAGE_ATTRIBUTE_GET(pConfigurationReq->u8PageType);

    /* Copy common bits from the request into the reply. */
    pReply->u8MessageLength   = 6; /* 6 32bit D-Words. */
    pReply->u8Action          = pConfigurationReq->u8Action;
    pReply->u8Function        = pConfigurationReq->u8Function;
    pReply->u32MessageContext = pConfigurationReq->u32MessageContext;

    switch (u8PageType)
    {
        case MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT:
        {
            /* Get the page data. */
            rc = lsilogicConfigurationIOUnitPageGetFromNumber(pLsiLogic,
                                                              pConfigurationReq->u8PageNumber,
                                                              &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_IOC:
        {
            /* Get the page data. */
            rc = lsilogicConfigurationIOCPageGetFromNumber(pLsiLogic,
                                                           pConfigurationReq->u8PageNumber,
                                                           &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING:
        {
            /* Get the page data. */
            rc = lsilogicConfigurationManufacturingPageGetFromNumber(pLsiLogic,
                                                                     pConfigurationReq->u8PageNumber,
                                                                     &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT:
        {
            /* Get the page data. */
            rc = lsilogicConfigurationSCSISPIPortPageGetFromNumber(pLsiLogic,
                                                                   pConfigurationReq->u.MPIPortNumber.u8PortNumber,
                                                                   pConfigurationReq->u8PageNumber,
                                                                   &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE:
        {
            /* Get the page data. */
            rc = lsilogicConfigurationSCSISPIDevicePageGetFromNumber(pLsiLogic,
                                                                     pConfigurationReq->u.BusAndTargetId.u8Bus,
                                                                     pConfigurationReq->u.BusAndTargetId.u8TargetID,
                                                                     pConfigurationReq->u8PageNumber,
                                                                     &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        default:
            rc = VERR_NOT_FOUND;
    }

    if (rc == VERR_NOT_FOUND)
    {
        //AssertMsgFailed(("todo\n"));
        pReply->u8PageType    = pConfigurationReq->u8PageType;
        pReply->u8PageNumber  = pConfigurationReq->u8PageNumber;
        pReply->u8PageLength  = pConfigurationReq->u8PageLength;
        pReply->u8PageVersion = pConfigurationReq->u8PageVersion;
        return VINF_SUCCESS;
    }

    pReply->u8PageType    = pPageHeader->u8PageType;
    pReply->u8PageNumber  = pPageHeader->u8PageNumber;
    pReply->u8PageLength  = pPageHeader->u8PageLength;
    pReply->u8PageVersion = pPageHeader->u8PageVersion;

    Log(("GuestRequest u8Action=%d\n", pConfigurationReq->u8Action));
    Log(("u8PageType=%d\n", pPageHeader->u8PageType));
    Log(("u8PageNumber=%d\n", pPageHeader->u8PageNumber));
    Log(("u8PageLength=%d\n", pPageHeader->u8PageLength));
    Log(("u8PageVersion=%d\n", pPageHeader->u8PageVersion));

    for (int i = 0; i < pReply->u8PageLength; i++)
        LogFlowFunc(("PageData[%d]=%#x\n", i, ((uint32_t *)pbPageData)[i]));

    /*
     * Don't use the scatter gather handling code as the configuration request always have only one
     * simple element.
     */
    switch (pConfigurationReq->u8Action)
    {
        case MPT_CONFIGURATION_REQUEST_ACTION_DEFAULT: /* Nothing to do. We are always using the defaults. */
        case MPT_CONFIGURATION_REQUEST_ACTION_HEADER:
        {
            /* Already copied above nothing to do. */
            break;
        }
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_NVRAM:
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_CURRENT:
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_DEFAULT:
        {
            uint32_t cbBuffer = pConfigurationReq->SimpleSGElement.u24Length;
            if (cbBuffer != 0)
            {
                RTGCPHYS GCPhysAddrPageBuffer = pConfigurationReq->SimpleSGElement.u32DataBufferAddressLow;
                if (pConfigurationReq->SimpleSGElement.f64BitAddress)
                    GCPhysAddrPageBuffer |= (uint64_t)pConfigurationReq->SimpleSGElement.u32DataBufferAddressHigh << 32;

                PDMDevHlpPhysWrite(pLsiLogic->CTX_SUFF(pDevIns), GCPhysAddrPageBuffer, pbPageData,
                                   cbBuffer < cbPage ? cbBuffer : cbPage);
            }
            break;
        }
        case MPT_CONFIGURATION_REQUEST_ACTION_WRITE_CURRENT:
        case MPT_CONFIGURATION_REQUEST_ACTION_WRITE_NVRAM:
        {
            uint32_t cbBuffer = pConfigurationReq->SimpleSGElement.u24Length;
            if (cbBuffer != 0)
            {
                RTGCPHYS GCPhysAddrPageBuffer = pConfigurationReq->SimpleSGElement.u32DataBufferAddressLow;
                if (pConfigurationReq->SimpleSGElement.f64BitAddress)
                    GCPhysAddrPageBuffer |= (uint64_t)pConfigurationReq->SimpleSGElement.u32DataBufferAddressHigh << 32;

                PDMDevHlpPhysRead(pLsiLogic->CTX_SUFF(pDevIns), GCPhysAddrPageBuffer, pbPageData,
                                  cbBuffer < cbPage ? cbBuffer : cbPage);
            }
            break;
        }
        default:
            AssertMsgFailed(("todo\n"));
    }

    return VINF_SUCCESS;
}

/**
 * Initializes the configuration pages.
 *
 * @returns nothing
 * @param   pLsiLogic    Pointer to the Lsilogic SCSI instance.
 */
static void lsilogicInitializeConfigurationPages(PLSILOGICSCSI pLsiLogic)
{
    PMptConfigurationPagesSupported pPages = &pLsiLogic->ConfigurationPages;

    LogFlowFunc(("pLsiLogic=%#p\n", pLsiLogic));

    /* Clear everything first. */
    memset(pPages, 0, sizeof(MptConfigurationPagesSupported));

    /* Manufacturing Page 0. */
    pPages->ManufacturingPage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY
                                                              | MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING;
    pPages->ManufacturingPage0.u.fields.Header.u8PageNumber = 0;
    pPages->ManufacturingPage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageManufacturing0) / 4;
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abChipName,          "VBox MPT Fusion", 16);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abChipRevision,      "1.0", 8);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardName,         "VBox MPT Fusion", 16);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardAssembly,     "SUN", 8);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardTracerNumber, "CAFECAFECAFECAFE", 16);

    /* Manufacturing Page 1 - I don't know what this contains so we leave it 0 for now. */
    pPages->ManufacturingPage1.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY
                                                     | MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING;
    pPages->ManufacturingPage1.Header.u8PageNumber = 1;
    pPages->ManufacturingPage1.Header.u8PageLength = sizeof(MptConfigurationPageManufacturing1) / 4;

    /* Manufacturing Page 2. */
    pPages->ManufacturingPage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                              | MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING;
    pPages->ManufacturingPage2.u.fields.Header.u8PageNumber = 2;
    pPages->ManufacturingPage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageManufacturing2) / 4;
    pPages->ManufacturingPage2.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_DEVICE_ID;
    pPages->ManufacturingPage2.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_REVISION_ID;
    /* Hardware specific settings - everything 0 for now. */

    /* Manufacturing Page 3. */
    pPages->ManufacturingPage3.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                              | MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING;
    pPages->ManufacturingPage3.u.fields.Header.u8PageNumber = 3;
    pPages->ManufacturingPage3.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageManufacturing3) / 4;
    pPages->ManufacturingPage3.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_DEVICE_ID;
    pPages->ManufacturingPage3.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_REVISION_ID;
    /* Chip specific settings - everything 0 for now. */

    /* Manufacturing Page 4 - I don't know what this contains so we leave it 0 for now. */
    pPages->ManufacturingPage4.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY
                                                              | MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING;
    pPages->ManufacturingPage4.u.fields.Header.u8PageNumber = 4;
    pPages->ManufacturingPage4.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageManufacturing4) / 4;


    /* I/O Unit page 0. */
    pPages->IOUnitPage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                       | MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT;
    pPages->IOUnitPage0.u.fields.Header.u8PageNumber = 0;
    pPages->IOUnitPage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOUnit0) / 4;
    pPages->IOUnitPage0.u.fields.u64UniqueIdentifier = 0xcafe;

    /* I/O Unit page 1. */
    pPages->IOUnitPage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                       | MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT;
    pPages->IOUnitPage1.u.fields.Header.u8PageNumber = 1;
    pPages->IOUnitPage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOUnit1) / 4;
    pPages->IOUnitPage1.u.fields.fSingleFunction         = true;
    pPages->IOUnitPage1.u.fields.fAllPathsMapped         = false;
    pPages->IOUnitPage1.u.fields.fIntegratedRAIDDisabled = true;
    pPages->IOUnitPage1.u.fields.f32BitAccessForced      = false;

    /* I/O Unit page 2. */
    pPages->IOUnitPage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT
                                                       | MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT;
    pPages->IOUnitPage2.u.fields.Header.u8PageNumber = 2;
    pPages->IOUnitPage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOUnit2) / 4;
    pPages->IOUnitPage2.u.fields.fPauseOnError       = false;
    pPages->IOUnitPage2.u.fields.fVerboseModeEnabled = false;
    pPages->IOUnitPage2.u.fields.fDisableColorVideo  = false;
    pPages->IOUnitPage2.u.fields.fNotHookInt40h      = false;
    pPages->IOUnitPage2.u.fields.u32BIOSVersion      = 0xcafecafe;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].fAdapterEnabled = true;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].fAdapterEmbedded = true;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].u8PCIBusNumber = 0;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].u8PCIDevFn     = pLsiLogic->PciDev.devfn;

    /* I/O Unit page 3. */
    pPages->IOUnitPage3.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                       | MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT;
    pPages->IOUnitPage3.u.fields.Header.u8PageNumber = 3;
    pPages->IOUnitPage3.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOUnit3) / 4;
    pPages->IOUnitPage3.u.fields.u8GPIOCount = 0;

    /* IOC page 0. */
    pPages->IOCPage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage0.u.fields.Header.u8PageNumber = 0;
    pPages->IOCPage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC0) / 4;
    pPages->IOCPage0.u.fields.u32TotalNVStore      = 0;
    pPages->IOCPage0.u.fields.u32FreeNVStore       = 0;
    pPages->IOCPage0.u.fields.u16VendorId          = LSILOGICSCSI_PCI_VENDOR_ID;
    pPages->IOCPage0.u.fields.u16DeviceId          = LSILOGICSCSI_PCI_DEVICE_ID;
    pPages->IOCPage0.u.fields.u8RevisionId         = LSILOGICSCSI_PCI_REVISION_ID;
    pPages->IOCPage0.u.fields.u32ClassCode         = LSILOGICSCSI_PCI_CLASS_CODE;
    pPages->IOCPage0.u.fields.u16SubsystemVendorId = LSILOGICSCSI_PCI_SUBSYSTEM_VENDOR_ID;
    pPages->IOCPage0.u.fields.u16SubsystemId       = LSILOGICSCSI_PCI_SUBSYSTEM_ID;

    /* IOC page 1. */
    pPages->IOCPage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage1.u.fields.Header.u8PageNumber = 1;
    pPages->IOCPage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC1) / 4;
    pPages->IOCPage1.u.fields.fReplyCoalescingEnabled = false;
    pPages->IOCPage1.u.fields.u32CoalescingTimeout    = 0;
    pPages->IOCPage1.u.fields.u8CoalescingDepth       = 0;

    /* IOC page 2. */
    pPages->IOCPage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage2.u.fields.Header.u8PageNumber = 2;
    pPages->IOCPage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC2) / 4;
    /* Everything else here is 0. */

    /* IOC page 3. */
    pPages->IOCPage3.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage3.u.fields.Header.u8PageNumber = 3;
    pPages->IOCPage3.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC3) / 4;
    /* Everything else here is 0. */

    /* IOC page 4. */
    pPages->IOCPage4.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage4.u.fields.Header.u8PageNumber = 4;
    pPages->IOCPage4.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC4) / 4;
    /* Everything else here is 0. */

    /* IOC page 6. */
    pPages->IOCPage6.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                    | MPT_CONFIGURATION_PAGE_TYPE_IOC;
    pPages->IOCPage6.u.fields.Header.u8PageNumber = 6;
    pPages->IOCPage6.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageIOC6) / 4;
    /* Everything else here is 0. */

    for (unsigned i = 0; i < RT_ELEMENTS(pPages->aPortPages); i++)
    {
        /* SCSI-SPI port page 0. */
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageNumber = 0;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort0) / 4;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fInformationUnitTransfersCapable = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fDTCapable                       = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fQASCapable                      = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u8MinimumSynchronousTransferPeriod =  0;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u8MaximumSynchronousOffset         = 0xff;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fWide                              = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fAIPCapable                        = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u2SignalingType                    = 0x3; /* Single Ended. */

        /* SCSI-SPI port page 1. */
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageNumber = 1;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort1) / 4;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u8SCSIID                  = 7;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u16PortResponseIDsBitmask = (1 << 7);
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u32OnBusTimerValue        =  0;

        /* SCSI-SPI port page 2. */
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageNumber = 2;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort2) / 4;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.u4HostSCSIID           = 7;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.u2InitializeHBA        = 0x3;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.fTerminationDisabled   = true;
        for (unsigned iDevice = 0; iDevice < RT_ELEMENTS(pPages->aPortPages[i].SCSISPIPortPage2.u.fields.aDeviceSettings); iDevice++)
        {
            pPages->aPortPages[i].SCSISPIPortPage2.u.fields.aDeviceSettings[iDevice].fBootChoice   = true;
        }
        /* Everything else 0 for now. */
    }

    for (unsigned uBusCurr = 0; uBusCurr < RT_ELEMENTS(pPages->aBuses); uBusCurr++)
    {
        for (unsigned uDeviceCurr = 0; uDeviceCurr < RT_ELEMENTS(pPages->aBuses[uBusCurr].aDevicePages); uDeviceCurr++)
        {
            /* SCSI-SPI device page 0. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageNumber = 0;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice0) / 4;
            /* Everything else 0 for now. */

            /* SCSI-SPI device page 1. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageNumber = 1;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice1) / 4;
            /* Everything else 0 for now. */

            /* SCSI-SPI device page 2. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageNumber = 2;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice2) / 4;
            /* Everything else 0 for now. */

            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageNumber = 3;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice3) / 4;
            /* Everything else 0 for now. */
        }
    }
}

/**
 * Transmit queue consumer
 * Queue a new async task.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDevIns     The device instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
static DECLCALLBACK(bool) lsilogicNotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pDevIns=%#p pItem=%#p\n", pDevIns, pItem));

    /* Only process request which arrived before we received the notification. */
    uint32_t uRequestQueueNextEntryWrite = ASMAtomicReadU32(&pLsiLogic->uRequestQueueNextEntryFreeWrite);

    /* Reset notification event. */
    ASMAtomicXchgBool(&pLsiLogic->fNotificationSend, false);

    /* Go through the messages now and process them. */
    while (   RT_LIKELY(pLsiLogic->enmState == LSILOGICSTATE_OPERATIONAL)
           && (pLsiLogic->uRequestQueueNextAddressRead != uRequestQueueNextEntryWrite))
    {
        uint32_t  u32RequestMessageFrameDesc = pLsiLogic->CTX_SUFF(pRequestQueueBase)[pLsiLogic->uRequestQueueNextAddressRead];
        RTGCPHYS  GCPhysMessageFrameAddr = LSILOGIC_RTGCPHYS_FROM_U32(pLsiLogic->u32HostMFAHighAddr,
                                                                      (u32RequestMessageFrameDesc & ~0x07));

        PLSILOGICTASKSTATE pTaskState;

        /* Get new task state. */
        rc = RTCacheRequest(pLsiLogic->pTaskCache, (void **)&pTaskState);
        AssertRC(rc);

        pTaskState->GCPhysMessageFrameAddr = GCPhysMessageFrameAddr;

        /* Read the message header from the guest first. */
        PDMDevHlpPhysRead(pDevIns, GCPhysMessageFrameAddr, &pTaskState->GuestRequest, sizeof(MptMessageHdr));

        /* Determine the size of the request. */
        uint32_t cbRequest = 0;

        switch (pTaskState->GuestRequest.Header.u8Function)
        {
            case MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST:
                cbRequest = sizeof(MptSCSIIORequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_SCSI_TASK_MGMT:
                cbRequest = sizeof(MptSCSITaskManagementRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_IOC_INIT:
                cbRequest = sizeof(MptIOCInitRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_IOC_FACTS:
                cbRequest = sizeof(MptIOCFactsRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_CONFIG:
                cbRequest = sizeof(MptConfigurationRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_PORT_FACTS:
                cbRequest = sizeof(MptPortFactsRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_PORT_ENABLE:
                cbRequest = sizeof(MptPortEnableRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_EVENT_NOTIFICATION:
                cbRequest = sizeof(MptEventNotificationRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_EVENT_ACK:
                AssertMsgFailed(("todo\n"));
                //cbRequest = sizeof(MptEventAckRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_FW_DOWNLOAD:
                AssertMsgFailed(("todo\n"));
                break;
            default:
                AssertMsgFailed(("Unknown function issued %u\n", pTaskState->GuestRequest.Header.u8Function));
                lsilogicSetIOCFaultCode(pLsiLogic, LSILOGIC_IOCSTATUS_INVALID_FUNCTION);
        }

        if (cbRequest != 0)
        {
            /* Read the complete message frame from guest memory now. */
            PDMDevHlpPhysRead(pDevIns, GCPhysMessageFrameAddr, &pTaskState->GuestRequest, cbRequest);

            /* Handle SCSI I/O requests now. */
            if (pTaskState->GuestRequest.Header.u8Function == MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST)
            {
               rc = lsilogicProcessSCSIIORequest(pLsiLogic, pTaskState);
               AssertRC(rc);
            }
            else
            {
                MptReplyUnion Reply;
                rc = lsilogicProcessMessageRequest(pLsiLogic, &pTaskState->GuestRequest.Header, &Reply);
                AssertRC(rc);
                RTCacheInsert(pLsiLogic->pTaskCache, pTaskState);
            }

            pLsiLogic->uRequestQueueNextAddressRead++;
            pLsiLogic->uRequestQueueNextAddressRead %= pLsiLogic->cRequestQueueEntries;
        }
    }

    return true;
}

/**
 * Port I/O Handler for IN operations - legacy port.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static int  lsilogicIsaIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                   RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    int rc;
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    Assert(cb == 1);

    rc = vboxscsiReadRegister(&pLsiLogic->VBoxSCSI, (Port - LSILOGIC_ISA_IO_PORT), pu32);

    Log2(("%s: pu32=%p:{%.*Rhxs} iRegister=%d rc=%Rrc\n",
          __FUNCTION__, pu32, 1, pu32, (Port - LSILOGIC_ISA_IO_PORT), rc));

    return rc;
}

/**
 * Prepares a request from the BIOS.
 *
 * @returns VBox status code.
 * @param   pLsiLogic    Pointer to the LsiLogic device instance.
 */
static int lsilogicPrepareBIOSSCSIRequest(PLSILOGICSCSI pLsiLogic)
{
    int rc;
    PLSILOGICTASKSTATE pTaskState;
    uint32_t           uTargetDevice;

    rc = RTCacheRequest(pLsiLogic->pTaskCache, (void **)&pTaskState);
    AssertMsgRCReturn(rc, ("Getting task from cache failed rc=%Rrc\n", rc), rc);

    pTaskState->fBIOS = true;

    rc = vboxscsiSetupRequest(&pLsiLogic->VBoxSCSI, &pTaskState->PDMScsiRequest, &uTargetDevice);
    AssertMsgRCReturn(rc, ("Setting up SCSI request failed rc=%Rrc\n", rc), rc);

    pTaskState->PDMScsiRequest.pvUser = pTaskState;

    pTaskState->pTargetDevice = &pLsiLogic->aDeviceStates[uTargetDevice];

    if (!pTaskState->pTargetDevice->pDrvBase)
    {
        /* Device is not present. */
        AssertMsg(pTaskState->PDMScsiRequest.pbCDB[0] == SCSI_INQUIRY,
                    ("Device is not present but command is not inquiry\n"));

        SCSIINQUIRYDATA ScsiInquiryData;

        memset(&ScsiInquiryData, 0, sizeof(SCSIINQUIRYDATA));
        ScsiInquiryData.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
        ScsiInquiryData.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_NOT_CONNECTED_NOT_SUPPORTED;

        memcpy(pLsiLogic->VBoxSCSI.pBuf, &ScsiInquiryData, 5);

        rc = vboxscsiRequestFinished(&pLsiLogic->VBoxSCSI, &pTaskState->PDMScsiRequest);
        AssertMsgRCReturn(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc), rc);

        rc = RTCacheInsert(pLsiLogic->pTaskCache, pTaskState);
        AssertMsgRCReturn(rc, ("Getting task from cache failed rc=%Rrc\n", rc), rc);
    }
    else
    {
        ASMAtomicIncU32(&pTaskState->pTargetDevice->cOutstandingRequests);

        rc = pTaskState->pTargetDevice->pDrvSCSIConnector->pfnSCSIRequestSend(pTaskState->pTargetDevice->pDrvSCSIConnector,
                                                                                &pTaskState->PDMScsiRequest);
        AssertMsgRCReturn(rc, ("Sending request to SCSI layer failed rc=%Rrc\n", rc), rc);
    }

    return rc;
}

/**
 * Port I/O Handler for OUT operations - legacy port.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static int lsilogicIsaIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                   RTIOPORT Port, uint32_t u32, unsigned cb)
{
    int rc;
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    Log2(("#%d %s: pvUser=%#p cb=%d u32=%#x Port=%#x\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, u32, Port));

    Assert(cb == 1);

    rc = vboxscsiWriteRegister(&pLsiLogic->VBoxSCSI, (Port - LSILOGIC_ISA_IO_PORT), (uint8_t)u32);
    if (rc == VERR_MORE_DATA)
    {
        rc = lsilogicPrepareBIOSSCSIRequest(pLsiLogic);
        AssertRC(rc);
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Writing BIOS register failed %Rrc\n", rc));

    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for primary port range OUT string operations.
 * @see FNIOMIOPORTOUTSTRING for details.
 */
static DECLCALLBACK(int) lsilogicIsaIOPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc;

    Log2(("#%d %s: pvUser=%#p cb=%d Port=%#x\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    rc = vboxscsiWriteString(pDevIns, &pLsiLogic->VBoxSCSI, (Port - LSILOGIC_ISA_IO_PORT),
                             pGCPtrSrc, pcTransfer, cb);
    if (rc == VERR_MORE_DATA)
    {
        rc = lsilogicPrepareBIOSSCSIRequest(pLsiLogic);
        AssertRC(rc);
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Writing BIOS register failed %Rrc\n", rc));

    return rc;
}

/**
 * Port I/O Handler for primary port range IN string operations.
 * @see FNIOMIOPORTINSTRING for details.
 */
static DECLCALLBACK(int) lsilogicIsaIOPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("#%d %s: pvUser=%#p cb=%d Port=%#x\n",
                 pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    return vboxscsiReadString(pDevIns, &pLsiLogic->VBoxSCSI, (Port - LSILOGIC_ISA_IO_PORT),
                              pGCPtrDst, pcTransfer, cb);
}

static DECLCALLBACK(int) lsilogicMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion,
                                     RTGCPHYS GCPhysAddress, uint32_t cb,
                                     PCIADDRESSSPACE enmType)
{
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    AssertMsg(   (enmType == PCI_ADDRESS_SPACE_MEM && cb >= LSILOGIC_PCI_SPACE_MEM_SIZE)
              || (enmType == PCI_ADDRESS_SPACE_IO  && cb >= LSILOGIC_PCI_SPACE_IO_SIZE),
              ("PCI region type and size do not match\n"));

    if ((enmType == PCI_ADDRESS_SPACE_MEM) && (iRegion == 1))
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL,
                                   lsilogicMMIOWrite, lsilogicMMIORead, NULL, "LsiLogic");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, 0,
                                         "lsilogicMMIOWrite", "lsilogicMMIORead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterGC(pDevIns, GCPhysAddress, cb, 0,
                                         "lsilogicMMIOWrite", "lsilogicMMIORead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->GCPhysMMIOBase = GCPhysAddress;
    }
    else if ((enmType == PCI_ADDRESS_SPACE_MEM) && (iRegion == 2))
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL,
                                   lsilogicDiagnosticWrite, lsilogicDiagnosticRead, NULL, "LsiLogicDiag");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, 0,
                                         "lsilogicDiagnosticWrite", "lsilogicDiagnosticRead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterGC(pDevIns, GCPhysAddress, cb, 0,
                                         "lsilogicDiagnosticWrite", "lsilogicDiagnosticRead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    else if (enmType == PCI_ADDRESS_SPACE_IO)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                     NULL, lsilogicIOPortWrite, lsilogicIOPortRead, NULL, NULL, "LsiLogic");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                           0, "lsilogicIOPortWrite", "lsilogicIOPortRead", NULL, NULL, "LsiLogic");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                           0, "lsilogicIOPortWrite", "lsilogicIOPortRead", NULL, NULL, "LsiLogic");
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->IOPortBase = (RTIOPORT)GCPhysAddress;
    }
    else
        AssertMsgFailed(("Invalid enmType=%d iRegion=%d\n", enmType, iRegion));

    return rc;
}

/**
 * Waits until all I/O operations on all devices are complete.
 *
 * @retruns Flag which indicates if all I/O completed in the given timeout.
 * @param   pLsiLogic    Pointer to the dveice instance to check.
 * @param   cMillis      Timeout in milliseconds to wait.
 */
static bool lsilogicWaitForAsyncIOFinished(PLSILOGICSCSI pLsiLogic, unsigned cMillies)
{
    uint64_t u64Start;
    bool     fIdle;

    /*
     * Wait for any pending async operation to finish
     */
    u64Start = RTTimeMilliTS();
    do
    {
        fIdle = true;

        /* Check every port. */
        for (unsigned i = 0; i < RT_ELEMENTS(pLsiLogic->aDeviceStates); i++)
        {
            PLSILOGICDEVICE pLsiLogicDevice = &pLsiLogic->aDeviceStates[i];
            if (ASMAtomicReadU32(&pLsiLogicDevice->cOutstandingRequests))
            {
                fIdle = false;
                break;
            }
        }
        if (RTTimeMilliTS() - u64Start >= cMillies)
            break;

        /* Sleep for a bit. */
        RTThreadSleep(100);
    } while (!fIdle);

    return fIdle;
}

static DECLCALLBACK(int) lsilogicSaveLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    /* Wait that no task is pending on any device. */
    if (!lsilogicWaitForAsyncIOFinished(pLsiLogic, 20000))
    {
        AssertLogRelMsgFailed(("LsiLogic: There are still tasks outstanding\n"));
        return VERR_TIMEOUT;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) lsilogicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    /* Every device first. */
    for (unsigned i = 0; i < RT_ELEMENTS(pLsiLogic->aDeviceStates); i++)
    {
        PLSILOGICDEVICE pDevice = &pLsiLogic->aDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        SSMR3PutU32(pSSM, pDevice->cOutstandingRequests);
    }
    /* Now the main device state. */
    SSMR3PutU32   (pSSM, pLsiLogic->enmState);
    SSMR3PutU32   (pSSM, pLsiLogic->enmWhoInit);
    SSMR3PutBool  (pSSM, pLsiLogic->fDoorbellInProgress);
    SSMR3PutBool  (pSSM, pLsiLogic->fDiagnosticEnabled);
    SSMR3PutBool  (pSSM, pLsiLogic->fNotificationSend);
    SSMR3PutBool  (pSSM, pLsiLogic->fEventNotificationEnabled);
    SSMR3PutU32   (pSSM, pLsiLogic->uInterruptMask);
    SSMR3PutU32   (pSSM, pLsiLogic->uInterruptStatus);
    for (unsigned i = 0; i < RT_ELEMENTS(pLsiLogic->aMessage); i++)
        SSMR3PutU32   (pSSM, pLsiLogic->aMessage[i]);
    SSMR3PutU32   (pSSM, pLsiLogic->iMessage);
    SSMR3PutU32   (pSSM, pLsiLogic->cMessage);
    SSMR3PutMem   (pSSM, &pLsiLogic->ReplyBuffer, sizeof(pLsiLogic->ReplyBuffer));
    SSMR3PutU32   (pSSM, pLsiLogic->uNextReplyEntryRead);
    SSMR3PutU32   (pSSM, pLsiLogic->cReplySize);
    SSMR3PutU16   (pSSM, pLsiLogic->u16IOCFaultCode);
    SSMR3PutU32   (pSSM, pLsiLogic->u32HostMFAHighAddr);
    SSMR3PutU32   (pSSM, pLsiLogic->u32SenseBufferHighAddr);
    SSMR3PutU8    (pSSM, pLsiLogic->cMaxDevices);
    SSMR3PutU8    (pSSM, pLsiLogic->cMaxBuses);
    SSMR3PutU16   (pSSM, pLsiLogic->cbReplyFrame);
    SSMR3PutU32   (pSSM, pLsiLogic->iDiagnosticAccess);
    SSMR3PutU32   (pSSM, pLsiLogic->cReplyQueueEntries);
    SSMR3PutU32   (pSSM, pLsiLogic->cRequestQueueEntries);
    SSMR3PutU32   (pSSM, pLsiLogic->uReplyFreeQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pLsiLogic->uReplyFreeQueueNextAddressRead);
    SSMR3PutU32   (pSSM, pLsiLogic->uReplyPostQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pLsiLogic->uReplyPostQueueNextAddressRead);
    SSMR3PutU32   (pSSM, pLsiLogic->uRequestQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pLsiLogic->uRequestQueueNextAddressRead);
    SSMR3PutMem   (pSSM, &pLsiLogic->ConfigurationPages, sizeof(pLsiLogic->ConfigurationPages));
    /* Now the data for the BIOS interface. */
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.regIdentify);
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.uTargetDevice);
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.uTxDir);
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.cbCDB);
    SSMR3PutMem   (pSSM, pLsiLogic->VBoxSCSI.aCDB, sizeof(pLsiLogic->VBoxSCSI.aCDB));
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.iCDB);
    SSMR3PutU32   (pSSM, pLsiLogic->VBoxSCSI.cbBuf);
    SSMR3PutU32   (pSSM, pLsiLogic->VBoxSCSI.iBuf);
    SSMR3PutBool  (pSSM, pLsiLogic->VBoxSCSI.fBusy);
    SSMR3PutU8    (pSSM, pLsiLogic->VBoxSCSI.enmState);
    if (pLsiLogic->VBoxSCSI.cbCDB)
        SSMR3PutMem(pSSM, pLsiLogic->VBoxSCSI.pBuf, pLsiLogic->VBoxSCSI.cbBuf);

    return SSMR3PutU32(pSSM, ~0);
}

static DECLCALLBACK(int) lsilogicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    /* We support saved states only from this and older versions. */
    if (uVersion > LSILOGIC_SAVED_STATE_MINOR_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /* Every device first. */
    for (unsigned i = 0; i < RT_ELEMENTS(pLsiLogic->aDeviceStates); i++)
    {
        PLSILOGICDEVICE pDevice = &pLsiLogic->aDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        SSMR3GetU32(pSSM, (uint32_t *)&pDevice->cOutstandingRequests);
    }
    /* Now the main device state. */
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->enmState);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->enmWhoInit);
    SSMR3GetBool  (pSSM, &pLsiLogic->fDoorbellInProgress);
    SSMR3GetBool  (pSSM, &pLsiLogic->fDiagnosticEnabled);
    SSMR3GetBool  (pSSM, &pLsiLogic->fNotificationSend);
    SSMR3GetBool  (pSSM, &pLsiLogic->fEventNotificationEnabled);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uInterruptMask);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uInterruptStatus);
    for (unsigned i = 0; i < RT_ELEMENTS(pLsiLogic->aMessage); i++)
        SSMR3GetU32   (pSSM, &pLsiLogic->aMessage[i]);
    SSMR3GetU32   (pSSM, &pLsiLogic->iMessage);
    SSMR3GetU32   (pSSM, &pLsiLogic->cMessage);
    SSMR3GetMem   (pSSM, &pLsiLogic->ReplyBuffer, sizeof(pLsiLogic->ReplyBuffer));
    SSMR3GetU32   (pSSM, &pLsiLogic->uNextReplyEntryRead);
    SSMR3GetU32   (pSSM, &pLsiLogic->cReplySize);
    SSMR3GetU16   (pSSM, &pLsiLogic->u16IOCFaultCode);
    SSMR3GetU32   (pSSM, &pLsiLogic->u32HostMFAHighAddr);
    SSMR3GetU32   (pSSM, &pLsiLogic->u32SenseBufferHighAddr);
    SSMR3GetU8    (pSSM, &pLsiLogic->cMaxDevices);
    SSMR3GetU8    (pSSM, &pLsiLogic->cMaxBuses);
    SSMR3GetU16   (pSSM, &pLsiLogic->cbReplyFrame);
    SSMR3GetU32   (pSSM, &pLsiLogic->iDiagnosticAccess);
    SSMR3GetU32   (pSSM, &pLsiLogic->cReplyQueueEntries);
    SSMR3GetU32   (pSSM, &pLsiLogic->cRequestQueueEntries);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uReplyFreeQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uReplyFreeQueueNextAddressRead);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uReplyPostQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uReplyPostQueueNextAddressRead);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uRequestQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pLsiLogic->uRequestQueueNextAddressRead);
    SSMR3GetMem   (pSSM, &pLsiLogic->ConfigurationPages, sizeof(pLsiLogic->ConfigurationPages));
    /* Now the data for the BIOS interface. */
    SSMR3GetU8  (pSSM, &pLsiLogic->VBoxSCSI.regIdentify);
    SSMR3GetU8  (pSSM, &pLsiLogic->VBoxSCSI.uTargetDevice);
    SSMR3GetU8  (pSSM, &pLsiLogic->VBoxSCSI.uTxDir);
    SSMR3GetU8  (pSSM, &pLsiLogic->VBoxSCSI.cbCDB);
    SSMR3GetMem (pSSM, pLsiLogic->VBoxSCSI.aCDB, sizeof(pLsiLogic->VBoxSCSI.aCDB));
    SSMR3GetU8  (pSSM, &pLsiLogic->VBoxSCSI.iCDB);
    SSMR3GetU32 (pSSM, &pLsiLogic->VBoxSCSI.cbBuf);
    SSMR3GetU32 (pSSM, &pLsiLogic->VBoxSCSI.iBuf);
    SSMR3GetBool(pSSM, (bool *)&pLsiLogic->VBoxSCSI.fBusy);
    SSMR3GetU8  (pSSM, (uint8_t *)&pLsiLogic->VBoxSCSI.enmState);
    if (pLsiLogic->VBoxSCSI.cbCDB)
    {
        pLsiLogic->VBoxSCSI.pBuf = (uint8_t *)RTMemAllocZ(pLsiLogic->VBoxSCSI.cbCDB);
        if (!pLsiLogic->VBoxSCSI.pBuf)
        {
            LogRel(("LsiLogic: Out of memory during restore.\n"));
            return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY,
                                    N_("LsiLogic: Out of memory during restore\n"));
        }
        SSMR3GetMem(pSSM, pLsiLogic->VBoxSCSI.pBuf, pLsiLogic->VBoxSCSI.cbBuf);
    }

    uint32_t u32;
    int rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    AssertMsgReturn(u32 == ~0U, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

    return VINF_SUCCESS;
}

/**
 * Gets the pointer to the status LED of a device - called from the SCSi driver.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire. Always 0 here as the driver
 *                          doesn't know about other LUN's.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) lsilogicDeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PLSILOGICDEVICE pDevice = PDMILEDPORTS_2_PLSILOGICDEVICE(pInterface);
    if (iLUN == 0)
    {
        *ppLed = &pDevice->Led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to LSILOGICDEVICE::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) lsilogicDeviceQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PLSILOGICDEVICE pDevice = PDMIBASE_2_PLSILOGICDEVICE(pInterface);

    switch (enmInterface)
    {
        case PDMINTERFACE_SCSI_PORT:
            return &pDevice->ISCSIPort;
        case PDMINTERFACE_LED_PORTS:
            return &pDevice->ILed;
        default:
            return NULL;
    }
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) lsilogicStatusQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PLSILOGICSCSI pLsiLogic = PDMILEDPORTS_2_PLSILOGICSCSI(pInterface);
    if (iLUN < LSILOGIC_DEVICES_MAX)
    {
        *ppLed = &pLsiLogic->aDeviceStates[iLUN].Led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to ATADevState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) lsilogicStatusQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PLSILOGICSCSI pLsiLogic = PDMIBASE_2_PLSILOGICSCSI(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pLsiLogic->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pLsiLogic->ILeds;
        default:
            return NULL;
    }
}

/**
 * Detach notification.
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) lsilogicDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PLSILOGICSCSI   pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    PLSILOGICDEVICE pDevice = &pThis->aDeviceStates[iLUN];

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("LsiLogic: Device does not support hotplugging\n"));

    Log(("%s:\n", __FUNCTION__));

    /*
     * Zero some important members.
     */
    pDevice->pDrvBase = NULL;
    pDevice->pDrvSCSIConnector = NULL;
}

/**
 * Attach command.
 *
 * This is called when we change block driver.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  lsilogicAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PLSILOGICSCSI   pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    PLSILOGICDEVICE pDevice = &pThis->aDeviceStates[iLUN];
    int rc;

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("LsiLogic: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pDevice->pDrvBase);
    AssertRelease(!pDevice->pDrvSCSIConnector);
    Assert(pDevice->iLUN == iLUN);

    /*
     * Try attach the block device and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
    {
        /* Get SCSI connector interface. */
        pDevice->pDrvSCSIConnector = (PPDMISCSICONNECTOR)pDevice->pDrvBase->pfnQueryInterface(pDevice->pDrvBase, PDMINTERFACE_SCSI_CONNECTOR);
        AssertMsgReturn(pDevice->pDrvSCSIConnector, ("Missing SCSI interface below\n"), VERR_PDM_MISSING_INTERFACE);
    }
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pDevice->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pDevice->pDrvBase = NULL;
        pDevice->pDrvSCSIConnector = NULL;
    }
    return rc;
}

/**
 * @copydoc FNPDMDEVPOWEROFF
 */
static DECLCALLBACK(void) lsilogicPowerOff(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    bool fIdle = lsilogicWaitForAsyncIOFinished(pLsiLogic, 20000);
    Assert(fIdle);
}

/**
 * @copydoc FNPDMDEVSUSPEND
 */
static DECLCALLBACK(void) lsilogicSuspend(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    bool fIdle = lsilogicWaitForAsyncIOFinished(pLsiLogic, 20000);
    Assert(fIdle);
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) lsilogicReset(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pLsiLogic = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc;

    bool fIdle = lsilogicWaitForAsyncIOFinished(pLsiLogic, 20000);
    Assert(fIdle);

    rc = lsilogicHardReset(pLsiLogic);
    AssertRC(rc);

    vboxscsiInitialize(&pLsiLogic->VBoxSCSI);
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) lsilogicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    pThis->pDevInsRC        = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pNotificationQueueRC = PDMQueueRCPtr(pThis->pNotificationQueueR3);

    /* Relocate queues. */
    pThis->pReplyFreeQueueBaseRC += offDelta;
    pThis->pReplyPostQueueBaseRC += offDelta;
    pThis->pRequestQueueBaseRC   += offDelta;
}

/**
 * @copydoc FNPDMDEVDESTRUCT
 */
static DECLCALLBACK(int) lsilogicDestruct(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc = VINF_SUCCESS;

    PDMR3CritSectDelete(&pThis->ReplyFreeQueueCritSect);
    PDMR3CritSectDelete(&pThis->ReplyPostQueueCritSect);

    /* Destroy task cache. */
    if (pThis->pTaskCache)
        rc = RTCacheDestroy(pThis->pTaskCache);

    return rc;
}

/**
 * @copydoc FNPDMDEVCONSTRUCT
 */
static DECLCALLBACK(int) lsilogicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc = VINF_SUCCESS;
    PVM pVM = PDMDevHlpGetVM(pDevIns);

    /*
     * Validate and read configuration.
     */
    rc = CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0"
                                          "R0Enabled\0"
                                          "ReplyQueueDepth\0"
                                          "RequestQueueDepth\0");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("LsiLogic configuration error: unknown option specified"));
    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &pThis->fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, pThis->fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, pThis->fR0Enabled));

    rc = CFGMR3QueryU32Def(pCfgHandle, "ReplyQueueDepth",
                           &pThis->cReplyQueueEntries,
                           LSILOGICSCSI_REPLY_QUEUE_DEPTH_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read ReplyQueue as integer"));
    Log(("%s: ReplyQueueDepth=%u\n", __FUNCTION__, pThis->cReplyQueueEntries));

    rc = CFGMR3QueryU32Def(pCfgHandle, "RequestQueueDepth",
                           &pThis->cRequestQueueEntries,
                           LSILOGICSCSI_REQUEST_QUEUE_DEPTH_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read RequestQueue as integer"));
    Log(("%s: RequestQueueDepth=%u\n", __FUNCTION__, pThis->cRequestQueueEntries));


    /* Init static parts. */
    PCIDevSetVendorId         (&pThis->PciDev, LSILOGICSCSI_PCI_VENDOR_ID); /* LsiLogic */
    PCIDevSetDeviceId         (&pThis->PciDev, LSILOGICSCSI_PCI_DEVICE_ID); /* LSI53C1030 */
    PCIDevSetClassProg        (&pThis->PciDev,   0x00); /* SCSI */
    PCIDevSetClassSub         (&pThis->PciDev,   0x00); /* SCSI */
    PCIDevSetClassBase        (&pThis->PciDev,   0x01); /* Mass storage */
    PCIDevSetSubSystemVendorId(&pThis->PciDev, LSILOGICSCSI_PCI_SUBSYSTEM_VENDOR_ID);
    PCIDevSetSubSystemId      (&pThis->PciDev, LSILOGICSCSI_PCI_SUBSYSTEM_ID);
    PCIDevSetInterruptPin     (&pThis->PciDev,   0x01); /* Interrupt pin A */

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->IBase.pfnQueryInterface = lsilogicStatusQueryInterface;
    pThis->ILeds.pfnQueryStatusLed = lsilogicStatusQueryStatusLed;

    /*
     * Register the PCI device, it's I/O regions.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, LSILOGIC_PCI_SPACE_IO_SIZE, PCI_ADDRESS_SPACE_IO, lsilogicMap);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, LSILOGIC_PCI_SPACE_MEM_SIZE, PCI_ADDRESS_SPACE_MEM, lsilogicMap);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, LSILOGIC_PCI_SPACE_MEM_SIZE, PCI_ADDRESS_SPACE_MEM, lsilogicMap);
    if (RT_FAILURE(rc))
        return rc;

    /* Intialize task queue. */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 2, 0,
                                 lsilogicNotifyQueueConsumer, true, "LsiLogic-Task", &pThis->pNotificationQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotificationQueueR0 = PDMQueueR0Ptr(pThis->pNotificationQueueR3);
    pThis->pNotificationQueueRC = PDMQueueRCPtr(pThis->pNotificationQueueR3);

    /*
     * We need one entry free in the queue.
     */
    pThis->cReplyQueueEntries++;
    pThis->cRequestQueueEntries++;

    /*
     * Allocate memory for the queues.
     */
    uint32_t cbQueues;

    cbQueues  = 2*pThis->cReplyQueueEntries * sizeof(uint32_t);
    cbQueues += pThis->cRequestQueueEntries * sizeof(uint32_t);
    rc = MMHyperAlloc(pVM, cbQueues, 1, MM_TAG_PDM_DEVICE_USER,
                      (void **)&pThis->pReplyFreeQueueBaseR3);
    if (RT_FAILURE(rc))
        return VERR_NO_MEMORY;
    pThis->pReplyFreeQueueBaseR0 = MMHyperR3ToR0(pVM, (void *)pThis->pReplyFreeQueueBaseR3);
    pThis->pReplyFreeQueueBaseRC = MMHyperR3ToRC(pVM, (void *)pThis->pReplyFreeQueueBaseR3);

    pThis->pReplyPostQueueBaseR3 = pThis->pReplyFreeQueueBaseR3 + pThis->cReplyQueueEntries;
    pThis->pReplyPostQueueBaseR0 = MMHyperR3ToR0(pVM, (void *)pThis->pReplyPostQueueBaseR3);
    pThis->pReplyPostQueueBaseRC = MMHyperR3ToRC(pVM, (void *)pThis->pReplyPostQueueBaseR3);

    pThis->pRequestQueueBaseR3   = pThis->pReplyPostQueueBaseR3 + pThis->cReplyQueueEntries;
    pThis->pRequestQueueBaseR0   = MMHyperR3ToR0(pVM, (void *)pThis->pRequestQueueBaseR3);
    pThis->pRequestQueueBaseRC   = MMHyperR3ToRC(pVM, (void *)pThis->pRequestQueueBaseR3);

    /*
     * Create critical sections protecting the reply post and free queues.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->ReplyFreeQueueCritSect, "LsiLogicRFQ");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic: cannot create critical section for reply free queue"));

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->ReplyPostQueueCritSect, "LsiLogicRPQ");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic: cannot create critical section for reply post queue"));

    /*
     * Allocate task cache.
     */
    rc = RTCacheCreate(&pThis->pTaskCache, 0, sizeof(LSILOGICTASKSTATE), RTOBJCACHE_PROTECT_INSERT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Cannot create task cache"));

    /* Initialize per device state. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aDeviceStates); i++)
    {
        char szName[24];
        PLSILOGICDEVICE pDevice = &pThis->aDeviceStates[i];

        RTStrPrintf(szName, sizeof(szName), "Device%d", i);

        /* Initialize static parts of the device. */
        pDevice->iLUN = i;
        pDevice->pLsiLogicR3  = pThis;
        pDevice->pLsiLogicR0  = PDMINS_2_DATA_R0PTR(pDevIns);
        pDevice->pLsiLogicRC  = PDMINS_2_DATA_RCPTR(pDevIns);
        pDevice->Led.u32Magic = PDMLED_MAGIC;
        pDevice->IBase.pfnQueryInterface           = lsilogicDeviceQueryInterface;
        pDevice->ISCSIPort.pfnSCSIRequestCompleted = lsilogicDeviceSCSIRequestCompleted;
        pDevice->ILed.pfnQueryStatusLed            = lsilogicDeviceQueryStatusLed;

        /* Attach SCSI driver. */
        rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, szName);
        if (RT_SUCCESS(rc))
        {
            /* Get SCSI connector interface. */
            pDevice->pDrvSCSIConnector = (PPDMISCSICONNECTOR)pDevice->pDrvBase->pfnQueryInterface(pDevice->pDrvBase, PDMINTERFACE_SCSI_CONNECTOR);
            AssertMsgReturn(pDevice->pDrvSCSIConnector, ("Missing SCSI interface below\n"), VERR_PDM_MISSING_INTERFACE);
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pDevice->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("LsiLogic: no driver attached to device %s\n", szName));
        }
        else
        {
            AssertLogRelMsgFailed(("LsiLogic: Failed to attach %s\n", szName));
            return rc;
        }
    }

    /*
     * Attach status driver (optional).
     */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->pLedsConnector = (PDMILEDCONNECTORS *)pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot attach to status driver"));
    }

    /* Initialize the SCSI emulation for the BIOS. */
    rc = vboxscsiInitialize(&pThis->VBoxSCSI);
    AssertRC(rc);

    /* Register I/O port space in ISA region for BIOS access. */
    rc = PDMDevHlpIOPortRegister(pDevIns, LSILOGIC_ISA_IO_PORT, 3, NULL,
                                 lsilogicIsaIOPortWrite, lsilogicIsaIOPortRead,
                                 lsilogicIsaIOPortWriteStr, lsilogicIsaIOPortReadStr,
                                 "LsiLogic BIOS");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot register legacy I/O handlers"));

    /* Register save state handlers. */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, LSILOGIC_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL, NULL, NULL,
                                lsilogicSaveLoadPrep, lsilogicSaveExec, NULL,
                                lsilogicSaveLoadPrep, lsilogicLoadExec, NULL);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot register save state handlers"));

    pThis->enmWhoInit = LSILOGICWHOINIT_SYSTEM_BIOS;

    /* Perform hard reset. */
    rc = lsilogicHardReset(pThis);
    AssertRC(rc);

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceLsiLogicSCSI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "lsilogicscsi",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "LSI Logic 53c1030 SCSI controller.\n",
    /* fFlags */
      PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0
    | PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION
    | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(LSILOGICSCSI),
    /* pfnConstruct */
    lsilogicConstruct,
    /* pfnDestruct */
    lsilogicDestruct,
    /* pfnRelocate */
    lsilogicRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    lsilogicReset,
    /* pfnSuspend */
    lsilogicSuspend,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    lsilogicAttach,
    /* pfnDetach */
    lsilogicDetach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    lsilogicPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
