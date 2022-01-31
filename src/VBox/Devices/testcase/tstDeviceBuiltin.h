/** @file
 * tstDevice: Builtin tests.
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

#ifndef VBOX_INCLUDED_SRC_testcase_tstDeviceBuiltin_h
#define VBOX_INCLUDED_SRC_testcase_tstDeviceBuiltin_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/param.h>
#include <VBox/types.h>
#include <iprt/err.h>

#include "tstDevicePlugin.h"

RT_C_DECLS_BEGIN

extern const TSTDEVTESTCASEREG g_TestcaseSsmFuzz;
extern const TSTDEVTESTCASEREG g_TestcaseSsmLoadDbg;
extern const TSTDEVTESTCASEREG g_TestcaseIoFuzz;

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_testcase_tstDeviceBuiltin_h */
