/* $Id$ */
/** @file
 * VirtualBox COM - logging macros and function definitions, for new code.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_LoggingNew_h
#define MAIN_INCLUDED_LoggingNew_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef MAIN_INCLUDED_Logging_h
# error "You must include LoggingNew.h as the first include!"
#endif
#define MAIN_INCLUDED_Logging_h /* Prevent Logging.h from being included. */

#ifndef LOG_GROUP
# error "You must define LOG_GROUP immediately before including LoggingNew.h!"
#endif

#ifdef LOG_GROUP_MAIN_OVERRIDE
# error "Please, don't define LOG_GROUP_MAIN_OVERRIDE anymore!"
#endif

#include <VBox/log.h>

#endif /* !MAIN_INCLUDED_LoggingNew_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

