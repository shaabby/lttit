# 协议栈使用手册

litterTCP是一个轻量级、可移植的 IPv4 协议栈，参考4.4BSD-lite，支持：

- Ethernet II
- ARP
- IPv4
- ICMP（Echo）
- UDP
- TCP（三次握手、数据收发、四次挥手）
- 路由表
- socket API（inpcb + tcpcb）

协议栈不依赖操作系统，可运行在 MCU 上。

建议litterTCP加CSC分布式协议栈，完整的TCP/IP太重了，对于嵌入式场景不是很友好。

可以直接在UDP上运行CSC三层分布式协议栈，SCP和ccnet会提供类TCP可靠功能和路由数据包功能。

# 初始化

协议栈初始化分为 4 部分：

```
eth_init();
arp_init();
ip_init();
tcp_init();
```

示例：

```
eth_init();
arp_init();
ip_init();
tcp_init();
```

# 注册网卡（ifnet）

用户必须提供一个网卡驱动结构体：

```
struct ifnet_class {
    uint8_t hwaddr[6];
    struct _in_addr ipaddr;
    struct _in_addr netmask;

    struct buf *(*input)(void);
    void (*output)(struct buf *sk);
    struct ifnet_class *if_next;
};
```

注册方式：

```
ifnet_register(&my_if);
```

# 发送数据（Ethernet 层）

协议栈使用两个队列：

- `EthOutQue`：待发送队列
- `EthReadyQue`：ARP 未解析完成的队列

发送流程：

```
ether_output(ifp, sk, dst);
```

其中：

- `sk` 为 buf
- `dst` 为 `_sockaddr`（AF_INET 或 AF_UNSPEC）

协议栈会自动：

- 填写以太网头
- ARP 解析
- 入队
- 调用网卡驱动发送

# 接收数据（Ethernet 层）

网卡驱动收到帧后调用：

```
ether_input(ifp);
```

协议栈会自动：

- 校验目的 MAC（单播或广播）
- 去掉以太网头
- 根据 ether_type 分发到 ARP / IP

# ARP

### **ARP 请求**

```
arp_request(ifp, &src_ip, &dst_ip);
```

### **ARP 输入**

```
arp_input(ifp);
```

协议栈会自动：

- 更新 ARP 缓存
- 回复 ARP 请求
- 唤醒等待 ARP 的数据包（EthReadyQue）

# 路由表

添加路由：

```
route_add(dest, netmask, gateway, ifp);
```

查找路由：

```
rtlookup(dst_ip);
```

协议栈使用最长前缀匹配。

# IPv4

### **IP 输入**

```
ip_input();
```

协议栈会自动：

- 校验版本
- 校验 checksum
- 判断是否本机 IP
- 若不是 → ip_forward
- 若是 → 分发到 ICMP / UDP / TCP

### **IP 输出**

```
ip_output(sk, &dst_addr);
```

协议栈会自动：

- 填写 IP 头
- 计算 checksum
- 调用 ether_output

# ICMP

支持 Echo（ping）。

### **接收**

```
icmp_input(sk, ip_header_len);
```

### **回复**

```
icmp_reflect(sk);
```

# UDP

### **接收**

```
udp_input(sk, ip_header_len);
```

协议栈会自动：

- 校验 checksum
- 提取 payload
- 唤醒所有等待 UDP 的 inpcb（InpQue）

### **发送**

```
udp_output(inp, sk, sa);
```

协议栈会自动：

- 填写 UDP 头
- 填写伪头
- 计算 checksum
- 调用 ip_output

# TCP

支持：

- 三次握手
- 数据收发
- ACK
- FIN/ACK
- TIME_WAIT
- CLOSE_WAIT
- LAST_ACK

### **接收**

```
tcp_input(sk, ip_header_len);
```

协议栈会自动：

- 校验 checksum
- 状态机处理
- 唤醒 recv_sem / send_sem
- 处理 PUSH、ACK、FIN

### **发送响应**

```
tcp_respond(tp, sk, ack, seq, flags);
```

# Socket（inpcb + tcpcb）

### **分配 PCB**

```
in_pcballoc(so, &TcpInpcb);
```

### **绑定**

```
in_pcbbind(inp, &local_addr);
```

### **连接**

```
in_pcbconnect(inp, &remote_addr);
```

