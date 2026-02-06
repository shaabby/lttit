#ifndef BPF_BUILDER_H
#define BPF_BUILDER_H

#include "cbpf.h"
#include "bpf_format.h"
#include <stddef.h>

struct bpf_builder {
    struct bpf_insn *insns;
    int count;
    int capacity;
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
