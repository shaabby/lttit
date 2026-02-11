# 分布式协议栈 CSC使用手册

CSC 是一个用于 MCU 集群的分布式通信总线，由 **ccnet（路由）+ scp（可靠传输）+ ccrpc（远程调用）** 组成。
 它的目标是：

**让多个 MCU 像一台计算机一样协作。**

CSC 是网络协议，也是一种 **分布式抽象**。

**介绍**

API在后面，由于这是我自创的协议，有必要进行一些基本的介绍。

## CSC 的核心思想

**在 CSC 抽象是，任何物理层连接没有本质区别，本地与非本地也没有本质区别，只有信息传递速度的差别。**

不同的连接，不管是串口还是网卡等等，在CSC的视角里，只要可以发数据，不管你是串口还是网卡，都可以看成一条数据可达流。

如果通信足够快、足够可靠，那么：

- 远程函数调用 ≈ 本地函数调用
- 远程内存访问 ≈ 本地内存访问
- 远程服务 ≈ 本地服务

从计算机的角度看：

**本地和远程只是时间与空间的不同点，而不是概念上的不同类别。**

远程只是连接有点长的本地链路，如果远程访问比本地更快，那么远程自然就变成了本地。

当速度足够快时，为什么不能从远程计算机的内存读取数据到本地执行呢？就像在本地执行可执行文件一样。

所以，CSC 的设计目标正是：**让 MCU 多节点从多机通信跃迁到整体协作。**

# 为什么需要 CSC？

在传统 MCU 系统中，本地调用简单可靠，而远程通信复杂脆弱。

随着节点增多、拓扑变复杂，开发者不得不自己处理路由、转发、重传、RPC 等问题，这些方案既不通用，也无法扩展。

CSC 的目标不是“让设备能通信”，而是**让 MCU 多节点像一台计算机一样协作**。

CSC 的价值不是“能通信”，而是**把分布式系统的复杂性封装成一个统一抽象**。

### 为什么不用现有协议？

CSC 不与 TCP/IP、CAN、UART、MQTT、gRPC 竞争，它们解决的是“通信问题”，而 CSC 解决的是分布式与抽象问题。

TCP/IP 太重，CAN/UART 太弱，MQTT/gRPC 依赖复杂栈；而且，各自适用的场景完全不一样，CSC的场景是嵌入式物联网节点小规模互联。

而在 CSC 的视角里，只要能发数据，不管是 UART、CAN、SPI 还是网卡，**都是一条可达链路**。

## CSC三层结构

### **1. CCNET：节点互联与路由**

- 维护节点连通图
- 使用 Dijkstra 计算最短路径
- 自动转发非本地数据包
- 让任意节点都能“可达”

ccnet 的本质就是：应用层的 Dijkstra 算法路由协议，用于统一 UART、CAN、SPI、RS485 等非 IP 链路，解决MCU 集群内部的节点寻址问题。

**它解决的是：如何让数据包到达目标节点？**

### **2. SCP：可靠字节流**

- 类 TCP 的轻量流控
- 连接管理
- 序号管理
- 超时重传
- 窗口控制
- 保活机制
- 乱序重组

它解决的是：**如何实现可靠、有序的数据流？**

### **3. CCRPC：远程调用协议**

- 让远程函数调用像本地调用一样自然
- 支持结构化参数
- 支持同步
- 基于 SCP 的可靠传输

它解决的是：**如何让节点之间互相调用方法？**

## 网络推荐结构

CSC 可以直接运行在 网络 IP 之上：

小规模节点之间使用 CSC 互联，再由一个具备网卡的节点作为网关连接更大的网络。这让 CSC 在本地形成“单机式集群”，同时又能与外部世界通信。

当 CSC 运行在 IP 上时，IP 负责“把包送到网关”，ccnet 负责“把包送到集群内部的目标节点”。

如果运行在UDP上，CSC三层可以无缝衔接，如果你使用TCP，可以去除SCP，三层都是可拆卸的，并不会相互影响。

## CSC网络模型与TCP/IP

ccnet和scp其实相当于另一种形式的TCP/IP。

ccnet把计算机抽象为节点，**节点由用户从0递增自由分配，只负责路由数据包。**

