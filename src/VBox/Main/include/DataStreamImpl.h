/* $Id$ */
/** @file
 * VirtualBox COM class implementation
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


#ifndef ____H_DATASTREAMIMPL
#define ____H_DATASTREAMIMPL

#include "DataStreamWrap.h"


class ATL_NO_VTABLE DataStream
    : public DataStreamWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(DataStream)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(unsigned long aBufferSize);
    void uninit();

    /// Feed data into the stream, used by the stream source.
    /// Blocks if the internal buffer cannot take anything, otherwise
    /// as much as the internal buffer can hold is taken (if smaller
    /// than @a cbWrite). Modeled after RTStrmWriteEx.
    int i_write(const void *pvBuf, size_t cbWrite, size_t *pcbWritten);

private:
    // wrapped IDataStream attributes and methods
    HRESULT getReadSize(ULONG *aReadSize);
    HRESULT read(ULONG aSize, ULONG aTimeoutMS, std::vector<BYTE> &aData);

private:
    std::vector<BYTE> m_aBuffer;
};

#endif // !____H_DATASTREAMIMPL

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
