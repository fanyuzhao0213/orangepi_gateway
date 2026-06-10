# Gate_orangepi 项目框架说明

## 1. 项目概述

**Gate_orangepi** 是一个基于 Orange Pi Zero 2W 开发板的嵌入式网关服务程序，提供 TCP 网络通信、设备状态管理和 OLED 显示功能。

### 主要功能

| 功能模块 | 描述 |
|---------|------|
| TCP 服务器 | 监听客户端连接，接收命令并返回响应 |
| LED 控制 | 通过命令控制板载 LED 灯开关 |
| 温度监控 | 获取设备温度并显示 |
| 客户端管理 | 管理所有连接的客户端 |
| OLED 显示 | 显示系统状态、欢迎界面、命令帮助等 |
| 日志系统 | 记录系统运行日志 |
| 配置管理 | 加载和管理配置文件 |

---

## 2. 架构设计

### 2.1 分层架构

项目采用分层架构设计，各层职责清晰，便于维护和扩展。

```
┌─────────────────────────────────────────────────────────────┐
│                    App Layer (应用层)                       │
│                     main.c                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  主循环管理 | 线程创建 | 信号处理 | 系统初始化       │    │
│  └─────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│                  Business Layer (业务层)                    │
│  ┌──────────────┐    ┌──────────────┐                      │
│  │ cmd_parser   │    │ event_manager│                      │
│  │ 命令解析     │    │ 事件处理     │                      │
│  └──────────────┘    └──────────────┘                      │
├─────────────────────────────────────────────────────────────┤
│                  Network Layer (网络层)                     │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │ tcp_server   │    │ client_mgr   │    │ msg_queue    │  │
│  │ TCP服务端    │    │ 客户端管理   │    │ 消息队列     │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                   Service Layer (服务层)                    │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │ log_service  │    │ config_svc   │    │ device_svc   │  │
│  │ 日志服务     │    │ 配置服务     │    │ 设备状态服务 │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    HAL Layer (硬件抽象层)                    │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │ hal_oled     │    │ oled_display │    │ hal_led      │  │
│  │ OLED驱动     │    │ 显示界面     │    │ LED驱动      │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
orangepiZero2w/
├── app/                    # 应用层
│   └── main.c              # 主程序入口
├── business/               # 业务层
│   ├── cmd_parser.c/h      # 命令解析器
│   └── event_manager.c/h   # 事件管理器
├── network/                # 网络层
│   ├── tcp_server.c/h      # TCP服务器
│   ├── client_manager.c/h  # 客户端管理器
│   └── message_queue.c/h   # 消息队列
├── service/                # 服务层
│   ├── log_service.c/h     # 日志服务
│   ├── config_service.c/h  # 配置服务
│   └── device_service.c/h  # 设备状态服务
├── hal/                    # 硬件抽象层
│   ├── hal_oled.c/h        # OLED驱动
│   ├── hal_oled_data.c/h   # OLED字模数据
│   ├── oled_display.c/h    # OLED显示界面
│   └── hal_led.c/h         # LED驱动
└── gate_orangepi.conf      # 配置文件
```

---

## 3. 核心模块详解

### 3.1 应用层 (App Layer)

**文件**: `app/main.c`

**职责**:
- 程序入口，解析命令行参数
- 初始化系统各模块
- 创建工作线程
- 主循环管理系统状态
- 信号处理与优雅退出

**线程模型**:

| 线程名称 | 职责 | 优先级 |
|---------|------|-------|
| 主线程 | 主循环、状态更新、OLED显示 | - |
| 网络线程 | TCP服务器运行、客户端连接处理 | 高 |
| 业务线程 | 消息队列消费、命令处理 | 中 |
| 日志线程 | 异步日志写入（预留） | 低 |
| 欢迎界面线程 | OLED欢迎界面异步显示 | 低 |

---

### 3.2 业务层 (Business Layer)

#### 3.2.1 命令解析器 (cmd_parser)

