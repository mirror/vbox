/* $Id$ */
/** @file
 *
 * Internal helpers/structures for guest control functionality.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
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
#include "GuestSessionImpl.h"
#include "VMMDev.h"

#include <iprt/asm.h>
#include <iprt/cpp/utils.h> /* For unconst(). */
#include <iprt/ctype.h>
#ifdef DEBUG
# include <iprt/file.h>
#endif /* DEBUG */

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>

/******************************************************************************
 *   Structures and Typedefs                                                  *
 ******************************************************************************/

int GuestEnvironment::BuildEnvironmentBlock(void **ppvEnv, size_t *pcbEnv, uint32_t *pcEnvVars)
{
    AssertPtrReturn(ppvEnv, VERR_INVALID_POINTER);
    /* Rest is optional. */

    size_t cbEnv = 0;
    uint32_t cEnvVars = 0;

    int rc = VINF_SUCCESS;

    size_t cEnv = mEnvironment.size();
    if (cEnv)
    {
        std::map<Utf8Str, Utf8Str>::const_iterator itEnv = mEnvironment.begin();
        for (; itEnv != mEnvironment.end() && RT_SUCCESS(rc); itEnv++)
        {
            char *pszEnv;
            if (!RTStrAPrintf(&pszEnv, "%s=%s", itEnv->first.c_str(), itEnv->second.c_str()))
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            AssertPtr(pszEnv);
            rc = appendToEnvBlock(pszEnv, ppvEnv, &cbEnv, &cEnvVars);
            RTStrFree(pszEnv);
        }
        Assert(cEnv == cEnvVars);
    }

    if (pcbEnv)
        *pcbEnv = cbEnv;
    if (pcEnvVars)
        *pcEnvVars = cEnvVars;

    return rc;
}

void GuestEnvironment::Clear(void)
{
    mEnvironment.clear();
}

int GuestEnvironment::CopyFrom(const GuestEnvironmentArray &environment)
{
    int rc = VINF_SUCCESS;

    for (GuestEnvironmentArray::const_iterator it = environment.begin();
         it != environment.end() && RT_SUCCESS(rc);
         ++it)
    {
        rc = Set((*it));
    }

    return rc;
}

int GuestEnvironment::CopyTo(GuestEnvironmentArray &environment)
{
    size_t s = 0;
    for (std::map<Utf8Str, Utf8Str>::const_iterator it = mEnvironment.begin();
         it != mEnvironment.end();
         ++it, ++s)
    {
        environment[s] = Bstr(it->first + "=" + it->second).raw();
    }

    return VINF_SUCCESS;
}

/* static */
void GuestEnvironment::FreeEnvironmentBlock(void *pvEnv)
{
    if (pvEnv)
        RTMemFree(pvEnv);
}

Utf8Str GuestEnvironment::Get(size_t nPos)
{
    size_t curPos = 0;
    std::map<Utf8Str, Utf8Str>::const_iterator it = mEnvironment.begin();
    for (; it != mEnvironment.end() && curPos < nPos;
         ++it, ++curPos) { }

    if (it != mEnvironment.end())
        return Utf8Str(it->first + "=" + it->second);

    return Utf8Str("");
}

Utf8Str GuestEnvironment::Get(const Utf8Str &strKey)
{
    std::map <Utf8Str, Utf8Str>::const_iterator itEnv = mEnvironment.find(strKey);
    Utf8Str strRet;
    if (itEnv != mEnvironment.end())
        strRet = itEnv->second;
    return strRet;
}

bool GuestEnvironment::Has(const Utf8Str &strKey)
{
    std::map <Utf8Str, Utf8Str>::const_iterator itEnv = mEnvironment.find(strKey);
    return (itEnv != mEnvironment.end());
}

