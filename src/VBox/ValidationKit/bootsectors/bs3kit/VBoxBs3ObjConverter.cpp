/* $Id$ */
/** @file
 * VirtualBox Validation Kit - Boot Sector 3 object file convert.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iprt/types.h>
#include <iprt/ctype.h>
#include <iprt/assert.h>
#include <iprt/x86.h>

#include <iprt/formats/elf64.h>
#include <iprt/formats/elf-amd64.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/omf.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if ARCH_BITS == 64 && !defined(RT_OS_WINDOWS) && !defined(RT_OS_DARWIN)
# define ELF_FMT_X64  "lx"
# define ELF_FMT_D64  "ld"
#else
# define ELF_FMT_X64  "llx"
# define ELF_FMT_D64  "lld"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Verbosity level. */
static unsigned g_cVerbose = 0;


/**
 * Opens a file for binary reading or writing.
 *
 * @returns File stream handle.
 * @param   pszFile             The name of the file.
 * @param   fWrite              Whether to open for writing or reading.
 */
static FILE *openfile(const char *pszFile,  bool fWrite)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    FILE *pFile = fopen(pszFile, fWrite ? "wb" : "rb");
#else
    FILE *pFile = fopen(pszFile, fWrite ? "w" : "r");
#endif
    if (!pFile)
        fprintf(stderr, "error: Failed to open '%s' for %s: %s (%d)\n",
                pszFile, fWrite ? "writing" : "reading", strerror(errno), errno);
    return pFile;
}


/**
 * Read the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to read.
 * @param   ppvFile     Where to return the memory.
 * @param   pcbFile     Where to return the size.
 */
