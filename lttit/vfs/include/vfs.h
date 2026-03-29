#ifndef LTTIT_VFS_H
#define LTTIT_VFS_H

#include "prefix.h"

struct vfs_ops {
    int (*open)(void *self, const char *path, int flags);
    int (*read)(void *self, int fd, void *buf, int len);
    int (*write)(void *self, int fd, const void *buf, int len);
    int (*ctl)(void *self, int fd, int cmd, void *arg);
    int (*close)(void *self, int fd);
};

struct vnode {
    struct vfs_ops *ops;
    char *owner_name;
    char *remote_path;
};

struct vfs {
    struct prefix_map tree;
    char *local_name;
};

struct vfs_dump_ctx {
    char *buf;
    int len;
    int pos;
};

extern struct vfs g_vfs;

struct vnode *vnode_create(void);
void vnode_set_ops(struct vnode *n, struct vfs_ops *ops);
void vnode_set_remote(struct vnode *n, const char *owner_name, const char *remote_path);

void vfs_init(const char *local_name);

struct vnode *vfs_walk(const char *path);
struct vnode *vfs_mkdirs(const char *path);

int vfs_dump(struct vfs_dump_ctx *ctx);

#endif
