/** @file
 * innotek Portable Runtime - C++ Utilities (useful templates, defines and such).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___iprt_cpputils_h
#define ___iprt_cpputils_h

/** @defgroup grp_rt_cpputils   C++ Utilities
 * @ingroup grp_rt
 * @{
 */

/**
 * Shortcut to |const_cast<C &>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class that;
 *      ...
 *      unconst (that) = some_value;
 * @endcode
 */
template <class C>
inline C &unconst (const C &that) { return const_cast <C &> (that); }


/**
 * Shortcut to |const_cast<C *>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class *that;
 *      ...
 *      unconst (that) = some_value;
 * @endcode
 */
template <class C>
inline C *unconst (const C *that) { return const_cast <C *> (that); }

/** @} */

#endif

