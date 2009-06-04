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

extern "C" {
  extern void * crSPULoad(void *, int, char *, char *, void *);
  extern void crSPUUnloadChain(void *);
}

int main (int argc, char **argv)
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
