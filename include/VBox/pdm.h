/** @file
 * PDM - Pluggable Device Manager.
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

