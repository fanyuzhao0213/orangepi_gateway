# Gate_orangepi systemd 服务部署与运维指南

> 本文档面向 **生产环境运维人员**,详细说明 Gate_orangepi 网关服务在 Orange Pi Zero 2W (Armbian/Ubuntu/Debian) 上的部署、监控、故障排查方法。

---

## 1. 方案概览

| 启动方式 | 适用场景 | 优点 | 缺点 |
|---------|---------|------|------|
| `./Gate_orangepi` | 调试 | 最简单 | 退出 shell 就死 |
| `./Gate_orangepi -d` | 传统守护 | 脱离终端 | 无自动重启,无日志管理 |
| **systemd** (推荐) | **生产** | **自动重启/日志集成/依赖管理** | 需配置 service 文件 |

**本文档仅介绍 systemd 方案。**

---

## 2. 一键部署 (推荐)

### 2.1 在 Windows / Linux 编译主机上

```bash
# 1. 编译并部署二进制 + 安装 service + 启用自启
make deploy-service
```

执行后:
1. 交叉编译最新代码
2. scp 到 Orange Pi `/home/orangepi/fyz_test/`
3. 推送 service 单元到 `/etc/systemd/system/`
4. `daemon-reload` + `enable` + `restart`
5. 打印运维命令速查

### 2.2 在 Orange Pi 上单独安装 (备用)

```bash
cd /path/to/project
chmod +x scripts/install-service.sh
sudo ./scripts/install-service.sh            # 完整安装
sudo ./scripts/install-service.sh status     # 查看状态
sudo ./scripts/install-service.sh restart    # 重启
sudo ./scripts/install-service.sh uninstall  # 卸载
```

---

## 3. 服务架构

### 3.1 进程模型

```
systemd (PID 1)
    └─ Gate_orangepi (PID X)   ← systemd 跟踪的"主进程"
         ├─ 网络线程    (TCP:8888)
         ├─ 业务线程    (消息队列消费)
         ├─ 日志线程
         └─ Web 线程    (HTTP:8080)
```

> ⚠️ **重要**: 本程序**不再做 double-fork**。`main.c` 启动时检测 `PPID==1` 或 `INVOCATION_ID` 环境变量,识别为 systemd 启动后跳过 `daemon_init()`,这样 systemd 才能正确跟踪主进程。

### 3.2 文件路径

| 路径 | 用途 |
|------|------|
| `/home/orangepi/fyz_test/Gate_orangepi` | 主程序(由 make deploy 上传) |
| `/home/orangepi/fyz_test/gate_orangepi.conf` | 配置文件(同目录) |
| `/home/orangepi/fyz_test/gate_orangepi.log` | 应用日志(同目录) |
| `/etc/systemd/system/gate-orangepi.service` | systemd 单元文件 |
| `journalctl -u gate-orangepi.service` | systemd 日志(含 stdout/stderr) |

> 💡 程序启动时通过 `readlink /proc/self/exe` 拿到自身路径,自动 `chdir` 到该目录,所以配置/日志的相对路径在 systemd 下也正确。

---

## 4. 权限配置 (GPIO / I2C)

**症状**: `status=1/FAILURE`,日志显示 `OLED 初始化失败` 或 `HAL LED 初始化失败`。

**原因**: `User=orangepi` 默认没有 GPIO / I2C 设备访问权限。

**修复** (二选一):

### 方案 A:给 orangepi 用户加入硬件组(推荐)

```bash
sudo usermod -aG gpio,i2c,dialout orangepi
# 重新登录或重启服务使组权限生效
sudo systemctl restart gate-orangepi.service
```

### 方案 B:以 root 身份运行(不推荐,仅调试用)

修改 `/etc/systemd/system/gate-orangepi.service`:
```ini
[Service]
User=root
Group=root
```

然后:
```bash
sudo systemctl daemon-reload
sudo systemctl restart gate-orangepi.service
```

### 方案 C:用 udev 规则放行设备(最优雅)

```bash
sudo tee /etc/udev/rules.d/99-gate-orangepi.rules <<'EOF'
# GPIO
SUBSYSTEM=="gpio*", PROGRAM="/bin/sh -c 'chown -R root:gpio /sys/class/gpio && chmod -R 770 /sys/class/gpio'"
SUBSYSTEM=="gpio*", RUN+="/bin/chmod g+rw /dev/gpiochip0"

# I2C
KERNEL=="i2c-[0-9]*", GROUP="i2c", MODE="0660"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
sudo systemctl restart gate-orangepi.service
```

---

## 5. 常用运维命令

### 5.1 状态与日志

```bash
# 查看服务状态
sudo systemctl status gate-orangepi.service

# 实时跟踪日志
sudo journalctl -u gate-orangepi.service -f

# 查看最近 100 行日志(带时间戳)
sudo journalctl -u gate-orangepi.service -n 100 --no-pager

# 查看本次启动以来的日志
sudo journalctl -u gate-orangepi.service -b

# 查看今天的日志
sudo journalctl -u gate-orangepi.service --since today

# 按级别过滤(只看 error 及以上)
sudo journalctl -u gate-orangepi.service -p err
```

### 5.2 服务控制

