/* $Id$ */
/** @file
 * VirtualBox Main - VM process launcher helper for VBoxSVC & VBoxSDS.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_MachineLaunchVMCommonWorker_h
#define MAIN_INCLUDED_MachineLaunchVMCommonWorker_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <vector>
#include "VirtualBoxBase.h"

int MachineLaunchVMCommonWorker(const Utf8Str &aNameOrId,
                                const Utf8Str &aComment,
                                const Utf8Str &aFrontend,
                                const std::vector<com::Utf8Str> &aEnvironmentChanges,
                                const Utf8Str &aExtraArg,
                                const Utf8Str &aFilename,
                                uint32_t      aFlags,
                                void         *aExtraData,
                                RTPROCESS     &aPid);

#endif /* !MAIN_INCLUDED_MachineLaunchVMCommonWorker_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

