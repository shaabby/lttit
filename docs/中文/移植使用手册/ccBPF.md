# **CCBPF：在 MCU 上实现动态编程的轻量机制**

在传统 MCU 系统中，固件通常是静态的： 逻辑写死在镜像里，更新需要重新编译、烧录并重启设备。 这使得 MCU 在运行时缺乏灵活性，也难以在系统内部实现动态扩展。

ccbpf 的目标是提供一种**简单、可验证、资源占用极低的运行时可加载程序机制**， 用于在 MCU 上实现可扩展的 hook、策略更新和轻量逻辑注入。

ccbpf 并不是 eBPF 的移植版本，而是借鉴其思想， 结合 MCU 的资源限制重新设计的一个极简方案：

- 一个受限的 C 子集（无循环、无指针算术）
- 一个小型 BPF 虚拟机（几 KB）
- 一个可加载的程序格式
- 一个可扩展的 hook 机制
- 一个简单的 map 存储接口

它适用于以下场景：

- 在不修改固件的情况下调整系统行为
- 为协议栈、调度器、驱动等位置添加可插拔逻辑
- 在分布式节点中同步策略或过滤规则
- 在资源受限的 MCU 上实现“轻量可编程性”

ccbpf 的设计重点是：

- **实现简单**
- **行为可预测**
- **易于验证**
- **资源占用小**
- **适合嵌入式环境**

它不是为了替代脚本语言，也不是为了构建复杂应用， 而是为了在 MCU 上提供一个**安全、可控、可扩展的内核级 hook 机制**。

# **ccbpf 与 Linux eBPF 的关系**

| 项目     | Linux eBPF   | ccbpf            |
| -------- | ------------ | ---------------- |
| 运行环境 | Linux 内核   | MCU（RTOS/裸机） |
| 目标     | 内核可编程性 | 节点动态一致性   |
| 安全模型 | verifier     | 语法与 VM 限制   |
| 程序类型 | 多种         | hook 程序        |
| 复杂度   | 高           | 极简             |
| 资源需求 | 高           | 极低（几 KB）    |



# ccbpf使用手册

**ccbpf 是lttit 的可编程内核机制， 让 MCU 在运行时加载、执行、卸载安全的小程序，从而动态改变系统行为，实现鸟群式的一致性。**

ccBPF有两大组件：编译器和虚拟机。

建议使用方式是：资源较为丰富的节点（比如Linux）运行编译器，把编译好的程序通过CSC发给对应的节点，节点运行虚拟机执行程序。

# **ccbpf C 语言子集规范**

ccbpf 使用一种受限的 C 语言子集作为源语言。
 该语言专为 **安全、可验证、可预测的内核 Hook 程序** 设计，语法由编译器前端严格定义。

本规范描述 ccbpf 支持的全部语法能力。

# 设计目标

ccbpf C 子集的设计目标：

- **可验证**：无循环、无指针算术、无复杂控制流
- **可预测**：所有内存访问在编译期可确定
- **可映射**：语法直接映射到 BPF 指令
- **可安全**：ctx 访问严格受限
- **可移植**：适合 MCU 环境执行

------

# 1. 程序结构

ccbpf 程序必须包含且只能包含一个入口函数：

```c
int hook(void *ctx)
{
    ...
}
```

要求：

- 函数名必须为 `hook`
- 返回类型必须为 `int`
- 参数必须为 `void *ctx`
- 不支持其他函数定义
- 不支持递归、函数指针、可变参数

------

# 2. 类型系统

ccbpf 支持以下类型：

## 2.1 基本类型

因为目前虚拟机不支持有符号，所以unsigned int和int没什么区别，作为扩展保留。

## **整数语义说明（非常重要）**

ccbpf 虚拟机基于经典 BPF，所有算术与比较均为 **无符号语义**。 因此：

- `int` 实际等价于 `uint32_t`
- `short` 实际等价于 `uint16_t`
- `char` 实际等价于 `uint8_t`
- 所有比较均为无符号比较
- 所有右移均为逻辑右移
- 所有算术均为无符号算术
- `(int)`、`(short)`、`(char)` 不改变 signedness

| 类型             | 宽度 | 说明   |
| ---------------- | ---- | ------ |
| `int`            | 4    | 无符号 |
| `bool`           | 1    | 无符号 |
| `char`           | 1    | 无符号 |
| `short`          | 2    | 无符号 |
| `unsigned int`   | 4    | 无符号 |
| `unsigned short` | 2    | 无符号 |
| `unsigned char`  | 1    | 无符号 |

------

## 2.2 指针类型

支持任意层级的指针：

```c
int *p;
struct hdr *h;
unsigned char **pp;
```

但限制：

- 不支持指针算术（`p+1`、`p++` 等）
- 不支持解引用（`*p`）
- 指针主要用于 **ctx 派生的结构体指针**

------

## 2.3 数组类型

```c
int a[10];
char buf[32];
```

要求：

- 数组大小必须是常量
- 支持 `a[i]` 访问
- 不支持多维数组

------

## 2.4 struct 类型

