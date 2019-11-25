/** $Id$ */
/** @file
 * Guest Additions - X11 Shared Clipboard - Custom GVFS backend which registers
 * an own protocol handler to provide handling of clipboard file / directoriy entries.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <iprt/cdefs.h>

#ifdef N_
# undef N_ /* Gvfs also has this defined, so has IPRT. */
#endif

#ifdef _
# undef _ /* Gvfs also has this defined, so has IPRT. */
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "VBoxShClDaemon.h"

#include "gvfsjobopenforread.h"
#include "gvfsjobread.h"
#include "gvfsjobseekread.h"
#include "gvfsjobqueryinfo.h"
#include "gvfsjobenumerate.h"
#include "gvfsjobwrite.h"
#include "gvfsjobopenforwrite.h"
#include "gvfsjobclosewrite.h"
#include "gvfsjobqueryinfowrite.h"
#include "gvfsdaemonutils.h"


G_DEFINE_TYPE(GVfsBackendVBoxSC, g_vfs_backend_vboxsc, G_VFS_TYPE_BACKEND)


static void
g_vfs_backend_vboxsc_finalize(GObject *object)
{
    if (G_OBJECT_CLASS(g_vfs_backend_vboxsc_parent_class)->finalize)
        (*G_OBJECT_CLASS(g_vfs_backend_vboxsc_parent_class)->finalize)(object);
}

static void
g_vfs_backend_vboxsc_init(GVfsBackendVBoxSC *backend_vboxsc)
{
    GVfsBackend *backend = G_VFS_BACKEND(backend_vboxsc);
    GMountSpec *mount_spec;

    g_vfs_backend_set_user_visible(backend, FALSE);

    mount_spec = g_mount_spec_new("vboxsc");
    g_vfs_backend_set_mount_spec(backend, mount_spec);
    g_mount_spec_unref(mount_spec);

    g_vfs_backend_set_icon_name(backend, "folder");
    g_vfs_backend_set_symbolic_icon_name(backend, "folder-symbolic");
}


static gboolean
try_mount(GVfsBackend *backend,
          GVfsJobMount *job,
          GMountSpec *mount_spec,
          GMountSource *mount_source,
          gboolean is_automount)
{
    RT_NOREF(backend, mount_spec, mount_source, is_automount);
    g_vfs_job_succeeded(G_VFS_JOB(job));
    return TRUE;
}

