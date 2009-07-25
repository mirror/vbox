/* $Id$ */
/** @file
 * SSM - Saved State Manager.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


/** @page pg_ssm        SSM - The Saved State Manager
 *
 * The Saved State Manager (SSM) implements facilities for saving and loading a
 * VM state in a structural manner using callbacks for named data units.
 *
 * At init time each of the VMM components, Devices, Drivers and one or two
 * other things will register data units which they need to save and restore.
 * Each unit have a unique name (ascii), instance number, and a set of callbacks
 * associated with it.  The name will be used to identify the unit during
 * restore.  The callbacks are for the two operations, save and restore.  There
 * are three callbacks for each of the two - a prepare, a execute and a complete
 * - giving each component ample opportunity to perform actions both before and
 * afterwards.
 *
 * The SSM provides a number of APIs for encoding and decoding the data: @see
 * grp_ssm
 *
 *
 *
 * @section sec_ssm_live_snapshots  Live Snapshots
 *
 * The live snapshots feature (LS) is similar to live migration (LM) and was a
 * natural first step when implementing LM.  The main differences between LS and
 * LM are that after a live snapshot we will have a saved state file, disk image
 * snapshots, and the VM will still be running.
 *
 * Compared to normal saved stated and snapshots, the difference is in that the
 * VM is running while we do most of the saving.  Prior to LS, there was only
 * round of callback during saving, after LS there are 1 or more while the VM is
 * still running and a final one after it has been paused.  The runtime stages
 * is executed on a dedicated thread running at at the same priority as the EMTs
 * so that the saving doesn't starve or lose in scheduling questions.  The final
 * phase is done on EMT(0).
 *
 * There are a couple of common reasons why LS and LM will fail:
 *   - Memory configuration changed (PCI memory mappings).
 *   - Takes too long (LM) / Too much output (LS).
 *
 * FIGURE THIS: It is currently unclear who will resume the VM after it has been
 * paused.  The most efficient way to do this is by doing it before returning
 * from the VMR3Save call and use a callback for reconfiguring the disk images.
 * (It is more efficient because of fewer thread switches.) The more convenient
 * way is to have main do it after calling VMR3Save.
 *
 *
 * @section sec_ssm_live_migration  Live Migration
 *
 * As mentioned in the previous section, the main differences between this and
 * live snapshots are in where the saved state is written and what state the
 * local VM is in afterwards - at least from the VMM point of view.  The
 * necessary administrative work - establishing the connection to the remote
 * machine, cloning the VM config on it and doing lowlevel saved state data
 * transfer - is taken care of by layer above the VMM (i.e.  Main).
 *
 * The SSM data format was made streamable for the purpose of live migration
 * (v1.2 was the last non-streamable version).
 *
 *
 * @section sec_ssm_format          Saved State Format
 *
 * The stream format starts with a header (SSMFILEHDR) that indicates the
 * version and such things, it is followed by zero or more saved state units
 * (name + instance + phase), and the stream concludes with a footer
 * (SSMFILEFTR) that contains unit counts and optionally a checksum for the
 * entire file.  (In version 1.2 and earlier, the checksum was in the header and
 * there was no footer.  This meant that the header was updated after the entire
 * file was written.)
 *
 * The saved state units each starts with a variable sized header
 * (SSMFILEUNITHDR) that contains the name, instance and phase.  The data
 * follows the header and is encoded as records with a 2-8 byte record header
 * indicating the type, flags and size.  The first byte in the record header
 * indicates the type and flags:
 *
 *   - bits 0..3: Record type:
 *       - type 0: Invalid.
 *       - type 1: Terminator with CRC-32 and unit size.
 *       - type 2: Raw data record.
 *       - type 3: Named data - length prefixed name followed by the data.
 *       - types 4 thru 15 are current undefined.
 *   - bit 4: Important (set), can be skipped (clear).
 *   - bit 5: Undefined flag, must be zero.
 *   - bit 6: Undefined flag, must be zero.
 *   - bit 7: "magic" bit, always set.
 *
 * Record header byte 2 (optionally thru 7) is the size of the following data
 * encoded in UTF-8 style.
 *
 * The data part of the unit is compressed using LZF (via RTZip).
 *
 * (In version 1.2 and earlier the unit header also contained the compressed
 * size of the data, i.e.  it was updated after the data was written, and the
 * data was not record based.)
 *
 *
 * @section sec_ssm_future          Future Changes
 *
 * There are plans to extend SSM to make it easier to be both backwards and
 * (somewhat) forwards compatible.  One of the new features will be being able
 * to classify units and data items as unimportant (added to the format in
 * v2.0).  Another suggested feature is naming data items (also added to the
 * format in v2.0), perhaps by extending the SSMR3PutStruct API.  Both features
 * will require API changes, the naming may possibly require both buffering of
 * the stream as well as some helper managing them.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SSM
#include <VBox/ssm.h>
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include "SSMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/alloc.h>
#include <iprt/uuid.h>
#include <iprt/zip.h>
#include <iprt/crc32.h>
#include <iprt/thread.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The special value for the final phase.  */
#define SSM_PHASE_FINAL                         UINT32_MAX

/** The max length of a unit name. */
#define SSM_MAX_NAME_SIZE                       48

/** Saved state file magic base string. */
#define SSMFILEHDR_MAGIC_BASE                   "\177VirtualBox SavedState "
/** Saved state file magic indicating version 1.x. */
#define SSMFILEHDR_MAGIC_V1_X                   "\177VirtualBox SavedState V1."
/** Saved state file v1.1 magic. */
#define SSMFILEHDR_MAGIC_V1_1                   "\177VirtualBox SavedState V1.1\n"
/** Saved state file v1.2 magic. */
#define SSMFILEHDR_MAGIC_V1_2                   "\177VirtualBox SavedState V1.2\n\0\0\0"
/** Saved state file v2.0 magic. */
#define SSMFILEHDR_MAGIC_V2_0                   "\177VirtualBox SavedState V2.0\n\0\0\0"

/** @name SSMFILEHDR::fFlags
 * @{ */
/** The stream is checkesummed up to the footer using CRC-32. */
#define SSMFILEHDR_FLAGS_STREAM_CRC32           RT_BIT_32(0)
/** @} */

/** The directory magic. */
#define SSMFILEDIR_MAGIC                        "\nDir\n\0\0"

/** Saved state file v2.0 magic. */
#define SSMFILEFTR_MAGIC                        "\nFooter"

/** Data unit magic. */
#define SSMFILEUNITHDR_MAGIC                    "\nUnit\n\0"
/** Data end marker magic. */
#define SSMFILEUNITHDR_END                      "\nTheEnd"


/** @name Record Types (data unit)
 * @{ */
/** The record type mask. */
#define SSM_REC_TYPE_MASK                       UINT8_C(0x0f)
/** Invalid record. */
#define SSM_REC_TYPE_INVALID                    0
/** Normal termination record, see SSMRECTERM. */
#define SSM_REC_TYPE_TERM                       1
/** Raw data. The data follows the size field without further ado. */
#define SSM_REC_TYPE_RAW                        2
/** Named data items.
 * A length prefix zero terminated string (i.e. max 255) followed by the data.  */
#define SSM_REC_TYPE_NAMED                      3
/** Macro for validating the record type.
 * This can be used with the flags+type byte, no need to mask out the type first. */
#define SSM_REC_TYPE_IS_VALID(u8Type)           (   ((u8Type) & SSM_REC_TYPE_MASK) >  SSM_REC_TYPE_INVALID \
                                                 && ((u8Type) & SSM_REC_TYPE_MASK) <= SSM_REC_TYPE_NAMED )
/** @} */

/** The flag mask. */
#define SSM_REC_FLAGS_MASK                      UINT8_C(0xf0)
/** The record is important if this flag is set, if clear it can be omitted. */
#define SSM_REC_FLAGS_IMPORTANT                 UINT8_C(0x10)
/** This flag is always set. */
#define SSM_REC_FLAGS_FIXED                     UINT8_C(0x80)
/** Macro for validating the flags.
 * No need to mask the flags out of the flags+type byte before invoking this macro.  */
#define SSM_REC_FLAGS_ARE_VALID(fFlags)         ( ((fFlags) & UINT8_C(0xe0)) == UINT8_C(0x80) )

/** Macro for validating the type and flags byte in a data record. */
#define SSM_REC_ARE_TYPE_AND_FLAGS_VALID(u8)    ( SSM_REC_FLAGS_ARE_VALID(u8) && SSM_REC_TYPE_IS_VALID(u8) )

/** @name SSMRECTERM::fFlags
 * @{ */
/** There is CRC-32 for the data. */
#define SSMRECTERM_FLAGS_CRC32                  UINT16_C(0x0001)
/** @} */

/** Start structure magic. (Isacc Asimov) */
#define SSMR3STRUCT_BEGIN                       0x19200102
/** End structure magic. (Isacc Asimov) */
#define SSMR3STRUCT_END                         0x19920406


/** Number of bytes to log in Log2 and Log4 statements. */
#define SSM_LOG_BYTES                           16


/** Macro for checking the u32CRC field of a structure.
 * The Msg can assume there are  u32ActualCRC and u32CRC in the context. */
#define SSM_CHECK_CRC32_RET(p, cb, Msg) \
    do \
    { \
        uint32_t u32CRC = (p)->u32CRC; \
        (p)->u32CRC = 0; \
        uint32_t u32ActualCRC = RTCrc32((p), (cb)); \
        (p)->u32CRC = u32CRC; \
        AssertLogRelMsgReturn(u32ActualCRC == u32CRC, Msg, VERR_SSM_INTEGRITY_CRC); \
    } while (0)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** SSM state. */
typedef enum SSMSTATE
{
    SSMSTATE_INVALID = 0,
    SSMSTATE_SAVE_PREP,
    SSMSTATE_SAVE_EXEC,
    SSMSTATE_SAVE_DONE,
    SSMSTATE_LOAD_PREP,
    SSMSTATE_LOAD_EXEC,
    SSMSTATE_LOAD_DONE,
    SSMSTATE_OPEN_READ
} SSMSTATE;


/**
 * Handle structure.
 */
typedef struct SSMHANDLE
{
    /** The file handle. */
    RTFILE          hFile;
    /** The VM handle. */
    PVM             pVM;
    /** The current operation. */
    SSMSTATE        enmOp;
    /** What to do after save completes. (move the enum) */
    SSMAFTER        enmAfter;
    /** The current rc of the save operation. */
    int             rc;
    /** Number of compressed bytes left in the current data unit (V1). */
    uint64_t        cbUnitLeftV1;
    /** The current uncompressed offset into the data unit. */
    uint64_t        offUnit;
    /** Whether we're checksumming the unit or not. */
    bool            fUnitChecksummed;
    /** The unit CRC. */
    uint32_t        u32UnitCRC;

    /** Pointer to the progress callback function. */
    PFNVMPROGRESS   pfnProgress;
    /** User specified arguemnt to the callback function. */
    void           *pvUser;
    /** Next completion percentage. (corresponds to offEstProgress) */
    unsigned        uPercent;
    /** The position of the next progress callback in the estimated file. */
    uint64_t        offEstProgress;
    /** The estimated total byte count.
     * (Only valid after the prep.) */
    uint64_t        cbEstTotal;
    /** Current position in the estimated file. */
    uint64_t        offEst;
    /** End of current unit in the estimated file. */
    uint64_t        offEstUnitEnd;
    /** the amount of % we reserve for the 'prepare' phase */
    unsigned        uPercentPrepare;
    /** the amount of % we reserve for the 'done' stage */
    unsigned        uPercentDone;

    /** Whether we're checksumming reads/writes. */
    bool            fChecksummed;
    /** The stream CRC if fChecksummed is set. */
    uint32_t        u32StreamCRC;

    union
    {
        /** Write data. */
        struct
        {
            /** The compressor of the current data unit. */
            PRTZIPCOMP      pZipComp;
            /** Offset into the databuffer. */
            uint32_t        offDataBuffer;
            /** Space for the record header.  */
            uint8_t         abRecHdr[1+7];
            /** Data buffer. */
            uint8_t         abDataBuffer[4096];
        } Write;

        /** Read data. */
        struct
        {
            /** The decompressor of the current data unit. */
            PRTZIPDECOMP    pZipDecomp;
            /** The major format version number. */
            uint32_t        uFmtVerMajor;
            /** The minor format version number. */
            uint32_t        uFmtVerMinor;

            /** V2: Unread bytes in the current record. */
            uint32_t        cbRecLeft;
            /** V2: Bytes in the data buffer. */
            uint32_t        cbDataBuffer;
            /** V2: Current buffer position. */
            uint32_t        offDataBuffer;
            /** V2: End of data indicator. */
            bool            fEndOfData;
            /** V2: The type and flags byte fo the current record. */
            uint8_t         u8TypeAndFlags;

            /** RTGCPHYS size in bytes. (Only applicable when loading/reading.) */
            unsigned        cbGCPhys;
            /** RTGCPTR size in bytes. (Only applicable when loading/reading.) */
            unsigned        cbGCPtr;
            /** Whether cbGCPtr is fixed or settable. */
            bool            fFixedGCPtrSize;


            /** @name Header info (set by ssmR3ValidateFile)
             * @{ */
            /** The size of the file header. */
            size_t          cbFileHdr;
            /** The major version number. */
            uint16_t        u16VerMajor;
            /** The minor version number. */
            uint16_t        u16VerMinor;
            /** The build number. */
            uint32_t        u32VerBuild;
            /** The SVN revision. */
            uint32_t        u32SvnRev;
            /** 32 or 64 depending on the host. */
            uint8_t         cHostBits;
            /** The CRC of the loaded file. */
            uint32_t        u32LoadCRC;
            /** The size of the load file. */
            uint64_t        cbLoadFile;
            /** @} */

            /** V2: Data buffer. */
            uint8_t         abDataBuffer[4096];
        } Read;
    } u;
} SSMHANDLE;


/**
 * Header of the saved state file.
 *
 * Added in r5xxxx on 2009-07-2?, VirtualBox v3.0.51.
 */
typedef struct SSMFILEHDR
{
    /** Magic string which identifies this file as a version of VBox saved state
     *  file format (SSMFILEHDR_MAGIC_V2_0). */
    char            szMagic[32];
    /** The major version number. */
    uint16_t        u16VerMajor;
    /** The minor version number. */
    uint16_t        u16VerMinor;
    /** The build number. */
    uint32_t        u32VerBuild;
    /** The SVN revision. */
    uint32_t        u32SvnRev;
    /** 32 or 64 depending on the host. */
    uint8_t         cHostBits;
    /** The size of RTGCPHYS. */
    uint8_t         cbGCPhys;
    /** The size of RTGCPTR. */
    uint8_t         cbGCPtr;
    /** Reserved header space - must be zero. */
    uint8_t         u8Reserved;
    /** The number of units that (may) have stored data in the file. */
    uint32_t        cUnits;
    /** Flags, see SSMFILEHDR_FLAGS_XXX.  */
    uint32_t        fFlags;
    /** Reserved header space - must be zero. */
    uint32_t        u32Reserved;
    /** The checksum of this header.
     * This field is set to zero when calculating the checksum. */
    uint32_t        u32CRC;
} SSMFILEHDR;
AssertCompileSize(SSMFILEHDR, 64);
AssertCompileMemberOffset(SSMFILEHDR, u32CRC, 60);
AssertCompileMemberSize(SSMFILEHDR, szMagic, sizeof(SSMFILEHDR_MAGIC_V2_0));
/** Pointer to a saved state file header. */
typedef SSMFILEHDR *PSSMFILEHDR;
/** Pointer to a const saved state file header. */
typedef SSMFILEHDR const *PCSSMFILEHDR;


/**
 * Header of the saved state file.
 *
 * Added in r40980 on 2008-12-15, VirtualBox v2.0.51.
 *
 * @remarks This is a superset of SSMFILEHDRV11.
 */
typedef struct SSMFILEHDRV12
{
    /** Magic string which identifies this file as a version of VBox saved state
     *  file format (SSMFILEHDR_MAGIC_V1_2). */
    char            achMagic[32];
    /** The size of this file. Used to check
     * whether the save completed and that things are fine otherwise. */
    uint64_t        cbFile;
    /** File checksum. The actual calculation skips past the u32CRC field. */
    uint32_t        u32CRC;
    /** Padding. */
    uint32_t        u32Reserved;
    /** The machine UUID. (Ignored if NIL.) */
    RTUUID          MachineUuid;

    /** The major version number. */
    uint16_t        u16VerMajor;
    /** The minor version number. */
    uint16_t        u16VerMinor;
    /** The build number. */
    uint32_t        u32VerBuild;
    /** The SVN revision. */
    uint32_t        u32SvnRev;

    /** 32 or 64 depending on the host. */
    uint8_t         cHostBits;
    /** The size of RTGCPHYS. */
    uint8_t         cbGCPhys;
    /** The size of RTGCPTR. */
    uint8_t         cbGCPtr;
    /** Padding. */
    uint8_t         au8Reserved;
} SSMFILEHDRV12;
AssertCompileSize(SSMFILEHDRV12, 64+16);
AssertCompileMemberOffset(SSMFILEHDRV12, u32CRC, 40);
AssertCompileMemberSize(SSMFILEHDRV12, achMagic, sizeof(SSMFILEHDR_MAGIC_V1_2));
/** Pointer to a saved state file header. */
typedef SSMFILEHDRV12 *PSSMFILEHDRV12;


/**
 * Header of the saved state file, version 1.1.
 *
 * Added in r23677 on 2007-08-17, VirtualBox v1.4.1.
 */
typedef struct SSMFILEHDRV11
{
    /** Magic string which identifies this file as a version of VBox saved state
     *  file format (SSMFILEHDR_MAGIC_V1_1). */
    char            achMagic[32];
    /** The size of this file. Used to check
     * whether the save completed and that things are fine otherwise. */
    uint64_t        cbFile;
    /** File checksum. The actual calculation skips past the u32CRC field. */
    uint32_t        u32CRC;
    /** Padding. */
    uint32_t        u32Reserved;
    /** The machine UUID. (Ignored if NIL.) */
    RTUUID          MachineUuid;
} SSMFILEHDRV11;
AssertCompileSize(SSMFILEHDRV11, 64);
AssertCompileMemberOffset(SSMFILEHDRV11, u32CRC, 40);
/** Pointer to a saved state file header. */
typedef SSMFILEHDRV11 *PSSMFILEHDRV11;


/**
 * Data unit header.
 */
typedef struct SSMFILEUNITHDR
{
    /** Magic (SSMFILEUNITHDR_MAGIC or SSMFILEUNITHDR_END). */
    char            szMagic[8];
    /** The offset in the saved state stream of the start of this unit.
     * This is mainly intended for sanity checking. */
    uint64_t        offStream;
    /** The checksum of this structure, including the whole name.
     * Calculated with this field set to zero.  */
    uint32_t        u32CRC;
    /** Data version. */
    uint32_t        u32Version;
    /** Instance number. */
    uint32_t        u32Instance;
    /** Data phase number. */
    uint32_t        u32Phase;
    /** Flags reserved for future extensions. Must be zero. */
    uint32_t        fFlags;
    /** Size of the data unit name including the terminator. (bytes) */
    uint32_t        cbName;
    /** Data unit name, variable size. */
    char            szName[SSM_MAX_NAME_SIZE];
} SSMFILEUNITHDR;
AssertCompileMemberOffset(SSMFILEUNITHDR, szName, 40);
AssertCompileMemberSize(SSMFILEUNITHDR, szMagic, sizeof(SSMFILEUNITHDR_MAGIC));
AssertCompileMemberSize(SSMFILEUNITHDR, szMagic, sizeof(SSMFILEUNITHDR_END));
/** Pointer to SSMFILEUNITHDR.  */
typedef SSMFILEUNITHDR *PSSMFILEUNITHDR;


/**
 * Data unit header.
 *
 * This is used by v1.0, v1.1 and v1.2 of the format.
 */
typedef struct SSMFILEUNITHDRV1
{
    /** Magic (SSMFILEUNITHDR_MAGIC or SSMFILEUNITHDR_END). */
    char            achMagic[8];
    /** Number of bytes in this data unit including the header. */
    uint64_t        cbUnit;
    /** Data version. */
    uint32_t        u32Version;
    /** Instance number. */
    uint32_t        u32Instance;
    /** Size of the data unit name including the terminator. (bytes) */
    uint32_t        cchName;
    /** Data unit name. */
    char            szName[1];
} SSMFILEUNITHDRV1;
/** Pointer to SSMFILEUNITHDR.  */
typedef SSMFILEUNITHDRV1 *PSSMFILEUNITHDRV1;


/**
 * Termination data record.
 */
typedef struct SSMRECTERM
{
    uint8_t         u8TypeAndFlags;
    /** The record size (sizeof(SSMRECTERM) - 2). */
    uint8_t         cbRec;
    /** Flags, see SSMRECTERM_FLAGS_CRC32. */
    uint16_t        fFlags;
    /** The checksum of the data up to the start of this record. */
    uint32_t        u32CRC;
    /** The length of this data unit in bytes (including this record). */
    uint64_t        cbUnit;
} SSMRECTERM;
AssertCompileSize(SSMRECTERM, 16);
AssertCompileMemberAlignment(SSMRECTERM, cbUnit, 8);
/** Pointer to a termination record. */
typedef SSMRECTERM *PSSMRECTERM;
/** Pointer to a const termination record. */
typedef SSMRECTERM const *PCSSMRECTERM;


/**
 * Directory entry.
 */
typedef struct SSMFILEDIRENTRY
{
    /** The offset of the data unit. */
    uint64_t        off;
    /** The instance number. */
    uint32_t        u32Instance;
    /** The CRC-32 of the name excluding the terminator. (lazy bird) */
    uint32_t        u32NameCRC;
} SSMFILEDIRENTRY;
AssertCompileSize(SSMFILEDIRENTRY, 16);
/** Pointer to a directory entry. */
typedef SSMFILEDIRENTRY *PSSMFILEDIRENTRY;
/** Pointer to a const directory entry. */
typedef SSMFILEDIRENTRY const *PCSSMFILEDIRENTRY;

/**
 * Directory for the data units from the final phase.
 *
 * This is used to speed up SSMR3Seek (it would have to decompress and parse the
 * whole stream otherwise).
 */
