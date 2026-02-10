# TitShell：系统命令行使用手册

TitShell 是 lttit 系统内置的命令行解释器，支持：

- 字符流驱动（UART）
- 消息驱动（RPC / 远程 shell）
- 文件系统操作
- 任务管理
- 内存查看
- Vim 编辑器
- 远程命令执行
- BPF 加载与挂载

# 启动方式

Shell 有两种入口：

### **字符流驱动（本地 UART）**

```
shell_main();
```

流程：

- 打印提示符 `>`
- 调用 `shell_readline()` 读取一行
- 调用 `shell_parse()` 解析参数
- 调用 `shell_exec()` 执行命令

### **消息驱动（远程）**

```
shell_on_message(msg, len);
```

适用于：

- RPC 远程 shell
- 网络 shell
- NodeA → NodeB 的远程命令执行

# 路径解析

Shell 支持：

- 相对路径
- 绝对路径
- `.`、`..`
- 多个 `/` 自动折叠

核心函数：

```
make_abs_path(out, in);
normalize_path(out);
```

当前工作目录：

```
static char cwd[SHELL_MAX_PATH];
```

初始为空（表示根目录）。

# 内置命令

以下命令取决于编译选项：

```
SHELL_ENABLE_FS
SHELL_ENABLE_VIM
```

## 文件系统命令（FS）

### **ls**

```
ls [path]
```

列出目录内容。

### **cat**

```
cat <file>
```

打印文件内容。

### **touch**

```
touch <file>
```

创建空文件。

### **mkdir**

```
mkdir <dir>
```

创建目录（递归）。

### **cd**

```
cd <dir>
```

切换工作目录。

### **sync**

```
sync
```

同步 FS 位图与 inode 表到 Flash。

## **编辑器（Vim）**

```
vim <file>
```

进入 TitVim 编辑器（见 Vim 手册）。

## **系统命令**

### **mem**

```
mem
```

显示 heap 状态：

- 剩余空间
- 遍历空闲链表得到的总空闲
- 最大连续空闲块
- 空闲块数量

### **ps**

```
ps
```

显示任务列表：

- PID
- 状态
- 栈使用
- 周期
- deadline

### **memleak**

```
memleak
```

打印所有未释放的 heap 分配（需要 `HEAP_TRACKING=1`）。

## **远程命令**

### **remote**

```
remote <node_id> <cmd...>
```

将命令通过 SCP 发送到远程节点执行。

示例：

```
remote 1 ls /etc
```

## **文件系统 RPC 操作**

```
fsop <path> <flags> <read_size>
```

调用远程 FS 操作（NodeA → NodeB）。

## **BPF 加载与挂载**

```
bpf_hook <hook_name> <path>
```

从文件系统读取 BPF 程序并通过 RPC 加载到远程节点。

# Shell 行为

### **读取一行**

```
shell_readline()
```

支持：

- 回车结束
- Backspace 删除
- 回显输入字符

### **解析参数**

```
shell_parse()
```

按空格分割，最多 16 个参数。

### **执行命令**

```
shell_exec()
```

在命令表中查找：

```
cmd_table[] = {
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    ...
};
```

找不到 → 输出：

```
unknown command: xxx
```

# 消息驱动模式

```
shell_on_message(msg, len)
```

适用于：

- NodeA → NodeB 的远程 shell
- RPC 调用
- 网络 shell

行为：

- 将消息复制到 linebuf
- 调用 `shell_parse()`
- 调用 `shell_exec()`

无需回显、无需提示符。

# Shell 使用示例

### **列出根目录**

```
> ls /
```

### **查看文件**

```
> cat /etc/config.txt
```

### **编辑文件**

```
> vim /etc/config.txt
```

### **查看任务**

```
> ps
```

### **查看内存**

```
> mem
```

### **远程执行**

```
> remote 1 ls /home
```

### **加载 BPF**

```
> bpf_hook tcp_filter /bpf/tcp.bpf
```

# Shell 特点与限制

- 不支持管道
- 不支持重定向
- 不支持 Tab 补全
- 不支持历史记录
- 命令表静态定义
- 路径最大长度 64
- 参数最大数量 16

但它足够：

- 调试系统
- 查看 FS
- 管理任务
- 远程控制节点
- 加载 BPF
- 编辑配置文件

