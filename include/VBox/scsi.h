/** @file
 * VirtualBox - SCSI declarations.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_scsi_h
#define ___VBox_scsi_h

#include <iprt/assert.h>


/**
 * SCSI command opcode identifiers.
 *
 * SCSI-3, so far for CD/DVD Logical Units, from Table 49 of the MMC-3 draft standard.
 */
typedef enum SCSICMD
{
    SCSI_BLANK                          = 0xa1,
    SCSI_CLOSE_TRACK_SESSION            = 0x5b,
    SCSI_ERASE_10                       = 0x2c,
    SCSI_FORMAT_UNIT                    = 0x04,
    SCSI_GET_CONFIGURATION              = 0x46,
    SCSI_GET_EVENT_STATUS_NOTIFICATION  = 0x4a,
    SCSI_GET_PERFORMANCE                = 0xac,
    /** Inquiry command. */
    SCSI_INQUIRY                        = 0x12,
    SCSI_LOAD_UNLOAD_MEDIUM             = 0xa6,
    SCSI_MECHANISM_STATUS               = 0xbd,
    SCSI_MODE_SELECT_10                 = 0x55,
    SCSI_MODE_SENSE_10                  = 0x5a,
    SCSI_PAUSE_RESUME                   = 0x4b,
    SCSI_PLAY_AUDIO_10                  = 0x45,
    SCSI_PLAY_AUDIO_12                  = 0xa5,
    SCSI_PLAY_AUDIO_MSF                 = 0x47,
    SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1e,
    /** Read(10) command. */
    SCSI_READ_10                        = 0x28,
    SCSI_READ_12                        = 0xa8,
    SCSI_READ_BUFFER                    = 0x3c,
    SCSI_READ_BUFFER_CAPACITY           = 0x5c,
    /** Read Capacity(6) command. */
    SCSI_READ_CAPACITY                  = 0x25,
    SCSI_READ_CD                        = 0xbe,
    SCSI_READ_CD_MSF                    = 0xb9,
    SCSI_READ_DISC_INFORMATION          = 0x51,
    SCSI_READ_DVD_STRUCTURE             = 0xad,
    SCSI_READ_FORMAT_CAPACITIES         = 0x23,
    SCSI_READ_SUBCHANNEL                = 0x42,
    SCSI_READ_TOC_PMA_ATIP              = 0x43,
    SCSI_READ_TRACK_INFORMATION         = 0x52,
    SCSI_REPAIR_TRACK                   = 0x58,
    SCSI_REPORT_KEY                     = 0xa4,
    SCSI_REQUEST_SENSE                  = 0x03,
    SCSI_RESERVE_TRACK                  = 0x53,
    SCSI_SCAN                           = 0xba,
    SCSI_SEEK_10                        = 0x2b,
    SCSI_SEND_CUE_SHEET                 = 0x5d,
    SCSI_SEND_DVD_STRUCTURE             = 0xbf,
    SCSI_SEND_EVENT                     = 0xa2,
    SCSI_SEND_KEY                       = 0xa3,
    SCSI_SEND_OPC_INFORMATION           = 0x54,
    SCSI_SET_CD_SPEED                   = 0xbb,
    SCSI_SET_READ_AHEAD                 = 0xa7,
    SCSI_SET_STREAMING                  = 0xb6,
    SCSI_START_STOP_UNIT                = 0x1b,
    SCSI_STOP_PLAY_SCAN                 = 0x4e,
    /** Synchronize Cache command. */
    SCSI_SYNCHRONIZE_CACHE              = 0x35,
    SCSI_TEST_UNIT_READY                = 0x00,
    SCSI_VERIFY_10                      = 0x2f,
    /** Write(10) command. */
    SCSI_WRITE_10                       = 0x2a,
    SCSI_WRITE_12                       = 0xaa,
    SCSI_WRITE_AND_VERIFY_10            = 0x2e,
    SCSI_WRITE_BUFFER                   = 0x3b,

    /** Mode Sense(6) command */
    SCSI_MODE_SENSE_6                   = 0x1a,
    /** Report LUNs command. */
    SCSI_REPORT_LUNS                    = 0xa0,
    /** Rezero Unit command. Obsolete for ages now, but used by cdrecord. */
    SCSI_REZERO_UNIT                    = 0x01
} SCSICMD;