SCP 把连接直接做成一个结构体，**fd 只是这个结构体的句柄**，**因此多连接就是多个结构体**，无需 socket、无需端口、无需四元组。

在TCP/IP协议栈中，一条连接可以使用五元组表示：源ip、目的ip、源端口、目的端口、协议。

在 SCP 中，连接是一个结构体，fd 只是这个结构体的句柄；全局的一条连接由 节点 A, 节点 B, A 的 fd, B 的 fd唯一确定。

**因此，本地的fd在同一时刻不能重复，因为fd对应的结构体是唯一的。**

# ccnet 使用手册

ccnet 是 CSC 的路由层，用于在 MCU 集群内部进行节点寻址与转发。
 它不关心底层链路，也不关心上层协议，只负责根据节点 ID 选择下一跳，并把数据交给对应的处理函数。

## API

### 初始化

```c
int ccnet_init(uint16_t src, int node_count);
```

初始化 ccnet，全局只有一份。
 `src` 为本节点 ID，`node_count` 为节点总数。

### 设置链路代价

```c
void ccnet_graph_set_edge(int u, int v, int w);
```

设置 u → v 的链路代价。
 双向链路需要设置两次。

### 构建路由表

```c
void ccnet_build_routing_table(void);
```

根据邻接矩阵运行 Dijkstra，生成 next-hop 表。

### 注册节点处理函数

```c
int ccnet_register_node_link(uint16_t node_id, ccnet_link_t fun);
```

为某个节点 ID 注册处理函数。

- 如果 node_id == 本节点：这是上层入口（如 SCP）
- 如果 node_id 是下一跳：这是下层出口（如 UART/CAN/SPI）

### 发送数据

```c
int ccnet_output(void *ctx, void *data, int len);
```

`ctx` 为：

```c
struct ccnet_send_parameter {
    uint16_t dst;
    uint8_t ttl;
    uint8_t type;
};
```

### 接收数据

```c
int ccnet_input(void *ctx, void *data, int len);
```

输入一整个 ccnet 包，ccnet 会自动校验、TTL--、转发或投递。

# 使用

下面是一条完整链路：

我们当前节点是nodeB，要与nodeA组成网络链路：

```c
//每个节点用数字来代替，建议从0一直连续递增分配
#define NODE_ID_A 1 
#define NODE_ID_B 2
#define NODE_COUNT 3

static int ccnet_lower_send(void *user, const void *buf, size_t len)
{
    struct ccnet_send_parameter p = {
    		//使用ccnet发送数据包时，我们要手动指定目的节点
            .dst  = NODE_ID_A,
        	//最大转发次数
            .ttl  = CCNET_TTL_DEFAULT,
            //数据类型，其实目前只有这一种类型，但是预留，因为以后要做动态拓扑
            .type = CCNET_TYPE_DATA,
    };
    return ccnet_output(&p, (void *)buf, (int)len);
}

static int nodeA_provider(void *ctx, void *data, size_t len)
{
    //纯物理链路发送数据给nodeA，比如串口、iic等等
}

void taskA(void *ctx)
{
    //接收数据包到缓冲区packet，然后调用：
    ccnet_input(NULL, (void *)packet, packet_len); 
}

void APP(void)
{
    //初始化本地节点B
    ccnet_init(NODE_ID_B, NODE_COUNT);
    
    //注册对应的处理方法，当计算下一跳是节点B(本地)时，会调用xxx方法，把数据送给上层处理
    ccnet_register_node_link(NODE_ID_B, xxx);
    /*
     *当ccnet看到下一跳是节点A的时，会调用nodeA_provider方法，把数据包传输到nodeA上
     *至于这个数据包到达是节点c还是d的，我们不用关心
     *nodeA会继续调用ccnet计算下一跳，继续转发，直到送到对应的节点手里，这个节点会调用方法交给本地应用层
     */
    ccnet_register_node_link(NODE_ID_A, nodeA_provider);
    
    //设置可达性，表示从节点A到节点B是可达的，注意，每一个集群里，虽然我们只用设置直达节点的传输函数，但是状态图必须是全局的
    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_B, 1);
    //设置从B到A的可达性
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_A, 1);
    
    //提前计算节点B与全局节点的每一个下一跳，进行缓存，ccnet计算时直接查表
    ccnet_build_routing_table();
}
```

