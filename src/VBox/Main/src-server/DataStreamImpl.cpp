/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation: DataStream
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "DataStreamImpl.h"

#include "AutoCaller.h"
#include "Logging.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Boilerplate constructor & destructor                                                                                         *
*********************************************************************************************************************************/

DEFINE_EMPTY_CTOR_DTOR(DataStream)

HRESULT DataStream::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void DataStream::FinalRelease()
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}


/*********************************************************************************************************************************
*   Initializer & uninitializer                                                                                                  *
*********************************************************************************************************************************/

/**
 * Initializes the DataStream object.
 *
 * @param   aBufferSize     Size of the intermediate buffer.
 *
 */
HRESULT DataStream::init(unsigned long aBufferSize)
{
    LogFlowThisFunc(("cbBuffer=%zu\n", aBufferSize));

    /*
     * Enclose the state transition NotReady->InInit->Ready
     */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /*
     * Allocate data instance.
     */
    HRESULT hrc = S_OK;

    m_aBufferSize        = aBufferSize;
    m_hSemEvtDataAvail   = NIL_RTSEMEVENT;
    m_hSemEvtBufSpcAvail = NIL_RTSEMEVENT;
    int vrc = RTSemEventCreate(&m_hSemEvtDataAvail);
    if (RT_SUCCESS(vrc))
        vrc = RTSemEventCreate(&m_hSemEvtBufSpcAvail);

    if (RT_FAILURE(vrc))
        hrc = setErrorBoth(E_FAIL, vrc, tr("Failed to initialize data stream object (%Rrc)"), vrc);

    /*
     * Done. Just update object readiness state.
     */
    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(hrc);

    LogFlowThisFunc(("returns %Rhrc\n", hrc));
    return hrc;
}

/**
 * Uninitializes the instance (called from FinalRelease()).
 */
void DataStream::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
    {
        m_aBuffer.clear();
        if (m_hSemEvtDataAvail != NIL_RTSEMEVENT)
            RTSemEventDestroy(m_hSemEvtDataAvail);
        if (m_hSemEvtBufSpcAvail != NIL_RTSEMEVENT)
            RTSemEventDestroy(m_hSemEvtBufSpcAvail);
        m_hSemEvtDataAvail = NIL_RTSEMEVENT;
        m_hSemEvtBufSpcAvail = NIL_RTSEMEVENT;
    }

    LogFlowThisFuncLeave();
}


/*********************************************************************************************************************************
*   IDataStream attributes                                                                                                         *
*********************************************************************************************************************************/

HRESULT DataStream::getReadSize(ULONG *aReadSize)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aReadSize = (ULONG)m_aBuffer.size();
    return S_OK;
}


/*********************************************************************************************************************************
*   IDataStream methods                                                                                                            *
*********************************************************************************************************************************/

HRESULT DataStream::read(ULONG aSize, ULONG aTimeoutMS, std::vector<BYTE> &aData)
{
    /*
     * Allocate return buffer.
     */
    try
    {
        aData.resize(aSize);
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Do the reading.  To play safe we exclusivly lock the object while doing this.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int vrc = VINF_SUCCESS;
    while (   !m_aBuffer.size()
           && !m_fEos
           && RT_SUCCESS(vrc))
    {
        /* Wait for something to become available. */
        alock.release();
        vrc = RTSemEventWait(m_hSemEvtDataAvail, aTimeoutMS == 0 ? RT_INDEFINITE_WAIT : aTimeoutMS);
        alock.acquire();
    }

    /*
     * Manage the result.
     */
    HRESULT hrc;
    if (   RT_SUCCESS(vrc)
        && m_aBuffer.size())
    {
        size_t cbCopy = RT_MIN(aSize, m_aBuffer.size());
        if (cbCopy != aSize)
        {
            Assert(cbCopy < aSize);
            aData.resize(cbCopy);
        }
        aData.insert(aData.begin(), m_aBuffer.begin(), m_aBuffer.begin() + cbCopy);
        m_aBuffer.erase(m_aBuffer.begin(), m_aBuffer.begin() + cbCopy);
        vrc = RTSemEventSignal(m_hSemEvtBufSpcAvail);
        AssertRC(vrc);
        hrc = S_OK;
    }
    else
    {
        Assert(   RT_FAILURE(vrc)
               || (   m_fEos
                   && !m_aBuffer.size()));

        aData.resize(0);
        if (vrc == VERR_TIMEOUT)
            hrc = VBOX_E_TIMEOUT;
        else if (RT_FAILURE(vrc))
            hrc = setErrorBoth(E_FAIL, vrc, tr("Error reading %u bytes: %Rrc"), aSize, vrc);
    }

    return hrc;
}


/*********************************************************************************************************************************
*   DataStream internal methods                                                                                                   *
*********************************************************************************************************************************/

/**
 * Writes the given data into the temporary buffer blocking if it is full.
 *
 * @returns IPRT status code.
 * @param   pvBuf           The data to write.
 * @param   cbWrite         How much to write.
 * @param   pcbWritten      Where to store the amount of data written.
 */
int DataStream::i_write(const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(!m_fEos, VERR_INVALID_STATE);

    int vrc = VINF_SUCCESS;
    while (   m_aBuffer.size() == m_aBufferSize
           && RT_SUCCESS(vrc))
    {
        /* Wait for space to become available. */
        alock.release();
        vrc = RTSemEventWait(m_hSemEvtBufSpcAvail, RT_INDEFINITE_WAIT);
        alock.acquire();
    }

    if (RT_SUCCESS(vrc))
    {
        Assert(m_aBuffer.size() < m_aBufferSize);
        size_t cbCopy = RT_MIN(cbWrite, m_aBufferSize - m_aBuffer.size());
        if (m_aBuffer.capacity() < m_aBuffer.size() + cbWrite)
            m_aBuffer.reserve(m_aBuffer.size() + cbWrite);
        memcpy(&m_aBuffer.front() + m_aBuffer.size(), pvBuf, cbCopy);
        *pcbWritten = cbCopy;
        RTSemEventSignal(m_hSemEvtDataAvail);
    }

    return vrc;
}

/**
 * Marks the end of the stream.
 *
 * @returns IPRT status code.
 */
int DataStream::i_close()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m_fEos = true;
    RTSemEventSignal(m_hSemEvtDataAvail);
    return VINF_SUCCESS;
}

