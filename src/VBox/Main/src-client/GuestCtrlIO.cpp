/* $Id$ */
/** @file
 *
 * IO helper for IGuest COM class implementations.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/
#include "GuestCtrlImplPrivate.h"


/******************************************************************************
 *   Structures and Typedefs                                                  *
 ******************************************************************************/

/** @todo *NOT* thread safe yet! */

GuestProcessStream::GuestProcessStream()
    : m_cbAllocated(0),
      m_cbSize(0),
      m_cbOffset(0),
      m_cbParserOffset(0),
      m_pbBuffer(NULL)
{

}

GuestProcessStream::~GuestProcessStream()
{
    Destroy();
}

/**
 * Destroys the stored stream pairs.
 */
void GuestProcessStream::Destroy()
{
    ClearPairs();

    if (m_pbBuffer)
        RTMemFree(m_pbBuffer);
}

void GuestProcessStream::ClearPairs()
{
    for (GuestCtrlStreamPairsIter it = m_mapPairs.begin(); it != m_mapPairs.end(); it++)
    {
        if (it->second.pszValue)
            RTMemFree(it->second.pszValue);
    }

    m_mapPairs.clear();
}

/**
 * Returns a 32-bit unsigned integer of a specified key.
 *
 * @return  uint32_t            Value to return, 0 if not found / on failure.
 * @param   pszKey              Name of key to get the value for.
 */
const char* GuestProcessStream::GetString(const char *pszKey)
{
    AssertPtrReturn(pszKey, NULL);

    try
    {
        GuestCtrlStreamPairsIterConst itPairs = m_mapPairs.find(Utf8Str(pszKey));
        if (itPairs != m_mapPairs.end())
            return itPairs->second.pszValue;
    }
    catch (const std::exception &ex)
    {
        NOREF(ex);
    }
    return NULL;
}

