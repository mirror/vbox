/** @file
 * vboxvfs -- VirtualBox Guest Additions for Linux: mount(8) helper
 *
 * Parses options provided by mount (or user directly)
 * Packs them into struct vbsfmount and passes to mount(2)
 * Optionally adds entries to mtab
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* #define DEBUG */
#define DBG if (0)
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>
#include <mntent.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mntent.h>
#include <limits.h>
#include <iconv.h>

#include "vbsfmount.h"

/* Compile-time assertion.  If a == 0, we get two identical switch cases, which is not
   allowed. */
#define CT_ASSERT(a) \
    do { \
        switch(0) { case 0: case (a): ; } \
    } while (0)

struct opts {
    int uid;
    int gid;
    int ttl;
    int ronly;
    int noexec;
    int nodev;
    int nosuid;
    int remount;
    char nls_name[MAX_NLS_NAME];
    char *convertcp;
};

#define PANIC_ATTR __attribute ((noreturn, __format__ (__printf__, 1, 2)))

static void PANIC_ATTR
panic (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    exit (EXIT_FAILURE);
}

static void PANIC_ATTR
panic_err (const char *fmt, ...)
{
    va_list ap;
    int errno_code = errno;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    fprintf (stderr, ": %s\n", strerror (errno_code));
    exit (EXIT_FAILURE);
}

static int
safe_atoi (const char *s, size_t size)
{
    char *endptr;
    long long int val = strtoll (s, &endptr, 10);

    if (val < INT_MIN || val > INT_MAX || endptr < s + size) {
        errno = ERANGE;
        panic_err ("could not convert %.*s to integer, result = %d",
                   size, s, (int) val);
    }
    return (int) val;
}

static void
process_mount_opts (const char *s, struct opts *opts)
{
    const char *next = s;
    size_t len;
    typedef enum handler_opt
    {
        HORW,
        HORO,
        HOUID,
        HOGID,
        HOTTL,
        HOIOCHARSET,
        HOCONVERTCP,
        HONOEXEC,
        HOEXEC,
        HONODEV,
        HODEV,
        HONOSUID,
        HOSUID,
        HOREMOUNT
    } handler_opt;
    struct {
        const char *name;
        handler_opt opt;
        int has_arg;
        const char *desc;
    } handlers[] = {
        {"rw",        HORW,        0, "mount read write (default)"},
        {"ro",        HORO,        0, "mount read only"},
        {"uid",       HOUID,       1, "default file owner user id"},
        {"gid",       HOGID,       1, "default file owner group id"},
        {"ttl",       HOTTL,       1, "time to live for dentry"},
        {"iocharset", HOIOCHARSET, 1, "i/o charset (default utf8)"},
        {"convertcp", HOCONVERTCP, 1, "convert share name from given charset to utf8"},
        {"noexec",    HONOEXEC,    0, 0 }, /* don't document these options directly here */
        {"exec",      HOEXEC,      0, 0 }, /* as they are well known and described in the */
        {"nodev",     HONODEV,     0, 0 }, /* usual manpages */
        {"dev",       HODEV,       0, 0 },
        {"nosuid",    HONOSUID,    0, 0 },
        {"suid",      HOSUID,      0, 0 },
        {"remount",   HOREMOUNT,   0, 0 },
        {NULL, 0, 0, NULL}
    }, *handler;

    while (next) {
        const char *val;
        size_t key_len, val_len;

        s = next;
        next = strchr (s, ',');
        if (!next) {
            len = strlen (s);
        }
        else {
            len = next - s;
            next += 1;
            if (!*next) {
                next = 0;
            }
        }

        val = NULL;
        val_len = 0;
        for (key_len = 0; key_len < len; ++key_len) {
            if (s[key_len] == '=') {
                if (key_len + 1 < len) {
                    val = s + key_len + 1;
                    val_len = len - key_len - 1;
                }
                break;
            }
        }

        for (handler = handlers; handler->name; ++handler) {
            size_t j;
            for (j = 0; j < key_len && handler->name[j] == s[j]; ++j)
                ;

            if (j == key_len && !handler->name[j]) {
                if (handler->has_arg) {
                    if (!(val && *val)) {
                        panic ("%.*s requires an argument (i.e. %.*s=<arg>)\n",
                               len, s, len, s);
                    }
                }

                switch(handler->opt)
                {
                case HORW:
                    opts->ronly = 0;
                    break;
                case HORO:
                    opts->ronly = 1;
                    break;
                case HONOEXEC:
                    opts->noexec = 1;
                    break;
                case HOEXEC:
                    opts->noexec = 0;
                    break;
                case HONODEV:
                    opts->nodev = 1;
                    break;
                case HODEV:
                    opts->nodev = 0;
                    break;
                case HONOSUID:
                    opts->nosuid = 1;
                    break;
                case HOSUID:
                    opts->nosuid = 0;
                    break;
                case HOREMOUNT:
                    opts->remount = 1;
                    break;
                case HOUID:
                    opts->uid = safe_atoi (val, val_len);
                    break;
                case HOGID:
                    opts->gid = safe_atoi (val, val_len);
                    break;
                case HOTTL:
                    opts->ttl = safe_atoi (val, val_len);
                    break;
                case HOIOCHARSET:
                    if (val_len + 1 > sizeof (opts->nls_name)) {
                        panic ("iocharset name too long\n");
                    }
                    memcpy (opts->nls_name, val, val_len);
                    opts->nls_name[val_len] = 0;
                    break;
                case HOCONVERTCP:
                    opts->convertcp = malloc (val_len + 1);
                    if (!opts->convertcp) {
                        panic_err ("could not allocate memory");
                    }
                    memcpy (opts->convertcp, val, val_len);
                    opts->convertcp[val_len] = 0;
                    break;
                }
                break;
            }
            continue;
        }

        if (!handler->name) {
            fprintf (stderr, "unknown mount option `%.*s'\n", len, s);
            fprintf (stderr, "valid options:\n");

            for (handler = handlers; handler->name; ++handler) {
                if (handler->desc)
                    fprintf (stderr, "  %-10s%s %s\n", handler->name,
                         handler->has_arg ? "=<arg>" : "", handler->desc);
            }
            exit (EXIT_FAILURE);
        }
    }
}

