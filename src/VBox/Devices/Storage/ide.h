/** @file
 *
 * VBox storage devices:
 * ATA/ATAPI declarations
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
 */

#ifndef __IDE_h__
#define __IDE_h__


/* Bits of HD_STATUS */
#define ATA_STAT_ERR                0x01
#define ATA_STAT_INDEX              0x02
#define ATA_STAT_ECC                0x04    /* Corrected error */
#define ATA_STAT_DRQ                0x08
#define ATA_STAT_SEEK               0x10
#define ATA_STAT_SRV                0x10
#define ATA_STAT_WRERR              0x20
#define ATA_STAT_READY              0x40
#define ATA_STAT_BUSY               0x80

/* Bits for HD_ERROR */
#define MARK_ERR                0x01    /* Bad address mark */
#define TRK0_ERR                0x02    /* couldn't find track 0 */
#define ABRT_ERR                0x04    /* Command aborted */
#define MCR_ERR                 0x08    /* media change request */
#define ID_ERR                  0x10    /* ID field not found */
#define MC_ERR                  0x20    /* media changed */
#define ECC_ERR                 0x40    /* Uncorrectable ECC error */
#define BBD_ERR                 0x80    /* pre-EIDE meaning:  block marked bad */
#define ICRC_ERR                0x80    /* new meaning:  CRC error during transfer */

/* Bits for uATARegDevCtl. */
#define ATA_DEVCTL_DISABLE_IRQ  0x02
#define ATA_DEVCTL_RESET        0x04
#define ATA_DEVCTL_HOB          0x80


/* ATA/ATAPI Commands (as of ATA/ATAPI-8 draft T13/1699D Revision 3a).
 * Please keep this in sync with g_apszATACmdNames. */