#ifdef DEBUG
static const char * const g_apszSCSICmdNames[256] =
{
    "TEST UNIT READY",                     /* 0x00 */
    "REZERO UNIT",                         /* 0x01 */
    "",                                    /* 0x02 */
    "REQUEST SENSE",                       /* 0x03 */
    "FORMAT UNIT",                         /* 0x04 */
    "READ BLOCK LIMITS",                   /* 0x05 */
    "",                                    /* 0x06 */
    "REASSIGN BLOCKS",                     /* 0x07 */
    "READ (6)",                            /* 0x08 */
    "",                                    /* 0x09 */
    "WRITE (6)",                           /* 0x0a */
    "SEEK (6)",                            /* 0x0b */
    "",                                    /* 0x0c */
    "",                                    /* 0x0d */
    "",                                    /* 0x0e */
    "READ REVERSE (6)",                    /* 0x0f */
    "READ FILEMARKS (6)",                  /* 0x10 */
    "SPACE (6)",                           /* 0x11 */
    "INQUIRY",                             /* 0x12 */
    "VERIFY (6)",                          /* 0x13 */
    "RECOVER BUFFERED DATA",               /* 0x14 */
    "MODE SELECT (6)",                     /* 0x15 */
    "RESERVE (6)",                         /* 0x16 */
    "RELEASE (6)",                         /* 0x17 */
    "COPY",                                /* 0x18 */
    "ERASE (6)",                           /* 0x19 */
    "MODE SENSE (6)",                      /* 0x1a */
    "START STOP UNIT",                     /* 0x1b */
    "RECEIVE DIAGNOSTIC RESULTS",          /* 0x1c */
    "SEND DIAGNOSTIC",                     /* 0x1d */
    "PREVENT ALLOW MEDIUM REMOVAL",        /* 0x1e */
    "",                                    /* 0x1f */
    "",                                    /* 0x20 */
    "",                                    /* 0x21 */
    "",                                    /* 0x22 */
    "READ FORMAT CAPACITIES",              /* 0x23 */
    "SET WINDOW",                          /* 0x24 */
    "READ CAPACITY",                       /* 0x25 */
    "",                                    /* 0x26 */
    "",                                    /* 0x27 */
    "READ (10)",                           /* 0x28 */
    "READ GENERATION",                     /* 0x29 */
    "WRITE (10)",                          /* 0x2a */
    "SEEK (10)",                           /* 0x2b */
    "ERASE (10)",                          /* 0x2c */
    "READ UPDATED BLOCK",                  /* 0x2d */
    "WRITE AND VERIFY (10)",               /* 0x2e */
    "VERIFY (10)",                         /* 0x2f */
    "SEARCH DATA HIGH (10)",               /* 0x30 */
    "SEARCH DATA EQUAL (10)",              /* 0x31 */
    "SEARCH DATA LOW (10)",                /* 0x32 */
    "SET LIMITS (10)",                     /* 0x33 */
    "PRE-FETCH (10)",                      /* 0x34 */
    "SYNCHRONIZE CACHE (10)",              /* 0x35 */
    "LOCK UNLOCK CACHE (10)",              /* 0x36 */
    "READ DEFECT DATA (10)",               /* 0x37 */
    "MEDIUM SCAN",                         /* 0x38 */
    "COMPARE",                             /* 0x39 */
    "COPY AND VERIFY",                     /* 0x3a */
    "WRITE BUFFER",                        /* 0x3b */
    "READ BUFFER",                         /* 0x3c */
    "UPDATE BLOCK",                        /* 0x3d */
    "READ LONG (10)",                      /* 0x3e */
    "WRITE LONG (10)",                     /* 0x3f */
    "CHANGE DEFINITION",                   /* 0x40 */
    "WRITE SAME (10)",                     /* 0x41 */
    "READ SUBCHANNEL",                     /* 0x42 */
    "READ TOC/PMA/ATIP",                   /* 0x43 */
    "REPORT DENSITY SUPPORT",              /* 0x44 */
    "PLAY AUDIO (10)",                     /* 0x45 */
    "GET CONFIGURATION",                   /* 0x46 */
    "PLAY AUDIO MSF",                      /* 0x47 */
    "",                                    /* 0x48 */
    "",                                    /* 0x49 */
    "GET EVENT STATUS NOTIFICATION",       /* 0x4a */
    "PAUSE/RESUME",                        /* 0x4b */
    "LOG SELECT",                          /* 0x4c */
    "LOG SENSE",                           /* 0x4d */
    "STOP PLAY/SCAN",                      /* 0x4e */
    "",                                    /* 0x4f */
    "XDWRITE (10)",                        /* 0x50 */
    "READ DISC INFORMATION",               /* 0x51 */
    "READ TRACK INFORMATION",              /* 0x52 */
    "RESERVE TRACK",                       /* 0x53 */
    "SEND OPC INFORMATION",                /* 0x54 */
    "MODE SELECT (10)",                    /* 0x55 */
    "RESERVE (10)",                        /* 0x56 */
    "RELEASE (10)",                        /* 0x57 */
    "REPAIR TRACK",                        /* 0x58 */
    "",                                    /* 0x59 */
    "MODE SENSE (10)",                     /* 0x5a */
    "CLOSE TRACK/SESSION",                 /* 0x5b */
    "READ BUFFER CAPACITY",                /* 0x5c */
    "SEND CUE SHEET",                      /* 0x5d */
    "PERSISTENT RESERVE IN",               /* 0x5e */
    "PERSISTENT RESERVE OUT",              /* 0x5f */
    "",                                    /* 0x60 */
    "",                                    /* 0x61 */
    "",                                    /* 0x62 */
    "",                                    /* 0x63 */
    "",                                    /* 0x64 */
    "",                                    /* 0x65 */
    "",                                    /* 0x66 */
    "",                                    /* 0x67 */
    "",                                    /* 0x68 */
    "",                                    /* 0x69 */
    "",                                    /* 0x6a */
    "",                                    /* 0x6b */
    "",                                    /* 0x6c */
    "",                                    /* 0x6d */
    "",                                    /* 0x6e */
    "",                                    /* 0x6f */
    "",                                    /* 0x70 */
    "",                                    /* 0x71 */
    "",                                    /* 0x72 */
    "",                                    /* 0x73 */
    "",                                    /* 0x74 */
    "",                                    /* 0x75 */
    "",                                    /* 0x76 */
    "",                                    /* 0x77 */
    "",                                    /* 0x78 */
    "",                                    /* 0x79 */
    "",                                    /* 0x7a */
    "",                                    /* 0x7b */
    "",                                    /* 0x7c */
    "",                                    /* 0x7d */
    "",                                    /* 0x7e */
    "",                                    /* 0x7f */
    "WRITE FILEMARKS (16)",                /* 0x80 */
    "READ REVERSE (16)",                   /* 0x81 */
    "REGENERATE (16)",                     /* 0x82 */
    "EXTENDED COPY",                       /* 0x83 */
    "RECEIVE COPY RESULTS",                /* 0x84 */
    "ATA COMMAND PASS THROUGH (16)",       /* 0x85 */
    "ACCESS CONTROL IN",                   /* 0x86 */
    "ACCESS CONTROL OUT",                  /* 0x87 */
    "READ (16)",                           /* 0x88 */
    "",                                    /* 0x89 */
    "WRITE(16)",                           /* 0x8a */
    "",                                    /* 0x8b */
    "READ ATTRIBUTE",                      /* 0x8c */
    "WRITE ATTRIBUTE",                     /* 0x8d */
    "WRITE AND VERIFY (16)",               /* 0x8e */
    "VERIFY (16)",                         /* 0x8f */
    "PRE-FETCH (16)",                      /* 0x90 */
    "SYNCHRONIZE CACHE (16)",              /* 0x91 */
    "LOCK UNLOCK CACHE (16)",              /* 0x92 */
    "WRITE SAME (16)",                     /* 0x93 */
    "",                                    /* 0x94 */
    "",                                    /* 0x95 */
    "",                                    /* 0x96 */
    "",                                    /* 0x97 */
    "",                                    /* 0x98 */
    "",                                    /* 0x99 */
    "",                                    /* 0x9a */
    "",                                    /* 0x9b */
    "",                                    /* 0x9c */
    "",                                    /* 0x9d */
    "SERVICE ACTION IN (16)",              /* 0x9e */
    "SERVICE ACTION OUT (16)",             /* 0x9f */
    "REPORT LUNS",                         /* 0xa0 */
    "BLANK",                               /* 0xa1 */
    "SEND EVENT",                          /* 0xa2 */
    "SEND KEY",                            /* 0xa3 */
    "REPORT KEY",                          /* 0xa4 */
    "PLAY AUDIO (12)",                     /* 0xa5 */
    "LOAD/UNLOAD MEDIUM",                  /* 0xa6 */
    "SET READ AHEAD",                      /* 0xa7 */
    "READ (12)",                           /* 0xa8 */
    "SERVICE ACTION OUT (12)",             /* 0xa9 */
    "WRITE (12)",                          /* 0xaa */
    "SERVICE ACTION IN (12)",              /* 0xab */
    "GET PERFORMANCE",                     /* 0xac */
    "READ DVD STRUCTURE",                  /* 0xad */
    "WRITE AND VERIFY (12)",               /* 0xae */
    "VERIFY (12)",                         /* 0xaf */
    "SEARCH DATA HIGH (12)",               /* 0xb0 */
    "SEARCH DATA EQUAL (12)",              /* 0xb1 */
    "SEARCH DATA LOW (12)",                /* 0xb2 */
    "SET LIMITS (12)",                     /* 0xb3 */
    "READ ELEMENT STATUS ATTACHED",        /* 0xb4 */
    "REQUEST VOLUME ELEMENT ADDRESS",      /* 0xb5 */
    "SET STREAMING",                       /* 0xb6 */
    "READ DEFECT DATA (12)",               /* 0xb7 */
    "READ ELEMENT STATUS",                 /* 0xb8 */
    "READ CD MSF",                         /* 0xb9 */
    "SCAN",                                /* 0xba */
    "SET CD SPEED",                        /* 0xbb */
    "SPARE (IN)",                          /* 0xbc */
    "MECHANISM STATUS",                    /* 0xbd */
    "READ CD",                             /* 0xbe */
    "SEND DVD STRUCTURE",                  /* 0xbf */
    "",                                    /* 0xc0 */
    "",                                    /* 0xc1 */
    "",                                    /* 0xc2 */
    "",                                    /* 0xc3 */
    "",                                    /* 0xc4 */
    "",                                    /* 0xc5 */
    "",                                    /* 0xc6 */
    "",                                    /* 0xc7 */
    "",                                    /* 0xc8 */
    "",                                    /* 0xc9 */
    "",                                    /* 0xca */
    "",                                    /* 0xcb */
    "",                                    /* 0xcc */
    "",                                    /* 0xcd */
    "",                                    /* 0xce */
    "",                                    /* 0xcf */
    "",                                    /* 0xd0 */
    "",                                    /* 0xd1 */
    "",                                    /* 0xd2 */
    "",                                    /* 0xd3 */
    "",                                    /* 0xd4 */
    "",                                    /* 0xd5 */
    "",                                    /* 0xd6 */
    "",                                    /* 0xd7 */
    "",                                    /* 0xd8 */
    "",                                    /* 0xd9 */
    "",                                    /* 0xda */
    "",                                    /* 0xdb */
    "",                                    /* 0xdc */
    "",                                    /* 0xdd */
    "",                                    /* 0xde */
    "",                                    /* 0xdf */
    "",                                    /* 0xe0 */
    "",                                    /* 0xe1 */
    "",                                    /* 0xe2 */
    "",                                    /* 0xe3 */
    "",                                    /* 0xe4 */
    "",                                    /* 0xe5 */
    "",                                    /* 0xe6 */
    "",                                    /* 0xe7 */
    "",                                    /* 0xe8 */
    "",                                    /* 0xe9 */
    "",                                    /* 0xea */
    "",                                    /* 0xeb */
    "",                                    /* 0xec */
    "",                                    /* 0xed */
    "",                                    /* 0xee */
    "",                                    /* 0xef */
    "",                                    /* 0xf0 */
    "",                                    /* 0xf1 */
    "",                                    /* 0xf2 */
    "",                                    /* 0xf3 */
    "",                                    /* 0xf4 */
    "",                                    /* 0xf5 */
    "",                                    /* 0xf6 */
    "",                                    /* 0xf7 */
    "",                                    /* 0xf8 */
    "",                                    /* 0xf9 */
    "",                                    /* 0xfa */
    "",                                    /* 0xfb */
    "",                                    /* 0xfc */
    "",                                    /* 0xfd */
    "",                                    /* 0xfe */
    ""                                     /* 0xff */
};
#endif /* DEBUG */

