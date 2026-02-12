## CCBPF使用手册

ccBPF有两大组件：编译器和虚拟机。

建议使用方式是：资源较为丰富的节点（比如Linux）运行编译器，把编译好的程序通过CSC发给对应的节点，节点运行虚拟机执行程序。



## 编译文件从 C 源码生成 .ccbpf 程序

ccbpf 的完整编译流程：

- 输入：一段 C 代码
- 输出：可加载的 `.ccbpf` 程序镜像
- 步骤：词法 → 语法 → IR → BPF → 打包 → 写入文件系统（可选）

## **输入 C 源码**

我们可以通过打开文件等方式获得c语言字符串。

```c
const char *src =
    "struct udp_hdr {\n"
    "    unsigned short sport;\n"
    "    unsigned short dport;\n"
    "};\n"
    "\n"
    "int hook(void *ctx)\n"
    "{\n"
    "    unsigned int x;\n"
    "    unsigned int y;\n"
    "    struct udp_hdr *uh;\n"
    "\n"
    "    uh = (struct udp_hdr *)&ctx[0];\n"
    "    x = ntohs(uh->sport);\n"
    "    print(x);\n"
    "    y = ntohs(uh->dport);\n"
    "    print(y);\n"
    "    return x + y;\n"
    "}\n";
```

## 初始化编译器

```c
compiler_init(16, (4*1024), (2*1024));
lexer_set_input_buffer(src, strlen(src));
```

- `compiler_init()`：初始化前端/IR/后端的内存池，其中，16是内存池最大内存块的大小，为2^15大小，第二个参数是编译器前端所用内存大小设置，第三个参数是IR使用的内存1大小设置。
- `lexer_set_input_buffer()`：设置输入源代码

## 词法分析

```c
struct lexer lex;
lexer_init(&lex);
```

## 语法分析（生成 AST）

```c
struct Parser *p = parser_new(&lex);
parser_program(p);
```

## 清理前端资源（建议）

```c
frontend_destroy(&lex);
```

## **IR 阶段**

```c
struct ir_mes im;
ir_mes_get(&im);
```

此时 IR 已经由 parser 填充完毕。

## IR → BPF 指令生成

```c
struct bpf_builder b;
bpf_builder_init(&b, (3*1024));

ir_lower_program(im.ir_head, im.label_count, &b);

struct bpf_insn *prog = bpf_builder_data(&b);
int prog_len = bpf_builder_count(&b);
```

- `ir_lower_program()`：将 IR 降级为 BPF 指令序列
- `bpf_builder_data()`：获取最终 BPF 程序
- `bpf_builder_count()`：获取指令数量

## 打包成 ccbpf 可执行文件

```c
size_t image_len = 0;
uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

printf("=== CCBPF IMAGE READY ===\n");
printf("Image at %p, size = %u bytes\n", image, (unsigned)image_len);
```

`ccbpf_pack_memory()` 会生成可加载的 `.ccbpf` 镜像。

## 释放资源

```c
bpf_builder_free(&b);
```

## 完整使用示例

```c
int main(void)
{   //可以通过titvim编辑文件，然后打开文件得到字符串流
    const char *src =
            "struct udp_hdr {\n"
            "    unsigned short sport;\n"
            "    unsigned short dport;\n"
            "};\n"
            "\n"
            "int hook(void *ctx)\n"
            "{\n"
            "    unsigned int x;\n"
            "    unsigned int y;\n"
            "    struct udp_hdr *uh;\n"
            "\n"
            "    uh = (struct udp_hdr *)&ctx[0];\n"
            "    x = ntohs(uh->sport);\n"
            "    print(x);\n"
            "    y = ntohs(uh->dport);\n"
            "    print(y);\n"
            "    return x + y;\n"
            "}\n";
    //这些hal库的只是示例
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    //如果你不存放到文件系统里，那就没必要
    struct superblock sb;
    fs_port_init();

    if (fs_port_mount(&sb) != 0) {
        printf("FS mount after format failed!\r\n");
    }

    printf("FS mounted OK!\r\n");
    printf("Starting shell...\r\n");
    //初始化，设置内存
    compiler_init(16, (4*1024), (2*1024));
    lexer_set_input_buffer(src, strlen(src));

    struct lexer lex;
    lexer_init(&lex);

    struct Parser *p = parser_new(&lex);

    parser_program(p);
    frontend_destroy(&lex);

    struct bpf_builder b;
    bpf_builder_init(&b, (3*1024));

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);
    //上面都是固定流程，现在已经编译好了
    printf("=== CCBPF IMAGE READY ===\n");
    printf("Image at %p, size = %u bytes\n", image, (unsigned)image_len);

    struct inode *ino;
    if (fs_open("/prog.ccbpf", O_CREAT | O_RDWR, &ino) != 0) {
        printf("fs_open for write failed\n");
        return 0;
    }
    //把可执行文件写入文件系统
    int w = fs_write(ino, 0, image, (uint32_t)image_len);
    printf("fs_write wrote %d bytes\n", w);
    fs_close(ino);
    fs_sync();
    bpf_builder_free(&b);

    return 0;
}
```

