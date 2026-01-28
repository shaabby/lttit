#include "comm.h"
#include "ccnet.h"
#include "schedule.h"
#include "usart.h"

#define COMM_RX_BUF_SIZE 256

static uint8_t rx_buf[COMM_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static uint16_t comm_dst = 0;

static void rx_push(uint8_t b)
{
    uint16_t next = (rx_head + 1) % COMM_RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = b;
        rx_head = next;
    }
}

static int rx_pop(void)
{
    if (rx_head == rx_tail)
        return -1;
    uint8_t b = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % COMM_RX_BUF_SIZE;
    return b;
}

static int ccnet_peek(void)
{
    if (rx_head == rx_tail)
        return -1;
    return rx_buf[rx_tail];
}

static void ccnet_putc(char c)
{
    struct ccnet_send_parameter p;
    p.dst  = comm_dst;
    p.ttl  = CCNET_TTL_DEFAULT;
    p.type = CCNET_TYPE_DATA;

    uint8_t b = (uint8_t)c;
    ccnet_output(&p, &b, 1);
}

static char ccnet_getc(void)
{
    char v;

    HAL_UART_Receive(&huart1, &v, 1,  HAL_MAX_DELAY);
    if (v != 0) {
        printf("recv: %u\r\n", v);
    }
    return (char)v;
}

static void ccnet_write(const char *buf, int len)
{
    struct ccnet_send_parameter p;
    p.dst  = comm_dst;
    p.ttl  = CCNET_TTL_DEFAULT;
    p.type = CCNET_TYPE_DATA;

    ccnet_output(&p, (void *)buf, len);
}

static const comm_t ccnet_comm = {
        .putc  = ccnet_putc,
        .getc  = ccnet_getc,
        .write = ccnet_write,
        .peek  = ccnet_peek,
};

const comm_t *comm = &ccnet_comm;

void comm_ccnet_feed(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        rx_push(data[i]);
}

void comm_init_ccnet(uint16_t dst_node)
{
    comm_dst = dst_node;
}