### 注意

* 连通矩阵是全局的，全局可达都要进行设置
* 注册可达方法时，只需要设置直达邻居的链路方法



# scp 使用手册

scp 是 CSC 的可靠字节流层，提供序号、确认、重传、窗口、乱序重组等功能。
 它不关心底层链路，也不关心上层协议，只负责可靠传输。

## API

### 初始化

```c
int scp_init(size_t max_streams);
```

初始化 SCP，全局只有一份。

### 分配流

```c
struct scp_stream *scp_stream_alloc(struct scp_transport_class *st_class,
                                    int src_fd,
                                    int dst_fd);
```

创建一个流，绑定底层传输类。

**注意**

fd就是结构体的映射，由用户自由分配，同一时刻，一个节点上，fd不能重复。

### 释放流

```c
int scp_stream_free(struct scp_stream *ss);
```

释放流。

### 输入数据

```c
int scp_input(void *ctx, void *buf, size_t len);
```

输入一整个 SCP 包，SCP 会自动处理序号、确认、重传等逻辑。

### 发送数据

```c
int scp_send(int fd, void *buf, size_t len);
```

向指定 fd 的流发送数据。

### 接收数据

```c
int scp_recv(int fd, void *buf, size_t len);
```

从指定 fd 的流读取数据。

### 关闭流

```c
void scp_close(int fd);
```

关闭流。

### 定时器

```c
void scp_timer_process();
```

SCP 的重传、保活、persist 都依赖此函数，需要周期性调用，考虑到精度和抢占，建议在物理定时器中调用。

如果对精度这些要求不高，创建一个线程周期性执行也行。

不过，如果有大量依赖定时的场景，建议把RTOS内核的定时器放到某个精度较高的硬件定时器中，使用定时器创建不同的定时任务。

# 使用

下面是一条完整链路：
 当前节点为 nodeB，通过 ccnet 与 nodeA 通信，底层链路为 UART。
 非 SCP 部分全部用伪代码表示。

```c
#define SCP_FD_A2B 1
#define SCP_FD_B2A 1

/* 给scp提供的底层发送函数*/
static int scp_lower_send(void *user, const void *buf, size_t len)
{
    //这里的user就是你设置的类的参数
    xxx_send(user, buf, len);
}

/* 底层传输类 */
static struct scp_transport_class scp_trans = {
    .send  = scp_ccnet_send,
    .recv  = NULL,   /* 这个recv在目前的scp中不会被调用，只是为了接口扩展保留*/
    .close = NULL,   //为接口扩展保留
    .user  = NULL,   //这个user很有用，你可以保留自己想要的参数，scp会原封不动把这个参数传递给send函数
};

/* 定时器任务：周期性调用 SCP 定时器 */
void task_timer(void *ctx)
{
    while (1) {
        scp_timer_process();
        task_delay(100);//100ms执行一次
    }
}

/* 上层入口：收到给本节点 B 的 ccnet 包时进入 SCP */
static int nodeB_upper(void *ctx, void *data, size_t len)
{
    scp_input(NULL, data, len);
    return 0;
}

/* 下层出口：CCNet → UART（伪代码） */
static int nodeA_provider(void *ctx, void *data, size_t len)
{
    wrap_with_magic_and_uart_send(data, len); /*伪代码*/
    return 0;
}

void uart_frame_task(void *ctx)
{
    while (1) {
        read_physical_packet(packet, &packet_len); /*伪代码*/
        scp_input(NULL, packet, packet_len);

        /* SCP → 上层 */
        int rn = scp_recv(SCP_FD_B2A, buf, sizeof(buf));
        if (rn > 0) {
            upper_layer_input(buf, rn); /*伪代码*/
        }
    }
}

void APP(void)
{
    /* 初始化 SCP */
    scp_init(4);
    scp_stream_alloc(&scp_trans, SCP_FD_B2A, SCP_FD_A2B);

    /* 创建定时器任务 */
    task_create(task_timer, 1024, NULL, 0, 10, 0, &t_timer);
    task_create(uart_frame_task, 1024, NULL, 0, 12, 0, &t_shell);
}
```



# CCRPC