typedef enum ATACMD
{
    ATA_NOP                                 = 0x00,
    ATA_CFA_REQUEST_EXTENDED_ERROR_CODE     = 0x03,
    ATA_DEVICE_RESET                        = 0x08,
    ATA_RECALIBRATE                         = 0x10,
    ATA_READ_SECTORS                        = 0x20,
    ATA_READ_SECTORS_WITHOUT_RETRIES        = 0x21,
    ATA_READ_LONG                           = 0x22,
    ATA_READ_LONG_WITHOUT_RETRIES           = 0x23,
    ATA_READ_SECTORS_EXT                    = 0x24,
    ATA_READ_DMA_EXT                        = 0x25,
    ATA_READ_DMA_QUEUED_EXT                 = 0x26,
    ATA_READ_NATIVE_MAX_ADDRESS_EXT         = 0x27,
    ATA_READ_MULTIPLE_EXT                   = 0x29,
    ATA_READ_STREAM_DMA_EXT                 = 0x2a,
    ATA_READ_STREAM_EXT                     = 0x2b,
    ATA_READ_LOG_EXT                        = 0x2f,
    ATA_WRITE_SECTORS                       = 0x30,
    ATA_WRITE_SECTORS_WITHOUT_RETRIES       = 0x31,
    ATA_WRITE_LONG                          = 0x32,
    ATA_WRITE_LONG_WITHOUT_RETRIES          = 0x33,
    ATA_WRITE_SECTORS_EXT                   = 0x34,
    ATA_WRITE_DMA_EXT                       = 0x35,
    ATA_WRITE_DMA_QUEUED_EXT                = 0x36,
    ATA_SET_MAX_ADDRESS_EXT                 = 0x37,
    ATA_CFA_WRITE_SECTORS_WITHOUT_ERASE     = 0x38,
    ATA_WRITE_MULTIPLE_EXT                  = 0x39,
    ATA_WRITE_STREAM_DMA_EXT                = 0x3a,
    ATA_WRITE_STREAM_EXT                    = 0x3b,
    ATA_WRITE_VERIFY                        = 0x3c,
    ATA_WRITE_DMA_FUA_EXT                   = 0x3d,
    ATA_WRITE_DMA_QUEUED_FUA_EXT            = 0x3e,
    ATA_WRITE_LOG_EXT                       = 0x3f,
    ATA_READ_VERIFY_SECTORS                 = 0x40,
    ATA_READ_VERIFY_SECTORS_WITHOUT_RETRIES = 0x41,
    ATA_READ_VERIFY_SECTORS_EXT             = 0x42,
    ATA_WRITE_UNCORRECTABLE_EXT             = 0x45,
    ATA_READ_LOG_DMA_EXT                    = 0x47,
    ATA_FORMAT_TRACK                        = 0x50,
    ATA_CONFIGURE_STREAM                    = 0x51,
    ATA_WRITE_LOG_DMA_EXT                   = 0x57,
    ATA_TRUSTED_RECEIVE                     = 0x5c,
    ATA_TRUSTED_RECEIVE_DMA                 = 0x5d,
    ATA_TRUSTED_SEND                        = 0x5e,
    ATA_TRUSTED_SEND_DMA                    = 0x5f,
    ATA_SEEK                                = 0x70,
    ATA_CFA_TRANSLATE_SECTOR                = 0x87,
    ATA_EXECUTE_DEVICE_DIAGNOSTIC           = 0x90,
    ATA_INITIALIZE_DEVICE_PARAMETERS        = 0x91,
    ATA_DOWNLOAD_MICROCODE                  = 0x92,
    ATA_STANDBY_IMMEDIATE__ALT              = 0x94,
    ATA_IDLE_IMMEDIATE__ALT                 = 0x95,
    ATA_STANDBY__ALT                        = 0x96,
    ATA_IDLE__ALT                           = 0x97,
    ATA_CHECK_POWER_MODE__ALT               = 0x98,
    ATA_SLEEP__ALT                          = 0x99,
    ATA_PACKET                              = 0xa0,
    ATA_IDENTIFY_PACKET_DEVICE              = 0xa1,
    ATA_SERVICE                             = 0xa2,
    ATA_SMART                               = 0xb0,
    ATA_DEVICE_CONFIGURATION_OVERLAY        = 0xb1,
    ATA_NV_CACHE                            = 0xb6,
    ATA_CFA_ERASE_SECTORS                   = 0xc0,
    ATA_READ_MULTIPLE                       = 0xc4,
    ATA_WRITE_MULTIPLE                      = 0xc5,
    ATA_SET_MULTIPLE_MODE                   = 0xc6,
    ATA_READ_DMA_QUEUED                     = 0xc7,
    ATA_READ_DMA                            = 0xc8,
    ATA_READ_DMA_WITHOUT_RETRIES            = 0xc9,
    ATA_WRITE_DMA                           = 0xca,
    ATA_WRITE_DMA_WITHOUT_RETRIES           = 0xcb,
    ATA_WRITE_DMA_QUEUED                    = 0xcc,
    ATA_CFA_WRITE_MULTIPLE_WITHOUT_ERASE    = 0xcd,
    ATA_WRITE_MULTIPLE_FUA_EXT              = 0xce,
    ATA_CHECK_MEDIA_CARD_TYPE               = 0xd1,
    ATA_GET_MEDIA_STATUS                    = 0xda,
    ATA_ACKNOWLEDGE_MEDIA_CHANGE            = 0xdb,
    ATA_BOOT_POST_BOOT                      = 0xdc,
    ATA_BOOT_PRE_BOOT                       = 0xdd,
    ATA_MEDIA_LOCK                          = 0xde,
    ATA_MEDIA_UNLOCK                        = 0xdf,
    ATA_STANDBY_IMMEDIATE                   = 0xe0,
    ATA_IDLE_IMMEDIATE                      = 0xe1,
    ATA_STANDBY                             = 0xe2,
    ATA_IDLE                                = 0xe3,
    ATA_READ_BUFFER                         = 0xe4,
    ATA_CHECK_POWER_MODE                    = 0xe5,
    ATA_SLEEP                               = 0xe6,
    ATA_FLUSH_CACHE                         = 0xe7,
    ATA_WRITE_BUFFER                        = 0xe8,
    ATA_WRITE_SAME                          = 0xe9,
    ATA_FLUSH_CACHE_EXT                     = 0xea,
    ATA_IDENTIFY_DEVICE                     = 0xec,
    ATA_MEDIA_EJECT                         = 0xed,
    ATA_IDENTIFY_DMA                        = 0xee,
    ATA_SET_FEATURES                        = 0xef,
    ATA_SECURITY_SET_PASSWORD               = 0xf1,
    ATA_SECURITY_UNLOCK                     = 0xf2,
    ATA_SECURITY_ERASE_PREPARE              = 0xf3,
    ATA_SECURITY_ERASE_UNIT                 = 0xf4,
    ATA_SECURITY_FREEZE_LOCK                = 0xf5,
    ATA_SECURITY_DISABLE_PASSWORD           = 0xf6,
    ATA_READ_NATIVE_MAX_ADDRESS             = 0xf8,
    ATA_SET_MAX                             = 0xf9
} ATACMD;