int GuestEnvironment::Set(const Utf8Str &strKey, const Utf8Str &strValue)
{
    /** @todo Do some validation using regex. */
    if (strKey.isEmpty())
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;
    const char *pszString = strKey.c_str();
    while (*pszString != '\0' && RT_SUCCESS(rc))
    {
         if (   !RT_C_IS_ALNUM(*pszString)
             && !RT_C_IS_GRAPH(*pszString))
             rc = VERR_INVALID_PARAMETER;
         *pszString++;
    }

    if (RT_SUCCESS(rc))
        mEnvironment[strKey] = strValue;

    return rc;
}

int GuestEnvironment::Set(const Utf8Str &strPair)
{
    RTCList<RTCString> listPair = strPair.split("=", RTCString::KeepEmptyParts);
    /* Skip completely empty pairs. Note that we still need pairs with a valid
     * (set) key and an empty value. */
    if (listPair.size() <= 1)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;
    size_t p = 0;
    while (p < listPair.size() && RT_SUCCESS(rc))
    {
        Utf8Str strKey = listPair.at(p++);
        if (   strKey.isEmpty()
            || strKey.equals("=")) /* Skip pairs with empty keys (e.g. "=FOO"). */
        {
            break;
        }
        Utf8Str strValue;
        if (p < listPair.size()) /* Does the list also contain a value? */
            strValue = listPair.at(p++);

#ifdef DEBUG
        LogFlowFunc(("strKey=%s, strValue=%s\n",
                     strKey.c_str(), strValue.c_str()));
#endif
        rc = Set(strKey, strValue);
    }

    return rc;
}

size_t GuestEnvironment::Size(void)
{
    return mEnvironment.size();
}

int GuestEnvironment::Unset(const Utf8Str &strKey)
{
    std::map <Utf8Str, Utf8Str>::iterator itEnv = mEnvironment.find(strKey);
    if (itEnv != mEnvironment.end())
    {
        mEnvironment.erase(itEnv);
        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

GuestEnvironment& GuestEnvironment::operator=(const GuestEnvironmentArray &that)
{
    CopyFrom(that);
    return *this;
}

GuestEnvironment& GuestEnvironment::operator=(const GuestEnvironment &that)
{
    for (std::map<Utf8Str, Utf8Str>::const_iterator it = that.mEnvironment.begin();
         it != that.mEnvironment.end();
         ++it)
    {
        mEnvironment[it->first] = it->second;
    }

    return *this;
}

/**
 * Appends environment variables to the environment block.
 *
 * Each var=value pair is separated by the null character ('\\0').  The whole
 * block will be stored in one blob and disassembled on the guest side later to
 * fit into the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @param   pszEnvVar       The environment variable=value to append to the
 *                          environment block.
 * @param   ppvList         This is actually a pointer to a char pointer
 *                          variable which keeps track of the environment block
 *                          that we're constructing.
 * @param   pcbList         Pointer to the variable holding the current size of
 *                          the environment block.  (List is a misnomer, go
 *                          ahead a be confused.)
 * @param   pcEnvVars       Pointer to the variable holding count of variables
 *                          stored in the environment block.
 */
int GuestEnvironment::appendToEnvBlock(const char *pszEnv, void **ppvList, size_t *pcbList, uint32_t *pcEnvVars)
{
    int rc = VINF_SUCCESS;
    size_t cchEnv = strlen(pszEnv); Assert(cchEnv >= 2);
    if (*ppvList)
    {
        size_t cbNewLen = *pcbList + cchEnv + 1; /* Include zero termination. */
        char *pvTmp = (char *)RTMemRealloc(*ppvList, cbNewLen);
        if (pvTmp == NULL)
            rc = VERR_NO_MEMORY;
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cchEnv);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void **)pvTmp;
        }
    }
    else
    {
        char *pszTmp;
        if (RTStrAPrintf(&pszTmp, "%s", pszEnv) >= 0)
        {
            *ppvList = (void **)pszTmp;
            /* Reset counters. */
            *pcEnvVars = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += cchEnv + 1; /* Include zero termination. */
        *pcEnvVars += 1;        /* Increase env variable count. */
    }
    return rc;
}

