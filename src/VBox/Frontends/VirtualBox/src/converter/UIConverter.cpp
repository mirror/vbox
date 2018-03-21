/* $Id$ */
/** @file
 * VBox Qt GUI - UIConverter implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else
/* GUI includes: */
# include "UIConverter.h"
#endif


/* static */
UIConverter* UIConverter::s_pInstance = 0;

/* static */
void UIConverter::prepare()
{
    /* Make sure instance WAS NOT created yet: */
    if (s_pInstance)
        return;

    /* Prepare instance: */
    s_pInstance = new UIConverter;
}

/* static */
void UIConverter::cleanup()
{
    /* Make sure instance WAS NOT destroyed yet: */
    if (!s_pInstance)
        return;

    /* Cleanup instance: */
    delete s_pInstance;
    s_pInstance = 0;
}