typedef struct SSMFILEDIR
{
    /** Magic string (SSMFILEDIR_MAGIC). */
    char            szMagic[8];
    /** The CRC-32 for the whole directory.
     * Calculated with this field set to zero. */
    uint32_t        u32CRC;
    /** The number of directory entries. */
    uint32_t        cEntries;
    /** The directory entries (variable size). */
    SSMFILEDIRENTRY aEntries[1];
} SSMFILEDIR;
AssertCompileSize(SSMFILEDIR, 32);
/** Pointer to a directory. */
typedef SSMFILEDIR *PSSMFILEDIR;
/** Pointer to a const directory. */
typedef SSMFILEDIR *PSSMFILEDIR;


/**
 * Footer structure
 */
typedef struct SSMFILEFTR
{
    /** Magic string (SSMFILEFTR_MAGIC). */
    char            szMagic[8];
    /** The offset of this record in the stream. */
    uint64_t        offStream;
    /** The CRC for the stream.
     * This is set to zero if SSMFILEHDR_FLAGS_STREAM_CRC32 is clear. */
    uint32_t        u32StreamCRC;
    /** Number directory entries. */
    uint32_t        cDirEntries;
    /** Reserved footer space - must be zero. */
    uint32_t        u32Reserved;
    /** The CRC-32 for this structure.
     * Calculated with this field set to zero. */
    uint32_t        u32CRC;
} SSMFILEFTR;
AssertCompileSize(SSMFILEFTR, 32);
/** Pointer to a footer. */
typedef SSMFILEFTR *PSSMFILEFTR;
/** Pointer to a const footer. */
typedef SSMFILEFTR const *PCSSMFILEFTR;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  ssmR3LazyInit(PVM pVM);
static DECLCALLBACK(int)    ssmR3SelfSaveExec(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)    ssmR3SelfLoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static int                  ssmR3Register(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess, const char *pszBefore, PSSMUNIT *ppUnit);
static int                  ssmR3CalcChecksum(RTFILE File, uint64_t cbFile, uint32_t *pu32CRC);
static void                 ssmR3Progress(PSSMHANDLE pSSM, uint64_t cbAdvance);
static PSSMUNIT             ssmR3Find(PVM pVM, const char *pszName, uint32_t u32Instance);
static int                  ssmR3DataWriteFinish(PSSMHANDLE pSSM);
static void                 ssmR3DataWriteBegin(PSSMHANDLE pSSM);
static int                  ssmR3DataWriteRaw(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf);
static int                  ssmR3DataFlushBuffer(PSSMHANDLE pSSM);
static int                  ssmR3DataWrite(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf);
static void                 ssmR3DataReadFinishV1(PSSMHANDLE pSSM);
static void                 ssmR3DataReadBeginV2(PSSMHANDLE pSSM);
static void                 ssmR3DataReadFinishV2(PSSMHANDLE pSSM);
static int                  ssmR3DataReadRecHdrV2(PSSMHANDLE pSSM);
static int                  ssmR3DataRead(PSSMHANDLE pSSM, void *pvBuf, size_t cbBuf);


/**
 * Performs lazy initialization of the SSM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM.
 */
static int ssmR3LazyInit(PVM pVM)
{
    /*
     * Register a saved state unit which we use to put the VirtualBox version,
     * revision and similar stuff in.
     */
    pVM->ssm.s.fInitialized = true;
    int rc = SSMR3RegisterInternal(pVM, "SSM", 0 /*u32Instance*/, 1/*u32Version*/, 64 /*cbGuess*/,
                                   NULL /*pfnSavePrep*/, ssmR3SelfSaveExec, NULL /*pfnSaveDone*/,
                                   NULL /*pfnSavePrep*/, ssmR3SelfLoadExec, NULL /*pfnSaveDone*/);
    pVM->ssm.s.fInitialized = RT_SUCCESS(rc);
    return rc;
}


/**
 * For saving usful things without having to go thru the tedious process of
 * adding it to the header.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pSSM            The SSM handle.
 */
static DECLCALLBACK(int) ssmR3SelfSaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * String table containg pairs of variable and value string.
     * Terminated by two empty strings.
     */
#ifdef VBOX_OSE
    SSMR3PutStrZ(pSSM, "OSE");
    SSMR3PutStrZ(pSSM, "true");
#endif

    /* terminator */
    SSMR3PutStrZ(pSSM, "");
    return SSMR3PutStrZ(pSSM, "");
}


/**
 * For load the version + revision and stuff.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pSSM            The SSM handle.
 * @param   u32Version      The version (1).
 */
static DECLCALLBACK(int) ssmR3SelfLoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    AssertLogRelMsgReturn(u32Version == 1, ("%d", u32Version), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    /*
     * String table containg pairs of variable and value string.
     * Terminated by two empty strings.
     */
    for (unsigned i = 0; ; i++)
    {
        char szVar[128];
        char szValue[1024];
        int rc = SSMR3GetStrZ(pSSM, szVar, sizeof(szVar));
        AssertRCReturn(rc, rc);
        rc = SSMR3GetStrZ(pSSM, szValue, sizeof(szValue));
        AssertRCReturn(rc, rc);
        if (!szVar[0] && !szValue[0])
            break;
        if (i == 0)
            LogRel(("SSM: Saved state info:\n"));
        LogRel(("SSM:   %s: %s\n", szVar, szValue));
    }
    return VINF_SUCCESS;
}


/**
 * Internal registration worker.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance id.
 * @param   u32Version      The data unit version.
 * @param   cbGuess         The guessed data unit size.
 * @param   pszBefore       Name of data unit to be placed in front of.
 *                          Optional.
 * @param   ppUnit          Where to store the insterted unit node.
 *                          Caller must fill in the missing details.
 */
static int ssmR3Register(PVM pVM, const char *pszName, uint32_t u32Instance,
                         uint32_t u32Version, size_t cbGuess, const char *pszBefore, PSSMUNIT *ppUnit)
{
    /*
     * Validate input.
     */
    AssertPtr(pszName);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    size_t cchName = strlen(pszName);
    AssertMsgReturn(cchName < SSM_MAX_NAME_SIZE, ("%zu >= %u: %s\n", cchName, SSM_MAX_NAME_SIZE, pszName), VERR_OUT_OF_RANGE);

    AssertReturn(!pszBefore || *pszBefore, VERR_INVALID_PARAMETER);
    size_t cchBefore = pszBefore ? strlen(pszBefore) : 0;
    AssertMsgReturn(cchBefore < SSM_MAX_NAME_SIZE, ("%zu >= %u: %s\n", cchBefore, SSM_MAX_NAME_SIZE, pszBefore), VERR_OUT_OF_RANGE);

    /*
     * Lazy init.
     */
    if (!pVM->ssm.s.fInitialized)
    {
        int rc = ssmR3LazyInit(pVM);
        AssertRCReturn(rc, rc);
    }

    /*
     * Walk to the end of the list checking for duplicates as we go.
     */
    PSSMUNIT    pUnitBeforePrev = NULL;
    PSSMUNIT    pUnitBefore     = NULL;
    PSSMUNIT    pUnitPrev       = NULL;
    PSSMUNIT    pUnit           = pVM->ssm.s.pHead;
    while (pUnit)
    {
        if (    pUnit->u32Instance == u32Instance
            &&  pUnit->cchName == cchName
            &&  !memcmp(pUnit->szName, pszName, cchName))
        {
            AssertMsgFailed(("Duplicate registration %s\n", pszName));
            return VERR_SSM_UNIT_EXISTS;
        }
        if (    pUnit->cchName == cchBefore
            &&  !pUnitBefore
            &&  !memcmp(pUnit->szName, pszBefore, cchBefore))
        {
            pUnitBeforePrev = pUnitPrev;
            pUnitBefore     = pUnit;
        }

        /* next */
        pUnitPrev = pUnit;
        pUnit = pUnit->pNext;
    }

    /*
     * Allocate new node.
     */
    pUnit = (PSSMUNIT)MMR3HeapAllocZ(pVM, MM_TAG_SSM, RT_OFFSETOF(SSMUNIT, szName[cchName + 1]));
    if (!pUnit)
        return VERR_NO_MEMORY;

    /*
     * Fill in (some) data. (Stuff is zero'ed.)
     */
    pUnit->u32Version   = u32Version;
    pUnit->u32Instance  = u32Instance;
    pUnit->cbGuess      = cbGuess;
    pUnit->cchName      = cchName;
    memcpy(pUnit->szName, pszName, cchName);

    /*
     * Insert
     */
    if (pUnitBefore)
    {
        pUnit->pNext = pUnitBefore;
        if (pUnitBeforePrev)
            pUnitBeforePrev->pNext = pUnit;
        else
            pVM->ssm.s.pHead       = pUnit;
    }
    else if (pUnitPrev)
        pUnitPrev->pNext = pUnit;
    else
        pVM->ssm.s.pHead = pUnit;
    pVM->ssm.s.cUnits++;

    *ppUnit = pUnit;
    return VINF_SUCCESS;
}


/**
 * Register a PDM Devices data unit.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pszBefore       Name of data unit which we should be put in front
 *                          of. Optional (NULL).
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
VMMR3DECL(int) SSMR3RegisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess, const char *pszBefore,
    PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
    PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone)
{
    PSSMUNIT pUnit;
    int rc = ssmR3Register(pVM, pszName, u32Instance, u32Version, cbGuess, pszBefore, &pUnit);
    if (RT_SUCCESS(rc))
    {
        pUnit->enmType = SSMUNITTYPE_DEV;
        pUnit->u.Dev.pfnSavePrep = pfnSavePrep;
        pUnit->u.Dev.pfnSaveExec = pfnSaveExec;
        pUnit->u.Dev.pfnSaveDone = pfnSaveDone;
        pUnit->u.Dev.pfnLoadPrep = pfnLoadPrep;
        pUnit->u.Dev.pfnLoadExec = pfnLoadExec;
        pUnit->u.Dev.pfnLoadDone = pfnLoadDone;
        pUnit->u.Dev.pDevIns = pDevIns;
    }
    return rc;
}


/**
 * Register a PDM driver data unit.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pDrvIns         Driver instance.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
VMMR3DECL(int) SSMR3RegisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
    PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone)
{
    PSSMUNIT pUnit;
    int rc = ssmR3Register(pVM, pszName, u32Instance, u32Version, cbGuess, NULL, &pUnit);
    if (RT_SUCCESS(rc))
    {
        pUnit->enmType = SSMUNITTYPE_DRV;
        pUnit->u.Drv.pfnSavePrep = pfnSavePrep;
        pUnit->u.Drv.pfnSaveExec = pfnSaveExec;
        pUnit->u.Drv.pfnSaveDone = pfnSaveDone;
        pUnit->u.Drv.pfnLoadPrep = pfnLoadPrep;
        pUnit->u.Drv.pfnLoadExec = pfnLoadExec;
        pUnit->u.Drv.pfnLoadDone = pfnLoadDone;
        pUnit->u.Drv.pDrvIns = pDrvIns;
    }
    return rc;
}


/**
 * Register a internal data unit.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
VMMR3DECL(int) SSMR3RegisterInternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMINTSAVEPREP pfnSavePrep, PFNSSMINTSAVEEXEC pfnSaveExec, PFNSSMINTSAVEDONE pfnSaveDone,
    PFNSSMINTLOADPREP pfnLoadPrep, PFNSSMINTLOADEXEC pfnLoadExec, PFNSSMINTLOADDONE pfnLoadDone)
{
    PSSMUNIT pUnit;
    int rc = ssmR3Register(pVM, pszName, u32Instance, u32Version, cbGuess, NULL, &pUnit);
    if (RT_SUCCESS(rc))
    {
        pUnit->enmType = SSMUNITTYPE_INTERNAL;
        pUnit->u.Internal.pfnSavePrep = pfnSavePrep;
        pUnit->u.Internal.pfnSaveExec = pfnSaveExec;
        pUnit->u.Internal.pfnSaveDone = pfnSaveDone;
        pUnit->u.Internal.pfnLoadPrep = pfnLoadPrep;
        pUnit->u.Internal.pfnLoadExec = pfnLoadExec;
        pUnit->u.Internal.pfnLoadDone = pfnLoadDone;
    }
    return rc;
}


/**
 * Register an external data unit.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 * @param   pvUser          User argument.
 */
VMMR3DECL(int) SSMR3RegisterExternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMEXTSAVEPREP pfnSavePrep, PFNSSMEXTSAVEEXEC pfnSaveExec, PFNSSMEXTSAVEDONE pfnSaveDone,
    PFNSSMEXTLOADPREP pfnLoadPrep, PFNSSMEXTLOADEXEC pfnLoadExec, PFNSSMEXTLOADDONE pfnLoadDone, void *pvUser)
{
    PSSMUNIT pUnit;
    int rc = ssmR3Register(pVM, pszName, u32Instance, u32Version, cbGuess, NULL, &pUnit);
    if (RT_SUCCESS(rc))
    {
        pUnit->enmType = SSMUNITTYPE_EXTERNAL;
        pUnit->u.External.pfnSavePrep = pfnSavePrep;
        pUnit->u.External.pfnSaveExec = pfnSaveExec;
        pUnit->u.External.pfnSaveDone = pfnSaveDone;
        pUnit->u.External.pfnLoadPrep = pfnLoadPrep;
        pUnit->u.External.pfnLoadExec = pfnLoadExec;
        pUnit->u.External.pfnLoadDone = pfnLoadDone;
        pUnit->u.External.pvUser      = pvUser;
    }
    return rc;
}


/**
 * Deregister one or more PDM Device data units.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pszName         Data unit name.
 *                          Use NULL to deregister all data units for that device instance.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @remark  Only for dynmaic data units and dynamic unloaded modules.
 */
