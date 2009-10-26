/* $Id$ */
/** @file
 * VBox storage devices: BusLogic SCSI host adapter BT-958.
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

/* Implemented looking at the driver source in the linux kernel (drivers/scsi/BusLogic.[ch]). */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DEV_BUSLOGIC
#include <VBox/pdmdev.h>
#include <VBox/pdmifs.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/log.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/cache.h>
# include <iprt/param.h>
#endif

#include "VBoxSCSI.h"
#include "../Builtins.h"

/* Maximum number of attached devices the adapter can handle. */
#define BUSLOGIC_MAX_DEVICES 16

/* Maximum number of scatter gather elements this device can handle. */
#define BUSLOGIC_MAX_SCATTER_GATHER_LIST_SIZE 128

/* Size of the command buffer. */
#define BUSLOGIC_COMMAND_SIZE_MAX 5

/* Size of the reply buffer. */
#define BUSLOGIC_REPLY_SIZE_MAX 64

/* I/O port registered in the ISA compatible range to let the BIOS access
 * the controller.
 */
#define BUSLOGIC_ISA_IO_PORT 0x330

/** State saved version. */
#define BUSLOGIC_SAVED_STATE_MINOR_VERSION 1

/*
 * State of a device attached to the buslogic host adapter.
 */
typedef struct BUSLOGICDEVICE
{
    /** Pointer to the owning buslogic device instance. - R3 pointer */
    R3PTRTYPE(struct BUSLOGIC *)  pBusLogicR3;
    /** Pointer to the owning buslogic device instance. - R0 pointer */
    R0PTRTYPE(struct BUSLOGIC *)  pBusLogicR0;
    /** Pointer to the owning buslogic device instance. - RC pointer */
    RCPTRTYPE(struct BUSLOGIC *)  pBusLogicRC;

    /** Flag whether device is present. */
    bool                          fPresent;
    /** LUN of the device. */
    RTUINT                        iLUN;

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

#if HC_ARCH_BITS == 64
    uint32_t                      Alignment1;
#endif

    /** Number of outstanding tasks on the port. */
    volatile uint32_t             cOutstandingRequests;

} BUSLOGICDEVICE, *PBUSLOGICDEVICE;

/*
 * Commands the BusLogic adapter supports.
 */
enum BUSLOGICCOMMAND
{
    BUSLOGICCOMMAND_TEST_COMMAND_COMPLETE_INTERRUPT = 0x00,
    BUSLOGICCOMMAND_INITIALIZE_MAILBOX = 0x01,
    BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND = 0x02,
    BUSLOGICCOMMAND_EXECUTE_BIOS_COMMAND = 0x03,
    BUSLOGICCOMMAND_INQUIRE_BOARD_ID = 0x04,
    BUSLOGICCOMMAND_ENABLE_OUTGOING_MAILBOX_AVAILABLE_INTERRUPT = 0x05,
    BUSLOGICCOMMAND_SET_SCSI_SELECTION_TIMEOUT = 0x06,
    BUSLOGICCOMMAND_SET_PREEMPT_TIME_ON_BUS = 0x07,
    BUSLOGICCOMMAND_SET_TIME_OFF_BUS = 0x08,
    BUSLOGICCOMMAND_SET_BUS_TRANSFER_RATE = 0x09,
    BUSLOGICCOMMAND_INQUIRE_INSTALLED_DEVICES_ID_0_TO_7 = 0x0a,
    BUSLOGICCOMMAND_INQUIRE_CONFIGURATION = 0x0b,
    BUSLOGICCOMMAND_ENABLE_TARGET_MODE = 0x0c,
    BUSLOGICCOMMAND_INQUIRE_SETUP_INFORMATION = 0x0d,
    BUSLOGICCOMMAND_WRITE_ADAPTER_LOCAL_RAM = 0x1a,
    BUSLOGICCOMMAND_READ_ADAPTER_LOCAL_RAM = 0x1b,
    BUSLOGICCOMMAND_WRITE_BUSMASTER_CHIP_FIFO = 0x1c,
    BUSLOGICCOMMAND_READ_BUSMASTER_CHIP_FIFO = 0x1d,
    BUSLOGICCOMMAND_ECHO_COMMAND_DATA = 0x1f,
    BUSLOGICCOMMAND_HOST_ADAPTER_DIAGNOSTIC = 0x20,
    BUSLOGICCOMMAND_SET_ADAPTER_OPTIONS = 0x21,
    BUSLOGICCOMMAND_INQUIRE_INSTALLED_DEVICES_ID_8_TO_15 = 0x23,
    BUSLOGICCOMMAND_INQUIRE_TARGET_DEVICES = 0x24,
    BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT = 0x25,
    BUSLOGICCOMMAND_INITIALIZE_EXTENDED_MAILBOX = 0x81,
    BUSLOGICCOMMAND_EXECUTE_SCSI_COMMAND = 0x83,
    BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_3RD_LETTER = 0x84,
    BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_LETTER = 0x85,
    BUSLOGICCOMMAND_INQUIRE_PCI_HOST_ADAPTER_INFORMATION = 0x86,
    BUSLOGICCOMMAND_INQUIRE_HOST_ADAPTER_MODEL_NUMBER = 0x8b,
    BUSLOGICCOMMAND_INQUIRE_SYNCHRONOUS_PERIOD = 0x8c,
    BUSLOGICCOMMAND_INQUIRE_EXTENDED_SETUP_INFORMATION = 0x8d,
    BUSLOGICCOMMAND_ENABLE_STRICT_ROUND_ROBIN_MODE = 0x8f,
    BUSLOGICCOMMAND_STORE_HOST_ADAPTER_LOCAL_RAM = 0x90,
    BUSLOGICCOMMAND_FETCH_HOST_ADAPTER_LOCAL_RAM = 0x91,
    BUSLOGICCOMMAND_STORE_LOCAL_DATA_IN_EEPROM = 0x92,
    BUSLOGICCOMMAND_UPLOAD_AUTO_SCSI_CODE = 0x94,
    BUSLOGICCOMMAND_MODIFY_IO_ADDRESS = 0x95,
    BUSLOGICCOMMAND_SET_CCB_FORMAT = 0x96,
    BUSLOGICCOMMAND_WRITE_INQUIRY_BUFFER = 0x9a,
    BUSLOGICCOMMAND_READ_INQUIRY_BUFFER = 0x9b,
    BUSLOGICCOMMAND_FLASH_ROM_UPLOAD_DOWNLOAD = 0xa7,
    BUSLOGICCOMMAND_READ_SCAM_DATA = 0xa8,
    BUSLOGICCOMMAND_WRITE_SCAM_DATA = 0xa9
} BUSLOGICCOMMAND;

#pragma pack(1)
/**
 * Auto SCSI structure which is located
 * in host adapter RAM and contains several
 * configuration parameters.
 */
typedef struct AutoSCSIRam
{
    uint8_t       aInternalSignature[2];
    uint8_t       cbInformation;
    uint8_t       aHostAdaptertype[6];
    uint8_t       uReserved1;
    bool          fFloppyEnabled:           1;
    bool          fFloppySecondary:         1;
    bool          fLevelSensitiveInterrupt: 1;
    unsigned char uReserved2:               2;
    unsigned char uSystemRAMAreForBIOS:     3;
    unsigned char uDMAChannel:              7;
    bool          fDMAAutoConfiguration:    1;
    unsigned char uIrqChannel:              7;
    bool          fIrqAutoConfiguration:    1;
    uint8_t       uDMATransferRate;
    uint8_t       uSCSIId;
    bool          fLowByteTerminated:       1;
    bool          fParityCheckingEnabled:   1;
    bool          fHighByteTerminated:      1;
    bool          fNoisyCablingEnvironment: 1;
    bool          fFastSynchronousNeogtiation: 1;
    bool          fBusResetEnabled:            1;
    bool          fReserved3:                  1;
    bool          fActiveNegotiationEnabled:   1;
    uint8_t       uBusOnDelay;
    uint8_t       uBusOffDelay;
    bool          fHostAdapterBIOSEnabled:     1;
    bool          fBIOSRedirectionOfInt19:     1;
    bool          fExtendedTranslation:        1;
    bool          fMapRemovableAsFixed:        1;
    bool          fReserved4:                  1;
    bool          fBIOSSupportsMoreThan2Drives: 1;
    bool          fBIOSInterruptMode:           1;
    bool          fFlopticalSupport:            1;
    uint16_t      u16DeviceEnabledMask;
    uint16_t      u16WidePermittedMask;
    uint16_t      u16FastPermittedMask;
    uint16_t      u16SynchronousPermittedMask;
    uint16_t      u16DisconnectPermittedMask;
    uint16_t      u16SendStartUnitCommandMask;
    uint16_t      u16IgnoreInBIOSScanMask;
    unsigned char uPCIInterruptPin:             2;
    unsigned char uHostAdapterIoPortAddress:    2;
    bool          fStrictRoundRobinMode:        1;
    bool          fVesaBusSpeedGreaterThan33MHz: 1;
    bool          fVesaBurstWrite:               1;
    bool          fVesaBurstRead:                1;
    uint16_t      u16UltraPermittedMask;
    uint32_t      uReserved5;
    uint8_t       uReserved6;
    uint8_t       uAutoSCSIMaximumLUN;
    bool          fReserved7:                    1;
    bool          fSCAMDominant:                 1;
    bool          fSCAMenabled:                  1;
    bool          fSCAMLevel2:                   1;
    unsigned char uReserved8:                    4;
    bool          fInt13Extension:               1;
    bool          fReserved9:                    1;
    bool          fCDROMBoot:                    1;
    unsigned char uReserved10:                   5;
    unsigned char uBootTargetId:                 4;
    unsigned char uBootChannel:                  4;
    bool          fForceBusDeviceScanningOrder:  1;
    unsigned char uReserved11:                   7;
    uint16_t      u16NonTaggedToAlternateLunPermittedMask;
    uint16_t      u16RenegotiateSyncAfterCheckConditionMask;
    uint8_t       aReserved12[10];
    uint8_t       aManufacturingDiagnostic[2];
    uint16_t      u16Checksum;
} AutoSCSIRam, *PAutoSCSIRam;
AssertCompileSize(AutoSCSIRam, 64);
#pragma pack()

#pragma pack(1)
/**
 * The local Ram.
 */
typedef union HostAdapterLocalRam
{
    /* Byte view. */
    uint8_t u8View[256];
    /* Structured view. */
    struct
    {
        /** Offset 0 - 63 is for BIOS. */
        uint8_t     u8Bios[64];
        /** Auto SCSI structure. */
        AutoSCSIRam autoSCSIData;
    } structured;
} HostAdapterLocalRam, *PHostAdapterLocalRam;
AssertCompileSize(HostAdapterLocalRam, 256);
#pragma pack()

/*
 * Main BusLogic device state.
 */
typedef struct BUSLOGIC
{
    /** The PCI device structure. */
    PCIDEVICE                       dev;
    /** Pointer to the device instance - HC ptr */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 ptr */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

    /* Whether R0 is enabled. */
    bool                            fR0Enabled;
    /** Whether GC is enabled. */
    bool                            fGCEnabled;

    /** Base address of the I/O ports. */
    RTIOPORT                        IOPortBase;
    /** Base address of the memory mapping. */
    RTGCPHYS                        MMIOBase;
    /** Status register - Readonly. */
    volatile uint8_t                regStatus;
    /** Interrupt register - Readonly. */
    volatile uint8_t                regInterrupt;
    /** Geometry register - Readonly. */
    volatile uint8_t                regGeometry;

    /** Local RAM for the fetch hostadapter local RAM request.
     *  I don't know how big the buffer really is but the maximum
     *  seems to be 256 bytes because the offset and count field in the command request
     *  are only one byte big.
     */
    HostAdapterLocalRam             LocalRam;

    /** Command code the guest issued. */
    uint8_t                         uOperationCode;
    /** Buffer for the command parameters the adapter is currently receiving from the guest.
     *  Size of the largest command which is possible.
     */
    uint8_t                         aCommandBuffer[BUSLOGIC_COMMAND_SIZE_MAX]; /* Size of the biggest request. */
    /** Current position in the command buffer. */
    uint8_t                         iParameter;
    /** Parameters left until the command is complete. */
    uint8_t                         cbCommandParametersLeft;

    /** Whether we are using the RAM or reply buffer. */
    bool                            fUseLocalRam;
    /** Buffer to store reply data from the controller to the guest. */
    uint8_t                         aReplyBuffer[BUSLOGIC_REPLY_SIZE_MAX]; /* Size of the biggest reply. */
    /** Position in the buffer we are reading next. */
    uint8_t                         iReply;
    /** Bytes left until the reply buffer is empty. */
    uint8_t                         cbReplyParametersLeft;

    /** Flag whether IRQs are enabled. */
    bool                            fIRQEnabled;
    /** Flag whether the ISA I/O port range is disabled
     * to prevent the BIOs to access the device. */
    bool                            fISAEnabled;

    /** Number of mailboxes the guest set up. */
    uint32_t                        cMailbox;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment0;
#endif

    /** Physical base address of the outgoing mailboxes. */
    RTGCPHYS                        GCPhysAddrMailboxOutgoingBase;
    /** Current outgoing mailbox position. */
    uint32_t                        uMailboxOutgoingPositionCurrent;
    /** Number of mailboxes ready. */
    volatile uint32_t               cMailboxesReady;
    /** Whether a notification to R3 was send. */
    volatile bool                   fNotificationSend;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment1;
#endif

    /** Physical base address of the incoming mailboxes. */
    RTGCPHYS                        GCPhysAddrMailboxIncomingBase;
    /** Current incoming mailbox position. */
    uint32_t                        uMailboxIncomingPositionCurrent;

    /** Whether strict round robin is enabled. */
    bool                            fStrictRoundRobinMode;
    /** Whether the extended LUN CCB format is enabled for 32 possible logical units. */
    bool                            fExtendedLunCCBFormat;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;
    /** Queue to send tasks to R3. - HC ptr */
    R0PTRTYPE(PPDMQUEUE)            pNotifierQueueR0;
    /** Queue to send tasks to R3. - RC ptr */
    RCPTRTYPE(PPDMQUEUE)            pNotifierQueueRC;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment2;
#endif

    /** Cache for task states. */
    R3PTRTYPE(PRTOBJCACHE)          pTaskCache;

    /** Device state for BIOS access. */
    VBOXSCSI                        VBoxSCSI;

    /** BusLogic device states. */
    BUSLOGICDEVICE                  aDeviceStates[BUSLOGIC_MAX_DEVICES];

    /** The base interface */
    PDMIBASE                       IBase;
    /** Status Port - Leds interface. */
    PDMILEDPORTS                   ILeds;
    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)  pLedsConnector;
} BUSLOGIC, *PBUSLOGIC;

