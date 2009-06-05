/* $Id$ */

/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <stdio.h>
#include <iprt/initterm.h>
#ifdef RT_OS_WINDOWS
#include <windows.h>
#endif

extern "C" {
  extern void * crSPULoad(void *, int, char *, char *, void *);
  extern void crSPUUnloadChain(void *);
}

#ifndef RT_OS_WINDOWS
int main (int argc, char **argv)
#else
extern "C" int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nShowCmd*/)
#endif
{
    void *spu;
    int rc=1;

    RTR3Init();

    spu = crSPULoad(NULL, 0, "render", NULL, NULL);
    if (spu)
    {
        crSPUUnloadChain(spu);
        rc=0;
    }

    /*RTR3Term();*/
    return rc;
}
