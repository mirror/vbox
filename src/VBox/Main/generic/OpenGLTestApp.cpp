/* $Id$ */
/** @file
 * VBox host opengl support test application.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#ifdef RT_OS_WINDOWS
#include <Windows.h>
#endif

#ifdef VBOX_WITH_CROGL

extern "C" {
  extern void * crSPULoad(void *, int, char *, char *, void *);
  extern void crSPUUnloadChain(void *);
}


static int vboxCheck3DAccelerationSupported()
{
    void *spu = crSPULoad(NULL, 0, "render", NULL, NULL);
    if (spu)
    {
        crSPUUnloadChain(spu);
        return 0;
    }
    return 1;
}
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
#include <QGLWidget>
#include <QApplication>
#include <VBox/VBoxGL2D.h>

static int vboxCheck2DVideoAccelerationSupported()
{
    static int dummyArgc = 1;
    static char * dummyArgv = "GlTest";
    QApplication app (dummyArgc, &dummyArgv);

    VBoxGLTmpContext ctx;
    const QGLContext *pContext = ctx.makeCurrent();
    if(pContext)
    {
        VBoxVHWAInfo supportInfo;
        supportInfo.init(pContext);
        if(supportInfo.isVHWASupported())
            return 0;
    }
    return 1;
}

#endif

int main(int argc, char **argv)
{
    int rc=0;

    RTR3Init();

    if(argc < 3)
    {
#ifdef VBOX_WITH_CROGL
        /* backwards compatibility: check 3D */
        rc = vboxCheck3DAccelerationSupported();
#endif
    }
    else
    {
        static const RTGETOPTDEF s_aOptionDefs[] =
        {
            { "--test",           't',   RTGETOPT_REQ_STRING },
            { "-test",            't',   RTGETOPT_REQ_STRING },
        };

        RTGETOPTSTATE State;
        int rc = RTGetOptInit(&State, argc-1, argv+1, &s_aOptionDefs[0], RT_ELEMENTS(s_aOptionDefs), 0, 0);
        AssertRCReturn(rc, 49);

        for (;;)
        {
            RTGETOPTUNION Val;
            rc = RTGetOpt(&State, &Val);
            if (!rc)
                break;
            switch (rc)
            {
                case 't':
#ifdef VBOX_WITH_CROGL
                    if (!strcmp(Val.psz, "3D") || !strcmp(Val.psz, "3d"))
                    {
                        rc = vboxCheck3DAccelerationSupported();
                        break;
                    }
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
                    if (!strcmp(Val.psz, "2D") || !strcmp(Val.psz, "2d"))
                    {
                        rc = vboxCheck2DVideoAccelerationSupported();
                        break;
                    }
#endif
                    rc = 1;
                    break;
                case VERR_GETOPT_UNKNOWN_OPTION:
                case VINF_GETOPT_NOT_OPTION:
                    rc = 1;
                default:
                    break;
            }

            if(rc)
                break;
        }
    }

    /*RTR3Term();*/
    return rc;

}

#ifdef RT_OS_WINDOWS
extern "C" int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nShowCmd*/)
{
    return main(__argc, __argv);
}
#endif