/** Register offsets in the I/O port space. */
#define BUSLOGIC_REGISTER_CONTROL   0 /* Writeonly */
/** Fields for the control register. */
# define BUSLOGIC_REGISTER_CONTROL_SCSI_BUSRESET   RT_BIT(4)
# define BUSLOGIC_REGISTER_CONTROL_INTERRUPT_RESET RT_BIT(5)
# define BUSLOGIC_REGISTER_CONTROL_SOFT_RESET      RT_BIT(6)
# define BUSLOGIC_REGISTER_CONTROL_HARD_RESET      RT_BIT(7)

#define BUSLOGIC_REGISTER_STATUS    0 /* Readonly */
/** Fields for the status register. */
# define BUSLOGIC_REGISTER_STATUS_COMMAND_INVALID                 RT_BIT(0)
# define BUSLOGIC_REGISTER_STATUS_DATA_IN_REGISTER_READY          RT_BIT(2)
# define BUSLOGIC_REGISTER_STATUS_COMMAND_PARAMETER_REGISTER_BUSY RT_BIT(3)
# define BUSLOGIC_REGISTER_STATUS_HOST_ADAPTER_READY              RT_BIT(4)
# define BUSLOGIC_REGISTER_STATUS_INITIALIZATION_REQUIRED         RT_BIT(5)
# define BUSLOGIC_REGISTER_STATUS_DIAGNOSTIC_FAILURE              RT_BIT(6)
# define BUSLOGIC_REGISTER_STATUS_DIAGNOSTIC_ACTIVE               RT_BIT(7)

#define BUSLOGIC_REGISTER_COMMAND   1 /* Writeonly */
#define BUSLOGIC_REGISTER_DATAIN    1 /* Readonly */
#define BUSLOGIC_REGISTER_INTERRUPT 2 /* Readonly */
/** Fields for the interrupt register. */
# define BUSLOGIC_REGISTER_INTERRUPT_INCOMING_MAILBOX_LOADED      RT_BIT(0)
# define BUSLOGIC_REGISTER_INTERRUPT_OUTCOMING_MAILBOX_AVAILABLE  RT_BIT(1)
# define BUSLOGIC_REGISTER_INTERRUPT_COMMAND_COMPLETE             RT_BIT(2)
# define BUSLOGIC_REGISTER_INTERRUPT_EXTERNAL_BUS_RESET           RT_BIT(3)
# define BUSLOGIC_REGISTER_INTERRUPT_INTERRUPT_VALID              RT_BIT(7)

#define BUSLOGIC_REGISTER_GEOMETRY  3 /* Readonly */
# define BUSLOGIC_REGISTER_GEOMETRY_EXTENTED_TRANSLATION_ENABLED  RT_BIT(7)

/* Structure for the INQUIRE_PCI_HOST_ADAPTER_INFORMATION reply. */
#pragma pack(1)
typedef struct ReplyInquirePCIHostAdapterInformation
{
    uint8_t       IsaIOPort;
    uint8_t       IRQ;
    unsigned char LowByteTerminated:1;
    unsigned char HighByteTerminated:1;
    unsigned char uReserved:2; /* Reserved. */
    unsigned char JP1:1; /* Whatever that means. */
    unsigned char JP2:1; /* Whatever that means. */
    unsigned char JP3:1; /* Whatever that means. */
    /** Whether the provided info is valid. */
    unsigned char InformationIsValid: 1;
    uint8_t       uReserved2; /* Reserved. */
} ReplyInquirePCIHostAdapterInformation, *PReplyInquirePCIHostAdapterInformation;
AssertCompileSize(ReplyInquirePCIHostAdapterInformation, 4);
#pragma pack()

/* Structure for the INQUIRE_CONFIGURATION reply. */
#pragma pack(1)
typedef struct ReplyInquireConfiguration
{
    unsigned char uReserved1:     5;
    bool          fDmaChannel5:   1;
    bool          fDmaChannel6:   1;
    bool          fDmaChannel7:   1;
    bool          fIrqChannel9:   1;
    bool          fIrqChannel10:  1;
    bool          fIrqChannel11:  1;
    bool          fIrqChannel12:  1;
    unsigned char uReserved2:     1;
    bool          fIrqChannel14:  1;
    bool          fIrqChannel15:  1;
    unsigned char uReserved3:     1;
    unsigned char uHostAdapterId: 4;
    unsigned char uReserved4:     4;
} ReplyInquireConfiguration, *PReplyInquireConfiguration;
AssertCompileSize(ReplyInquireConfiguration, 3);
#pragma pack()

/* Structure for the INQUIRE_SETUP_INFORMATION reply. */
#pragma pack(1)
typedef struct ReplyInquireSetupInformationSynchronousValue
{
    unsigned char uOffset:         4;
    unsigned char uTransferPeriod: 3;
    bool fSynchronous:             1;
}ReplyInquireSetupInformationSynchronousValue, *PReplyInquireSetupInformationSynchronousValue;
AssertCompileSize(ReplyInquireSetupInformationSynchronousValue, 1);
#pragma pack()

#pragma pack(1)
typedef struct ReplyInquireSetupInformation
{
    bool fSynchronousInitiationEnabled: 1;
    bool fParityCheckingEnabled:        1;
    unsigned char uReserved1:           6;
    uint8_t uBusTransferRate;
    uint8_t uPreemptTimeOnBus;
    uint8_t uTimeOffBus;
    uint8_t cMailbox;
    uint8_t MailboxAddress[3];
    ReplyInquireSetupInformationSynchronousValue SynchronousValuesId0To7[8];
    uint8_t uDisconnectPermittedId0To7;
    uint8_t uSignature;
    uint8_t uCharacterD;
    uint8_t uHostBusType;
    uint8_t uWideTransferPermittedId0To7;
    uint8_t uWideTransfersActiveId0To7;
    ReplyInquireSetupInformationSynchronousValue SynchronousValuesId8To15[8];
    uint8_t uDisconnectPermittedId8To15;
    uint8_t uReserved2;
    uint8_t uWideTransferPermittedId8To15;
    uint8_t uWideTransfersActiveId8To15;
} ReplyInquireSetupInformation, *PReplyInquireSetupInformation;
AssertCompileSize(ReplyInquireSetupInformation, 34);
#pragma pack()

/* Structure for the INQUIRE_EXTENDED_SETUP_INFORMATION. */
#pragma pack(1)
typedef struct ReplyInquireExtendedSetupInformation
{
    uint8_t       uBusType;
    uint8_t       uBiosAddress;
    uint16_t      u16ScatterGatherLimit;
    uint8_t       cMailbox;
    uint32_t      uMailboxAddressBase;
    unsigned char uReserved1: 2;
    bool          fFastEISA:  1;
    unsigned char uReserved2: 3;
    bool          fLevelSensitiveInterrupt: 1;
    unsigned char uReserved3: 1;
    unsigned char aFirmwareRevision[3];
    bool          fHostWideSCSI: 1;
    bool          fHostDifferentialSCSI: 1;
    bool          fHostSupportsSCAM: 1;
    bool          fHostUltraSCSI: 1;
    bool          fHostSmartTermination: 1;
    unsigned char uReserved4: 3;
} ReplyInquireExtendedSetupInformation, *PReplyInquireExtendedSetupInformation;
AssertCompileSize(ReplyInquireExtendedSetupInformation, 14);
#pragma pack()

/* Structure for the INITIALIZE EXTENDED MAILBOX request. */
#pragma pack(1)
typedef struct RequestInitializeExtendedMailbox
{
    /** Number of mailboxes in guest memory. */
    uint8_t  cMailbox;
    /** Physical address of the first mailbox. */
    uint32_t uMailboxBaseAddress;
} RequestInitializeExtendedMailbox, *PRequestInitializeExtendedMailbox;
AssertCompileSize(RequestInitializeExtendedMailbox, 5);
#pragma pack()

/*
 * Structure of a mailbox in guest memory.
 * The incoming and outgoing mailbox have the same size
 * but the incoming one has some more fields defined which
 * are marked as reserved in the outgoing one.
 * The last field is also different from the type.
 * For outgoing mailboxes it is the action and
 * for incoming ones the completion status code for the task.
 * We use one structure for both types.
 */
#pragma pack(1)
typedef struct Mailbox
{
    /** Physical adress of the CCB structure in the guest memory. */
    uint32_t u32PhysAddrCCB;
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
            uint8_t  uHostAdapterStatus;
            /** The status of the device which executed the request after executing it. */
            uint8_t  uTargetDeviceStatus;
            /** Reserved. */
            uint8_t  uReserved;
            /** The completion status code of the request. */
            uint8_t uCompletionCode;
        } in;
    } u;
} Mailbox, *PMailbox;
AssertCompileSize(Mailbox, 8);
#pragma pack()

/*
 * Action codes for outgoing mailboxes.
 */
enum BUSLOGIC_MAILBOX_OUTGOING_ACTION
{
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_FREE = 0x00,
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_START_COMMAND = 0x01,
    BUSLOGIC_MAILBOX_OUTGOING_ACTION_ABORT_COMMAND = 0x02
};

/*
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

/*
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

/*
 * Device status codes for incoming mailboxes.
 */
enum BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS
{
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_OPERATION_GOOD = 0x00,
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_CHECK_CONDITION = 0x02,
    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_DEVICE_BUSY = 0x08
};

/*
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

/*
 * Data transfer direction.
 */
enum BUSLOGIC_CCB_DIRECTION
{
    BUSLOGIC_CCB_DIRECTION_UNKNOWN = 0x00,
    BUSLOGIC_CCB_DIRECTION_IN      = 0x01,
    BUSLOGIC_CCB_DIRECTION_OUT     = 0x02,
    BUSLOGIC_CCB_DIRECTION_NO_DATA = 0x03
};

/*
 * The command control block for a SCSI request.
 */
#pragma pack(1)
typedef struct CommandControlBlock
{
    /** Opcode. */
    uint8_t       uOpcode;
    /** Reserved */
    unsigned char uReserved1:       3;
    /** Data direction for the request. */
    unsigned char uDataDirection:   2;
    /** Whether the request is tag queued. */
    bool          fTagQueued:       1;
    /** Queue tag mode. */
    unsigned char uQueueTag:        2;
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
    /** The device the request is send to. */
    uint8_t       uTargetId;
    /**The LUN in the device. */
    unsigned char uLogicalUnit:     5;
    /** Legacy tag. */
    bool          fLegacyTagEnable: 1;
    /** Legacy queue tag. */
    unsigned char uLegacyQueueTag:  2;
    /** The SCSI CDB. */
    uint8_t       aCDB[12]; /* A CDB can be 12 bytes long. */
    /** Reserved. */
    uint8_t       uReserved3[6];
    /** Sense data pointer. */
    uint32_t      u32PhysAddrSenseData;
} CommandControlBlock, *PCommandControlBlock;
AssertCompileSize(CommandControlBlock, 40);
#pragma pack()

#pragma pack(1)
typedef struct ScatterGatherEntry
{
    uint32_t   cbSegment;
    uint32_t   u32PhysAddrSegmentBase;
} ScatterGatherEntry, *PScatterGatherEntry;
AssertCompileSize(ScatterGatherEntry, 8);
#pragma pack()

/*
 * Task state for a CCB request.
 */
typedef struct BUSLOGICTASKSTATE
{
    /** Device this task is assigned to. */
    R3PTRTYPE(PBUSLOGICDEVICE)     pTargetDeviceR3;
    /** The command control block from the guest. */
    CommandControlBlock CommandControlBlockGuest;
    /** Mailbox read from guest memory. */
    Mailbox             MailboxGuest;
    /** The SCSI request we pass to the underlying SCSI engine. */
    PDMSCSIREQUEST      PDMScsiRequest;
    /** Number of bytes in all scatter gather entries. */
    uint32_t            cbScatterGather;
    /** Number of entries in the scatter gather list. */
    uint32_t            cScatterGather;
    /** Page map lock array. */
    PPGMPAGEMAPLOCK     paPageLock;
    /** Pointer to the scatter gather array. */
    PPDMDATASEG         paScatterGather;
    /** Pointer to the page map lock for the sense buffer. */
    PGMPAGEMAPLOCK      pPageLockSense;
    /** Pointer to the R3 sense buffer. */
    uint8_t            *pu8SenseBuffer;
    /** Flag whether this is a request from the BIOS. */
    bool                fBIOS;
} BUSLOGICTASKSTATE, *PBUSLOGICTASKSTATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) buslogicIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                        RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) buslogicIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) buslogicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser,
                                     RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) buslogicMMIORead(PPDMDEVINS pDevIns, void *pvUser,
                                    RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
RT_C_DECLS_END

#define PDMIBASE_2_PBUSLOGICDEVICE(pInterface)     ( (PBUSLOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(BUSLOGICDEVICE, IBase)) )
#define PDMISCSIPORT_2_PBUSLOGICDEVICE(pInterface) ( (PBUSLOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(BUSLOGICDEVICE, ISCSIPort)) )
#define PDMILEDPORTS_2_PBUSLOGICDEVICE(pInterface) ( (PBUSLOGICDEVICE)((uintptr_t)(pInterface) - RT_OFFSETOF(BUSLOGICDEVICE, ILed)) )
#define PDMIBASE_2_PBUSLOGIC(pInterface)           ( (PBUSLOGIC)((uintptr_t)(pInterface) - RT_OFFSETOF(BUSLOGIC, IBase)) )
#define PDMILEDPORTS_2_PBUSLOGIC(pInterface)       ( (PBUSLOGIC)((uintptr_t)(pInterface) - RT_OFFSETOF(BUSLOGIC, ILeds)) )

/**
 * Deasserts the interrupt line of the BusLogic adapter.
 *
 * @returns nothing
 * @param   pBuslogic  Pointer to the BusLogic device instance.
 */
static void buslogicClearInterrupt(PBUSLOGIC pBusLogic)
{
    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));
    pBusLogic->regInterrupt = 0;
    PDMDevHlpPCISetIrqNoWait(pBusLogic->CTX_SUFF(pDevIns), 0, 0);
}

