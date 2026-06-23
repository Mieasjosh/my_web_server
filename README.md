# Tiny Web Server

基于 **epoll + 线程池** 的高并发 C++ HTTP 服务器，支持静态文件服务、用户登录注册、断点续传文件上传。

## 特性

- **I/O 多路复用**：epoll 边缘触发 / 水平触发（可选）
- **并发模型**：支持 Proactor 和 Reactor 两种模式
- **线程池**：可配置 worker 数量，信号量调度
- **零拷贝文件传输**：`mmap` + `writev` scatter/gather I/O
- **大文件断点续传**：三阶段上传协议（init → chunk → complete），MD5 完整性校验
- **定时器**：基于排序链表的连接超时管理
- **日志系统**：支持同步 / 异步写日志，自动按日期拆分
- **MySQL 连接池**：RAII 连接管理，预加载用户表
- **优雅关闭**：SO_LINGER 可选

## 性能

在 WSL2 Ubuntu 26.04 / i7 / 8 线程环境下的压测结果（wrk）：

| 并发连接 | QPS | 平均延迟 | 吞吐量 |
|---------|------|---------|--------|
| 100 | 25,627 | 17ms | 6.5 MB/s |
| 5,000 | 25,057 | 6.6ms | 6.4 MB/s |
| **10,000** | **23,304** | **7.1ms** | 5.9 MB/s |
| 大文件(5.8MB) | 905 | 38ms | **5.1 GB/s** |

> 瓶颈在 CPU（单 epoll 线程 + 8 worker）。大文件场景下 mmap + writev 接近内存带宽上限。

## 构建

**依赖：**

- g++（需支持 C++11）
- libmysqlclient-dev
- pthread

```bash
# Ubuntu
sudo apt-get install -y g++ make libmysqlclient-dev

# 编译（debug 模式）
make server

# 编译（release 模式）
make server DEBUG=0
```

> Windows WSL2 用户注意：源码不要放在 `/mnt/e/...`（跨文件系统 9P 性能损失可达 20+ 倍），复制到 `/tmp/` 或 `~/` 下编译运行。

## 运行

```bash
./server -p 9006 -t 8 -l 0 -m 0 -a 0 -o 0 -s 8 -c 0
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-p` | 监听端口 | 9006 |
| `-t` | 线程池大小 | 8 |
| `-l` | 日志写入方式（0=同步, 1=异步） | 0 |
| `-m` | epoll 触发模式（0=LT+LT, 1=LT+ET, 2=ET+LT, 3=ET+ET） | 0 |
| `-a` | 并发模型（0=Proactor, 1=Reactor） | 0 |
| `-o` | 优雅关闭（0=不禁用, 1=禁用） | 0 |
| `-s` | 数据库连接池大小 | 8 |
| `-c` | 关闭日志（0=开启, 1=关闭） | 0 |

### 触发模式说明

| 值 | listenfd | connfd |
|----|----------|--------|
| 0 | LT | LT |
| 1 | LT | ET |
| 2 | ET | LT |
| 3 | ET | ET |

### 并发模型

- **Proactor**（`-a 0`）：主线程负责 I/O（read/write），线程池负责业务处理
- **Reactor**（`-a 1`）：线程池同时负责 I/O 和业务处理

## HTTP 路由

| 路径 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 跳转到登录页 |
| `/0` | GET | 注册页面 |
| `/1` | GET | 登录页面 |
| `/2` | POST | 登录校验 |
| `/3` | POST | 注册校验 |
| `/5` | GET | 图片页面 |
| `/6` | GET | 视频页面 |
| `/7` | GET | 粉丝页面 |
| `/upload/init` | POST | 初始化上传（JSON: filename, totalSize, md5） |
| `/upload/chunk` | POST | 上传数据块（Header: X-Filename, X-Offset） |
| `/upload/complete` | POST | 完成上传（JSON: filename），返回 MD5 校验结果 |
| `/*` | GET | 静态文件服务（`./root/` 目录） |

## 文件上传协议

支持大文件和断点续传的三阶段上传：

```
1. POST /upload/init     {"filename": "a.mp4", "totalSize": 1073741824, "md5": "d41d8cd9..."}
   → 200  {"status": "ready", "received": 0}  或 {"status": "resume", "received": 524288}

2. POST /upload/chunk    Header: X-Filename: a.mp4, X-Offset: 0
   [binary data in body]
   → 200  {"status": "chunk_ok", "received": 1048576}

3. POST /upload/complete {"filename": "a.mp4"}
   → 200  {"status": "complete", "md5": "d41d8cd9...", "md5_match": true}
```

- chunk 阶段使用 `pwrite()` 按偏移量写入，支持乱序和并发块
- complete 阶段计算文件 MD5 并与 init 时提供的值对比
- 超过 1 小时未完成的临时上传会被自动清理

## MySQL 配置

代码中硬编码的数据库信息（见 `config.cpp`）：

```cpp
host:     localhost
port:     3306
user:     root
password: 123456
database: yourdb
```

建表语句：

```sql
CREATE DATABASE IF NOT EXISTS yourdb;
USE yourdb;
CREATE TABLE user (
    username VARCHAR(64) PRIMARY KEY,
    passwd   VARCHAR(64) NOT NULL
);
```

## 项目结构

```
.
├── main.cpp                        # 入口，解析 CLI 参数
├── webserver.h / webserver.cpp     # 核心服务：epoll 事件循环、连接管理
├── config.h / config.cpp           # 配置解析
├── Makefile                        # 构建文件
│
├── http/
│   └── http_conn.h / http_conn.cpp # HTTP 解析、响应生成、路由分发
├── threadpool/
│   └── threadpool.h                # 通用线程池模板
├── lock/
│   └── locker.h                    # 互斥锁、信号量、条件变量封装
├── log/
│   ├── log.h / log.cpp             # 日志系统（同步/异步、文件轮转）
│   └── block_queue.h               # 线程安全阻塞队列（循环数组）
├── timer/
│   ├── lst_timer.h / lst_timer.cpp # 排序链表定时器、epoll/signal 工具
├── CGImysql/
│   ├── sql_connection_pool.h / .cpp# MySQL 连接池（RAII）
├── upload/
│   ├── upload_handler.h / .cpp     # 三阶段上传管理器
│   └── md5.h / md5.cpp             # MD5 实现（RFC 1321）
├── root/                           # 静态资源目录
│   ├── *.html
│   ├── *.gif, *.jpg, *.mp4
│   └── README.md                   # 页面路由表
└── uploads/                        # 上传文件存储目录
```

## 技术要点

- **epoll I/O 复用**：LT/ET 可选，通过 `epoll_create1` + `EPOLLIN/EPOLLOUT` 事件驱动
- **Proactor 模式**：主线程完成非阻塞 I/O 后将任务投递到线程池，避免线程池内部做 I/O
- **mmap + writev**：`mmap()` 映射文件到内存 + `writev()` 聚合发送响应头和文件内容，减少系统调用和用户态拷贝
- **HTTP 解析**：有限状态机解析请求行 → 请求头 → 请求体
- **定时器**：排序链表，SIGALRM 每 5s 触发一次 `tick()`，超时连接被 `cb_func` 回收
- **日志异步写**：独立线程从阻塞队列取日志写入磁盘，业务线程不等待 I/O

## 许可证

MIT
