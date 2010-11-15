/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer - Guid class declaration.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_com_Guid_h
#define ___VBox_com_Guid_h

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#if defined(VBOX_WITH_XPCOM)
# include <nsMemory.h>
#endif

#include "VBox/com/string.h"

#include <iprt/uuid.h>
#include <iprt/err.h>

namespace com
{

/**
 *  Helper class that represents the UUID type and hides platform-specific
 *  implementation details.
 */
class Guid
{
public:

    Guid()
    {
        ::RTUuidClear(&mUuid);
        refresh();
    }

    Guid(const Guid &that)
    {
        mUuid = that.mUuid;
        refresh();
    }

    Guid(const RTUUID &that)
    {
        mUuid = that;
        refresh();
    }

    Guid(const GUID &that)
    {
        AssertCompileSize(GUID, sizeof(RTUUID));
        ::memcpy(&mUuid, &that, sizeof(GUID));
        refresh();
    }

    /**
     * Construct a GUID from a string.
     *
     * Should the string be invalid, the object will be set to the null GUID
     * (isEmpty() == true).
     *
     * @param   that        The UUID string.  We feed this to RTUuidFromStr(),
     *                      so check it out for the exact format.
     */
    Guid(const char *that)
    {
        int rc = ::RTUuidFromStr(&mUuid, that);
        if (RT_FAILURE(rc))
            ::RTUuidClear(&mUuid);
        refresh();
    }

    /**
     * Construct a GUID from a BSTR.
     *
     * Should the string be empty or invalid, the object will be set to the
     * null GUID (isEmpty() == true).
     *
     * @param   that        The UUID BSTR.  We feed this to RTUuidFromUtf16(),
     *                      so check it out for the exact format.
     */
    Guid(const Bstr &that)
    {
        int rc = !that.isEmpty()
               ? ::RTUuidFromUtf16(&mUuid, that.raw())
               : VERR_INVALID_UUID_FORMAT;
        if (RT_FAILURE(rc))
            ::RTUuidClear(&mUuid);
        refresh();
    }

    Guid& operator=(const Guid &that)
    {
        ::memcpy(&mUuid, &that.mUuid, sizeof (RTUUID));
        refresh();
        return *this;
    }
    Guid& operator=(const GUID &guid)
    {
        ::memcpy(&mUuid, &guid, sizeof (GUID));
        refresh();
        return *this;
    }
    Guid& operator=(const RTUUID &guid)
    {
        ::memcpy(&mUuid, &guid, sizeof (RTUUID));
        refresh();
        return *this;
    }
    Guid& operator=(const char *str)
    {
        int rc = ::RTUuidFromStr(&mUuid, str);
        if (RT_FAILURE(rc))
            ::RTUuidClear(&mUuid);
        refresh();
        return *this;
    }

    void create()
    {
        ::RTUuidCreate(&mUuid);
        refresh();
    }
    void clear()
    {
        ::RTUuidClear(&mUuid);
        refresh();
    }

    /**
     * Convert the GUID to a string.
     *
     * @returns String object containing the formatted GUID.
     * @throws  std::bad_alloc
     */
    Utf8Str toString() const
    {
        char buf[RTUUID_STR_LENGTH];
        ::RTUuidToStr(&mUuid, buf, RTUUID_STR_LENGTH);
        return Utf8Str(buf);
    }

    /**
     * Like toString, but encloses the returned string in curly brackets.
     *
     * @returns String object containing the formatted GUID in curly brackets.
     * @throws  std::bad_alloc
     */
    Utf8Str toStringCurly() const
    {
        char buf[RTUUID_STR_LENGTH + 2] = "{";
        ::RTUuidToStr(&mUuid, buf + 1, RTUUID_STR_LENGTH);
        buf[sizeof(buf) - 2] = '}';
        buf[sizeof(buf) - 1] = '\0';
        return Utf8Str(buf);
    }

    /**
     * Convert the GUID to a string.
     *
     * @returns Bstr object containing the formatted GUID.
     * @throws  std::bad_alloc
     */
    Bstr toUtf16() const
    {
        if (isEmpty())
          return Bstr();

        RTUTF16 buf[RTUUID_STR_LENGTH];
        ::RTUuidToUtf16(&mUuid, buf, RTUUID_STR_LENGTH);
        return Bstr(buf);
    }

