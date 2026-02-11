## CCBPF使用手册

ccBPF有两大组件：编译器和虚拟机。

建议使用方式是：资源较为丰富的节点（比如Linux）运行编译器，把编译好的程序通过CSC发给对应的节点，节点运行虚拟机执行程序。



## 编译文件从 C 源码生成 .ccbpf 程序

下面示例展示了 ccbpf 的完整编译流程：

- 输入：一段 C 代码
- 输出：可加载的 `.ccbpf` 程序镜像
- 步骤：词法 → 语法 → IR → BPF → 打包 → 写入文件系统

这是 ccbpf 的最小可运行示例。

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

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    struct superblock sb;
    fs_port_init();

    if (fs_port_mount(&sb) != 0) {
        printf("FS mount after format failed!\r\n");
    }

    printf("FS mounted OK!\r\n");
    printf("Starting shell...\r\n");

    cmd_mem();

    compiler_init(16, (4*1024), (2*1024));
    lexer_set_input_buffer(src, strlen(src));

    struct lexer lex;
    lexer_init(&lex);

    struct Parser *p = parser_new(&lex);

    parser_program(p);

    cmd_mem();
    heap_debug_dump_leaks();

    frontend_destroy(&lex);

    cmd_mem();
    heap_debug_dump_leaks();

    mg_region_print_pools(frontend_region);
    mg_region_print_pools(longterm_region);
    mg_region_print_pools(ir_region);

    struct bpf_builder b;
    bpf_builder_init(&b, (3*1024));

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

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
    mg_region_print_pools(backend_region);
    bpf_builder_free(&b);

    cmd_mem();

    return 0;
}
```