static bool readfile(const char *pszFile, void **ppvFile, size_t *pcbFile)
{
    FILE *pFile = openfile(pszFile, false);
    if (pFile)
    {
        /*
         * Figure the size.
         */
        if (fseek(pFile, 0, SEEK_END) == 0)
        {
            long cbFile = ftell(pFile);
            if (cbFile > 0)
            {
                if (fseek(pFile, SEEK_SET, 0) == 0)
                {
                    /*
                     * Allocate and read content.
                     */
                    void *pvFile = malloc((size_t)cbFile);
                    if (pvFile)
                    {
                        if (fread(pvFile, cbFile, 1, pFile) == 1)
                        {
                            *ppvFile = pvFile;
                            *pcbFile = (size_t)cbFile;
                            fclose(pFile);
                            return true;
                        }
                        free(pvFile);
                        fprintf(stderr, "error: fread failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
                    }
                    else
                        fprintf(stderr, "error: failed to allocate %ld bytes of memory for '%s'\n", cbFile, pszFile);
                }
                else
                    fprintf(stderr, "error: fseek #2 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
            }
            else
                fprintf(stderr, "error: ftell failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        }
        else
            fprintf(stderr, "error: fseek #1 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Write the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to write.
 * @param   pvFile      Where to return the memory.
 * @param   cbFile      Where to return the size.
 */
static bool writefile(const char *pszFile, void const *pvFile, size_t cbFile)
{
    remove(pszFile);

    int rc = -1;
    FILE *pFile = openfile(pszFile, true);
    if (pFile)
    {
        if (fwrite(pvFile, cbFile, 1, pFile) == 1)
        {
            fclose(pFile);
            return true;
        }
        fprintf(stderr, "error: fwrite failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Reports an error and returns false.
 *
 * @returns false
 * @param   pszFile             The filename.
 * @param   pszFormat           The message format string.
 * @param   ...                 Format arguments.
 */
static bool error(const char *pszFile, const char *pszFormat, ...)
{
    fflush(stdout);
    fprintf(stderr, "error: %s: ", pszFile);
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);
    return false;
}



/*********************************************************************************************************************************
*   Common OMF Writer                                                                                                            *
*********************************************************************************************************************************/

/** Entry for each segment/section in the source format for mapping it to a
 *  segment defintion. */
typedef struct OMFTOSEGDEF
{
    /** The segment defintion index of the section, UINT16_MAX if not translated. */
    uint16_t    iSegDef;
    /** The group index for this segment, UINT16_MAX if not applicable. */
    uint16_t    iGrpDef;
    /** The class name table entry, UINT16_MAX if not applicable. */
    uint16_t    iClassNm;
    /** The group name for this segment, UINT16_MAX if not applicable. */
    uint16_t    iGrpNm;
    /** The group name for this segment, UINT16_MAX if not applicable. */
    uint16_t    iSegNm;
    /** The number of public definitions for this segment. */
    uint32_t    cPubDefs;
    /** The segment name (OMF). */
    char       *pszName;
} OMFTOSEGDEF;
/** Pointer to a segment/section to segdef mapping. */
typedef OMFTOSEGDEF *POMFTOSEGDEF;

/** Symbol table translation type. */
typedef enum OMFSYMTYPE
{
    /** Invalid symbol table entry (aux sym). */
    OMFSYMTYPE_INVALID = 0,
    /** Ignored. */
    OMFSYMTYPE_IGNORED,
    /** A public defintion.  */
    OMFSYMTYPE_PUBDEF,
    /** An external definition. */
    OMFSYMTYPE_EXTDEF,
    /** A segment reference for fixups. */
    OMFSYMTYPE_SEGDEF,
    /** Internal symbol that may be used for fixups. */
    OMFSYMTYPE_INTERNAL
} OMFSYMTYPE;

/** Symbol table translation. */
typedef struct OMFSYMBOL
{
    /** What this source symbol table entry should be translated into. */
    OMFSYMTYPE      enmType;
    /** The OMF table index. UINT16_MAX if not applicable. */
    uint16_t        idx;
    /** The OMF segment definition index. */
    uint16_t        idxSegDef;
    /** The OMF group definition index. */
    uint16_t        idxGrpDef;
} OMFSYMBOL;
/** Pointer to an source symbol table translation entry. */
typedef OMFSYMBOL *POMFSYMBOL;

/**
 * OMF converter & writer instance.
 */
typedef struct OMFWRITER
{
    /** The source file name (for bitching). */
    const char     *pszSrc;
    /** The destination output file. */
    FILE           *pDst;

    /** Number of source segments/sections. */
    uint32_t        cSegments;
    /** Pointer to the table mapping from source segments/section to segdefs. */
    POMFTOSEGDEF    paSegments;

    /** Number of entries in the source symbol table. */
    uint32_t        cSymbols;
    /** Pointer to the table mapping from source symbols to OMF stuff. */
    POMFSYMBOL      paSymbols;

    /** The index of the next list of names entry. */
    uint16_t        idxNextName;

    /** The current record size.  */
    uint16_t        cbRec;
    /** The current record type */
    uint8_t         bType;
    /** The record data buffer (too large, but whatever).  */
    uint8_t         abData[_1K + 64];
    /** Alignment padding. */
    uint8_t         abAlign[2];

    /** Current FIXUPP entry. */
    uint8_t         iFixupp;
    /** FIXUPP records being prepared for LEDATA currently stashed in abData.
     * We may have to adjust addend values in the LEDATA when converting to OMF
     * fixups. */
    struct
    {
        uint16_t    cbRec;
        uint8_t     abData[_1K + 64];
        uint8_t     abAlign[2]; /**< Alignment padding. */
    } aFixupps[3];

    /** The index of the FLAT group. */
    uint16_t        idxGrpFlat;
    /** The EXTDEF index of the __ImageBase symbol. */
    uint16_t        idxExtImageBase;
} OMFWRITE;
/** Pointer to an OMF writer. */
typedef OMFWRITE *POMFWRITER;


/**
 * Creates an OMF writer instance.
 */
static POMFWRITER omfWriter_Create(const char *pszSrc, uint32_t cSegments, uint32_t cSymbols, FILE *pDst)
{
    POMFWRITER pThis = (POMFWRITER)calloc(sizeof(OMFWRITER), 1);
    if (pThis)
    {
        pThis->pszSrc        = pszSrc;
        pThis->idxNextName   = 1;       /* We start counting at 1. */
        pThis->cSegments     = cSegments;
        pThis->paSegments    = (POMFTOSEGDEF)calloc(sizeof(OMFTOSEGDEF), cSegments);
        if (pThis->paSegments)
        {
            pThis->cSymbols  = cSymbols;
            pThis->paSymbols = (POMFSYMBOL)calloc(sizeof(OMFSYMBOL), cSymbols);
            if (pThis->paSymbols)
            {
                pThis->pDst  = pDst;
                return pThis;
            }
            free(pThis->paSegments);
        }
        free(pThis);
    }
    error(pszSrc, "Out of memory!\n");
    return NULL;
}

/**
 * Destroys the given OMF writer instance.
 * @param   pThis           OMF writer instance.
 */
static void omfWriter_Destroy(POMFWRITER pThis)
{
    free(pThis->paSymbols);
    for (uint32_t i = 0; i < pThis->cSegments; i++)
        if (pThis->paSegments[i].pszName)
            free(pThis->paSegments[i].pszName);
    free(pThis->paSegments);
    free(pThis);
}

static bool omfWriter_RecBegin(POMFWRITER pThis, uint8_t bType)
{
    pThis->bType = bType;
    pThis->cbRec = 0;
    return true;
}

static bool omfWriter_RecAddU8(POMFWRITER pThis, uint8_t b)
{
    if (pThis->cbRec < OMF_MAX_RECORD_PAYLOAD)
    {
        pThis->abData[pThis->cbRec++] = b;
        return true;
    }
    return error(pThis->pszSrc, "Exceeded max OMF record length (bType=%#x)!\n", pThis->bType);
}

static bool omfWriter_RecAddU16(POMFWRITER pThis, uint16_t u16)
{
    if (pThis->cbRec + 2 <= OMF_MAX_RECORD_PAYLOAD)
    {
        pThis->abData[pThis->cbRec++] = (uint8_t)u16;
        pThis->abData[pThis->cbRec++] = (uint8_t)(u16 >> 8);
        return true;
    }
    return error(pThis->pszSrc, "Exceeded max OMF record length (bType=%#x)!\n", pThis->bType);
}

static bool omfWriter_RecAddU32(POMFWRITER pThis, uint32_t u32)
{
    if (pThis->cbRec + 4 <= OMF_MAX_RECORD_PAYLOAD)
    {
        pThis->abData[pThis->cbRec++] = (uint8_t)u32;
        pThis->abData[pThis->cbRec++] = (uint8_t)(u32 >> 8);
        pThis->abData[pThis->cbRec++] = (uint8_t)(u32 >> 16);
        pThis->abData[pThis->cbRec++] = (uint8_t)(u32 >> 24);
        return true;
    }
    return error(pThis->pszSrc, "Exceeded max OMF record length (bType=%#x)!\n", pThis->bType);
}

static bool omfWriter_RecAddIdx(POMFWRITER pThis, uint16_t idx)
{
    if (idx < 128)
        return omfWriter_RecAddU8(pThis, (uint8_t)idx);
    if (idx < _32K)
        return omfWriter_RecAddU8(pThis, (uint8_t)(idx >> 7) | 0x80)
            && omfWriter_RecAddU8(pThis, (uint8_t)idx);
    return error(pThis->pszSrc, "Index out of range %#x\n", idx);
}

static bool omfWriter_RecAddBytes(POMFWRITER pThis, const void *pvData, size_t cbData)
{
    if (cbData + pThis->cbRec <= OMF_MAX_RECORD_PAYLOAD)
    {
        memcpy(&pThis->abData[pThis->cbRec], pvData, cbData);
        pThis->cbRec += (uint16_t)cbData;
        return true;
    }
    return error(pThis->pszSrc, "Exceeded max OMF record length (bType=%#x, cbData=%#x)!\n", pThis->bType, (unsigned)cbData);
}

static bool omfWriter_RecAddStringN(POMFWRITER pThis, const char *pchString, size_t cchString)
{
    if (cchString < 256)
    {
        return omfWriter_RecAddU8(pThis, (uint8_t)cchString)
            && omfWriter_RecAddBytes(pThis, pchString, cchString);
    }
    return error(pThis->pszSrc, "String too long (%u bytes): '%*.*s'\n",
                 (unsigned)cchString, (int)cchString, (int)cchString, pchString);
}

static bool omfWriter_RecAddString(POMFWRITER pThis, const char *pszString)
{
    return omfWriter_RecAddStringN(pThis, pszString, strlen(pszString));
}

static bool omfWriter_RecEnd(POMFWRITER pThis, bool fAddCrc)
{
    if (   !fAddCrc
        || omfWriter_RecAddU8(pThis, 0))
    {
        OMFRECHDR RecHdr = { pThis->bType, RT_H2LE_U16(pThis->cbRec) };
        if (   fwrite(&RecHdr, sizeof(RecHdr), 1, pThis->pDst) == 1
            && fwrite(pThis->abData, pThis->cbRec, 1, pThis->pDst) == 1)
        {
            pThis->bType = 0;
            pThis->cbRec = 0;
            return true;
        }
        return error(pThis->pszSrc, "Write error\n");
    }
    return false;
}

static bool omfWriter_RecEndWithCrc(POMFWRITER pThis)
{
    return omfWriter_RecEnd(pThis, true /*fAddCrc*/);
}


static bool omfWriter_BeginModule(POMFWRITER pThis, const char *pszFile)
{
    return omfWriter_RecBegin(pThis, OMF_THEADR)
        && omfWriter_RecAddString(pThis, pszFile)
        && omfWriter_RecEndWithCrc(pThis);
}

static bool omfWriter_LNamesAddN(POMFWRITER pThis, const char *pchName, size_t cchName, uint16_t *pidxName)
{
    /* split? */
    if (pThis->cbRec + 1 /*len*/ + cchName + 1 /*crc*/ > OMF_MAX_RECORD_PAYLOAD)
    {
        if (pThis->cbRec == 0)
            return error(pThis->pszSrc, "Too long LNAME '%*.*s'\n", (int)cchName, (int)cchName, pchName);
        if (   !omfWriter_RecEndWithCrc(pThis)
            || !omfWriter_RecBegin(pThis, OMF_LNAMES))
            return false;
    }

    if (pidxName)
        *pidxName = pThis->idxNextName;
    pThis->idxNextName++;
    return omfWriter_RecAddStringN(pThis, pchName, cchName);
}

static bool omfWriter_LNamesAdd(POMFWRITER pThis, const char *pszName, uint16_t *pidxName)
{
    return omfWriter_LNamesAddN(pThis, pszName, strlen(pszName), pidxName);
}

static bool omfWriter_LNamesBegin(POMFWRITER pThis)
{
    /* First entry is an empty string. */
    return omfWriter_RecBegin(pThis, OMF_LNAMES)
        && (   pThis->idxNextName > 1
            || omfWriter_LNamesAddN(pThis, "", 0, NULL));
}

static bool omfWriter_LNamesEnd(POMFWRITER pThis)
{
    return omfWriter_RecEndWithCrc(pThis);
}


static bool omfWriter_SegDef(POMFWRITER pThis, uint8_t bSegAttr, uint32_t cbSeg, uint16_t idxSegName, uint16_t idxSegClass)
{
    return omfWriter_RecBegin(pThis, OMF_SEGDEF32)
        && omfWriter_RecAddU8(pThis, bSegAttr)
        && omfWriter_RecAddU32(pThis, cbSeg)
        && omfWriter_RecAddIdx(pThis, idxSegName)
        && omfWriter_RecAddIdx(pThis, idxSegClass)
        && omfWriter_RecAddIdx(pThis, 1) /* overlay name index = NULL entry */
        && omfWriter_RecEndWithCrc(pThis);
}

static bool omfWriter_GrpDefBegin(POMFWRITER pThis, uint16_t idxGrpName)
{
    return omfWriter_RecBegin(pThis, OMF_GRPDEF)
        && omfWriter_RecAddIdx(pThis, idxGrpName);
}

static bool omfWriter_GrpDefAddSegDef(POMFWRITER pThis, uint16_t idxSegDef)
{
    return omfWriter_RecAddU8(pThis, 0xff)
        && omfWriter_RecAddIdx(pThis, idxSegDef);
}

static bool omfWriter_GrpDefEnd(POMFWRITER pThis)
{
    return omfWriter_RecEndWithCrc(pThis);
}


static bool omfWriter_PubDefBegin(POMFWRITER pThis, uint16_t idxGrpDef, uint16_t idxSegDef)
{
    return omfWriter_RecBegin(pThis, OMF_PUBDEF32)
        && omfWriter_RecAddIdx(pThis, idxGrpDef)
        && omfWriter_RecAddIdx(pThis, idxSegDef)
        && (   idxSegDef != 0
            || omfWriter_RecAddU16(pThis, 0));

}

static bool omfWriter_PubDefAddN(POMFWRITER pThis, uint32_t uValue, const char *pchString, size_t cchString)
{
    /* Split? */
    if (pThis->cbRec + 1 + cchString + 4 + 1 + 1 > OMF_MAX_RECORD_PAYLOAD)
    {
        if (cchString >= 256)
            return error(pThis->pszSrc, "PUBDEF string too long %u ('%s')\n",
                         (unsigned)cchString, (int)cchString, (int)cchString, pchString);
        if (!omfWriter_RecEndWithCrc(pThis))
            return false;

        /* Figure out the initial data length. */
        pThis->cbRec      = 1 + ((pThis->abData[0] & 0x80) != 0);
        if (pThis->abData[pThis->cbRec] != 0)
            pThis->cbRec += 1 + ((pThis->abData[pThis->cbRec] & 0x80) != 0);
        else
            pThis->cbRec += 3;
        pThis->bType = OMF_PUBDEF32;
    }

    return omfWriter_RecAddStringN(pThis, pchString, cchString)
        && omfWriter_RecAddU32(pThis, uValue)
        && omfWriter_RecAddIdx(pThis, 0); /* type */
}

static bool omfWriter_PubDefAdd(POMFWRITER pThis, uint32_t uValue, const char *pszString)
{
    return omfWriter_PubDefAddN(pThis, uValue, pszString, strlen(pszString));
}

static bool omfWriter_PubDefEnd(POMFWRITER pThis)
{
    return omfWriter_RecEndWithCrc(pThis);
}

/**
 * EXTDEF - Begin record.
 */
static bool omfWriter_ExtDefBegin(POMFWRITER pThis)
{
    return omfWriter_RecBegin(pThis, OMF_EXTDEF);

}

/**
 * EXTDEF - Add an entry, split record if necessary.
 */
static bool omfWriter_ExtDefAddN(POMFWRITER pThis, const char *pchString, size_t cchString)
{
    /* Split? */
    if (pThis->cbRec + 1 + cchString + 1 + 1 > OMF_MAX_RECORD_PAYLOAD)
    {
        if (cchString >= 256)
            return error(pThis->pszSrc, "EXTDEF string too long %u ('%s')\n",
                         (unsigned)cchString, (int)cchString, (int)cchString, pchString);
        if (   !omfWriter_RecEndWithCrc(pThis)
            || !omfWriter_RecBegin(pThis, OMF_EXTDEF))
            return false;
    }

    return omfWriter_RecAddStringN(pThis, pchString, cchString)
        && omfWriter_RecAddIdx(pThis, 0); /* type */
}

/**
 * EXTDEF - Add an entry, split record if necessary.
 */
static bool omfWriter_ExtDefAdd(POMFWRITER pThis, const char *pszString)
{
    return omfWriter_ExtDefAddN(pThis, pszString, strlen(pszString));
}

/**
 * EXTDEF - End of record.
 */
static bool omfWriter_ExtDefEnd(POMFWRITER pThis)
{
    return omfWriter_RecEndWithCrc(pThis);
}

/**
 * COMENT/LINK_PASS_SEP - Add a link pass separator comment.
 */
static bool omfWriter_LinkPassSeparator(POMFWRITER pThis)
{
    return omfWriter_RecBegin(pThis, OMF_COMENT)
        && omfWriter_RecAddU8(pThis, OMF_CTYP_NO_LIST)
        && omfWriter_RecAddU8(pThis, OMF_CCLS_LINK_PASS_SEP)
        && omfWriter_RecAddU8(pThis, 1)
        && omfWriter_RecEndWithCrc(pThis);
}

/**
 * LEDATA + FIXUPP - Begin records.
 */
static bool omfWriter_LEDataBegin(POMFWRITER pThis, uint16_t idxSeg, uint32_t offSeg,
                                  uint32_t cbData, uint32_t cbRawData, void const *pbRawData, uint8_t **ppbData)
{
    if (   omfWriter_RecBegin(pThis, OMF_LEDATA32)
        && omfWriter_RecAddIdx(pThis, idxSeg)
        && omfWriter_RecAddU32(pThis, offSeg))
    {
        if (   cbData <= _1K
            && pThis->cbRec + cbData + 1 <= OMF_MAX_RECORD_PAYLOAD)
        {
            uint8_t *pbDst = &pThis->abData[pThis->cbRec];
            if (ppbData)
                *ppbData = pbDst;

            if (cbRawData)
                memcpy(pbDst, pbRawData, RT_MIN(cbData, cbRawData));
            if (cbData > cbRawData)
                memset(&pbDst[cbRawData], 0, cbData - cbRawData);

            pThis->cbRec += cbData;

            /* Reset the associated FIXUPP records. */
            pThis->iFixupp = 0;
            for (unsigned i = 0; i < RT_ELEMENTS(pThis->aFixupps); i++)
                pThis->aFixupps[i].cbRec = 0;
            return true;
        }
        error(pThis->pszSrc, "Too much data for LEDATA record! (%#x)\n", (unsigned)cbData);
    }
    return false;
}

/**
 * LEDATA + FIXUPP - Add FIXUPP subrecord bytes, split if necessary.
 */
static bool omfWriter_LEDataAddFixuppBytes(POMFWRITER pThis, void *pvSubRec, size_t cbSubRec)
{
    /* Split? */
    unsigned iFixupp = pThis->iFixupp;
    if (pThis->aFixupps[iFixupp].cbRec + cbSubRec >= OMF_MAX_RECORD_PAYLOAD)
    {
        iFixupp++;
        if (iFixupp >= RT_ELEMENTS(pThis->aFixupps))
            return error(pThis->pszSrc, "Out of FIXUPP records\n");
        pThis->iFixupp = iFixupp;
        pThis->aFixupps[iFixupp].cbRec = 0; /* paranoia */
    }

    /* Append the sub-record data. */
    memcpy(&pThis->aFixupps[iFixupp].abData[pThis->aFixupps[iFixupp].cbRec], pvSubRec, cbSubRec);
    pThis->aFixupps[iFixupp].cbRec += (uint16_t)cbSubRec;
    return true;
}

/**
 * LEDATA + FIXUPP - Add fixup, split if necessary.
 */
static bool omfWriter_LEDataAddFixup(POMFWRITER pThis, uint16_t offDataRec, bool fSelfRel, uint8_t bLocation,
                                     uint8_t bFrame, uint16_t idxFrame,
                                     uint8_t bTarget, uint16_t idxTarget, bool fTargetDisp, uint32_t offTargetDisp)
{
    if (g_cVerbose >= 2)
        printf("debug: FIXUP[%#x]: off=%#x frame=%u:%#x target=%u:%#x disp=%d:%#x\n", pThis->aFixupps[pThis->iFixupp].cbRec,
               offDataRec, bFrame, idxFrame, bTarget, idxTarget, fTargetDisp, offTargetDisp);

    if (   offDataRec >= _1K
        || bFrame >= 6
        || bTarget > 6
        || idxFrame >= _32K
        || idxTarget >= _32K
        || fTargetDisp != (bTarget <= 4) )
        /*return*/ error(pThis->pszSrc,  "Internal error: off=%#x frame=%u:%#x target=%u:%#x disp=%d:%#x\n",
                     offDataRec, bFrame, idxFrame, bTarget, idxTarget, fTargetDisp, offTargetDisp);

    /*
     * Encode the FIXUP subrecord.
     */
    uint8_t abFixup[16];
    uint8_t off = 0;
    /* Location */
    abFixup[off++] = (offDataRec >> 8) | (bLocation << 2) | ((uint8_t)!fSelfRel << 6) | 0x80;
    abFixup[off++] = (uint8_t)offDataRec;
    /* Fix Data */
    abFixup[off++] = 0x00 /*F=0*/ | (bFrame << 4) | 0x00 /*T=0*/ | bTarget;
    /* Frame Datum */
    if (bFrame <= OMF_FIX_F_FRAME_NO)
    {
        if (idxFrame >= 128)
            abFixup[off++] = (uint8_t)(idxFrame >> 8);
        abFixup[off++] = (uint8_t)idxFrame;
    }
    /* Target Datum */
    if (idxTarget >= 128)
        abFixup[off++] = (uint8_t)(idxTarget >> 8);
    abFixup[off++] = (uint8_t)idxTarget;
    /* Target Displacement */
    if (fTargetDisp)
    {
        abFixup[off++] = RT_BYTE1(offTargetDisp);
        abFixup[off++] = RT_BYTE2(offTargetDisp);
        abFixup[off++] = RT_BYTE3(offTargetDisp);
        abFixup[off++] = RT_BYTE4(offTargetDisp);
    }

    return omfWriter_LEDataAddFixuppBytes(pThis, abFixup, off);
}

/**
 * LEDATA + FIXUPP - End of records.
 */
static bool omfWriter_LEDataEnd(POMFWRITER pThis)
{
    if (omfWriter_RecEndWithCrc(pThis))
    {
        for (unsigned iFixupp = 0; iFixupp <= pThis->iFixupp; iFixupp++)
        {
            uint8_t const cbRec = pThis->aFixupps[iFixupp].cbRec;
            if (!cbRec)
                break;
            if (   !omfWriter_RecBegin(pThis, OMF_FIXUPP32)
                || !omfWriter_RecAddBytes(pThis, pThis->aFixupps[iFixupp].abData, cbRec)
                || !omfWriter_RecEndWithCrc(pThis))
                return false;
        }
        pThis->iFixupp = 0;
        return true;
    }
    return false;
}

/**
 * MODEND - End of module, simple variant.
 */
static bool omfWriter_EndModule(POMFWRITER pThis)
{
    return omfWriter_RecBegin(pThis, OMF_MODEND32)
        && omfWriter_RecAddU8(pThis, 0)
        && omfWriter_RecEndWithCrc(pThis);
}




/*********************************************************************************************************************************
*   ELF64/AMD64 -> ELF64/i386 Converter                                                                                          *
*********************************************************************************************************************************/

/** AMD64 relocation type names for ELF. */
static const char * const g_apszElfAmd64RelTypes[] =
{
    "R_X86_64_NONE",
    "R_X86_64_64",
    "R_X86_64_PC32",
    "R_X86_64_GOT32",
    "R_X86_64_PLT32",
    "R_X86_64_COPY",
    "R_X86_64_GLOB_DAT",
    "R_X86_64_JMP_SLOT",
    "R_X86_64_RELATIVE",
    "R_X86_64_GOTPCREL",
    "R_X86_64_32",
    "R_X86_64_32S",
    "R_X86_64_16",
    "R_X86_64_PC16",
    "R_X86_64_8",
    "R_X86_64_PC8",
    "R_X86_64_DTPMOD64",
    "R_X86_64_DTPOFF64",
    "R_X86_64_TPOFF64",
    "R_X86_64_TLSGD",
    "R_X86_64_TLSLD",
    "R_X86_64_DTPOFF32",
    "R_X86_64_GOTTPOFF",
    "R_X86_64_TPOFF32",
};


typedef struct ELFDETAILS
{
    /** The ELF header. */
    Elf64_Ehdr const   *pEhdr;
    /** The section header table.   */
    Elf64_Shdr const   *paShdrs;
    /** The string table for the section names. */
    const char         *pchShStrTab;

    /** The symbol table section number. UINT16_MAX if not found.   */
    uint16_t            iSymSh;
    /** The string table section number. UINT16_MAX if not found. */
    uint16_t            iStrSh;

    /** The symbol table.   */
    Elf64_Sym const    *paSymbols;
    /** The number of symbols in the symbol table. */
    uint32_t            cSymbols;

    /** Pointer to the (symbol) string table if found. */
    const char         *pchStrTab;
    /** The string table size. */
    size_t              cbStrTab;

} ELFDETAILS;
typedef ELFDETAILS *PELFDETAILS;


static bool validateElf(const char *pszFile, uint8_t const *pbFile, size_t cbFile, PELFDETAILS pElfStuff)
{
    /*
     * Initialize the ELF details structure.
     */
    memset(pElfStuff, 0,  sizeof(*pElfStuff));
    pElfStuff->iSymSh = UINT16_MAX;
    pElfStuff->iStrSh = UINT16_MAX;

    /*
     * Validate the header and our other expectations.
     */
    Elf64_Ehdr const *pEhdr = (Elf64_Ehdr const *)pbFile;
    pElfStuff->pEhdr = pEhdr;
    if (   pEhdr->e_ident[EI_CLASS] != ELFCLASS64
        || pEhdr->e_ident[EI_DATA]  != ELFDATA2LSB
        || pEhdr->e_ehsize          != sizeof(Elf64_Ehdr)
        || pEhdr->e_shentsize       != sizeof(Elf64_Shdr)
        || pEhdr->e_version         != EV_CURRENT )
        return error(pszFile, "Unsupported ELF config\n");
    if (pEhdr->e_type != ET_REL)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_type);
    if (pEhdr->e_machine != EM_X86_64)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_machine);
    if (pEhdr->e_phnum != 0)
        return error(pszFile, "Expected e_phnum to be zero not %u\n", pEhdr->e_phnum);
    if (pEhdr->e_shnum < 2)
        return error(pszFile, "Expected e_shnum to be two or higher\n");
    if (pEhdr->e_shstrndx >= pEhdr->e_shnum || pEhdr->e_shstrndx == 0)
        return error(pszFile, "Bad e_shstrndx=%u (e_shnum=%u)\n", pEhdr->e_shstrndx, pEhdr->e_shnum);
    if (   pEhdr->e_shoff >= cbFile
        || pEhdr->e_shoff + pEhdr->e_shnum * sizeof(Elf64_Shdr) > cbFile)
        return error(pszFile, "Section table is outside the file (e_shoff=%#llx, e_shnum=%u, cbFile=%#llx)\n",
                     pEhdr->e_shstrndx, pEhdr->e_shnum, (uint64_t)cbFile);

    /*
     * Locate the section name string table.
     * We assume it's okay as we only reference it in verbose mode.
     */
    Elf64_Shdr const *paShdrs = (Elf64_Shdr const *)&pbFile[pEhdr->e_shoff];
    pElfStuff->paShdrs = paShdrs;

    Elf64_Xword const cbShStrTab = paShdrs[pEhdr->e_shstrndx].sh_size;
    if (   paShdrs[pEhdr->e_shstrndx].sh_offset > cbFile
        || cbShStrTab > cbFile
        || paShdrs[pEhdr->e_shstrndx].sh_offset + cbShStrTab > cbFile)
        return error(pszFile,
                     "Section string table is outside the file (sh_offset=%#" ELF_FMT_X64 " sh_size=%#" ELF_FMT_X64 " cbFile=%#" ELF_FMT_X64 ")\n",
                     paShdrs[pEhdr->e_shstrndx].sh_offset, paShdrs[pEhdr->e_shstrndx].sh_size, (Elf64_Xword)cbFile);
    const char *pchShStrTab = (const char *)&pbFile[paShdrs[pEhdr->e_shstrndx].sh_offset];
    pElfStuff->pchShStrTab = pchShStrTab;

    /*
     * Work the section table.
     */
    bool fRet = true;
    for (uint32_t i = 1; i < pEhdr->e_shnum; i++)
    {
        if (paShdrs[i].sh_name >= cbShStrTab)
            return error(pszFile, "Invalid sh_name value (%#x) for section #%u\n", paShdrs[i].sh_name, i);
        const char *pszShNm = &pchShStrTab[paShdrs[i].sh_name];

        if (   paShdrs[i].sh_offset > cbFile
            || paShdrs[i].sh_size > cbFile
            || paShdrs[i].sh_offset + paShdrs[i].sh_size > cbFile)
            return error(pszFile, "Section #%u '%s' has data outside the file: %#" ELF_FMT_X64 " LB %#" ELF_FMT_X64 " (cbFile=%#" ELF_FMT_X64 ")\n",
                         i, pszShNm, paShdrs[i].sh_offset, paShdrs[i].sh_size, (Elf64_Xword)cbFile);
        if (g_cVerbose)
            printf("shdr[%u]: name=%#x '%s' type=%#x flags=%#" ELF_FMT_X64 " addr=%#" ELF_FMT_X64 " off=%#" ELF_FMT_X64 " size=%#" ELF_FMT_X64 "\n"
                   "          link=%u info=%#x align=%#" ELF_FMT_X64 " entsize=%#" ELF_FMT_X64 "\n",
                   i, paShdrs[i].sh_name, pszShNm, paShdrs[i].sh_type, paShdrs[i].sh_flags,
                   paShdrs[i].sh_addr, paShdrs[i].sh_offset, paShdrs[i].sh_size,
                   paShdrs[i].sh_link, paShdrs[i].sh_info, paShdrs[i].sh_addralign, paShdrs[i].sh_entsize);

        if (paShdrs[i].sh_link >= pEhdr->e_shnum)
            return error(pszFile, "Section #%u '%s' links to a section outside the section table: %#x, max %#x\n",
                         i, pszShNm, paShdrs[i].sh_link, pEhdr->e_shnum);
        if (!RT_IS_POWER_OF_TWO(paShdrs[i].sh_addralign))
            return error(pszFile, "Section #%u '%s' alignment value is not a power of two: %#" ELF_FMT_X64 "\n",
                         i, pszShNm, paShdrs[i].sh_addralign);
        if (!RT_IS_POWER_OF_TWO(paShdrs[i].sh_addralign))
            return error(pszFile, "Section #%u '%s' alignment value is not a power of two: %#" ELF_FMT_X64 "\n",
                         i, pszShNm, paShdrs[i].sh_addralign);

        if (paShdrs[i].sh_type == SHT_RELA)
        {
            if (paShdrs[i].sh_entsize != sizeof(Elf64_Rela))
                return error(pszFile, "Expected sh_entsize to be %u not %u for section #%u (%s)\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, pszShNm);
            uint32_t const cRelocs = paShdrs[i].sh_size / sizeof(Elf64_Rela);
            if (cRelocs * sizeof(Elf64_Rela) != paShdrs[i].sh_size)
                return error(pszFile, "Uneven relocation entry count in #%u (%s): sh_size=%#" ELF_FMT_X64 "\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, pszShNm, paShdrs[i].sh_size);
            if (   paShdrs[i].sh_offset > cbFile
                || paShdrs[i].sh_size  >= cbFile
                || paShdrs[i].sh_offset + paShdrs[i].sh_size > cbFile)
                return error(pszFile, "The content of section #%u '%s' is outside the file (%#" ELF_FMT_X64 " LB %#" ELF_FMT_X64 ", cbFile=%#lx)\n",
                             i, pszShNm, paShdrs[i].sh_offset, paShdrs[i].sh_size, (unsigned long)cbFile);

            Elf64_Rela const  *paRelocs = (Elf64_Rela *)&pbFile[paShdrs[i].sh_offset];
            for (uint32_t j = 0; j < cRelocs; j++)
            {
                uint8_t const bType = ELF64_R_TYPE(paRelocs[j].r_info);
                if (RT_UNLIKELY(bType >= R_X86_64_COUNT))
                    fRet = error(pszFile,
                                 "%#018" ELF_FMT_X64 "  %#018" ELF_FMT_X64 " unknown fix up %#x  (%+" ELF_FMT_D64 ")\n",
                                 paRelocs[j].r_offset, paRelocs[j].r_info, bType, paRelocs[j].r_addend);
            }
        }
        else if (paShdrs[i].sh_type == SHT_REL)
            fRet = error(pszFile, "Section #%u '%s': Unexpected SHT_REL section\n", i, pszShNm);
        else if (paShdrs[i].sh_type == SHT_SYMTAB)
        {
            if (paShdrs[i].sh_entsize != sizeof(Elf64_Sym))
                fRet = error(pszFile, "Section #%u '%s': Unsupported symbol table entry size in : #%u (expected #%u)\n",
                             i, pszShNm, paShdrs[i].sh_entsize, sizeof(Elf64_Sym));
            uint32_t cSymbols = paShdrs[i].sh_size / paShdrs[i].sh_entsize;
            if ((Elf64_Xword)cSymbols * paShdrs[i].sh_entsize != paShdrs[i].sh_size)
                fRet = error(pszFile, "Section #%u '%s': Size not a multiple of entry size: %#" ELF_FMT_X64 " %% %#" ELF_FMT_X64 " = %#" ELF_FMT_X64 "\n",
                             i, pszShNm, paShdrs[i].sh_size, paShdrs[i].sh_entsize, paShdrs[i].sh_size % paShdrs[i].sh_entsize);
            if (pElfStuff->iSymSh == UINT16_MAX)
            {
                pElfStuff->iSymSh    = (uint16_t)i;
                pElfStuff->paSymbols = (Elf64_Sym const *)&pbFile[paShdrs[i].sh_offset];
                pElfStuff->cSymbols  = cSymbols;

                if (paShdrs[i].sh_link != 0)
                {
                    /* Note! The symbol string table section header may not have been validated yet! */
                    Elf64_Shdr const *pStrTabShdr = &paShdrs[paShdrs[i].sh_link];
                    pElfStuff->iStrSh    = paShdrs[i].sh_link;
                    pElfStuff->pchStrTab = (const char *)&pbFile[pStrTabShdr->sh_offset];
                    pElfStuff->cbStrTab  = (size_t)pStrTabShdr->sh_size;
                }
                else
                    fRet = error(pszFile, "Section #%u '%s': String table link is out of bounds (%#x)\n",
                                 i, pszShNm, paShdrs[i].sh_link);
            }
            else
                fRet = error(pszFile, "Section #%u '%s': Found additonal symbol table, previous in #%u\n",
                             i, pszShNm, pElfStuff->iSymSh);
        }
    }
    return fRet;
}


#ifdef ELF_TO_OMF_CONVERSION

static bool convertElfToOmf(const char *pszFile, uint8_t const *pbFile, size_t cbFile, FILE *pDst)
{
    /*
     * Validate the source file a little.
     */
    ELFDETAILS ElfStuff;
    if (!validateElf(pszFile, pbFile, cbFile, &ElfStuff))
        return false;

    /*
     * Instantiate the OMF writer.
     */
    PIMAGE_FILE_HEADER pHdr = (PIMAGE_FILE_HEADER)pbFile;
    POMFWRITER pThis = omfWriter_Create(pszFile, pHdr->NumberOfSections, pHdr->NumberOfSymbols, pDst);
    if (!pThis)
        return false;

    /*
     * Write the OMF object file.
     */
    if (omfWriter_BeginModule(pThis, pszFile))
    {
        Elf64_Ehdr const *pEhdr     = (Elf64_Ehdr const *)pbFile;
        Elf64_Shdr const *paShdrs   = (Elf64_Shdr const *)&pbFile[pEhdr->e_shoff];
        const char       *pszStrTab = (const char *)&pbFile[paShdrs[pEhdr->e_shstrndx].sh_offset];

        if (   convertElfSectionsToSegDefsAndGrpDefs(pThis, paShdrs, pHdr->NumberOfSections)
            //&& convertElfSymbolsToPubDefsAndExtDefs(pThis, paSymTab, pHdr->NumberOfSymbols, pchStrTab, paShdrs)
            && omfWriter_LinkPassSeparator(pThis)
            //&& convertElfSectionsToLeDataAndFixupps(pThis, pbFile, cbFile, paShdrs, pHdr->NumberOfSections,
            //                                        paSymTab, pHdr->NumberOfSymbols, pchStrTab)
            && omfWriter_EndModule(pThis) )
        {

            omfWriter_Destroy(pThis);
            return true;
        }
    }

    omfWriter_Destroy(pThis);
    return false;
}

#else /* !ELF_TO_OMF_CONVERSION */

static bool convertelf(const char *pszFile, uint8_t *pbFile, size_t cbFile)
{
    /*
     * Validate the header and our other expectations.
     */
    ELFDETAILS ElfStuff;
    if (!validateElf(pszFile, pbFile, cbFile, &ElfStuff))
        return false;

    /*
     * Locate the section name string table.
     * We assume it's okay as we only reference it in verbose mode.
     */
    Elf64_Ehdr const   *pEhdr     = (Elf64_Ehdr const *)pbFile;
    Elf64_Shdr const   *paShdrs   = (Elf64_Shdr const *)&pbFile[pEhdr->e_shoff];
    const char         *pszStrTab = (const char *)&pbFile[paShdrs[pEhdr->e_shstrndx].sh_offset];

    /*
     * Work the section table.
     */
    for (uint32_t i = 1; i < pEhdr->e_shnum; i++)
    {
        if (g_cVerbose)
            printf("shdr[%u]: name=%#x '%s' type=%#x flags=%#" ELF_FMT_X64 " addr=%#" ELF_FMT_X64 " off=%#" ELF_FMT_X64 " size=%#" ELF_FMT_X64 "\n"
                   "          link=%u info=%#x align=%#" ELF_FMT_X64 " entsize=%#" ELF_FMT_X64 "\n",
                   i, paShdrs[i].sh_name, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_type, paShdrs[i].sh_flags,
                   paShdrs[i].sh_addr, paShdrs[i].sh_offset, paShdrs[i].sh_size,
                   paShdrs[i].sh_link, paShdrs[i].sh_info, paShdrs[i].sh_addralign, paShdrs[i].sh_entsize);
        if (paShdrs[i].sh_type == SHT_RELA)
        {
            Elf64_Rela    *paRelocs = (Elf64_Rela *)&pbFile[paShdrs[i].sh_offset];
            uint32_t const cRelocs  = paShdrs[i].sh_size / sizeof(Elf64_Rela);
            for (uint32_t j = 0; j < cRelocs; j++)
            {
                uint8_t const bType = ELF64_R_TYPE(paRelocs[j].r_info);
                if (g_cVerbose > 1)
                    printf("%#018" ELF_FMT_X64 "  %#018" ELF_FMT_X64 " %s  %+" ELF_FMT_D64 "\n", paRelocs[j].r_offset, paRelocs[j].r_info,
                           bType < RT_ELEMENTS(g_apszElfAmd64RelTypes) ? g_apszElfAmd64RelTypes[bType] : "unknown", paRelocs[j].r_addend);

                /* Truncate 64-bit wide absolute relocations, ASSUMING that the high bits
                   are already zero and won't be non-zero after calculating the fixup value. */
                if (bType == R_X86_64_64)
                {
                    paRelocs[j].r_info &= ~(uint64_t)0xff;
                    paRelocs[j].r_info |= R_X86_64_32;
                }
            }
        }
        else if (paShdrs[i].sh_type == SHT_REL)
            return error(pszFile, "Did not expect SHT_REL sections (#%u '%s')\n", i, &pszStrTab[paShdrs[i].sh_name]);
    }
    return true;
}

#endif /* !ELF_TO_OMF_CONVERSION */




/*********************************************************************************************************************************
*   COFF -> OMF Converter                                                                                                        *
*********************************************************************************************************************************/

/** AMD64 relocation type names for (Microsoft) COFF. */
static const char * const g_apszCoffAmd64RelTypes[] =
{
    "ABSOLUTE",
    "ADDR64",
    "ADDR32",
    "ADDR32NB",
    "REL32",
    "REL32_1",
    "REL32_2",
    "REL32_3",
    "REL32_4",
    "REL32_5",
    "SECTION",
    "SECREL",
    "SECREL7",
    "TOKEN",
    "SREL32",
    "PAIR",
    "SSPAN32"
};

/** AMD64 relocation type sizes for (Microsoft) COFF. */
static uint8_t const g_acbCoffAmd64RelTypes[] =
{
    8, /* ABSOLUTE */
    8, /* ADDR64   */
    4, /* ADDR32   */
    4, /* ADDR32NB */
    4, /* REL32    */
    4, /* REL32_1  */
    4, /* REL32_2  */
    4, /* REL32_3  */
    4, /* REL32_4  */
    4, /* REL32_5  */
    2, /* SECTION  */
    4, /* SECREL   */
    1, /* SECREL7  */
    0, /* TOKEN    */
    4, /* SREL32   */
    0, /* PAIR     */
    4, /* SSPAN32  */
};

/** Macro for getting the size of a AMD64 COFF relocation. */
#define COFF_AMD64_RELOC_SIZE(a_Type) ( (a_Type) < RT_ELEMENTS(g_acbCoffAmd64RelTypes) ? g_acbCoffAmd64RelTypes[(a_Type)] : 1)


static const char *coffGetSymbolName(PCIMAGE_SYMBOL pSym, const char *pchStrTab, uint32_t cbStrTab, char pszShortName[16])
{
    if (pSym->N.Name.Short != 0)
    {
        memcpy(pszShortName, pSym->N.ShortName, 8);
        pszShortName[8] = '\0';
        return pszShortName;
    }
    if (pSym->N.Name.Long < cbStrTab)
    {
        uint32_t const cbLeft = cbStrTab - pSym->N.Name.Long;
        const char    *pszRet = pchStrTab + pSym->N.Name.Long;
        if (memchr(pszRet, '\0', cbLeft) != NULL)
            return pszRet;
    }
    error("<null>",  "Invalid string table index %#x!\n", pSym->N.Name.Long);
    return "Invalid Symbol Table Entry";
}

static bool validateCoff(const char *pszFile, uint8_t const *pbFile, size_t cbFile)
{
    /*
     * Validate the header and our other expectations.
     */
    PIMAGE_FILE_HEADER pHdr = (PIMAGE_FILE_HEADER)pbFile;
    if (pHdr->Machine != IMAGE_FILE_MACHINE_AMD64)
        return error(pszFile, "Expected IMAGE_FILE_MACHINE_AMD64 not %#x\n", pHdr->Machine);
    if (pHdr->SizeOfOptionalHeader != 0)
        return error(pszFile, "Expected SizeOfOptionalHeader to be zero, not %#x\n", pHdr->SizeOfOptionalHeader);
    if (pHdr->NumberOfSections == 0)
        return error(pszFile, "Expected NumberOfSections to be non-zero\n");
    uint32_t const cbHeaders = pHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER) + sizeof(*pHdr);
    if (cbHeaders > cbFile)
        return error(pszFile, "Section table goes beyond the end of the of the file (cSections=%#x)\n", pHdr->NumberOfSections);
    if (pHdr->NumberOfSymbols)
    {
        if (   pHdr->PointerToSymbolTable >= cbFile
            || pHdr->NumberOfSymbols * (uint64_t)IMAGE_SIZE_OF_SYMBOL > cbFile)
            return error(pszFile, "Symbol table goes beyond the end of the of the file (cSyms=%#x, offFile=%#x)\n",
                         pHdr->NumberOfSymbols, pHdr->PointerToSymbolTable);
    }

    return true;
}


static bool convertCoffSectionsToSegDefsAndGrpDefs(POMFWRITER pThis, PCIMAGE_SECTION_HEADER paShdrs, uint16_t cSections)
{
    /*
     * Do the list of names pass.
     */
    uint16_t idxGrpFlat, idxGrpData;
    uint16_t idxClassCode, idxClassData, idxClassDebugSymbols, idxClassDebugTypes;
    if (   !omfWriter_LNamesBegin(pThis)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("FLAT"), &idxGrpFlat)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("BS3DATA64_GROUP"), &idxGrpData)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("BS3CODE64"), &idxClassCode)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("FAR_DATA"), &idxClassData)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("DEBSYM"), &idxClassDebugSymbols)
        || !omfWriter_LNamesAddN(pThis, RT_STR_TUPLE("DEBTYP"), &idxClassDebugTypes)
       )
        return false;

    bool fHaveData = false;
    for (uint16_t i = 0; i < cSections; i++)
    {
        /* Copy the name and terminate it. */
        char szName[32];
        memcpy(szName, paShdrs[i].Name, sizeof(paShdrs[i].Name));
        unsigned cchName = sizeof(paShdrs[i].Name);
        while (cchName > 0 && RT_C_IS_SPACE(szName[cchName - 1]))
            cchName--;
        if (cchName == 0)
            return error(pThis->pszSrc, "Section #%u has an empty name!\n", i);
        szName[cchName] = '\0';

        if (   (paShdrs[i].Characteristics & (IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_LNK_INFO))
            || strcmp(szName, ".pdata") == 0 /* Exception stuff, I think, so discard it. */
            || strcmp(szName, ".xdata") == 0 /* Ditto. */ )
        {
            pThis->paSegments[i].iSegDef  = UINT16_MAX;
            pThis->paSegments[i].iGrpDef  = UINT16_MAX;
            pThis->paSegments[i].iSegNm   = UINT16_MAX;
            pThis->paSegments[i].iGrpNm   = UINT16_MAX;
            pThis->paSegments[i].iClassNm = UINT16_MAX;
            pThis->paSegments[i].pszName  = NULL;
        }
        else
        {
            /* Translate the name, group and class. */
            if (strcmp(szName, ".text") == 0)
            {
                strcpy(szName, "BS3TEXT64");
                pThis->paSegments[i].iGrpNm   = idxGrpFlat;
                pThis->paSegments[i].iClassNm = idxClassCode;
            }
            else if (strcmp(szName, ".data") == 0)
            {
                strcpy(szName, "BS3DATA64");
                pThis->paSegments[i].iGrpNm   = idxGrpData;
                pThis->paSegments[i].iClassNm = idxClassData;
            }
            else if (strcmp(szName, ".bss") == 0)
            {
                strcpy(szName, "BS3BSS64");
                pThis->paSegments[i].iGrpNm   = idxGrpData;
                pThis->paSegments[i].iClassNm = idxClassData;
            }
            else if (strcmp(szName, ".rdata") == 0)
            {
                strcpy(szName, "BS3DATA64CONST");
                pThis->paSegments[i].iGrpNm   = idxGrpData;
                pThis->paSegments[i].iClassNm = idxClassData;
            }
            else if (strcmp(szName, ".debug$S") == 0)
            {
                strcpy(szName, "$$SYMBOLS");
                pThis->paSegments[i].iGrpNm   = UINT16_MAX;
                pThis->paSegments[i].iClassNm = idxClassDebugSymbols;
            }
            else if (strcmp(szName, ".debug$T") == 0)
            {
                strcpy(szName, "$$TYPES");
                pThis->paSegments[i].iGrpNm   = UINT16_MAX;
                pThis->paSegments[i].iClassNm = idxClassDebugTypes;
            }
            else if (paShdrs[i].Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE))
            {
                pThis->paSegments[i].iGrpNm   = idxGrpFlat;
                pThis->paSegments[i].iClassNm = idxClassCode;
                error(pThis->pszSrc, "Unknown code segment: '%s'\n", szName);
            }
            else
            {
                pThis->paSegments[i].iGrpNm = idxGrpData;
                pThis->paSegments[i].iClassNm = idxClassData;
                error(pThis->pszSrc, "Unknown data (?) segment: '%s'\n", szName);
            }

            /* Save the name. */
            pThis->paSegments[i].pszName = strdup(szName);
            if (!pThis->paSegments[i].pszName)
                return error(pThis->pszSrc, "Out of memory!\n");

            /* Add the section name. */
            if (!omfWriter_LNamesAdd(pThis, pThis->paSegments[i].pszName, &pThis->paSegments[i].iSegNm))
                return false;

            fHaveData |= pThis->paSegments[i].iGrpDef == idxGrpData;
        }
    }

    if (!omfWriter_LNamesEnd(pThis))
        return false;

    /*
     * Emit segment definitions.
     */
    uint16_t iSegDef = 1; /* Start counting at 1. */
    for (uint16_t i = 0; i < cSections; i++)
    {
        if (pThis->paSegments[i].iSegDef == UINT16_MAX)
            continue;

        uint8_t bSegAttr = 0;

        /* The A field. */
        switch (paShdrs[i].Characteristics & IMAGE_SCN_ALIGN_MASK)
        {
            default:
            case IMAGE_SCN_ALIGN_1BYTES:
                bSegAttr |= 1 << 5;
                break;
            case IMAGE_SCN_ALIGN_2BYTES:
                bSegAttr |= 2 << 5;
                break;
            case IMAGE_SCN_ALIGN_4BYTES:
                bSegAttr |= 5 << 5;
                break;
            case IMAGE_SCN_ALIGN_8BYTES:
            case IMAGE_SCN_ALIGN_16BYTES:
                bSegAttr |= 3 << 5;
                break;
            case IMAGE_SCN_ALIGN_32BYTES:
            case IMAGE_SCN_ALIGN_64BYTES:
            case IMAGE_SCN_ALIGN_128BYTES:
            case IMAGE_SCN_ALIGN_256BYTES:
                bSegAttr |= 4 << 5;
                break;
            case IMAGE_SCN_ALIGN_512BYTES:
            case IMAGE_SCN_ALIGN_1024BYTES:
            case IMAGE_SCN_ALIGN_2048BYTES:
            case IMAGE_SCN_ALIGN_4096BYTES:
            case IMAGE_SCN_ALIGN_8192BYTES:
                bSegAttr |= 6 << 5; /* page aligned, pharlabs extension. */
                break;
        }

        /* The C field. */
        bSegAttr |= 2 << 2; /* public */

        /* The B field. We don't have 4GB segments, so leave it as zero. */

        /* The D field shall be set as we're doing USE32.  */
        bSegAttr |= 1;


        /* Done. */
        if (!omfWriter_SegDef(pThis, bSegAttr, paShdrs[i].SizeOfRawData,
                              pThis->paSegments[i].iSegNm,
                              pThis->paSegments[i].iClassNm))
            return false;
        pThis->paSegments[i].iSegDef = iSegDef++;
    }

    /*
     * Flat group definition (#1) - special, no members.
     */
    uint16_t iGrpDef = 1;
    if (   !omfWriter_GrpDefBegin(pThis, idxGrpFlat)
        || !omfWriter_GrpDefEnd(pThis))
        return false;
    for (uint16_t i = 0; i < cSections; i++)
        if (pThis->paSegments[i].iGrpNm == idxGrpFlat)
            pThis->paSegments[i].iGrpDef = iGrpDef;
    pThis->idxGrpFlat = iGrpDef++;

    /*
     * Data group definition (#2).
     */
    /** @todo do we need to consider missing segments and ordering? */
    uint16_t cGrpNms = 0;
    uint16_t aiGrpNms[2];
    if (fHaveData)
        aiGrpNms[cGrpNms++] = idxGrpData;
    for (uint32_t iGrpNm = 0; iGrpNm < cGrpNms; iGrpNm++)
    {
        if (!omfWriter_GrpDefBegin(pThis, aiGrpNms[iGrpNm]))
            return false;
        for (uint16_t i = 0; i < cSections; i++)
            if (pThis->paSegments[i].iGrpNm == aiGrpNms[iGrpNm])
            {
                pThis->paSegments[i].iGrpDef = iGrpDef;
                if (!omfWriter_GrpDefAddSegDef(pThis, pThis->paSegments[i].iSegDef))
                    return false;
            }
        if (!omfWriter_GrpDefEnd(pThis))
            return false;
        iGrpDef++;
    }

    return true;
}

/**
 * This is for matching STATIC symbols with value 0 against the section name,
 * to see if it's a section reference or symbol at offset 0 reference.
 *
 * @returns true / false.
 * @param   pszSymbol       The symbol name.
 * @param   pachSectName8   The section name (8-bytes).
 */
static bool isCoffSymbolMatchingSectionName(const char *pszSymbol, uint8_t const pachSectName8[8])
{
    uint32_t off = 0;
    char ch;
    while (off < 8 && (ch = pszSymbol[off]) != '\0')
    {
        if (ch != pachSectName8[off])
            return false;
        off++;
    }
    while (off < 8)
    {
        if (!RT_C_IS_SPACE((ch = pachSectName8[off])))
            return ch == '\0';
        off++;
    }
    return true;
}

static bool convertCoffSymbolsToPubDefsAndExtDefs(POMFWRITER pThis, PCIMAGE_SYMBOL paSymbols, uint16_t cSymbols,
                                                  const char *pchStrTab, PCIMAGE_SECTION_HEADER paShdrs)
{

    if (!cSymbols)
        return true;
    uint32_t const  cbStrTab = *(uint32_t const *)pchStrTab;
    char            szShort[16];

    /*
     * Process the symbols the first.
     */
    uint32_t iSymImageBase = UINT32_MAX;
    uint32_t cAbsSyms = 0;
    uint32_t cExtSyms = 0;
    uint32_t cPubSyms = 0;
    for (uint32_t iSeg = 0; iSeg < pThis->cSegments; iSeg++)
        pThis->paSegments[iSeg].cPubDefs = 0;

    for (uint16_t iSym = 0; iSym < cSymbols; iSym++)
    {
        const char *pszSymName = coffGetSymbolName(&paSymbols[iSym], pchStrTab, cbStrTab, szShort);

        pThis->paSymbols[iSym].enmType   = OMFSYMTYPE_IGNORED;
        pThis->paSymbols[iSym].idx       = UINT16_MAX;
        pThis->paSymbols[iSym].idxSegDef = UINT16_MAX;
        pThis->paSymbols[iSym].idxGrpDef = UINT16_MAX;

        int16_t const idxSection = paSymbols[iSym].SectionNumber;
        if (   (idxSection >= 1 && idxSection <= (int32_t)pThis->cSegments)
            || idxSection == IMAGE_SYM_ABSOLUTE)
        {
            switch (paSymbols[iSym].StorageClass)
            {
                case IMAGE_SYM_CLASS_EXTERNAL:
                    if (idxSection != IMAGE_SYM_ABSOLUTE)
                    {
                        if (pThis->paSegments[idxSection - 1].iSegDef != UINT16_MAX)
                        {
                            pThis->paSymbols[iSym].enmType   = OMFSYMTYPE_PUBDEF;
                            pThis->paSymbols[iSym].idxSegDef = pThis->paSegments[idxSection - 1].iSegDef;
                            pThis->paSymbols[iSym].idxGrpDef = pThis->paSegments[idxSection - 1].iGrpDef;
                            pThis->paSegments[idxSection - 1].cPubDefs++;
                            cPubSyms++;
                        }
                    }
                    else
                    {
                        pThis->paSymbols[iSym].enmType   = OMFSYMTYPE_PUBDEF;
                        pThis->paSymbols[iSym].idxSegDef = 0;
                        pThis->paSymbols[iSym].idxGrpDef = 0;
                        cAbsSyms++;
                    }
                    break;

                case IMAGE_SYM_CLASS_STATIC:
                    if (   paSymbols[iSym].Value == 0
                        && idxSection != IMAGE_SYM_ABSOLUTE
                        && isCoffSymbolMatchingSectionName(pszSymName, paShdrs[idxSection - 1].Name) )
                    {
                        pThis->paSymbols[iSym].enmType   = OMFSYMTYPE_SEGDEF;
                        pThis->paSymbols[iSym].idxSegDef = pThis->paSegments[idxSection - 1].iSegDef;
                        pThis->paSymbols[iSym].idxGrpDef = pThis->paSegments[idxSection - 1].iGrpDef;
                        break;
                    }
                    /* fall thru */

                case IMAGE_SYM_CLASS_END_OF_FUNCTION:
                case IMAGE_SYM_CLASS_AUTOMATIC:
                case IMAGE_SYM_CLASS_REGISTER:
                case IMAGE_SYM_CLASS_LABEL:
                case IMAGE_SYM_CLASS_MEMBER_OF_STRUCT:
                case IMAGE_SYM_CLASS_ARGUMENT:
                case IMAGE_SYM_CLASS_STRUCT_TAG:
                case IMAGE_SYM_CLASS_MEMBER_OF_UNION:
                case IMAGE_SYM_CLASS_UNION_TAG:
                case IMAGE_SYM_CLASS_TYPE_DEFINITION:
                case IMAGE_SYM_CLASS_ENUM_TAG:
                case IMAGE_SYM_CLASS_MEMBER_OF_ENUM:
                case IMAGE_SYM_CLASS_REGISTER_PARAM:
                case IMAGE_SYM_CLASS_BIT_FIELD:
                case IMAGE_SYM_CLASS_BLOCK:
                case IMAGE_SYM_CLASS_FUNCTION:
                case IMAGE_SYM_CLASS_END_OF_STRUCT:
                case IMAGE_SYM_CLASS_FILE:
                    pThis->paSymbols[iSym].enmType = OMFSYMTYPE_INTERNAL;
                    if (idxSection != IMAGE_SYM_ABSOLUTE)
                    {
                        pThis->paSymbols[iSym].idxSegDef = pThis->paSegments[idxSection - 1].iSegDef;
                        pThis->paSymbols[iSym].idxGrpDef = pThis->paSegments[idxSection - 1].iGrpDef;
                    }
                    else
                    {
                        pThis->paSymbols[iSym].idxSegDef = 0;
                        pThis->paSymbols[iSym].idxGrpDef = 0;
                    }
                    break;

                case IMAGE_SYM_CLASS_SECTION:
                case IMAGE_SYM_CLASS_EXTERNAL_DEF:
                case IMAGE_SYM_CLASS_NULL:
                case IMAGE_SYM_CLASS_UNDEFINED_LABEL:
                case IMAGE_SYM_CLASS_UNDEFINED_STATIC:
                case IMAGE_SYM_CLASS_CLR_TOKEN:
                case IMAGE_SYM_CLASS_FAR_EXTERNAL:
                case IMAGE_SYM_CLASS_WEAK_EXTERNAL:
                    return error(pThis->pszSrc, "Unsupported storage class value %#x for symbol #%u (%s)\n",
                                 paSymbols[iSym].StorageClass, iSym, pszSymName);

                default:
                    return error(pThis->pszSrc, "Unknown storage class value %#x for symbol #%u (%s)\n",
                                 paSymbols[iSym].StorageClass, iSym, pszSymName);
            }
        }
        else if (idxSection == IMAGE_SYM_UNDEFINED)
        {
            if (   paSymbols[iSym].StorageClass == IMAGE_SYM_CLASS_EXTERNAL
                || paSymbols[iSym].StorageClass == IMAGE_SYM_CLASS_EXTERNAL_DEF)
            {
                pThis->paSymbols[iSym].enmType = OMFSYMTYPE_EXTDEF;
                cExtSyms++;
                if (iSymImageBase == UINT32_MAX && strcmp(pszSymName, "__ImageBase") == 0)
                    iSymImageBase = iSym;
            }
            else
                return error(pThis->pszSrc, "Unknown/unknown storage class value %#x for undefined symbol #%u (%s)\n",
                             paSymbols[iSym].StorageClass, iSym, pszSymName);
        }
        else if (idxSection != IMAGE_SYM_DEBUG)
            return error(pThis->pszSrc, "Invalid section number %#x for symbol #%u (%s)\n", idxSection, iSym, pszSymName);

        /* Skip AUX symbols. */
        uint8_t cAuxSyms = paSymbols[iSym].NumberOfAuxSymbols;
        while (cAuxSyms-- > 0)
        {
            iSym++;
            pThis->paSymbols[iSym].enmType = OMFSYMTYPE_INVALID;
            pThis->paSymbols[iSym].idx     = UINT16_MAX;
        }
    }

    /*
     * Emit the PUBDEFs the first time around (see order of records in TIS spec).
     */
    uint16_t idxPubDef = 1;
    if (cPubSyms)
    {
        for (uint32_t iSeg = 0; iSeg < pThis->cSegments; iSeg++)
            if (pThis->paSegments[iSeg].cPubDefs > 0)
            {
                uint16_t const idxSegDef = pThis->paSegments[iSeg].iSegDef;
                if (!omfWriter_PubDefBegin(pThis, pThis->paSegments[iSeg].iGrpDef, idxSegDef))
                    return false;
                for (uint16_t iSym = 0; iSym < cSymbols; iSym++)
                    if (   pThis->paSymbols[iSym].idxSegDef == idxSegDef
                        && pThis->paSymbols[iSym].enmType   == OMFSYMTYPE_PUBDEF)
                    {
                        const char *pszName = coffGetSymbolName(&paSymbols[iSym], pchStrTab, cbStrTab, szShort);
                        if (!omfWriter_PubDefAdd(pThis, paSymbols[iSym].Value, pszName))
                            return false;

                        /* If the symbol doesn't start with an underscore, add an underscore
                           prefixed alias to ease access from 16-bit and 32-bit code. */
                        if (*pszName != '_')
                        {
                            char   szCdeclName[512];
                            size_t cchName = strlen(pszName);
                            if (cchName > sizeof(szCdeclName) - 2)
                                cchName = sizeof(szCdeclName) - 2;
                            szCdeclName[0] = '_';
                            memcpy(&szCdeclName[1], pszName, cchName);
                            szCdeclName[cchName + 1] = '\0';
                            if (!omfWriter_PubDefAdd(pThis, paSymbols[iSym].Value, szCdeclName))
                                return false;
                        }

                        pThis->paSymbols[iSym].idx = idxPubDef++;
                    }
                if (!omfWriter_PubDefEnd(pThis))
                    return false;
            }
    }

    if (cAbsSyms > 0)
    {
        if (!omfWriter_PubDefBegin(pThis, 0, 0))
            return false;
        for (uint16_t iSym = 0; iSym < cSymbols; iSym++)
            if (   pThis->paSymbols[iSym].idxSegDef == 0
                && pThis->paSymbols[iSym].enmType   == OMFSYMTYPE_PUBDEF)
            {
                if (!omfWriter_PubDefAdd(pThis, paSymbols[iSym].Value,
                                         coffGetSymbolName(&paSymbols[iSym], pchStrTab, cbStrTab, szShort)) )
                    return false;
                pThis->paSymbols[iSym].idx = idxPubDef++;
            }
        if (!omfWriter_PubDefEnd(pThis))
            return false;
    }

    /*
     * Go over the symbol table and emit external definition records.
     */
    if (!omfWriter_ExtDefBegin(pThis))
        return false;
    uint16_t idxExtDef = 1;
    for (uint16_t iSym = 0; iSym < cSymbols; iSym++)
        if (pThis->paSymbols[iSym].enmType == OMFSYMTYPE_EXTDEF)
        {
            if (!omfWriter_ExtDefAdd(pThis, coffGetSymbolName(&paSymbols[iSym], pchStrTab, cbStrTab, szShort)))
                return false;
            pThis->paSymbols[iSym].idx = idxExtDef++;
        }

    /* Always add an __ImageBase reference, in case we need it to deal with ADDR32NB fixups. */
    /** @todo maybe we don't actually need this and could use FLAT instead? */
    if (iSymImageBase != UINT32_MAX)
        pThis->idxExtImageBase = pThis->paSymbols[iSymImageBase].idx;
    else if (omfWriter_ExtDefAdd(pThis, "__ImageBase"))
        pThis->idxExtImageBase = idxExtDef;
    else
        return false;

    if (!omfWriter_ExtDefEnd(pThis))
        return false;

    return true;
}


static bool convertCoffSectionsToLeDataAndFixupps(POMFWRITER pThis, uint8_t const *pbFile, size_t cbFile,
                                                  PCIMAGE_SECTION_HEADER paShdrs, uint16_t cSections,
                                                  PCIMAGE_SYMBOL paSymbols, uint16_t cSymbols, const char *pchStrTab)
{
    uint32_t const  cbStrTab = *(uint32_t const *)pchStrTab;
    bool            fRet     = true;
    for (uint32_t i = 0; i < pThis->cSegments; i++)
    {
        if (pThis->paSegments[i].iSegDef == UINT16_MAX)
            continue;

        char                szShortName[16];
        const char         *pszSegNm   = pThis->paSegments[i].pszName;
        uint16_t            cRelocs    = paShdrs[i].NumberOfRelocations;
        PCIMAGE_RELOCATION  paRelocs   = (PCIMAGE_RELOCATION)&pbFile[paShdrs[i].PointerToRelocations];
        uint32_t            cbVirtData = paShdrs[i].SizeOfRawData;
        uint32_t            cbData     = paShdrs[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA ? 0 : cbVirtData;
        uint8_t const      *pbData     = &pbFile[paShdrs[i].PointerToRawData];
        uint32_t            off        = 0;

        /* Check that the relocations are sorted and within the section. */
        for (uint32_t iReloc = 1; iReloc < cRelocs; iReloc++)
            if (paRelocs[iReloc - 1].u.VirtualAddress >= paRelocs[iReloc].u.VirtualAddress)
                return error(pThis->pszSrc, "Section #%u (%s) relocations aren't sorted\n", i, pszSegNm);
        if (   cRelocs > 0
            &&    paRelocs[cRelocs - 1].u.VirtualAddress - paShdrs[i].VirtualAddress
               +  COFF_AMD64_RELOC_SIZE(paRelocs[cRelocs - 1].Type) > cbVirtData)
            return error(pThis->pszSrc,
                         "Section #%u (%s) relocations beyond section data! cbVirtData=%#x RvaFix=%#x RVASeg=%#x type=%#x\n",
                         i, pszSegNm, cbVirtData, paRelocs[cRelocs - 1].u.VirtualAddress, paShdrs[i].VirtualAddress,
                         paRelocs[cRelocs - 1].Type);

        /* The OMF record size requires us to split larger sections up.  To make
           life simple, we fill zeros for unitialized (BSS) stuff. */
        const uint32_t cbMaxData = RT_MIN(OMF_MAX_RECORD_PAYLOAD - 1 - (pThis->paSegments[i].iSegDef >= 128) - 4 - 1, _1K);
        while (cbVirtData > 0)
        {
            /* Figure out how many bytes to put out in this chunk.  Must make sure
               fixups doesn't cross chunk boundraries.  ASSUMES sorted relocs. */
            uint32_t       cChunkRelocs = cRelocs;
            uint32_t       cbChunk      = cbVirtData;
            uint32_t       uRvaEnd      = paShdrs[i].VirtualAddress + off + cbChunk;
            if (cbChunk > cbMaxData)
            {
                cbChunk      = cbMaxData;
                uRvaEnd      = paShdrs[i].VirtualAddress + off + cbChunk;
                cChunkRelocs = 0;

                /* Quickly determin the reloc range. */
                while (   cChunkRelocs < cRelocs
                       && paRelocs[cChunkRelocs].u.VirtualAddress < uRvaEnd)
                    cChunkRelocs++;

                /* Ensure final reloc doesn't go beyond chunk. */
                while (   cChunkRelocs > 0
                       &&   paRelocs[cChunkRelocs - 1].u.VirtualAddress + COFF_AMD64_RELOC_SIZE(paRelocs[cChunkRelocs - 1].Type)
                          > uRvaEnd)
                {
                    uint32_t cbDrop = uRvaEnd - paRelocs[cChunkRelocs - 1].u.VirtualAddress;
                    cbChunk -= cbDrop;
                    uRvaEnd -= cbDrop;
                    cChunkRelocs--;
                }

                if (!cbVirtData)
                    return error(pThis->pszSrc, "Wtf? cbVirtData is zero!\n");
            }

            /*
             * We stash the bytes into the OMF writer record buffer, receiving a
             * pointer to the start of it so we can make adjustments if necessary.
             */
            uint8_t *pbCopy;
            if (!omfWriter_LEDataBegin(pThis, pThis->paSegments[i].iSegDef, off, cbChunk, cbData, pbData, &pbCopy))
                return false;

            /*
             * Convert fiuxps.
             */
            uint32_t const uRvaChunk = paShdrs[i].VirtualAddress + off;
            for (uint32_t iReloc = 0; iReloc < cChunkRelocs; iReloc++)
            {
                /* Get the OMF and COFF data for the symbol the reloc references. */
                if (paRelocs[iReloc].SymbolTableIndex >= pThis->cSymbols)
                    return error(pThis->pszSrc, "Relocation symtab index (%#x) is out of range in segment #%u '%s'\n",
                                 paRelocs[iReloc].SymbolTableIndex, i, pszSegNm);
                PCIMAGE_SYMBOL pCoffSym =        &paSymbols[paRelocs[iReloc].SymbolTableIndex];
                POMFSYMBOL     pOmfSym  = &pThis->paSymbols[paRelocs[iReloc].SymbolTableIndex];

                /* Calc fixup location in the pending chunk and setup a flexible pointer to it. */
                uint16_t  offDataRec = (uint16_t)paRelocs[iReloc].u.VirtualAddress - uRvaChunk;
                RTPTRUNION uLoc;
                uLoc.pu8 = &pbCopy[offDataRec];

                /* OMF fixup data initialized with typical defaults. */
                bool        fSelfRel  = true;
                uint8_t     bLocation = OMF_FIX_LOC_32BIT_OFFSET;
                uint8_t     bFrame    = OMF_FIX_F_GRPDEF;
                uint16_t    idxFrame  = pThis->idxGrpFlat;
                uint8_t     bTarget;
                uint16_t    idxTarget;
                bool        fTargetDisp;
                uint32_t    offTargetDisp;
                switch (pOmfSym->enmType)
                {
                    case OMFSYMTYPE_INTERNAL:
                    case OMFSYMTYPE_PUBDEF:
                        bTarget       = OMF_FIX_T_SEGDEF;
                        idxTarget     = pOmfSym->idxSegDef;
                        fTargetDisp   = true;
                        offTargetDisp = pCoffSym->Value;
                        break;

                    case OMFSYMTYPE_SEGDEF:
                        bTarget       = OMF_FIX_T_SEGDEF_NO_DISP;
                        idxTarget     = pOmfSym->idxSegDef;
                        fTargetDisp   = false;
                        offTargetDisp = 0;
                        break;

                    case OMFSYMTYPE_EXTDEF:
                        bTarget       = OMF_FIX_T_EXTDEF_NO_DISP;
                        idxTarget     = pOmfSym->idx;
                        fTargetDisp   = false;
                        offTargetDisp = 0;
                        break;

                    default:
                        return error(pThis->pszSrc, "Relocation in segment #%u '%s' references ignored or invalid symbol (%s)\n",
                                     i, pszSegNm, coffGetSymbolName(pCoffSym, pchStrTab, cbStrTab, szShortName));
                }

                /* Do COFF relocation type conversion. */
                switch (paRelocs[iReloc].Type)
                {
                    case IMAGE_REL_AMD64_ADDR64:
                    {
                        uint64_t uAddend = *uLoc.pu64;
                        if (uAddend > _1G)
                            fRet = error(pThis->pszSrc, "ADDR64 with large addend (%#llx) at %#x in segment #%u '%s'\n",
                                         uAddend, paRelocs[iReloc].u.VirtualAddress, i, pszSegNm);
                        fSelfRel = false;
                        break;
                    }

                    case IMAGE_REL_AMD64_REL32_1:
                    case IMAGE_REL_AMD64_REL32_2:
                    case IMAGE_REL_AMD64_REL32_3:
                    case IMAGE_REL_AMD64_REL32_4:
                    case IMAGE_REL_AMD64_REL32_5:
                        /** @todo Check whether OMF read addends from the data or relies on the
                         *        displacement. Also, check what it's relative to. */
                        *uLoc.pu32 += paRelocs[iReloc].Type - IMAGE_REL_AMD64_REL32;
                        break;

                    case IMAGE_REL_AMD64_ADDR32:
                        fSelfRel = false;
                        break;

                    case IMAGE_REL_AMD64_ADDR32NB:
                        fSelfRel = false;
                        bFrame   = OMF_FIX_F_EXTDEF;
                        idxFrame = pThis->idxExtImageBase;
                        break;

                    case IMAGE_REL_AMD64_REL32:
                        /* defaults are ok. */
                        break;

                    case IMAGE_REL_AMD64_SECTION:
                        bLocation = OMF_FIX_LOC_16BIT_SEGMENT;
                        /* fall thru */

                    case IMAGE_REL_AMD64_SECREL:
                        fSelfRel = false;
                        if (pOmfSym->enmType == OMFSYMTYPE_EXTDEF)
                        {
                            bFrame   = OMF_FIX_F_EXTDEF;
                            idxFrame = pOmfSym->idx;
                        }
                        else
                        {
                            bFrame   = OMF_FIX_F_SEGDEF;
                            idxFrame = pOmfSym->idxSegDef;
                        }
                        break;

                    case IMAGE_REL_AMD64_ABSOLUTE:
                        continue; /* Ignore it like the PECOFF.DOC says we should. */

                    case IMAGE_REL_AMD64_SECREL7:
                    default:
                        return error(pThis->pszSrc, "Unsupported fixup type %#x (%s) at rva=%#x in section #%u '%-8.8s'\n",
                                     paRelocs[iReloc].Type,
                                     paRelocs[iReloc].Type < RT_ELEMENTS(g_apszCoffAmd64RelTypes)
                                     ? g_apszCoffAmd64RelTypes[paRelocs[iReloc].Type] : "unknown",
                                     paRelocs[iReloc].u.VirtualAddress, i, paShdrs[i].Name);
                }

                /* Add the fixup. */
                if (idxFrame == UINT16_MAX)
                    error(pThis->pszSrc, "idxFrame=UINT16_MAX for %s type=%s\n",
                          coffGetSymbolName(pCoffSym, pchStrTab, cbStrTab, szShortName),
                          g_apszCoffAmd64RelTypes[paRelocs[iReloc].Type]);
                fRet = omfWriter_LEDataAddFixup(pThis, offDataRec, fSelfRel, bLocation, bFrame, idxFrame,
                                                bTarget, idxTarget, fTargetDisp, offTargetDisp) && fRet;
            }

            /*
             * Write the LEDATA and associated FIXUPPs.
             */
            if (!omfWriter_LEDataEnd(pThis))
                return false;

            /*
             * Advance.
             */
            paRelocs   += cChunkRelocs;
            cRelocs    -= cChunkRelocs;
            if (cbData > cbChunk)
            {
                cbData -= cbChunk;
                pbData += cbChunk;
            }
            else
                cbData  = 0;
            off        += cbChunk;
            cbVirtData -= cbChunk;
        }
    }

    return fRet;
}


static bool convertCoffToOmf(const char *pszFile, uint8_t const *pbFile, size_t cbFile, FILE *pDst)
{
    /*
     * Validate the source file a little.
     */
    if (!validateCoff(pszFile, pbFile, cbFile))
        return false;

    /*
     * Instantiate the OMF writer.
     */
    PIMAGE_FILE_HEADER pHdr = (PIMAGE_FILE_HEADER)pbFile;
    POMFWRITER pThis = omfWriter_Create(pszFile, pHdr->NumberOfSections, pHdr->NumberOfSymbols, pDst);
    if (!pThis)
        return false;

    /*
     * Write the OMF object file.
     */
    if (omfWriter_BeginModule(pThis, pszFile))
    {
        PCIMAGE_SECTION_HEADER paShdrs   = (PCIMAGE_SECTION_HEADER)(pHdr + 1);
        PCIMAGE_SYMBOL         paSymTab  = (PCIMAGE_SYMBOL)&pbFile[pHdr->PointerToSymbolTable];
        const char            *pchStrTab = (const char *)&paSymTab[pHdr->NumberOfSymbols];
        if (   convertCoffSectionsToSegDefsAndGrpDefs(pThis, paShdrs, pHdr->NumberOfSections)
            && convertCoffSymbolsToPubDefsAndExtDefs(pThis, paSymTab, pHdr->NumberOfSymbols, pchStrTab, paShdrs)
            && omfWriter_LinkPassSeparator(pThis)
            && convertCoffSectionsToLeDataAndFixupps(pThis, pbFile, cbFile, paShdrs, pHdr->NumberOfSections,
                                                     paSymTab, pHdr->NumberOfSymbols, pchStrTab)
            && omfWriter_EndModule(pThis) )
        {

            omfWriter_Destroy(pThis);
            return true;
        }
    }

    omfWriter_Destroy(pThis);
    return false;
}



/*********************************************************************************************************************************
*   OMF Converter/Tweaker                                                                                                        *
*********************************************************************************************************************************/

/** Watcom intrinsics we need to modify so we can mix 32-bit and 16-bit
 * code, since the 16 and 32 bit compilers share several names.
 * The names are length prefixed.
 */
static const char * const g_apszExtDefRenames[] =
{
    "\x05" "__I4D",
    "\x05" "__I4M",
    "\x05" "__I8D",
    "\x06" "__I8DQ",
    "\x07" "__I8DQE",
    "\x06" "__I8DR",
    "\x07" "__I8DRE",
    "\x06" "__I8LS",
    "\x05" "__I8M",
    "\x06" "__I8ME",
    "\x06" "__I8RS",
    "\x05" "__PIA",
    "\x05" "__PIS",
    "\x05" "__PTC",
    "\x05" "__PTS",
    "\x05" "__U4D",
    "\x05" "__U4M",
    "\x05" "__U8D",
    "\x06" "__U8DQ",
    "\x07" "__U8DQE",
    "\x06" "__U8DR",
    "\x07" "__U8DRE",
    "\x06" "__U8LS",
    "\x05" "__U8M",
    "\x06" "__U8ME",
    "\x06" "__U8RS",
};

/**
 * Renames references to intrinsic helper functions so they won't clash between
 * 32-bit and 16-bit code.
 *
 * @returns true / false.
 * @param   pszFile     File name for complaining.
 * @param   pbFile      Pointer to the file content.
 * @param   cbFile      Size of the file content.
 */
static bool convertomf(const char *pszFile, uint8_t *pbFile, size_t cbFile, const char **papchLNames, uint32_t cLNamesMax)
{
    uint32_t        cLNames = 0;
    uint32_t        cExtDefs = 0;
    uint32_t        cPubDefs = 0;
    bool            fProbably32bit = false;
    uint32_t        off = 0;

    while (off + 3 < cbFile)
    {
        uint8_t     bRecType = pbFile[off];
        uint16_t    cbRec    = RT_MAKE_U16(pbFile[off + 1], pbFile[off + 2]);
        if (g_cVerbose > 2)
            printf( "%#07x: type=%#04x len=%#06x\n", off, bRecType, cbRec);
        if (off + cbRec > cbFile)
            return error(pszFile, "Invalid record length at %#x: %#x (cbFile=%#lx)\n", off, cbRec, (unsigned long)cbFile);

        if (bRecType & OMF_REC32)
            fProbably32bit = true;

        uint32_t offRec = 0;
        uint8_t *pbRec  = &pbFile[off + 3];
#define OMF_CHECK_RET(a_cbReq, a_Name) /* Not taking the checksum into account, so we're good with 1 or 2 byte fields. */ \
            if (offRec + (a_cbReq) <= cbRec) {/*likely*/} \
            else return error(pszFile, "Malformed " #a_Name "! off=%#x offRec=%#x cbRec=%#x cbNeeded=%#x line=%d\n", \
                              off, offRec, cbRec, (a_cbReq), __LINE__)
        switch (bRecType)
        {
            /*
             * Scan external definitions for intrinsics needing mangling.
             */
            case OMF_EXTDEF:
            {
                while (offRec + 1 < cbRec)
                {
                    uint8_t cch = pbRec[offRec++];
                    OMF_CHECK_RET(cch, EXTDEF);
                    char *pchName = (char *)&pbRec[offRec];
                    offRec += cch;

                    OMF_CHECK_RET(2, EXTDEF);
                    uint16_t idxType = pbRec[offRec++];
                    if (idxType & 0x80)
                        idxType = ((idxType & 0x7f) << 8) | pbRec[offRec++];

                    if (g_cVerbose > 2)
                        printf("  EXTDEF [%u]: %-*.*s type=%#x\n", cExtDefs, cch, cch, pchName, idxType);
                    else if (g_cVerbose > 0)
                        printf("              U %-*.*s\n", cch, cch, pchName);

                    /* Look for g_apszExtDefRenames entries that requires changing. */
                    if (   cch >= 5
                        && cch <= 7
                        && pchName[0] == '_'
                        && pchName[1] == '_'
                        && (   pchName[2] == 'U'
                            || pchName[2] == 'I'
                            || pchName[2] == 'P')
                        && (   pchName[3] == '4'
                            || pchName[3] == '8'
                            || pchName[3] == 'I'
                            || pchName[3] == 'T') )
                    {
                        uint32_t i = RT_ELEMENTS(g_apszExtDefRenames);
                        while (i-- > 0)
                            if (   cch == (uint8_t)g_apszExtDefRenames[i][0]
                                && memcmp(&g_apszExtDefRenames[i][1], pchName, cch) == 0)
                            {
                                pchName[0] = fProbably32bit ? '?' : '_';
                                pchName[1] = '?';
                                break;
                            }
                    }

                    cExtDefs++;
                }
                break;
            }

            /*
             * Record LNAME records, scanning for FLAT.
             */
            case OMF_LNAMES:
            {
                while (offRec + 1 < cbRec)
                {
                    uint8_t cch = pbRec[offRec];
                    if (offRec + 1 + cch >= cbRec)
                        return error(pszFile, "Invalid LNAME string length at %#x+3+%#x: %#x (cbFile=%#lx)\n",
                                     off, offRec, cch, (unsigned long)cbFile);
                    if (cLNames + 1 >= cLNamesMax)
                        return error(pszFile, "Too many LNAME strings\n");

                    if (g_cVerbose > 2)
                        printf("  LNAME[%u]: %-*.*s\n", cLNames, cch, cch, &pbRec[offRec + 1]);

                    papchLNames[cLNames++] = (const char *)&pbRec[offRec];
                    if (cch == 4 && memcmp(&pbRec[offRec + 1], "FLAT", 4) == 0)
                        fProbably32bit = true;

                    offRec += cch + 1;
                }
                break;
            }

            /*
             * Display public names if -v is specified.
             */
            case OMF_PUBDEF16:
            case OMF_PUBDEF32:
            case OMF_LPUBDEF16:
            case OMF_LPUBDEF32:
            {
                if (g_cVerbose > 0)
                {
                    char const  chType  = bRecType == OMF_PUBDEF16 || bRecType == OMF_PUBDEF32 ? 'T' : 't';
                    const char *pszRec = "LPUBDEF";
                    if (chType == 'T')
                        pszRec++;

                    OMF_CHECK_RET(2, [L]PUBDEF);
                    uint16_t idxGrp = pbRec[offRec++];
                    if (idxGrp & 0x80)
                        idxGrp = ((idxGrp & 0x7f) << 8) | pbRec[offRec++];

                    OMF_CHECK_RET(2, [L]PUBDEF);
                    uint16_t idxSeg = pbRec[offRec++];
                    if (idxSeg & 0x80)
                        idxSeg = ((idxSeg & 0x7f) << 8) | pbRec[offRec++];

                    uint16_t uFrameBase = 0;
                    if (idxSeg == 0)
                    {
                        OMF_CHECK_RET(2, [L]PUBDEF);
                        uFrameBase = RT_MAKE_U16(pbRec[offRec], pbRec[offRec + 1]);
                        offRec += 2;
                    }
                    if (g_cVerbose > 2)
                        printf("  %s: idxGrp=%#x idxSeg=%#x uFrameBase=%#x\n", pszRec, idxGrp, idxSeg, uFrameBase);
                    uint16_t const uSeg = idxSeg ? idxSeg : uFrameBase;

                    while (offRec + 1 < cbRec)
                    {
                        uint8_t cch = pbRec[offRec++];
                        OMF_CHECK_RET(cch, [L]PUBDEF);
                        const char *pchName = (const char *)&pbRec[offRec];
                        offRec += cch;

                        uint32_t offSeg;
                        if (bRecType & OMF_REC32)
                        {
                            OMF_CHECK_RET(4, [L]PUBDEF);
                            offSeg = RT_MAKE_U32_FROM_U8(pbRec[offRec], pbRec[offRec + 1], pbRec[offRec + 2], pbRec[offRec + 3]);
                            offRec += 4;
                        }
                        else
                        {
                            OMF_CHECK_RET(2, [L]PUBDEF);
                            offSeg = RT_MAKE_U16(pbRec[offRec], pbRec[offRec + 1]);
                            offRec += 2;
                        }

                        OMF_CHECK_RET(2, [L]PUBDEF);
                        uint16_t idxType = pbRec[offRec++];
                        if (idxType & 0x80)
                            idxType = ((idxType & 0x7f) << 8) | pbRec[offRec++];

                        if (g_cVerbose > 2)
                            printf("  %s[%u]: off=%#010x type=%#x %-*.*s\n", pszRec, cPubDefs, offSeg, idxType, cch, cch, pchName);
                        else if (g_cVerbose > 0)
                            printf("%04x:%08x %c %-*.*s\n", uSeg, offSeg, chType, cch, cch, pchName);

                        cPubDefs++;
                    }
                }
                break;
            }
        }

        /* advance */
        off += cbRec + 3;
    }
    return true;
}


/**
 * Does the convertion using convertelf and convertcoff.
 *
 * @returns exit code (0 on success, non-zero on failure)
 * @param   pszFile     The file to convert.
 */
static int convertit(const char *pszFile)
{
    /* Construct the filename for saving the unmodified file. */
    char szOrgFile[_4K];
    size_t cchFile = strlen(pszFile);
    if (cchFile + sizeof(".original") > sizeof(szOrgFile))
    {
        error(pszFile, "Filename too long!\n");
        return RTEXITCODE_FAILURE;
    }
    memcpy(szOrgFile, pszFile, cchFile);
    memcpy(&szOrgFile[cchFile], ".original", sizeof(".original"));

    /* Read the whole file. */
    void  *pvFile;
    size_t cbFile;
    if (readfile(pszFile, &pvFile, &cbFile))
    {
        /*
         * Do format conversions / adjustments.
         */
        bool fRc = false;
        uint8_t *pbFile = (uint8_t *)pvFile;
        if (   cbFile > sizeof(Elf64_Ehdr)
            && pbFile[0] == ELFMAG0
            && pbFile[1] == ELFMAG1
            && pbFile[2] == ELFMAG2
            && pbFile[3] == ELFMAG3)
        {
#ifdef ELF_TO_OMF_CONVERSION
            if (writefile(szOrgFile, pvFile, cbFile))
            {
                FILE *pDst = openfile(pszFile, true /*fWrite*/);
                if (pDst)
                {
                    fRc = convertElfToOmf(pszFile, pbFile, cbFile, pDst);
                    fRc = fclose(pDst) == 0 && fRc;
                }
            }
#else
            fRc = writefile(szOrgFile, pvFile, cbFile)
               && convertelf(pszFile, pbFile, cbFile)
               && writefile(pszFile, pvFile, cbFile);
#endif
        }
        else if (   cbFile > sizeof(IMAGE_FILE_HEADER)
                 && RT_MAKE_U16(pbFile[0], pbFile[1]) == IMAGE_FILE_MACHINE_AMD64
                 &&   RT_MAKE_U16(pbFile[2], pbFile[3]) * sizeof(IMAGE_SECTION_HEADER) + sizeof(IMAGE_FILE_HEADER)
                    < cbFile
                 && RT_MAKE_U16(pbFile[2], pbFile[3]) > 0)
        {
            if (writefile(szOrgFile, pvFile, cbFile))
            {
                FILE *pDst = openfile(pszFile, true /*fWrite*/);
                if (pDst)
                {
                    fRc = convertCoffToOmf(pszFile, pbFile, cbFile, pDst);
                    fRc = fclose(pDst) == 0 && fRc;
                }
            }
        }
        else if (   cbFile >= 8
                 && pbFile[0] == OMF_THEADR
                 && RT_MAKE_U16(pbFile[1], pbFile[2]) < cbFile)
        {
            const char **papchLNames = (const char **)calloc(sizeof(*papchLNames), _32K);
            fRc = writefile(szOrgFile, pvFile, cbFile)
               && convertomf(pszFile, pbFile, cbFile, papchLNames, _32K)
               && writefile(pszFile, pvFile, cbFile);
            free(papchLNames);
        }
        else
            fprintf(stderr, "error: Don't recognize format of '%s' (%#x %#x %#x %#x, cbFile=%lu)\n",
                    pszFile, pbFile[0], pbFile[1], pbFile[2], pbFile[3], (unsigned long)cbFile);
        free(pvFile);
        if (fRc)
            return 0;
    }
    return 1;
}


int main(int argc, char **argv)
{
    int rcExit = 0;

    /*
     * Scan the arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            const char *pszOpt = &argv[i][1];
            if (*pszOpt == '-')
            {
                /* Convert long options to short ones. */
                pszOpt--;
                if (!strcmp(pszOpt, "--verbose"))
                    pszOpt = "v";
                else if (!strcmp(pszOpt, "--version"))
                    pszOpt = "V";
                else if (!strcmp(pszOpt, "--help"))
                    pszOpt = "h";
                else
                {
                    fprintf(stderr, "syntax errro: Unknown options '%s'\n", pszOpt);
                    return 2;
                }
            }

            /* Process the list of short options. */
            while (*pszOpt)
            {
                switch (*pszOpt++)
                {
                    case 'v':
                        g_cVerbose++;
                        break;

                    case 'V':
                        printf("%s\n", "$Revision$");
                        return 0;

                    case '?':
                    case 'h':
                        printf("usage: %s [options] -o <output> <input1> [input2 ... [inputN]]\n",
                               argv[0]);
                        return 0;
                }
            }
        }
        else
        {
            /*
             * File to convert.  Do the job right away.
             */
            rcExit = convertit(argv[i]);
            if (rcExit != 0)
                break;
        }
    }

    return rcExit;
}


