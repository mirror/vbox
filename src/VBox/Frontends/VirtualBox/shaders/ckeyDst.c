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
uniform sampler2DRect uDstTex;
uniform vec4 uDstClr;
void vboxCKeyDst(void)
{
    vec4 dstClr = texture2DRect(uDstTex, vec2(gl_TexCoord[1]));
    vec3 difClr = dstClr.rgb - uDstClr.rgb;
    if(any(greaterThan(difClr, vec3(0.01, 0.01, 0.01)))
        || any(lessThan(difClr, vec3(-0.01, -0.01, -0.01))))
            discard;
}