#ifdef DEBUG
static const char * const g_apszATACmdNames[256] =
{
    "NOP",                                 /* 0x00 */
    "",                                    /* 0x01 */
    "",                                    /* 0x02 */
    "CFA REQUEST EXTENDED ERROR CODE",     /* 0x03 */
    "",                                    /* 0x04 */
    "",                                    /* 0x05 */
    "",                                    /* 0x06 */
    "",                                    /* 0x07 */
    "DEVICE RESET",                        /* 0x08 */
    "",                                    /* 0x09 */
    "",                                    /* 0x0a */
    "",                                    /* 0x0b */
    "",                                    /* 0x0c */
    "",                                    /* 0x0d */
    "",                                    /* 0x0e */
    "",                                    /* 0x0f */
    "RECALIBRATE",                         /* 0x10 */
    "",                                    /* 0x11 */
    "",                                    /* 0x12 */
    "",                                    /* 0x13 */
    "",                                    /* 0x14 */
    "",                                    /* 0x15 */
    "",                                    /* 0x16 */
    "",                                    /* 0x17 */
    "",                                    /* 0x18 */
    "",                                    /* 0x19 */
    "",                                    /* 0x1a */
    "",                                    /* 0x1b */
    "",                                    /* 0x1c */
    "",                                    /* 0x1d */
    "",                                    /* 0x1e */
    "",                                    /* 0x1f */
    "READ SECTORS",                        /* 0x20 */
    "READ SECTORS WITHOUT RETRIES",        /* 0x21 */
    "READ LONG",                           /* 0x22 */
    "READ LONG WITHOUT RETRIES",           /* 0x23 */
    "READ SECTORS EXT",                    /* 0x24 */
    "READ DMA EXT",                        /* 0x25 */
    "READ DMA QUEUED EXT",                 /* 0x26 */
    "READ NATIVE MAX ADDRESS EXT",         /* 0x27 */
    "",                                    /* 0x28 */
    "READ MULTIPLE EXT",                   /* 0x29 */
    "READ STREAM DMA EXT",                 /* 0x2a */
    "READ STREAM EXT",                     /* 0x2b */
    "",                                    /* 0x2c */
    "",                                    /* 0x2d */
    "",                                    /* 0x2e */
    "READ LOG EXT",                        /* 0x2f */
    "WRITE SECTORS",                       /* 0x30 */
    "WRITE SECTORS WITHOUT RETRIES",       /* 0x31 */
    "WRITE LONG",                          /* 0x32 */
    "WRITE LONG WITHOUT RETRIES",          /* 0x33 */
    "WRITE SECTORS EXT",                   /* 0x34 */
    "WRITE DMA EXT",                       /* 0x35 */
    "WRITE DMA QUEUED EXT",                /* 0x36 */
    "SET MAX ADDRESS EXT",                 /* 0x37 */
    "CFA WRITE SECTORS WITHOUT ERASE",     /* 0x38 */
    "WRITE MULTIPLE EXT",                  /* 0x39 */
    "WRITE STREAM DMA EXT",                /* 0x3a */
    "WRITE STREAM EXT",                    /* 0x3b */
    "WRITE VERIFY",                        /* 0x3c */
    "WRITE DMA FUA EXT",                   /* 0x3d */
    "WRITE DMA QUEUED FUA EXT",            /* 0x3e */
    "WRITE LOG EXT",                       /* 0x3f */
    "READ VERIFY SECTORS",                 /* 0x40 */
    "READ VERIFY SECTORS WITHOUT RETRIES", /* 0x41 */
    "READ VERIFY SECTORS EXT",             /* 0x42 */
    "",                                    /* 0x43 */
    "",                                    /* 0x44 */
    "WRITE UNCORRECTABLE EXT",             /* 0x45 */
    "",                                    /* 0x46 */
    "READ LOG DMA EXT",                    /* 0x47 */
    "",                                    /* 0x48 */
    "",                                    /* 0x49 */
    "",                                    /* 0x4a */
    "",                                    /* 0x4b */
    "",                                    /* 0x4c */
    "",                                    /* 0x4d */
    "",                                    /* 0x4e */
    "",                                    /* 0x4f */
    "FORMAT TRACK",                        /* 0x50 */
    "CONFIGURE STREAM",                    /* 0x51 */
    "",                                    /* 0x52 */
    "",                                    /* 0x53 */
    "",                                    /* 0x54 */
    "",                                    /* 0x55 */
    "",                                    /* 0x56 */
    "WRITE LOG DMA EXT",                   /* 0x57 */
    "",                                    /* 0x58 */
    "",                                    /* 0x59 */
    "",                                    /* 0x5a */
    "",                                    /* 0x5b */
    "TRUSTED RECEIVE",                     /* 0x5c */
    "TRUSTED RECEIVE DMA",                 /* 0x5d */
    "TRUSTED SEND",                        /* 0x5e */
    "TRUSTED SEND DMA",                    /* 0x5f */
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
    "SEEK",                                /* 0x70 */
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
    "",                                    /* 0x80 */
    "",                                    /* 0x81 */
    "",                                    /* 0x82 */
    "",                                    /* 0x83 */
    "",                                    /* 0x84 */
    "",                                    /* 0x85 */
    "",                                    /* 0x86 */
    "CFA TRANSLATE SECTOR",                /* 0x87 */
    "",                                    /* 0x88 */
    "",                                    /* 0x89 */
    "",                                    /* 0x8a */
    "",                                    /* 0x8b */
    "",                                    /* 0x8c */
    "",                                    /* 0x8d */
    "",                                    /* 0x8e */
    "",                                    /* 0x8f */
    "EXECUTE DEVICE DIAGNOSTIC",           /* 0x90 */
    "INITIALIZE DEVICE PARAMETERS",        /* 0x91 */
    "DOWNLOAD MICROCODE",                  /* 0x92 */
    "",                                    /* 0x93 */
    "STANDBY IMMEDIATE  ALT",              /* 0x94 */
    "IDLE IMMEDIATE  ALT",                 /* 0x95 */
    "STANDBY  ALT",                        /* 0x96 */
    "IDLE  ALT",                           /* 0x97 */
    "CHECK POWER MODE  ALT",               /* 0x98 */
    "SLEEP  ALT",                          /* 0x99 */
    "",                                    /* 0x9a */
    "",                                    /* 0x9b */
    "",                                    /* 0x9c */
    "",                                    /* 0x9d */
    "",                                    /* 0x9e */
    "",                                    /* 0x9f */
    "PACKET",                              /* 0xa0 */
    "IDENTIFY PACKET DEVICE",              /* 0xa1 */
    "SERVICE",                             /* 0xa2 */
    "",                                    /* 0xa3 */
    "",                                    /* 0xa4 */
    "",                                    /* 0xa5 */
    "",                                    /* 0xa6 */
    "",                                    /* 0xa7 */
    "",                                    /* 0xa8 */
    "",                                    /* 0xa9 */
    "",                                    /* 0xaa */
    "",                                    /* 0xab */
    "",                                    /* 0xac */
    "",                                    /* 0xad */
    "",                                    /* 0xae */
    "",                                    /* 0xaf */
    "SMART",                               /* 0xb0 */
    "DEVICE CONFIGURATION OVERLAY",        /* 0xb1 */
    "",                                    /* 0xb2 */
    "",                                    /* 0xb3 */
    "",                                    /* 0xb4 */
    "",                                    /* 0xb5 */
    "NV CACHE",                            /* 0xb6 */
    "",                                    /* 0xb7 */
    "",                                    /* 0xb8 */
    "",                                    /* 0xb9 */
    "",                                    /* 0xba */
    "",                                    /* 0xbb */
    "",                                    /* 0xbc */
    "",                                    /* 0xbd */
    "",                                    /* 0xbe */
    "",                                    /* 0xbf */
    "CFA ERASE SECTORS",                   /* 0xc0 */
    "",                                    /* 0xc1 */
    "",                                    /* 0xc2 */
    "",                                    /* 0xc3 */
    "READ MULTIPLE",                       /* 0xc4 */
    "WRITE MULTIPLE",                      /* 0xc5 */
    "SET MULTIPLE MODE",                   /* 0xc6 */
    "READ DMA QUEUED",                     /* 0xc7 */
    "READ DMA",                            /* 0xc8 */
    "READ DMA WITHOUT RETRIES",            /* 0xc9 */
    "WRITE DMA",                           /* 0xca */
    "WRITE DMA WITHOUT RETRIES",           /* 0xcb */
    "WRITE DMA QUEUED",                    /* 0xcc */
    "CFA WRITE MULTIPLE WITHOUT ERASE",    /* 0xcd */
    "WRITE MULTIPLE FUA EXT",              /* 0xce */
    "",                                    /* 0xcf */
    "",                                    /* 0xd0 */
    "CHECK MEDIA CARD TYPE",               /* 0xd1 */
    "",                                    /* 0xd2 */
    "",                                    /* 0xd3 */
    "",                                    /* 0xd4 */
    "",                                    /* 0xd5 */
    "",                                    /* 0xd6 */
    "",                                    /* 0xd7 */
    "",                                    /* 0xd8 */
    "",                                    /* 0xd9 */
    "GET MEDIA STATUS",                    /* 0xda */
    "ACKNOWLEDGE MEDIA CHANGE",            /* 0xdb */
    "BOOT POST BOOT",                      /* 0xdc */
    "BOOT PRE BOOT",                       /* 0xdd */
    "MEDIA LOCK",                          /* 0xde */
    "MEDIA UNLOCK",                        /* 0xdf */
    "STANDBY IMMEDIATE",                   /* 0xe0 */
    "IDLE IMMEDIATE",                      /* 0xe1 */
    "STANDBY",                             /* 0xe2 */
    "IDLE",                                /* 0xe3 */
    "READ BUFFER",                         /* 0xe4 */
    "CHECK POWER MODE",                    /* 0xe5 */
    "SLEEP",                               /* 0xe6 */
    "FLUSH CACHE",                         /* 0xe7 */
    "WRITE BUFFER",                        /* 0xe8 */
    "WRITE SAME",                          /* 0xe9 */
    "FLUSH CACHE EXT",                     /* 0xea */
    "",                                    /* 0xeb */
    "IDENTIFY DEVICE",                     /* 0xec */
    "MEDIA EJECT",                         /* 0xed */
    "IDENTIFY DMA",                        /* 0xee */
    "SET FEATURES",                        /* 0xef */
    "",                                    /* 0xf0 */
    "SECURITY SET PASSWORD",               /* 0xf1 */
    "SECURITY UNLOCK",                     /* 0xf2 */
    "SECURITY ERASE PREPARE",              /* 0xf3 */
    "SECURITY ERASE UNIT",                 /* 0xf4 */
    "SECURITY FREEZE LOCK",                /* 0xf5 */
    "SECURITY DISABLE PASSWORD",           /* 0xf6 */
    "",                                    /* 0xf7 */
    "READ NATIVE MAX ADDRESS",             /* 0xf8 */
    "SET MAX",                             /* 0xf9 */
    "",                                    /* 0xfa */
    "",                                    /* 0xfb */
    "",                                    /* 0xfc */
    "",                                    /* 0xfd */
    "",                                    /* 0xfe */
    ""                                     /* 0xff */
};
#endif /* DEBUG */


#define ATA_MODE_MDMA   0x20
#define ATA_MODE_UDMA   0x40


#define ATA_TRANSFER_ID(thismode, maxspeed, currmode) \
    (   ((1 << (maxspeed + 1)) - 1) \
     |  ((((thismode ^ currmode) & 0xf8) == 0) ? 1 << ((currmode & 0x07) + 8) : 0))


/* ATAPI defines */

#define ATAPI_PACKET_SIZE 12


#define ATAPI_INT_REASON_CD             0x01 /* 0 = data transfer */
#define ATAPI_INT_REASON_IO             0x02 /* 1 = transfer to the host */
#define ATAPI_INT_REASON_REL            0x04
#define ATAPI_INT_REASON_TAG_MASK       0xf8


#endif /* __IDE_h__ */