VMMR3DECL(int) SSMR3DeregisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance)
{
    /*
     * Validate input.
     */
    if (!pDevIns)
    {
        AssertMsgFailed(("pDevIns is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Search the list.
     */
    size_t      cchName = pszName ? strlen(pszName) : 0;
    int         rc = pszName ? VERR_SSM_UNIT_NOT_FOUND : VINF_SUCCESS;
    PSSMUNIT    pUnitPrev = NULL;
    PSSMUNIT    pUnit = pVM->ssm.s.pHead;
    while (pUnit)
    {
        if (    pUnit->enmType == SSMUNITTYPE_DEV
            &&  (   !pszName
                 || (   pUnit->cchName == cchName
                     && !memcmp(pUnit->szName, pszName, cchName)))
            &&  pUnit->u32Instance == u32Instance
            )
        {
            if (pUnit->u.Dev.pDevIns == pDevIns)
            {
                /*
                 * Unlink it, advance pointer, and free the node.
                 */
                PSSMUNIT pFree = pUnit;
                pUnit = pUnit->pNext;
                if (pUnitPrev)
                    pUnitPrev->pNext = pUnit;
                else
                    pVM->ssm.s.pHead = pUnit;
                pVM->ssm.s.cUnits--;
                Log(("SSM: Removed data unit '%s' (pdm dev).\n", pFree->szName));
                MMR3HeapFree(pFree);

                if (pszName)
                    return VINF_SUCCESS;
                rc = VINF_SUCCESS;
                continue;
            }
            else if (pszName)
            {
                AssertMsgFailed(("Caller is not owner! Owner=%p Caller=%p %s\n",
                                 pUnit->u.Dev.pDevIns, pDevIns, pszName));
                return VERR_SSM_UNIT_NOT_OWNER;
            }
        }

        /* next */
        pUnitPrev = pUnit;
        pUnit = pUnit->pNext;
    }

    return rc;
}


/**
 * Deregister one ore more PDM Driver data units.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pDrvIns         Driver instance.
 * @param   pszName         Data unit name.
 *                          Use NULL to deregister all data units for that driver instance.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique. Ignored if pszName is NULL.
 * @remark  Only for dynmaic data units and dynamic unloaded modules.
 */
VMMR3DECL(int) SSMR3DeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance)
{
    /*
     * Validate input.
     */
    if (!pDrvIns)
    {
        AssertMsgFailed(("pDrvIns is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Search the list.
     */
    size_t      cchName = pszName ? strlen(pszName) : 0;
    int         rc = pszName ? VERR_SSM_UNIT_NOT_FOUND : VINF_SUCCESS;
    PSSMUNIT    pUnitPrev = NULL;
    PSSMUNIT    pUnit = pVM->ssm.s.pHead;
    while (pUnit)
    {
        if (    pUnit->enmType == SSMUNITTYPE_DRV
            &&  (   !pszName
                 || (   pUnit->cchName == cchName
                     && !memcmp(pUnit->szName, pszName, cchName)
                     && pUnit->u32Instance == u32Instance))
            )
        {
            if (pUnit->u.Drv.pDrvIns == pDrvIns)
            {
                /*
                 * Unlink it, advance pointer, and free the node.
                 */
                PSSMUNIT pFree = pUnit;
                pUnit = pUnit->pNext;
                if (pUnitPrev)
                    pUnitPrev->pNext = pUnit;
                else
                    pVM->ssm.s.pHead = pUnit;
                pVM->ssm.s.cUnits--;
                Log(("SSM: Removed data unit '%s' (pdm drv).\n", pFree->szName));
                MMR3HeapFree(pFree);

                if (pszName)
                    return VINF_SUCCESS;
                rc = VINF_SUCCESS;
                continue;
            }
            else if (pszName)
            {
                AssertMsgFailed(("Caller is not owner! Owner=%p Caller=%p %s\n",
                                 pUnit->u.Drv.pDrvIns, pDrvIns, pszName));
                return VERR_SSM_UNIT_NOT_OWNER;
            }
        }

        /* next */
        pUnitPrev = pUnit;
        pUnit = pUnit->pNext;
    }

    return rc;
}


/**
 * Deregister a data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   enmType         Unit type
 * @param   pszName         Data unit name.
 * @remark  Only for dynmaic data units.
 */
static int ssmR3DeregisterByNameAndType(PVM pVM, const char *pszName, SSMUNITTYPE enmType)
{
    /*
     * Validate input.
     */
    if (!pszName)
    {
        AssertMsgFailed(("pszName is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Search the list.
     */
    size_t      cchName = strlen(pszName);
    int         rc = VERR_SSM_UNIT_NOT_FOUND;
    PSSMUNIT    pUnitPrev = NULL;
    PSSMUNIT    pUnit = pVM->ssm.s.pHead;
    while (pUnit)
    {
        if (    pUnit->enmType == enmType
            &&  pUnit->cchName == cchName
            &&  !memcmp(pUnit->szName, pszName, cchName))
        {
            /*
             * Unlink it, advance pointer, and free the node.
             */
            PSSMUNIT pFree = pUnit;
            pUnit = pUnit->pNext;
            if (pUnitPrev)
                pUnitPrev->pNext = pUnit;
            else
                pVM->ssm.s.pHead = pUnit;
            pVM->ssm.s.cUnits--;
            Log(("SSM: Removed data unit '%s' (type=%d).\n", pFree->szName, enmType));
            MMR3HeapFree(pFree);
            return VINF_SUCCESS;
        }

        /* next */
        pUnitPrev = pUnit;
        pUnit = pUnit->pNext;
    }

    return rc;
}


/**
 * Deregister an internal data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @remark  Only for dynmaic data units.
 */
VMMR3DECL(int) SSMR3DeregisterInternal(PVM pVM, const char *pszName)
{
    return ssmR3DeregisterByNameAndType(pVM, pszName, SSMUNITTYPE_INTERNAL);
}


/**
 * Deregister an external data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @remark  Only for dynmaic data units.
 */
VMMR3DECL(int) SSMR3DeregisterExternal(PVM pVM, const char *pszName)
{
    return ssmR3DeregisterByNameAndType(pVM, pszName, SSMUNITTYPE_EXTERNAL);
}


/**
 * Stream output routine.
 *
 * @returns VBox status code.
 * @param   pSSM        The SSM handle.
 * @param   pvBuf       What to write.
 * @param   cbToWrite   How much to write.
 */
static int ssmR3StrmWrite(PSSMHANDLE pSSM, const void *pvBuf, size_t cbToWrite)
{
    Assert(pSSM->enmOp <= SSMSTATE_SAVE_DONE);
    int rc = RTFileWrite(pSSM->hFile, pvBuf, cbToWrite, NULL);
    if (RT_SUCCESS(rc))
    {
        if (pSSM->fChecksummed)
            pSSM->u32StreamCRC = RTCrc32Process(pSSM->u32StreamCRC, pvBuf, cbToWrite);
        return VINF_SUCCESS;
    }

    LogRel(("ssmR3StrmWrite: RTFileWrite(,,%zu) -> %Rrc\n", cbToWrite, rc));
    if (RT_SUCCESS(pSSM->rc))
        pSSM->rc = rc;
    return rc;
}


/**
 * Stream input routine.
 *
 * @returns VBox status code.
 * @param   pSSM        The SSM handle.
 * @param   pvBuf       Where to put what we read.
 * @param   cbToRead    How much to read.
 */
static int ssmR3StrmRead(PSSMHANDLE pSSM, void *pvBuf, size_t cbToRead)
{
    Assert(pSSM->enmOp >= SSMSTATE_LOAD_PREP);
    int rc = RTFileRead(pSSM->hFile, pvBuf, cbToRead, NULL);
    if (RT_SUCCESS(rc))
    {
        if (pSSM->fChecksummed)
            pSSM->u32StreamCRC = RTCrc32Process(pSSM->u32StreamCRC, pvBuf, cbToRead);
        return VINF_SUCCESS;
    }

    LogRel(("ssmR3StrmRead: RTFileRead(,,%zu) -> %Rrc\n", cbToRead, rc));
    if (RT_SUCCESS(pSSM->rc))
        pSSM->rc = rc;
    return rc;
}


/**
 * Flush any buffered data.
 *
 * @returns VBox status code.
 * @param   pSSM        The SSM handle.
 */
static int ssmR3StrmFlush(PSSMHANDLE pSSM)
{
    /* no buffering yet. */
    return VINF_SUCCESS;
}


/**
 * Tell current stream position.
 *
 * @returns stream position.
 * @param   pSSM        The SSM handle.
 */
static uint64_t ssmR3StrmTell(PSSMHANDLE pSSM)
{
    return RTFileTell(pSSM->hFile);
}



/**
 * Calculate the checksum of a file portion.
 *
 * @returns VBox status.
 * @param   File        Handle to the file. Positioned where to start
 *                      checksumming.
 * @param   cbFile      The portion of the file to checksum.
 * @param   pu32CRC     Where to store the calculated checksum.
 */
static int ssmR3CalcChecksum(RTFILE File, uint64_t cbFile, uint32_t *pu32CRC)
{
    /*
     * Allocate a buffer.
     */
    void *pvBuf = RTMemTmpAlloc(32*1024);
    if (!pvBuf)
        return VERR_NO_TMP_MEMORY;

    /*
     * Loop reading and calculating CRC32.
     */
    int         rc = VINF_SUCCESS;
    uint32_t    u32CRC = RTCrc32Start();
    while (cbFile)
    {
        /* read chunk */
        register unsigned cbToRead = 32*1024;
        if (cbFile < 32*1024)
            cbToRead = (unsigned)cbFile;
        rc = RTFileRead(File, pvBuf, cbToRead, NULL);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Failed with rc=%Rrc while calculating crc.\n", rc));
            RTMemTmpFree(pvBuf);
            return rc;
        }

        /* update total */
        cbFile -= cbToRead;

        /* calc crc32. */
        u32CRC = RTCrc32Process(u32CRC, pvBuf,  cbToRead);
    }
    RTMemTmpFree(pvBuf);

    /* store the calculated crc */
    u32CRC = RTCrc32Finish(u32CRC);
    Log(("SSM: u32CRC=0x%08x\n", u32CRC));
    *pu32CRC = u32CRC;

    return VINF_SUCCESS;
}


/**
 * Works the progress calculation.
 *
 * @param   pSSM        The SSM handle.
 * @param   cbAdvance   Number of bytes to advance
 */
static void ssmR3Progress(PSSMHANDLE pSSM, uint64_t cbAdvance)
{
    /* Can't advance it beyond the estimated end of the unit. */
    uint64_t cbLeft = pSSM->offEstUnitEnd - pSSM->offEst;
    if (cbAdvance > cbLeft)
        cbAdvance = cbLeft;
    pSSM->offEst += cbAdvance;

    /* uPercentPrepare% prepare, xx% exec, uPercentDone% done+crc */
    while (   pSSM->offEst >= pSSM->offEstProgress
           && pSSM->uPercent <= 100-pSSM->uPercentDone)
    {
        if (pSSM->pfnProgress)
            pSSM->pfnProgress(pSSM->pVM, pSSM->uPercent, pSSM->pvUser);
        pSSM->uPercent++;
        pSSM->offEstProgress = (pSSM->uPercent - pSSM->uPercentPrepare) * pSSM->cbEstTotal
                             / (100 - pSSM->uPercentDone - pSSM->uPercentPrepare);
    }
}


/**
 * Writes the directory.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pSSM                The SSM handle.
 * @param   pcEntries           Where to return the number of directory entries.
 */
static int ssmR3WriteDirectory(PVM pVM, PSSMHANDLE pSSM, uint32_t *pcEntries)
{
    /*
     * Grab some temporary memory for the dictionary.
     */
    size_t      cbDir = RT_OFFSETOF(SSMFILEDIR, aEntries[pVM->ssm.s.cUnits]);
    PSSMFILEDIR pDir  = (PSSMFILEDIR)RTMemTmpAlloc(cbDir);
    if (!pDir)
    {
        LogRel(("ssmR3WriteDirectory: failed to allocate %zu bytes!\n", cbDir));
        return VERR_NO_TMP_MEMORY;
    }

    /*
     * Initialize it.
     */
    memcpy(pDir->szMagic, SSMFILEDIR_MAGIC, sizeof(pDir->szMagic));
    pDir->u32CRC   = 0;
    pDir->cEntries = 0;

    for (PSSMUNIT pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        if (pUnit->offStream != RTFOFF_MIN)
        {
            PSSMFILEDIRENTRY pEntry = &pDir->aEntries[pDir->cEntries++];
            Assert(pDir->cEntries <= pVM->ssm.s.cUnits);
            pEntry->off         = pUnit->offStream;
            pEntry->u32Instance = pUnit->u32Instance;
            pEntry->u32NameCRC  = RTCrc32(pUnit->szName, pUnit->cchName);
        }

    /*
     * Calculate the actual size and CRC-32, then write the directory
     * out to the stream.
     */
    *pcEntries = pDir->cEntries;
    cbDir = RT_OFFSETOF(SSMFILEDIR, aEntries[pDir->cEntries]);
    pDir->u32CRC = RTCrc32(pDir, cbDir);
    int rc = ssmR3StrmWrite(pSSM, pDir, cbDir);
    RTMemTmpFree(pDir);
    return rc;
}


/**
 * Start VM save operation.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pszFilename     Name of the file to save the state in.
 * @param   enmAfter        What is planned after a successful save operation.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 *
 * @thread  EMT
 */
VMMR3DECL(int) SSMR3Save(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("SSMR3Save: pszFilename=%p:{%s} enmAfter=%d pfnProgress=%p pvUser=%p\n", pszFilename, pszFilename, enmAfter, pfnProgress, pvUser));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input.
     */
    if (    enmAfter != SSMAFTER_DESTROY
        &&  enmAfter != SSMAFTER_CONTINUE)
    {
        AssertMsgFailed(("Invalid enmAfter=%d!\n", enmAfter));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the handle and try open the file.
     *
     * Note that there might be quite some work to do after executing the saving,
     * so we reserve 20% for the 'Done' period.  The checksumming and closing of
     * the saved state file might take a long time.
     */
    SSMHANDLE Handle        = {0};
    Handle.hFile            = NIL_RTFILE;
    Handle.pVM              = pVM;
    Handle.enmOp            = SSMSTATE_INVALID;
    Handle.enmAfter         = enmAfter;
    Handle.rc               = VINF_SUCCESS;
    Handle.cbUnitLeftV1     = 0;
    Handle.offUnit          = UINT64_MAX;
    Handle.fUnitChecksummed = false;
    Handle.u32UnitCRC       = 0;
    Handle.pfnProgress      = pfnProgress;
    Handle.pvUser           = pvUser;
    Handle.uPercent         = 0;
    Handle.offEstProgress   = 0;
    Handle.cbEstTotal       = 0;
    Handle.offEst           = 0;
    Handle.offEstUnitEnd    = 0;
    Handle.uPercentPrepare  = 20;
    Handle.uPercentDone     = 2;
    Handle.fChecksummed     = true;
    Handle.u32StreamCRC     = RTCrc32Start();
    Handle.u.Write.pZipComp         = NULL;
    Handle.u.Write.offDataBuffer    = 0;

    int rc = RTFileOpen(&Handle.hFile, pszFilename, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
    {
        LogRel(("SSM: Failed to create save state file '%s', rc=%Rrc.\n",  pszFilename, rc));
        return rc;
    }

    Log(("SSM: Starting state save to file '%s'...\n", pszFilename));

    /*
     * Write header.
     */
    union
    {
        SSMFILEHDR FileHdr;
        SSMFILEUNITHDR UnitHdr;
        SSMFILEFTR Footer;
    } u;

    memcpy(&u.FileHdr.szMagic, SSMFILEHDR_MAGIC_V2_0, sizeof(u.FileHdr.szMagic));
    u.FileHdr.u16VerMajor  = VBOX_VERSION_MAJOR;
    u.FileHdr.u16VerMinor  = VBOX_VERSION_MINOR;
    u.FileHdr.u32VerBuild  = VBOX_VERSION_BUILD;
    u.FileHdr.u32SvnRev    = VMMGetSvnRev(),
    u.FileHdr.cHostBits    = HC_ARCH_BITS;
    u.FileHdr.cbGCPhys     = sizeof(RTGCPHYS);
    u.FileHdr.cbGCPtr      = sizeof(RTGCPTR);
    u.FileHdr.u8Reserved   = 0;
    u.FileHdr.cUnits       = pVM->ssm.s.cUnits;
    u.FileHdr.fFlags       = SSMFILEHDR_FLAGS_STREAM_CRC32;
    u.FileHdr.u32Reserved  = 0;
    u.FileHdr.u32CRC       = 0;
    u.FileHdr.u32CRC       = RTCrc32(&u.FileHdr, sizeof(u.FileHdr));
    rc = ssmR3StrmWrite(&Handle, &u.FileHdr, sizeof(u.FileHdr));
    if (RT_SUCCESS(rc))
    {
        /*
         * Clear the per unit flags and offsets.
         */
        PSSMUNIT pUnit;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        {
            pUnit->fCalled   = false;
            pUnit->offStream = RTFOFF_MIN;
        }

        /*
         * Do the prepare run.
         */
        Handle.rc = VINF_SUCCESS;
        Handle.enmOp = SSMSTATE_SAVE_PREP;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        {
            if (pUnit->u.Common.pfnSavePrep)
            {
                switch (pUnit->enmType)
                {
                    case SSMUNITTYPE_DEV:
                        rc = pUnit->u.Dev.pfnSavePrep(pUnit->u.Dev.pDevIns, &Handle);
                        break;
                    case SSMUNITTYPE_DRV:
                        rc = pUnit->u.Drv.pfnSavePrep(pUnit->u.Drv.pDrvIns, &Handle);
                        break;
                    case SSMUNITTYPE_INTERNAL:
                        rc = pUnit->u.Internal.pfnSavePrep(pVM, &Handle);
                        break;
                    case SSMUNITTYPE_EXTERNAL:
                        rc = pUnit->u.External.pfnSavePrep(&Handle, pUnit->u.External.pvUser);
                        break;
                }
                pUnit->fCalled = true;
                if (RT_FAILURE(rc))
                {
                    LogRel(("SSM: Prepare save failed with rc=%Rrc for data unit '%s.\n", rc, pUnit->szName));
                    break;
                }
            }

            Handle.cbEstTotal += pUnit->cbGuess;
        }

        /* Progress. */
        if (pfnProgress)
            pfnProgress(pVM, Handle.uPercentPrepare-1, pvUser);
        Handle.uPercent = Handle.uPercentPrepare;

        /*
         * Do the execute run.
         */
        if (RT_SUCCESS(rc))
        {
            Handle.enmOp = SSMSTATE_SAVE_EXEC;
            for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
            {
                /*
                 * Estimate.
                 */
                ssmR3Progress(&Handle, Handle.offEstUnitEnd - Handle.offEst);
                Handle.offEstUnitEnd += pUnit->cbGuess;

                /*
                 * Does this unit have a callback? If, not skip it.
                 */
                if (!pUnit->u.Common.pfnSaveExec)
                {
                    pUnit->fCalled = true;
                    continue;
                }
                pUnit->offStream = ssmR3StrmTell(&Handle);

                /*
                 * Write data unit header
                 */
                memcpy(&u.UnitHdr.szMagic[0], SSMFILEUNITHDR_MAGIC, sizeof(u.UnitHdr.szMagic));
                u.UnitHdr.offStream   = pUnit->offStream;
                u.UnitHdr.u32CRC      = 0;
                u.UnitHdr.u32Version  = pUnit->u32Version;
                u.UnitHdr.u32Instance = pUnit->u32Instance;
                u.UnitHdr.u32Phase    = SSM_PHASE_FINAL;
                u.UnitHdr.fFlags      = 0;
                u.UnitHdr.cbName      = (uint32_t)pUnit->cchName + 1;
                memcpy(&u.UnitHdr.szName[0], &pUnit->szName[0], u.UnitHdr.cbName);
                u.UnitHdr.u32CRC      = RTCrc32(&u.UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[u.UnitHdr.cbName]));
                Log(("SSM: Unit at %#9llx: '%s', instance %u, phase %#x, version %u\n",
                     u.UnitHdr.offStream, u.UnitHdr.szName, u.UnitHdr.u32Instance, u.UnitHdr.u32Phase, u.UnitHdr.u32Version));
                rc = ssmR3StrmWrite(&Handle, &u.UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[u.UnitHdr.cbName]));
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Call the execute handler.
                     */
                    ssmR3DataWriteBegin(&Handle);
                    switch (pUnit->enmType)
                    {
                        case SSMUNITTYPE_DEV:
                            rc = pUnit->u.Dev.pfnSaveExec(pUnit->u.Dev.pDevIns, &Handle);
                            break;
                        case SSMUNITTYPE_DRV:
                            rc = pUnit->u.Drv.pfnSaveExec(pUnit->u.Drv.pDrvIns, &Handle);
                            break;
                        case SSMUNITTYPE_INTERNAL:
                            rc = pUnit->u.Internal.pfnSaveExec(pVM, &Handle);
                            break;
                        case SSMUNITTYPE_EXTERNAL:
                            pUnit->u.External.pfnSaveExec(&Handle, pUnit->u.External.pvUser);
                            rc = Handle.rc;
                            break;
                    }
                    pUnit->fCalled = true;
                    if (RT_SUCCESS(rc))
                        rc = ssmR3DataFlushBuffer(&Handle); /* will return SSMHANDLE::rc if its set */
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Write the termination record and flush the compression stream.
                         */
                        SSMRECTERM TermRec;
                        TermRec.u8TypeAndFlags = SSM_REC_FLAGS_FIXED | SSM_REC_FLAGS_IMPORTANT | SSM_REC_TYPE_TERM;
                        TermRec.cbRec       = sizeof(TermRec) - 2;
                        if (Handle.fUnitChecksummed)
                        {
                            TermRec.fFlags  = SSMRECTERM_FLAGS_CRC32;
                            TermRec.u32CRC  = RTCrc32Finish(Handle.u32UnitCRC);
                        }
                        else
                        {
                            TermRec.fFlags  = 0;
                            TermRec.u32CRC  = 0;
                        }
                        TermRec.cbUnit      = Handle.offUnit + sizeof(TermRec);
                        rc = ssmR3DataWriteRaw(&Handle, &TermRec, sizeof(TermRec));
                        if (RT_SUCCESS(rc))
                            rc = ssmR3DataWriteFinish(&Handle);
                        if (RT_SUCCESS(rc))
                        {
                            Handle.offUnit     = UINT64_MAX;
                            Handle.u32UnitCRC  = 0;
                        }
                        else
                        {
                            LogRel(("SSM: Failed ending compression stream. rc=%Rrc\n", rc));
                            break;
                        }
                    }
                    else
                    {
                        LogRel(("SSM: Execute save failed with rc=%Rrc for data unit '%s.\n", rc, pUnit->szName));
                        break;
                    }
                }
                if (RT_FAILURE(rc))
                {
                    LogRel(("SSM: Failed to write unit header. rc=%Rrc\n", rc));
                    break;
                }
            } /* for each unit */

            /* finish the progress. */
            if (RT_SUCCESS(rc))
                ssmR3Progress(&Handle, Handle.offEstUnitEnd - Handle.offEst);
        }
        /* (progress should be pending 99% now) */
        AssertMsg(RT_FAILURE(rc) || Handle.uPercent == (101-Handle.uPercentDone), ("%d\n", Handle.uPercent));

        /*
         * Do the done run.
         */
        Handle.rc = rc;
        Handle.enmOp = SSMSTATE_SAVE_DONE;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        {
            if (    pUnit->u.Common.pfnSaveDone
                &&  (   pUnit->fCalled
                     || (!pUnit->u.Common.pfnSavePrep && !pUnit->u.Common.pfnSaveExec)))
            {
                switch (pUnit->enmType)
                {
                    case SSMUNITTYPE_DEV:
                        rc = pUnit->u.Dev.pfnSaveDone(pUnit->u.Dev.pDevIns, &Handle);
                        break;
                    case SSMUNITTYPE_DRV:
                        rc = pUnit->u.Drv.pfnSaveDone(pUnit->u.Drv.pDrvIns, &Handle);
                        break;
                    case SSMUNITTYPE_INTERNAL:
                        rc = pUnit->u.Internal.pfnSaveDone(pVM, &Handle);
                        break;
                    case SSMUNITTYPE_EXTERNAL:
                        rc = pUnit->u.External.pfnSaveDone(&Handle, pUnit->u.External.pvUser);
                        break;
                }
                if (RT_FAILURE(rc))
                {
                    LogRel(("SSM: Done save failed with rc=%Rrc for data unit '%s.\n", rc, pUnit->szName));
                    if (RT_SUCCESS(Handle.rc))
                        Handle.rc = rc;
                }
            }
        }
        rc = Handle.rc;

        /*
         * Finalize the file if successfully saved.
         */
        if (RT_SUCCESS(rc))
        {
            /* Write the end unit. */
            memcpy(&u.UnitHdr.szMagic[0], SSMFILEUNITHDR_END, sizeof(u.UnitHdr.szMagic));
            u.UnitHdr.offStream   = ssmR3StrmTell(&Handle);
            u.UnitHdr.u32Version  = 0;
            u.UnitHdr.u32Instance = 0;
            u.UnitHdr.u32Phase    = SSM_PHASE_FINAL;
            u.UnitHdr.fFlags      = 0;
            u.UnitHdr.cbName      = 0;
            u.UnitHdr.u32CRC      = 0;
            u.UnitHdr.u32CRC      = RTCrc32(&u.UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[0]));
            Log(("SSM: Unit at %#9llx: END UNIT\n", u.UnitHdr.offStream));
            rc = ssmR3StrmWrite(&Handle,  &u.UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[0]));
            if (RT_SUCCESS(rc))
            {
                /* Write the directory for the final units and then the footer. */
                rc = ssmR3WriteDirectory(pVM, &Handle, &u.Footer.cDirEntries);
                /* Flush the stream buffers so that the CRC is up to date. */
                if (RT_SUCCESS(rc))
                    rc = ssmR3StrmFlush(&Handle);
                if (RT_SUCCESS(rc))
                {
                    memcpy(u.Footer.szMagic, SSMFILEFTR_MAGIC, sizeof(u.Footer.szMagic));
                    u.Footer.offStream    = ssmR3StrmTell(&Handle);
                    u.Footer.u32StreamCRC = Handle.fChecksummed ? RTCrc32Finish(Handle.u32StreamCRC) : 0;
                    u.Footer.u32Reserved  = 0;
                    u.Footer.u32CRC       = 0;
                    u.Footer.u32CRC       = RTCrc32(&u.Footer, sizeof(u.Footer));
                    Log(("SSM: Footer at %#9llx: \n", u.Footer.offStream));
                    rc = ssmR3StrmWrite(&Handle, &u.Footer, sizeof(u.Footer));
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTFileClose(Handle.hFile);
                        AssertRC(rc);
                        if (pfnProgress)
                            pfnProgress(pVM, 100, pvUser);
                        Log(("SSM: Successfully saved the vm state to '%s'.\n", pszFilename));
                        return VINF_SUCCESS;
                    }
                }
            }
            LogRel(("SSM: Failed to finalize state file! rc=%Rrc\n", rc));
        }
    }

    /*
     * Delete the file on failure and destroy any compressors.
     */
    int rc2 = RTFileClose(Handle.hFile);
    AssertRC(rc2);
    rc2 = RTFileDelete(pszFilename);
    AssertRC(rc2);
    if (Handle.u.Write.pZipComp)
        RTZipCompDestroy(Handle.u.Write.pZipComp);

    return rc;
}


/**
 * Validates the header information stored in the handle.
 *
 * @returns VBox status code.
 *
 * @param   pSSM            The handle.
 * @param   fHaveHostBits   Set if the host bits field is valid.
 * @param   fHaveVersion    Set if we have a version.
 */
static int ssmR3ValidateHeaderInfo(PSSMHANDLE pSSM, bool fHaveHostBits, bool fHaveVersion)
{
    Assert(pSSM->u.Read.cbFileHdr < 256 && pSSM->u.Read.cbFileHdr > 32);
    Assert(pSSM->u.Read.uFmtVerMajor == 1 || pSSM->u.Read.uFmtVerMajor == 2);
    Assert(pSSM->u.Read.uFmtVerMinor <= 2);

    if (fHaveVersion)
    {
        if (    pSSM->u.Read.u16VerMajor == 0
            ||  pSSM->u.Read.u16VerMajor > 1000
            ||  pSSM->u.Read.u16VerMinor > 1000
            ||  pSSM->u.Read.u32VerBuild > _1M
            ||  pSSM->u.Read.u32SvnRev == 0
            ||  pSSM->u.Read.u32SvnRev > 10000000 /*100M*/)
        {
            LogRel(("SSM: Incorrect version values: %u.%u.%u.r%u\n",
                    pSSM->u.Read.u16VerMajor, pSSM->u.Read.u16VerMinor, pSSM->u.Read.u32VerBuild, pSSM->u.Read.u32SvnRev));
            return VERR_SSM_INTEGRITY_VBOX_VERSION;
        }
    }
    else
        AssertLogRelReturn(   pSSM->u.Read.u16VerMajor == 0
                           && pSSM->u.Read.u16VerMinor == 0
                           && pSSM->u.Read.u32VerBuild == 0
                           && pSSM->u.Read.u32SvnRev   == 0,
                           VERR_SSM_INTEGRITY_VBOX_VERSION);

    if (fHaveHostBits)
    {
        if (    pSSM->u.Read.cHostBits != 32
            &&  pSSM->u.Read.cHostBits != 64)
        {
            LogRel(("SSM: Incorrect cHostBits value: %u\n", pSSM->u.Read.cHostBits));
            return VERR_SSM_INTEGRITY_SIZES;
        }
    }
    else
        AssertLogRelReturn(pSSM->u.Read.cHostBits == 0, VERR_SSM_INTEGRITY_SIZES);

    if (    pSSM->u.Read.cbGCPhys != sizeof(uint32_t)
        &&  pSSM->u.Read.cbGCPhys != sizeof(uint64_t))
    {
        LogRel(("SSM: Incorrect cbGCPhys value: %d\n", pSSM->u.Read.cbGCPhys));
        return VERR_SSM_INTEGRITY_SIZES;
    }
    if (    pSSM->u.Read.cbGCPtr != sizeof(uint32_t)
        &&  pSSM->u.Read.cbGCPtr != sizeof(uint64_t))
    {
        LogRel(("SSM: Incorrect cbGCPtr value: %d\n", pSSM->u.Read.cbGCPtr));
        return VERR_SSM_INTEGRITY_SIZES;
    }

    return VINF_SUCCESS;
}


/**
 * Validates the integrity of a saved state file.
 *
 * @returns VBox status.
 * @param   File                File to validate.
 *                              The file position is undefined on return.
 * @param   fChecksumIt         Whether to checksum the file or not.
 * @param   fChecksumOnRead     Whether to validate the checksum while reading
 *                              the stream instead of up front. If not possible,
 *                              verify the checksum up front.
 * @param   pHdr                Where to store the file header.
 */
static int ssmR3ValidateFile(PSSMHANDLE pSSM, bool fChecksumIt, bool fChecksumOnRead)
{
    /*
     * Read the header.
     */
    union
    {
        SSMFILEHDR          v2_0;
        SSMFILEHDRV12       v1_2;
        SSMFILEHDRV11       v1_1;
    } uHdr;
    int rc = RTFileRead(pSSM->hFile, &uHdr, sizeof(uHdr), NULL);
    if (RT_FAILURE(rc))
    {
        LogRel(("SSM: Failed to read file header. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Verify the magic and make adjustments for versions differences.
     */
    if (memcmp(uHdr.v2_0.szMagic, SSMFILEHDR_MAGIC_BASE, sizeof(SSMFILEHDR_MAGIC_BASE) - 1))
    {
        Log(("SSM: Not a saved state file. magic=%.*s\n", sizeof(uHdr.v2_0.szMagic) - 1, uHdr.v2_0.szMagic));
        return VERR_SSM_INTEGRITY_MAGIC;
    }
    if (memcmp(uHdr.v2_0.szMagic, SSMFILEHDR_MAGIC_V1_X, sizeof(SSMFILEHDR_MAGIC_V1_X) - 1))
    {
        /*
         * Version 2.0 and later.
         */
        bool fChecksummed;
        if (!memcmp(uHdr.v2_0.szMagic, SSMFILEHDR_MAGIC_V2_0, sizeof(SSMFILEHDR_MAGIC_V2_0)))
        {
            /* validate the header. */
            SSM_CHECK_CRC32_RET(&uHdr.v2_0, sizeof(uHdr.v2_0), ("Header CRC mismatch: %08x, correct is %08x\n", u32CRC, u32ActualCRC));
            if (    uHdr.v2_0.u8Reserved
                ||  uHdr.v2_0.u32Reserved)
            {
                LogRel(("SSM: Reserved header fields aren't zero: %02x %08x\n", uHdr.v2_0.u8Reserved, uHdr.v2_0.u32Reserved));
                return VERR_SSM_INTEGRITY;
            }
            if ((uHdr.v2_0.fFlags & ~SSMFILEHDR_FLAGS_STREAM_CRC32))
            {
                LogRel(("SSM: Unknown header flags: %08x\n", uHdr.v2_0.fFlags));
                return VERR_SSM_INTEGRITY;
            }

            /* set the header info. */
            pSSM->u.Read.uFmtVerMajor   = 2;
            pSSM->u.Read.uFmtVerMinor   = 2;
            pSSM->u.Read.cbFileHdr      = sizeof(uHdr.v2_0);
            pSSM->u.Read.cHostBits      = uHdr.v2_0.cHostBits;
            pSSM->u.Read.u16VerMajor    = uHdr.v2_0.u16VerMajor;
            pSSM->u.Read.u16VerMinor    = uHdr.v2_0.u16VerMinor;
            pSSM->u.Read.u32VerBuild    = uHdr.v2_0.u32VerBuild;
            pSSM->u.Read.u32SvnRev      = uHdr.v2_0.u32SvnRev;
            pSSM->u.Read.cbGCPhys       = uHdr.v2_0.cbGCPhys;
            pSSM->u.Read.cbGCPtr        = uHdr.v2_0.cbGCPtr;
            pSSM->u.Read.fFixedGCPtrSize = true;
            fChecksummed = !!(uHdr.v2_0.fFlags & SSMFILEHDR_FLAGS_STREAM_CRC32);
        }
        else
        {
            Log(("SSM: Unknown file format version. magic=%.*s\n", sizeof(uHdr.v2_0.szMagic) - 1, uHdr.v2_0.szMagic));
            return VERR_SSM_INTEGRITY_VERSION;
        }

        /*
         * Read and validate the footer.
         */
        SSMFILEFTR  Footer;
        uint64_t    offFooter;
        rc = RTFileSeek(pSSM->hFile, -(RTFOFF)sizeof(Footer), RTFILE_SEEK_END, &offFooter);
        AssertLogRelRCReturn(rc, rc);
        rc = RTFileRead(pSSM->hFile, &Footer, sizeof(Footer), NULL);
        AssertLogRelRCReturn(rc, rc);
        if (memcmp(Footer.szMagic, SSMFILEFTR_MAGIC, sizeof(Footer.szMagic)))
        {
            LogRel(("SSM: Bad footer magic: %.*Rhxs\n", sizeof(Footer.szMagic), &Footer.szMagic[0]));
            return VERR_SSM_INTEGRITY;
        }
        SSM_CHECK_CRC32_RET(&Footer, sizeof(Footer), ("Footer CRC mismatch: %08x, correct is %08x\n", u32CRC, u32ActualCRC));
        if (Footer.offStream != offFooter)
        {
            LogRel(("SSM: SSMFILEFTR::offStream is wrong: %llx, expected %llx\n", Footer.offStream, offFooter));
            return VERR_SSM_INTEGRITY;
        }
        if (Footer.u32Reserved)
        {
            LogRel(("SSM: Reserved footer field isn't zero: %08x\n", Footer.u32Reserved));
            return VERR_SSM_INTEGRITY;
        }
        if (    !fChecksummed
            &&  Footer.u32StreamCRC)
        {
            LogRel(("SSM: u32StreamCRC field isn't zero, but header says stream checksumming is disabled.\n"));
            return VERR_SSM_INTEGRITY;
        }

        pSSM->u.Read.cbLoadFile = offFooter + sizeof(Footer);
        pSSM->u.Read.u32LoadCRC = Footer.u32StreamCRC;

        /*
         * Validate the header info we've set in the handle.
         */
        rc = ssmR3ValidateHeaderInfo(pSSM, true /*fHaveHostBits*/, true /*fHaveVersion*/);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Check the checksum if that's called for and possible.
         */
        if (    fChecksummed
            &&  fChecksumIt)
        {
            rc = RTFileSeek(pSSM->hFile, 0, RTFILE_SEEK_BEGIN, NULL);
            AssertLogRelRCReturn(rc, rc);
            uint32_t u32CRC;
            rc = ssmR3CalcChecksum(pSSM->hFile, offFooter, &u32CRC);
            if (RT_FAILURE(rc))
                return rc;
            if (u32CRC != pSSM->u.Read.u32LoadCRC)
            {
                LogRel(("SSM: Invalid CRC! Calculated %#010x, in header %#010x\n", u32CRC, pSSM->u.Read.u32LoadCRC));
                return VERR_SSM_INTEGRITY_CRC;
            }
        }
    }
    else
    {
        /*
         * Version 1.x of the format.
         */
        bool        fHaveHostBits = true;
        bool        fHaveVersion  = false;
        RTUUID      MachineUuidFromHdr;

        pSSM->fChecksummed = false;
        pSSM->u.Read.uFmtVerMajor = 1;
        if (!memcmp(uHdr.v2_0.szMagic, SSMFILEHDR_MAGIC_V1_1, sizeof(SSMFILEHDR_MAGIC_V1_1)))
        {
            pSSM->u.Read.uFmtVerMinor   = 1;
            pSSM->u.Read.cbFileHdr      = sizeof(uHdr.v1_1);
            pSSM->u.Read.cHostBits      = 0; /* unknown */
            pSSM->u.Read.u16VerMajor    = 0;
            pSSM->u.Read.u16VerMinor    = 0;
            pSSM->u.Read.u32VerBuild    = 0;
            pSSM->u.Read.u32SvnRev      = 0;
            pSSM->u.Read.cbLoadFile     = uHdr.v1_1.cbFile;
            pSSM->u.Read.u32LoadCRC     = uHdr.v1_1.u32CRC;
            pSSM->u.Read.cbGCPhys       = sizeof(RTGCPHYS);
            pSSM->u.Read.cbGCPtr        = sizeof(RTGCPTR);
            pSSM->u.Read.fFixedGCPtrSize = false; /* settable */

            MachineUuidFromHdr  = uHdr.v1_1.MachineUuid;
            fHaveHostBits       = false;
        }
        else if (!memcmp(uHdr.v2_0.szMagic, SSMFILEHDR_MAGIC_V1_2, sizeof(SSMFILEHDR_MAGIC_V1_2)))
        {
            pSSM->u.Read.uFmtVerMinor   = 2;
            pSSM->u.Read.cbFileHdr      = sizeof(uHdr.v1_2);
            pSSM->u.Read.cHostBits      = uHdr.v1_2.cHostBits;
            pSSM->u.Read.u16VerMajor    = uHdr.v1_2.u16VerMajor;
            pSSM->u.Read.u16VerMinor    = uHdr.v1_2.u16VerMinor;
            pSSM->u.Read.u32VerBuild    = uHdr.v1_2.u32VerBuild;
            pSSM->u.Read.u32SvnRev      = uHdr.v1_2.u32SvnRev;
            pSSM->u.Read.cbLoadFile     = uHdr.v1_2.cbFile;
            pSSM->u.Read.u32LoadCRC     = uHdr.v1_2.u32CRC;
            pSSM->u.Read.cbGCPhys       = uHdr.v1_2.cbGCPhys;
            pSSM->u.Read.cbGCPtr        = uHdr.v1_2.cbGCPtr;
            pSSM->u.Read.fFixedGCPtrSize = true;

            MachineUuidFromHdr  = uHdr.v1_2.MachineUuid;
            fHaveVersion        = true;
        }
        else
        {
            LogRel(("SSM: Unknown file format version. magic=%.*s\n", sizeof(uHdr.v2_0.szMagic) - 1, uHdr.v2_0.szMagic));
            return VERR_SSM_INTEGRITY_VERSION;
        }

        /*
         * The MachineUuid must be NULL (was never used).
         */
        if (!RTUuidIsNull(&MachineUuidFromHdr))
        {
            LogRel(("SSM: The UUID of the saved state doesn't match the running VM.\n"));
            return VERR_SMM_INTEGRITY_MACHINE;
        }

        /*
         * Verify the file size.
         */
        uint64_t cbFile;
        rc = RTFileGetSize(pSSM->hFile, &cbFile);
        AssertLogRelRCReturn(rc, rc);
        if (cbFile != pSSM->u.Read.cbLoadFile)
        {
            LogRel(("SSM: File size mismatch. hdr.cbFile=%lld actual %lld\n", pSSM->u.Read.cbLoadFile, cbFile));
            return VERR_SSM_INTEGRITY_SIZE;
        }

        /*
         * Validate the header info we've set in the handle.
         */
        rc = ssmR3ValidateHeaderInfo(pSSM, fHaveHostBits, fHaveVersion);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Verify the checksum if requested.
         *
         * Note! The checksum is not actually generated for the whole file,
         *       this is of course a bug in the v1.x code that we cannot do
         *       anything about.
         */
        if (    fChecksumIt
            ||  fChecksumOnRead)
        {
            rc = RTFileSeek(pSSM->hFile, RT_OFFSETOF(SSMFILEHDRV11, u32CRC) + sizeof(uHdr.v1_1.u32CRC), RTFILE_SEEK_BEGIN, NULL);
            AssertLogRelRCReturn(rc, rc);
            uint32_t u32CRC;
            rc = ssmR3CalcChecksum(pSSM->hFile, cbFile - pSSM->u.Read.cbFileHdr, &u32CRC);
            if (RT_FAILURE(rc))
                return rc;
            if (u32CRC != pSSM->u.Read.u32LoadCRC)
            {
                LogRel(("SSM: Invalid CRC! Calculated %#010x, in header %#010x\n", u32CRC, pSSM->u.Read.u32LoadCRC));
                return VERR_SSM_INTEGRITY_CRC;
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Open a saved state for reading.
 *
 * The file will be positioned at the first data unit upon successful return.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   pszFilename         The filename.
 * @param   fChecksumIt         Check the checksum for the entire file.
 * @param   fChecksumOnRead     Whether to validate the checksum while reading
 *                              the stream instead of up front. If not possible,
 *                              verify the checksum up front.
 * @param   pSSM                Pointer to the handle structure. This will be
 *                              completely initialized on success.
 */
static int ssmR3OpenFile(PVM pVM, const char *pszFilename, bool fChecksumIt, bool fChecksumOnRead, PSSMHANDLE pSSM)
{
    /*
     * Initialize the handle.
     */
    pSSM->hFile            = NIL_RTFILE;
    pSSM->pVM              = pVM;
    pSSM->enmOp            = SSMSTATE_INVALID;
    pSSM->enmAfter         = SSMAFTER_INVALID;
    pSSM->rc               = VINF_SUCCESS;
    pSSM->cbUnitLeftV1     = 0;
    pSSM->offUnit          = UINT64_MAX;
    pSSM->fUnitChecksummed = 0;
    pSSM->u32UnitCRC       = 0;
    pSSM->pfnProgress      = NULL;
    pSSM->pvUser           = NULL;
    pSSM->uPercent         = 0;
    pSSM->offEstProgress   = 0;
    pSSM->cbEstTotal       = 0;
    pSSM->offEst           = 0;
    pSSM->offEstUnitEnd    = 0;
    pSSM->uPercentPrepare  = 5;
    pSSM->uPercentDone     = 2;
    pSSM->fChecksummed     = fChecksumOnRead;
    pSSM->u32StreamCRC     = 0;

    pSSM->u.Read.pZipDecomp     = NULL;
    pSSM->u.Read.uFmtVerMajor   = UINT32_MAX;
    pSSM->u.Read.uFmtVerMinor   = UINT32_MAX;
    pSSM->u.Read.cbFileHdr      = UINT32_MAX;
    pSSM->u.Read.cbGCPhys       = UINT8_MAX;
    pSSM->u.Read.cbGCPtr        = UINT8_MAX;
    pSSM->u.Read.fFixedGCPtrSize= false;
    pSSM->u.Read.u16VerMajor    = UINT16_MAX;
    pSSM->u.Read.u16VerMinor    = UINT16_MAX;
    pSSM->u.Read.u32VerBuild    = UINT32_MAX;
    pSSM->u.Read.u32SvnRev      = UINT32_MAX;
    pSSM->u.Read.cHostBits      = UINT8_MAX;
    pSSM->u.Read.cbLoadFile     = UINT64_MAX;

    pSSM->u.Read.cbRecLeft      = 0;
    pSSM->u.Read.cbDataBuffer   = 0;
    pSSM->u.Read.offDataBuffer  = 0;
    pSSM->u.Read.fEndOfData     = 0;
    pSSM->u.Read.u8TypeAndFlags = 0;

    /*
     * Try open and validate the file.
     */
    int rc = RTFileOpen(&pSSM->hFile, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        rc = ssmR3ValidateFile(pSSM, fChecksumIt, fChecksumOnRead);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileSeek(pSSM->hFile, pSSM->u.Read.cbFileHdr, RTFILE_SEEK_BEGIN, NULL);
            if (RT_SUCCESS(rc))
                return rc;
        }

        RTFileClose(pSSM->hFile);
    }
    else
        Log(("SSM: Failed to open save state file '%s', rc=%Rrc.\n",  pszFilename, rc));
    pSSM->hFile = NIL_RTFILE;
    return rc;
}


/**
 * Find a data unit by name.
 *
 * @returns Pointer to the unit.
 * @returns NULL if not found.
 *
 * @param   pVM             VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The data unit instance id.
 */
static PSSMUNIT ssmR3Find(PVM pVM, const char *pszName, uint32_t u32Instance)
{
    size_t   cchName = strlen(pszName);
    PSSMUNIT pUnit = pVM->ssm.s.pHead;
    while (     pUnit
           &&   (   pUnit->u32Instance != u32Instance
                 || pUnit->cchName != cchName
                 || memcmp(pUnit->szName, pszName, cchName)))
        pUnit = pUnit->pNext;
    return pUnit;
}


/**
 * Executes the loading of a V1.X file.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pSSM                The saved state handle.
 */
static int ssmR3LoadExecV1(PVM pVM, PSSMHANDLE pSSM)
{
    int     rc;
    char   *pszName = NULL;
    size_t  cchName = 0;
    pSSM->enmOp = SSMSTATE_LOAD_EXEC;
    for (;;)
    {
        /*
         * Save the current file position and read the data unit header.
         */
        uint64_t         offUnit = ssmR3StrmTell(pSSM);
        SSMFILEUNITHDRV1 UnitHdr;
        rc = ssmR3StrmRead(pSSM, &UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName));
        if (RT_SUCCESS(rc))
        {
            /*
             * Check the magic and see if it's valid and whether it is a end header or not.
             */
            if (memcmp(&UnitHdr.achMagic[0], SSMFILEUNITHDR_MAGIC, sizeof(SSMFILEUNITHDR_MAGIC)))
            {
                if (!memcmp(&UnitHdr.achMagic[0], SSMFILEUNITHDR_END, sizeof(SSMFILEUNITHDR_END)))
                {
                    Log(("SSM: EndOfFile: offset %#9llx size %9d\n", offUnit, UnitHdr.cbUnit));
                    /* Complete the progress bar (pending 99% afterwards). */
                    ssmR3Progress(pSSM, pSSM->cbEstTotal - pSSM->offEst);
                    break;
                }
                LogRel(("SSM: Invalid unit magic at offset %#llx (%lld), '%.*s'!\n",
                        offUnit, offUnit, sizeof(UnitHdr.achMagic) - 1, &UnitHdr.achMagic[0]));
                rc = VERR_SSM_INTEGRITY_UNIT_MAGIC;
                break;
            }

            /*
             * Read the name.
             * Adjust the name buffer first.
             */
            if (cchName < UnitHdr.cchName)
            {
                if (pszName)
                    RTMemTmpFree(pszName);
                cchName = RT_ALIGN_Z(UnitHdr.cchName, 64);
                pszName = (char *)RTMemTmpAlloc(cchName);
            }
            if (pszName)
            {
                rc = ssmR3StrmRead(pSSM, pszName, UnitHdr.cchName);
                if (RT_SUCCESS(rc))
                {
                    if (pszName[UnitHdr.cchName - 1])
                    {
                        LogRel(("SSM: Unit name '%.*s' was not properly terminated.\n", UnitHdr.cchName, pszName));
                        rc = VERR_SSM_INTEGRITY;
                        break;
                    }
                    Log(("SSM: Data unit: offset %#9llx size %9lld '%s'\n", offUnit, UnitHdr.cbUnit, pszName));

                    /*
                     * Find the data unit in our internal table.
                     */
                    PSSMUNIT pUnit = ssmR3Find(pVM, pszName, UnitHdr.u32Instance);
                    if (pUnit)
                    {
                        /*
                         * Call the execute handler.
                         */
                        pSSM->cbUnitLeftV1 = UnitHdr.cbUnit - RT_OFFSETOF(SSMFILEUNITHDR, szName[UnitHdr.cchName]);
                        pSSM->offUnit = 0;
                        if (!pUnit->u.Common.pfnLoadExec)
                        {
                            LogRel(("SSM: No load exec callback for unit '%s'!\n", pszName));
                            rc = VERR_SSM_NO_LOAD_EXEC;
                            break;
                        }
                        switch (pUnit->enmType)
                        {
                            case SSMUNITTYPE_DEV:
                                rc = pUnit->u.Dev.pfnLoadExec(pUnit->u.Dev.pDevIns, pSSM, UnitHdr.u32Version);
                                break;
                            case SSMUNITTYPE_DRV:
                                rc = pUnit->u.Drv.pfnLoadExec(pUnit->u.Drv.pDrvIns, pSSM, UnitHdr.u32Version);
                                break;
                            case SSMUNITTYPE_INTERNAL:
                                rc = pUnit->u.Internal.pfnLoadExec(pVM, pSSM, UnitHdr.u32Version);
                                break;
                            case SSMUNITTYPE_EXTERNAL:
                                rc = pUnit->u.External.pfnLoadExec(pSSM, pUnit->u.External.pvUser, UnitHdr.u32Version);
                                break;
                        }

                        /*
                         * Close the reader stream.
                         */
                        ssmR3DataReadFinishV1(pSSM);

                        pUnit->fCalled = true;
                        if (RT_SUCCESS(rc))
                            rc = pSSM->rc;
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Now, we'll check the current position to see if all, or
                             * more than all, the data was read.
                             *
                             * Note! Because of buffering / compression we'll only see the
                             * really bad ones here.
                             */
                            uint64_t off = ssmR3StrmTell(pSSM);
                            int64_t i64Diff = off - (offUnit + UnitHdr.cbUnit);
                            if (i64Diff < 0)
                            {
                                Log(("SSM: Unit '%s' left %lld bytes unread!\n", pszName, -i64Diff));
                                rc = RTFileSeek(pSSM->hFile, offUnit + UnitHdr.cbUnit, RTFILE_SEEK_BEGIN, NULL);
                                ssmR3Progress(pSSM, offUnit + UnitHdr.cbUnit - pSSM->offEst);
                            }
                            else if (i64Diff > 0)
                            {
                                LogRel(("SSM: Unit '%s' read %lld bytes too much!\n", pszName, i64Diff));
                                rc = VERR_SSM_INTEGRITY;
                                break;
                            }

                            pSSM->offUnit = UINT64_MAX;
                        }
                        else
                        {
                            LogRel(("SSM: Load exec failed for '%s' instance #%u ! (version %u)\n",
                                    pszName, UnitHdr.u32Instance, UnitHdr.u32Version));
                            rc = VERR_SSM_INTEGRITY;
                            break;
                        }
                    }
                    else
                    {
                        /*
                         * SSM unit wasn't found - ignore this when loading for the debugger.
                         */
                        LogRel(("SSM: Found no handler for unit '%s'!\n", pszName));
                        rc = VERR_SSM_INTEGRITY_UNIT_NOT_FOUND;
                        if (pSSM->enmAfter != SSMAFTER_DEBUG_IT)
                            break;
                        rc = RTFileSeek(pSSM->hFile, offUnit + UnitHdr.cbUnit, RTFILE_SEEK_BEGIN, NULL);
                    }
                }
            }
            else
                rc = VERR_NO_TMP_MEMORY;
        }

        /*
         * I/O errors ends up here (yea, I know, very nice programming).
         */
        if (RT_FAILURE(rc))
        {
            LogRel(("SSM: I/O error. rc=%Rrc\n", rc));
            break;
        }
    }

    RTMemTmpFree(pszName);
    return rc;
}


/**
 * Executes the loading of a V2.X file.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pSSM                The saved state handle.
 */
static int ssmR3LoadExecV2(PVM pVM, PSSMHANDLE pSSM)
{
    pSSM->enmOp = SSMSTATE_LOAD_EXEC;
    for (;;)
    {
        /*
         * Read the unit header and check its integrity.
         */
        uint64_t        offUnit = ssmR3StrmTell(pSSM);
        SSMFILEUNITHDR  UnitHdr;
        int rc = ssmR3StrmRead(pSSM, &UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName));
        if (RT_FAILURE(rc))
            return rc;
        if (RT_UNLIKELY(    memcmp(&UnitHdr.szMagic[0], SSMFILEUNITHDR_MAGIC, sizeof(UnitHdr.szMagic))
                        &&  memcmp(&UnitHdr.szMagic[0], SSMFILEUNITHDR_END,   sizeof(UnitHdr.szMagic))))
        {
            LogRel(("SSM: Unit at %#llx (%lld): Invalid unit magic: %.*Rhxs!\n",
                    offUnit, offUnit, sizeof(UnitHdr.szMagic) - 1, &UnitHdr.szMagic[0]));
            return VERR_SSM_INTEGRITY_UNIT_MAGIC;
        }
        if (UnitHdr.cbName)
        {
            if (RT_UNLIKELY(UnitHdr.cbName > sizeof(UnitHdr.szName)))
            {
                LogRel(("SSM: Unit at %#llx (%lld): UnitHdr.cbName=%u > %u\n",
                        offUnit, offUnit, UnitHdr.cbName, sizeof(UnitHdr.szName)));
                return VERR_SSM_INTEGRITY;
            }
            rc = ssmR3StrmRead(pSSM, &UnitHdr.szName[0], UnitHdr.cbName);
            if (RT_FAILURE(rc))
                return rc;
            if (RT_UNLIKELY(UnitHdr.szName[UnitHdr.cbName - 1]))
            {
                LogRel(("SSM: Unit at %#llx (%lld): Name %.*Rhxs was not properly terminated.\n",
                        offUnit, offUnit, UnitHdr.cbName, UnitHdr.szName));
                return VERR_SSM_INTEGRITY;
            }
        }
        SSM_CHECK_CRC32_RET(&UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[UnitHdr.cbName]),
                            ("Unit at %#llx (%lld): CRC mismatch: %08x, correct is %08x\n", offUnit, offUnit, u32CRC, u32ActualCRC));
        if (RT_UNLIKELY(UnitHdr.offStream != offUnit))
        {
            LogRel(("SSM: Unit at %#llx (%lld): offStream=%#llx, expected %#llx\n", offUnit, offUnit, UnitHdr.offStream, offUnit));
            return VERR_SSM_INTEGRITY;
        }
        AssertLogRelMsgReturn(!UnitHdr.fFlags, ("Unit at %#llx (%lld): fFlags=%08x\n", offUnit, offUnit, UnitHdr.fFlags), VERR_SSM_INTEGRITY);
        if (!memcmp(&UnitHdr.szMagic[0], SSMFILEUNITHDR_END,   sizeof(UnitHdr.szMagic)))
        {
            AssertLogRelMsgReturn(   UnitHdr.cbName       == 0
                                  && UnitHdr.u32Instance  == 0
                                  && UnitHdr.u32Version   == 0
                                  && UnitHdr.u32Phase     == SSM_PHASE_FINAL,
                                  ("Unit at %#llx (%lld): Malformed END unit\n", offUnit, offUnit),
                                  VERR_SSM_INTEGRITY);

            /*
             * Complete the progress bar (pending 99% afterwards) and RETURN.
             */
            Log(("SSM: Unit at %#9llx: END UNIT\n", offUnit));
            ssmR3Progress(pSSM, pSSM->cbEstTotal - pSSM->offEst);
            return VINF_SUCCESS;
        }
        AssertLogRelMsgReturn(UnitHdr.cbName > 1, ("Unit at %#llx (%lld): No name\n", offUnit, offUnit), VERR_SSM_INTEGRITY);

        Log(("SSM: Unit at %#9llx: '%s', instance %u, phase %#x, version %u\n",
             offUnit, UnitHdr.szName, UnitHdr.u32Instance, UnitHdr.u32Phase, UnitHdr.u32Version));

        /*
         * Find the data unit in our internal table.
         */
        PSSMUNIT pUnit = ssmR3Find(pVM, UnitHdr.szName, UnitHdr.u32Instance);
        if (pUnit)
        {
            /*
             * Call the execute handler.
             */
            AssertLogRelMsgReturn(pUnit->u.Common.pfnLoadExec,
                                  ("SSM: No load exec callback for unit '%s'!\n", UnitHdr.szName),
                                  VERR_SSM_NO_LOAD_EXEC);
            ssmR3DataReadBeginV2(pSSM);
            switch (pUnit->enmType)
            {
                case SSMUNITTYPE_DEV:
                    rc = pUnit->u.Dev.pfnLoadExec(pUnit->u.Dev.pDevIns, pSSM, UnitHdr.u32Version);
                    break;
                case SSMUNITTYPE_DRV:
                    rc = pUnit->u.Drv.pfnLoadExec(pUnit->u.Drv.pDrvIns, pSSM, UnitHdr.u32Version);
                    break;
                case SSMUNITTYPE_INTERNAL:
                    rc = pUnit->u.Internal.pfnLoadExec(pVM, pSSM, UnitHdr.u32Version);
                    break;
                case SSMUNITTYPE_EXTERNAL:
                    rc = pUnit->u.External.pfnLoadExec(pSSM, pUnit->u.External.pvUser, UnitHdr.u32Version);
                    break;
            }
            ssmR3DataReadFinishV2(pSSM);
            pUnit->fCalled = true;
            if (RT_SUCCESS(rc))
                rc = pSSM->rc;
            if (RT_SUCCESS(rc))
                pSSM->offUnit = UINT64_MAX;
            else
            {
                LogRel(("SSM: LoadExec failed for '%s' instance #%u (version %u): %Rrc\n",
                        UnitHdr.szName, UnitHdr.u32Instance, UnitHdr.u32Version, UnitHdr.u32Phase, rc));
                return rc;
            }
        }
        else
        {
            /*
             * SSM unit wasn't found - ignore this when loading for the debugger.
             */
            LogRel(("SSM: Found no handler for unit '%s' instance #%u!\n", UnitHdr.szName, UnitHdr.u32Instance));
            //if (pSSM->enmAfter != SSMAFTER_DEBUG_IT)
                return VERR_SSM_INTEGRITY_UNIT_NOT_FOUND;
            /** @todo Read data unit to /dev/null. */
        }
    }
    /* won't get here */
}




/**
 * Load VM save operation.
 *
 * @returns VBox status.
 *
 * @param   pVM             The VM handle.
 * @param   pszFilename     Name of the file to save the state in.
 * @param   enmAfter        What is planned after a successful load operation.
 *                          Only acceptable values are SSMAFTER_RESUME and SSMAFTER_DEBUG_IT.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 *
 * @thread  EMT
 */
VMMR3DECL(int) SSMR3Load(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("SSMR3Load: pszFilename=%p:{%s} enmAfter=%d pfnProgress=%p pvUser=%p\n", pszFilename, pszFilename, enmAfter, pfnProgress, pvUser));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input.
     */
    if (    enmAfter != SSMAFTER_RESUME
        &&  enmAfter != SSMAFTER_DEBUG_IT)
    {
        AssertMsgFailed(("Invalid enmAfter=%d!\n", enmAfter));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the handle and open the file.
     */
    SSMHANDLE Handle;
    int rc = ssmR3OpenFile(pVM, pszFilename, false /* fChecksumIt */, true /* fChecksumOnRead */, &Handle);
    if (RT_SUCCESS(rc))
    {
        Handle.enmAfter     = enmAfter;
        Handle.pfnProgress  = pfnProgress;
        Handle.pvUser       = pvUser;

        if (Handle.u.Read.u16VerMajor)
            LogRel(("SSM: File header: Format %u.%u, VirtualBox Version %u.%u.%u r%u, %u-bit host, cbGCPhys=%u, cbGCPtr=%u\n",
                    Handle.u.Read.uFmtVerMajor, Handle.u.Read.uFmtVerMinor,
                    Handle.u.Read.u16VerMajor, Handle.u.Read.u16VerMinor, Handle.u.Read.u32VerBuild, Handle.u.Read.u32SvnRev,
                    Handle.u.Read.cHostBits, Handle.u.Read.cbGCPhys, Handle.u.Read.cbGCPtr));
        else
            LogRel(("SSM: File header: Format %u.%u, %u-bit host, cbGCPhys=%u, cbGCPtr=%u\n" ,
                    Handle.u.Read.uFmtVerMajor, Handle.u.Read.uFmtVerMinor,
                    Handle.u.Read.cHostBits, Handle.u.Read.cbGCPhys, Handle.u.Read.cbGCPtr));

        if (pfnProgress)
            pfnProgress(pVM, Handle.uPercent, pvUser);

        /*
         * Clear the per unit flags.
         */
        PSSMUNIT pUnit;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
            pUnit->fCalled = false;

        /*
         * Do the prepare run.
         */
        Handle.rc = VINF_SUCCESS;
        Handle.enmOp = SSMSTATE_LOAD_PREP;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        {
            if (pUnit->u.Common.pfnLoadPrep)
            {
                pUnit->fCalled = true;
                switch (pUnit->enmType)
                {
                    case SSMUNITTYPE_DEV:
                        rc = pUnit->u.Dev.pfnLoadPrep(pUnit->u.Dev.pDevIns, &Handle);
                        break;
                    case SSMUNITTYPE_DRV:
                        rc = pUnit->u.Drv.pfnLoadPrep(pUnit->u.Drv.pDrvIns, &Handle);
                        break;
                    case SSMUNITTYPE_INTERNAL:
                        rc = pUnit->u.Internal.pfnLoadPrep(pVM, &Handle);
                        break;
                    case SSMUNITTYPE_EXTERNAL:
                        rc = pUnit->u.External.pfnLoadPrep(&Handle, pUnit->u.External.pvUser);
                        break;
                }
                if (RT_FAILURE(rc))
                {
                    LogRel(("SSM: Prepare load failed with rc=%Rrc for data unit '%s.\n", rc, pUnit->szName));
                    break;
                }
            }
        }

        /* pending 2% */
        if (pfnProgress)
            pfnProgress(pVM, Handle.uPercentPrepare-1, pvUser);
        Handle.uPercent      = Handle.uPercentPrepare;
        Handle.cbEstTotal    = Handle.u.Read.cbLoadFile;
        Handle.offEstUnitEnd = Handle.u.Read.cbLoadFile;

        /*
         * Do the execute run.
         */
        if (RT_SUCCESS(rc))
        {
            if (Handle.u.Read.uFmtVerMajor >= 2)
                rc = ssmR3LoadExecV2(pVM, &Handle);
            else
                rc = ssmR3LoadExecV1(pVM, &Handle);
            /* (progress should be pending 99% now) */
            AssertMsg(RT_FAILURE(rc) || Handle.uPercent == (101-Handle.uPercentDone), ("%d\n", Handle.uPercent));
        }

        /*
         * Do the done run.
         */
        Handle.rc = rc;
        Handle.enmOp = SSMSTATE_LOAD_DONE;
        for (pUnit = pVM->ssm.s.pHead; pUnit; pUnit = pUnit->pNext)
        {
            if (    pUnit->u.Common.pfnLoadDone
                && (   pUnit->fCalled
                    || (!pUnit->u.Common.pfnLoadPrep && !pUnit->u.Common.pfnLoadExec)))
            {
                rc = VINF_SUCCESS;
                switch (pUnit->enmType)
                {
                    case SSMUNITTYPE_DEV:
                        rc = pUnit->u.Dev.pfnLoadDone(pUnit->u.Dev.pDevIns, &Handle);
                        break;
                    case SSMUNITTYPE_DRV:
                        rc = pUnit->u.Drv.pfnLoadDone(pUnit->u.Drv.pDrvIns, &Handle);
                        break;
                    case SSMUNITTYPE_INTERNAL:
                        rc = pUnit->u.Internal.pfnLoadDone(pVM, &Handle);
                        break;
                    case SSMUNITTYPE_EXTERNAL:
                        rc = pUnit->u.External.pfnLoadDone(&Handle, pUnit->u.External.pvUser);
                        break;
                }
                if (RT_FAILURE(rc))
                {
                    LogRel(("SSM: LoadDone failed with rc=%Rrc for data unit '%s' instance #%u.\n",
                            rc, pUnit->szName, pUnit->u32Instance));
                    if (RT_SUCCESS(Handle.rc))
                        Handle.rc = rc;
                }
            }
        }
        rc = Handle.rc;

        /* progress */
        if (pfnProgress)
            pfnProgress(pVM, 99, pvUser);
    }

    /*
     * Done
     */
    int rc2 = RTFileClose(Handle.hFile);
    AssertRC(rc2);
    if (RT_SUCCESS(rc))
    {
        /* progress */
        if (pfnProgress)
            pfnProgress(pVM, 100, pvUser);
        Log(("SSM: Load of '%s' completed!\n", pszFilename));
    }
    return rc;
}


/**
 * Validates a file as a validate SSM saved state.
 *
 * This will only verify the file format, the format and content of individual
 * data units are not inspected.
 *
 * @returns VINF_SUCCESS if valid.
 * @returns VBox status code on other failures.
 *
 * @param   pszFilename     The path to the file to validate.
 * @param   fChecksumIt     Whether to checksum the file or not.
 *
 * @thread  Any.
 */
VMMR3DECL(int) SSMR3ValidateFile(const char *pszFilename, bool fChecksumIt)
{
    LogFlow(("SSMR3ValidateFile: pszFilename=%p:{%s} fChecksumIt=%RTbool\n", pszFilename, pszFilename, fChecksumIt));

    /*
     * Try open the file and validate it.
     */
    SSMHANDLE Handle;
    int rc = ssmR3OpenFile(NULL, pszFilename, fChecksumIt, false /*fChecksumOnRead*/, &Handle);
    if (RT_SUCCESS(rc))
        RTFileClose(Handle.hFile);
    else
        Log(("SSM: Failed to open saved state file '%s', rc=%Rrc.\n",  pszFilename, rc));
    return rc;
}


/**
 * Opens a saved state file for reading.
 *
 * @returns VBox status code.
 *
 * @param   pszFilename     The path to the saved state file.
 * @param   fFlags          Open flags. Reserved, must be 0.
 * @param   ppSSM           Where to store the SSM handle.
 *
 * @thread  Any.
 */
VMMR3DECL(int) SSMR3Open(const char *pszFilename, unsigned fFlags, PSSMHANDLE *ppSSM)
{
    LogFlow(("SSMR3Open: pszFilename=%p:{%s} fFlags=%#x ppSSM=%p\n", pszFilename, pszFilename, fFlags, ppSSM));

    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszFilename), ("%p\n", pszFilename), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!fFlags, ("%#x\n", fFlags), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(ppSSM), ("%p\n", ppSSM), VERR_INVALID_PARAMETER);

    /*
     * Allocate a handle.
     */
    PSSMHANDLE pSSM = (PSSMHANDLE)RTMemAllocZ(sizeof(*pSSM));
    AssertReturn(pSSM, VERR_NO_MEMORY);

    /*
     * Try open the file and validate it.
     */
    int rc = ssmR3OpenFile(NULL, pszFilename, true /*fChecksumIt*/, false /*fChecksumOnRead*/, pSSM);
    if (RT_SUCCESS(rc))
    {
        pSSM->enmAfter = SSMAFTER_OPENED;
        pSSM->enmOp    = SSMSTATE_OPEN_READ;
        *ppSSM = pSSM;
        LogFlow(("SSMR3Open: returns VINF_SUCCESS *ppSSM=%p\n", *ppSSM));
        return VINF_SUCCESS;
    }

    Log(("SSMR3Open: Failed to open saved state file '%s', rc=%Rrc.\n",  pszFilename, rc));
    RTMemFree(pSSM);
    return rc;

}


/**
 * Closes a saved state file opened by SSMR3Open().
 *
 * @returns VBox status code.
 *
 * @param   pSSM            The SSM handle returned by SSMR3Open().
 *
 * @thread  Any, but the caller is responsible for serializing calls per handle.
 */
VMMR3DECL(int) SSMR3Close(PSSMHANDLE pSSM)
{
    LogFlow(("SSMR3Close: pSSM=%p\n", pSSM));

    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pSSM), ("%p\n", pSSM), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pSSM->enmAfter == SSMAFTER_OPENED, ("%d\n", pSSM->enmAfter),VERR_INVALID_PARAMETER);
    AssertMsgReturn(pSSM->enmOp == SSMSTATE_OPEN_READ, ("%d\n", pSSM->enmOp), VERR_INVALID_PARAMETER);

    /*
     * Close the file and free the handle.
     */
    int rc = RTFileClose(pSSM->hFile);
    AssertRC(rc);
    if (pSSM->u.Read.pZipDecomp)
    {
        RTZipDecompDestroy(pSSM->u.Read.pZipDecomp);
        pSSM->u.Read.pZipDecomp = NULL;
    }
    RTMemFree(pSSM);
    return rc;
}