int GuestFsObjData::FromLs(const GuestProcessStreamBlock &strmBlk)
{
    LogFlowFunc(("\n"));

    int rc = VINF_SUCCESS;

    try
    {
#ifdef DEBUG
        strmBlk.DumpToLog();
#endif
        /* Object name. */
        mName = strmBlk.GetString("name");
        if (mName.isEmpty()) throw VERR_NOT_FOUND;
        /* Type. */
        Utf8Str strType(strmBlk.GetString("ftype"));
        if (strType.equalsIgnoreCase("-"))
            mType = FsObjType_File;
        else if (strType.equalsIgnoreCase("d"))
            mType = FsObjType_Directory;
        /** @todo Add more types! */
        else
            mType = FsObjType_Undefined;
        /* Object size. */
        rc = strmBlk.GetInt64Ex("st_size", &mObjectSize);
        if (RT_FAILURE(rc)) throw rc;
        /** @todo Add complete ls info! */
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestFsObjData::FromStat(const GuestProcessStreamBlock &strmBlk)
{
    LogFlowFunc(("\n"));

    int rc = VINF_SUCCESS;

    try
    {
#ifdef DEBUG
        strmBlk.DumpToLog();
#endif
        /* Node ID, optional because we don't include this
         * in older VBoxService (< 4.2) versions. */
        mNodeID = strmBlk.GetInt64("node_id");
        /* Object name. */
        mName = strmBlk.GetString("name");
        if (mName.isEmpty()) throw VERR_NOT_FOUND;
        /* Type. */
        Utf8Str strType(strmBlk.GetString("ftype"));
        if (strType.equalsIgnoreCase("-"))
            mType = FsObjType_File;
        else if (strType.equalsIgnoreCase("d"))
            mType = FsObjType_Directory;
        /** @todo Add more types! */
        else
            mType = FsObjType_Undefined;
        /* Object size. */
        rc = strmBlk.GetInt64Ex("st_size", &mObjectSize);
        if (RT_FAILURE(rc)) throw rc;
        /** @todo Add complete stat info! */
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

///////////////////////////////////////////////////////////////////////////////

/** @todo *NOT* thread safe yet! */
/** @todo Add exception handling for STL stuff! */

GuestProcessStreamBlock::GuestProcessStreamBlock(void)
{

}

/*
GuestProcessStreamBlock::GuestProcessStreamBlock(const GuestProcessStreamBlock &otherBlock)
{
    for (GuestCtrlStreamPairsIter it = otherBlock.mPairs.begin();
         it != otherBlock.end(); it++)
    {
        mPairs[it->first] = new
        if (it->second.pszValue)
        {
            RTMemFree(it->second.pszValue);
            it->second.pszValue = NULL;
        }
    }
}*/

GuestProcessStreamBlock::~GuestProcessStreamBlock()
{
    Clear();
}

/**
 * Destroys the currently stored stream pairs.
 *
 * @return  IPRT status code.
 */
void GuestProcessStreamBlock::Clear(void)
{
    mPairs.clear();
}

#ifdef DEBUG
void GuestProcessStreamBlock::DumpToLog(void) const
{
    LogFlowFunc(("Dumping contents of stream block=0x%p (%ld items):\n",
                 this, mPairs.size()));

    for (GuestCtrlStreamPairMapIterConst it = mPairs.begin();
         it != mPairs.end(); it++)
    {
        LogFlowFunc(("\t%s=%s\n", it->first.c_str(), it->second.mValue.c_str()));
    }
}
#endif

/**
 * Returns a 64-bit signed integer of a specified key.
 *
 * @return  IPRT status code. VERR_NOT_FOUND if key was not found.
 * @param  pszKey               Name of key to get the value for.
 * @param  piVal                Pointer to value to return.
 */
int GuestProcessStreamBlock::GetInt64Ex(const char *pszKey, int64_t *piVal) const
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
int64_t GuestProcessStreamBlock::GetInt64(const char *pszKey) const
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
size_t GuestProcessStreamBlock::GetCount(void) const
{
    return mPairs.size();
}

/**
 * Returns a string value of a specified key.
 *
 * @return  uint32_t            Pointer to string to return, NULL if not found / on failure.
 * @param   pszKey              Name of key to get the value for.
 */
const char* GuestProcessStreamBlock::GetString(const char *pszKey) const
{
    AssertPtrReturn(pszKey, NULL);

    try
    {
        GuestCtrlStreamPairMapIterConst itPairs = mPairs.find(Utf8Str(pszKey));
        if (itPairs != mPairs.end())
            return itPairs->second.mValue.c_str();
    }
    catch (const std::exception &ex)
    {
        NOREF(ex);
    }
    return NULL;
}

/**
 * Returns a 32-bit unsigned integer of a specified key.
 *
 * @return  IPRT status code. VERR_NOT_FOUND if key was not found.
 * @param  pszKey               Name of key to get the value for.
 * @param  puVal                Pointer to value to return.
 */
int GuestProcessStreamBlock::GetUInt32Ex(const char *pszKey, uint32_t *puVal) const
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
uint32_t GuestProcessStreamBlock::GetUInt32(const char *pszKey) const
{
    uint32_t uVal;
    if (RT_SUCCESS(GetUInt32Ex(pszKey, &uVal)))
        return uVal;
    return 0;
}

/**
 * Sets a value to a key or deletes a key by setting a NULL value.
 *
 * @return  IPRT status code.
 * @param   pszKey              Key name to process.
 * @param   pszValue            Value to set. Set NULL for deleting the key.
 */
int GuestProcessStreamBlock::SetValue(const char *pszKey, const char *pszValue)
{
    AssertPtrReturn(pszKey, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    try
    {
        Utf8Str Utf8Key(pszKey);

        /* Take a shortcut and prevent crashes on some funny versions
         * of STL if map is empty initially. */
        if (!mPairs.empty())
        {
            GuestCtrlStreamPairMapIter it = mPairs.find(Utf8Key);
            if (it != mPairs.end())
                 mPairs.erase(it);
        }

        if (pszValue)
        {
            GuestProcessStreamValue val(pszValue);
            mPairs[Utf8Key] = val;
        }
    }
    catch (const std::exception &ex)
    {
        NOREF(ex);
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////

GuestProcessStream::GuestProcessStream(void)
    : m_cbAllocated(0),
      m_cbSize(0),
      m_cbOffset(0),
      m_pbBuffer(NULL)
{

}

GuestProcessStream::~GuestProcessStream(void)
{
    Destroy();
}

/**
 * Adds data to the internal parser buffer. Useful if there
 * are multiple rounds of adding data needed.
 *
 * @return  IPRT status code.
 * @param   pbData              Pointer to data to add.
 * @param   cbData              Size (in bytes) of data to add.
 */
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

/**
 * Destroys the internal data buffer.
 */
void GuestProcessStream::Destroy(void)
{
    if (m_pbBuffer)
    {
        RTMemFree(m_pbBuffer);
        m_pbBuffer = NULL;
    }

    m_cbAllocated = 0;
    m_cbSize = 0;
    m_cbOffset = 0;
}

#ifdef DEBUG
void GuestProcessStream::Dump(const char *pszFile)
{
    LogFlowFunc(("Dumping contents of stream=0x%p (cbAlloc=%u, cbSize=%u, cbOff=%u) to %s\n",
                 m_pbBuffer, m_cbAllocated, m_cbSize, m_cbOffset, pszFile));

    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFile, RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(hFile, m_pbBuffer, m_cbSize, NULL /* pcbWritten */);
        RTFileClose(hFile);
    }
}
#endif

/**
 * Returns the current offset of the parser within
 * the internal data buffer.
 *
 * @return  uint32_t            Parser offset.
 */
uint32_t GuestProcessStream::GetOffset()
{
    return m_cbOffset;
}

uint32_t GuestProcessStream::GetSize()
{
    return m_cbSize;
}

/**
 * Tries to parse the next upcoming pair block within the internal
 * buffer.
 *
 * Returns VERR_NO_DATA is no data is in internal buffer or buffer has been
 * completely parsed already.
 *
 * Returns VERR_MORE_DATA if current block was parsed (with zero or more pairs
 * stored in stream block) but still contains incomplete (unterminated)
 * data.
 *
 * Returns VINF_SUCCESS if current block was parsed until the next upcoming
 * block (with zero or more pairs stored in stream block).
 *
 * @return IPRT status code.
 * @param streamBlock               Reference to guest stream block to fill.
 *
 */
int GuestProcessStream::ParseBlock(GuestProcessStreamBlock &streamBlock)
{
    if (   !m_pbBuffer
        || !m_cbSize)
    {
        return VERR_NO_DATA;
    }

    AssertReturn(m_cbOffset <= m_cbSize, VERR_INVALID_PARAMETER);
    if (m_cbOffset == m_cbSize)
        return VERR_NO_DATA;

    int rc = VINF_SUCCESS;

    char    *pszOff    = (char*)&m_pbBuffer[m_cbOffset];
    char    *pszStart  = pszOff;
    uint32_t uDistance;
    while (*pszStart)
    {
        size_t pairLen = strlen(pszStart);
        uDistance = (pszStart - pszOff);
        if (m_cbOffset + uDistance + pairLen + 1 >= m_cbSize)
        {
            rc = VERR_MORE_DATA;
            break;
        }
        else
        {
            char *pszSep = strchr(pszStart, '=');
            char *pszVal = NULL;
            if (pszSep)
                pszVal = pszSep + 1;
            if (!pszSep || !pszVal)
            {
                rc = VERR_MORE_DATA;
                break;
            }

            /* Terminate the separator so that we can
             * use pszStart as our key from now on. */
            *pszSep = '\0';

            rc = streamBlock.SetValue(pszStart, pszVal);
            if (RT_FAILURE(rc))
                return rc;
        }

        /* Next pair. */
        pszStart += pairLen + 1;
    }

    /* If we did not do any movement but we have stuff left
     * in our buffer just skip the current termination so that
     * we can try next time. */
    uDistance = (pszStart - pszOff);
    if (   !uDistance
        && *pszStart == '\0'
        && m_cbOffset < m_cbSize)
    {
        uDistance++;
    }
    m_cbOffset += uDistance;

    return rc;
}

GuestBase::GuestBase(void)
    : mConsole(NULL),
      mNextContextID(0)
{
}

GuestBase::~GuestBase(void)
{
}

int GuestBase::baseInit(void)
{
    int rc = RTCritSectInit(&mWaitEventCritSect);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void GuestBase::baseUninit(void)
{
    LogFlowThisFuncEnter();

    int rc = RTCritSectDelete(&mWaitEventCritSect);

    LogFlowFuncLeaveRC(rc);
    /* No return value. */
}

int GuestBase::cancelWaitEvents(void)
{
    LogFlowThisFuncEnter();

    int rc = RTCritSectEnter(&mWaitEventCritSect);
    if (RT_SUCCESS(rc))
    {
        GuestWaitEventTypes::iterator itEventTypes = mWaitEvents.begin();
        while (itEventTypes != mWaitEvents.end())
        {
            GuestWaitEvents::iterator itEvents = itEventTypes->second.begin();
            while (itEvents != itEventTypes->second.end())
            {
                GuestWaitEvent *pEvent = (*itEvents);
                AssertPtr(pEvent);

                /*
                 * Just cancel the event and remove it from the wait events
                 * map. Don't delete it though, this (hopefully) is done by
                 * the caller using unregisterWaitEvent().
                 */
                int rc2 = pEvent->Signal(NULL /* Cancel */);
                AssertRC(rc2);
                itEvents = itEventTypes->second.erase(itEvents);
            }

            mWaitEvents.erase(itEventTypes);
            itEventTypes = mWaitEvents.begin();
        }

        int rc2 = RTCritSectLeave(&mWaitEventCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestBase::generateContextID(uint32_t uSessionID, uint32_t uObjectID, uint32_t *puContextID)
{
    AssertPtrReturn(puContextID, VERR_INVALID_POINTER);

    uint32_t uCount = ASMAtomicIncU32(&mNextContextID);
    if (uCount == VBOX_GUESTCTRL_MAX_CONTEXTS)
        uCount = 0;

    uint32_t uNewContextID =
        VBOX_GUESTCTRL_CONTEXTID_MAKE(uSessionID, uObjectID, uCount);

    *puContextID = uNewContextID;

    return VINF_SUCCESS;
}

int GuestBase::registerWaitEvent(uint32_t uSessionID, uint32_t uObjectID,
                                 const std::list<VBoxEventType_T> &lstEvents,
                                 GuestWaitEvent **ppEvent)
{
    AssertPtrReturn(ppEvent, VERR_INVALID_POINTER);

    uint32_t uContextID;
    int rc = generateContextID(uSessionID, uObjectID, &uContextID);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTCritSectEnter(&mWaitEventCritSect);
    if (RT_SUCCESS(rc))
    {
        try
        {
            GuestWaitEvent *pEvent = new GuestWaitEvent(uContextID, lstEvents);
            AssertPtr(pEvent);

            for (std::list<VBoxEventType_T>::const_iterator itEvents = lstEvents.begin();
                 itEvents != lstEvents.end(); itEvents++)
            {
                mWaitEvents[(*itEvents)].push_back(pEvent);
            }

            *ppEvent = pEvent;
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }

        int rc2 = RTCritSectLeave(&mWaitEventCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

int GuestBase::signalWaitEvents(VBoxEventType_T aType, IEvent *aEvent)
{
    int rc = RTCritSectEnter(&mWaitEventCritSect);
    if (RT_SUCCESS(rc))
    {
        GuestWaitEventTypes::iterator itTypes = mWaitEvents.find(aType);
        if (itTypes != mWaitEvents.end())
        {
            for (GuestWaitEvents::iterator itEvents = itTypes->second.begin();
                 itEvents != itTypes->second.end(); itEvents++)
            {
                ComPtr<IEvent> pThisEvent = aEvent;
                Assert(!pThisEvent.isNull());
                int rc2 = (*itEvents)->Signal(aEvent);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }

        int rc2 = RTCritSectLeave(&mWaitEventCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

void GuestBase::unregisterWaitEvent(GuestWaitEvent *pEvent)
{
    if (!pEvent) /* Nothing to unregister. */
        return;

    int rc = RTCritSectEnter(&mWaitEventCritSect);
    if (RT_SUCCESS(rc))
    {
        const std::list<VBoxEventType_T> lstTypes = pEvent->Types();
        for (std::list<VBoxEventType_T>::const_iterator itEvents = lstTypes.begin();
             itEvents != lstTypes.end(); itEvents++)
        {
            /** @todo Slow O(n) lookup. Optimize this. */
            GuestWaitEvents::iterator itCurEvent = mWaitEvents[(*itEvents)].begin();
            while (itCurEvent != mWaitEvents[(*itEvents)].end())
            {
             if ((*itCurEvent) == pEvent)
             {
                 itCurEvent = mWaitEvents[(*itEvents)].erase(itCurEvent);
                 break;
             }
             else
                 itCurEvent++;
            }
        }

        delete pEvent;
        pEvent = NULL;

        int rc2 = RTCritSectLeave(&mWaitEventCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
}

int GuestBase::waitForEvent(GuestWaitEvent *pEvent, uint32_t uTimeoutMS,
                            VBoxEventType_T *pType, IEvent **ppEvent)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    LogFlowFunc(("pEvent=%p, uTimeoutMS=%RU32\n",
                 pEvent, uTimeoutMS));

    int vrc = pEvent->Wait(uTimeoutMS);
    if (RT_SUCCESS(vrc))
    {
        const ComPtr<IEvent> pThisEvent = pEvent->Event();
        Assert(!pThisEvent.isNull());

        if (pType)
        {
            HRESULT hr = pThisEvent->COMGETTER(Type)(pType);
            if (FAILED(hr))
                vrc = VERR_COM_UNEXPECTED;
        }
        if (   RT_SUCCESS(vrc)
            && ppEvent)
            pThisEvent.queryInterfaceTo(ppEvent);

        unconst(pThisEvent).setNull();
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

GuestObject::GuestObject(void)
    : mSession(NULL),
      mObjectID(0)
{
}

GuestObject::~GuestObject(void)
{
}

int GuestObject::bindToSession(Console *pConsole, GuestSession *pSession, uint32_t uObjectID)
{
    AssertPtrReturn(pConsole, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    mConsole  = pConsole;
    mSession  = pSession;
    mObjectID = uObjectID;

    return VINF_SUCCESS;
}

int GuestObject::registerWaitEvent(const std::list<VBoxEventType_T> &lstEvents,
                                   GuestWaitEvent **ppEvent)
{
    AssertPtr(mSession);
    return GuestBase::registerWaitEvent(mSession->getId(), mObjectID, lstEvents, ppEvent);
}

int GuestObject::sendCommand(uint32_t uFunction,
                             uint32_t uParms, PVBOXHGCMSVCPARM paParms)
{
    LogFlowThisFuncEnter();

#ifndef VBOX_GUESTCTRL_TEST_CASE
    ComObjPtr<Console> pConsole = mConsole;
    Assert(!pConsole.isNull());

    /* Forward the information to the VMM device. */
    VMMDev *pVMMDev = pConsole->getVMMDev();
    AssertPtr(pVMMDev);

    LogFlowThisFunc(("uFunction=%RU32, uParms=%RU32\n", uFunction, uParms));
    int vrc = pVMMDev->hgcmHostCall(HGCMSERVICE_NAME, uFunction, uParms, paParms);
    if (RT_FAILURE(vrc))
    {
        /** @todo What to do here? */
    }
#else
    /* Not needed within testcases. */
    int vrc = VINF_SUCCESS;
#endif
    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

GuestWaitEvent::GuestWaitEvent(uint32_t uCID,
                               const std::list<VBoxEventType_T> &lstEvents)
    : fAborted(false),
      mCID(uCID),
      mEventTypes(lstEvents),
      mEventSem(NIL_RTSEMEVENT)
{
    int rc = RTSemEventCreate(&mEventSem);
    AssertRC(rc);
    /** @todo Throw an exception on failure! */
}

GuestWaitEvent::~GuestWaitEvent(void)
{

}

/**
 * Signals the event. Passing NULL will abort (cancel)
 * the event.
 *
 * @return  IPRT status code.
 * @param   pEvent              IEvent to associate.
 */
int GuestWaitEvent::Signal(IEvent *pEvent)
{
    AssertReturn(mEventSem != NIL_RTSEMEVENT, VERR_CANCELLED);

    if (pEvent)
        mEvent = pEvent;
    else
        ASMAtomicWriteBool(&fAborted, true);

    return RTSemEventSignal(mEventSem);
}

int GuestWaitEvent::Wait(RTMSINTERVAL uTimeoutMS)
{
    LogFlowThisFunc(("uTimeoutMS=%RU32ms\n", uTimeoutMS));

    int rc = VINF_SUCCESS;

    if (ASMAtomicReadBool(&fAborted))
        rc = VERR_CANCELLED;

    if (RT_SUCCESS(rc))
    {
        AssertReturn(mEventSem != NIL_RTSEMEVENT, VERR_CANCELLED);

        RTMSINTERVAL msInterval = uTimeoutMS;
        if (!uTimeoutMS)
            msInterval = RT_INDEFINITE_WAIT;
        rc = RTSemEventWait(mEventSem, msInterval);
        if (ASMAtomicReadBool(&fAborted))
            rc = VERR_CANCELLED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

