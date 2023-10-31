/** @file
 * Common code for mime-type data conversion.
 *
 * This code supposed to be shared between Shared Clipboard and
 * Drag-And-Drop services. The main purpose is to convert data into and
 * from VirtualBox internal representation and host/guest specific format.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <iprt/string.h>
#include <iprt/utf16.h>
#include <iprt/mem.h>
#include <iprt/log.h>

#include <VBox/GuestHost/mime-type-converter.h>

#define VBOX_WAYLAND_MIME_TYPE_NAME_MAX     (32)

/* Declaration of mime-type conversion helper function. */
typedef DECLCALLBACKTYPE(int, FNVBFMTCONVERTOR, (void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut));
typedef FNVBFMTCONVERTOR *PFNVBFMTCONVERTOR;

/**
 * A helper function that converts UTF-8 string into UTF-16.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains UTF-8 data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain UTF-16 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertUtf8ToUtf16(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;
    size_t cwDst;
    void  *pvDst = NULL;

    rc = RTStrValidateEncodingEx((char *)pvBufIn, cbBufIn, 0);
    if (RT_SUCCESS(rc))
    {
        rc = ShClConvUtf8LFToUtf16CRLF((const char *)pvBufIn, cbBufIn, (PRTUTF16 *)&pvDst, &cwDst);
        if (RT_SUCCESS(rc))
        {
            *ppvBufOut = pvDst;
            *pcbBufOut = cwDst * sizeof(RTUTF16);
        }
        else
            LogRel(("Data Converter: unable to convert input UTF8 string into VBox format, rc=%Rrc\n", rc));
    }
    else
        LogRel(("Data Converter: unable to validate input UTF8 string, rc=%Rrc\n", rc));

    return rc;
}

/**
 * A helper function that converts UTF-16 string into UTF-8.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains UTF-16 data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain UTF-8 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertUtf16ToUtf8(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;

    rc = RTUtf16ValidateEncodingEx((PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16),
                                   RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED | RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
    if (RT_SUCCESS(rc))
    {
        size_t chDst = 0;
        rc = ShClUtf16LenUtf8((PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16), &chDst);
        if (RT_SUCCESS(rc))
        {
            /* Add space for '\0'. */
            chDst++;

            char *pszDst = (char *)RTMemAllocZ(chDst);
            if (pszDst)
            {
                size_t cbActual = 0;
                rc = ShClConvUtf16CRLFToUtf8LF((PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16), pszDst, chDst, &cbActual);
                if (RT_SUCCESS(rc))
                {
                    *pcbBufOut = cbActual;
                    *ppvBufOut = pszDst;
                }
            }
            else
            {
                LogRel(("Data Converter: unable to allocate memory for UTF16 string conversion, rc=%Rrc\n", rc));
                rc = VERR_NO_MEMORY;
            }
        }
    }

    return rc;
}

/**
 * A helper function that converts Latin-1 string into UTF-16.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains Latin-1 data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain UTF-16 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertLatin1ToUtf16(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;
    size_t cwDst;
    void *pvDst = NULL;

    rc = ShClConvLatin1LFToUtf16CRLF((char *)pvBufIn, cbBufIn, (PRTUTF16 *)&pvDst, &cwDst);
    if (RT_SUCCESS(rc))
    {
        *ppvBufOut = pvDst;
        *pcbBufOut = cwDst * sizeof(RTUTF16);
    }
    else
        LogRel(("Data Converter: unable to convert input Latin1 string into VBox format, rc=%Rrc\n", rc));

    return rc;
}

/**
 * A helper function that converts UTF-16 string into Latin-1.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains UTF-16 data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain Latin-1 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertUtf16ToLatin1(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;

    /* Make RTUtf16ToLatin1ExTag() to allocate output buffer. */
    *ppvBufOut = NULL;

    rc = RTUtf16ValidateEncodingEx((PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16),
                                   RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED | RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
    if (RT_SUCCESS(rc))
        rc = RTUtf16ToLatin1ExTag(
            (PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16), (char **)ppvBufOut, cbBufIn, pcbBufOut, RTSTR_TAG);

    return rc;
}