/**
 * Worker for SSMR3Seek that seeks version 1 saved state files.
 *
 * @returns VBox status code.
 * @param   pSSM                The SSM handle.
 * @param   pszUnit             The unit to seek to.
 * @param   iInstance           The particulart insance we seek.
 * @param   piVersion           Where to store the unit version number.
 */
static int ssmR3FileSeekV1(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion)
{
    /*
     * Walk the data units until we find EOF or a match.
     */
    size_t              cbUnit = strlen(pszUnit) + 1;
    AssertLogRelReturn(cbUnit > SSM_MAX_NAME_SIZE, VERR_SSM_UNIT_NOT_FOUND);
    char                szName[SSM_MAX_NAME_SIZE];
    SSMFILEUNITHDRV1    UnitHdr;
    for (RTFOFF off = pSSM->u.Read.cbFileHdr; ; off += UnitHdr.cbUnit)
    {
        /*
         * Read the unit header and verify it.
         */
        int rc = RTFileReadAt(pSSM->hFile, off, &UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName), NULL);
        AssertRCReturn(rc, rc);
        if (!memcmp(&UnitHdr.achMagic[0], SSMFILEUNITHDR_MAGIC, sizeof(SSMFILEUNITHDR_MAGIC)))
        {
            /*
             * Does what we've got match, if so read the name.
             */
            if (    UnitHdr.u32Instance == iInstance
                &&  UnitHdr.cchName     == cbUnit)
            {
                rc = RTFileRead(pSSM->hFile, szName, cbUnit, NULL);
                AssertRCReturn(rc, rc);
                AssertLogRelMsgReturn(!szName[UnitHdr.cchName - 1],
                                      (" Unit name '%.*s' was not properly terminated.\n", cbUnit, szName),
                                      VERR_SSM_INTEGRITY);

                /*
                 * Does the name match?
                 */
                if (memcmp(szName, pszUnit, cbUnit))
                {
                    pSSM->cbUnitLeftV1 = UnitHdr.cbUnit - RT_OFFSETOF(SSMFILEUNITHDR, szName[cbUnit]);
                    pSSM->offUnit = 0;
                    if (piVersion)
                        *piVersion = UnitHdr.u32Version;
                    return VINF_SUCCESS;
                }
            }
        }
        else if (!memcmp(&UnitHdr.achMagic[0], SSMFILEUNITHDR_END, sizeof(SSMFILEUNITHDR_END)))
            return VERR_SSM_UNIT_NOT_FOUND;
        else
            AssertLogRelMsgFailedReturn(("Invalid unit magic at offset %RTfoff, '%.*s'!\n",
                                         off, sizeof(UnitHdr.achMagic) - 1, &UnitHdr.achMagic[0]),
                                        VERR_SSM_INTEGRITY_UNIT_MAGIC);
    }
    /* won't get here. */
}