/**
 * Assert IRQ line of the BusLogic adapter.
 *
 * @returns nothing.
 * @param   pBusLogic  Pointer to the BusLogic device instance.
 */
static void buslogicSetInterrupt(PBUSLOGIC pBusLogic)
{
    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));
    pBusLogic->regInterrupt |= BUSLOGIC_REGISTER_INTERRUPT_INTERRUPT_VALID;
    PDMDevHlpPCISetIrqNoWait(pBusLogic->CTX_SUFF(pDevIns), 0, 1);
}

#if defined(IN_RING3)
/**
 * Initialize local RAM of host adapter with default values.
 *
 * @returns nothing.
 * @param   pBusLogic.
 */
static void buslogicInitializeLocalRam(PBUSLOGIC pBusLogic)
{
    /*
     * These values are mostly from what I think is right
     * looking at the dmesg output from a Linux guest inside
     * a VMware server VM.
     *
     * So they don't have to be right :)
     */
    memset(pBusLogic->LocalRam.u8View, 0, sizeof(HostAdapterLocalRam));
    pBusLogic->LocalRam.structured.autoSCSIData.fLevelSensitiveInterrupt = true;
    pBusLogic->LocalRam.structured.autoSCSIData.fParityCheckingEnabled = true;
    pBusLogic->LocalRam.structured.autoSCSIData.fExtendedTranslation = true; /* Same as in geometry register. */
    pBusLogic->LocalRam.structured.autoSCSIData.u16DeviceEnabledMask = ~0; /* All enabled. Maybe mask out non present devices? */
    pBusLogic->LocalRam.structured.autoSCSIData.u16WidePermittedMask = ~0;
    pBusLogic->LocalRam.structured.autoSCSIData.u16FastPermittedMask = ~0;
    pBusLogic->LocalRam.structured.autoSCSIData.u16SynchronousPermittedMask = ~0;
    pBusLogic->LocalRam.structured.autoSCSIData.u16DisconnectPermittedMask = ~0;
    pBusLogic->LocalRam.structured.autoSCSIData.fStrictRoundRobinMode = pBusLogic->fStrictRoundRobinMode;
    pBusLogic->LocalRam.structured.autoSCSIData.u16UltraPermittedMask = ~0;
    /* @todo calculate checksum? */
}

/**
 * Do a hardware reset of the buslogic adapter.
 *
 * @returns VBox status code.
 * @param   pBusLogic Pointer to the BusLogic device instance.
 */
static int buslogicHwReset(PBUSLOGIC pBusLogic)
{
    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));

    /* Reset registers to default value. */
    pBusLogic->regStatus = BUSLOGIC_REGISTER_STATUS_HOST_ADAPTER_READY;
    pBusLogic->regInterrupt = 0;
    pBusLogic->regGeometry = BUSLOGIC_REGISTER_GEOMETRY_EXTENTED_TRANSLATION_ENABLED;
    pBusLogic->uOperationCode = 0xff; /* No command executing. */
    pBusLogic->iParameter = 0;
    pBusLogic->cbCommandParametersLeft = 0;
    pBusLogic->fIRQEnabled = true;
    pBusLogic->fISAEnabled = true;
    pBusLogic->uMailboxOutgoingPositionCurrent = 0;
    pBusLogic->uMailboxIncomingPositionCurrent = 0;

    buslogicInitializeLocalRam(pBusLogic);
    vboxscsiInitialize(&pBusLogic->VBoxSCSI);

    return VINF_SUCCESS;
}
#endif

/**
 * Resets the command state machine for the next command and notifies the guest.
 *
 * @returns nothing.
 * @param   pBusLogic   Pointer to the BusLogic device instance
 */
static void buslogicCommandComplete(PBUSLOGIC pBusLogic)
{
    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));

    pBusLogic->fUseLocalRam = false;
    pBusLogic->regStatus |= BUSLOGIC_REGISTER_STATUS_HOST_ADAPTER_READY;
    pBusLogic->iReply = 0;

    /* Modify I/O address does not generate an interrupt. */
    if (   (pBusLogic->uOperationCode != BUSLOGICCOMMAND_MODIFY_IO_ADDRESS)
        && (pBusLogic->uOperationCode != BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND))
    {
        /* Notify that the command is complete. */
        pBusLogic->regStatus &= ~BUSLOGIC_REGISTER_STATUS_DATA_IN_REGISTER_READY;
        pBusLogic->regInterrupt |= BUSLOGIC_REGISTER_INTERRUPT_COMMAND_COMPLETE;

        if (pBusLogic->fIRQEnabled)
            buslogicSetInterrupt(pBusLogic);
    }

    pBusLogic->uOperationCode = 0xff;
    pBusLogic->iParameter = 0;
}

#if defined(IN_RING3)
/**
 * Initiates a hard reset which was issued from the guest.
 *
 * @returns nothing
 * @param   pBusLogic   Pointer to the BusLogic device instance.
 */
static void buslogicIntiateHardReset(PBUSLOGIC pBusLogic)
{
    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));

    buslogicHwReset(pBusLogic);

    /* We set the diagnostic active in the status register. */
    pBusLogic->regStatus |= BUSLOGIC_REGISTER_STATUS_DIAGNOSTIC_ACTIVE;
}

/**
 * Send a mailbox with set status codes to the guest.
 *
 * @returns nothing.
 * @param   pBusLogicR                Pointer to the BubsLogic device instance.
 * @param   pTaskState                Pointer to the task state with the mailbox to send.
 * @param   uHostAdapterStatus        The host adapter status code to set.
 * @param   uDeviceStatus             The target device status to set.
 * @param   uMailboxCompletionCode    Completion status code to set in the mailbox.
 */
static void buslogicSendIncomingMailbox(PBUSLOGIC pBusLogic, PBUSLOGICTASKSTATE pTaskState,
                                        uint8_t uHostAdapterStatus, uint8_t uDeviceStatus,
                                        uint8_t uMailboxCompletionCode)
{
    pTaskState->MailboxGuest.u.in.uHostAdapterStatus = uHostAdapterStatus;
    pTaskState->MailboxGuest.u.in.uTargetDeviceStatus = uDeviceStatus;
    pTaskState->MailboxGuest.u.in.uCompletionCode = uMailboxCompletionCode;

    RTGCPHYS GCPhysAddrMailboxIncoming = pBusLogic->GCPhysAddrMailboxIncomingBase + (pBusLogic->uMailboxIncomingPositionCurrent * sizeof(Mailbox));
    RTGCPHYS GCPhysAddrCCB = (RTGCPHYS)pTaskState->MailboxGuest.u32PhysAddrCCB;

    /* Update CCB. */
    pTaskState->CommandControlBlockGuest.uHostAdapterStatus = uHostAdapterStatus;
    pTaskState->CommandControlBlockGuest.uDeviceStatus = uDeviceStatus;
    PDMDevHlpPhysWrite(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrCCB, &pTaskState->CommandControlBlockGuest, sizeof(CommandControlBlock));

    /* Update mailbox. */
    PDMDevHlpPhysWrite(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrMailboxIncoming, &pTaskState->MailboxGuest, sizeof(Mailbox));

    /* Advance to next mailbox position. */
    pBusLogic->uMailboxIncomingPositionCurrent++;
    if (pBusLogic->uMailboxIncomingPositionCurrent >= pBusLogic->cMailbox)
        pBusLogic->uMailboxIncomingPositionCurrent = 0;

    pBusLogic->regInterrupt |= BUSLOGIC_REGISTER_INTERRUPT_INCOMING_MAILBOX_LOADED;
    if (pBusLogic->fIRQEnabled)
        buslogicSetInterrupt(pBusLogic);
}

#if defined(DEBUG)
/**
 * Dumps the content of a mailbox for debugging purposes.
 *
 * @return nothing
 * @param  pMailbox   The mialbox to dump.
 * @param  fOutgoing  true if dumping the outgoing state.
 *                    false if dumping the incoming state.
 */
static void buslogicDumpMailboxInfo(PMailbox pMailbox, bool fOutgoing)
{
    Log(("%s: Dump for %s mailbox:\n", __FUNCTION__, fOutgoing ? "outgoing" : "incoming"));
    Log(("%s: u32PhysAddrCCB=%#x\n", __FUNCTION__, pMailbox->u32PhysAddrCCB));
    if (fOutgoing)
    {
        Log(("%s: uActionCode=%u\n", __FUNCTION__, pMailbox->u.out.uActionCode));
    }
    else
    {
        Log(("%s: uHostAdapterStatus=%u\n", __FUNCTION__, pMailbox->u.in.uHostAdapterStatus));
        Log(("%s: uTargetDeviceStatus=%u\n", __FUNCTION__, pMailbox->u.in.uTargetDeviceStatus));
        Log(("%s: uCompletionCode=%u\n", __FUNCTION__, pMailbox->u.in.uCompletionCode));
    }
}

/**
 * Dumps the content of a command control block for debugging purposes.
 *
 * @returns nothing.
 * @param   pCCB    Pointer to the command control block to dump.
 */
static void buslogicDumpCCBInfo(PCommandControlBlock pCCB)
{
    Log(("%s: Dump for Command Control Block:\n", __FUNCTION__));
    Log(("%s: uOpCode=%#x\n", __FUNCTION__, pCCB->uOpcode));
    Log(("%s: uDataDirection=%u\n", __FUNCTION__, pCCB->uDataDirection));
    Log(("%s: fTagQueued=%d\n", __FUNCTION__, pCCB->fTagQueued));
    Log(("%s: uQueueTag=%u\n", __FUNCTION__, pCCB->uQueueTag));
    Log(("%s: cbCDB=%u\n", __FUNCTION__, pCCB->cbCDB));
    Log(("%s: cbSenseData=%u\n", __FUNCTION__, pCCB->cbSenseData));
    Log(("%s: cbData=%u\n", __FUNCTION__, pCCB->cbData));
    Log(("%s: u32PhysAddrData=%#x\n", __FUNCTION__, pCCB->u32PhysAddrData));
    Log(("%s: uHostAdapterStatus=%u\n", __FUNCTION__, pCCB->uHostAdapterStatus));
    Log(("%s: uDeviceStatus=%u\n", __FUNCTION__, pCCB->uDeviceStatus));
    Log(("%s: uTargetId=%u\n", __FUNCTION__, pCCB->uTargetId));
    Log(("%s: uLogicalUnit=%u\n", __FUNCTION__, pCCB->uLogicalUnit));
    Log(("%s: fLegacyTagEnable=%u\n", __FUNCTION__, pCCB->fLegacyTagEnable));
    Log(("%s: uLegacyQueueTag=%u\n", __FUNCTION__, pCCB->uLegacyQueueTag));
    Log(("%s: uCDB[0]=%#x\n", __FUNCTION__, pCCB->aCDB[0]));
    for (int i = 1; i < pCCB->cbCDB; i++)
        Log(("%s: uCDB[%d]=%u\n", __FUNCTION__, i, pCCB->aCDB[i]));
    Log(("%s: u32PhysAddrSenseData=%#x\n", __FUNCTION__, pCCB->u32PhysAddrSenseData));
}
#endif

/**
 * Maps the data buffer into R3.
 *
 * @returns VBox status code.
 * @param   pTaskState    Pointer to the task state.
 * @param   fReadonly     Flag whether the mappings should be readonly.
 */