/**
 * A helper function that converts HTML data into internal VBox representation (UTF-8).
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains HTML data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain UTF-8 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertHtmlToVBox(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc = VERR_PARSE_ERROR;
    void *pvDst = NULL;
    size_t cbDst = 0;

    /* From X11 counterpart: */

    /*
     * The common VBox HTML encoding will be - UTF-8
     * because it more general for HTML formats then UTF-16
     * X11 clipboard returns UTF-16, so before sending it we should
     * convert it to UTF-8.
     *
     * Some applications sends data in UTF-16, some in UTF-8,
     * without indication it in MIME.
     *
     * In case of UTF-16, at least [Open|Libre] Office adds an byte order mark (0xfeff)
     * at the start of the clipboard data.
     */

    if (   cbBufIn >= (int)sizeof(RTUTF16)
        && *(PRTUTF16)pvBufIn == VBOX_SHCL_UTF16LEMARKER)
    {
        /* Input buffer is expected to be UTF-16 encoded. */
        LogRel(("Data Converter: unable to convert UTF-16 encoded HTML data into VBox format\n"));

        rc = RTUtf16ValidateEncodingEx((PCRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16),
                                       RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED | RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
        if (RT_SUCCESS(rc))
        {
            rc = ShClConvUtf16ToUtf8HTML((PRTUTF16)pvBufIn, cbBufIn / sizeof(RTUTF16), (char**)&pvDst, &cbDst);
            if (RT_SUCCESS(rc))
            {
                *ppvBufOut = pvDst;
                *pcbBufOut = cbDst;
            }
            else
                LogRel(("Data Converter: unable to convert input UTF16 string into VBox format, rc=%Rrc\n", rc));
        }
        else
            LogRel(("Data Converter: unable to validate input UTF8 string, rc=%Rrc\n", rc));
    }
    else
    {
        LogRel(("Data Converter: converting UTF-8 encoded HTML data into VBox format\n"));

        /* Input buffer is expected to be UTF-8 encoded. */
        rc = RTStrValidateEncodingEx((char *)pvBufIn, cbBufIn, 0);
        if (RT_SUCCESS(rc))
        {
            pvDst = RTMemAllocZ(cbBufIn + 1 /* '\0' */);
            if (pvDst)
            {
                memcpy(pvDst, pvBufIn, cbBufIn);
                *ppvBufOut = pvDst;
                *pcbBufOut = cbBufIn + 1 /* '\0' */;
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

/**
 * A helper function that validates HTML data in UTF-8 format and passes out a duplicated buffer.
 *
 * This function supposed to convert HTML data from internal VBox representation (UTF-8)
 * into what will be accepted by X11/Wayland clients. However, since we paste HTML content
 * in UTF-8 representation, there is nothing to do with conversion. We only validate buffer here.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains HTML data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain UTF-8 data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertVBoxToHtml(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;

    rc = RTStrValidateEncodingEx((char *)pvBufIn, cbBufIn, 0);
    if (RT_SUCCESS(rc))
    {
        void *pvBuf = RTMemAllocZ(cbBufIn);
        if (pvBuf)
        {
            memcpy(pvBuf, pvBufIn, cbBufIn);
            *ppvBufOut = pvBuf;
            *pcbBufOut = cbBufIn;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

/**
 * A helper function that converts BMP image data into internal VBox representation.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains BMP image data.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain image data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertBmpToVBox(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    int rc;
    const void *pvBufOutTmp = NULL;
    size_t cbBufOutTmp = 0;

    rc = ShClBmpGetDib(pvBufIn, cbBufIn, &pvBufOutTmp, &cbBufOutTmp);
    if (RT_SUCCESS(rc))
    {
        void *pvBuf = RTMemAllocZ(cbBufOutTmp);
        if (pvBuf)
        {
            memcpy(pvBuf, pvBufOutTmp, cbBufOutTmp);
            *ppvBufOut = pvBuf;
            *pcbBufOut = cbBufOutTmp;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        LogRel(("Data Converter: unable to convert image data (%u bytes) into BMP format, rc=%Rrc\n", cbBufIn, rc));

    return rc;
}

/**
 * A helper function that converts image data from internal VBox representation into BMP.
 *
 * @returns IPRT status code.
 * @param   pvBufIn         Input buffer which contains image data in VBox format.
 * @param   cbBufIn         Size of input buffer in bytes.
 * @param   ppvBufOut       Newly allocated output buffer which will contain BMP image data (must be freed by caller).
 * @param   pcbBufOut       Size of output buffer.
 */
static DECLCALLBACK(int) vbConvertVBoxToBmp(void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    return ShClDibToBmp(pvBufIn, cbBufIn, ppvBufOut, pcbBufOut);
}

/* This table represents mime-types cache and contains its
 * content converted into VirtualBox internal representation. */
static struct VBCONVERTERFMTTABLE
{
    /** Content mime-type as reported by X11/Wayland. */
    const char *                    pcszMimeType;
    /** VBox content type representation. */
    const SHCLFORMAT                uFmtVBox;
    /** Pointer to a function which converts data into internal
     *  VirtualBox representation. */
    const PFNVBFMTCONVERTOR         pfnConvertToVbox;
    /** Pointer to a function which converts data from internal
     *  VirtualBox representation into X11/Wayland format. */
    const PFNVBFMTCONVERTOR         pfnConvertToNative;
    /** A buffer which contains mime-type data cache in VirtualBox
     *  internal representation. */
    void *                          pvBuf;
    /** Size of cached data. */
    size_t                          cbBuf;
} g_aConverterFormats[] =
{
    { "INVALID",                        VBOX_SHCL_FMT_NONE,         NULL,                       NULL,                 NULL, 0, },

    { "UTF8_STRING",                    VBOX_SHCL_FMT_UNICODETEXT,  vbConvertUtf8ToUtf16,     vbConvertUtf16ToUtf8,   NULL, 0, },
    { "text/plain;charset=UTF-8",       VBOX_SHCL_FMT_UNICODETEXT,  vbConvertUtf8ToUtf16,     vbConvertUtf16ToUtf8,   NULL, 0, },
    { "text/plain;charset=utf-8",       VBOX_SHCL_FMT_UNICODETEXT,  vbConvertUtf8ToUtf16,     vbConvertUtf16ToUtf8,   NULL, 0, },
    { "STRING",                         VBOX_SHCL_FMT_UNICODETEXT,  vbConvertLatin1ToUtf16,   vbConvertUtf16ToLatin1, NULL, 0, },
    { "TEXT",                           VBOX_SHCL_FMT_UNICODETEXT,  vbConvertLatin1ToUtf16,   vbConvertUtf16ToLatin1, NULL, 0, },
    { "text/plain",                     VBOX_SHCL_FMT_UNICODETEXT,  vbConvertLatin1ToUtf16,   vbConvertUtf16ToLatin1, NULL, 0, },

    { "text/html",                      VBOX_SHCL_FMT_HTML,         vbConvertHtmlToVBox,      vbConvertVBoxToHtml,    NULL, 0, },
    { "text/html;charset=utf-8",        VBOX_SHCL_FMT_HTML,         vbConvertHtmlToVBox,      vbConvertVBoxToHtml,    NULL, 0, },
    { "application/x-moz-nativehtml",   VBOX_SHCL_FMT_HTML,         vbConvertHtmlToVBox,      vbConvertVBoxToHtml,    NULL, 0, },

    { "image/bmp",                      VBOX_SHCL_FMT_BITMAP,       vbConvertBmpToVBox,       vbConvertVBoxToBmp,     NULL, 0, },
    { "image/x-bmp",                    VBOX_SHCL_FMT_BITMAP,       vbConvertBmpToVBox,       vbConvertVBoxToBmp,     NULL, 0, },
    { "image/x-MS-bmp",                 VBOX_SHCL_FMT_BITMAP,       vbConvertBmpToVBox,       vbConvertVBoxToBmp,     NULL, 0, },
};


RTDECL(void) VBoxMimeConvEnumerateMimeById(const SHCLFORMAT uFmtVBox, PFNVBFMTCONVMIMEBYID pfnCb, void *pvData)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aConverterFormats); i++)
        if (uFmtVBox & g_aConverterFormats[i].uFmtVBox)
            pfnCb(g_aConverterFormats[i].pcszMimeType, pvData);
}

RTDECL(const char *) VBoxMimeConvGetMimeById(const SHCLFORMAT uFmtVBox)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aConverterFormats); i++)
        if (uFmtVBox & g_aConverterFormats[i].uFmtVBox)
            return g_aConverterFormats[i].pcszMimeType;

    return NULL;
}