/**
 * Worker for ssmR3FileSeekV2 for simplifying memory cleanup.
 *
 * @returns VBox status code.
 * @param   pSSM                The SSM handle.
 * @param   pszUnit             The unit to seek to.
 * @param   iInstance           The particulart insance we seek.
 * @param   piVersion           Where to store the unit version number.
 */
static int ssmR3FileSeekSubV2(PSSMHANDLE pSSM, PSSMFILEDIR pDir, size_t cbDir, uint32_t cDirEntries,
                              const char *pszUnit, uint32_t iInstance, uint32_t *piVersion)
{
    /*
     * Read it.
     */
    uint64_t const offDir = pSSM->u.Read.cbLoadFile - sizeof(SSMFILEFTR) - cbDir;
    int rc = RTFileReadAt(pSSM->hFile, offDir, pDir, cbDir, NULL);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelReturn(!memcmp(pDir->szMagic, SSMFILEDIR_MAGIC, sizeof(pDir->szMagic)), VERR_SSM_INTEGRITY);
    SSM_CHECK_CRC32_RET(pDir, cbDir, ("Bad directory CRC: %08x, actual %08x\n", u32CRC, u32ActualCRC));
    AssertLogRelMsgReturn(pDir->cEntries == cDirEntries,
                          ("Bad directory entry count: %#x, expected %#x (from the footer)\n", pDir->cEntries, cDirEntries),
                          VERR_SSM_INTEGRITY);
    for (uint32_t i = 0; i < cDirEntries; i++)
        AssertLogRelMsgReturn(pDir->aEntries[i].off < offDir,
                              ("i=%u off=%lld offDir=%lld\n", i, pDir->aEntries[i].off, offDir),
                              VERR_SSM_INTEGRITY);

    /*
     * Search the directory.
     */
    size_t          cbUnit     = strlen(pszUnit) + 1;
    uint32_t const  u32NameCRC = RTCrc32(pszUnit, cbUnit - 1);
    for (uint32_t i = 0; i < cDirEntries; i++)
    {
        if (    pDir->aEntries[i].u32NameCRC  == u32NameCRC
            &&  pDir->aEntries[i].u32Instance == iInstance)
        {
            /*
             * Read and validate the unit header.
             */
            SSMFILEUNITHDR  UnitHdr;
            size_t          cbToRead = sizeof(UnitHdr);
            if (pDir->aEntries[i].off + cbToRead > offDir)
            {
                cbToRead = offDir - pDir->aEntries[i].off;
                RT_ZERO(UnitHdr);
            }
            rc = RTFileReadAt(pSSM->hFile, pDir->aEntries[i].off, &UnitHdr, cbToRead, NULL);
            AssertLogRelRCReturn(rc, rc);

            AssertLogRelMsgReturn(!memcmp(UnitHdr.szMagic, SSMFILEUNITHDR_MAGIC, sizeof(UnitHdr.szMagic)),
                                  ("Bad unit header or dictionary offset: i=%u off=%lld\n", i, pDir->aEntries[i].off),
                                  VERR_SSM_INTEGRITY);
            AssertLogRelMsgReturn(UnitHdr.offStream == pDir->aEntries[i].off,
                                  ("Bad unit header: i=%d off=%lld offStream=%lld\n", i, pDir->aEntries[i].off, UnitHdr.offStream),
                                  VERR_SSM_INTEGRITY);
            AssertLogRelMsgReturn(UnitHdr.u32Instance == pDir->aEntries[i].u32Instance,
                                  ("Bad unit header: i=%d off=%lld u32Instance=%u Dir.u32Instance=%u\n",
                                   i, pDir->aEntries[i].off, UnitHdr.u32Instance, pDir->aEntries[i].u32Instance),
                                  VERR_SSM_INTEGRITY);
            uint32_t cbUnitHdr = RT_UOFFSETOF(SSMFILEUNITHDR, szName[UnitHdr.cbName]);
            AssertLogRelMsgReturn(   UnitHdr.cbName > 0
                                  && UnitHdr.cbName < sizeof(UnitHdr)
                                  && cbUnitHdr <= cbToRead,
                                  ("Bad unit header: i=%u off=%lld cbName=%#x cbToRead=%#x\n", i, pDir->aEntries[i].off, UnitHdr.cbName, cbToRead),
                                  VERR_SSM_INTEGRITY);
            SSM_CHECK_CRC32_RET(&UnitHdr, RT_OFFSETOF(SSMFILEUNITHDR, szName[UnitHdr.cbName]),
                                ("Bad unit header CRC: i=%u off=%lld u32CRC=%#x u32ActualCRC=%#x\n",
                                 i, pDir->aEntries[i].off, u32CRC, u32ActualCRC));

            /*
             * Ok, it is valid, get on with the comparing now.
             */
            if (    UnitHdr.cbName == cbUnit
                &&  !memcmp(UnitHdr.szName, pszUnit, cbUnit))
            {
                if (piVersion)
                    *piVersion = UnitHdr.u32Version;
                rc = RTFileSeek(pSSM->hFile, pDir->aEntries[i].off + cbUnitHdr, RTFILE_SEEK_BEGIN, NULL);
                AssertLogRelRCReturn(rc, rc);

                ssmR3DataReadBeginV2(pSSM);
                return VINF_SUCCESS;
            }
        }
    }

    return VERR_SSM_UNIT_NOT_FOUND;
}


