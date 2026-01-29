#include "comm.h"

struct shell_trans_class *g_shell_trans = NULL;

void comm_bind(struct shell_trans_class *t, void *ctx)
{
    g_shell_trans = t;
    if (g_shell_trans) {
        g_shell_trans->ctx = ctx;
        if (g_shell_trans->init) {
            g_shell_trans->init(ctx);
        }
    }
}
