/** @file
 * PDM - Pluggable Device Manager.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_pdm_h
#define ___VBox_pdm_h

#include <VBox/pdmapi.h>
#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmthread.h>
#include <VBox/pdmifs.h>
#include <VBox/pdmdrv.h>
#include <VBox/pdmdev.h>
#include <VBox/pdmusb.h>
#include <VBox/pdmsrv.h>

/** Source position.
 * @deprecated Use RT_SRC_POS */
#define PDM_SRC_POS         RT_SRC_POS

/** Source position declaration.
 * @deprecated Use RT_SRC_POS_DECL */
#define PDM_SRC_POS_DECL    RT_SRC_POS_DECL

/** Source position arguments.
 * @deprecated Use RT_SRC_POS_ARGS */
#define PDM_SRC_POS_ARGS    RT_SRC_POS_ARGS

#endif