static int buslogicMapGCDataBufIntoR3(PBUSLOGICTASKSTATE pTaskState, bool fReadonly)
{
    int rc = VINF_SUCCESS;
    uint32_t cScatterGatherEntriesR3 = 0;
    PPDMDEVINS pDevIns = pTaskState->CTX_SUFF(pTargetDevice)->CTX_SUFF(pBusLogic)->CTX_SUFF(pDevIns);

    /*
     * @todo: Check following assumption and what residual means.
     *
     * The BusLogic adapter can handle two different data buffer formats.
     * The first one is that the data pointer entry in the CCB points to
     * the buffer directly. In second mode the data pointer points to a
     * scatter gather list which describes the buffer.
     */
    if (   (pTaskState->CommandControlBlockGuest.uOpcode == BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_SCATTER_GATHER)
        || (pTaskState->CommandControlBlockGuest.uOpcode == BUSLOGIC_CCB_OPCODE_INITIATOR_CCB_RESIDUAL_SCATTER_GATHER))
    {
        uint32_t cScatterGatherGCRead;
        uint32_t iScatterGatherEntry;
        ScatterGatherEntry aScatterGatherReadGC[32]; /* Number of scatter gather list entries read from guest memory. */
        uint32_t cScatterGatherGCLeft = pTaskState->CommandControlBlockGuest.cbData / sizeof(ScatterGatherEntry);
        RTGCPHYS GCPhysAddrScatterGatherCurrent = (RTGCPHYS)pTaskState->CommandControlBlockGuest.u32PhysAddrData;

        /* First pass - count needed R3 scatter gather list entries. */
        do
        {
            cScatterGatherGCRead =   (cScatterGatherGCLeft < RT_ELEMENTS(aScatterGatherReadGC))
                                   ? cScatterGatherGCLeft
                                   : RT_ELEMENTS(aScatterGatherReadGC);
            cScatterGatherGCLeft -= cScatterGatherGCRead;

            /* Read the SG entries. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrScatterGatherCurrent, &aScatterGatherReadGC[0],
                              cScatterGatherGCRead * sizeof(ScatterGatherEntry));

            for (iScatterGatherEntry = 0; iScatterGatherEntry < cScatterGatherGCRead; iScatterGatherEntry++)
            {
                RTGCPHYS    GCPhysAddrDataBase;
                size_t      cbDataToTransfer;

                Log(("%s: iScatterGatherEntry=%u\n", __FUNCTION__, iScatterGatherEntry));

                GCPhysAddrDataBase = (RTGCPHYS)aScatterGatherReadGC[iScatterGatherEntry].u32PhysAddrSegmentBase;
                cbDataToTransfer = aScatterGatherReadGC[iScatterGatherEntry].cbSegment;

                Log(("%s: GCPhysAddrDataBase=%RGp cbDataToTransfer=%u\n", __FUNCTION__, GCPhysAddrDataBase, cbDataToTransfer));

                /*
                 * Check if the physical address is page aligned.
                 */
                if (GCPhysAddrDataBase & PAGE_OFFSET_MASK)
                {
                    RTGCPHYS    GCPhysAddrDataNextPage = PAGE_ADDRESS(GCPhysAddrDataBase) + PAGE_SIZE;
                    uint32_t    u32GCPhysAddrDiff = GCPhysAddrDataNextPage - GCPhysAddrDataBase;

                    Log(("%s: Align page: GCPhysAddrDataBase=%RGp GCPhysAddrDataNextPage=%RGp\n",
                         __FUNCTION__, GCPhysAddrDataBase, GCPhysAddrDataNextPage));

                    cScatterGatherEntriesR3++;
                    /* Subtract size of the buffer in the actual page. */
                    if (cbDataToTransfer < u32GCPhysAddrDiff)
                        cbDataToTransfer = 0;
                    else
                        cbDataToTransfer -= u32GCPhysAddrDiff;
                }

                /* The address is now page aligned. */
                while (cbDataToTransfer)
                {
                    Log(("%s: GCPhysAddrDataBase=%RGp cbDataToTransfer=%u cScatterGatherEntriesR3=%u\n",
                         __FUNCTION__, GCPhysAddrDataBase, cbDataToTransfer, cScatterGatherEntriesR3));

                    cScatterGatherEntriesR3++;

                    /* Check if this is the last page the buffer is in. */
                    if (cbDataToTransfer < PAGE_SIZE)
                        cbDataToTransfer = 0;
                    else
                        cbDataToTransfer -= PAGE_SIZE;
                }
            }

            /* Set address to the next entries to read. */
            GCPhysAddrScatterGatherCurrent += cScatterGatherGCRead * sizeof(ScatterGatherEntry);
        } while (cScatterGatherGCLeft);

        Log(("%s: cScatterGatherEntriesR3=%u\n", __FUNCTION__, cScatterGatherEntriesR3));

        /*
         * Allocate page map lock and scatter gather array.
         * @todo: Optimize with caching.
         */
        AssertMsg(!pTaskState->paPageLock && !pTaskState->paScatterGather, ("paPageLock or/and paScatterGather are not NULL\n"));
        pTaskState->cScatterGather = cScatterGatherEntriesR3;
        pTaskState->cbScatterGather = 0;
        pTaskState->paPageLock = (PPGMPAGEMAPLOCK)RTMemAllocZ(cScatterGatherEntriesR3 * sizeof(PGMPAGEMAPLOCK));
        AssertMsgReturn(pTaskState->paPageLock, ("Allocating page lock array failed\n"), VERR_NO_MEMORY);
        pTaskState->paScatterGather = (PPDMDATASEG)RTMemAllocZ(cScatterGatherEntriesR3 * sizeof(PDMDATASEG));
        AssertMsgReturn(pTaskState->paScatterGather, ("Allocating page lock array failed\n"), VERR_NO_MEMORY);

        /* Second pass - map the elements into R3. **/
        cScatterGatherGCLeft = pTaskState->CommandControlBlockGuest.cbData / sizeof(ScatterGatherEntry);
        GCPhysAddrScatterGatherCurrent = (RTGCPHYS)pTaskState->CommandControlBlockGuest.u32PhysAddrData;
        PPGMPAGEMAPLOCK    pPageLockCurrent = pTaskState->paPageLock;
        PPDMDATASEG        pScatterGatherCurrent = pTaskState->paScatterGather;

        do
        {
            cScatterGatherGCRead = (cScatterGatherGCLeft < RT_ELEMENTS(aScatterGatherReadGC)) ? cScatterGatherGCLeft : RT_ELEMENTS(aScatterGatherReadGC);
            cScatterGatherGCLeft -= cScatterGatherGCRead;

            /* Read the SG entries. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrScatterGatherCurrent, &aScatterGatherReadGC[0], cScatterGatherGCRead * sizeof(ScatterGatherEntry));

            for (iScatterGatherEntry = 0; iScatterGatherEntry < cScatterGatherGCRead; iScatterGatherEntry++)
            {
                RTGCPHYS    GCPhysAddrDataBase;
                uint32_t    cbDataToTransfer;

                GCPhysAddrDataBase = (RTGCPHYS)aScatterGatherReadGC[iScatterGatherEntry].u32PhysAddrSegmentBase;
                cbDataToTransfer = aScatterGatherReadGC[iScatterGatherEntry].cbSegment;
                pTaskState->cbScatterGather += cbDataToTransfer;

                /*
                 * Check if the physical address is page aligned.
                 */
                if (GCPhysAddrDataBase & PAGE_OFFSET_MASK)
                {
                    RTGCPHYS    GCPhysAddrDataNextPage = PAGE_ADDRESS(GCPhysAddrDataBase) + PAGE_SIZE;
                    uint32_t    u32GCPhysAddrDiff = GCPhysAddrDataNextPage - GCPhysAddrDataBase; /* Difference from the buffer start to the next page boundary. */

                    /* Check if the mapping ends at the page boundary and set segment size accordingly. */
                    pScatterGatherCurrent->cbSeg = (cbDataToTransfer < u32GCPhysAddrDiff) ? cbDataToTransfer : u32GCPhysAddrDiff;

                    /* Create the mapping. */
                    if (fReadonly)
                        rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, PAGE_ADDRESS(GCPhysAddrDataBase), 0, (const void **)&pScatterGatherCurrent->pvSeg, pPageLockCurrent); /** @todo r=bird: PAGE_ADDRESS is the wrong macro here as well... */
                    else
                        rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, PAGE_ADDRESS(GCPhysAddrDataBase), 0, &pScatterGatherCurrent->pvSeg, pPageLockCurrent);

                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("Creating mapping failed rc=%Rrc\n", rc));

                    /* Let pvBuf point to the start of the buffer in the page. */
                    pScatterGatherCurrent->pvSeg = ((uint8_t *)pScatterGatherCurrent->pvSeg) + (GCPhysAddrDataBase - PAGE_ADDRESS(GCPhysAddrDataBase));

                    /* Subtract size of the buffer in the actual page. */
                    cbDataToTransfer -= (uint32_t)pScatterGatherCurrent->cbSeg;
                    pPageLockCurrent++;
                    pScatterGatherCurrent++;
                    /* Let physical address point to the next page in the buffer. */
                    GCPhysAddrDataBase = GCPhysAddrDataNextPage;
                }

                /* The address is now page aligned. */
                while (cbDataToTransfer)
                {
                    /* Check if this is the last page the buffer is in. */
                    if (cbDataToTransfer < PAGE_SIZE)
                    {
                        pScatterGatherCurrent->cbSeg = cbDataToTransfer;
                        cbDataToTransfer = 0;
                    }
                    else
                    {
                        cbDataToTransfer -= PAGE_SIZE;
                        pScatterGatherCurrent->cbSeg = PAGE_SIZE;
                    }

                    /* Create the mapping. */
                    if (fReadonly)
                        rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, GCPhysAddrDataBase, 0, (const void **)&pScatterGatherCurrent->pvSeg, pPageLockCurrent);
                    else
                        rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrDataBase, 0, &pScatterGatherCurrent->pvSeg, pPageLockCurrent);

                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("Creating mapping failed rc=%Rrc\n", rc));

                    /* Go to the next page. */
                    GCPhysAddrDataBase += PAGE_SIZE;
                    pPageLockCurrent++;
                    pScatterGatherCurrent++;
                }
            }

            /* Set address to the next entries to read. */
            GCPhysAddrScatterGatherCurrent += cScatterGatherGCRead * sizeof(ScatterGatherEntry);

        } while (cScatterGatherGCLeft);

    }
    else if (   (pTaskState->CommandControlBlockGuest.u32PhysAddrData != 0)
             && (pTaskState->CommandControlBlockGuest.cbData != 0))
    {
        /* The buffer is not scattered. */
        RTGCPHYS GCPhysAddrDataBase     = (RTGCPHYS)PAGE_ADDRESS(pTaskState->CommandControlBlockGuest.u32PhysAddrData);
        RTGCPHYS GCPhysAddrDataEnd      = (RTGCPHYS)(pTaskState->CommandControlBlockGuest.u32PhysAddrData + pTaskState->CommandControlBlockGuest.cbData);
        RTGCPHYS GCPhysAddrDataEndBase  = (RTGCPHYS)PAGE_ADDRESS(GCPhysAddrDataEnd);
        RTGCPHYS GCPhysAddrDataNext     = (RTGCPHYS)PAGE_ADDRESS(GCPhysAddrDataEnd) + PAGE_SIZE;
        uint32_t cPages = (GCPhysAddrDataNext - GCPhysAddrDataBase) / PAGE_SIZE;
        uint32_t cbOffsetFirstPage = pTaskState->CommandControlBlockGuest.u32PhysAddrData & PAGE_OFFSET_MASK;

        Log(("Non scattered buffer:\n"));
        Log(("u32PhysAddrData=%#x\n", pTaskState->CommandControlBlockGuest.u32PhysAddrData));
        Log(("cbData=%u\n", pTaskState->CommandControlBlockGuest.cbData));
        Log(("GCPhysAddrDataBase=0x%RGp\n", GCPhysAddrDataBase));
        Log(("GCPhysAddrDataEnd=0x%RGp\n", GCPhysAddrDataEnd));
        Log(("GCPhysAddrDataEndBase=0x%RGp\n", GCPhysAddrDataEndBase));
        Log(("GCPhysAddrDataNext=0x%RGp\n", GCPhysAddrDataNext));
        Log(("cPages=%u\n", cPages));

        pTaskState->paPageLock = (PPGMPAGEMAPLOCK)RTMemAllocZ(cPages * sizeof(PGMPAGEMAPLOCK));
        AssertMsgReturn(pTaskState->paPageLock, ("Allocating page lock array failed\n"), VERR_NO_MEMORY);
        pTaskState->paScatterGather = (PPDMDATASEG)RTMemAllocZ(cPages * sizeof(PDMDATASEG));
        AssertMsgReturn(pTaskState->paScatterGather, ("Allocating scatter gather list failed\n"), VERR_NO_MEMORY);

        PPGMPAGEMAPLOCK    pPageLockCurrent = pTaskState->paPageLock;
        PPDMDATASEG        pScatterGatherCurrent = pTaskState->paScatterGather;

        for (uint32_t i = 0; i < cPages; i++)
        {
            if (fReadonly)
                rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, GCPhysAddrDataBase, 0, (const void **)&pScatterGatherCurrent->pvSeg, pPageLockCurrent);
            else
                rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrDataBase, 0, &pScatterGatherCurrent->pvSeg, pPageLockCurrent);

            pScatterGatherCurrent->cbSeg = PAGE_SIZE;

            pPageLockCurrent++;
            pScatterGatherCurrent++;
            GCPhysAddrDataBase += PAGE_SIZE;
        }

        /* Correct pointer of the first entry. */
        pTaskState->paScatterGather[0].pvSeg = (uint8_t *)pTaskState->paScatterGather[0].pvSeg + cbOffsetFirstPage;
        pTaskState->paScatterGather[0].cbSeg -= cbOffsetFirstPage;
        /* Correct size of the last entry. */
        pTaskState->paScatterGather[cPages-1].cbSeg = GCPhysAddrDataEnd - GCPhysAddrDataEndBase;
        pTaskState->cScatterGather = cPages;
        pTaskState->cbScatterGather = pTaskState->CommandControlBlockGuest.cbData;
    }

    return rc;
}

/**
 * Free mapped pages and other allocated resources used for the scatter gather list.
 *
 * @returns nothing.
 * @param   pTaskState    Pointer to the task state.
 */
static void buslogicFreeGCDataBuffer(PBUSLOGICTASKSTATE pTaskState)
{
    PPGMPAGEMAPLOCK pPageMapLock = pTaskState->paPageLock;
    PPDMDEVINS      pDevIns = pTaskState->CTX_SUFF(pTargetDevice)->CTX_SUFF(pBusLogic)->CTX_SUFF(pDevIns);

    for (uint32_t iPageLockCurrent = 0; iPageLockCurrent < pTaskState->cScatterGather; iPageLockCurrent++)
    {
        PDMDevHlpPhysReleasePageMappingLock(pDevIns, pPageMapLock);
        pPageMapLock++;
    }

    /* @todo: optimize with caching. */
    RTMemFree(pTaskState->paPageLock);
    RTMemFree(pTaskState->paScatterGather);
    pTaskState->paPageLock      = NULL;
    pTaskState->paScatterGather = NULL;
    pTaskState->cScatterGather  = 0;
    pTaskState->cbScatterGather = 0;
}

/**
 * Free the sense buffer.
 *
 * @returns nothing.
 * @param   pTaskState   Pointer to the task state.
 */
static void buslogicFreeGCSenseBuffer(PBUSLOGICTASKSTATE pTaskState)
{
    PPDMDEVINS pDevIns = pTaskState->CTX_SUFF(pTargetDevice)->CTX_SUFF(pBusLogic)->CTX_SUFF(pDevIns);

    PDMDevHlpPhysReleasePageMappingLock(pDevIns, &pTaskState->pPageLockSense);
    pTaskState->pu8SenseBuffer = NULL;
}

/**
 * Map the sense buffer into R3.
 *
 * @returns VBox status code.
 * @param   pTaskState    Pointer to the task state.
 * @note Current assumption is that the sense buffer is not scattered and does not cross a page boundary.
 */