/**
 * Worker for SSMR3Seek that seeks version 2 saved state files.
 *
 * @returns VBox status code.
 * @param   pSSM                The SSM handle.
 * @param   pszUnit             The unit to seek to.
 * @param   iInstance           The particulart insance we seek.
 * @param   piVersion           Where to store the unit version number.
 */
static int ssmR3FileSeekV2(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion)
{
    /*
     * Read the footer, allocate a temporary buffer for the dictionary and
     * pass it down to a worker to simplify cleanup.
     */
    SSMFILEFTR      Footer;
    int rc = RTFileReadAt(pSSM->hFile, pSSM->u.Read.cbLoadFile - sizeof(Footer), &Footer, sizeof(Footer), NULL);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelReturn(!memcmp(Footer.szMagic, SSMFILEFTR_MAGIC, sizeof(Footer.szMagic)), VERR_SSM_INTEGRITY);
    SSM_CHECK_CRC32_RET(&Footer, sizeof(Footer), ("Bad footer CRC: %08x, actual %08x\n", u32CRC, u32ActualCRC));

    size_t const    cbDir = RT_OFFSETOF(SSMFILEDIR, aEntries[Footer.cDirEntries]);
    PSSMFILEDIR     pDir  = (PSSMFILEDIR)RTMemTmpAlloc(cbDir);
    if (RT_UNLIKELY(!pDir))
        return VERR_NO_TMP_MEMORY;
    rc = ssmR3FileSeekSubV2(pSSM, pDir, cbDir, Footer.cDirEntries, pszUnit, iInstance, piVersion);
    RTMemTmpFree(pDir);

    return rc;
}



/**
 * Seeks to a specific data unit.
 *
 * After seeking it's possible to use the getters to on
 * that data unit.
 *
 * @returns VBox status code.
 * @returns VERR_SSM_UNIT_NOT_FOUND if the unit+instance wasn't found.
 *
 * @param   pSSM            The SSM handle returned by SSMR3Open().
 * @param   pszUnit         The name of the data unit.
 * @param   iInstance       The instance number.
 * @param   piVersion       Where to store the version number. (Optional)
 *
 * @thread  Any, but the caller is responsible for serializing calls per handle.
 */
VMMR3DECL(int) SSMR3Seek(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion)
{
    LogFlow(("SSMR3Seek: pSSM=%p pszUnit=%p:{%s} iInstance=%RU32 piVersion=%p\n",
             pSSM, pszUnit, pszUnit, iInstance, piVersion));

    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pSSM), ("%p\n", pSSM), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pSSM->enmAfter == SSMAFTER_OPENED, ("%d\n", pSSM->enmAfter),VERR_INVALID_PARAMETER);
    AssertMsgReturn(pSSM->enmOp == SSMSTATE_OPEN_READ, ("%d\n", pSSM->enmOp), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pszUnit), ("%p\n", pszUnit), VERR_INVALID_POINTER);
    AssertMsgReturn(!piVersion || VALID_PTR(piVersion), ("%p\n", piVersion), VERR_INVALID_POINTER);

    /*
     * Reset the state.
     */
    if (pSSM->u.Read.pZipDecomp)
    {
        RTZipDecompDestroy(pSSM->u.Read.pZipDecomp);
        pSSM->u.Read.pZipDecomp = NULL;
    }
    pSSM->cbUnitLeftV1  = 0;
    pSSM->offUnit       = UINT64_MAX;

    /*
     * Call the version specific workers.
     */
    if (pSSM->u.Read.uFmtVerMajor >= 2)
        pSSM->rc = ssmR3FileSeekV2(pSSM, pszUnit, iInstance, piVersion);
    else
        pSSM->rc = ssmR3FileSeekV1(pSSM, pszUnit, iInstance, piVersion);
    return pSSM->rc;
}


/**
 * Finishes a data unit.
 * All buffers and compressor instances are flushed and destroyed.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 */
static int ssmR3DataWriteFinish(PSSMHANDLE pSSM)
{
    //Log2(("ssmR3DataWriteFinish: %#010llx start\n", RTFileTell(pSSM->File)));
    int rc = ssmR3DataFlushBuffer(pSSM);
    if (RT_SUCCESS(rc))
    {
        if (!pSSM->u.Write.pZipComp)
            return VINF_SUCCESS;

        int rc = RTZipCompFinish(pSSM->u.Write.pZipComp);
        if (RT_SUCCESS(rc))
        {
            rc = RTZipCompDestroy(pSSM->u.Write.pZipComp);
            if (RT_SUCCESS(rc))
            {
                pSSM->u.Write.pZipComp = NULL;
                //Log2(("ssmR3DataWriteFinish: %#010llx done\n", RTFileTell(pSSM->File)));
                return VINF_SUCCESS;
            }
        }
    }

    if (RT_SUCCESS(pSSM->rc))
        pSSM->rc = rc;
    Log2(("ssmR3DataWriteFinish: failure rc=%Rrc\n", rc));
    return rc;
}


/**
 * Callback for flusing the output buffer of a compression stream.
 *
 * @returns VBox status.
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           Compressed data.
 * @param   cbBuf           Size of the compressed data.
 */
static DECLCALLBACK(int) ssmR3WriteOut(void *pvSSM, const void *pvBuf, size_t cbBuf)
{
    return ssmR3StrmWrite((PSSMHANDLE)pvSSM, pvBuf, cbBuf);
}


/**
 * Begins writing the data of a data unit.
 *
 * Errors are signalled via pSSM->rc.
 *
 * @param   pSSM            The saved state handle.
 */
static void ssmR3DataWriteBegin(PSSMHANDLE pSSM)
{
    Assert(!pSSM->u.Write.pZipComp);

    pSSM->u.Write.pZipComp  = NULL;
    pSSM->offUnit           = 0;
    if (pSSM->fUnitChecksummed)
        pSSM->u32UnitCRC    = RTCrc32Start();

    //int rc = RTZipCompCreate(&pSSM->u.Write.pZipComp, pSSM, ssmR3WriteOut, RTZIPTYPE_ZLIB, RTZIPLEVEL_FAST);
    int rc = RTZipCompCreate(&pSSM->u.Write.pZipComp, pSSM, ssmR3WriteOut, RTZIPTYPE_LZF, RTZIPLEVEL_FAST);
    //int rc = RTZipCompCreate(&pSSM->u.Write.pZipComp, pSSM, ssmR3WriteOut, RTZIPTYPE_STORE, RTZIPLEVEL_FAST);
    if (RT_FAILURE(rc) && RT_SUCCESS(pSSM->rc))
        pSSM->rc = rc;
}


/**
 * Writes a record to the current data item in the saved state file.
 *
 * @returns VBox status code. Sets pSSM->rc on failure.
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           The bits to write.
 * @param   cbBuf           The number of bytes to write.
 */
static int ssmR3DataWriteRaw(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf)
{
    Log2(("ssmR3DataWriteRaw: %08llx: pvBuf=%p cbBuf=%#x %.*Rhxs%s\n", pSSM->offUnit, pvBuf, cbBuf, RT_MIN(cbBuf, SSM_LOG_BYTES), pvBuf, cbBuf > SSM_LOG_BYTES ? "..." : ""));

    /*
     * Check that everything is fine.
     */
    if (RT_FAILURE(pSSM->rc))
        return pSSM->rc;

    /*
     * Do the stream checksumming before we write the data.
     */
    if (pSSM->fUnitChecksummed)
        pSSM->u32UnitCRC = RTCrc32Process(pSSM->u32UnitCRC, pvBuf, cbBuf);

    /*
     * Write the data item in 1MB chunks for progress indicator reasons.
     */
    while (cbBuf > 0)
    {
        size_t cbChunk = RT_MIN(cbBuf, _1M);
        int rc = pSSM->rc = RTZipCompress(pSSM->u.Write.pZipComp, pvBuf, cbChunk);
        if (RT_FAILURE(rc))
            return rc;
        ssmR3Progress(pSSM, cbChunk);
        pSSM->offUnit += cbChunk;
        cbBuf -= cbChunk;
        pvBuf = (char *)pvBuf + cbChunk;
    }

    return VINF_SUCCESS;
}

/**
 * Writes a record header for the specified amount of data.
 *
 * @returns VBox status code. Sets pSSM->rc on failure.
 * @param   pSSM            The saved state handle
 * @param   cb              The amount of data.
 * @param   u8TypeAndFlags  The record type and flags.
 */
static int ssmR3DataWriteRecHdr(PSSMHANDLE pSSM, size_t cb, uint8_t u8TypeAndFlags)
{
    size_t  cbHdr;
    uint8_t abHdr[8];
    abHdr[0] = u8TypeAndFlags;
    if (cb < 0x80)
    {
        cbHdr = 2;
        abHdr[1] = cb;
    }
    else if (cb < 0x00000800)
    {
        cbHdr = 3;
        abHdr[1] = 0xc0 | (cb >> 6);
        abHdr[2] = 0x80 | (cb & 0x3f);
    }
    else if (cb < 0x00010000)
    {
        cbHdr = 4;
        abHdr[1] = 0xe0 | (cb >> 12);
        abHdr[2] = 0x80 | ((cb >> 6) & 0x3f);
        abHdr[3] = 0x80 | (cb & 0x3f);
    }
    else if (cb < 0x00200000)
    {
        cbHdr = 5;
        abHdr[1] = 0xf0 |  (cb >> 18);
        abHdr[2] = 0x80 | ((cb >> 12) & 0x3f);
        abHdr[3] = 0x80 | ((cb >>  6) & 0x3f);
        abHdr[4] = 0x80 |  (cb        & 0x3f);
    }
    else if (cb < 0x04000000)
    {
        cbHdr = 6;
        abHdr[1] = 0xf8 |  (cb >> 24);
        abHdr[2] = 0x80 | ((cb >> 18) & 0x3f);
        abHdr[3] = 0x80 | ((cb >> 12) & 0x3f);
        abHdr[4] = 0x80 | ((cb >>  6) & 0x3f);
        abHdr[5] = 0x80 |  (cb        & 0x3f);
    }
    else if (cb <= 0x7fffffff)
    {
        cbHdr = 7;
        abHdr[1] = 0xfc | (cb >> 30);
        abHdr[2] = 0x80 | ((cb >> 24) & 0x3f);
        abHdr[3] = 0x80 | ((cb >> 18) & 0x3f);
        abHdr[4] = 0x80 | ((cb >> 12) & 0x3f);
        abHdr[5] = 0x80 | ((cb >> 6) & 0x3f);
        abHdr[6] = 0x80 | (cb & 0x3f);
    }
    else
        AssertLogRelMsgFailedReturn(("cb=%#x\n", cb), pSSM->rc = VERR_INTERNAL_ERROR);

    Log3(("ssmR3DataWriteRecHdr: %08llx/%08x: Type=%02x fImportant=%RTbool cbHdr=%u\n",
          pSSM->offUnit + cbHdr, cb, u8TypeAndFlags & SSM_REC_TYPE_MASK, !!(u8TypeAndFlags & SSM_REC_FLAGS_IMPORTANT), cbHdr));

    return ssmR3DataWriteRaw(pSSM, &abHdr[0], cbHdr);
}


/**
 * Worker that flushes the buffered data.
 *
 * @returns VBox status code. Will set pSSM->rc on error.
 * @param   pSSM            The saved state handle.
 */
static int ssmR3DataFlushBuffer(PSSMHANDLE pSSM)
{
    uint32_t cb = pSSM->u.Write.offDataBuffer;
    if (!cb)
        return pSSM->rc;

    /*
     * Create a record header.
     */
    uint32_t    cbTotal = cb + 1;
    uint8_t    *pbHdr   = &pSSM->u.Write.abRecHdr[sizeof(pSSM->u.Write.abRecHdr) - 1];
    if (cb < 0x80)
    {
        cbTotal += 1;
        pbHdr   -= 1;
        pbHdr[1] = cb;
    }
    else if (cb < 0x00000800)
    {
        cbTotal += 2;
        pbHdr   -= 2;
        pbHdr[1] = 0xc0 | (cb >> 6);
        pbHdr[2] = 0x80 | (cb & 0x3f);
    }
    else if (cb < 0x00010000)
    {
        cbTotal += 3;
        pbHdr   -= 3;
        pbHdr[1] = 0xe0 | (cb >> 12);
        pbHdr[2] = 0x80 | ((cb >> 6) & 0x3f);
        pbHdr[3] = 0x80 | (cb & 0x3f);
    }
    else if (cb < 0x00200000)
    {
        cbTotal += 4;
        pbHdr   -= 4;
        pbHdr[1] = 0xf0 | (cb >> 18);
        pbHdr[2] = 0x80 | ((cb >> 12) & 0x3f);
        pbHdr[3] = 0x80 | ((cb >> 6) & 0x3f);
        pbHdr[4] = 0x80 | (cb & 0x3f);
    }
    else if (cb < 0x04000000)
    {
        cbTotal += 5;
        pbHdr   -= 5;
        pbHdr[1] = 0xf8 | (cb >> 24);
        pbHdr[2] = 0x80 | ((cb >> 18) & 0x3f);
        pbHdr[3] = 0x80 | ((cb >> 12) & 0x3f);
        pbHdr[4] = 0x80 | ((cb >> 6) & 0x3f);
        pbHdr[5] = 0x80 | (cb & 0x3f);
    }
    else if (cb <= 0x7fffffff)
    {
        cbTotal += 6;
        pbHdr   -= 6;
        pbHdr[1] = 0xfc | (cb >> 30);
        pbHdr[2] = 0x80 | ((cb >> 24) & 0x3f);
        pbHdr[3] = 0x80 | ((cb >> 18) & 0x3f);
        pbHdr[4] = 0x80 | ((cb >> 12) & 0x3f);
        pbHdr[5] = 0x80 | ((cb >> 6) & 0x3f);
        pbHdr[6] = 0x80 | (cb & 0x3f);
    }
    else
        AssertLogRelMsgFailedReturn(("cb=%#x\n", cb), VERR_INTERNAL_ERROR);
    pbHdr[0] = SSM_REC_FLAGS_FIXED | SSM_REC_FLAGS_IMPORTANT | SSM_REC_TYPE_RAW;

    Log3(("ssmR3DataFlushBuffer: %08llx/%08x: Type=RAW fImportant=true cbHdr=%u\n",
          pSSM->offUnit + (cbTotal - cb), cb, (cbTotal - cb) ));

    /*
     * Write it and reset the buffer.
     */
    int rc = ssmR3DataWriteRaw(pSSM, pbHdr, cbTotal);
    pSSM->u.Write.offDataBuffer = 0;
    return rc;
}


/**
 * ssmR3DataWrite worker that writes big stuff.
 *
 * @returns VBox status code
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           The bits to write.
 * @param   cbBuf           The number of bytes to write.
 */
static int ssmR3DataWriteBig(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf)
{
    int rc = ssmR3DataFlushBuffer(pSSM);
    if (RT_SUCCESS(rc))
    {
        rc = ssmR3DataWriteRecHdr(pSSM, cbBuf, SSM_REC_FLAGS_FIXED | SSM_REC_FLAGS_IMPORTANT | SSM_REC_TYPE_RAW);
        if (RT_SUCCESS(rc))
            rc = ssmR3DataWriteRaw(pSSM, pvBuf, cbBuf);
    }
    return rc;
}

/**
 * ssmR3DataWrite worker that is called when there isn't enough room in the
 * buffer for the current chunk of data.
 *
 * This will first flush the buffer and then add the new bits to it.
 *
 * @returns VBox status code
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           The bits to write.
 * @param   cbBuf           The number of bytes to write.
 */
static int ssmR3DataWriteFlushAndBuffer(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf)
{
    int rc = ssmR3DataFlushBuffer(pSSM);
    if (RT_SUCCESS(rc))
    {
        memcpy(&pSSM->u.Write.abDataBuffer[0], pvBuf, cbBuf);
        pSSM->u.Write.offDataBuffer = cbBuf;
    }
    return rc;
}


/**
 * Writes data to the current data unit.
 *
 * This is an inlined wrapper that optimizes the small writes that so many of
 * the APIs make.
 *
 * @returns VBox status code
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           The bits to write.
 * @param   cbBuf           The number of bytes to write.
 */
DECLINLINE(int) ssmR3DataWrite(PSSMHANDLE pSSM, const void *pvBuf, size_t cbBuf)
{
    if (cbBuf > sizeof(pSSM->u.Write.abDataBuffer) / 8)
        return ssmR3DataWriteBig(pSSM, pvBuf, cbBuf);
    if (!cbBuf)
        return VINF_SUCCESS;

    uint32_t off = pSSM->u.Write.offDataBuffer;
    if (RT_UNLIKELY(cbBuf + off > sizeof(pSSM->u.Write.abDataBuffer)))
        return ssmR3DataWriteFlushAndBuffer(pSSM, pvBuf, cbBuf);

    memcpy(&pSSM->u.Write.abDataBuffer[off], pvBuf, cbBuf);
    pSSM->u.Write.offDataBuffer = off + cbBuf;
    return VINF_SUCCESS;
}


/**
 * Puts a structure.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 * @param   pvStruct        The structure address.
 * @param   paFields        The array of structure fields descriptions.
 *                          The array must be terminated by a SSMFIELD_ENTRY_TERM().
 */
VMMR3DECL(int) SSMR3PutStruct(PSSMHANDLE pSSM, const void *pvStruct, PCSSMFIELD paFields)
{
    /* begin marker. */
    int rc = SSMR3PutU32(pSSM, SSMR3STRUCT_BEGIN);
    if (RT_FAILURE(rc))
        return rc;

    /* put the fields */
    for (PCSSMFIELD pCur = paFields;
         pCur->cb != UINT32_MAX && pCur->off != UINT32_MAX;
         pCur++)
    {
        rc = ssmR3DataWrite(pSSM, (uint8_t *)pvStruct + pCur->off, pCur->cb);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* end marker */
    return SSMR3PutU32(pSSM, SSMR3STRUCT_END);
}


/**
 * Saves a boolean item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   fBool           Item to save.
 */
VMMR3DECL(int) SSMR3PutBool(PSSMHANDLE pSSM, bool fBool)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
    {
        uint8_t u8 = fBool; /* enforce 1 byte size */
        return ssmR3DataWrite(pSSM, &u8, sizeof(u8));
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 8-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u8              Item to save.
 */
VMMR3DECL(int) SSMR3PutU8(PSSMHANDLE pSSM, uint8_t u8)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u8, sizeof(u8));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 8-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i8              Item to save.
 */
VMMR3DECL(int) SSMR3PutS8(PSSMHANDLE pSSM, int8_t i8)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i8, sizeof(i8));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 16-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u16             Item to save.
 */