rpc 是 CSC 的远程调用层，用于在节点之间进行结构化的函数调用。
 它不关心底层传输，也不关心路由，只负责参数解析、方法调用、结果编码、同步等待等逻辑。

## API

### 初始化

```c
void rpc_init(uint8_t method_cap, uint8_t pending_cap, uint8_t transport_cap);
```

初始化 RPC 框架。

- `method_cap`：最大方法数量
- `pending_cap`：最大挂起调用数量
- `transport_cap`：最大传输类数量

### 创建传输类

```c
struct rpc_transport_class *rpc_trans_class_create(
        void *send,
        void *recv,
        void *close,
        void *user);
```

创建一个传输类，用于绑定底层发送/接收接口。

- `send`：底层发送函数
- `recv`：底层接收函数（可选）
- `close`：关闭函数（可选）
- `user`：用户上下文

### 绑定传输类

```c
void rpc_bind_transport(const char *name, struct rpc_transport_class *t);
```

将方法名绑定到某个传输类。
 调用远程方法时，会根据方法名选择对应的传输类。

### 查找传输类

```c
struct rpc_transport_class *rpc_transport_lookup(const char *name);
```

根据方法名查找传输类。

### 注册本地方法

```c
void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder,
                         rpc_free_param_t free_param,
                         rpc_free_param_t free_result);
```

注册本地方法。

- `name`：方法名
- `parser`：TLV → 参数结构体
- `handler`：执行本地逻辑
- `encoder`：结果结构体 → TLV
- `free_param`：释放参数
- `free_result`：释放结果

### 处理底层输入数据

```c
void rpc_on_data(struct rpc_transport_class *t,
                 const uint8_t *buf, size_t len);
```

底层收到数据后调用此函数，RPC 会自动完成重组、解析、分发。

### 调用远程方法

```c
int rpc_call_with_tlv(const char *name,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *out_tlv, size_t *out_len,
                      uint32_t timeout_ms);
```

同步调用远程方法。

- `name`：方法名
- `tlv`：输入参数 TLV
- `out_tlv`：输出结果 TLV
- `timeout_ms`：超时

返回值为 `rpc_status_t`。

### rpc_status_t 状态码表

| 状态码                          | 数值 | 说明                                         |
| ------------------------------- | ---- | -------------------------------------------- |
| **RPC_STATUS_OK**               | 0    | 调用成功，返回结果有效。                     |
| **RPC_STATUS_METHOD_NOT_FOUND** | 1    | 方法名不存在，未注册对应的 RPC 方法。        |
| **RPC_STATUS_INVALID_PARAMS**   | 2    | 参数解析失败，TLV 无效或格式不符合方法要求。 |
| **RPC_STATUS_INTERNAL_ERROR**   | 3    | 方法执行失败，handler 内部错误。             |
| **RPC_STATUS_TRANSPORT_ERROR**  | 4    | 底层传输失败（如 SCP/CCNet 发送失败）。      |
| **RPC_STATUS_TIMEOUT**          | 5    | 超时未收到响应，远端无应答或链路中断。       |

## **同步与异步 RPC**

ccrpc 默认提供 **同步 RPC**： 调用方阻塞等待远程节点返回结果，保证语义简单、确定、可预测。

虽然默认是同步的，但 ccrpc 的内部机制（pending_call + seq + 分发器） **天然支持异步扩展**。 未来可以在不破坏现有 API 的前提下加入 callback 或 future 模型。

## 使用

### 定义RPC方法

定义一个xdef文件，对于本地提供的方法，这样写，记住，是RPC_METHOD_PROVIDER：

```c
RPC_METHOD_PROVIDER(
    fs_operation,
    "fs.operation",
    PARAMS(
        FIELD(string, path);
        FIELD(u32, flags);
        FIELD(u32, read_size);
    ),
    RESULTS(
        FIELD(bytes, read_data);
        FIELD(u32, read_len);
        FIELD(u32, status);
    )
)
```

至于调用远程的方法，这样写，唯一的不同是：RPC_METHOD_REQUEST

```c
RPC_METHOD_REQUEST(
    fs_operation,
    "fs.operation",
    PARAMS(
        FIELD(string, path);
        FIELD(u32, flags);
        FIELD(u32, read_size);
    ),
    RESULTS(
        FIELD(bytes, read_data);
        FIELD(u32, read_len);
        FIELD(u32, status);
    )
)
```

