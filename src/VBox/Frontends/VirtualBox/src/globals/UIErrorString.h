/* $Id$ */
/** @file
 * VBox Qt GUI - UIErrorString class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIErrorString_h
#define FEQT_INCLUDED_SRC_globals_UIErrorString_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QString>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Other VBox includes: */
#include <VBox/com/defs.h>

/* Forward declarations: */
class COMBaseWithEI;
class COMErrorInfo;
class COMResult;
class CProgress;
class CVirtualBoxErrorInfo;

/** Namespace simplifying COM error formatting. */
class SHARED_LIBRARY_STUFF UIErrorString
{
public:

    /** Returns formatted @a rc information. */
    static QString formatRC(HRESULT rc);
    /** Returns full formatted @a rc information. */
    static QString formatRCFull(HRESULT rc);
    /** Returns formatted error information for passed @a comProgress. */
    static QString formatErrorInfo(const CProgress &comProgress);
    /** Returns formatted error information for passed @a comInfo and @a wrapperRC. */
    static QString formatErrorInfo(const COMErrorInfo &comInfo, HRESULT wrapperRC = S_OK);
    /** Returns formatted error information for passed @a comInfo. */
    static QString formatErrorInfo(const CVirtualBoxErrorInfo &comInfo);
    /** Returns formatted error information for passed @a comWrapper. */
    static QString formatErrorInfo(const COMBaseWithEI &comWrapper);
    /** Returns formatted error information for passed @a comRc. */
    static QString formatErrorInfo(const COMResult &comRc);

private:

    /** Converts passed @a comInfo and @a wrapperRC to string. */
    static QString errorInfoToString(const COMErrorInfo &comInfo, HRESULT wrapperRC = S_OK);
};

#endif /* !FEQT_INCLUDED_SRC_globals_UIErrorString_h */