static gboolean
open_idle_cb(gpointer data)
{
    GVfsJobOpenForRead *job = data;
    int fd;

    g_print("open_idle_cb (%s)\n", job->filename);

    if (g_vfs_job_is_cancelled(G_VFS_JOB(job)))
    {
        g_vfs_job_failed(G_VFS_JOB(job), G_IO_ERROR,
                         G_IO_ERROR_CANCELLED,
                         _("Operation was cancelled"));
        return FALSE;
    }

    fd = g_open(job->filename, O_RDONLY);
    if (fd == -1)
    {
        int errsv = errno;

        g_vfs_job_failed(G_VFS_JOB(job), G_IO_ERROR,
                         g_io_error_from_errno(errsv),
                         "Error opening file %s: %s",
                         job->filename, g_strerror(errsv));
    }
    else
    {
        g_vfs_job_open_for_read_set_can_seek(job, TRUE);
        g_vfs_job_open_for_read_set_handle(job, GINT_TO_POINTER(fd));
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
    return FALSE;
}

static void
open_read_cancelled_cb(GVfsJob *job, gpointer data)
{
    guint tag = GPOINTER_TO_INT(data);

    g_print("open_read_cancelled_cb\n");

    if (g_source_remove(tag))
        g_vfs_job_failed(job, G_IO_ERROR,
                         G_IO_ERROR_CANCELLED,
                         _("Operation was cancelled"));
}

static gboolean
try_open_for_read(GVfsBackend *backend,
                  GVfsJobOpenForRead *job,
                  const char *filename)
{
    RT_NOREF(backend);

    g_print("try_open_for_read (%s)\n", filename);

    guint tag = g_timeout_add(0, open_idle_cb, job);
    g_signal_connect(job, "cancelled", (GCallback)open_read_cancelled_cb, GINT_TO_POINTER(tag));

    return TRUE;
}

static gboolean
read_idle_cb(gpointer data)
{
    RT_NOREF(data);

    GVfsJobRead *job = data;
    int fd;
    ssize_t res;

    fd = GPOINTER_TO_INT(job->handle);

    res = read(fd, job->buffer, job->bytes_requested);

    if (res == -1)
    {
        int errsv = errno;

        g_vfs_job_failed(G_VFS_JOB(job), G_IO_ERROR,
                         g_io_error_from_errno(errsv),
                         "Error reading from file: %s",
                         g_strerror(errsv));
    }
    else
    {
        g_vfs_job_read_set_size(job, res);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }

    return FALSE;
}

static void
read_cancelled_cb(GVfsJob *job, gpointer data)
{
    RT_NOREF(data);

    guint tag = GPOINTER_TO_INT(job->backend_data);

    g_source_remove(tag);
    g_vfs_job_failed(job, G_IO_ERROR,
                     G_IO_ERROR_CANCELLED,
                     _("Operation was cancelled"));
}

static gboolean
try_read(GVfsBackend *backend,
         GVfsJobRead *job,
         GVfsBackendHandle handle,
         char *buffer,
         gsize bytes_requested)
{
    RT_NOREF(backend, handle, buffer);

    guint tag;

    g_print( "read (%" G_GSIZE_FORMAT " )\n", bytes_requested);

    tag = g_timeout_add(5000, read_idle_cb, job);
    G_VFS_JOB(job)->backend_data = GINT_TO_POINTER(tag);
    g_signal_connect(job, "cancelled", (GCallback)read_cancelled_cb, NULL);

    return TRUE;
}

static void
do_seek_on_read(GVfsBackend *backend,
                GVfsJobSeekRead *job,
                GVfsBackendHandle handle,
                goffset    offset,
                GSeekType  type)
{
    RT_NOREF(backend);

    int whence;
    int fd;
    off_t final_offset;

    g_print("seek_on_read (%d, %u)\n", (int)offset, type);

    if ((whence = gvfs_seek_type_to_lseek(type)) == -1)
        whence = SEEK_SET;

    fd = GPOINTER_TO_INT(handle);

    final_offset = lseek(fd, offset, whence);

    if (final_offset == (off_t)-1)
    {
        int errsv = errno;

        g_vfs_job_failed(G_VFS_JOB(job), G_IO_ERROR,
                         g_io_error_from_errno(errsv),
                         "Error seeking in file: %s",
                         g_strerror(errsv));
    }
    else
    {
        g_vfs_job_seek_read_set_offset(job, offset);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
}

static void
do_query_info_on_read(GVfsBackend *backend,
                      GVfsJobQueryInfoRead *job,
                      GVfsBackendHandle handle,
                      GFileInfo *info,
                      GFileAttributeMatcher *attribute_matcher)
{
    RT_NOREF(backend, attribute_matcher);

    int fd, res;
    struct stat statbuf;

    fd = GPOINTER_TO_INT(handle);

    res = fstat(fd, &statbuf);

    if (res == -1)
    {
        int errsv = errno;

        g_vfs_job_failed(G_VFS_JOB(job), G_IO_ERROR,
                         g_io_error_from_errno(errsv),
                         "Error querying info in file: %s",
                         g_strerror(errsv));
    }
    else
    {
        g_file_info_set_size(info, statbuf.st_size);
        g_file_info_set_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_DEVICE,
                                         statbuf.st_dev);
        g_file_info_set_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED, statbuf.st_mtime);
        g_file_info_set_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_ACCESS, statbuf.st_atime);
        g_file_info_set_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_CHANGED, statbuf.st_ctime);

        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
}

