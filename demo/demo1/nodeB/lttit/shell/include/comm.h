#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stddef.h>

struct shell_trans_class {
    void (*init)(void *ctx);
    void (*send)(void *ctx, void *buf, size_t len);
    void *ctx;
};

extern struct shell_trans_class *g_shell_trans;

void comm_bind(struct shell_trans_class *t, void *ctx);

static inline void comm_write(const char *buf, int len)
{
    if (g_shell_trans && g_shell_trans->send && buf && len > 0) {
        g_shell_trans->send(g_shell_trans->ctx, buf, len);
    }
}

static inline void comm_putc(char c)
{
    comm_write(&c, 1);
}

static inline char comm_getc()
{
    return 0;
}

#endif