```bash
# 启动
sudo systemctl start gate-orangepi.service

# 停止
sudo systemctl stop gate-orangepi.service

# 重启
sudo systemctl restart gate-orangepi.service

# 重载配置(不重启进程)
sudo systemctl reload gate-orangepi.service
# 注:本程序未实现 reload 处理,等同于 restart

# 取消开机自启
sudo systemctl disable gate-orangepi.service

# 重新启用开机自启
sudo systemctl enable gate-orangepi.service
```

### 5.3 信号转发

| 信号 | 用途 | 触发命令 |
|------|------|---------|
| SIGUSR1 | 重载配置文件 | `sudo systemctl kill -s SIGUSR1 gate-orangepi.service` |
| SIGUSR2 | 切换日志级别(预留) | `sudo systemctl kill -s SIGUSR2 gate-orangepi.service` |

### 5.4 Makefile 快捷命令 (在编译主机执行)

```bash
make service-status     # 查看服务状态
make service-logs       # 最近 50 行日志
make service-logs-f     # 实时跟踪
make service-restart    # 重启服务
make service-uninstall  # 完全卸载
```

---

## 6. 故障排查

### 6.1 服务持续重启

```bash
sudo journalctl -u gate-orangepi.service -n 30 --no-pager
```

**最常见原因**:
- `Main PID: ... (code=exited, status=1/FAILURE)` → 程序主动返回 1
  - 通常是初始化失败(LED / I2C / 配置文件路径)
  - **检查**:配置/日志相对路径是否在当前目录找到
- `Main PID: ... (code=exited, status=0/SUCCESS)` → 程序正常退出
  - **原因**:可能是 daemon 模式 (用了 `-d`),程序 fork 出去后 systemd 跟踪的主进程退出
  - **修复**:service 文件的 `ExecStart` **不要**加 `-d`

### 6.2 端口被占用

```bash
# 查看 8888 / 8080 谁在占用
sudo ss -tlnp | grep -E ':8888|:8080'

# 杀掉占用进程
sudo fuser -k 8888/tcp
sudo fuser -k 8080/tcp
```

### 6.3 GPIO / I2C 权限不足

```bash
# 验证当前用户是否在 gpio/i2c 组
groups orangepi

# 测试 I2C 设备是否能打开
sudo -u orangepi ls -la /dev/i2c-2
```

### 6.4 工作目录不对

查看日志,如果出现:
```
工作目录已切换到: /home/orangepi/fyz_test
```
说明 chdir 成功。

如果出现:
```
切换工作目录失败: ...
```
说明 `/proc/self/exe` 不可读,极少见,可能是 `/proc` 没挂载。

### 6.5 重启次数过多被限流

`StartLimitBurst=3` 触发后会停止重启。手动恢复:
```bash
sudo systemctl reset-failed gate-orangepi.service
sudo systemctl start gate-orangepi.service
```

---

## 7. 升级流程

```bash
# 1. 编译主机: 重新编译 + 部署二进制 + 重启服务
make deploy-service

# 2. 验证
make service-status
make service-logs

# 3. 如有问题快速回滚(在 Orange Pi 上)
sudo systemctl stop gate-orangepi.service
cp /home/orangepi/fyz_test/Gate_orangepi.bak /home/orangepi/fyz_test/Gate_orangepi
sudo systemctl start gate-orangepi.service
```

> 💡 **建议**: 部署脚本里加个 `Gate_orangepi.bak` 自动备份(本次未实现,见 TODO)。

---

## 8. 开机启动验证清单

部署完成后,在 Orange Pi 上执行:

```bash
# 1. 确认已启用
sudo systemctl is-enabled gate-orangepi.service
# 期望输出: enabled

# 2. 立即重启测试
sudo reboot
```

等 30-60 秒后重新登录:

```bash
# 3. 服务应该自动启动了
sudo systemctl is-active gate-orangepi.service
# 期望输出: active

# 4. 端口应该监听中
sudo ss -tlnp | grep -E ':8888|:8080'

# 5. Web 界面可访问
curl -s http://127.0.0.1:8080/ | head -5
```

如果全部通过,开机自启动配置完成。

---

## 9. 安全建议

### 9.1 不要以 root 运行

已在 service 文件中默认 `User=orangepi`。如果必须以 root 运行,务必:
- 加固防火墙(`ufw` / `iptables`)
- 限制 Web 端口(`8080`)只监听内网或加 Basic 认证

### 9.2 Web API 鉴权 (TODO)

当前 Web API **无鉴权**,任何能访问 8080 端口的人都能控制 LED / 改配置。

**生产环境强烈建议**:
- 用防火墙限制 Web 端口来源 IP
- 或在 `web_service.c` 中加 Basic Auth(下个版本)

### 9.3 文件权限

```bash
# 配置和日志应仅 orangepi 可读写
chmod 600 /home/orangepi/fyz_test/gate_orangepi.conf
chmod 600 /home/orangepi/fyz_test/gate_orangepi.log
```

---

## 10. 附录:完整 service 文件

`service/gate-orangepi.service`:

```ini
[Unit]
Description=Gate_orangepi Gateway Service
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=60
StartLimitBurst=3

[Service]
Type=simple
User=orangepi
Group=orangepi
WorkingDirectory=/home/orangepi/fyz_test
ExecStart=/home/orangepi/fyz_test/Gate_orangepi
Environment="PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
Restart=always
RestartSec=5
StandardInput=null
StandardOutput=journal
StandardError=journal
SyslogLevel=info
MemoryMax=128M
TasksMax=64

[Install]
WantedBy=multi-user.target
```