VMMR3DECL(int) SSMR3PutU16(PSSMHANDLE pSSM, uint16_t u16)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u16, sizeof(u16));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 16-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i16             Item to save.
 */
VMMR3DECL(int) SSMR3PutS16(PSSMHANDLE pSSM, int16_t i16)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i16, sizeof(i16));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 32-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u32             Item to save.
 */
VMMR3DECL(int) SSMR3PutU32(PSSMHANDLE pSSM, uint32_t u32)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u32, sizeof(u32));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 32-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i32             Item to save.
 */
VMMR3DECL(int) SSMR3PutS32(PSSMHANDLE pSSM, int32_t i32)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i32, sizeof(i32));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 64-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u64             Item to save.
 */
VMMR3DECL(int) SSMR3PutU64(PSSMHANDLE pSSM, uint64_t u64)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u64, sizeof(u64));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 64-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i64             Item to save.
 */
VMMR3DECL(int) SSMR3PutS64(PSSMHANDLE pSSM, int64_t i64)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i64, sizeof(i64));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 128-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u128            Item to save.
 */
VMMR3DECL(int) SSMR3PutU128(PSSMHANDLE pSSM, uint128_t u128)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u128, sizeof(u128));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 128-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i128            Item to save.
 */
VMMR3DECL(int) SSMR3PutS128(PSSMHANDLE pSSM, int128_t i128)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i128, sizeof(i128));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a VBox unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 */
VMMR3DECL(int) SSMR3PutUInt(PSSMHANDLE pSSM, RTUINT u)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u, sizeof(u));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a VBox signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i               Item to save.
 */
VMMR3DECL(int) SSMR3PutSInt(PSSMHANDLE pSSM, RTINT i)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &i, sizeof(i));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a GC natural unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 *
 * @deprecated Silly type, don't use it.
 */
VMMR3DECL(int) SSMR3PutGCUInt(PSSMHANDLE pSSM, RTGCUINT u)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u, sizeof(u));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a GC unsigned integer register item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 */
VMMR3DECL(int) SSMR3PutGCUIntReg(PSSMHANDLE pSSM, RTGCUINTREG u)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &u, sizeof(u));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 32 bits GC physical address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPhys          The item to save
 */
VMMR3DECL(int) SSMR3PutGCPhys32(PSSMHANDLE pSSM, RTGCPHYS32 GCPhys)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &GCPhys, sizeof(GCPhys));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a 64 bits GC physical address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPhys          The item to save
 */
VMMR3DECL(int) SSMR3PutGCPhys64(PSSMHANDLE pSSM, RTGCPHYS64 GCPhys)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &GCPhys, sizeof(GCPhys));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a GC physical address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPhys          The item to save
 */
VMMR3DECL(int) SSMR3PutGCPhys(PSSMHANDLE pSSM, RTGCPHYS GCPhys)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &GCPhys, sizeof(GCPhys));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a GC virtual address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPtr           The item to save.
 */
VMMR3DECL(int) SSMR3PutGCPtr(PSSMHANDLE pSSM, RTGCPTR GCPtr)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &GCPtr, sizeof(GCPtr));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves an RC virtual address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   RCPtr           The item to save.
 */
VMMR3DECL(int) SSMR3PutRCPtr(PSSMHANDLE pSSM, RTRCPTR RCPtr)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &RCPtr, sizeof(RCPtr));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a GC virtual address (represented as an unsigned integer) item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPtr           The item to save.
 */
VMMR3DECL(int) SSMR3PutGCUIntPtr(PSSMHANDLE pSSM, RTGCUINTPTR GCPtr)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &GCPtr, sizeof(GCPtr));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a I/O port address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   IOPort          The item to save.
 */
VMMR3DECL(int) SSMR3PutIOPort(PSSMHANDLE pSSM, RTIOPORT IOPort)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &IOPort, sizeof(IOPort));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a selector item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   Sel             The item to save.
 */
VMMR3DECL(int) SSMR3PutSel(PSSMHANDLE pSSM, RTSEL Sel)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, &Sel, sizeof(Sel));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a memory item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pv              Item to save.
 * @param   cb              Size of the item.
 */
VMMR3DECL(int) SSMR3PutMem(PSSMHANDLE pSSM, const void *pv, size_t cb)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
        return ssmR3DataWrite(pSSM, pv, cb);
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Saves a zero terminated string item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Item to save.
 */
VMMR3DECL(int) SSMR3PutStrZ(PSSMHANDLE pSSM, const char *psz)
{
    if (pSSM->enmOp == SSMSTATE_SAVE_EXEC)
    {
        size_t cch = strlen(psz);
        if (cch > _1M)
        {
            AssertMsgFailed(("a %d byte long string, what's this!?!\n"));
            return VERR_TOO_MUCH_DATA;
        }
        uint32_t u32 = (uint32_t)cch;
        int rc = ssmR3DataWrite(pSSM, &u32, sizeof(u32));
        if (rc)
            return rc;
        return ssmR3DataWrite(pSSM, psz, cch);
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}





/**
 * Closes the decompressor of a data unit.
 *
 * @param   pSSM            SSM operation handle.
 */
static void ssmR3DataReadFinishV1(PSSMHANDLE pSSM)
{
    if (pSSM->u.Read.pZipDecomp)
    {
        int rc = RTZipDecompDestroy(pSSM->u.Read.pZipDecomp);
        AssertRC(rc);
        pSSM->u.Read.pZipDecomp = NULL;
    }
}


/**
 * Callback for reading compressed data into the input buffer of the
 * decompressor, for saved file format version 1.
 *
 * @returns VBox status code.
 * @param   pvSSM       The SSM handle.
 * @param   pvBuf       Where to store the compressed data.
 * @param   cbBuf       Size of the buffer.
 * @param   pcbRead     Number of bytes actually stored in the buffer.
 */
static DECLCALLBACK(int) ssmR3ReadInV1(void *pvSSM, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    PSSMHANDLE  pSSM   = (PSSMHANDLE)pvSSM;
    size_t      cbRead = cbBuf;
    if (pSSM->cbUnitLeftV1 < cbBuf)
        cbRead = (size_t)pSSM->cbUnitLeftV1;
    if (cbRead)
    {
        //Log2(("ssmR3ReadInV1: %#010llx cbBug=%#x cbRead=%#x\n", RTFileTell(pSSM->File), cbBuf, cbRead));
        int rc = ssmR3StrmRead(pSSM, pvBuf, cbRead);
        if (RT_SUCCESS(rc))
        {
            pSSM->cbUnitLeftV1 -= cbRead;
            if (pcbRead)
                *pcbRead = cbRead;
            ssmR3Progress(pSSM, cbRead);
            return VINF_SUCCESS;
        }
        return rc;
    }

    /** @todo weed out lazy saving */
    if (pSSM->enmAfter != SSMAFTER_DEBUG_IT)
        AssertMsgFailed(("SSM: attempted reading more than the unit!\n"));
    return VERR_SSM_LOADED_TOO_MUCH;
}


/**
 * Internal read worker for reading data from a version 1 unit.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvBuf           Where to store the read data.
 * @param   cbBuf           Number of bytes to read.
 */
static int ssmR3DataReadV1(PSSMHANDLE pSSM, void *pvBuf, size_t cbBuf)
{
    /*
     * Open the decompressor on the first read.
     */
    if (!pSSM->u.Read.pZipDecomp)
    {
        pSSM->rc = RTZipDecompCreate(&pSSM->u.Read.pZipDecomp, pSSM, ssmR3ReadInV1);
        if (RT_FAILURE(pSSM->rc))
            return pSSM->rc;
    }

    /*
     * Do the requested read.
     */
    int rc = pSSM->rc = RTZipDecompress(pSSM->u.Read.pZipDecomp, pvBuf, cbBuf, NULL);
    if (RT_SUCCESS(rc))
    {
        Log2(("ssmR3DataRead: pvBuf=%p cbBuf=%#x offUnit=%#llx %.*Rhxs%s\n", pvBuf, cbBuf, pSSM->offUnit, RT_MIN(cbBuf, SSM_LOG_BYTES), pvBuf, cbBuf > SSM_LOG_BYTES ? "..." : ""));
        pSSM->offUnit += cbBuf;
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("rc=%Rrc cbBuf=%#x\n", rc, cbBuf));
    return rc;
}


/**
 * Callback for reading compressed data into the input buffer of the
 * decompressor, for saved file format version 2.
 *
 * @returns VBox status code.
 * @param   pvSSM       The SSM handle.
 * @param   pvBuf       Where to store the compressed data.
 * @param   cbBuf       Size of the buffer.
 * @param   pcbRead     Number of bytes actually stored in the buffer.
 */
static DECLCALLBACK(int) ssmR3ReadInV2(void *pvSSM, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    PSSMHANDLE  pSSM   = (PSSMHANDLE)pvSSM;
    //Log2(("ssmR3ReadInV2: %#010llx cbBug=%#x cbRead=%#x\n", RTFileTell(pSSM->hFile), cbBuf, cbRead));
    int rc = ssmR3StrmRead(pSSM, pvBuf, cbBuf);
    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbBuf;
        ssmR3Progress(pSSM, cbBuf);
        return VINF_SUCCESS;
    }
    /** @todo weed out lazy saving */
    if (pSSM->enmAfter != SSMAFTER_DEBUG_IT)
        AssertMsgFailed(("SSM: attempted reading more than the unit!\n"));
    return VERR_SSM_LOADED_TOO_MUCH;
}


/**
 * Creates the decompressor for the data unit.
 *
 * pSSM->rc will be set on error.
 *
 * @param   pSSM            SSM operation handle.
 */
static void ssmR3DataReadBeginV2(PSSMHANDLE pSSM)
{
    Assert(!pSSM->u.Read.pZipDecomp);
    Assert(!pSSM->u.Read.cbDataBuffer || pSSM->u.Read.cbDataBuffer == pSSM->u.Read.offDataBuffer);
    Assert(!pSSM->u.Read.cbRecLeft);

    pSSM->offUnit = 0;
    pSSM->u.Read.pZipDecomp     = NULL;
    pSSM->u.Read.cbRecLeft      = 0;
    pSSM->u.Read.cbDataBuffer   = 0;
    pSSM->u.Read.offDataBuffer  = 0;
    pSSM->u.Read.fEndOfData     = false;
    pSSM->u.Read.u8TypeAndFlags = 0;

    int rc = RTZipDecompCreate(&pSSM->u.Read.pZipDecomp, pSSM, ssmR3ReadInV2);
    if (RT_FAILURE(rc) && RT_SUCCESS(pSSM->rc))
        pSSM->rc = rc;
}


/**
 * Checks for the termination record and closes the decompressor.
 *
 * pSSM->rc will be set on error.
 *
 * @param   pSSM            SSM operation handle.
 */
static void ssmR3DataReadFinishV2(PSSMHANDLE pSSM)
{
    if (RT_UNLIKELY(!pSSM->u.Read.pZipDecomp))
    {
        Assert(RT_FAILURE_NP(pSSM->rc));
        return;
    }

    /*
     * If we haven't encountered the end of the record, it must be the next one.
     */
    if (    !pSSM->u.Read.fEndOfData
        &&  RT_SUCCESS(pSSM->rc))
    {
        int rc = ssmR3DataReadRecHdrV2(pSSM);
        if (    RT_SUCCESS(rc)
            &&  !pSSM->u.Read.fEndOfData)
            rc = VERR_SSM_LOADED_TOO_MUCH; /** @todo More error codes! */
        pSSM->rc = rc;
    }

    /*
     * Destroy the decompressor.
     */
    int rc = RTZipDecompDestroy(pSSM->u.Read.pZipDecomp);
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        if (RT_SUCCESS(pSSM->rc))
            pSSM->rc = rc;
    }
    pSSM->u.Read.pZipDecomp = NULL;
}


/**
 * Read reader that does the decompression and keeps the offset and CRC members
 * up to date.
 *
 * Does not set SSM::rc.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 * @param   pvBuf           Where to put the bits
 * @param   cbBuf           How many bytes to read.
 */
DECLINLINE(int) ssmR3DataReadV2Raw(PSSMHANDLE pSSM, void *pvBuf, size_t cbToRead)
{
    int rc = RTZipDecompress(pSSM->u.Read.pZipDecomp, pvBuf, cbToRead, NULL);
    if (RT_FAILURE(rc))
        return rc;
    pSSM->offUnit += cbToRead;
    if (pSSM->fUnitChecksummed)
        pSSM->u32UnitCRC = RTCrc32Process(pSSM->u32UnitCRC, pvBuf, cbToRead);
    return VINF_SUCCESS;
}


/**
 * Worker for reading the record header.
 *
 * It sets pSSM->u.Read.cbRecLeft, pSSM->u.Read.u8TypeAndFlags and
 * pSSM->u.Read.fEndOfData.  When a termination record is encounter, it will be
 * read in full and validated, the fEndOfData indicator is set, and VINF_SUCCESS
 * is returned.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 */
static int ssmR3DataReadRecHdrV2(PSSMHANDLE pSSM)
{
    AssertLogRelReturn(!pSSM->u.Read.fEndOfData, VERR_SSM_LOADED_TOO_MUCH);

    /*
     * Read the two mandatory bytes.
     */
    uint32_t u32UnitCRC = pSSM->u32UnitCRC;
    uint8_t  abHdr[8];
    int rc = ssmR3DataReadV2Raw(pSSM, abHdr, 2);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Validate the first byte and check for the termination records.
     */
    pSSM->u.Read.u8TypeAndFlags = abHdr[0];
    AssertLogRelMsgReturn(SSM_REC_ARE_TYPE_AND_FLAGS_VALID(abHdr[0]), ("%#x %#x\n", abHdr[0], abHdr[1]), VERR_SSM_INTEGRITY);
    if ((abHdr[0] & SSM_REC_TYPE_MASK) == SSM_REC_TYPE_TERM)
    {
        pSSM->u.Read.cbRecLeft = 0;
        pSSM->u.Read.fEndOfData = true;
        AssertLogRelMsgReturn(abHdr[1] == sizeof(SSMRECTERM) - 2, ("%#x\n", abHdr[1]), VERR_SSM_INTEGRITY);
        AssertLogRelMsgReturn(abHdr[0] & SSM_REC_FLAGS_IMPORTANT, ("%#x\n", abHdr[0]), VERR_SSM_INTEGRITY);

        /* get the rest */
        SSMRECTERM TermRec;
        int rc = ssmR3DataReadV2Raw(pSSM, (uint8_t *)&TermRec + 2, sizeof(SSMRECTERM) - 2);
        if (RT_FAILURE(rc))
            return rc;

        /* validate integrity */
        AssertLogRelMsgReturn(TermRec.cbUnit == pSSM->offUnit,
                              ("cbUnit=%#llx offUnit=%#llx\n", TermRec.cbUnit, pSSM->offUnit),
                              VERR_SSM_INTEGRITY);
        AssertLogRelMsgReturn(!(TermRec.fFlags & ~SSMRECTERM_FLAGS_CRC32), ("%#x\n", TermRec.fFlags), VERR_SSM_INTEGRITY);
        if (!(TermRec.fFlags & SSMRECTERM_FLAGS_CRC32))
            AssertLogRelMsgReturn(TermRec.u32CRC == 0, ("%#x\n", TermRec.u32CRC), VERR_SSM_INTEGRITY);
        else if (pSSM->fUnitChecksummed)
        {
            u32UnitCRC = RTCrc32Finish(u32UnitCRC);
            AssertLogRelMsgReturn(TermRec.u32CRC == u32UnitCRC, ("%#x, %#x\n", TermRec.u32CRC, u32UnitCRC), VERR_SSM_INTEGRITY_CRC);
        }

        Log3(("ssmR3DataReadRecHdrV2: %08llx: TERM\n", pSSM->offUnit));
        return VINF_SUCCESS;
    }

    /*
     * Figure the size. The 2nd byte is encoded in UTF-8 fashion, so this
     * is can be highly enjoyable.
     */
    uint32_t cbHdr = 2;
    uint32_t cb = abHdr[1];
    if (!(cb & 0x80))
        pSSM->u.Read.cbRecLeft = cb;
    else
    {
        /*
         * Need more data. Figure how much and read it.
         */
        if (!(cb & RT_BIT(5)))
            cb = 2;
        else if (!(cb & RT_BIT(4)))
            cb = 3;
        else if (!(cb & RT_BIT(3)))
            cb = 4;
        else if (!(cb & RT_BIT(2)))
            cb = 5;
        else if (!(cb & RT_BIT(1)))
            cb = 6;
        else
            AssertLogRelMsgFailedReturn(("Invalid record size byte: %#x\n", cb), VERR_SSM_INTEGRITY);
        cbHdr = cb + 1;

        rc = ssmR3DataReadV2Raw(pSSM, &abHdr[2], cb - 1);
        if (RT_FAILURE(rc))
            return rc;
Log(("ssmR3DataReadRecHdrV2: cb=%u %.*Rhxs\n", cb, cb + 1, abHdr));

        /*
         * Validate what we've read.
         */
        switch (cb)
        {
            case 6:
                AssertLogRelMsgReturn((abHdr[6] & 0xc0) == 0x80, ("6/%u: %.*Rhxs\n", cb, cb + 1, &abHdr[0]), VERR_SSM_INTEGRITY);
            case 5:
                AssertLogRelMsgReturn((abHdr[5] & 0xc0) == 0x80, ("5/%u: %.*Rhxs\n", cb, cb + 1, &abHdr[0]), VERR_SSM_INTEGRITY);
            case 4:
                AssertLogRelMsgReturn((abHdr[4] & 0xc0) == 0x80, ("4/%u: %.*Rhxs\n", cb, cb + 1, &abHdr[0]), VERR_SSM_INTEGRITY);
            case 3:
                AssertLogRelMsgReturn((abHdr[3] & 0xc0) == 0x80, ("3/%u: %.*Rhxs\n", cb, cb + 1, &abHdr[0]), VERR_SSM_INTEGRITY);
            case 2:
                AssertLogRelMsgReturn((abHdr[2] & 0xc0) == 0x80, ("2/%u: %.*Rhxs\n", cb, cb + 1, &abHdr[0]), VERR_SSM_INTEGRITY);
                break;
            default:
                return VERR_INTERNAL_ERROR;
        }

        /*
         * Decode it and validate the range.
         */
        switch (cb)
        {
            case 6:
                cb =             (abHdr[6] & 0x3f)
                    | ((uint32_t)(abHdr[5] & 0x3f) << 6)
                    | ((uint32_t)(abHdr[4] & 0x3f) << 12)
                    | ((uint32_t)(abHdr[3] & 0x3f) << 18)
                    | ((uint32_t)(abHdr[2] & 0x3f) << 24)
                    | ((uint32_t)(abHdr[1] & 0x01) << 30);
                AssertLogRelMsgReturn(cb >= 0x04000000 && cb <= 0x7fffffff, ("cb=%#x\n", cb), VERR_SSM_INTEGRITY);
                break;
            case 5:
                cb =             (abHdr[5] & 0x3f)
                    | ((uint32_t)(abHdr[4] & 0x3f) << 6)
                    | ((uint32_t)(abHdr[3] & 0x3f) << 12)
                    | ((uint32_t)(abHdr[2] & 0x3f) << 18)
                    | ((uint32_t)(abHdr[1] & 0x03) << 24);
                AssertLogRelMsgReturn(cb >= 0x00200000 && cb <= 0x03ffffff, ("cb=%#x\n", cb), VERR_SSM_INTEGRITY);
                break;
            case 4:
                cb =             (abHdr[4] & 0x3f)
                    | ((uint32_t)(abHdr[3] & 0x3f) << 6)
                    | ((uint32_t)(abHdr[2] & 0x3f) << 12)
                    | ((uint32_t)(abHdr[1] & 0x07) << 18);
                AssertLogRelMsgReturn(cb >= 0x00010000 && cb <= 0x001fffff, ("cb=%#x\n", cb), VERR_SSM_INTEGRITY);
                break;
            case 3:
                cb =             (abHdr[3] & 0x3f)
                    | ((uint32_t)(abHdr[2] & 0x3f) << 6)
                    | ((uint32_t)(abHdr[1] & 0x0f) << 12);
                AssertLogRelMsgReturn(cb >= 0x00000800 && cb <= 0x0000ffff, ("cb=%#x\n", cb), VERR_SSM_INTEGRITY);
                break;
            case 2:
                cb =             (abHdr[2] & 0x3f)
                    | ((uint32_t)(abHdr[1] & 0x1f) << 6);
                AssertLogRelMsgReturn(cb >= 0x00000080 && cb <= 0x000007ff, ("cb=%#x\n", cb), VERR_SSM_INTEGRITY);
                break;
            default:
                return VERR_INTERNAL_ERROR;
        }

        pSSM->u.Read.cbRecLeft = cb;
    }

    Log3(("ssmR3DataReadRecHdrV2: %08llx/%08x: Type=%02x fImportant=%RTbool cbHdr=%u\n",
          pSSM->offUnit, pSSM->u.Read.cbRecLeft,
          pSSM->u.Read.u8TypeAndFlags & SSM_REC_TYPE_MASK,
          !!(pSSM->u.Read.u8TypeAndFlags & SSM_REC_FLAGS_IMPORTANT),
          cbHdr
        )); NOREF(cbHdr);
    return VINF_SUCCESS;
}


/**
 * Buffer miss, do an unbuffered read.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvBuf           Where to store the read data.
 * @param   cbBuf           Number of bytes to read.
 */
