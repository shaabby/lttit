#include "cluster.h"
#include <string.h>
#include <stdlib.h>

static struct hashmap name_to_transport;

static int open_root(void *self, const char *path, int flags)
{
    return 0;
}

static int write_root(void *self, int fd, const void *buf, int len)
{
    const char *p = buf;
    const char *end = p + len;
    char line[256];

    while (p < end) {
        const char *nl = memchr(p, end - p, '\n');
        int l = nl ? (nl - p) : (end - p);

        if (l > 0 && l < (int)sizeof(line)) {
            memcpy(line, p, l);
            line[l] = 0;

            const char *prefix = line;
            const char *slash = strchr(prefix, '/');
            if (!slash) {
                p += l + 1;
                continue;
            }

            int plen = slash - prefix;
            char name[64];
            if (plen >= (int)sizeof(name)) plen = sizeof(name) - 1;
            memcpy(name, prefix, plen);
            name[plen] = 0;

            const char *rest = slash + 1;

            struct vnode *n = vfs_mkdirs(line);
            vnode_set_remote(n, name, rest);
        }

        p += l + 1;
    }

    return len;
}

static int read_root(void *self, int fd, void *buf, int len)
{
    struct vfs_dump_ctx ctx = {
        .buf = buf,
        .len = len,
        .pos = 0,
    };

    return vfs_dump(&ctx);
}

static int close_root(void *self, int fd)
{
    return 0;
}

static struct vfs_ops ops = {
    open_root,
    read_root,
    write_root,
    0,
    close_root
};

struct vfs_ops *cluster_root_ops(void)
{
    return &ops;
}

void cluster_init(void)
{
    hashmap_init(&name_to_transport, 16, HASHMAP_KEY_STRING);
}

void cluster_add_route(const char *name, void *transport)
{
    hashmap_put(&name_to_transport, (void *)name, transport);
}

void *cluster_route(const char *name)
{
    if (g_vfs.local_name && strcmp(g_vfs.local_name, name) == 0)
        return 0;

    return hashmap_get(&name_to_transport, (void *)name);
}
