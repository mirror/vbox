/** @file
 * IPRT - C++ Meta programming.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___iprt_cpp_meta_h
#define ___iprt_cpp_meta_h

namespace iprt
{

/** @defgroup grp_rt_cpp_meta   C++ Meta programming utilities
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Check for a condition on compile time and dependent of the result TrueResult
 * or FalseResult will be defined.
 *
 * @param   Condition     Condition to check.
 * @param   TrueResult    Result when condition is true.
 * @param   FalseResult   Result when condition is false
 */
template <bool Condition, typename TrueResult, typename FalseResult>
struct if_;

/**
 * True specialization of if_.
 *
 * @param   TrueResult    Result when condition is true.
 * @param   FalseResult   Result when condition is false
 */
template <typename TrueResult, typename FalseResult>
struct if_<true, TrueResult, FalseResult>
{
    typedef TrueResult result;
};

/**
 * False specialization of if_.
 *
 * @param   TrueResult    Result when condition is true.
 * @param   FalseResult   Result when condition is false
 */
template <typename TrueResult, typename FalseResult>
struct if_<false, TrueResult, FalseResult>
{
    typedef FalseResult result;
};

/** @} */

} /* namespace iprt */

#endif /* !___iprt_cpp_meta_h */