### **查找 PCB**

```
in_pcblookup(&TcpInpcb, faddr, laddr, fport, lport);
```

### **断开**

```
in_pcbdisconnect(inp);
```

# buf（数据包）

协议栈使用 buf 作为统一的数据包结构：

```
struct buf {
    uint8_t *data;
    uint16_t data_len;
    uint16_t data_mes_len;
    uint8_t type;
    struct list_node node;
};
```

分配：

```
buf_get(size);
```

释放：

```
buf_free(sk);
```

调试：

```
buf_dump(sk);
```

# 协议栈典型使用流程

### **发送 UDP 数据**

```
struct buf *sk = buf_get(payload_len);
memcpy(sk->data, payload, payload_len);
sk->data_len = payload_len;

udp_output(inp, sk, &dst);
```

### **接收 UDP 数据**

```
sem_wait(&inp->recv_sem);
printf("recv %d bytes\n", inp->recv_len);
```

### **TCP 连接**

```
connect():
    → in_pcballoc
    → in_pcbbind
    → in_pcbconnect
    → tcp state machine
```

# 协议栈线程模型

协议栈本身不创建线程，用户需要：

- 在网卡中断中调用 `ether_input`
- 在主循环中调用 `arp_input` / `ip_input`
- socket 使用信号量同步（recv_sem / send_sem / sem_connected）

# 协议栈限制

- 不支持分片
- 不支持重传
- 不支持拥塞控制
- 不支持窗口缩放
- 不支持 TCP 超时重传
- UDP 广播未实现
- 路由表最多 16 条
- ARP 缓存无老化机制



# 使用示例

以下是在Linux用户态运行时的使用示例：

```c
//Your code like this:
ifnet_class *net_init()
{
    eth_init();
    arp_init();
    ip_init();
    tcp_init();
    socket_init();

    ifnet_class *ifp = new_ifnet_class("192.168.1.200",
                                       "9e:4d:9e:e3:48:9f",
                                       1500);
    ifnet_register(ifp);                                  

    route_add(inet_addr("192.168.1.0"),
              inet_addr("255.255.255.0"),
              0,
              ifp);

    route_add(0,
              0,
              inet_addr("192.168.1.1"),
              ifp);

    return ifp;
}

void *net_thread(void *arg) 
{
    fd_set readfds;
    ifnet_class *ifp = (ifnet_class *)arg;
    int ret;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(ifp->fd, &readfds);
        ret = select(ifp->fd + 1, &readfds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("select error");
            break;
        } else if (ret > 0 && FD_ISSET(ifp->fd, &readfds)) {
            ether_input(ifp);
        }
    }
}

#define SERVER_IP "192.168.1.200"  
#define SERVER_PORT 1234        
#define BUFFER_SIZE 1024

// TCP Client
void *tcp_thread(void *arg) 
{
    int so_fd = _socket(0, 0,IPPROTO_TCP);

    struct sockaddr_in server_addr = {0}, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.1"); 
    server_addr.sin_port = htons(8080); 

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("192.168.1.200"); 
    client_addr.sin_port = htons(1234); 

    _bind(so_fd, (struct _sockaddr *)&client_addr, 0);
    _connect(so_fd, (struct _sockaddr *)&server_addr);

    char buffer[20] = {0};
    strcpy(buffer, "Hello,linux Server");
    _sendto(so_fd, buffer, sizeof(buffer), (struct _sockaddr *)&server_addr);

    memset(buffer, 0, sizeof(buffer));
    int n = _recvfrom(so_fd, buffer, (struct _sockaddr *)&server_addr);
    if (n < 0) {
        perror("Recvfrom error");
    }
    buffer[n] = '\0'; 
    printf("Received: %s\n", buffer);

    _close(so_fd);  
    while(1) 
    {
        
    }
}


int main() {
    pthread_t thread_id1, thread_id2, thread_id3;

    ifnet_class *ifp = net_init();
    ifp->init("192.168.1.200", "9e:4d:9e:e3:48:9f", 1460);

    if (pthread_create(&thread_id1, NULL, net_thread, ifp) != 0) {
        perror("Failed to create thread1");
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread_id3, NULL, tcp_thread, NULL) != 0) {
        perror("Failed to create thread3");
        return EXIT_FAILURE;
    }

    while(1) {
        
    }
}
```

