#include "uf.h"
#include "vfs.h"
#include "cluster.h"
#include "rpc.h"
#include "heap.h"
#include <string.h>

static char *h_strdup(const char *s)
{
    size_t len = strlen(s);
    char *p = heap_malloc(len + 1);
    if (!p) return 0;
    memcpy(p, s, len + 1);
    return p;
}

int uf_handle(const struct rpc_request *in,
              struct rpc_response *out)
{
    const char *path = in->path ? in->path : "";

    const char *p = path;
    while (*p == '/') p++;

    if (*p == 0) {
        out->output = h_strdup("bad path");
        out->exitcode = 1;
        return 0;
    }

    const char *slash = strchr(p, '/');
    const char *local_path = 0;
    struct vnode *n = 0;

    if (!slash) {
        local_path = p;              /* "root" / "dev/led" */
        n = vfs_walk(local_path);
    } else {
        int plen = slash - p;
        char prefix[64];
        if (plen >= (int)sizeof(prefix)) plen = sizeof(prefix) - 1;
        memcpy(prefix, p, plen);
        prefix[plen] = 0;

        const char *rest = slash + 1;

        if (strcmp(prefix, g_vfs.local_name) != 0) {
            void *t = cluster_route(prefix);
            if (!t) {
                out->output = h_strdup("NO ROUTE");
                out->exitcode = 1;
                return 0;
            }

            struct rpc_request inner = *in;
            inner.path = in->path;

            struct rpc_response r;
            memset(&r, 0, sizeof(r));

            int st = rpc_call(t, &inner, &r, 10000);
            if (st != RPC_STATUS_OK) {
                out->output = h_strdup("FORWARD FAIL");
                out->exitcode = 2;
                return 0;
            }

            out->output = r.output;
            out->exitcode = r.exitcode;
            return 0;
        }

        local_path = rest;           /* "root" / "dev/led" */
        n = vfs_walk(local_path);
    }

    if (!n || !n->ops) {
        out->output = h_strdup("no such path");
        out->exitcode = 1;
        return 0;
    }

    switch (in->op) {
    case RPC_OP_OPEN:
        if (!n->ops->open) break;
        n->ops->open(n, local_path, 0);
        out->output = h_strdup("open ok");
        out->exitcode = 0;
        return 0;

    case RPC_OP_READ: {
        if (!n->ops->read) break;
        char buf[1024];
        int r = n->ops->read(n, 0, buf, sizeof(buf) - 1);
        if (r < 0) {
            out->output = h_strdup("read failed");
            out->exitcode = 2;
            return 0;
        }
        buf[r] = 0;
        out->output = h_strdup(buf);
        out->exitcode = 0;
        return 0;
    }

    case RPC_OP_WRITE: {
        if (!n->ops->write) break;
        const char *data = in->args ? in->args : "";
        n->ops->write(n, 0, data, strlen(data));
        out->output = h_strdup("write ok");
        out->exitcode = 0;
        return 0;
    }

    case RPC_OP_CTL:
        if (!n->ops->ctl) break;
        n->ops->ctl(n, 0, 0, (void *)(in->args ? in->args : ""));
        out->output = h_strdup("ctl ok");
        out->exitcode = 0;
        return 0;

    case RPC_OP_CLOSE:
        if (!n->ops->close) break;
        n->ops->close(n, 0);
        out->output = h_strdup("close ok");
        out->exitcode = 0;
        return 0;
    }

    out->output = h_strdup("op not supported");
    out->exitcode = 1;
    return 0;
}