static void
complete (char *host_name, char *mount_point,
          unsigned long flags, struct opts *opts)
{
    FILE *f, *m;
    char *buf;
    size_t size;
    struct mntent e;

    m = open_memstream (&buf, &size);
    if (!m)
        panic_err ("could not update mount table (failed to create memstream)");

    if (opts->uid)
        fprintf (m, "uid=%d,", opts->uid);
    if (opts->gid)
        fprintf (m, "gid=%d,", opts->gid);
    if (opts->ttl)
        fprintf (m, "ttl=%d,", opts->ttl);
    if (*opts->nls_name)
        fprintf (m, "iocharset=%s,", opts->nls_name);
    if (flags & MS_NOSUID)
        fprintf (m, "%s,", MNTOPT_NOSUID);
    if (flags & MS_RDONLY)
        fprintf (m, "%s,", MNTOPT_RO);
    else
        fprintf (m, "%s,", MNTOPT_RW);

    fclose (m);

    if (size > 0)
        buf[size - 1] = 0;
    else
        buf = "defaults";

    f = setmntent (MOUNTED, "a+");
    if (!f)
        panic_err ("could not open mount table for update");

    e.mnt_fsname = host_name;
    e.mnt_dir = mount_point;
    e.mnt_type = "vboxsf";
    e.mnt_opts = buf;
    e.mnt_freq = 0;
    e.mnt_passno = 0;

    if (addmntent (f, &e))
    {
        if (size > 0)
        {
            memset (buf, size, 0);
            free (buf);
        }
        panic_err ("could not add an entry to the mount table");
    }

    endmntent (f);

    if (size > 0)
    {
        memset (buf, size, 0);
        free (buf);
    }
}

static void
convertcp (char *in_codeset, char *host_name, struct vbsf_mount_info *info)
{
    char *i = host_name;
    char *o = info->name;
    size_t ib = strlen (host_name);
    size_t ob = sizeof (info->name) - 1;
    iconv_t cd;

    cd = iconv_open ("UTF-8", in_codeset);
    if (cd == (iconv_t) -1) {
        panic_err ("could not convert share name, iconv_open `%s' failed",
                   in_codeset);
    }

    while (ib) {
        size_t c = iconv (cd, &i, &ib, &o, &ob);
        if (c == (size_t) -1) {
            panic_err ("could not convert share name(%s) at %d",
                       host_name, strlen (host_name) - ib);
        }
    }
    *o = 0;
}


