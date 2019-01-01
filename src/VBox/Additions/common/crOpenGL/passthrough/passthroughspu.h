/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef GA_INCLUDED_SRC_common_crOpenGL_passthrough_passthroughspu_h
#define GA_INCLUDED_SRC_common_crOpenGL_passthrough_passthroughspu_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#include "cr_spu.h"


extern SPUNamedFunctionTable _cr_passthrough_table[];
extern void BuildPassthroughTable( SPU *child );


#endif /* !GA_INCLUDED_SRC_common_crOpenGL_passthrough_passthroughspu_h */