static int buslogicMapGCSenseBufferIntoR3(PBUSLOGICTASKSTATE pTaskState)
{
    int rc = VINF_SUCCESS;
    PPDMDEVINS pDevIns = pTaskState->CTX_SUFF(pTargetDevice)->CTX_SUFF(pBusLogic)->CTX_SUFF(pDevIns);
    RTGCPHYS GCPhysAddrSenseBuffer = (RTGCPHYS)pTaskState->CommandControlBlockGuest.u32PhysAddrSenseData;
#ifdef RT_STRICT
    uint32_t cbSenseBuffer = pTaskState->CommandControlBlockGuest.cbSenseData;
#endif
    RTGCPHYS GCPhysAddrSenseBufferBase = PAGE_ADDRESS(GCPhysAddrSenseBuffer);

    AssertMsg(GCPhysAddrSenseBuffer >= GCPhysAddrSenseBufferBase,
              ("Impossible GCPhysAddrSenseBuffer < GCPhysAddrSenseBufferBase\n"));

    /* Sanity checks for the assumption. */
    AssertMsg(((GCPhysAddrSenseBuffer + cbSenseBuffer) < (GCPhysAddrSenseBufferBase + PAGE_SIZE)),
              ("Sense buffer crosses page boundary\n"));

    rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrSenseBufferBase, 0, (void **)&pTaskState->pu8SenseBuffer, &pTaskState->pPageLockSense);
    AssertMsgRC(rc, ("Mapping sense buffer failed rc=%Rrc\n", rc));

    /* Correct start address of the sense buffer. */
    pTaskState->pu8SenseBuffer += (GCPhysAddrSenseBuffer - GCPhysAddrSenseBufferBase);

    return rc;
}
#endif /* IN_RING3 */

/**
 * Parses the command buffer and executes it.
 *
 * @returns VBox status code.
 * @param   pBusLogic  Pointer to the BusLogic device instance.
 */
