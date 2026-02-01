#include "comm.h"
#include "scp.h"

static void nodeb_putc(void *ctx, char c)
{
    (void)ctx;
    scp_send(1, &c, 1);   // 1 = NodeA
}

static char nodeb_getc(void *ctx)
{
    (void)ctx;
    return 0;
}

static void nodeb_write(void *ctx, const char *buf, int len)
{
    (void)ctx;
    if (buf && len > 0)
        scp_send(1, buf, len);
}

static int nodeb_peek(void *ctx)
{
    (void)ctx;
    return -1;
}

static comm_t nodeb_comm = {
        .putc  = nodeb_putc,
        .getc  = nodeb_getc,
        .write = nodeb_write,
        .peek  = nodeb_peek,
        .ctx   = NULL,
};

comm_t *comm = &nodeb_comm;