记得在cmake中定义xdef文件路径，例如：

```c
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/lttit/CSC/ccrpc/include/rpc_methods_node.xdef\"
)
```



## 提供方法

当我们提供方法供非本地调用时，需要声明对应的函数，以fs_operation为例，记住不要写成函数名，统一声明的方法名加_handler：

```c
int fs_operation_handler(const struct rpc_param_fs_operation *in,
                         struct rpc_result_fs_operation *out)
{
    xxxxxx
}
```

在本地调用远程方法:

```c
int cmd_fsop(int argc, char **argv)
{
    //按照模板写好参数，rpc_param_fs_operation这个结构体是根据头文件自动生成的
    struct rpc_param_fs_operation p;
    memset(&p, 0, sizeof(p));
    p.path = (char *)path;
    p.flags = flags;
    p.read_size = read_size;

    //这个也同理，会作为结果结构体返回
    struct rpc_result_fs_operation r;
    memset(&r, 0, sizeof(r));
    //统一rpc_call声明的名字，传入结构体，这里设置为等待10000ms
    int st = rpc_call_fs_operation(&p, &r, 10000);
    //现在重新执行回来了，看看返回结果
    printf("[NodeB] rpc_call_fs_operation => %d\n", st);
    //释放内存
    free_result_fs_operation(&r);

    return st;
}
```



## 整体配合使用

```c
/* ---------- 节点定义 ---------- */
#define NODE_ID_A 1
#define NODE_ID_B 2
#define NODE_COUNT 3

/* ---------- RPC → SCP ---------- */
static size_t rpc_scp_send(void *user, const uint8_t *buf, size_t len)
{
    return scp_send(/*fd*/ 1, (void *)buf, (int)len);
}

static size_t rpc_scp_recv(void *user, uint8_t *buf, size_t maxlen)
{
    return 0;   /* 不使用底层 recv */
}

static void rpc_scp_close(void *user) {}

/* ---------- SCP → CCNet ---------- */
static int scp_ccnet_send(void *user, const void *buf, size_t len)
{
    struct ccnet_send_parameter p = {
        .dst  = NODE_ID_A,
        .ttl  = CCNET_TTL_DEFAULT,
        .type = CCNET_TYPE_DATA,
    };
    return ccnet_output(&p, (void *)buf, (int)len);
}

static struct scp_transport_class scp_trans = {
    .send  = scp_ccnet_send,
    .recv  = NULL,
    .close = NULL,
    .user  = NULL,
};

//SCP 定时器
static void timer_excu(void *ctx)
{
    scp_timer_process();
}

// UART 收到一帧 → 交给 CCNet → CCNet 交给 SCP → SCP 交给 RPC 
void uart_frame_arrived(const uint8_t *frame, size_t len)
{
    ccnet_input(NULL, frame, len);

    uint8_t buf[256];
    int rn = scp_recv(/*fd*/ 1, buf, sizeof(buf));
    if (rn > 0) {
        rpc_on_data(global_rt, buf, rn);
    }
}

void APP(void)
{
    /* CCNet 初始化 */
    ccnet_init(NODE_ID_B, NODE_COUNT);
    ccnet_register_node_link(NODE_ID_B, scp_input);
    ccnet_register_node_link(NODE_ID_A, /*UART 发送函数*/ uart_send_frame);

    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_B, 1);
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_A, 1);
    ccnet_build_routing_table();

    /* SCP 初始化 */
    scp_init(4);
    scp_stream_alloc(&scp_trans, /*local_fd*/1, /*remote_fd*/1);

    /* RPC 初始化 */
    rpc_init(4, 4, 4);
    rpc_register_all();

    global_rt = rpc_trans_class_create(
        rpc_scp_send,
        rpc_scp_recv,
        rpc_scp_close,
        NULL
    );

    rpc_bind_transport("fs.operation", global_rt);
    rpc_bind_transport("bpf.load_and_attach", global_rt);

    /* 启动 UART 接收、定时器、任务调度（伪代码） */
    uart_start_receive();        /* 伪代码 */
    timer_create(timer_excu);    /* 伪代码 */
    task_create(uart_frame_arrived); /* 伪代码 */
}
```