**文件**: `business/cmd_parser.c/h`

**功能**: 将客户端发送的字符串命令解析为枚举值

**支持的命令**:

| 命令字符串 | 枚举值 | 描述 |
|-----------|--------|------|
| `LED ON` | `CMD_LED_ON` | 打开LED灯 |
| `LED OFF` | `CMD_LED_OFF` | 关闭LED灯 |
| `GET STATUS` | `CMD_GET_STATUS` | 获取系统状态 |
| `GET TEMP` | `CMD_GET_TEMP` | 获取温度 |
| `GET CLIENT` | `CMD_GET_CLIENT` | 获取客户端数量 |
| `RELOAD CONFIG` | `CMD_RELOAD_CONFIG` | 重新加载配置 |
| `OLED HELP` | `CMD_OLED_HELP` | 显示命令帮助界面 |
| `OLED MAIN` | `CMD_OLED_MAIN` | 返回主界面 |

#### 3.2.2 事件管理器 (event_manager)

**文件**: `business/event_manager.c/h`

**功能**: 根据解析后的命令执行相应的业务逻辑，并生成响应

**处理流程**:
```
命令枚举 → 执行对应操作 → 更新设备状态 → 生成响应字符串
```

---

### 3.3 网络层 (Network Layer)

#### 3.3.1 TCP服务器 (tcp_server)

**文件**: `network/tcp_server.c/h`

**功能**: 基于 epoll 实现的高性能 TCP 服务器

**特性**:
- 支持高并发客户端连接
- 非阻塞 IO 模型
- 接收客户端数据后放入消息队列

#### 3.3.2 客户端管理器 (client_manager)

**文件**: `network/client_manager.c/h`

**功能**: 管理所有连接的客户端

**API**:
- `client_add()` - 添加客户端
- `client_remove()` - 移除客户端
- `client_count()` - 获取客户端数量
- `client_broadcast()` - 广播消息到所有客户端

#### 3.3.3 消息队列 (message_queue)

**文件**: `network/message_queue.c/h`

**功能**: 实现线程安全的消息队列，用于网络层和业务层解耦

**数据结构**:
```c
typedef struct {
    int client_fd;      // 客户端文件描述符
    char data[256];     // 消息数据
    size_t data_len;    // 数据长度
} message_t;
```

**特性**:
- 支持多线程并发访问（互斥锁保护）
- 支持阻塞弹出（条件变量）
- 支持优雅停止

---

### 3.4 服务层 (Service Layer)

#### 3.4.1 日志服务 (log_service)

**文件**: `service/log_service.c/h`

**功能**: 提供日志记录功能

**日志级别**:
- `LOG_LEVEL_INFO` - 信息日志
- `LOG_LEVEL_WARN` - 警告日志
- `LOG_LEVEL_ERROR` - 错误日志

**API**:
- `log_info()` - 记录信息
- `log_warn()` - 记录警告
- `log_error()` - 记录错误

#### 3.4.2 配置服务 (config_service)

**文件**: `service/config_service.c/h`

**功能**: 管理配置文件的加载和查询

**支持的配置项**:

| 配置项 | 默认值 | 描述 |
|-------|-------|------|
| `server_port` | `8888` | TCP服务器监听端口 |
| `log_file` | `./gate_orangepi.log` | 日志文件路径 |
| `status_interval` | `10` | 状态更新间隔（秒） |

**API**:
- `config_load()` - 加载配置文件
- `config_get()` - 获取配置值
- `config_set()` - 设置配置值

#### 3.4.3 设备状态服务 (device_service)

**文件**: `service/device_service.c/h`

**功能**: 管理设备状态，提供线程安全的状态访问

**状态结构**:
```c
typedef struct {
    int led_status;       // LED状态 (0: off, 1: on)
    int client_count;     // 客户端数量
    int temperature;      // 温度 (摄氏度 * 10)
} device_status_t;
```