支持结构体定义：

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};
```

支持：

- 多字段
- 字段宽度自动累加
- 字段偏移由编译器计算

不支持：

- 嵌套 struct
- struct 赋值
- struct 作为局部变量（只能作为指针）

------

## 2.5 struct 指针

支持：

```c
struct udp_hdr *uh;
uh->sport;
```

但仅限：

- 来自 `(struct T *)&ctx[offset]` 的 ctx 派生指针

------

# 3. ctx 访问模型（核心）

ccbpf 的核心语义是 **ctx 访问**。

## 3.1 ctx 数组访问

```c
ctx[0]
ctx[12]
```

要求：

- 下标必须是常量（编译期常量）
- 只能用于派生结构体指针

## 3.2 ctx → struct 指针

```c
struct udp_hdr *uh = (struct udp_hdr *)&ctx[0];
```

编译器会：

- 记录结构体类型
- 记录基地址偏移
- 允许 `uh->field` 访问

## 3.3 ctx 字段访问

```c
x = uh->sport;
```

编译器会将其转换为：

```
ctx_load(offset + field_offset)
```

------

# 4. 表达式支持

ccbpf 支持以下表达式：

## 4.1 算术表达式

```
+   -   *   /   %
```

## 4.2 比较表达式

```
<   <=   >   >=   ==   !=
```

## 4.3 逻辑表达式

```
&&   ||
```

## 4.4 位运算

```
&   |
```

## 4.5 一元运算

```
-   !   &ctx[CONST]
```

限制：

- `&expr` 仅允许 `&ctx[常量]`
- 不支持一般取地址

------

# 5. 语句支持

## 5.1 赋值语句

```c
x = y;
arr[i] = v;
```

## 5.2 return

```c
return x + y;
```

## 5.3 if / else

```c
if (cond) { ... }
else { ... }
```

## 5.4 代码块

```c
{
    ...
}
```

## 5.5 空语句

```c
;
```

# 6. 内置函数（builtin）

编译器内置以下函数：

支持ntohs,ntohl和print，你可以添加本地调用，只需要在语法解析进行添加，并且在虚拟机分支处理添加，后面我会更新教程。

## 6.1 字节序转换

```c
ntohs(x)
ntohl(x)
```

## 6.2 打印

```c
print(x)
print("string")
```

## 6.3 map 操作

```c
map_lookup(mapid, key)
map_update(mapid, key, value)
```

------

# 7. 常量支持

## 7.1 整数常量

```
123
0
42
```

## 7.2 布尔常量

```
true
false
```

## 字符串常量

```
"hello"
```

# 不支持的语法

以下语法在 parser.c 中未实现，属于 **明确不支持**：

| 不支持           | 原因          |
| ---------------- | ------------- |
| for / while / do | 无循环结构    |
| 多函数           | 仅支持 hook() |
| switch           | if足够了      |
| typedef          | 没必要        |
| 全局变量         | 请使用map     |
| 指针算术         | 不安全        |
| 解引用 *p        | 不安全        |
| &expr（除 ctx）  | 不安全        |
| struct 赋值      | 没必要        |
| struct 嵌套      | 没必要        |
| 多维数组         | 没必要        |
| 浮点数           | 没必要        |

# 示例程序

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh = (struct udp_hdr *)&ctx[0];

    unsigned int s = ntohs(uh->sport);
    unsigned int d = ntohs(uh->dport);

    print(s);
    print(d);

    return s + d;
}
```

## 使用示例

这里是一些测试demo，可以作为编写的使用示例。

基本运算：

```c
int hook(void *ctx)
{
    int a;
    int b;
    int c;
    int arr[4];

    a = 3;
    b = 4;
    c = a + b * 2;

    arr[1] = c;
    a = arr[1];

    if (a < b && c == 10) {
        c = c - 1;
    }

    if (a > b || !(c == 10)) {
        c = c + 1;
    }

    if (!(a < b) && !(c != 10)) {
        c = c + 2;
    }
    return 0;
}
```

map使用及更新,其中的count用来统计捕获了多少个udp数据包：

```c
struct udp_hdr {
    unsigned int sport;
    unsigned int dport;
};


int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int key;
    unsigned int count;
    uh = (struct udp_hdr *)ctx;

    key = ntohs(uh->sport);
    count = map_lookup(0, key);

    count = count + 1;
    map_update(0, key, count);

    print(count);

    return 0;
}
```

测试demo：

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    unsigned int x;
    unsigned int y;
    unsigned int key;
    unsigned int val;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&ctx[0];
    x = ntohs(uh->sport);
    print(x);
    y = ntohs(uh->dport);
    print(y);

    key = x;
    val = y;

    map_update(0, key, val);

    val = map_lookup(0, key);
    print(val);

    print(map_lookup(0, 9999)); 
    map_update(0, 1, 11);
    map_update(0, 2, 22);
    map_update(0, 3, 33);
    print(map_lookup(0, 1));
    print(map_lookup(0, 2));
    print(map_lookup(0, 3));

    return x + y;
}
```

print只有两个功能，只能打印字符串或者数字,

但是，将这两个功能组合起来，足已实现任何解析打印：

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport; 
    unsigned int dport; 
    uh = (struct udp_hdr *)ctx;
    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);

    print("UDP ");
    print("sport=");
    print(sport);
    print(" dport=");
    print(dport);
    print("\n");

    return 0;
}
```

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