static int buslogicProcessCommand(PBUSLOGIC pBusLogic)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBusLogic=%#p\n", pBusLogic));
    AssertMsg(pBusLogic->uOperationCode != 0xff, ("There is no command to execute\n"));

    switch (pBusLogic->uOperationCode)
    {
        case BUSLOGICCOMMAND_INQUIRE_PCI_HOST_ADAPTER_INFORMATION:
        {
            PReplyInquirePCIHostAdapterInformation pReply = (PReplyInquirePCIHostAdapterInformation)pBusLogic->aReplyBuffer;
            memset(pReply, 0, sizeof(ReplyInquirePCIHostAdapterInformation));

            /* It seems VMware does not provide valid information here too, lets do the same :) */
            pReply->InformationIsValid = 0;
            pReply->IsaIOPort = 0xff; /* Make it invalid. */
            pBusLogic->cbReplyParametersLeft = sizeof(ReplyInquirePCIHostAdapterInformation);
            break;
        }
        case BUSLOGICCOMMAND_MODIFY_IO_ADDRESS:
        {
            pBusLogic->cbReplyParametersLeft = 0;
            if (pBusLogic->aCommandBuffer[0] == 0x06)
            {
                Log(("Disabling ISA I/O ports.\n"));
                pBusLogic->fISAEnabled = false;
            }
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_BOARD_ID:
        {
            pBusLogic->aReplyBuffer[0] = '0'; /* @todo figure out what to write here. */
            pBusLogic->aReplyBuffer[1] = '0'; /* @todo figure out what to write here. */

            /* We report version 5.07B. This reply will provide the first two digits. */
            pBusLogic->aReplyBuffer[2] = '5'; /* Major version 5 */
            pBusLogic->aReplyBuffer[3] = '0'; /* Minor version 0 */
            pBusLogic->cbReplyParametersLeft = 4; /* Reply is 4 bytes long */
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_3RD_LETTER:
        {
            pBusLogic->aReplyBuffer[0] = '7';
            pBusLogic->cbReplyParametersLeft = 1;
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_LETTER:
        {
            pBusLogic->aReplyBuffer[0] = 'B';
            pBusLogic->cbReplyParametersLeft = 1;
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_HOST_ADAPTER_MODEL_NUMBER:
        {
            /* The reply length is set by the guest and is found in the first byte of the command buffer. */
            pBusLogic->cbReplyParametersLeft = pBusLogic->aCommandBuffer[0];
            memset(pBusLogic->aReplyBuffer, 0, pBusLogic->cbReplyParametersLeft);
            const char aModelName[] = "958";
            int cCharsToTransfer =   (pBusLogic->cbReplyParametersLeft <= sizeof(aModelName))
                                   ? pBusLogic->cbReplyParametersLeft
                                   : sizeof(aModelName);

            for (int i = 0; i < cCharsToTransfer; i++)
                pBusLogic->aReplyBuffer[i] = aModelName[i];

            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_CONFIGURATION:
        {
            pBusLogic->cbReplyParametersLeft = sizeof(ReplyInquireConfiguration);
            PReplyInquireConfiguration pReply = (PReplyInquireConfiguration)pBusLogic->aReplyBuffer;
            memset(pReply, 0, sizeof(ReplyInquireConfiguration));

            pReply->uHostAdapterId = 7; /* The controller has always 7 as ID. */
            /*
             * The rest of this reply only applies for ISA adapters.
             * This is a PCI adapter so they are not important and are skipped.
             */
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_EXTENDED_SETUP_INFORMATION:
        {
            /* The reply length is set by the guest and is found in the first byte of the command buffer. */
            pBusLogic->cbReplyParametersLeft = pBusLogic->aCommandBuffer[0];
            PReplyInquireExtendedSetupInformation pReply = (PReplyInquireExtendedSetupInformation)pBusLogic->aReplyBuffer;
            memset(pReply, 0, sizeof(ReplyInquireExtendedSetupInformation));

            pReply->fHostWideSCSI = true;
            pReply->fHostUltraSCSI = true;
            pReply->u16ScatterGatherLimit = 8192;

            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_SETUP_INFORMATION:
        {
            /* The reply length is set by the guest and is found in the first byte of the command buffer. */
            pBusLogic->cbReplyParametersLeft = pBusLogic->aCommandBuffer[0];
            PReplyInquireSetupInformation pReply = (PReplyInquireSetupInformation)pBusLogic->aReplyBuffer;
            memset(pReply, 0, sizeof(ReplyInquireSetupInformation));
            break;
        }
        case BUSLOGICCOMMAND_FETCH_HOST_ADAPTER_LOCAL_RAM:
        {
            /*
             * First element in the command buffer contains start offset to read from
             * and second one the number of bytes to read.
             */
            uint8_t uOffset = pBusLogic->aCommandBuffer[0];
            pBusLogic->cbReplyParametersLeft  = pBusLogic->aCommandBuffer[1];

            pBusLogic->fUseLocalRam = true;
            pBusLogic->iReply = uOffset;
            break;
        }
        case BUSLOGICCOMMAND_INITIALIZE_EXTENDED_MAILBOX:
        {
            PRequestInitializeExtendedMailbox pRequest = (PRequestInitializeExtendedMailbox)pBusLogic->aCommandBuffer;

            pBusLogic->cMailbox = pRequest->cMailbox;
            pBusLogic->GCPhysAddrMailboxOutgoingBase = (RTGCPHYS)pRequest->uMailboxBaseAddress;
            /* The area for incoming mailboxes is right after the last entry of outgoing mailboxes. */
            pBusLogic->GCPhysAddrMailboxIncomingBase = (RTGCPHYS)pRequest->uMailboxBaseAddress + (pBusLogic->cMailbox * sizeof(Mailbox));

            Log(("GCPhysAddrMailboxOutgoingBase=%RGp\n", pBusLogic->GCPhysAddrMailboxOutgoingBase));
            Log(("GCPhysAddrMailboxOutgoingBase=%RGp\n", pBusLogic->GCPhysAddrMailboxIncomingBase));
            Log(("cMailboxes=%u\n", pBusLogic->cMailbox));

            pBusLogic->cbReplyParametersLeft = 0;
            break;
        }
        case BUSLOGICCOMMAND_ENABLE_STRICT_ROUND_ROBIN_MODE:
        {
            if (pBusLogic->aCommandBuffer[0] == 0)
                pBusLogic->fStrictRoundRobinMode = false;
            else if (pBusLogic->aCommandBuffer[0] == 1)
                pBusLogic->fStrictRoundRobinMode = true;
            else
                AssertMsgFailed(("Invalid round robin mode %d\n", pBusLogic->aCommandBuffer[0]));

            pBusLogic->cbReplyParametersLeft = 0;
            break;
        }
        case BUSLOGICCOMMAND_SET_CCB_FORMAT:
        {
            if (pBusLogic->aCommandBuffer[0] == 0)
                pBusLogic->fExtendedLunCCBFormat = false;
            else if (pBusLogic->aCommandBuffer[0] == 1)
                pBusLogic->fExtendedLunCCBFormat = true;
            else
                AssertMsgFailed(("Invalid CCB format %d\n", pBusLogic->aCommandBuffer[0]));

            pBusLogic->cbReplyParametersLeft = 0;
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_TARGET_DEVICES:
        {
            /* Each bit which is set in the 16bit wide variable means a present device. */
            uint16_t u16TargetsPresentMask = 0;

            for (uint8_t i = 0; i < RT_ELEMENTS(pBusLogic->aDeviceStates); i++)
            {
                if (pBusLogic->aDeviceStates[i].fPresent)
                    u16TargetsPresentMask |= (1 << i);
            }
            pBusLogic->aReplyBuffer[0] = (uint8_t)u16TargetsPresentMask;
            pBusLogic->aReplyBuffer[1] = (uint8_t)(u16TargetsPresentMask >> 8);
            pBusLogic->cbReplyParametersLeft = 2;
            break;
        }
        case BUSLOGICCOMMAND_INQUIRE_SYNCHRONOUS_PERIOD:
        {
            pBusLogic->cbReplyParametersLeft = pBusLogic->aCommandBuffer[0];

            for (uint8_t i = 0; i < pBusLogic->cbReplyParametersLeft; i++)
                pBusLogic->aReplyBuffer[i] = 0; /* @todo Figure if we need something other here. It's not needed for the linux driver */

            break;
        }
        case BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT:
        {
            if (pBusLogic->aCommandBuffer[0] == 0)
                pBusLogic->fIRQEnabled = false;
            else
                pBusLogic->fIRQEnabled = true;
            break;
        }
        case BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND: /* Should be handled already. */
        default:
            AssertMsgFailed(("Invalid command %#x\n", pBusLogic->uOperationCode));
    }

    Log(("cbReplyParametersLeft=%d\n", pBusLogic->cbReplyParametersLeft));

    /* Set the data in ready bit in the status register in case the command has a reply. */
    if (pBusLogic->cbReplyParametersLeft)
        pBusLogic->regStatus |= BUSLOGIC_REGISTER_STATUS_DATA_IN_REGISTER_READY;
    else
        buslogicCommandComplete(pBusLogic);

    return rc;
}

/**
 * Read a register from the BusLogic adapter.
 *
 * @returns VBox status code.
 * @param   pBusLogic    Pointer to the BusLogic instance data.
 * @param   iRegister    The index of the register to read.
 * @param   pu32         Where to store the register content.
 */
static int buslogicRegisterRead(PBUSLOGIC pBusLogic, unsigned iRegister, uint32_t *pu32)
{
    int rc = VINF_SUCCESS;

    switch (iRegister)
    {
        case BUSLOGIC_REGISTER_STATUS:
        {
            *pu32 = pBusLogic->regStatus;
            /*
             * If the diagnostic active bit is set we are in a hard reset initiated from the guest.
             * The guest reads the status register and waits that the host adapter ready bit is set.
             */
            if (pBusLogic->regStatus & BUSLOGIC_REGISTER_STATUS_DIAGNOSTIC_ACTIVE)
            {
                pBusLogic->regStatus &= ~BUSLOGIC_REGISTER_STATUS_DIAGNOSTIC_ACTIVE;
                pBusLogic->regStatus |= BUSLOGIC_REGISTER_STATUS_HOST_ADAPTER_READY;
            }
            break;
        }
        case BUSLOGIC_REGISTER_DATAIN:
        {
            if (pBusLogic->fUseLocalRam)
                *pu32 = pBusLogic->LocalRam.u8View[pBusLogic->iReply];
            else
                *pu32 = pBusLogic->aReplyBuffer[pBusLogic->iReply];

            pBusLogic->iReply++;
            pBusLogic->cbReplyParametersLeft--;

            if (!pBusLogic->cbReplyParametersLeft)
            {
                /*
                 * Reply finished, set command complete bit, unset data in ready bit and
                 * interrupt the guest if enabled.
                 */
                buslogicCommandComplete(pBusLogic);
            }
            break;
        }
        case BUSLOGIC_REGISTER_INTERRUPT:
        {
            *pu32 = pBusLogic->regInterrupt;
#if 0
            if (pBusLogic->uOperationCode == BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT)
                rc = PDMDeviceDBGFStop(pBusLogic->CTX_SUFF(pDevIns), RT_SRC_POS, "Interrupt disable command\n");
#endif
            break;
        }
        case BUSLOGIC_REGISTER_GEOMETRY:
        {
            *pu32 = pBusLogic->regGeometry;
            break;
        }
        default:
            AssertMsgFailed(("Register not available\n"));
            rc = VERR_IOM_IOPORT_UNUSED;
    }

    Log2(("%s: pu32=%p:{%.*Rhxs} iRegister=%d rc=%Rrc\n",
          __FUNCTION__, pu32, 1, pu32, iRegister, rc));

    return rc;
}

/**
 * Write a value to a register.
 *
 * @returns VBox status code.
 * @param   pBusLogic    Pointer to the BusLogic instance data.
 * @param   iRegister    The index of the register to read.
 * @param   uVal         The value to write.
 */
static int buslogicRegisterWrite(PBUSLOGIC pBusLogic, unsigned iRegister, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    switch (iRegister)
    {
        case BUSLOGIC_REGISTER_CONTROL:
        {
            if (uVal & BUSLOGIC_REGISTER_CONTROL_INTERRUPT_RESET)
                buslogicClearInterrupt(pBusLogic);

            if ((uVal & BUSLOGIC_REGISTER_CONTROL_HARD_RESET) || (uVal & BUSLOGIC_REGISTER_CONTROL_SOFT_RESET))
            {
#ifdef IN_RING3
                buslogicIntiateHardReset(pBusLogic);
#else
                rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            }

            break;
        }
        case BUSLOGIC_REGISTER_COMMAND:
        {
            /* Fast path for mailbox execution command. */
            if ((uVal == BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND) && (pBusLogic->uOperationCode = 0xff))
            {
                ASMAtomicIncU32(&pBusLogic->cMailboxesReady);
                if (!ASMAtomicXchgBool(&pBusLogic->fNotificationSend, true))
                {
                    /* Send new notification to the queue. */
                    PPDMQUEUEITEMCORE pItem = PDMQueueAlloc(pBusLogic->CTX_SUFF(pNotifierQueue));
                    AssertMsg(pItem, ("Allocating item for queue failed\n"));
                    PDMQueueInsert(pBusLogic->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
                }

                return rc;
            }

            /*
             * Check if we are already fetch command parameters from the guest.
             * If not we initialize executing a new command.
             */
            if (pBusLogic->uOperationCode == 0xff)
            {
                pBusLogic->uOperationCode = uVal;
                pBusLogic->iParameter = 0;

                /* Mark host adapter as busy. */
                pBusLogic->regStatus &= ~BUSLOGIC_REGISTER_STATUS_HOST_ADAPTER_READY;

                /* Get the number of bytes for parameters from the command code. */
                switch (pBusLogic->uOperationCode)
                {
                    case BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_LETTER:
                    case BUSLOGICCOMMAND_INQUIRE_BOARD_ID:
                    case BUSLOGICCOMMAND_INQUIRE_FIRMWARE_VERSION_3RD_LETTER:
                    case BUSLOGICCOMMAND_INQUIRE_PCI_HOST_ADAPTER_INFORMATION:
                    case BUSLOGICCOMMAND_INQUIRE_CONFIGURATION:
                    case BUSLOGICCOMMAND_INQUIRE_TARGET_DEVICES:
                        pBusLogic->cbCommandParametersLeft = 0;
                        break;
                    case BUSLOGICCOMMAND_MODIFY_IO_ADDRESS:
                    case BUSLOGICCOMMAND_INQUIRE_EXTENDED_SETUP_INFORMATION:
                    case BUSLOGICCOMMAND_INQUIRE_SETUP_INFORMATION:
                    case BUSLOGICCOMMAND_INQUIRE_HOST_ADAPTER_MODEL_NUMBER:
                    case BUSLOGICCOMMAND_ENABLE_STRICT_ROUND_ROBIN_MODE:
                    case BUSLOGICCOMMAND_SET_CCB_FORMAT:
                    case BUSLOGICCOMMAND_INQUIRE_SYNCHRONOUS_PERIOD:
                    case BUSLOGICCOMMAND_DISABLE_HOST_ADAPTER_INTERRUPT:
                        pBusLogic->cbCommandParametersLeft = 1;
                        break;
                    case BUSLOGICCOMMAND_FETCH_HOST_ADAPTER_LOCAL_RAM:
                        pBusLogic->cbCommandParametersLeft = 2;
                        break;
                    case BUSLOGICCOMMAND_INITIALIZE_EXTENDED_MAILBOX:
                        pBusLogic->cbCommandParametersLeft = sizeof(RequestInitializeExtendedMailbox);
                        break;
                    case BUSLOGICCOMMAND_EXECUTE_MAILBOX_COMMAND: /* Should not come here anymore. */
                    default:
                        AssertMsgFailed(("Invalid operation code %#x\n", uVal));
                }
            }
            else
            {
                /*
                 * The real adapter would set the Command register busy bit in the status register.
                 * The guest has to wait until it is unset.
                 * We don't need to do it because the guest does not continue execution while we are in this
                 * function.
                 */
                pBusLogic->aCommandBuffer[pBusLogic->iParameter] = uVal;
                pBusLogic->iParameter++;
                pBusLogic->cbCommandParametersLeft--;
            }

            /* Start execution of command if there are no parameters left. */
            if (!pBusLogic->cbCommandParametersLeft)
            {
                rc = buslogicProcessCommand(pBusLogic);
                AssertMsgRC(rc, ("Processing command failed rc=%Rrc\n", rc));
            }
            break;
        }
        default:
            AssertMsgFailed(("Register not available\n"));
            rc = VERR_IOM_IOPORT_UNUSED;
    }

    return rc;
}

/**
 * Memory mapped I/O Handler for read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) buslogicMMIORead(PPDMDEVINS pDevIns, void *pvUser,
                                    RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    /* the linux driver does not make use of the MMIO area. */
    AssertMsgFailed(("MMIO Read\n"));
    return VINF_SUCCESS;
}

/**
 * Memory mapped I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) buslogicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser,
                                     RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    /* the linux driver does not make use of the MMIO area. */
    AssertMsgFailed(("MMIO Write\n"));
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) buslogicIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);;
    unsigned iRegister = Port - pBusLogic->IOPortBase;

    Assert(cb == 1);

    return buslogicRegisterRead(pBusLogic, iRegister, pu32);
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) buslogicIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                        RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    int rc = VINF_SUCCESS;
    unsigned iRegister = Port - pBusLogic->IOPortBase;
    uint8_t uVal = (uint8_t)u32;

    Assert(cb == 1);

    rc = buslogicRegisterWrite(pBusLogic, iRegister, (uint8_t)uVal);

    Log2(("#%d %s: pvUser=%#p cb=%d u32=%#x Port=%#x rc=%Rrc\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, u32, Port, rc));

    return rc;
}

#ifdef IN_RING3
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
static int  buslogicIsaIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                   RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    int rc;
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    Assert(cb == 1);

    if (!pBusLogic->fISAEnabled)
        return VERR_IOM_IOPORT_UNUSED;

    rc = vboxscsiReadRegister(&pBusLogic->VBoxSCSI, (Port - BUSLOGIC_ISA_IO_PORT), pu32);

    Log2(("%s: pu32=%p:{%.*Rhxs} iRegister=%d rc=%Rrc\n",
          __FUNCTION__, pu32, 1, pu32, (Port - BUSLOGIC_ISA_IO_PORT), rc));

    return rc;
}

static int buslogicPrepareBIOSSCSIRequest(PBUSLOGIC pBusLogic)
{
    int rc;
    PBUSLOGICTASKSTATE pTaskState;
    uint32_t           uTargetDevice;

    rc = RTCacheRequest(pBusLogic->pTaskCache, (void **)&pTaskState);
    AssertMsgRCReturn(rc, ("Getting task from cache failed rc=%Rrc\n", rc), rc);

    pTaskState->fBIOS = true;

    rc = vboxscsiSetupRequest(&pBusLogic->VBoxSCSI, &pTaskState->PDMScsiRequest, &uTargetDevice);
    AssertMsgRCReturn(rc, ("Setting up SCSI request failed rc=%Rrc\n", rc), rc);

    pTaskState->PDMScsiRequest.pvUser = pTaskState;

    pTaskState->CTX_SUFF(pTargetDevice) = &pBusLogic->aDeviceStates[uTargetDevice];

    if (!pTaskState->CTX_SUFF(pTargetDevice)->fPresent)
    {
        /* Device is not present. */
        AssertMsg(pTaskState->PDMScsiRequest.pbCDB[0] == SCSI_INQUIRY,
                    ("Device is not present but command is not inquiry\n"));

        SCSIINQUIRYDATA ScsiInquiryData;

        memset(&ScsiInquiryData, 0, sizeof(SCSIINQUIRYDATA));
        ScsiInquiryData.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
        ScsiInquiryData.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_NOT_CONNECTED_NOT_SUPPORTED;

        memcpy(pBusLogic->VBoxSCSI.pBuf, &ScsiInquiryData, 5);

        rc = vboxscsiRequestFinished(&pBusLogic->VBoxSCSI, &pTaskState->PDMScsiRequest);
        AssertMsgRCReturn(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc), rc);

        rc = RTCacheInsert(pBusLogic->pTaskCache, pTaskState);
        AssertMsgRCReturn(rc, ("Getting task from cache failed rc=%Rrc\n", rc), rc);
    }
    else
    {
        LogFlowFunc(("before increment %u\n", pTaskState->CTX_SUFF(pTargetDevice)->cOutstandingRequests));
        ASMAtomicIncU32(&pTaskState->CTX_SUFF(pTargetDevice)->cOutstandingRequests);
        LogFlowFunc(("after increment %u\n", pTaskState->CTX_SUFF(pTargetDevice)->cOutstandingRequests));

        rc = pTaskState->CTX_SUFF(pTargetDevice)->pDrvSCSIConnector->pfnSCSIRequestSend(pTaskState->CTX_SUFF(pTargetDevice)->pDrvSCSIConnector,
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
static int buslogicIsaIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                   RTIOPORT Port, uint32_t u32, unsigned cb)
{
    int rc;
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    Log2(("#%d %s: pvUser=%#p cb=%d u32=%#x Port=%#x\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, u32, Port));

    Assert(cb == 1);

    if (!pBusLogic->fISAEnabled)
        return VERR_IOM_IOPORT_UNUSED;

    rc = vboxscsiWriteRegister(&pBusLogic->VBoxSCSI, (Port - BUSLOGIC_ISA_IO_PORT), (uint8_t)u32);
    if (rc == VERR_MORE_DATA)
    {
        rc = buslogicPrepareBIOSSCSIRequest(pBusLogic);
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
static DECLCALLBACK(int) buslogicIsaIOPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    int rc;

    Log2(("#%d %s: pvUser=%#p cb=%d Port=%#x\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    rc = vboxscsiWriteString(pDevIns, &pBusLogic->VBoxSCSI, (Port - BUSLOGIC_ISA_IO_PORT),
                             pGCPtrSrc, pcTransfer, cb);
    if (rc == VERR_MORE_DATA)
    {
        rc = buslogicPrepareBIOSSCSIRequest(pBusLogic);
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
static DECLCALLBACK(int) buslogicIsaIOPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    LogFlowFunc(("#%d %s: pvUser=%#p cb=%d Port=%#x\n",
                 pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    return vboxscsiReadString(pDevIns, &pBusLogic->VBoxSCSI, (Port - BUSLOGIC_ISA_IO_PORT),
                              pGCPtrDst, pcTransfer, cb);
}

static DECLCALLBACK(int) buslogicMMIOMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion,
                                         RTGCPHYS GCPhysAddress, uint32_t cb,
                                         PCIADDRESSSPACE enmType)
{
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    PBUSLOGIC  pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering MMIO area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    Assert(cb >= 32);

    if (enmType == PCI_ADDRESS_SPACE_MEM)
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL,
                                   buslogicMMIOWrite, buslogicMMIORead, NULL, "BusLogic");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, 0,
                                         "buslogicMMIOWrite", "buslogicMMIORead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterGC(pDevIns, GCPhysAddress, cb, 0,
                                         "buslogicMMIOWrite", "buslogicMMIORead", NULL);
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->MMIOBase = GCPhysAddress;
    }
    else if (enmType == PCI_ADDRESS_SPACE_IO)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, 32,
                                     NULL, buslogicIOPortWrite, buslogicIOPortRead, NULL, NULL, "BusLogic");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, (RTIOPORT)GCPhysAddress, 32,
                                           0, "buslogicIOPortWrite", "buslogicIOPortRead", NULL, NULL, "BusLogic");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, (RTIOPORT)GCPhysAddress, 32,
                                           0, "buslogicIOPortWrite", "buslogicIOPortRead", NULL, NULL, "BusLogic");
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->IOPortBase = (RTIOPORT)GCPhysAddress;
    }
    else
        AssertMsgFailed(("Invalid enmType=%d\n", enmType));

    return rc;
}

static DECLCALLBACK(int) buslogicDeviceSCSIRequestCompleted(PPDMISCSIPORT pInterface, PPDMSCSIREQUEST pSCSIRequest, int rcCompletion)
{
    int rc;
    PBUSLOGICTASKSTATE pTaskState = (PBUSLOGICTASKSTATE)pSCSIRequest->pvUser;
    PBUSLOGICDEVICE pBusLogicDevice = pTaskState->CTX_SUFF(pTargetDevice);
    PBUSLOGIC pBusLogic = pBusLogicDevice->CTX_SUFF(pBusLogic);

    LogFlowFunc(("before decrement %u\n", pBusLogicDevice->cOutstandingRequests));
    ASMAtomicDecU32(&pBusLogicDevice->cOutstandingRequests);
    LogFlowFunc(("after decrement %u\n", pBusLogicDevice->cOutstandingRequests));

    if (pTaskState->fBIOS)
    {
        rc = vboxscsiRequestFinished(&pBusLogic->VBoxSCSI, pSCSIRequest);
        AssertMsgRC(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc));
    }
    else
    {
        buslogicFreeGCDataBuffer(pTaskState);

        if (pTaskState->pu8SenseBuffer)
            buslogicFreeGCSenseBuffer(pTaskState);

        buslogicSendIncomingMailbox(pBusLogic, pTaskState,
                                    BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_CMD_COMPLETED,
                                    BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_OPERATION_GOOD,
                                    BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITHOUT_ERROR);
    }

    /* Add task to the cache. */
    rc = RTCacheInsert(pBusLogic->pTaskCache, pTaskState);
    AssertMsgRC(rc, ("Inserting task state into cache failed rc=%Rrc\n", rc));

    return VINF_SUCCESS;
}

/**
 * Read mailbox from the guest and execute command.
 *
 * @returns VBox status code.
 * @param   pBusLogic    Pointer to the BusLogic instance data.
 */
static int buslogicProcessMailboxNext(PBUSLOGIC pBusLogic)
{
    PBUSLOGICTASKSTATE pTaskState = NULL;
    RTGCPHYS           GCPhysAddrMailboxCurrent;
    int rc;

    rc = RTCacheRequest(pBusLogic->pTaskCache, (void **)&pTaskState);
    AssertMsgReturn(RT_SUCCESS(rc) && (pTaskState != NULL), ("Failed to get task state from cache\n"), rc);

    pTaskState->fBIOS = false;

    if (!pBusLogic->fStrictRoundRobinMode)
    {
        /* Search for a filled mailbox. */
        do
        {
            /* Fetch mailbox from guest memory. */
            GCPhysAddrMailboxCurrent = pBusLogic->GCPhysAddrMailboxOutgoingBase + (pBusLogic->uMailboxOutgoingPositionCurrent * sizeof(Mailbox));

            PDMDevHlpPhysRead(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrMailboxCurrent,
                              &pTaskState->MailboxGuest, sizeof(Mailbox));

            pBusLogic->uMailboxOutgoingPositionCurrent++;

            /* Check if we reached the end and start from the beginning if so. */
            if (pBusLogic->uMailboxOutgoingPositionCurrent >= pBusLogic->cMailbox)
                pBusLogic->uMailboxOutgoingPositionCurrent = 0;
        } while (pTaskState->MailboxGuest.u.out.uActionCode == BUSLOGIC_MAILBOX_OUTGOING_ACTION_FREE);
    }
    else
    {
        /* Fetch mailbox from guest memory. */
        GCPhysAddrMailboxCurrent = pBusLogic->GCPhysAddrMailboxOutgoingBase + (pBusLogic->uMailboxOutgoingPositionCurrent * sizeof(Mailbox));

        PDMDevHlpPhysRead(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrMailboxCurrent,
                          &pTaskState->MailboxGuest, sizeof(Mailbox));
    }

#ifdef DEBUG
    buslogicDumpMailboxInfo(&pTaskState->MailboxGuest, true);
#endif

    if (pTaskState->MailboxGuest.u.out.uActionCode == BUSLOGIC_MAILBOX_OUTGOING_ACTION_START_COMMAND)
    {
        bool fReadonly = false;

        /* Fetch CCB now. */
        RTGCPHYS GCPhysAddrCCB = (RTGCPHYS)pTaskState->MailboxGuest.u32PhysAddrCCB;
        PDMDevHlpPhysRead(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrCCB,
                            &pTaskState->CommandControlBlockGuest, sizeof(CommandControlBlock));

        PBUSLOGICDEVICE pTargetDevice = &pBusLogic->aDeviceStates[pTaskState->CommandControlBlockGuest.uTargetId];
        pTaskState->CTX_SUFF(pTargetDevice) = pTargetDevice;

#ifdef DEBUG
        buslogicDumpCCBInfo(&pTaskState->CommandControlBlockGuest);
#endif

        switch (pTaskState->CommandControlBlockGuest.uDataDirection)
        {
            case BUSLOGIC_CCB_DIRECTION_UNKNOWN:
            case BUSLOGIC_CCB_DIRECTION_IN:
                fReadonly = false;
                break;
            case BUSLOGIC_CCB_DIRECTION_OUT:
            case BUSLOGIC_CCB_DIRECTION_NO_DATA:
                fReadonly = true;
                break;
            default:
                AssertMsgFailed(("Invalid data transfer direction type %u\n",
                                    pTaskState->CommandControlBlockGuest.uDataDirection));
        }

        /* Map required buffers. */
        rc = buslogicMapGCDataBufIntoR3(pTaskState, fReadonly);
        AssertMsgRC(rc, ("Mapping failed rc=%Rrc\n", rc));

        if (pTaskState->CommandControlBlockGuest.cbSenseData)
        {
            rc = buslogicMapGCSenseBufferIntoR3(pTaskState);
            AssertMsgRC(rc, ("Mapping sense buffer failed rc=%Rrc\n", rc));
        }

        /* Check if device is present on bus. If not return error immediately and don't process this further. */
        if (!pBusLogic->aDeviceStates[pTaskState->CommandControlBlockGuest.uTargetId].fPresent)
        {
            buslogicFreeGCDataBuffer(pTaskState);

            if (pTaskState->pu8SenseBuffer)
                buslogicFreeGCSenseBuffer(pTaskState);

            buslogicSendIncomingMailbox(pBusLogic, pTaskState,
                                        BUSLOGIC_MAILBOX_INCOMING_ADAPTER_STATUS_SCSI_SELECTION_TIMEOUT,
                                        BUSLOGIC_MAILBOX_INCOMING_DEVICE_STATUS_OPERATION_GOOD,
                                        BUSLOGIC_MAILBOX_INCOMING_COMPLETION_WITH_ERROR);

            rc = RTCacheInsert(pBusLogic->pTaskCache, pTaskState);
            AssertMsgRC(rc, ("Failed to insert task state into cache rc=%Rrc\n", rc));
        }
        else
        {
            /* Setup SCSI request. */
            pTaskState->PDMScsiRequest.uLogicalUnit          = pTaskState->CommandControlBlockGuest.uLogicalUnit;

            if (pTaskState->CommandControlBlockGuest.uDataDirection == BUSLOGIC_CCB_DIRECTION_UNKNOWN)
                pTaskState->PDMScsiRequest.uDataDirection = PDMSCSIREQUESTTXDIR_UNKNOWN;
            else if (pTaskState->CommandControlBlockGuest.uDataDirection == BUSLOGIC_CCB_DIRECTION_IN)
                pTaskState->PDMScsiRequest.uDataDirection = PDMSCSIREQUESTTXDIR_FROM_DEVICE;
            else if (pTaskState->CommandControlBlockGuest.uDataDirection == BUSLOGIC_CCB_DIRECTION_OUT)
                pTaskState->PDMScsiRequest.uDataDirection = PDMSCSIREQUESTTXDIR_TO_DEVICE;
            else if (pTaskState->CommandControlBlockGuest.uDataDirection == BUSLOGIC_CCB_DIRECTION_NO_DATA)
                pTaskState->PDMScsiRequest.uDataDirection = PDMSCSIREQUESTTXDIR_NONE;
            else
                AssertMsgFailed(("Invalid data direction type %d\n", pTaskState->CommandControlBlockGuest.uDataDirection));

            pTaskState->PDMScsiRequest.cbCDB                 = pTaskState->CommandControlBlockGuest.cbCDB;
            pTaskState->PDMScsiRequest.pbCDB                 = pTaskState->CommandControlBlockGuest.aCDB;
            pTaskState->PDMScsiRequest.cbScatterGather       = pTaskState->cbScatterGather;
            pTaskState->PDMScsiRequest.cScatterGatherEntries = pTaskState->cScatterGather;
            pTaskState->PDMScsiRequest.paScatterGatherHead   = pTaskState->paScatterGather;
            pTaskState->PDMScsiRequest.cbSenseBuffer         = pTaskState->CommandControlBlockGuest.cbSenseData;
            pTaskState->PDMScsiRequest.pbSenseBuffer         = pTaskState->pu8SenseBuffer;
            pTaskState->PDMScsiRequest.pvUser                = pTaskState;

            LogFlowFunc(("before increment %u\n", pTargetDevice->cOutstandingRequests));
            ASMAtomicIncU32(&pTargetDevice->cOutstandingRequests);
            LogFlowFunc(("after increment %u\n", pTargetDevice->cOutstandingRequests));
            rc = pTargetDevice->pDrvSCSIConnector->pfnSCSIRequestSend(pTargetDevice->pDrvSCSIConnector, &pTaskState->PDMScsiRequest);
            AssertMsgRC(rc, ("Sending request to SCSI layer failed rc=%Rrc\n", rc));
        }
    }
    else if (pTaskState->MailboxGuest.u.out.uActionCode == BUSLOGIC_MAILBOX_OUTGOING_ACTION_ABORT_COMMAND)
    {
        AssertMsgFailed(("Not implemented yet\n"));
    }
    else
        AssertMsgFailed(("Invalid outgoing mailbox action code %u\n", pTaskState->MailboxGuest.u.out.uActionCode));

    /* We got the mailbox, mark it as free in the guest. */
    pTaskState->MailboxGuest.u.out.uActionCode = BUSLOGIC_MAILBOX_OUTGOING_ACTION_FREE;
    PDMDevHlpPhysWrite(pBusLogic->CTX_SUFF(pDevIns), GCPhysAddrMailboxCurrent, &pTaskState->MailboxGuest, sizeof(Mailbox));

    if (pBusLogic->fStrictRoundRobinMode)
    {
        pBusLogic->uMailboxOutgoingPositionCurrent++;

        /* Check if we reached the end and start from the beginning if so. */
        if (pBusLogic->uMailboxOutgoingPositionCurrent >= pBusLogic->cMailbox)
            pBusLogic->uMailboxOutgoingPositionCurrent = 0;
    }

    return rc;
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
static DECLCALLBACK(bool) buslogicNotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    PBUSLOGIC  pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    AssertMsg(pBusLogic->cMailboxesReady > 0, ("Got notification without any mailboxes ready\n"));

    /* Reset notification send flag now. */
    ASMAtomicXchgBool(&pBusLogic->fNotificationSend, false);

    /* Process mailboxes. */
    do
    {
        int rc;

        rc = buslogicProcessMailboxNext(pBusLogic);
        AssertMsgRC(rc, ("Processing mailbox failed rc=%Rrc\n", rc));
    } while (ASMAtomicDecU32(&pBusLogic->cMailboxesReady) > 0);

    return true;
}

static bool buslogicWaitForAsyncIOFinished(PBUSLOGIC pBusLogic, unsigned cMillies)
{
    uint64_t u64Start;
    bool     fIdle;

    /*
     * Wait for any pending async operation to finish
     */
    u64Start = RTTimeMilliTS();
    for (;;)
    {
        fIdle = true;

        /* Check every port. */
        for (unsigned i = 0; i < RT_ELEMENTS(pBusLogic->aDeviceStates); i++)
        {
            PBUSLOGICDEVICE pBusLogicDevice = &pBusLogic->aDeviceStates[i];
            if (ASMAtomicReadU32(&pBusLogicDevice->cOutstandingRequests))
            {
                fIdle = false;
                break;
            }
        }
        if (   fIdle
            || RTTimeMilliTS() - u64Start >= cMillies)
            break;

        /* Sleep for a bit. */
        RTThreadSleep(100); /** @todo wait on something which can be woken up. 100ms is too long for teleporting VMs! */
    }

    return fIdle;
}

static DECLCALLBACK(int) buslogicLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PBUSLOGIC pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    /* Save the device config. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aDeviceStates); i++)
        SSMR3PutBool(pSSM, pThis->aDeviceStates[i].fPresent);

    return VINF_SSM_DONT_CALL_AGAIN;
}

static DECLCALLBACK(int) buslogicSaveLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    /* Wait that no task is pending on any device. */
    if (!buslogicWaitForAsyncIOFinished(pBusLogic, 20000))
    {
        AssertLogRelMsgFailed(("BusLogic: There are still tasks outstanding\n"));
        return VERR_TIMEOUT;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) buslogicSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    /* Every device first. */
    for (unsigned i = 0; i < RT_ELEMENTS(pBusLogic->aDeviceStates); i++)
    {
        PBUSLOGICDEVICE pDevice = &pBusLogic->aDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        SSMR3PutBool(pSSM, pDevice->fPresent);
        SSMR3PutU32(pSSM, pDevice->cOutstandingRequests);
    }
    /* Now the main device state. */
    SSMR3PutU8    (pSSM, pBusLogic->regStatus);
    SSMR3PutU8    (pSSM, pBusLogic->regInterrupt);
    SSMR3PutU8    (pSSM, pBusLogic->regGeometry);
    SSMR3PutMem   (pSSM, &pBusLogic->LocalRam, sizeof(pBusLogic->LocalRam));
    SSMR3PutU8    (pSSM, pBusLogic->uOperationCode);
    SSMR3PutMem   (pSSM, &pBusLogic->aCommandBuffer, sizeof(pBusLogic->aCommandBuffer));
    SSMR3PutU8    (pSSM, pBusLogic->iParameter);
    SSMR3PutU8    (pSSM, pBusLogic->cbCommandParametersLeft);
    SSMR3PutBool  (pSSM, pBusLogic->fUseLocalRam);
    SSMR3PutMem   (pSSM, pBusLogic->aReplyBuffer, sizeof(pBusLogic->aReplyBuffer));
    SSMR3PutU8    (pSSM, pBusLogic->iReply);
    SSMR3PutU8    (pSSM, pBusLogic->cbReplyParametersLeft);
    SSMR3PutBool  (pSSM, pBusLogic->fIRQEnabled);
    SSMR3PutBool  (pSSM, pBusLogic->fISAEnabled);
    SSMR3PutU32   (pSSM, pBusLogic->cMailbox);
    SSMR3PutGCPhys(pSSM, pBusLogic->GCPhysAddrMailboxOutgoingBase);
    SSMR3PutU32   (pSSM, pBusLogic->uMailboxOutgoingPositionCurrent);
    SSMR3PutU32   (pSSM, pBusLogic->cMailboxesReady);
    SSMR3PutBool  (pSSM, pBusLogic->fNotificationSend);
    SSMR3PutGCPhys(pSSM, pBusLogic->GCPhysAddrMailboxIncomingBase);
    SSMR3PutU32   (pSSM, pBusLogic->uMailboxIncomingPositionCurrent);
    SSMR3PutBool  (pSSM, pBusLogic->fStrictRoundRobinMode);
    SSMR3PutBool  (pSSM, pBusLogic->fExtendedLunCCBFormat);
    /* Now the data for the BIOS interface. */
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.regIdentify);
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.uTargetDevice);
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.uTxDir);
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.cbCDB);
    SSMR3PutMem   (pSSM, pBusLogic->VBoxSCSI.aCDB, sizeof(pBusLogic->VBoxSCSI.aCDB));
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.iCDB);
    SSMR3PutU32   (pSSM, pBusLogic->VBoxSCSI.cbBuf);
    SSMR3PutU32   (pSSM, pBusLogic->VBoxSCSI.iBuf);
    SSMR3PutBool  (pSSM, pBusLogic->VBoxSCSI.fBusy);
    SSMR3PutU8    (pSSM, pBusLogic->VBoxSCSI.enmState);
    if (pBusLogic->VBoxSCSI.cbCDB)
        SSMR3PutMem(pSSM, pBusLogic->VBoxSCSI.pBuf, pBusLogic->VBoxSCSI.cbBuf);

    return SSMR3PutU32(pSSM, ~0);
}

