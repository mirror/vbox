/* $Id$ */
/** @file
 * VBox Qt GUI - Global library definitions.
 */

/*
 * Copyright (C) 2018-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UILibraryDefs_h
#define FEQT_INCLUDED_SRC_globals_UILibraryDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Define shared library stuff: */
#ifdef VBOX_GUI_LIBRARY
# define SHARED_LIBRARY_STUFF DECLEXPORT_CLASS
#else
# define SHARED_LIBRARY_STUFF DECLIMPORT_CLASS
#endif

#endif /* !FEQT_INCLUDED_SRC_globals_UILibraryDefs_h */
