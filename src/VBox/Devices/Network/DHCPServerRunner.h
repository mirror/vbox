/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#include <iprt/err.h>
#include <iprt/types.h>
#include <iprt/string.h>
#include <iprt/mem.h>
//#include <VBox/com/string.h>

//using namespace com;
typedef enum
{
    DHCPCFG_NAME = 1,
    DHCPCFG_NETNAME,
    DHCPCFG_TRUNKTYPE,
    DHCPCFG_TRUNKNAME,
    DHCPCFG_MACADDRESS,
    DHCPCFG_IPADDRESS,
    DHCPCFG_LEASEDB,
    DHCPCFG_VERBOSE,
    DHCPCFG_GATEWAY,
    DHCPCFG_LOWERIP,
    DHCPCFG_UPPERIP,
    DHCPCFG_NETMASK,
    DHCPCFG_HELP,
    DHCPCFG_VERSION,
    DHCPCFG_BEGINCONFIG,
    DHCPCFG_NOTOPT_MAXVAL
}DHCPCFG;

#define TRUNKTYPE_WHATEVER "whatever"
#define TRUNKTYPE_NETFLT   "netflt"
#define TRUNKTYPE_NETADP   "netadp"
#define TRUNKTYPE_SRVNAT   "srvnat"

class Utf8Str
{
public:

    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };

    typedef char *String;
    typedef const char *ConstString;

    Utf8Str () : str (NULL) {}

    Utf8Str (const Utf8Str &that) : str (NULL) { raw_copy (str, that.str); }
    Utf8Str (const char *that) : str (NULL) { raw_copy (str, that); }

    /** Shortcut that calls #alloc(aSize) right after object creation. */
    Utf8Str (size_t aSize) : str (NULL) { alloc(aSize); }

    virtual ~Utf8Str () { setNull(); }

    Utf8Str &operator = (const Utf8Str &that) { safe_assign (that.str); return *this; }
    Utf8Str &operator = (const char *that) { safe_assign (that); return *this; }

    Utf8Str &setNull()
    {
        if (str)
        {
            ::RTStrFree (str);
            str = NULL;
        }
        return *this;
    }

    Utf8Str &setNullIfEmpty()
    {
        if (str && *str == 0)
        {
            ::RTStrFree (str);
            str = NULL;
        }
        return *this;
    }

    /**
     *  Allocates memory for a string capable to store \a aSize - 1 bytes (not characters!);
     *  in other words, aSize includes the terminating zero character. If \a aSize
     *  is zero, or if a memory allocation error occurs, this object will become null.
     */
    Utf8Str &alloc (size_t aSize)
    {
        setNull();
        if (aSize)
        {
#if !defined (VBOX_WITH_XPCOM)
            str = (char *) ::RTMemTmpAlloc (aSize);
#else
            str = (char *) nsMemory::Alloc (aSize);
#endif
            if (str)
                str [0] = 0;
        }
        return *this;
    }

    void append(const Utf8Str &that)
    {
        size_t cbThis = length();
        size_t cbThat = that.length();

        if (cbThat)
        {
            size_t cbBoth = cbThis + cbThat + 1;

            // @todo optimize with realloc() once the memory management is fixed
            char *pszTemp;
            pszTemp = (char*)::RTMemTmpAlloc(cbBoth);
            if (str)
            {
                memcpy(pszTemp, str, cbThis);
                setNull();
            }
            if (that.str)
                memcpy(pszTemp + cbThis, that.str, cbThat);
            pszTemp[cbThis + cbThat] = '\0';

            str = pszTemp;
        }
    }

    int compare (const char *pcsz, CaseSensitivity cs = CaseSensitive) const
    {
        if (str == pcsz)
            return 0;
        if (str == NULL)
            return -1;
        if (pcsz == NULL)
            return 1;

        if (cs == CaseSensitive)
            return ::RTStrCmp(str, pcsz);
        else
            return ::RTStrICmp(str, pcsz);
    }

    int compare (const Utf8Str &that, CaseSensitivity cs = CaseSensitive) const
    {
        return compare (that.str, cs);
    }

    bool operator == (const Utf8Str &that) const { return !compare (that); }
    bool operator != (const Utf8Str &that) const { return !!compare (that); }
    bool operator == (const char *that) const { return !compare (that); }
    bool operator != (const char *that) const { return !!compare (that); }
    bool operator < (const Utf8Str &that) const { return compare (that) < 0; }
    bool operator < (const char *that) const { return compare (that) < 0; }

    bool endsWith (const Utf8Str &that, CaseSensitivity cs = CaseSensitive) const
    {
        if (isNull() || that.isNull())
            return false;

        if (length() < that.length())
            return false;

        int l = length() - that.length();
        if (cs == CaseSensitive)
            return ::RTStrCmp(&str[l], that.str) == 0;
        else
            return ::RTStrICmp(&str[l], that.str) == 0;
    }

    bool startsWith (const Utf8Str &that, CaseSensitivity cs = CaseSensitive) const
    {
        if (isNull() || that.isNull())
            return false;

        if (length() < that.length())
            return false;

        if (cs == CaseSensitive)
            return ::RTStrNCmp(str, that.str, that.length()) == 0;
        else
            return ::RTStrNICmp(str, that.str, that.length()) == 0;
    }

    bool isNull() const { return str == NULL; }
    operator bool() const { return !isNull(); }

    bool isEmpty() const { return isNull() || *str == 0; }

    size_t length() const { return isNull() ? 0 : ::strlen (str); }

    /** Intended to to pass instances as input (|char *|) parameters to methods. */
    operator const char *() const { return str; }

    /** The same as operator const char *(), but for situations where the compiler
        cannot typecast implicitly (for example, in printf() argument list). */
    const char *raw() const { return str; }

    /** The same as operator const char *(), but for situations where the compiler
        cannot typecast implicitly (for example, in printf() argument list). */
    const char *c_str() const { return str; }

    /**
     *  Returns a non-const raw pointer that allows to modify the string directly.
     *  @warning
     *      Be sure not to modify data beyond the allocated memory! The
     *      guaranteed size of the allocated memory is at least #length()
     *      bytes after creation and after every assignment operation.
     */
    char *mutableRaw() { return str; }

    /**
     *  Intended to assign instances to |char *| out parameters from within the
     *  interface method. Transfers the ownership of the duplicated string to the
     *  caller.
     */
    const Utf8Str &cloneTo (char **pstr) const
    {
        if (pstr)
        {
            *pstr = NULL;
            raw_copy (*pstr, str);
        }
        return *this;
    }

    /**
     *  Intended to assign instances to |char *| out parameters from within the
     *  interface method. Transfers the ownership of the original string to the
     *  caller and resets the instance to null.
     *
     *  As opposed to cloneTo(), this method doesn't create a copy of the
     *  string.
     */
    Utf8Str &detachTo (char **pstr)
    {
        *pstr = str;
        str = NULL;
        return *this;
    }

    static const size_t npos;

    /**
     *  Intended to pass instances as out (|char **|) parameters to methods.
     *  Takes the ownership of the returned data.
     */
    char **asOutParam() { setNull(); return &str; }

    /**
     *  Static immutable null object. May be used for comparison purposes.
     */
    static const Utf8Str Null;