static DECLCALLBACK(int) buslogicLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PBUSLOGIC   pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    int         rc;

    /* We support saved states only from this and older versions. */
    if (uVersion > BUSLOGIC_SAVED_STATE_MINOR_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* Every device first. */
    for (unsigned i = 0; i < RT_ELEMENTS(pBusLogic->aDeviceStates); i++)
    {
        PBUSLOGICDEVICE pDevice = &pBusLogic->aDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        bool fPresent;
        rc = SSMR3GetBool(pSSM, &fPresent);
        AssertRCReturn(rc, rc);
        if (pDevice->fPresent != fPresent)
        {
            LogRel(("BusLogic: Target %u config mismatch: config=%RTbool state=%RTbool\n", i, pDevice->fPresent, fPresent));
            return VERR_SSM_LOAD_CONFIG_MISMATCH;
        }

        if (uPass == SSM_PASS_FINAL)
            SSMR3GetU32(pSSM, (uint32_t *)&pDevice->cOutstandingRequests);
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* Now the main device state. */
    SSMR3GetU8    (pSSM, (uint8_t *)&pBusLogic->regStatus);
    SSMR3GetU8    (pSSM, (uint8_t *)&pBusLogic->regInterrupt);
    SSMR3GetU8    (pSSM, (uint8_t *)&pBusLogic->regGeometry);
    SSMR3GetMem   (pSSM, &pBusLogic->LocalRam, sizeof(pBusLogic->LocalRam));
    SSMR3GetU8    (pSSM, &pBusLogic->uOperationCode);
    SSMR3GetMem   (pSSM, &pBusLogic->aCommandBuffer, sizeof(pBusLogic->aCommandBuffer));
    SSMR3GetU8    (pSSM, &pBusLogic->iParameter);
    SSMR3GetU8    (pSSM, &pBusLogic->cbCommandParametersLeft);
    SSMR3GetBool  (pSSM, &pBusLogic->fUseLocalRam);
    SSMR3GetMem   (pSSM, pBusLogic->aReplyBuffer, sizeof(pBusLogic->aReplyBuffer));
    SSMR3GetU8    (pSSM, &pBusLogic->iReply);
    SSMR3GetU8    (pSSM, &pBusLogic->cbReplyParametersLeft);
    SSMR3GetBool  (pSSM, &pBusLogic->fIRQEnabled);
    SSMR3GetBool  (pSSM, &pBusLogic->fISAEnabled);
    SSMR3GetU32   (pSSM, &pBusLogic->cMailbox);
    SSMR3GetGCPhys(pSSM, &pBusLogic->GCPhysAddrMailboxOutgoingBase);
    SSMR3GetU32   (pSSM, &pBusLogic->uMailboxOutgoingPositionCurrent);
    SSMR3GetU32   (pSSM, (uint32_t *)&pBusLogic->cMailboxesReady);
    SSMR3GetBool  (pSSM, (bool *)&pBusLogic->fNotificationSend);
    SSMR3GetGCPhys(pSSM, &pBusLogic->GCPhysAddrMailboxIncomingBase);
    SSMR3GetU32   (pSSM, &pBusLogic->uMailboxIncomingPositionCurrent);
    SSMR3GetBool  (pSSM, &pBusLogic->fStrictRoundRobinMode);
    SSMR3GetBool  (pSSM, &pBusLogic->fExtendedLunCCBFormat);
    /* Now the data for the BIOS interface. */
    SSMR3GetU8  (pSSM, &pBusLogic->VBoxSCSI.regIdentify);
    SSMR3GetU8  (pSSM, &pBusLogic->VBoxSCSI.uTargetDevice);
    SSMR3GetU8  (pSSM, &pBusLogic->VBoxSCSI.uTxDir);
    SSMR3GetU8  (pSSM, &pBusLogic->VBoxSCSI.cbCDB);
    SSMR3GetMem (pSSM, pBusLogic->VBoxSCSI.aCDB, sizeof(pBusLogic->VBoxSCSI.aCDB));
    SSMR3GetU8  (pSSM, &pBusLogic->VBoxSCSI.iCDB);
    SSMR3GetU32 (pSSM, &pBusLogic->VBoxSCSI.cbBuf);
    SSMR3GetU32 (pSSM, &pBusLogic->VBoxSCSI.iBuf);
    SSMR3GetBool(pSSM, (bool *)&pBusLogic->VBoxSCSI.fBusy);
    SSMR3GetU8  (pSSM, (uint8_t *)&pBusLogic->VBoxSCSI.enmState);
    if (pBusLogic->VBoxSCSI.cbCDB)
    {
        pBusLogic->VBoxSCSI.pBuf = (uint8_t *)RTMemAllocZ(pBusLogic->VBoxSCSI.cbCDB);
        if (!pBusLogic->VBoxSCSI.pBuf)
        {
            LogRel(("BusLogic: Out of memory during restore.\n"));
            return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY,
                                    N_("BusLogic: Out of memory during restore\n"));
        }
        SSMR3GetMem(pSSM, pBusLogic->VBoxSCSI.pBuf, pBusLogic->VBoxSCSI.cbBuf);
    }

    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);
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
static DECLCALLBACK(int) buslogicDeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PBUSLOGICDEVICE pDevice = PDMILEDPORTS_2_PBUSLOGICDEVICE(pInterface);
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
 * @param   pInterface          Pointer to BUSLOGICDEVICE::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) buslogicDeviceQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PBUSLOGICDEVICE pDevice = PDMIBASE_2_PBUSLOGICDEVICE(pInterface);

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
static DECLCALLBACK(int) buslogicStatusQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PBUSLOGIC pBusLogic = PDMILEDPORTS_2_PBUSLOGIC(pInterface);
    if (iLUN < BUSLOGIC_MAX_DEVICES)
    {
        *ppLed = &pBusLogic->aDeviceStates[iLUN].Led;
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
static DECLCALLBACK(void *) buslogicStatusQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PBUSLOGIC pBusLogic = PDMIBASE_2_PBUSLOGIC(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pBusLogic->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pBusLogic->ILeds;
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
static DECLCALLBACK(void) buslogicDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PBUSLOGIC       pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    PBUSLOGICDEVICE pDevice = &pThis->aDeviceStates[iLUN];

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("BusLogic: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pDevice->pDrvBase = NULL;
    pDevice->fPresent = false;
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
static DECLCALLBACK(int)  buslogicAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PBUSLOGIC       pThis   = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    PBUSLOGICDEVICE pDevice = &pThis->aDeviceStates[iLUN];
    int rc;

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("BusLogic: Device does not support hotplugging\n"),
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
        pDevice->fPresent = true;
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

static DECLCALLBACK(void) buslogicSuspend(PPDMDEVINS pDevIns)
{
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    /* Wait that no task is pending on any device. */
    if (!buslogicWaitForAsyncIOFinished(pBusLogic, 20000))
        AssertLogRelMsgFailed(("BusLogic: There are still tasks outstanding\n"));
}

static DECLCALLBACK(void) buslogicRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    uint32_t i;
    PBUSLOGIC pBusLogic = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    pBusLogic->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pBusLogic->pNotifierQueueRC = PDMQueueRCPtr(pBusLogic->pNotifierQueueR3);

    for (i = 0; i < BUSLOGIC_MAX_DEVICES; i++)
    {
        PBUSLOGICDEVICE pDevice = &pBusLogic->aDeviceStates[i];

        pDevice->pBusLogicRC = PDMINS_2_DATA_RCPTR(pDevIns);
    }

}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  buslogicReset(PPDMDEVINS pDevIns)
{
    PBUSLOGIC pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    buslogicHwReset(pThis);
}

/**
 * Destroy a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) buslogicDestruct(PPDMDEVINS pDevIns)
{
    int rc = VINF_SUCCESS;
    PBUSLOGIC  pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);

    rc = RTCacheDestroy(pThis->pTaskCache);
    AssertMsgRC(rc, ("Destroying task cache failed rc=%Rrc\n", rc));

    return rc;
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) buslogicConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PBUSLOGIC  pThis = PDMINS_2_DATA(pDevIns, PBUSLOGIC);
    int        rc = VINF_SUCCESS;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "GCEnabled\0"
                              "R0Enabled\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("BusLogic configuration error: unknown option specified"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &pThis->fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("BusLogic configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, pThis->fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("BusLogic configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, pThis->fR0Enabled));


    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->IBase.pfnQueryInterface = buslogicStatusQueryInterface;
    pThis->ILeds.pfnQueryStatusLed = buslogicStatusQueryStatusLed;

    PCIDevSetVendorId         (&pThis->dev, 0x104b); /* BusLogic */
    PCIDevSetDeviceId         (&pThis->dev, 0x1040); /* BT-958 */
    PCIDevSetCommand          (&pThis->dev, 0x0003);
    PCIDevSetRevisionId       (&pThis->dev, 0x01);
    PCIDevSetClassProg        (&pThis->dev, 0x00); /* SCSI */
    PCIDevSetClassSub         (&pThis->dev, 0x00); /* SCSI */
    PCIDevSetClassBase        (&pThis->dev, 0x01); /* Mass storage */
    PCIDevSetBaseAddress      (&pThis->dev, 0, true  /*IO*/, false /*Pref*/, false /*64-bit*/, 0x00000000);
    PCIDevSetBaseAddress      (&pThis->dev, 1, false /*IO*/, false /*Pref*/, false /*64-bit*/, 0x00000000);
    PCIDevSetSubSystemVendorId(&pThis->dev, 0x104b);
    PCIDevSetSubSystemId      (&pThis->dev, 0x1040);
    PCIDevSetInterruptLine    (&pThis->dev, 0x00);
    PCIDevSetInterruptPin     (&pThis->dev, 0x01);

    /*
     * Register the PCI device, it's I/O regions.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 32, PCI_ADDRESS_SPACE_IO, buslogicMMIOMap);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, 32, PCI_ADDRESS_SPACE_MEM, buslogicMMIOMap);
    if (RT_FAILURE(rc))
        return rc;

    /* Register I/O port space in ISA region for BIOS access. */
    rc = PDMDevHlpIOPortRegister(pDevIns, BUSLOGIC_ISA_IO_PORT, 3, NULL,
                                 buslogicIsaIOPortWrite, buslogicIsaIOPortRead,
                                 buslogicIsaIOPortWriteStr, buslogicIsaIOPortReadStr,
                                 "BusLogic BIOS");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("BusLogic cannot register legacy I/O handlers"));

    /* Initialize task cache. */
    rc = RTCacheCreate(&pThis->pTaskCache, 0 /* unlimited */, sizeof(BUSLOGICTASKSTATE), RTOBJCACHE_PROTECT_INSERT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("BusLogic: Failed to initialize task cache\n"));

    /* Intialize task queue. */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 5, 0,
                                 buslogicNotifyQueueConsumer, true, "BugLogicTask", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotifierQueueR0 = PDMQueueR0Ptr(pThis->pNotifierQueueR3);
    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    /* Initialize per device state. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aDeviceStates); i++)
    {
        char szName[24];
        PBUSLOGICDEVICE pDevice = &pThis->aDeviceStates[i];

        RTStrPrintf(szName, sizeof(szName), "Device%d", i);

        /* Initialize static parts of the device. */
        pDevice->iLUN = i;
        pDevice->pBusLogicR3 = pThis;
        pDevice->pBusLogicR0 = PDMINS_2_DATA_R0PTR(pDevIns);
        pDevice->pBusLogicRC = PDMINS_2_DATA_RCPTR(pDevIns);
        pDevice->Led.u32Magic = PDMLED_MAGIC;
        pDevice->IBase.pfnQueryInterface           = buslogicDeviceQueryInterface;
        pDevice->ISCSIPort.pfnSCSIRequestCompleted = buslogicDeviceSCSIRequestCompleted;
        pDevice->ILed.pfnQueryStatusLed            = buslogicDeviceQueryStatusLed;

        /* Attach SCSI driver. */
        rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, szName);
        if (RT_SUCCESS(rc))
        {
            /* Get SCSI connector interface. */
            pDevice->pDrvSCSIConnector = (PPDMISCSICONNECTOR)pDevice->pDrvBase->pfnQueryInterface(pDevice->pDrvBase, PDMINTERFACE_SCSI_CONNECTOR);
            AssertMsgReturn(pDevice->pDrvSCSIConnector, ("Missing SCSI interface below\n"), VERR_PDM_MISSING_INTERFACE);

            pDevice->fPresent = true;
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pDevice->pDrvBase = NULL;
            pDevice->fPresent = false;
            rc = VINF_SUCCESS;
            Log(("BusLogic: no driver attached to device %s\n", szName));
        }
        else
        {
            AssertLogRelMsgFailed(("BusLogic: Failed to attach %s\n", szName));
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
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("BusLogic cannot attach to status driver"));
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, BUSLOGIC_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL,                 buslogicLiveExec, NULL,
                                buslogicSaveLoadPrep, buslogicSaveExec, NULL,
                                buslogicSaveLoadPrep, buslogicLoadExec, NULL);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("BusLogic cannot register save state handlers"));

    rc = buslogicHwReset(pThis);
    AssertMsgRC(rc, ("hardware reset of BusLogic host adapter failed rc=%Rrc\n", rc));

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceBusLogic =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "buslogic",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "BusLogic BT-958 SCSI host adapter.\n",
    /* fFlags */
      PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0
    | PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION
    | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(BUSLOGIC),
    /* pfnConstruct */
    buslogicConstruct,
    /* pfnDestruct */
    buslogicDestruct,
    /* pfnRelocate */
    buslogicRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    buslogicReset,
    /* pfnSuspend */
    buslogicSuspend,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    buslogicAttach,
    /* pfnDetach */
    buslogicDetach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

