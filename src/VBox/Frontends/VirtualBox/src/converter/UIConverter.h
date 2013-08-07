/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIConverter declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIConverter_h__
#define __UIConverter_h__

/* GUI includes: */
#include "UIConverterBackend.h"

/* High-level interface for different conversions between GUI classes: */
class UIConverter
{
public:

    /* Singleton instance: */
    static UIConverter* instance() { return m_spInstance; }

    /* Prepare/cleanup: */
    static void prepare();
    static void cleanup();

    /* QColor <= template class: */
    template<class T> QColor toColor(const T &data) const
    {
        if (canConvert<T>())
            return ::toColor(data);
        Assert(0); return QColor();
    }

    /* QPixmap <= template class: */
    template<class T> QPixmap toPixmap(const T &data) const
    {
        if (canConvert<T>())
            return ::toPixmap(data);
        Assert(0); return QPixmap();
    }
    /* QPixmap <= template class: */
    template<class T> QPixmap toWarningPixmap(const T &data) const
    {
        if (canConvert<T>())
            return ::toWarningPixmap(data);
        Assert(0); return QPixmap();
    }

    /* QString <= template class: */
    template<class T> QString toString(const T &data) const
    {
        if (canConvert<T>())
            return ::toString(data);
        Assert(0); return QString();
    }
    /* Template class <= QString: */
    template<class T> T fromString(const QString &strData) const
    {
        if (canConvert<T>())
            return ::fromString<T>(strData);
        Assert(0); return T();
    }

    /* QString <= template class: */
    template<class T> QString toInternalString(const T &data) const
    {
        if (canConvert<T>())
            return ::toInternalString(data);
        Assert(0); return QString();
    }
    /* Template class <= QString: */
    template<class T> T fromInternalString(const QString &strData) const
    {
        if (canConvert<T>())
            return ::fromInternalString<T>(strData);
        Assert(0); return T();
    }

private:

    /* Constructor: */
    UIConverter() {}

    /* Static instance: */
    static UIConverter *m_spInstance;
};
#define gpConverter UIConverter::instance()

#endif /* __UIConverter_h__ */

