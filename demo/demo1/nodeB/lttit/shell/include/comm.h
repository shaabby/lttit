#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void (*putc)(char c);
    char (*getc)(void);
    void (*write)(const char *buf, int len);
    int  (*peek)(void);
} comm_t;

extern const comm_t *comm;

void comm_init_ccnet(uint16_t dst_node);
void comm_ccnet_feed(const uint8_t *data, size_t len);

static inline void comm_putc(char c)
{
    comm->putc(c);
}

static inline char comm_getc(void)
{
    return comm->getc();
}

static inline void comm_write(const char *buf, int len)
{
    comm->write(buf, len);
}

static inline int comm_peek(void)
{
    return comm->peek();
}

#endif