int GuestProcessStream::GetUInt32Ex(const char *pszKey, uint32_t *puVal)
{
    AssertPtrReturn(pszKey, VERR_INVALID_POINTER);
    AssertPtrReturn(puVal, VERR_INVALID_POINTER);
    const char *pszValue = GetString(pszKey);
    if (pszValue)
    {
        *puVal = RTStrToUInt32(pszValue);
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}

/**
 * Returns a 32-bit unsigned integer of a specified key.
 *
 * @return  uint32_t            Value to return, 0 if not found / on failure.
 * @param   pszKey              Name of key to get the value for.
 */
uint32_t GuestProcessStream::GetUInt32(const char *pszKey)
{
    uint32_t uVal;
    if (RT_SUCCESS(GetUInt32Ex(pszKey, &uVal)))
        return uVal;
    return 0;
}

int GuestProcessStream::GetInt64Ex(const char *pszKey, int64_t *piVal)
{
    AssertPtrReturn(pszKey, VERR_INVALID_POINTER);
    AssertPtrReturn(piVal, VERR_INVALID_POINTER);
    const char *pszValue = GetString(pszKey);
    if (pszValue)
    {
        *piVal = RTStrToInt64(pszValue);
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}

/**
 * Returns a 64-bit integer of a specified key.
 *
 * @return  int64_t             Value to return, 0 if not found / on failure.
 * @param   pszKey              Name of key to get the value for.
 */
int64_t GuestProcessStream::GetInt64(const char *pszKey)
{
    int64_t iVal;
    if (RT_SUCCESS(GetInt64Ex(pszKey, &iVal)))
        return iVal;
    return 0;
}

/**
 * Returns the current number of stream pairs.
 *
 * @return  uint32_t            Current number of stream pairs.
 */
size_t GuestProcessStream::GetNumPairs()
{
    return m_mapPairs.size();
}

int GuestProcessStream::AddData(const BYTE *pbData, size_t cbData)
{
    AssertPtrReturn(pbData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    /* Rewind the buffer if it's empty. */
    size_t     cbInBuf   = m_cbSize - m_cbOffset;
    bool const fAddToSet = cbInBuf == 0;
    if (fAddToSet)
        m_cbSize = m_cbOffset = 0;

    /* Try and see if we can simply append the data. */
    if (cbData + m_cbSize <= m_cbAllocated)
    {
        memcpy(&m_pbBuffer[m_cbSize], pbData, cbData);
        m_cbSize += cbData;
    }
    else
    {
        /* Move any buffered data to the front. */
        cbInBuf = m_cbSize - m_cbOffset;
        if (cbInBuf == 0)
            m_cbSize = m_cbOffset = 0;
        else if (m_cbOffset) /* Do we have something to move? */
        {
            memmove(m_pbBuffer, &m_pbBuffer[m_cbOffset], cbInBuf);
            m_cbSize = cbInBuf;
            m_cbOffset = 0;
        }

        /* Do we need to grow the buffer? */
        if (cbData + m_cbSize > m_cbAllocated)
        {
            size_t cbAlloc = m_cbSize + cbData;
            cbAlloc = RT_ALIGN_Z(cbAlloc, _64K);
            void *pvNew = RTMemRealloc(m_pbBuffer, cbAlloc);
            if (pvNew)
            {
                m_pbBuffer = (uint8_t *)pvNew;
                m_cbAllocated = cbAlloc;
            }
            else
                rc = VERR_NO_MEMORY;
        }

        /* Finally, copy the data. */
        if (RT_SUCCESS(rc))
        {
            if (cbData + m_cbSize <= m_cbAllocated)
            {
                memcpy(&m_pbBuffer[m_cbSize], pbData, cbData);
                m_cbSize += cbData;
            }
            else
                rc = VERR_BUFFER_OVERFLOW;
        }
    }

    return rc;
}

int GuestProcessStream::Parse()
{
    AssertPtrReturn(m_pbBuffer, VERR_INVALID_POINTER);
    AssertReturn(m_cbSize, VERR_INVALID_PARAMETER);
    AssertReturn(m_cbParserOffset < m_cbSize, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    size_t uCur = m_cbParserOffset;
    for (;uCur < m_cbSize;)
    {
        const char *pszStart = (char*)&m_pbBuffer[uCur];
        const char *pszEnd = pszStart;

        /* Search end of current pair (key=value\0). */
        while (uCur++ < m_cbSize)
        {
            if (*pszEnd == '\0')
                break;
            pszEnd++;
        }

        size_t uPairLen = pszEnd - pszStart;
        if (uPairLen)
        {
            const char *pszSep = pszStart;
            while (   *pszSep != '='
                   &&  pszSep != pszEnd)
            {
                pszSep++;
            }

            /* No separator found (or incomplete key=value pair)? */
            if (   pszSep == pszStart
                || pszSep == pszEnd)
            {
                m_cbParserOffset =  uCur - uPairLen - 1;
                rc = VERR_MORE_DATA;
            }

            if (RT_FAILURE(rc))
                break;

            size_t uKeyLen = pszSep - pszStart;
            size_t uValLen = pszEnd - (pszSep + 1);

            /* Get key (if present). */
            if (uKeyLen)
            {
                Assert(pszSep > pszStart);
                char *pszKey = (char*)RTMemAllocZ(uKeyLen + 1);
                if (!pszKey)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                memcpy(pszKey, pszStart, uKeyLen);

                m_mapPairs[Utf8Str(pszKey)].pszValue = NULL;

                /* Get value (if present). */
                if (uValLen)
                {
                    Assert(pszEnd > pszSep);
                    char *pszVal = (char*)RTMemAllocZ(uValLen + 1);
                    if (!pszVal)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                    memcpy(pszVal, pszSep + 1, uValLen);

                    m_mapPairs[Utf8Str(pszKey)].pszValue = pszVal;
                }

                RTMemFree(pszKey);

                m_cbParserOffset += uCur - m_cbParserOffset;
            }
        }
        else /* No pair detected, check for a new block. */
        {
            do
            {
                if (*pszEnd == '\0')
                {
                    m_cbParserOffset = uCur;
                    rc = VERR_MORE_DATA;
                    break;
                }
                pszEnd++;
            } while (++uCur < m_cbSize);
        }

        if (RT_FAILURE(rc))
            break;
    }

    RT_CLAMP(m_cbParserOffset, 0, m_cbSize);

    return rc;
}

