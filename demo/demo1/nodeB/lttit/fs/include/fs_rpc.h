#ifndef _FS_RPC_H
#define _FS_RPC_H

#include <stdint.h>
#include "fs.h"


int fs_operation_handler(const struct rpc_param_fs_operation *in,
                         struct rpc_result_fs_operation *out);

#endif
