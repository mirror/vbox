/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Guid class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#if defined (VBOX_WITH_XPCOM)
#include <nsMemory.h>
#endif

#include "VBox/com/string.h"

#include <iprt/cpputils.h>
#include <iprt/uuid.h>

namespace com
{

/**
 *  Helper class that represents the UUID type and hides platform-siecific
 *  implementation details.
 */
class Guid
{
public:

    Guid () { ::RTUuidClear (&uuid); }
    Guid (const Guid &that) { uuid = that.uuid; }
    Guid (const RTUUID &that) { uuid = that; }
    Guid (const GUID &that) { ::memcpy (&uuid, &that, sizeof (GUID)); }
    Guid (const char *that)
    {
        ::RTUuidClear (&uuid);
        ::RTUuidFromStr (&uuid, that);
    }

    Guid &operator= (const Guid &that)
    {
        ::memcpy (&uuid, &that.uuid, sizeof (RTUUID));
        return *this;
    }
    Guid &operator= (const GUID &guid)
    {
        ::memcpy (&uuid, &guid, sizeof (GUID));
        return *this;
    }
    Guid &operator= (const RTUUID &guid)
    {
        ::memcpy (&uuid, &guid, sizeof (RTUUID));
        return *this;
    }
    Guid &operator= (const char *str)
    {
        ::RTUuidFromStr (&uuid, str);
        return *this;
    }

    void create() { ::RTUuidCreate (&uuid); }
    void clear() { ::RTUuidClear (&uuid); }

    Utf8Str toString () const
    {
        char buf [RTUUID_STR_LENGTH];
        ::RTUuidToStr (&uuid, buf, RTUUID_STR_LENGTH);
        return Utf8Str (buf);
    }

    bool isEmpty() const { return ::RTUuidIsNull (&uuid) != 0; }
    operator bool() const { return !isEmpty(); }

    bool operator== (const Guid &that) const { return ::RTUuidCompare (&uuid, &that.uuid) == 0; }
    bool operator== (const GUID &guid) const { return ::RTUuidCompare (&uuid, (PRTUUID) &guid) == 0; }
    bool operator!= (const Guid &that) const { return !operator==(that); }
    bool operator!= (const GUID &guid) const { return !operator==(guid); }
    bool operator< (const Guid &that) const { return ::RTUuidCompare (&uuid, &that.uuid) < 0; }
    bool operator< (const GUID &guid) const { return ::RTUuidCompare (&uuid, (PRTUUID) &guid) < 0; }

    /* to pass instances as GUIDPARAM parameters to interface methods */
    operator const GUID &() const { return *(GUID *) &uuid; }

    /* to directly pass instances to RTPrintf("%Vuuid") */
    PRTUUID ptr() { return &uuid; }

    /* to pass instances to printf-like functions */
    PCRTUUID raw() const { return &uuid; }

    /* to pass instances to RTUuid*() as a constant argument */
    operator const RTUUID * () const { return &uuid; }

#if !defined (VBOX_WITH_XPCOM)

    /* to assign instances to GUIDPARAMOUT parameters from within the
     *  interface method */
    const Guid &cloneTo (GUID *pguid) const
    {
        if (pguid) { ::memcpy (pguid, &uuid, sizeof (GUID)); }
        return *this;
    }

    /* to pass instances as GUIDPARAMOUT parameters to interface methods */
    GUID *asOutParam() { return (GUID *) &uuid; }

#else

    /* to assign instances to GUIDPARAMOUT parameters from within the
     * interface method */
    const Guid &cloneTo (nsID **ppguid) const
    {
        if (ppguid) { *ppguid = (nsID *) nsMemory::Clone (&uuid, sizeof (nsID)); }
        return *this;
    }

    /* internal helper class for asOutParam() */
    class GuidOutParam
    {
        GuidOutParam (Guid &guid) : ptr (0), outer (guid) { outer.clear(); }
        nsID *ptr;
        Guid &outer;
        GuidOutParam (const GuidOutParam &that); // disabled
        GuidOutParam &operator= (const GuidOutParam &that); // disabled
    public:
        operator nsID **() { return &ptr; }
        ~GuidOutParam()
        {
            if (ptr && outer.isEmpty()) { outer = *ptr; nsMemory::Free (ptr); }
        }
        friend class Guid;
    };

    /* to pass instances as GUIDPARAMOUT parameters to interface methods */
    GuidOutParam asOutParam() { return GuidOutParam (*this); }

#endif

    /* to directly test GUIDPARAM interface method's parameters */
    static bool isEmpty (const GUID &guid)
    {
        return ::RTUuidIsNull ((PRTUUID) &guid) != 0;
    }

    /**
     *  Static immutable empty object. May be used for comparison purposes.
     */
    static const Guid Empty;

private:

    RTUUID uuid;
};

/* work around error C2593 of the stupid MSVC 7.x ambiguity resolver */
WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP (Guid);

} /* namespace com */

#endif /* ___VBox_com_Guid_h */