static void
do_close_read(GVfsBackend *backend,
              GVfsJobCloseRead *job,
              GVfsBackendHandle handle)
{
    RT_NOREF(backend);

    int fd;

    g_print("close ()\n");

    fd = GPOINTER_TO_INT(handle);
    close(fd);

    g_vfs_job_succeeded(G_VFS_JOB(job));
}

static void
do_query_info(GVfsBackend *backend,
              GVfsJobQueryInfo *job,
              const char *filename,
              GFileQueryInfoFlags flags,
              GFileInfo *info,
              GFileAttributeMatcher *matcher)
{
    RT_NOREF(backend, matcher);

    GFile *file;
    GFileInfo *info2;
    GError *error;
    GVfs *local_vfs;

    g_print("do_get_file_info (%s)\n", filename);

    local_vfs = g_vfs_get_local();
    file = g_vfs_get_file_for_path(local_vfs, "/tmp/vboxsc-1000/file1.txt");

    error = NULL;
    info2 = g_file_query_info(file, job->attributes, flags,
                              NULL, &error);

    if (info2)
    {
        g_file_info_copy_into(info2, info);
        g_object_unref(info2);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
    else
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);

    g_object_unref(file);
}

static void
do_create(GVfsBackend *backend,
          GVfsJobOpenForWrite *job,
          const char *filename,
          GFileCreateFlags flags)
{
    RT_NOREF(backend);

    GFile *file;
    GFileOutputStream *out;
    GError *error;

    file = g_vfs_get_file_for_path(g_vfs_get_local(),
                                   filename);

    error = NULL;
    out = g_file_create(file, flags, G_VFS_JOB(job)->cancellable, &error);
    g_object_unref(file);
    if (out)
    {
        g_vfs_job_open_for_write_set_can_seek(job, FALSE);
        g_vfs_job_open_for_write_set_handle(job, out);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
    else
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
}

static void
do_append_to(GVfsBackend *backend,
             GVfsJobOpenForWrite *job,
             const char *filename,
             GFileCreateFlags flags)
{
    RT_NOREF(backend);

    GFile *file;
    GFileOutputStream *out;
    GError *error;

    file = g_vfs_get_file_for_path(g_vfs_get_local(),
                                   filename);

    error = NULL;
    out = g_file_append_to(file, flags, G_VFS_JOB(job)->cancellable, &error);
    g_object_unref(file);
    if (out)
    {
        g_vfs_job_open_for_write_set_can_seek(job, FALSE);
        g_vfs_job_open_for_write_set_handle(job, out);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
    else
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
}

static void
do_replace(GVfsBackend *backend,
           GVfsJobOpenForWrite *job,
           const char *filename,
           const char *etag,
           gboolean make_backup,
           GFileCreateFlags flags)
{
    RT_NOREF(backend);

    GFile *file;
    GFileOutputStream *out;
    GError *error;

    file = g_vfs_get_file_for_path(g_vfs_get_local(),
                                   filename);

    error = NULL;
    out = g_file_replace(file,
                         etag, make_backup,
                         flags, G_VFS_JOB(job)->cancellable,
                         &error);
    g_object_unref(file);
    if (out)
    {
        g_vfs_job_open_for_write_set_can_seek(job, FALSE);
        g_vfs_job_open_for_write_set_handle(job, out);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
    else
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
}

static void
do_close_write(GVfsBackend *backend,
               GVfsJobCloseWrite *job,
               GVfsBackendHandle handle)
{
    RT_NOREF(backend);

    GFileOutputStream *out;
    GError *error;
    char *etag;

    out = (GFileOutputStream *)handle;

    error = NULL;
    if (!g_output_stream_close(G_OUTPUT_STREAM(out),
                               G_VFS_JOB(job)->cancellable,
                               &error))
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
    else
    {
        etag = g_file_output_stream_get_etag(out);

        if (etag)
        {
            g_vfs_job_close_write_set_etag(job, etag);
            g_free(etag);
        }

        g_vfs_job_succeeded(G_VFS_JOB(job));
    }

    g_object_unref(out);
}

static void
do_write(GVfsBackend *backend,
         GVfsJobWrite *job,
         GVfsBackendHandle handle,
         char *buffer,
         gsize buffer_size)
{
    RT_NOREF(backend);

    GFileOutputStream *out;
    GError *error;
    gssize res;

    g_print("do_write\n");

    out = (GFileOutputStream *)handle;

    error = NULL;
    res = g_output_stream_write(G_OUTPUT_STREAM(out),
                                buffer, buffer_size,
                                G_VFS_JOB(job)->cancellable,
                                &error);
    if (res < 0)
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
    else
    {
        g_vfs_job_write_set_written_size(job, res);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
}

static void
do_query_info_on_write(GVfsBackend *backend,
                       GVfsJobQueryInfoWrite *job,
                       GVfsBackendHandle handle,
                       GFileInfo *info,
                       GFileAttributeMatcher *attribute_matcher)
{
    RT_NOREF(backend, attribute_matcher);

    GFileOutputStream *out;
    GError *error;
    GFileInfo *info2;

    g_print("do_query_info_on_write\n");

    out = (GFileOutputStream *)handle;

    error = NULL;
    info2 = g_file_output_stream_query_info(out, job->attributes,
                                            G_VFS_JOB(job)->cancellable,
                                            &error);
    if (info2 == NULL)
    {
        g_vfs_job_failed_from_error(G_VFS_JOB(job), error);
        g_error_free(error);
    }
    else
    {
        g_file_info_copy_into(info2, info);
        g_object_unref(info2);
        g_vfs_job_succeeded(G_VFS_JOB(job));
    }
}

static gboolean
try_enumerate(GVfsBackend *backend,
              GVfsJobEnumerate *job,
              const char *filename,
              GFileAttributeMatcher *matcher,
              GFileQueryInfoFlags flags)
{
    RT_NOREF(backend, matcher, flags);

    GFileInfo * info1,*info2;
    GList *l;

    g_print("try_enumerate (%s)\n", filename);

    g_vfs_job_succeeded(G_VFS_JOB(job));

    info1 = g_file_info_new();
    info2 = g_file_info_new();
    g_file_info_set_name(info1, "/tmp/vboxsc-1000/file1.txt");
    g_file_info_set_size(info1, 0);
    //g_file_info_set_file_type (info1, G_FILE_TYPE_REGULAR);
    g_file_info_set_attribute_boolean(info1, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL, TRUE);

    g_file_info_set_name(info2, "file2.doc");
    g_file_info_set_file_type(info2, G_FILE_TYPE_REGULAR);

    l = NULL;
    l = g_list_append(l, info1);
    l = g_list_append(l, info2);

    g_vfs_job_enumerate_add_infos(job, l);

    g_list_free(l);
    g_object_unref(info1);
    g_object_unref(info2);

    g_vfs_job_enumerate_done(job);

    return TRUE;
}

static void g_vfs_backend_vboxsc_class_init(GVfsBackendVBoxSCClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GVfsBackendClass *backend_class = G_VFS_BACKEND_CLASS(klass);

    gobject_class->finalize = g_vfs_backend_vboxsc_finalize;

    backend_class->try_mount = try_mount;
    backend_class->try_open_for_read = try_open_for_read;
    backend_class->try_read = try_read;
    backend_class->seek_on_read = do_seek_on_read;
    backend_class->query_info_on_read = do_query_info_on_read;
    backend_class->close_read = do_close_read;

    backend_class->replace = do_replace;
    backend_class->create = do_create;
    backend_class->append_to = do_append_to;
    backend_class->write = do_write;
    backend_class->query_info_on_write = do_query_info_on_write;
    backend_class->close_write = do_close_write;

    backend_class->query_info = do_query_info;
    backend_class->try_enumerate = try_enumerate;
}