/* Mode page codes for mode sense/select commands. */
#define SCSI_MODEPAGE_ERROR_RECOVERY   0x01
#define SCSI_MODEPAGE_WRITE_PARAMETER  0x05
#define SCSI_MODEPAGE_CD_STATUS        0x2a


/* Page control codes. */
#define SCSI_PAGECONTROL_CURRENT        0x00
#define SCSI_PAGECONTROL_CHANGEABLE     0x01
#define SCSI_PAGECONTROL_DEFAULT        0x02
#define SCSI_PAGECONTROL_SAVED          0x03


#define SCSI_SENSE_NONE             0
#define SCSI_SENSE_NOT_READY        2
#define SCSI_SENSE_MEDIUM_ERROR     3
#define SCSI_SENSE_ILLEGAL_REQUEST  5
#define SCSI_SENSE_UNIT_ATTENTION   6


#define SCSI_ASC_NONE                               0x00
#define SCSI_ASC_READ_ERROR                         0x11
#define SCSI_ASC_ILLEGAL_OPCODE                     0x20
#define SCSI_ASC_LOGICAL_BLOCK_OOR                  0x21
#define SCSI_ASC_INV_FIELD_IN_CMD_PACKET            0x24
#define SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED            0x28
#define SCSI_ASC_MEDIUM_NOT_PRESENT                 0x3a
#define SCSI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED    0x39
#define SCSI_ASC_MEDIA_LOAD_OR_EJECT_FAILED         0x53


