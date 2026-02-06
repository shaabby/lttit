#ifndef BPF_BUILDER_H
#define BPF_BUILDER_H

#include "cbpf.h"
#include <stddef.h>

struct bpf_builder {
    struct bpf_insn *insns;
    int count;
    int capacity;
};

#define CCBPF_MAGIC 0x43434250  /* 'C' 'C' 'B' 'P' */

struct CCBPF_Header {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;

    uint32_t code_offset;
    uint32_t code_size;

    uint32_t data_offset;
    uint32_t data_size;

    uint32_t entry;
};


extern struct mg_region *backend_region;

void bpf_builder_init(struct bpf_builder *b, uint32_t cap);
void bpf_builder_free(struct bpf_builder *b);
void bpf_builder_reset(struct bpf_builder *b);

uint8_t *ccbpf_pack_memory(struct bpf_insn *insns,
                           size_t insn_count,
                           size_t *out_len);
int  bpf_builder_emit(struct bpf_builder *b, struct bpf_insn insn);
struct bpf_insn *bpf_builder_data(struct bpf_builder *b);
int  bpf_builder_count(struct bpf_builder *b);


#endif
