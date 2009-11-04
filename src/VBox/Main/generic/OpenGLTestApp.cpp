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

#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#ifdef RT_OS_WINDOWS
# include <Windows.h>
#endif
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
# include <sys/resource.h>
# include <fcntl.h>
# include <unistd.h>
#endif

#include <string.h>

#ifdef VBOX_WITH_CROGL

extern "C"
{
  extern void * crSPULoad(void *, int, char *, char *, void *);
  extern void crSPUUnloadChain(void *);
}


static int vboxCheck3DAccelerationSupported()
{
    void *spu = crSPULoad(NULL, 0, (char*)"render", NULL, NULL);
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
    static char * dummyArgv = (char*)"GlTest";
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
    int rc = 0;

    RTR3Init();

#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
    /* This small test application might crash on some hosts. Do never
     * generate a core dump as most likely some OpenGL library is
     * responsible. */
    struct rlimit lim = { 0, 0 };
    setrlimit(RLIMIT_CORE, &lim);

    /* Redirect stderr to /dev/null */
    int fd = open("/dev/null", O_WRONLY);
    if (fd != -1)
        dup2(fd, STDERR_FILENO);
#endif

    if(argc < 2)
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
            { "--help",           'h',   RTGETOPT_REQ_NOTHING },
        };

        RTGETOPTSTATE State;
        rc = RTGetOptInit(&State, argc-1, argv+1, &s_aOptionDefs[0], RT_ELEMENTS(s_aOptionDefs), 0, 0);
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

                case 'h':
                    RTPrintf("VirtualBox Helper for testing 2D/3D OpenGL capabilities %u.%u.%u\n"
                             "(C) 2009 Sun Microsystems, Inc.\n"
                             "All rights reserved.\n"
                             "\n"
                             "Parameters:\n"
                             "  --test 2D      test for 2D (video) OpenGL capabilities\n"
                             "  --test 3D      test for 3D OpenGL capabilities\n"
                             "\n",
                            RTBldCfgVersionMajor(), RTBldCfgVersionMinor(), RTBldCfgVersionBuild());
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