**特性**: 使用互斥锁保证线程安全

---

### 3.5 硬件抽象层 (HAL Layer)

#### 3.5.1 OLED驱动 (hal_oled)

**文件**: `hal/hal_oled.c/h`

**功能**: 底层 OLED 驱动，基于 I2C 通信

**支持的字体**:
- `OLED_8X16` - ASCII 字符 8x16 点阵
- `OLED_6X8` - ASCII 字符 6x8 点阵
- `OLED_CF16x16` - 汉字 16x16 点阵（UTF-8编码）

**API**:
- `hal_oled_init()` - 初始化OLED
- `hal_oled_clear()` - 清屏
- `hal_oled_show_string()` - 显示字符串
- `hal_oled_update()` - 更新显示

#### 3.5.2 OLED显示界面 (oled_display)

**文件**: `hal/oled_display.c/h`

**功能**: 提供高级显示界面封装

**界面类型**:

| 界面函数 | 描述 |
|---------|------|
| `oled_display_welcome_async()` | 异步显示开机欢迎界面 |
| `oled_display_main()` | 显示系统主界面 |
| `oled_display_help()` | 显示命令帮助界面 |
| `oled_display_message()` | 显示消息提示界面 |
| `oled_display_command()` | 显示接收到的命令 |

**主界面布局**:
```
┌──────────────────────────────────────┐
│ Gate_orangepi      12:30:45         │ ← 标题栏
├──────────────────────────────────────┤
│ IP: 192.168.1.100                   │ ← 网络信息
│ Port: 8888                          │
├──────────────────────────────────────┤
│ Clients: 3    LED: ON ●             │ ← 状态信息
│ Temp: 32.5C                         │
└──────────────────────────────────────┘
```

#### 3.5.3 LED驱动 (hal_led)

**文件**: `hal/hal_led.c/h`

**功能**: LED 灯控制

**API**:
- `hal_led_init()` - 初始化LED
- `hal_led_on()` - 打开LED
- `hal_led_off()` - 关闭LED

---

## 4. 数据流与调用链

### 4.1 命令处理流程

```
客户端发送命令
        ↓
┌─────────────────┐
│  TCP Server     │ ← network/tcp_server.c
│  (网络线程)      │
└────────┬────────┘
         ↓
┌─────────────────┐
│ Message Queue   │ ← network/message_queue.c
│  (线程安全)      │
└────────┬────────┘
         ↓
┌─────────────────┐
│ Business Thread │ ← main.c (业务线程)
└────────┬────────┘
         ↓
┌─────────────────┐
│ cmd_parser      │ ← business/cmd_parser.c
│ 解析命令字符串   │
└────────┬────────┘
         ↓
┌─────────────────┐
│ event_manager   │ ← business/event_manager.c
│ 执行业务逻辑    │
└────────┬────────┘
         ↓
┌─────────────────┐
│ device_service  │ ← service/device_service.c
│ 更新设备状态    │
└────────┬────────┘
         ↓
    返回响应
        ↓
   发送给客户端
```

### 4.2 初始化顺序

```
main()
  │
  ├─→ signal_setup()          // 设置信号处理
  │
  ├─→ system_init()
  │     │
  │     ├─→ hal_led_init()        // 1. 初始化LED
  │     ├─→ log_init()            // 2. 初始化日志
  │     ├─→ config_load()         // 3. 加载配置
  │     ├─→ device_status_init()  // 4. 初始化设备状态
  │     ├─→ mq_init()             // 5. 初始化消息队列
  │     ├─→ client_manager_init() // 6. 初始化客户端管理
  │     ├─→ oled_display_init()   // 7. 初始化OLED
  │     └─→ oled_display_welcome_async() // 8. 异步显示欢迎界面
  │
  ├─→ pthread_create(network_thread)   // 创建网络线程
  ├─→ pthread_create(business_thread)  // 创建业务线程
  └─→ pthread_create(log_thread)       // 创建日志线程
```

