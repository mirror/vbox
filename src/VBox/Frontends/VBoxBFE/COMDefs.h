/** @file
 * VBox frontends: Basic Frontend (BFE):
 * COM definitions
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef COMDefs_H
#define COMDefs_H

#ifdef RT_OS_WINDOWS
# include <Windows.h>
#endif
#include <stdarg.h>
#include <iprt/assert.h>

/* This is a transitional file used to get rid of all COM
 * definitions from COM/XPCOM. */

#define COMGETTER(n)    get_##n
#define COMSETTER(n)    put_##n

#define NS_LIKELY(x)    RT_LIKELY(x)
#define NS_UNLIKELY(x)  RT_UNLIKELY(x)

#define NS_SUCCEEDED(_nsresult) (NS_LIKELY(!((_nsresult) & 0x80000000)))
#define NS_FAILED(_nsresult)    (NS_UNLIKELY((_nsresult) & 0x80000000))

#define ATL_NO_VTABLE

#ifndef RT_OS_WINDOWS
  typedef unsigned long  HRESULT;
  typedef unsigned long  ULONG; /// @todo 64-bit: ULONG is 32-bit.
  typedef signed   long  LONG;  /// @todo 64-bit: ULONG is 32-bit.
  typedef unsigned short USHORT;
  typedef bool           BOOL;
  typedef unsigned char  BYTE;
# define STDMETHODIMP   unsigned long
# define STDMETHOD(a)   virtual unsigned long a
# define S_OK           0

# if ! defined(E_NOTIMPL)
#  define E_NOTIMPL     ((unsigned long) 0x80004001L)
# endif
# define E_POINTER      ((unsigned long) 0x80004003L)
# define E_FAIL         ((unsigned long) 0x80004005L)
# define E_UNEXPECTED   ((unsigned long) 0x8000ffffL)
# define E_INVALIDARG   ((unsigned long) 0x80070057L)

# if ! defined(FALSE)
#  define FALSE false
# endif

# if ! defined(TRUE)
#  define TRUE  true
# endif

# define SUCCEEDED      NS_SUCCEEDED
# define FAILED         NS_FAILED
#endif /* !RT_OS_WINDOWS */

#endif