    bool isEmpty() const
    {
        return ::RTUuidIsNull(&mUuid);
    }

    bool isNotEmpty() const
    {
        return !::RTUuidIsNull(&mUuid);
    }

    bool operator==(const Guid &that) const { return ::RTUuidCompare(&mUuid, &that.mUuid)    == 0; }
    bool operator==(const GUID &guid) const { return ::RTUuidCompare(&mUuid, (PRTUUID)&guid) == 0; }
    bool operator!=(const Guid &that) const { return !operator==(that); }
    bool operator!=(const GUID &guid) const { return !operator==(guid); }
    bool operator<( const Guid &that) const { return ::RTUuidCompare(&mUuid, &that.mUuid)    < 0; }
    bool operator<( const GUID &guid) const { return ::RTUuidCompare(&mUuid, (PRTUUID)&guid) < 0; }

    /**
     * To directly copy the contents to a GUID, or for passing it as an input
     * parameter of type (const GUID *), the compiler converts. */
    const GUID &ref() const
    {
        return *(const GUID *)&mUuid;
    }

    /**
     * To pass instances to printf-like functions.
     */
    PCRTUUID raw() const
    {
        return (PCRTUUID)&mUuid;
    }

#if !defined(VBOX_WITH_XPCOM)

    /** To assign instances to OUT_GUID parameters from within the interface
     * method. */
    const Guid &cloneTo(GUID *pguid) const
    {
        if (pguid)
            ::memcpy(pguid, &mUuid, sizeof(GUID));
        return *this;
    }

    /** To pass instances as OUT_GUID parameters to interface methods. */
    GUID *asOutParam()
    {
        return (GUID *)&mUuid;
    }

#else

    /** To assign instances to OUT_GUID parameters from within the
     * interface method */
    const Guid &cloneTo(nsID **ppGuid) const
    {
        if (ppGuid)
            *ppGuid = (nsID *)nsMemory::Clone(&mUuid, sizeof(nsID));
        return *this;
    }

    /**
     * Internal helper class for asOutParam().
     *
     * This takes a GUID refrence in the constructor and copies the mUuid from
     * the method to that instance in its destructor.
     */
    class GuidOutParam
    {
        GuidOutParam(Guid &guid)
            : ptr(0),
              outer(guid)
        {
            outer.clear();
        }

        nsID *ptr;
        Guid &outer;
        GuidOutParam(const GuidOutParam &that); // disabled
        GuidOutParam &operator=(const GuidOutParam &that); // disabled
    public:
        operator nsID**() { return &ptr; }
        ~GuidOutParam()
        {
            if (ptr && outer.isEmpty())
            {
                outer = *ptr;
                outer.refresh();
                nsMemory::Free(ptr);
            }
        }
        friend class Guid;
    };

    /** to pass instances as OUT_GUID parameters to interface methods */
    GuidOutParam asOutParam() { return GuidOutParam(*this); }

#endif

    /* to directly test IN_GUID interface method's parameters */
    static bool isEmpty(const GUID &guid)
    {
        return ::RTUuidIsNull((PRTUUID)&guid);
    }

    /**
     *  Static immutable empty object. May be used for comparison purposes.
     */
    static const Guid Empty;

private:
    /**
     * Refresh the debug-only UUID string.
     *
     * In debug code, refresh the UUID string representatino for debugging;
     * must be called every time the internal uuid changes; compiles to nothing
     * in release code.
     */
    inline void refresh()
    {
#ifdef DEBUG
        ::RTUuidToStr(&mUuid, mszUuid, RTUUID_STR_LENGTH);
        m_pcszUUID = mszUuid;
#endif
    }

    /** The UUID. */
    RTUUID mUuid;

#ifdef DEBUG
    /** String representation of mUuid for printing in the debugger. */
    char mszUuid[RTUUID_STR_LENGTH];
    /** Another string variant for the debugger, points to szUUID. */
    const char *m_pcszUUID;
#endif
};

inline Bstr asGuidStr(const Bstr& str)
{
   Guid guid(str);
   return guid.isEmpty() ? Bstr() : guid.toUtf16();
}

inline bool isValidGuid(const Bstr& str)
{
   Guid guid(str);
   return !guid.isEmpty();
}

} /* namespace com */

#endif /* !___VBox_com_Guid_h */

