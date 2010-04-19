/* $Id$ */
/** @file
 * NAT - helpers (declarations/defines).
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

/*
 * This code is based on:
 *
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#ifndef _MISC_H_
#define _MISC_H_


extern int x_port, x_server, x_display;

int show_x (char *, struct socket *);
void redir_x (u_int32_t, int, int, int);
void getouraddr (PNATState);
void slirp_insque  (PNATState, void *, void *);
void slirp_remque  (PNATState, void *);
int slirp_openpty (int *, int *);
int fork_exec (PNATState, struct socket *, char *, int);
void snooze_hup (int);
void snooze (void);
void relay (int);
void add_emu (char *);
void fd_nonblock (int);
void fd_block (int);

#ifdef VBOX_WITH_SLIRP_BSD_MBUF
/* UVM interface */
#define UMA_ALIGN_PTR       (1 << 0)
#define UMA_ZONE_REFCNT     (1 << 1)
#define UMA_ZONE_MAXBUCKET  (1 << 2)
#define UMA_ZONE_ZINIT      (1 << 3)
#define UMA_SLAB_KERNEL     (1 << 4)
#define UMA_ZFLAG_FULL      (1 << 5)

struct uma_zone;
typedef struct uma_zone *uma_zone_t;
typedef void *(*uma_alloc_t)(uma_zone_t, int, u_int8_t *, int);
typedef void (*uma_free_t)(void *, int, u_int8_t);

typedef int (*ctor_t)(PNATState, void *, int, void *, int);
typedef void (*dtor_t)(PNATState, void *, int, void *);
typedef int (*zinit_t)(PNATState, void *, int, int);
typedef void (*zfini_t)(PNATState, void *, int);
uma_zone_t uma_zcreate(PNATState, char *, size_t, ctor_t, dtor_t, zinit_t, zfini_t, int, int);
uma_zone_t uma_zsecond_create(char *, ctor_t, dtor_t, zinit_t, zfini_t, uma_zone_t);
void uma_zone_set_max(uma_zone_t, int);
void uma_zone_set_allocf(uma_zone_t, uma_alloc_t);
void uma_zone_set_freef(uma_zone_t, uma_free_t);

uint32_t *uma_find_refcnt(uma_zone_t, void *);
void *uma_zalloc(uma_zone_t, int);
void *uma_zalloc_arg(uma_zone_t, void *, int);
void uma_zfree(uma_zone_t, void *);
void uma_zfree_arg(uma_zone_t, void *, void *);
int uma_zone_exhausted_nolock(uma_zone_t);
void zone_drain(uma_zone_t);

void slirp_null_arg_free(void *, void *);
void m_fini(PNATState pData);
#endif /* VBOX_WITH_SLIRP_BSD_MBUF */

#endif