/** @name SCSI_INQUIRY
 * @{
 */
#pragma pack(1)
typedef struct SCSIINQUIRYCDB
{
    unsigned u8Cmd : 8;
    unsigned fEVPD : 1;
    unsigned u4Reserved : 4;
    unsigned u3LUN : 3;
    unsigned u8PageCode : 8;
    unsigned u8Reserved : 8;
    uint8_t cbAlloc;
    uint8_t u8Control;
} SCSIINQUIRYCDB;
#pragma pack()
AssertCompileSize(SCSIINQUIRYCDB, 6);
typedef SCSIINQUIRYCDB *PSCSIINQUIRYCDB;
typedef const SCSIINQUIRYCDB *PCSCSIINQUIRYCDB;

#pragma pack(1)
typedef struct SCSIINQUIRYDATA
{
    unsigned u5PeripherialDeviceType : 5;   /**< 0x00 / 00 */
    unsigned u3PeripherialQualifier : 3;
    unsigned u6DeviceTypeModifier : 7;      /**< 0x01 */
    unsigned fRMB : 1;
    unsigned u3AnsiVersion : 3;             /**< 0x02 */
    unsigned u3EcmaVersion : 3;
    unsigned u2IsoVersion : 2;
    unsigned u4ResponseDataFormat : 4;      /**< 0x03 */
    unsigned u2Reserved0 : 2;
    unsigned fTrmlOP : 1;
    unsigned fAEC : 1;
    unsigned cbAdditional : 8;              /**< 0x04 */
    unsigned u8Reserved1 : 8;               /**< 0x05 */
    unsigned u8Reserved2 : 8;               /**< 0x06 */
    unsigned fSftRe : 1;                    /**< 0x07 */
    unsigned fCmdQue : 1;
    unsigned fReserved3 : 1;
    unsigned fLinked : 1;
    unsigned fSync : 1;
    unsigned fWBus16 : 1;
    unsigned fWBus32 : 1;
    unsigned fRelAdr : 1;
    int8_t achVendorId[8];                  /**< 0x08 */
    int8_t achProductId[16];                /**< 0x10 */
    int8_t achProductLevel[4];              /**< 0x20 */
    uint8_t abVendorSpecific[20];           /**< 0x24/36 - Optional it seems. */
    uint8_t abReserved4[40];
    uint8_t abVendorSpecificParameters[1];  /**< 0x60/96 - Variable size. */
} SCSIINQUIRYDATA;
#pragma pack()
AssertCompileSize(SCSIINQUIRYDATA, 97);
typedef SCSIINQUIRYDATA *PSCSIINQUIRYDATA;
typedef const SCSIINQUIRYDATA *PCSCSIINQUIRYDATA;
/** @} */

#endif
