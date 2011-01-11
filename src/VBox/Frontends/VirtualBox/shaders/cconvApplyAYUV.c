/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * Shader programming for VBoxFBOverlay.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

void vboxCConvApplyAYUV(vec4 color)
{
    float y, u, v, r, g, b;
    y = color.g;
    u = color.r;
    v = color.a;
    u = u - 0.5;
    v = v - 0.5;
    y = 1.164*(y-0.0625);
    b = y + 2.018*u;
    g = y - 0.813*v - 0.391*u;
    r = y + 1.596*v;
    gl_FragColor = vec4(r,g,b,1.0);
}
