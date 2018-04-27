/* $Id$ */
/** @file
 * VBox Qt GUI - 2D Video Acceleration helpers implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#if defined(VBOX_GUI_USE_QGL) || defined(VBOX_WITH_VIDEOHWACCEL)

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

# define LOG_GROUP LOG_GROUP_GUI

// WORKAROUND:
// QGLWidget drags in Windows.h and stdint.h
# ifdef RT_OS_WINDOWS
#  include <iprt/win/windows.h>
#  include <iprt/stdint.h>
# endif

/* Qt includes: */
# include <QGLWidget>

/* GUI includes: */
#include "VBox2DHelpers.h"

/* Other VBox includes: */
# include <VBox/VBoxGL2D.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static bool g_fVBoxVHWAChecked = false;
static bool g_fVBoxVHWASupported = false;


/*********************************************************************************************************************************
*   Namespace VBox2DHelpers implementation.                                                                                      *
*********************************************************************************************************************************/

bool VBox2DHelpers::isAcceleration2DVideoAvailable()
{
    if (!g_fVBoxVHWAChecked)
    {
        g_fVBoxVHWAChecked = true;
        g_fVBoxVHWASupported = VBoxVHWAInfo::checkVHWASupport();
    }
    return g_fVBoxVHWASupported;
}

quint64 VBox2DHelpers::required2DOffscreenVideoMemory()
{
    /* HDTV == 1920x1080 ~ 2M
     * for the 4:2:2 formats each pixel is 2Bytes
     * so each frame may be 4MiB
     * so for triple-buffering we would need 12 MiB */
    return _1M * 12;
}

#endif /* VBOX_GUI_USE_QGL || VBOX_WITH_VIDEOHWACCEL */
