#include "bpf_builder.h"
#include "heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bpf_builder_init(struct bpf_builder *b)
{
    b->count = 0;
    b->capacity = 128;

    b->insns = heap_malloc(sizeof(struct bpf_insn) * b->capacity);
    if (!b->insns)
        abort();
}

void bpf_builder_free(struct bpf_builder *b)
{
    if (b->insns)
        heap_free(b->insns);

    b->insns = NULL;
    b->count = 0;
    b->capacity = 0;
}

void bpf_builder_reset(struct bpf_builder *b)
{
    b->count = 0;
}

static void bpf_builder_grow(struct bpf_builder *b)
{
    int new_cap = b->capacity * 2;
    struct bpf_insn *new_insns;

    new_insns = realloc(b->insns,
                        sizeof(struct bpf_insn) * new_cap);
    if (!new_insns)
        abort();

    b->insns = new_insns;
    b->capacity = new_cap;
}

int bpf_builder_emit(struct bpf_builder *b, struct bpf_insn insn)
{
    if (b->count >= b->capacity)
        bpf_builder_grow(b);

    b->insns[b->count] = insn;
    return b->count++;
}

struct bpf_insn *bpf_builder_data(struct bpf_builder *b)
{
    return b->insns;
}

int bpf_builder_count(struct bpf_builder *b)
{
    return b->count;
}

extern char *string_pool[256];
extern int   string_pool_count;

uint8_t *ccbpf_pack_memory(struct bpf_insn *insns,
                           size_t insn_count,
                           size_t *out_len)
{
    struct CCBPF_Header hdr = {0};

    hdr.magic   = CCBPF_MAGIC;
    hdr.version = 1;
    hdr.flags   = 0;

    hdr.code_offset = (uint32_t)sizeof(struct CCBPF_Header);
    hdr.code_size   = (uint32_t)(insn_count * sizeof(struct bpf_insn));

    uint32_t str_size = sizeof(int);
    for (int i = 0; i < string_pool_count; i++) {
        int len = (int)strlen(string_pool[i]) + 1;
        str_size += sizeof(int);
        str_size += (uint32_t)len;
    }

    hdr.data_offset = hdr.code_offset + hdr.code_size;
    hdr.data_size   = str_size;

    hdr.entry = 0;

    size_t total_len = sizeof(struct CCBPF_Header)
                       + hdr.code_size
                       + hdr.data_size;

    uint8_t *buf = heap_malloc(total_len);
    if (!buf)
        return NULL;

    uint8_t *p = buf;

    memcpy(p, &hdr, sizeof(hdr));
    p += sizeof(hdr);

    memcpy(p, insns, insn_count * sizeof(struct bpf_insn));
    p += insn_count * sizeof(struct bpf_insn);

    memcpy(p, &string_pool_count, sizeof(int));
    p += sizeof(int);

    for (int i = 0; i < string_pool_count; i++) {
        int len = (int)strlen(string_pool[i]) + 1;

        memcpy(p, &len, sizeof(int));
        p += sizeof(int);

        memcpy(p, string_pool[i], (size_t)len);
        p += len;
    }

    if (out_len)
        *out_len = total_len;

    return buf;
}

int ccbpf_write_file(const char *path,
                     const uint8_t *buf,
                     size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen ccbpf");
        return -1;
    }

    size_t n = fwrite(buf, 1, len, fp);
    fclose(fp);

    if (n != len) {
        fprintf(stderr, "ccbpf_write_file: short write\n");
        return -1;
    }

    return 0;
}

void write_ccbpf(const char *path,
                 struct bpf_insn *insns,
                 size_t insn_count)
{
    size_t len = 0;
    uint8_t *buf = ccbpf_pack_memory(insns, insn_count, &len);
    if (!buf) {
        fprintf(stderr, "write_ccbpf: pack failed\n");
        return;
    }

    if (ccbpf_write_file(path, buf, len) != 0) {
        fprintf(stderr, "write_ccbpf: write_file failed\n");
    }

    heap_free(buf);
}