### 4.3 主循环流程

```
while (g_running)
  │
  ├─→ print_system_status()       // 打印系统状态到日志
  │
  ├─→ device_status_get_all()     // 获取设备状态
  │
  └─→ if (!oled_display_is_welcome_running())
          oled_display_main()     // 更新OLED主界面
  │
  └─→ sleep(status_interval)      // 等待配置的间隔时间
```

---

## 5. 同步机制

### 5.1 互斥锁

| 互斥锁名称 | 保护资源 | 使用位置 |
|-----------|---------|---------|
| `g_status_mutex` | 设备状态结构体 | device_service.c |
| `mq_mutex` | 消息队列 | message_queue.c |
| `client_mutex` | 客户端列表 | client_manager.c |
| `config_mutex` | 配置链表 | config_service.c |

### 5.2 条件变量

| 条件变量 | 用途 |
|---------|------|
| `mq_cond` | 消息队列非空通知，用于阻塞弹出 |

---

## 6. 信号处理

| 信号 | 处理方式 |
|------|---------|
| `SIGINT` / `SIGTERM` | 设置 `g_running = 0`，触发优雅退出 |
| `SIGUSR1` | 重新加载配置文件 |
| `SIGUSR2` | 预留：切换日志等级 |
| `SIGPIPE` | 忽略，防止 socket 写错误导致进程退出 |

---

## 7. 编译与运行

### 7.1 编译命令

```bash
gcc -o gate_orangepi \
    app/main.c \
    business/cmd_parser.c business/event_manager.c \
    network/tcp_server.c network/client_manager.c network/message_queue.c \
    service/log_service.c service/config_service.c service/device_service.c \
    hal/hal_oled.c hal/oled_display.c hal/hal_oled_data.c hal/hal_led.c \
    -lpthread -lm
```

### 7.2 运行方式

```bash
# 前台运行
./gate_orangepi

# 守护进程模式
./gate_orangepi -d
```

### 7.3 配置文件

```ini
# gate_orangepi.conf
server_port=8888
log_file=./gate_orangepi.log
status_interval=10
```

---

## 8. 客户端命令说明

客户端可通过 TCP 连接发送以下命令：

| 命令 | 功能 | 响应示例 |
|------|------|---------|
| `LED ON` | 打开LED灯 | `OK: LED turned ON` |
| `LED OFF` | 关闭LED灯 | `OK: LED turned OFF` |
| `GET STATUS` | 获取系统状态 | `STATUS: LED=ON, Clients=2, Temp=32.5C` |
| `GET TEMP` | 获取温度 | `TEMP: 32.5C` |
| `GET CLIENT` | 获取客户端数量 | `CLIENTS: 2` |
| `RELOAD CONFIG` | 重新加载配置 | `OK: Config reloaded` |
| `OLED HELP` | 显示命令帮助界面 | `OK: Showing help` |
| `OLED MAIN` | 返回主界面 | `OK: Back to main` |

---

## 9. 设计特点

### 9.1 异步初始化
- OLED欢迎界面采用异步显示，不阻塞系统初始化流程
- 当"Gate_orangepi 服务已启动"日志打印时，系统已完全初始化

### 9.2 模块化设计
- 各层职责清晰，接口定义明确
- 便于单元测试和模块替换

### 9.3 线程安全
- 使用互斥锁保护共享资源
- 使用条件变量实现高效的线程同步

### 9.4 优雅退出
- 支持信号触发的优雅退出
- 正确清理所有资源和线程

---

## 10. 扩展建议

### 10.1 功能扩展
- 添加更多传感器支持（如湿度、光照等）
- 支持 MQTT 协议连接云端
- 添加 Web 管理界面

### 10.2 性能优化
- 实现异步日志写入
- 添加连接池机制
- 优化 OLED 显示刷新策略

### 10.3 可靠性增强
- 添加看门狗定时器
- 实现配置热更新
- 添加异常恢复机制