RTDECL(SHCLFORMAT) VBoxMimeConvGetIdByMime(const char *pcszMimeType)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aConverterFormats); i++)
        if (RTStrNCmp(g_aConverterFormats[i].pcszMimeType, pcszMimeType, VBOX_WAYLAND_MIME_TYPE_NAME_MAX) == 0)
            return g_aConverterFormats[i].uFmtVBox;

    return VBOX_SHCL_FMT_NONE;
}

RTDECL(int) VBoxMimeConvVBoxToNative(const char *pcszMimeType, void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aConverterFormats); i++)
        if (RTStrNCmp(g_aConverterFormats[i].pcszMimeType, pcszMimeType, VBOX_WAYLAND_MIME_TYPE_NAME_MAX) == 0)
            return g_aConverterFormats[i].pfnConvertToNative(pvBufIn, cbBufIn, ppvBufOut, pcbBufOut);

    return VERR_NOT_FOUND;
}

RTDECL(int) VBoxMimeConvNativeToVBox(const char *pcszMimeType, void *pvBufIn, int cbBufIn, void **ppvBufOut, size_t *pcbBufOut)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aConverterFormats); i++)
        if (RTStrNCmp(g_aConverterFormats[i].pcszMimeType, pcszMimeType, VBOX_WAYLAND_MIME_TYPE_NAME_MAX) == 0)
            return g_aConverterFormats[i].pfnConvertToVbox(pvBufIn, cbBufIn, ppvBufOut, pcbBufOut);

    return VERR_NOT_FOUND;
}
