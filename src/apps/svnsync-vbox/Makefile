#
# Makefile
#
#
# Copyright (C) 2006-2007 Oracle Corporation
#
# Oracle Corporation confidential
# All rights reserved
#

CC = gcc
CFLAGS = -O2 -g -Wall -pedantic -Wno-long-long -Wno-trigraphs \
  -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
DEFS = -DVBOX -D_FILE_OFFSET_BITS=64
INCS = -I. -I/usr/include/subversion-1 -I/usr/include/apr-1.0
LIBS = -lsvn_ra-1 -lsvn_ra_neon-1 -lsvn_ra_local-1 \
  -lsvn_repos-1 -lsvn_fs-1 -lsvn_fs_base-1 -lsvn_fs_fs-1 -lsvn_delta-1 \
  -lsvn_subr-1 -lneon -lapr-1 -laprutil-1

all: svnsync-vbox

svnsync-vbox: main.c
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $(LIBS) -o $@ $<

svnsync-vbox: svn_private_config.h

svnsync-vbox: Makefile

