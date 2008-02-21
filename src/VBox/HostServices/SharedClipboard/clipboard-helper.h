/** @file
 *
 * Shared Clipboard:
 * Some helper function for converting between the various eol.
 */

/*
 * Copyright (C) 2006-2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __CLIPBOARD_HELPER_H
#define __CLIPBOARD_HELPER_H

#include <iprt/string.h>

/** Constants needed for string conversions done by the Linux/Mac clipboard code. */
enum {
    /** In Linux, lines end with a linefeed character. */
    LINEFEED = 0xa,
    /** In Windows, lines end with a carriage return and a linefeed character. */
    CARRIAGERETURN = 0xd,
    /** Little endian "real" Utf16 strings start with this marker. */
    UTF16LEMARKER = 0xfeff,
    /** Big endian "real" Utf16 strings start with this marker. */
    UTF16BEMARKER = 0xfffe
};

/**
 * Get the size of the buffer needed to hold a Utf16-LE zero terminated string with Windows EOLs
 * converted from a Utf16 string with Linux EOLs.
 *
 * @returns RT error code
 *
 * @param   pu16Src  The source Utf16 string
 * @param   cwSrc    The length in 16 bit words of the source string
 * @retval  pcwDest  The length of the destination string in 16 bit words
 */
int vboxClipboardUtf16GetWinSize(PRTUTF16 pu16Src, size_t cwSrc, size_t *pcwDest);

/**
 * Convert a Utf16 text with Linux EOLs to null-terminated Utf16-LE with Windows EOLs.  Does no
 * checking for validity.
 *
 * @returns VBox status code
 *
 * @param   pu16Src  Source Utf16 text to convert
 * @param   cwSrc    Size of the source text in 16 bit words
 * @retval  pu16Dest Buffer to store the converted text to.
 * @retval  pcwDest  Size of the buffer for the converted text in 16 bit words
 */
int vboxClipboardUtf16LinToWin(PRTUTF16 pu16Src, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest);

/**
 * Get the size of the buffer needed to hold a zero-terminated Utf16 string with Linux EOLs
 * converted from a Utf16 string with Windows EOLs.
 *
 * @returns RT status code
 *
 * @param   pu16Src  The source Utf16 string
 * @param   cwSrc    The length in 16 bit words of the source string
 * @retval  pcwDest  The length of the destination string in 16 bit words
 */
int vboxClipboardUtf16GetLinSize(PRTUTF16 pu16Src, size_t cwSrc, size_t *pcwDest);

/**
 * Convert Utf16-LE text with Windows EOLs to zero-terminated Utf16 with Linux EOLs.  This
 * function does not verify that the Utf16 is valid.
 *
 * @returns VBox status code
 *
 * @param   pu16Src       Text to convert
 * @param   cwSrc         Size of the source text in 16 bit words
 * @param   pu16Dest      The buffer to store the converted text to
 * @param   cwDest        The size of the buffer for the destination text in 16 bit words
 */
int vboxClipboardUtf16WinToLin(PRTUTF16 pu16Src, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest);

#endif /* __CLIPBOARD_HELPER_H */