# VM使用

ccbpf vm运行时提供以下能力：

- 从内存或文件加载 `.ccbpf` 程序
- 卸载程序
- 执行程序（传入 frame/ctx）
- 将程序 attach 到某个 hook
- 在 hook 触发时执行所有已 attach 的程序

运行时 API 是lttit“动态一致性”的核心。

## 结构体概览

###  **struct ccbpf_program**

```c
struct ccbpf_program {
    struct bpf_insn *insns;   // 指令序列
    size_t insn_count;        // 指令数量

    char **strings;           // 字符串常量池
    int string_count;

    void *data;               // 未来扩展
    size_t data_size;

    size_t map_count;         // map 数量
    struct hashmap maps[CCBPF_MAX_MAPS];

    uint32_t entry;           // 入口偏移
};
```

#  API 总览

| API                        | 作用                         |
| -------------------------- | ---------------------------- |
| `ccbpf_load_from_memory()` | 从内存加载 `.ccbpf` 程序     |
| `ccbpf_load()`             | 从文件系统加载 `.ccbpf` 程序 |
| `ccbpf_unload()`           | 卸载程序、释放资源           |
| `ccbpf_run_frame()`        | 执行程序（传入 frame/ctx）   |
| `hook_attach()`            | 将程序 attach 到某个 hook    |
| `hook_detach()`            | 卸载某个 hook 下的所有程序   |
| `hook_run()`               | 执行某个 hook 下的所有程序   |

下面逐一讲解。

# ccbpf_load_from_memory()

```c
struct ccbpf_program *ccbpf_load_from_memory(const uint8_t *image, size_t len);
```

### **功能**

从内存中的 `.ccbpf` 镜像加载一个程序。

### **参数**

| 参数    | 类型        | 说明              |
| ------- | ----------- | ----------------- |
| `image` | `uint8_t *` | `.ccbpf` 镜像数据 |
| `len`   | `size_t`    | 镜像长度          |

### **返回值**

- 成功：返回 `struct ccbpf_program *`
- 失败：返回 `NULL`

### **典型用法**

```c
uint8_t *image = ...; // 从文件或网络读取
size_t len = ...;

struct ccbpf_program *p = ccbpf_load_from_memory(image, len);
if (!p) {
    printf("load failed\n");
}
```

# ccbpf_unload()

```c
void ccbpf_unload(struct ccbpf_program *p);
```

### **功能**

释放程序占用的所有资源：

- 指令内存
- 字符串常量池
- map
- 程序结构体本身

### **参数**

| 参数 | 类型                     | 说明         |
| ---- | ------------------------ | ------------ |
| `p`  | `struct ccbpf_program *` | 要卸载的程序 |

# ccbpf_run_frame()

```c
uint32_t ccbpf_run_frame(struct ccbpf_program *prog,
                         void *frame,
                         size_t frame_size);
```

### **功能**

执行程序入口点，传入一个“frame”（上下文）。

### **参数**

| 参数         | 类型                     | 说明              |
| ------------ | ------------------------ | ----------------- |
| `prog`       | `struct ccbpf_program *` | 已加载的程序      |
| `frame`      | `void *`                 | 上下文数据（ctx） |
| `frame_size` | `size_t`                 | ctx 大小          |

### **返回值**

- 程序返回值（32-bit）

### **典型用法**

```c
uint8_t frame[64];
fill_frame(frame);

uint32_t ret = ccbpf_run_frame(p, frame, sizeof(frame));
printf("hook returned %u\n", ret);
```

