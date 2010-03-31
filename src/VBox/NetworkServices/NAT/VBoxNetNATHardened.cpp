/* $Id$ */
/** @file
 * VBoxNetNAT - Hardened main().
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <VBox/sup.h>


int main(int argc, char **argv, char **envp)
{
    return SUPR3HardenedMain("VBoxNetNAT", 0 /* fFlags */, argc, argv, envp);
}
