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

#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
void vboxCConvApplyAYUV(vec4 color);
void vboxCConv()
{
    vec2 srcCoord = vec2(gl_TexCoord[0]);
    float x = srcCoord.x;
    int pix = int(x);
    vec4 srcClr = texture2DRect(uSrcTex, vec2(float(pix), srcCoord.y));
    float u = srcClr.g;
    float v = srcClr.a;
    float part = x - float(pix);
    float y;
    if(part < 0.5)
    {
        y = srcClr.b;
    }
    else
    {
        y = srcClr.r;
    }
    vboxCConvApplyAYUV(vec4(u, y, 0.0, v));
}
