#include "rpc_tlv.h"
#include <stdio.h>
#include <stdlib.h>
#include "rpc_tlv.h"
#include "rpc_gen.h"
#include "fs_rpc.h"
#include "heap.h"
#include "ccbpf.h"
#include <string.h>

int fs_operation_handler(const struct rpc_param_fs_operation *in,
                         struct rpc_result_fs_operation *out)
{
    printf("NodeA: fs_operation(path=%s, flags=%u, read_size=%u)\n",
           in->path ? in->path : "(null)",
           (unsigned)in->flags,
           (unsigned)in->read_size);

    struct inode *ino = NULL;

    int rc = fs_open(in->path, in->flags, &ino);
    if (rc < 0 || !ino) {
        out->status = rc;
        out->read_data.ptr = NULL;
        out->read_data.len = 0;
        out->read_len = 0;
        return 0;
    }

    if (in->read_size > 0) {
        uint8_t *buf = heap_malloc(in->read_size);
        if (!buf) {
            out->status = -1;
            fs_close(ino);
            out->read_data.ptr = NULL;
            out->read_data.len = 0;
            out->read_len = 0;
            return 0;
        }

        int n = fs_read(ino, 0, buf, in->read_size);
        if (n < 0) {
            heap_free(buf);
            out->status = n;
            fs_close(ino);
            out->read_data.ptr = NULL;
            out->read_data.len = 0;
            out->read_len = 0;
            return 0;
        }

        out->read_data.ptr = buf;
        out->read_data.len = n;
        out->read_len = n;
    } else {
        out->read_data.ptr = NULL;
        out->read_data.len = 0;
        out->read_len = 0;
    }

    out->status = fs_close(ino);

    return 0;
}

int bpf_load_and_attach_handler(const struct rpc_param_bpf_load_and_attach *in,
                                struct rpc_result_bpf_load_and_attach *out)
{
    printf("NodeB: bpf_load_and_attach(hook=%s, image_len=%u)\n",
           in->hook_name ? in->hook_name : "(null)",
           (unsigned)in->image.len);

    if (!in->hook_name || !in->image.ptr || in->image.len == 0) {
        out->status  = -1;
        out->message = "invalid parameters";
        return 0;
    }

    int rc = hook_attach(in->hook_name, in->image.ptr, in->image.len);
    if (rc != 0) {
        printf("hook_attach failed: rc=%d\n", rc);
        out->status  = rc;
        out->message = "hook_attach failed";
        return 0;
    }

    out->status  = 0;
    const char *msg = "ok";
    size_t len = strlen(msg) + 1;
    out->message = heap_malloc(len);
    memcpy(out->message, msg, len);

    return 0;
}
