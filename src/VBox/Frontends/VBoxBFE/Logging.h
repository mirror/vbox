/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Logging macros and function definitions
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

#ifndef ____H_LOGGING
#define ____H_LOGGING

/*
 * We might be including the VBox logging subsystem before
 * including this header file, so reset the logging group.
 */
#ifdef LOG_GROUP
#undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/thread.h>

/**
 *  Helpful macro to trace execution.
 */

#if defined(DEBUG) || defined(LOG_ENABLED)
#   define LogFlowMember(m)     \
    do { LogFlow (("{%p} ", this)); LogFlow (m); } while (0)
#else // if !DEBUG
#   define LogFlowMember(m)     do {} while (0)
#endif // !DEBUG

#endif // ____H_LOGGING
