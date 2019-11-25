
#ifndef __G_VFS_BACKEND_VBOXSC_H__
#define __G_VFS_BACKEND_VBOXSC_H__

#include "gvfsbackend.h"
#include "gmountspec.h"

G_BEGIN_DECLS

#define G_VFS_TYPE_BACKEND_VBOXSC         (g_vfs_backend_vboxsc_get_type ())
#define G_VFS_BACKEND_VBOXSC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_VFS_TYPE_BACKEND_VBOXSC, GVfsBackendVBoxSC))
#define G_VFS_BACKEND_VBOXSC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_VFS_TYPE_BACKEND_VBOXSC, GVfsBackendVBoxSCClass))
#define G_VFS_IS_BACKEND_VBOXSC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_VFS_TYPE_BACKEND_VBOXSC))
#define G_VFS_IS_BACKEND_VBOXSC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_VFS_TYPE_BACKEND_VBOXSC))
#define G_VFS_BACKEND_VBOXSC_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_VFS_TYPE_BACKEND_VBOXSC, GVfsBackendVBoxSCClass))

typedef struct _GVfsBackendVBoxSC        GVfsBackendVBoxSC;
typedef struct _GVfsBackendVBoxSCClass   GVfsBackendVBoxSCClass;

typedef enum
{
    GVFS_JOB_UNMOUNT = 1 << 0,
    GVFS_JOB_MOUNT = 1 << 1,
    GVFS_JOB_OPEN_FOR_READ = 1 << 2,
    GVFS_JOB_CLOSE_READ = 1 << 3,
    GVFS_JOB_READ = 1 << 4,
    GVFS_JOB_SEEK_ON_READ = 1 << 5,
    GVFS_JOB_CREATE = 1 << 6,
    GVFS_JOB_APPEND_TO = 1 << 7,
    GVFS_JOB_REPLACE = 1 << 8,
    GVFS_JOB_CLOSE_WRITE = 1 << 9,
    GVFS_JOB_WRITE = 1 << 10,
    GVFS_JOB_SEEK_ON_WRITE = 1 << 11,
    GVFS_JOB_QUERY_INFO = 1 << 12,
    GVFS_JOB_QUERY_FS_INFO = 1 << 13,
    GVFS_JOB_ENUMERATE = 1 << 14,
    GVFS_JOB_SET_DISPLAY_NAME = 1 << 15,
    GVFS_JOB_DELETE = 1 << 16,
    GVFS_JOB_TRASH = 1 << 17,
    GVFS_JOB_MAKE_DIRECTORY = 1 << 18,
    GVFS_JOB_MAKE_SYMLINK = 1 << 19,
    GVFS_JOB_COPY = 1 << 20,
    GVFS_JOB_MOVE = 1 << 21,
    GVFS_JOB_SET_ATTRIBUTE = 1 << 22,
    GVFS_JOB_CREATE_DIR_MONITOR = 1 << 23,
    GVFS_JOB_CREATE_FILE_MONITOR = 1 << 24,
    GVFS_JOB_QUERY_SETTABLE_ATTRIBUTES = 1 << 25,
    GVFS_JOB_QUERY_WRITABLE_NAMESPACES = 1 << 26,
    GVFS_JOB_TRUNCATE = 1 << 27,
} GVfsJobType;



struct _GVfsBackendVBoxSC
{
    GVfsBackend parent_instance;
    GMountSpec *mount_spec;
};

struct _GVfsBackendVBoxSCClass
{
    GVfsBackendClass parent_class;
};

GType g_vfs_backend_vboxsc_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __G_VFS_BACKEND_VBOXSC_H__ */

