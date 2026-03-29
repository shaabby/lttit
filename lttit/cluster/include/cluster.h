#ifndef LTTIT_CLUSTER_H
#define LTTIT_CLUSTER_H

#include "vfs.h"
#include "hashmap.h"

void cluster_init(void);
void cluster_add_route(const char *name, void *transport);
void *cluster_route(const char *name);

struct vfs_ops *cluster_root_ops(void);

#endif
