/* $Id$ */
/** @file
 * VBox NLS.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef MAIN_INCLUDED_VBoxNls_h
#define MAIN_INCLUDED_VBoxNls_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VBOX_WITH_MAIN_NLS

#include <VBox/com/defs.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>
#include "VirtualBoxTranslator.h"


# define DECLARE_TRANSLATION_CONTEXT(ctx) \
struct ctx \
{\
   static const char *tr(const char *pszSource, const char *pszComment = NULL, const size_t aNum = ~(size_t)0) \
   { \
       return VirtualBoxTranslator::translate(NULL, #ctx, pszSource, pszComment, aNum); \
   } \
}
#else
# define DECLARE_TRANSLATION_CONTEXT(ctx) \
struct ctx \
{\
   static const char *tr(const char *pszSource, const char *pszComment = NULL, const size_t aNum = ~(size_t)0) \
   { \
       NOREF(pszComment); \
       NOREF(aNum);       \
       return pszSource;  \
   } \
}
#endif

#endif /* !MAIN_INCLUDED_VBoxNls_h */

