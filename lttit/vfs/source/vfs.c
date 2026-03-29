#include "vfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct vfs g_vfs;

static const char *normalize_path(const char *path)
{
    while (*path == '/') path++;
    return path;
}

struct vnode *vnode_create(void)
{
    struct vnode *n = malloc(sizeof(struct vnode));
    if (!n) return 0;
    n->ops = 0;
    n->owner_name = 0;
    n->remote_path = 0;
    return n;
}

void vnode_set_ops(struct vnode *n, struct vfs_ops *ops)
{
    n->ops = ops;
}

void vnode_set_remote(struct vnode *n, const char *owner_name, const char *remote_path)
{
    if (n->owner_name) free(n->owner_name);
    if (n->remote_path) free(n->remote_path);
    n->owner_name = owner_name ? strdup(owner_name) : 0;
    n->remote_path = remote_path ? strdup(remote_path) : 0;
}

void vfs_init(const char *local_name)
{
    prefix_map_init(&g_vfs.tree);
    g_vfs.local_name = local_name ? strdup(local_name) : 0;
}

struct vnode *vfs_walk(const char *path)
{
    const char *p = normalize_path(path);
    return prefix_map_get(&g_vfs.tree, p);
}

struct vnode *vfs_mkdirs(const char *path)
{
    const char *p = normalize_path(path);
    struct vnode *n = prefix_map_get(&g_vfs.tree, p);
    if (n) return n;
    n = vnode_create();
    if (!n) return 0;
    if (prefix_map_set(&g_vfs.tree, p, n) < 0) {
        free(n);
        return 0;
    }
    return n;
}

static int dump_cb(const char *key, void *value, void *arg)
{
    struct vfs_dump_ctx *c = arg;
    if (c->pos >= c->len)
        return -1;
    int left = c->len - c->pos;
    int n = snprintf(c->buf + c->pos, left, "%s\n", key);
    if (n <= 0 || n >= left)
        return -1;
    c->pos += n;
    return 0;
}

int vfs_dump(struct vfs_dump_ctx *ctx)
{
    prefix_map_iter(&g_vfs.tree, dump_cb, ctx);
    return ctx->pos;
}
