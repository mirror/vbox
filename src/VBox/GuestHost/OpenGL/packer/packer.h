/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_PACKER_H
#define CR_PACKER_H

#ifdef DLLDATA
#undef DLLDATA
#endif
#define DLLDATA(type) DECLEXPORT(type)

#include <stdio.h>  /* for sprintf() */
#include "cr_pack.h"
#include "cr_packfunctions.h"
#include "packer_extensions.h"
#include "cr_mem.h"

extern void __PackError( int line, const char *file, GLenum error, const char *info );

#endif /* CR_PACKER_H */