static int ssmR3DataReadUnbufferedV2(PSSMHANDLE pSSM, void *pvBuf, size_t cbBuf)
{
    void   const *pvBufOrg = pvBuf; NOREF(pvBufOrg);
    size_t const  cbBufOrg = cbBuf; NOREF(cbBufOrg);

    /*
     * Copy out what we've got in the buffer.
     */
    uint32_t off        = pSSM->u.Read.offDataBuffer;
    int32_t  cbInBuffer = pSSM->u.Read.cbDataBuffer - off;
    Log4(("ssmR3DataReadUnbufferedV2: %08llx/%08x/%08x: cbBuf=%#x\n", pSSM->offUnit, pSSM->u.Read.cbRecLeft, cbInBuffer, cbBufOrg));
    if (cbInBuffer > 0)
    {
        uint32_t const cbToCopy = (uint32_t)cbInBuffer;
        Assert(cbBuf > cbToCopy);
        memcpy(pvBuf, &pSSM->u.Read.abDataBuffer[off], cbToCopy);
        pvBuf = (uint8_t *)pvBuf + cbToCopy;
        cbBuf -= cbToCopy;
        pSSM->u.Read.cbDataBuffer = 0;
        pSSM->u.Read.offDataBuffer = 0;
    }

    /*
     * Read data.
     */
    do
    {
        /*
         * Read the next record header if no more data.
         */
        if (!pSSM->u.Read.cbRecLeft)
        {
            int rc = ssmR3DataReadRecHdrV2(pSSM);
            if (RT_FAILURE(rc))
                return pSSM->rc = rc;
        }
        AssertLogRelMsgReturn(!pSSM->u.Read.fEndOfData, ("cbBuf=%zu", cbBuf), pSSM->rc = VERR_SSM_LOADED_TOO_MUCH);

        /*
         * Read data from the current record.
         */
        size_t cbToRead = RT_MIN(cbBuf, pSSM->u.Read.cbRecLeft);
        int rc = ssmR3DataReadV2Raw(pSSM, pvBuf, cbToRead);
        if (RT_FAILURE(rc))
            return pSSM->rc = rc;
        pSSM->u.Read.cbRecLeft -= cbToRead;
        cbBuf -= cbToRead;
        pvBuf = (uint8_t *)pvBuf + cbToRead;
    } while (cbBuf > 0);

    Log4(("ssmR3DataReadUnBufferedV2: %08llx/%08x/%08x: cbBuf=%#x %.*Rhxs%s\n",
          pSSM->offUnit, pSSM->u.Read.cbRecLeft, 0, cbBufOrg, RT_MIN(SSM_LOG_BYTES, cbBufOrg), pvBufOrg, cbBufOrg > SSM_LOG_BYTES ? "..." : ""));
    return VINF_SUCCESS;
}


/**
 * Buffer miss, do a buffered read.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvBuf           Where to store the read data.
 * @param   cbBuf           Number of bytes to read.
 */
static int ssmR3DataReadBufferedV2(PSSMHANDLE pSSM, void *pvBuf, size_t cbBuf)
{
    void   const *pvBufOrg = pvBuf; NOREF(pvBufOrg);
    size_t const  cbBufOrg = cbBuf; NOREF(cbBufOrg);

    /*
     * Copy out what we've got in the buffer.
     */
    uint32_t off        = pSSM->u.Read.offDataBuffer;
    int32_t  cbInBuffer = pSSM->u.Read.cbDataBuffer - off;
    Log4(("ssmR3DataReadBufferedV2: %08llx/%08x/%08x: cbBuf=%#x\n", pSSM->offUnit, pSSM->u.Read.cbRecLeft, cbInBuffer, cbBufOrg));
    if (cbInBuffer > 0)
    {
        uint32_t const cbToCopy = (uint32_t)cbInBuffer;
        Assert(cbBuf > cbToCopy);
        memcpy(pvBuf, &pSSM->u.Read.abDataBuffer[off], cbToCopy);
        pvBuf = (uint8_t *)pvBuf + cbToCopy;
        cbBuf -= cbToCopy;
        pSSM->u.Read.cbDataBuffer = 0;
        pSSM->u.Read.offDataBuffer = 0;
    }

    /*
     * Buffer more data.
     */
    do
    {
        /*
         * Read the next record header if no more data.
         */
        if (!pSSM->u.Read.cbRecLeft)
        {
            int rc = ssmR3DataReadRecHdrV2(pSSM);
            if (RT_FAILURE(rc))
                return pSSM->rc = rc;
        }
        AssertLogRelMsgReturn(!pSSM->u.Read.fEndOfData, ("cbBuf=%zu", cbBuf), pSSM->rc = VERR_SSM_LOADED_TOO_MUCH);

        /*
         * Read data from the current record.
         * LATER: optimize by reading directly into the output buffer for some cases.
         */
        size_t cbToRead = RT_MIN(sizeof(pSSM->u.Read.abDataBuffer), pSSM->u.Read.cbRecLeft);
        int rc = ssmR3DataReadV2Raw(pSSM, &pSSM->u.Read.abDataBuffer[0], cbToRead);
        if (RT_FAILURE(rc))
            return pSSM->rc = rc;
        pSSM->u.Read.cbRecLeft -= cbToRead;
        pSSM->u.Read.cbDataBuffer = cbToRead;
        /*pSSM->u.Read.offDataBuffer = 0;*/

        /*
         * Copy data from the buffer.
         */
        size_t cbToCopy = RT_MIN(cbBuf, cbToRead);
        memcpy(pvBuf, &pSSM->u.Read.abDataBuffer[0], cbToCopy);
        cbBuf -= cbToCopy;
        pvBuf = (uint8_t *)pvBuf + cbToCopy;
        pSSM->u.Read.offDataBuffer = cbToCopy;
    } while (cbBuf > 0);

    Log4(("ssmR3DataReadBufferedV2: %08llx/%08x/%08x: cbBuf=%#x %.*Rhxs%s\n",
          pSSM->offUnit, pSSM->u.Read.cbRecLeft, pSSM->u.Read.cbDataBuffer - pSSM->u.Read.offDataBuffer,
          cbBufOrg, RT_MIN(SSM_LOG_BYTES, cbBufOrg), pvBufOrg, cbBufOrg > SSM_LOG_BYTES ? "..." : ""));
    return VINF_SUCCESS;
}


/**
 * Inlined worker that handles format checks and buffered reads.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvBuf           Where to store the read data.
 * @param   cbBuf           Number of bytes to read.
 */
DECLINLINE(int) ssmR3DataRead(PSSMHANDLE pSSM, void *pvBuf, size_t cbBuf)
{
    /*
     * Fend off previous errors and V1 data units.
     */
    if (RT_FAILURE(pSSM->rc))
        return pSSM->rc;
    if (RT_UNLIKELY(pSSM->u.Read.uFmtVerMajor == 1))
        return ssmR3DataReadV1(pSSM, pvBuf, cbBuf);

    /*
     * Check if the requested data is buffered.
     */
    uint32_t off = pSSM->u.Read.offDataBuffer;
    if (    off + cbBuf > pSSM->u.Read.cbDataBuffer
        ||  cbBuf > sizeof(pSSM->u.Read.abDataBuffer))
    {
        if (cbBuf <= sizeof(pSSM->u.Read.abDataBuffer) / 8)
            return ssmR3DataReadBufferedV2(pSSM, pvBuf, cbBuf);
        return ssmR3DataReadUnbufferedV2(pSSM, pvBuf, cbBuf);
    }

    memcpy(pvBuf, &pSSM->u.Read.abDataBuffer[off], cbBuf);
    pSSM->u.Read.offDataBuffer = off + cbBuf;
    Log4((cbBuf
          ? "ssmR3DataRead: %08llx/%08x/%08x: cbBuf=%#x %.*Rhxs%s\n"
          : "ssmR3DataRead: %08llx/%08x/%08x: cbBuf=%#x\n",
          pSSM->offUnit, pSSM->u.Read.cbRecLeft, pSSM->u.Read.cbDataBuffer - pSSM->u.Read.offDataBuffer,
          cbBuf, RT_MIN(SSM_LOG_BYTES, cbBuf), pvBuf, cbBuf > SSM_LOG_BYTES ? "..." : ""));

    return VINF_SUCCESS;
}


/**
 * Gets a structure.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 * @param   pvStruct        The structure address.
 * @param   paFields        The array of structure fields descriptions.
 *                          The array must be terminated by a SSMFIELD_ENTRY_TERM().
 */
VMMR3DECL(int) SSMR3GetStruct(PSSMHANDLE pSSM, void *pvStruct, PCSSMFIELD paFields)
{
    /* begin marker. */
    uint32_t u32Magic;
    int rc = SSMR3GetU32(pSSM, &u32Magic);
    if (RT_FAILURE(rc))
        return rc;
    if (u32Magic != SSMR3STRUCT_BEGIN)
        AssertMsgFailedReturn(("u32Magic=%#RX32\n", u32Magic), VERR_SSM_STRUCTURE_MAGIC);

    /* put the fields */
    for (PCSSMFIELD pCur = paFields;
         pCur->cb != UINT32_MAX && pCur->off != UINT32_MAX;
         pCur++)
    {
        rc = ssmR3DataRead(pSSM, (uint8_t *)pvStruct + pCur->off, pCur->cb);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* end marker */
    rc = SSMR3GetU32(pSSM, &u32Magic);
    if (RT_FAILURE(rc))
        return rc;
    if (u32Magic != SSMR3STRUCT_END)
        AssertMsgFailedReturn(("u32Magic=%#RX32\n", u32Magic), VERR_SSM_STRUCTURE_MAGIC);
    return rc;
}


/**
 * Loads a boolean item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pfBool          Where to store the item.
 */
VMMR3DECL(int) SSMR3GetBool(PSSMHANDLE pSSM, bool *pfBool)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
    {
        uint8_t u8; /* see SSMR3PutBool */
        int rc = ssmR3DataRead(pSSM, &u8, sizeof(u8));
        if (RT_SUCCESS(rc))
        {
            Assert(u8 <= 1);
            *pfBool = !!u8;
        }
        return rc;
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 8-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu8             Where to store the item.
 */
VMMR3DECL(int) SSMR3GetU8(PSSMHANDLE pSSM, uint8_t *pu8)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu8, sizeof(*pu8));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 8-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi8             Where to store the item.
 */
VMMR3DECL(int) SSMR3GetS8(PSSMHANDLE pSSM, int8_t *pi8)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi8, sizeof(*pi8));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 16-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu16            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetU16(PSSMHANDLE pSSM, uint16_t *pu16)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu16, sizeof(*pu16));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 16-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi16            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetS16(PSSMHANDLE pSSM, int16_t *pi16)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi16, sizeof(*pi16));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 32-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu32            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetU32(PSSMHANDLE pSSM, uint32_t *pu32)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu32, sizeof(*pu32));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 32-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi32            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetS32(PSSMHANDLE pSSM, int32_t *pi32)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi32, sizeof(*pi32));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 64-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu64            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetU64(PSSMHANDLE pSSM, uint64_t *pu64)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu64, sizeof(*pu64));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 64-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi64            Where to store the item.
 */
VMMR3DECL(int) SSMR3GetS64(PSSMHANDLE pSSM, int64_t *pi64)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi64, sizeof(*pi64));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 128-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu128           Where to store the item.
 */
VMMR3DECL(int) SSMR3GetU128(PSSMHANDLE pSSM, uint128_t *pu128)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu128, sizeof(*pu128));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 128-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi128           Where to store the item.
 */
VMMR3DECL(int) SSMR3GetS128(PSSMHANDLE pSSM, int128_t *pi128)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi128, sizeof(*pi128));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a VBox unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 */
VMMR3DECL(int) SSMR3GetUInt(PSSMHANDLE pSSM, PRTUINT pu)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pu, sizeof(*pu));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a VBox signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi              Where to store the integer.
 */
VMMR3DECL(int) SSMR3GetSInt(PSSMHANDLE pSSM, PRTINT pi)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pi, sizeof(*pi));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a GC natural unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 *
 * @deprecated Silly type with an incorrect size, don't use it.
 */
VMMR3DECL(int) SSMR3GetGCUInt(PSSMHANDLE pSSM, PRTGCUINT pu)
{
    AssertCompile(sizeof(RTGCPTR) == sizeof(*pu));
    return SSMR3GetGCPtr(pSSM, (PRTGCPTR)pu);
}


/**
 * Loads a GC unsigned integer register item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 */
VMMR3DECL(int) SSMR3GetGCUIntReg(PSSMHANDLE pSSM, PRTGCUINTREG pu)
{
    AssertCompile(sizeof(RTGCPTR) == sizeof(*pu));
    return SSMR3GetGCPtr(pSSM, (PRTGCPTR)pu);
}


/**
 * Loads a 32 bits GC physical address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPhys         Where to store the GC physical address.
 */
VMMR3DECL(int) SSMR3GetGCPhys32(PSSMHANDLE pSSM, PRTGCPHYS32 pGCPhys)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pGCPhys, sizeof(*pGCPhys));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a 64 bits GC physical address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPhys         Where to store the GC physical address.
 */
VMMR3DECL(int) SSMR3GetGCPhys64(PSSMHANDLE pSSM, PRTGCPHYS64 pGCPhys)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pGCPhys, sizeof(*pGCPhys));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a GC physical address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPhys         Where to store the GC physical address.
 */
VMMR3DECL(int) SSMR3GetGCPhys(PSSMHANDLE pSSM, PRTGCPHYS pGCPhys)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
    {
        if (RT_LIKELY(sizeof(*pGCPhys) == pSSM->u.Read.cbGCPhys))
            return ssmR3DataRead(pSSM, pGCPhys, sizeof(*pGCPhys));

        /*
         * Fiddly.
         */
        Assert(sizeof(*pGCPhys)      == sizeof(uint64_t) || sizeof(*pGCPhys)      == sizeof(uint32_t));
        Assert(pSSM->u.Read.cbGCPhys == sizeof(uint64_t) || pSSM->u.Read.cbGCPhys == sizeof(uint32_t));
        if (pSSM->u.Read.cbGCPhys == sizeof(uint64_t))
        {
            /* 64-bit saved, 32-bit load: try truncate it. */
            uint64_t u64;
            int rc = ssmR3DataRead(pSSM, &u64, sizeof(uint64_t));
            if (RT_FAILURE(rc))
                return rc;
            if (u64 >= _4G)
                return VERR_SSM_GCPHYS_OVERFLOW;
            *pGCPhys = (RTGCPHYS)u64;
            return rc;
        }

        /* 32-bit saved, 64-bit load: clear the high part. */
        *pGCPhys = 0;
        return ssmR3DataRead(pSSM, pGCPhys, sizeof(uint32_t));
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a GC virtual address item from the current data unit.
 *
 * Only applies to in the 1.1 format:
 *  - SSMR3GetGCPtr
 *  - SSMR3GetGCUIntPtr
 *  - SSMR3GetGCUInt
 *  - SSMR3GetGCUIntReg
 *
 * Put functions are not affected.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   cbGCPtr         Size of RTGCPTR
 *
 * @remarks This interface only works with saved state version 1.1, if the
 *          format isn't 1.1 the call will be ignored.
 */
VMMR3DECL(int) SSMR3SetGCPtrSize(PSSMHANDLE pSSM, unsigned cbGCPtr)
{
    Assert(cbGCPtr == sizeof(RTGCPTR32) || cbGCPtr == sizeof(RTGCPTR64));
    if (!pSSM->u.Read.fFixedGCPtrSize)
    {
        Log(("SSMR3SetGCPtrSize: %u -> %u bytes\n", pSSM->u.Read.cbGCPtr, cbGCPtr));
        pSSM->u.Read.cbGCPtr = cbGCPtr;
        pSSM->u.Read.fFixedGCPtrSize = true;
    }
    else if (   pSSM->u.Read.cbGCPtr != cbGCPtr
             && pSSM->u.Read.cbFileHdr == sizeof(SSMFILEHDRV11))
        AssertMsgFailed(("SSMR3SetGCPtrSize: already fixed at %u bytes; requested %u bytes\n", pSSM->u.Read.cbGCPtr, cbGCPtr));

    return VINF_SUCCESS;
}


/**
 * Loads a GC virtual address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPtr          Where to store the GC virtual address.
 */
VMMR3DECL(int) SSMR3GetGCPtr(PSSMHANDLE pSSM, PRTGCPTR pGCPtr)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
    {
        if (RT_LIKELY(sizeof(*pGCPtr) == pSSM->u.Read.cbGCPtr))
            return ssmR3DataRead(pSSM, pGCPtr, sizeof(*pGCPtr));

        /*
         * Fiddly.
         */
        Assert(sizeof(*pGCPtr)      == sizeof(uint64_t) || sizeof(*pGCPtr)      == sizeof(uint32_t));
        Assert(pSSM->u.Read.cbGCPtr == sizeof(uint64_t) || pSSM->u.Read.cbGCPtr == sizeof(uint32_t));
        if (pSSM->u.Read.cbGCPtr == sizeof(uint64_t))
        {
            /* 64-bit saved, 32-bit load: try truncate it. */
            uint64_t u64;
            int rc = ssmR3DataRead(pSSM, &u64, sizeof(uint64_t));
            if (RT_FAILURE(rc))
                return rc;
            if (u64 >= _4G)
                return VERR_SSM_GCPTR_OVERFLOW;
            *pGCPtr = (RTGCPTR)u64;
            return rc;
        }

        /* 32-bit saved, 64-bit load: clear the high part. */
        *pGCPtr = 0;
        return ssmR3DataRead(pSSM, pGCPtr, sizeof(uint32_t));
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a GC virtual address (represented as unsigned integer) item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPtr          Where to store the GC virtual address.
 */
VMMR3DECL(int) SSMR3GetGCUIntPtr(PSSMHANDLE pSSM, PRTGCUINTPTR pGCPtr)
{
    AssertCompile(sizeof(RTGCPTR) == sizeof(*pGCPtr));
    return SSMR3GetGCPtr(pSSM, (PRTGCPTR)pGCPtr);
}


/**
 * Loads an RC virtual address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pRCPtr          Where to store the RC virtual address.
 */
VMMR3DECL(int) SSMR3GetRCPtr(PSSMHANDLE pSSM, PRTRCPTR pRCPtr)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pRCPtr, sizeof(*pRCPtr));

    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a I/O port address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pIOPort         Where to store the I/O port address.
 */
VMMR3DECL(int) SSMR3GetIOPort(PSSMHANDLE pSSM, PRTIOPORT pIOPort)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pIOPort, sizeof(*pIOPort));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a selector item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pSel            Where to store the selector.
 */
VMMR3DECL(int) SSMR3GetSel(PSSMHANDLE pSSM, PRTSEL pSel)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pSel, sizeof(*pSel));
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a memory item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pv              Where to store the item.
 * @param   cb              Size of the item.
 */
VMMR3DECL(int) SSMR3GetMem(PSSMHANDLE pSSM, void *pv, size_t cb)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
        return ssmR3DataRead(pSSM, pv, cb);
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Loads a string item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Where to store the item.
 * @param   cbMax           Max size of the item (including '\\0').
 */
VMMR3DECL(int) SSMR3GetStrZ(PSSMHANDLE pSSM, char *psz, size_t cbMax)
{
    return SSMR3GetStrZEx(pSSM, psz, cbMax, NULL);
}


/**
 * Loads a string item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Where to store the item.
 * @param   cbMax           Max size of the item (including '\\0').
 * @param   pcbStr          The length of the loaded string excluding the '\\0'. (optional)
 */
VMMR3DECL(int) SSMR3GetStrZEx(PSSMHANDLE pSSM, char *psz, size_t cbMax, size_t *pcbStr)
{
    if (pSSM->enmOp == SSMSTATE_LOAD_EXEC || pSSM->enmOp == SSMSTATE_OPEN_READ)
    {
        /* read size prefix. */
        uint32_t u32;
        int rc = SSMR3GetU32(pSSM, &u32);
        if (RT_SUCCESS(rc))
        {
            if (pcbStr)
                *pcbStr = u32;
            if (u32 < cbMax)
            {
                /* terminate and read string content. */
                psz[u32] = '\0';
                return ssmR3DataRead(pSSM, psz, u32);
            }
            return VERR_TOO_MUCH_DATA;
        }
        return rc;
    }
    AssertMsgFailed(("Invalid state %d\n", pSSM->enmOp));
    return VERR_SSM_INVALID_STATE;
}


/**
 * Skips a number of bytes in the current data unit.
 *
 * @returns VBox status code.
 * @param   pSSM                The SSM handle.
 * @param   cb                  The number of bytes to skip.
 */
VMMR3DECL(int) SSMR3Skip(PSSMHANDLE pSSM, size_t cb)
{
    AssertMsgReturn(   pSSM->enmOp == SSMSTATE_LOAD_EXEC
                    || pSSM->enmOp == SSMSTATE_OPEN_READ,
                    ("Invalid state %d\n", pSSM->enmOp),
                    VERR_SSM_INVALID_STATE);
    while (cb > 0)
    {
        uint8_t abBuf[8192];
        size_t  cbCur = RT_MIN(sizeof(abBuf), cb);
        cb -= cbCur;
        int rc = ssmR3DataRead(pSSM, abBuf, cbCur);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Query what the VBox status code of the operation is.
 *
 * This can be used for putting and getting a batch of values
 * without bother checking the result till all the calls have
 * been made.
 *
 * @returns SSMAFTER enum value.
 * @param   pSSM            SSM operation handle.
 */
VMMR3DECL(int) SSMR3HandleGetStatus(PSSMHANDLE pSSM)
{
    return pSSM->rc;
}


/**
 * Fail the load operation.
 *
 * This is mainly intended for sub item loaders (like timers) which
 * return code isn't necessarily heeded by the caller but is important
 * to SSM.
 *
 * @returns SSMAFTER enum value.
 * @param   pSSM            SSM operation handle.
 * @param   iStatus         Failure status code. This MUST be a VERR_*.
 */
VMMR3DECL(int) SSMR3HandleSetStatus(PSSMHANDLE pSSM, int iStatus)
{
    if (RT_FAILURE(iStatus))
    {
        if (RT_SUCCESS(pSSM->rc))
            pSSM->rc = iStatus;
        return pSSM->rc = iStatus;
    }
    AssertMsgFailed(("iStatus=%d %Rrc\n", iStatus, iStatus));
    return VERR_INVALID_PARAMETER;
}


/**
 * Get what to do after this operation.
 *
 * @returns SSMAFTER enum value.
 * @param   pSSM            SSM operation handle.
 */
VMMR3DECL(SSMAFTER) SSMR3HandleGetAfter(PSSMHANDLE pSSM)
{
    return pSSM->enmAfter;
}


/**
 * Get the current unit byte offset (uncompressed).
 *
 * @returns The offset. UINT64_MAX if called at a wrong time.
 * @param   pSSM            SSM operation handle.
 */
VMMR3DECL(uint64_t) SSMR3HandleGetUnitOffset(PSSMHANDLE pSSM)
{
    return pSSM->offUnit;
}