/**
  * Print out a usage message and exit.
  *
  * @param name The name of the application
  */
static void __attribute ((noreturn)) usage(char *name)
{
    printf("Usage: %s [OPTIONS] NAME MOUNTPOINT\n"
           "Mount the VirtualBox shared folder NAME from the host system to MOUNTPOINT.\n"
           "\n"
           "  -w                    mount the shared folder writably (the default)\n"
           "  -r                    mount the shared folder read-only\n"
           "  -n                    do not add information about the folder to the mtab file\n"
           "  -o OPTION[,OPTION...] use the mount options specified\n"
           "\n", name);
    printf("Available mount options are:\n"
           "\n"
           "     rw                 mount writably (the default)\n"
           "     ro                 mount read only\n"
           "     uid=UID            set the default file owner user id to UID\n"
           "     gid=GID            set the default file owner group id to GID\n"
           "     ttl=TTL            set the \"time to live\" to TID for the dentry\n"
           "     iocharset CHARSET  use the character set CHARSET for i/o operations (default utf8)\n"
           "     convertcp CHARSET  convert the shared folder name from the character set CHARSET to utf8\n");
    printf("Less common used options:\n"
           "     noexec,exec,nodev,dev,nosuid,suid\n");
    exit(1);
}

int
main (int argc, char **argv)
{
    int c;
    int err;
    int nomtab = 0;
    unsigned long flags = MS_NODEV;
    char *host_name;
    char *mount_point;
    struct vbsf_mount_info mntinf;
    struct opts opts = {
        0,     /* uid */
        0,     /* gid */
        0,     /* ttl */
        0,     /* ronly */
        0,     /* noexec */
        0,     /* nodev */
        0,     /* nosuid */
        0,     /* remount */
        "\0",  /* nls_name */
        NULL,  /* convertcp */
    };

    if (getuid ())
        panic ("Only root can mount shared folders from the host.\n");

    if (!argv[0])
        argv[0] = "mount.vboxsf";

    /* Compile-time assertions */
    CT_ASSERT(sizeof(uid_t) == sizeof(int));
    CT_ASSERT(sizeof(gid_t) == sizeof(int));

    while ((c = getopt (argc, argv, "rwno:h")) != -1) {
        switch (c) {
            default:
                fprintf (stderr, "unknown option `%c:%#x'\n", c, c);
            case '?':
            case 'h':
                usage(argv[0]);

            case 'r':
                opts.ronly = 1;
                break;

            case 'w':
                opts.ronly = 0;

            case 'o':
                process_mount_opts (optarg, &opts);
                break;

            case 'n':
                nomtab = 1;
                break;
        }
    }

    if (argc - optind < 2)
        usage(argv[0]);

    host_name = argv[optind];
    mount_point = argv[optind + 1];

    if (opts.convertcp)
        convertcp (opts.convertcp, host_name, &mntinf);
    else
    {
        if (strlen (host_name) > MAX_HOST_NAME - 1)
            panic ("host name is too big\n");

        strcpy (mntinf.name, host_name);
    }

    if (strlen (opts.nls_name) > MAX_NLS_NAME - 1)
        panic ("%s: the character set name for I/O is too long.\n", argv[0]);

    strcpy (mntinf.nls_name, opts.nls_name);

    if (opts.ronly)
        flags |= MS_RDONLY;
    if (opts.noexec)
        flags |= MS_NOEXEC;
    if (opts.nodev)
        flags |= MS_NODEV;

    mntinf.uid = opts.uid;
    mntinf.gid = opts.gid;
    mntinf.ttl = opts.ttl;

    err = mount (NULL, mount_point, "vboxsf", flags, &mntinf);
    if (err)
        panic_err ("%s: mounting failed with the error", argv[0]);

    if (!nomtab)
        complete (host_name, mount_point, flags, &opts);

    exit (EXIT_SUCCESS);
}
