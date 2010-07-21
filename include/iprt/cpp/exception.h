/** @file
 * IPRT - C++ Base Exceptions.
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

#ifndef ___iprt_cpp_exception_h
#define ___iprt_cpp_exception_h

#include <iprt/cpp/ministring.h>

/** @defgroup grp_rt_cppxcpt    C++ Exceptions
 * @ingroup grp_rt
 * @{
 */

namespace iprt
{

/**
 * Base exception class for IPRT, derived from std::exception.
 * The XML exceptions are based on this.
 */
class RT_DECL_CLASS Error
    : public std::exception
{
public:

    Error(const char *pcszMessage)
        : m_s(pcszMessage)
    {
    }

    Error(const iprt::MiniString &s)
        : m_s(s)
    {
    }

    Error(const Error &s)
        : std::exception(s),
          m_s(s.what())
    {
    }

    virtual ~Error() throw()
    {
    }

    void operator=(const Error &s)
    {
        m_s = s.what();
    }

    void setWhat(const char *pcszMessage)
    {
        m_s = pcszMessage;
    }

    virtual const char* what() const throw()
    {
        return m_s.c_str();
    }

private:
    /* Hide the default constructor to make sure the extended one above is
       always used. */
    Error();

    iprt::MiniString m_s;
};

} /* namespace iprt */

/** @} */

#endif