# hook_attach()

```c
int hook_attach(const char *hook_name, uint8_t *image, size_t len);
```

### **功能**

将一个 `.ccbpf` 程序 attach 到某个 hook。

### **参数**

| 参数        | 类型           | 说明                               |
| ----------- | -------------- | ---------------------------------- |
| `hook_name` | `const char *` | hook 名称（如 `"hook_udp_input"`） |
| `image`     | `uint8_t *`    | `.ccbpf` 镜像                      |
| `len`       | `size_t`       | 镜像长度                           |

### **返回值**

- `0`：成功
- `<0`：失败

### **说明**

- 一个 hook 可以 attach 多个程序
- 程序按链表顺序执行

# hook_detach()

```c
int hook_detach(const char *hook_name);
```

### **功能**

卸载某个 hook 下的所有程序。

### **返回值**

- `0`：成功
- `<0`：失败

# **hook_run()**

```c
uint32_t hook_run(const char *hook_name, uint8_t *frame, size_t frame_size);
```

### **功能**

执行某个 hook 下的所有程序。

### **执行顺序**

- 多个程序按链表顺序依次执行
- 返回值为“最后一个程序”的返回值

### **参数**

| 参数         | 类型           | 说明           |
| ------------ | -------------- | -------------- |
| `hook_name`  | `const char *` | hook 名称      |
| `frame`      | `uint8_t *`    | ctx/frame 数据 |
| `frame_size` | `size_t`       | ctx 大小       |

### **返回值**

- 最后一个程序的返回值

# **典型使用流程（运行时）**

```c
// 1. 从文件加载程序
struct ccbpf_program *p = ccbpf_load("/prog.ccbpf");

// 2. attach 到 hook
hook_attach("hook_udp_input", image, image_len);

// 3. hook 触发时执行
uint32_t r = hook_run("hook_udp_input", frame, frame_size);

// 4. 卸载
hook_detach("hook_udp_input");
ccbpf_unload(p);
```

# hook 列表（默认内置）

```c
static struct hook_entry g_hooks[] = {
    { "hook_udp_input", NULL },
    { "hook_tcp_input", NULL },
    { "hook_sched", NULL },
    { NULL, NULL }
};
```

你可以扩展更多 hook。

## 注意

一个hook点，是可以同时hook多个程序的，这些hook程序会链式执行。

# 使用示例

你可以随便找一个你想hook的位置，比如调度器这里制造一个事件，同时记得在hook 列表里面添加：

当没有发生attach时，这里的ret默认什么都没有。

```c

uint32_t udp_input(uint8_t *frame, size_t frame_size)
{   在这里定义一个hook
    uint32_t ret = hook_run("hook_udp_input", frame, frame_size);
    
    //正常的udp输入处理
}

void task_switch_context(void)
{   //假设这里发生了udp包处理事件，很抱歉，我实在找不到几个会经常发生的事件，所以就使用调度器这里了
    uint8_t pkt[64];
    struct udp_hdr *uh = (struct udp_hdr *)pkt;

    uh->sport = htons(10000);
    uh->dport = htons(20000);
    uh->len   = htons(20);
    uh->checksum = 0;
    //假设这个数据包被输入udp处理路径
    uint32_t ret = udp_input(pkt, 20);
    //调度器逻辑
}
```

如果我们把这段c语言编译，然后attach，就会发现返回值变成了30000：

```c
const char *src =
            "struct udp_hdr {\n"
            "    unsigned short sport;\n"
            "    unsigned short dport;\n"
            "};\n"
            "\n"
            "int hook(void *ctx)\n"
            "{\n"
            "    unsigned int x;\n"
            "    unsigned int y;\n"
            "    struct udp_hdr *uh;\n"
            "\n"
            "    uh = (struct udp_hdr *)&ctx[0];\n"
            "    x = ntohs(uh->sport);\n"
            "    print(x);\n"
            "    y = ntohs(uh->dport);\n"
            "    print(y);\n"
            "    return x + y;\n"
            "}\n";
```

比如：

```c
	if (hook_attach("hook_udp_input", image, size) != 0) {
        printf("hook_attach failed\n");
        heap_free(image);
        return 0;
    }
```

我们也可以detach程序，那么一切恢复并且释放内存。

```c
hook_detach("hook_udp_input");
```

