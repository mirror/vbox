/* $Id$ */
/** @file
 * Shared Clipboard - Some helper function for converting between the various EOLs.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VBOX_INCLUDED_GuestHost_clipboard_helper_h
#define VBOX_INCLUDED_GuestHost_clipboard_helper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/string.h>

#include <VBox/GuestHost/SharedClipboard.h>

/** Constants needed for string conversions done by the Linux/Mac clipboard code. */
enum
{
    /** In Linux, lines end with a linefeed character. */
    VBOX_SHCL_LINEFEED = 0xa,
    /** In Windows, lines end with a carriage return and a linefeed character. */
    VBOX_SHCL_CARRIAGERETURN = 0xd,
    /** Little endian "real" UTF-16 strings start with this marker. */
    VBOX_SHCL_UTF16LEMARKER = 0xfeff,
    /** Big endian "real" UTF-16 strings start with this marker. */
    VBOX_SHCL_UTF16BEMARKER = 0xfffe
};

/**
 * Get the size of the buffer needed to hold a UTF-16-LE zero terminated string
 * with Windows EOLs converted from a UTF-16 string with Linux EOLs.
 *
 * @returns VBox status code.
 * @param   pcwszSrc The source UTF-16 string.
 * @param   cwcSrc   The length of the source string in RTUTF16 units.
 * @param   pcwcDst  The length of the destination string in RTUTF16 units.
 */
int ShClUtf16GetWinSize(PCRTUTF16 pcwszSrc, size_t cwcSrc, size_t *pcwcDst);

/**
 * Convert a UTF-16 text with Linux EOLs to null-terminated UTF-16-LE with
 * Windows EOLs.
 *
 * Does no checking for validity.
 *
 * @returns VBox status code.
 * @param   pcwszSrc Source UTF-16 text to convert.
 * @param   cwcSrc   Size of the source text int RTUTF16 units.
 * @param   pwszDst  Buffer to store the converted text to.
 * @param   cwcDst   Size of the buffer for the converted text in RTUTF16 units.
 */
int ShClUtf16LinToWin(PCRTUTF16 pcwszSrc, size_t cwcSrc, PRTUTF16 pwszDst, size_t cwcDst);

/**
 * Get the size of the buffer needed to hold a zero-terminated UTF-16 string
 * with Linux EOLs converted from a UTF-16 string with Windows EOLs.
 *
 * @returns VBox status code.
 * @param   pcwszSrc The source UTF-16 string.
 * @param   cwcSrc   The length of the source string in RTUTF16 units.
 * @param   pcwcDst  The length of the destination string in RTUTF16 units.
 */
int ShClUtf16GetLinSize(PCRTUTF16 pcwszSrc, size_t cwcSrc, size_t *pcwcDst);

/**
 * Convert UTF-16-LE text with Windows EOLs to zero-terminated UTF-16 with Linux
 * EOLs.  This function does not verify that the UTF-16 is valid.
 *
 * @returns VBox status code.
 * @param   pcwszSrc Text to convert.
 * @param   cwcSrc   Size of the source text in RTUTF16 units.
 * @param   pwszDst  The buffer to store the converted text to.
 * @param   cwcDst   The size of the buffer for the destination text in RTUTF16 chars.
 */
int ShClUtf16WinToLin(PCRTUTF16 pcwszSrc, size_t cwcSrc, PRTUTF16 pwszDst, size_t cwcDst);

#pragma pack(1)
/** @todo r=bird: Why duplicate these structures here, we've got them in
 *        DevVGA.cpp already! */
/**
 * Bitmap File Header. Official win32 name is BITMAPFILEHEADER
 * Always Little Endian.
 */
typedef struct BMFILEHEADER
{
    uint16_t uType;
    uint32_t uSize;
    uint16_t uReserved1;
    uint16_t uReserved2;
    uint32_t uOffBits;
} BMFILEHEADER;
#pragma pack()

/** Pointer to a BMFILEHEADER structure. */
typedef BMFILEHEADER *PBMFILEHEADER;
/** BMP file magic number */
#define BITMAPHEADERMAGIC (RT_H2LE_U16_C(0x4d42))

/**
 * Bitmap Info Header. Official win32 name is BITMAPINFOHEADER
 * Always Little Endian.
 */
typedef struct BMINFOHEADER
{
    uint32_t uSize;
    uint32_t uWidth;
    uint32_t uHeight;
    uint16_t uPlanes;
    uint16_t uBitCount;
    uint32_t uCompression;
    uint32_t uSizeImage;
    uint32_t uXBitsPerMeter;
    uint32_t uYBitsPerMeter;
    uint32_t uClrUsed;
    uint32_t uClrImportant;
} BMINFOHEADER;
/** Pointer to a BMINFOHEADER structure. */
typedef BMINFOHEADER *PBMINFOHEADER;

/**
 * Convert CF_DIB data to full BMP data by prepending the BM header.
 * Allocates with RTMemAlloc.
 *
 * @returns VBox status code.
 * @param   pvSrc         DIB data to convert
 * @param   cbSrc         Size of the DIB data to convert in bytes
 * @param   ppvDst        Where to store the pointer to the buffer for the
 *                        destination data
 * @param   pcbDst        Pointer to the size of the buffer for the destination
 *                        data in bytes.
 */
int ShClDibToBmp(const void *pvSrc, size_t cbSrc, void **ppvDst, size_t *pcbDst);

/**
 * Get the address and size of CF_DIB data in a full BMP data in the input buffer.
 * Does not do any allocation.
 *
 * @returns VBox status code.
 * @param   pvSrc         BMP data to convert
 * @param   cbSrc         Size of the BMP data to convert in bytes
 * @param   ppvDst        Where to store the pointer to the destination data
 * @param   pcbDst        Pointer to the size of the destination data in bytes
 */
int ShClBmpGetDib(const void *pvSrc, size_t cbSrc, const void **ppvDst, size_t *pcbDst);

#ifdef LOG_ENABLED
/**
 * Dumps HTML data to the debug log.
 *
 * @returns VBox status code.
 * @param   pszSrc              HTML data to dump.
 * @param   cbSrc               Size (in bytes) of HTML data to dump.
 */
int ShClDbgDumpHtml(const char *pszSrc, size_t cbSrc);

/**
 * Dumps data using a specified clipboard format.
 *
 * @param   pv                  Pointer to data to dump.
 * @param   cb                  Size (in bytes) of data to dump.
 * @param   u32Format           Clipboard format to use for dumping.
 */
void ShClDbgDumpData(const void *pv, size_t cb, SHCLFORMAT u32Format);
#endif /* LOG_ENABLED */

/**
 * Translates a Shared Clipboard host function number to a string.
 *
 * @returns Function ID string name.
 * @param   uFn                 The function to translate.
 */
const char *ShClHostFunctionToStr(uint32_t uFn);

/**
 * Translates a Shared Clipboard host message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *ShClHostMsgToStr(uint32_t uMsg);

/**
 * Translates a Shared Clipboard guest message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *ShClGuestMsgToStr(uint32_t uMsg);

#endif /* !VBOX_INCLUDED_GuestHost_clipboard_helper_h */