protected:

    void safe_assign (const char *s)
    {
        if (str != s)
        {
            setNull();
            raw_copy (str, s);
        }
    }

    inline static void raw_copy (char *&ls, const char *rs)
    {
        if (rs)
//#if !defined (VBOX_WITH_XPCOM)
            ::RTStrDupEx (&ls, rs);
//#else
//            ls = (char *) nsMemory::Clone (rs, strlen (rs) + 1);
//#endif
    }

    char *str;

};

class DHCPServerRunner
{
public:
    DHCPServerRunner() : mProcess (NIL_RTPROCESS) {}
    ~DHCPServerRunner() { stop(); /* don't leave abandoned servers */}

    int setOption(DHCPCFG opt, const char *val)
    {
        if(opt == 0 || opt >= DHCPCFG_NOTOPT_MAXVAL)
            return VERR_INVALID_PARAMETER;
        if(isRunning())
            return VERR_INVALID_STATE;

#ifdef RT_OS_WINDOWS
        if(val)
        {
            mOptions[opt] = "\"";
            mOptions[opt].append(val);
            mOptions[opt].append("\"");
        }
#endif
        else
        {
            mOptions[opt] = val;
        }
        return VINF_SUCCESS;
    }

    int start();
    int stop();
    bool isRunning();

    void detachFromServer();
private:
    Utf8Str mOptions[DHCPCFG_NOTOPT_MAXVAL];
    RTPROCESS mProcess;
};
