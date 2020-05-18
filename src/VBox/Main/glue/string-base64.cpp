/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer - UTF-8 and UTF-16 string classes, BASE64 bits.
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
 */

#include "VBox/com/string.h"

#include <iprt/base64.h>
#include <iprt/errcore.h>


namespace com
{

HRESULT Bstr::base64Encode(const void *pvData, size_t cbData, bool fLineBreaks /*= false*/)
{
    uint32_t const fFlags     = fLineBreaks ? RTBASE64_FLAGS_EOL_LF : RTBASE64_FLAGS_NO_LINE_BREAKS;
    size_t         cwcEncoded = RTBase64EncodedUtf16LengthEx(cbData, fFlags);
    HRESULT hrc = reserveNoThrow(cwcEncoded + 1);
    if (SUCCEEDED(hrc))
    {
        int vrc = RTBase64EncodeUtf16Ex(pvData, cbData, fFlags, mutableRaw(), cwcEncoded, &cwcEncoded);
        AssertRCReturnStmt(vrc, setNull(), E_FAIL);
        hrc = joltNoThrow(cwcEncoded);
    }
    return hrc;
}

int Bstr::base64Decode(void *pvData, size_t cbData, size_t *pcbActual /*= NULL*/, PRTUTF16 *ppwszEnd /*= NULL*/)
{
    return RTBase64DecodeUtf16Ex(raw(), RTSTR_MAX, pvData, cbData, pcbActual, ppwszEnd);
}

ssize_t Bstr::base64DecodedSize(PRTUTF16 *ppwszEnd /*= NULL*/)
{
    return RTBase64DecodedUtf16SizeEx(raw(), RTSTR_MAX, ppwszEnd);
}

} /* namespace com */
