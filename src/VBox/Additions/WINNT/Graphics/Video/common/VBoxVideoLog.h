/* $Id$ */

/** @file
 * VBox Video drivers, logging helper
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXVIDEOLOG_H
#define VBOXVIDEOLOG_H

#ifndef VBOX_VIDEO_LOG_NAME
# error VBOX_VIDEO_LOG_NAME should be defined!
#endif

/* Uncomment to show file/line info in the log */
/*#define VBOX_VIDEO_LOG_SHOWLINEINFO*/

#define VBOX_VIDEO_LOG_PREFIX_FMT VBOX_VIDEO_LOG_NAME"::"LOG_FN_FMT": "
#define VBOX_VIDEO_LOG_PREFIX_PARMS __PRETTY_FUNCTION__

#ifdef VBOX_VIDEO_LOG_SHOWLINEINFO
# define VBOX_VIDEO_LOG_SUFFIX_FMT " (%s:%d)\n"
# define VBOX_VIDEO_LOG_SUFFIX_PARMS ,__FILE__, __LINE__
#else
# define VBOX_VIDEO_LOG_SUFFIX_FMT "\n"
# define VBOX_VIDEO_LOG_SUFFIX_PARMS
#endif

#ifdef DEBUG_misha
# define BP_WARN() AssertFailed()
#else
# define BP_WARN() do {} while(0)
#endif

#define _LOGMSG_EXACT(_logger, _a)                                          \
    do                                                                      \
    {                                                                       \
        _logger(_a);                                                        \
    } while (0)

#define _LOGMSG(_logger, _a)                                                \
    do                                                                      \
    {                                                                       \
        _logger((VBOX_VIDEO_LOG_PREFIX_FMT, VBOX_VIDEO_LOG_PREFIX_PARMS));  \
        _logger(_a);                                                        \
        _logger((VBOX_VIDEO_LOG_SUFFIX_FMT  VBOX_VIDEO_LOG_SUFFIX_PARMS));  \
    } while (0)

#define WARN_NOBP(_a)                                                          \
    do                                                                            \
    {                                                                             \
        Log((VBOX_VIDEO_LOG_PREFIX_FMT"WARNING! ", VBOX_VIDEO_LOG_PREFIX_PARMS)); \
        Log(_a);                                                                  \
        Log((VBOX_VIDEO_LOG_SUFFIX_FMT VBOX_VIDEO_LOG_SUFFIX_PARMS));             \
    } while (0)

#define WARN(_a)                                                                  \
    do                                                                            \
    {                                                                             \
        WARN_NOBP(_a);                                                         \
        BP_WARN();                                                             \
    } while (0)

#define LOG(_a) _LOGMSG(Log, _a)
#define LOGREL(_a) _LOGMSG(LogRel, _a)
#define LOGF(_a) _LOGMSG(LogFlow, _a)
#define LOGF_ENTER() LOGF(("ENTER"))
#define LOGF_LEAVE() LOGF(("LEAVE"))
#define LOGREL_EXACT(_a) _LOGMSG_EXACT(Log, _a)

#endif /*VBOXVIDEOLOG_H*/
