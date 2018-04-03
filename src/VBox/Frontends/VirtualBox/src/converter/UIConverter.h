/* $Id$ */
/** @file
 * VBox Qt GUI - UIConverter declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIConverter_h___
#define ___UIConverter_h___

/* GUI includes: */
#include "UIConverterBackend.h"

/** High-level interface for different conversions between GUI classes.
  * @todo Replace singleton with static template interface. */
class SHARED_LIBRARY_STUFF UIConverter
{
public:

    /** Returns singleton instance. */
    static UIConverter *instance() { return s_pInstance; }

    /** Prepares everything. */
    static void prepare();
    /** Cleanups everything. */
    static void cleanup();

    /** Converts QColor <= template class. */
    template<class T> QColor toColor(const T &data) const
    {
        if (canConvert<T>())
            return ::toColor(data);
        AssertFailed();
        return QColor();
    }

    /** Converts QIcon <= template class. */
    template<class T> QIcon toIcon(const T &data) const
    {
        if (canConvert<T>())
            return ::toIcon(data);
        AssertFailed();
        return QIcon();
    }
    /** Converts QPixmap <= template class. */
    template<class T> QPixmap toWarningPixmap(const T &data) const
    {
        if (canConvert<T>())
            return ::toWarningPixmap(data);
        AssertFailed();
        return QPixmap();
    }

    /** Converts QString <= template class. */
    template<class T> QString toString(const T &data) const
    {
        if (canConvert<T>())
            return ::toString(data);
        AssertFailed();
        return QString();
    }
    /** Converts template class <= QString. */
    template<class T> T fromString(const QString &strData) const
    {
        if (canConvert<T>())
            return ::fromString<T>(strData);
        AssertFailed();
        return T();
    }

    /** Converts QString <= template class. */
    template<class T> QString toInternalString(const T &data) const
    {
        if (canConvert<T>())
            return ::toInternalString(data);
        AssertFailed();
        return QString();
    }
    /** Converts template class <= QString. */
    template<class T> T fromInternalString(const QString &strData) const
    {
        if (canConvert<T>())
            return ::fromInternalString<T>(strData);
        AssertFailed();
        return T();
    }

    /** Converts int <= template class. */
    template<class T> int toInternalInteger(const T &data) const
    {
        if (canConvert<T>())
            return ::toInternalInteger(data);
        AssertFailed();
        return 0;
    }
    /** Converts template class <= int. */
    template<class T> T fromInternalInteger(const int &iData) const
    {
        if (canConvert<T>())
            return ::fromInternalInteger<T>(iData);
        AssertFailed();
        return T();
    }

private:

    /** Constructs converter. */
    UIConverter() {}

    /** Holds the static instance. */
    static UIConverter *s_pInstance;
};

/** Singleton UI converter 'official' name. */
#define gpConverter UIConverter::instance()

#endif /* !___UIConverter_h___ */

