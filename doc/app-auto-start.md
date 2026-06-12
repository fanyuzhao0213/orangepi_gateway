================================================================

&#x20;        APP 开机自启动配置文档

================================================================



文档版本 : v1.0

编写日期 : 2026-06-12

适用系统 : Orange Pi Linux (Armbian/Debian)

应用名称 : APP\_orangepi

服务文件 : app-orangepi.service

方案选择 : systemd（主流方案）



================================================================

目录

================================================================

一、方案选择

二、操作流程

三、服务配置文件详解

四、常用管理命令

五、快速操作清单



================================================================

一、方案选择

================================================================



当前 Linux 系统的主流方案是 systemd，Orange Pi 官方镜像默认使用此方案。



对比项            rc.local（旧方案）        systemd（推荐）

\----------------------------------------------------------------

系统支持          新版系统默认禁用          开箱即用

依赖管理          无法感知网络等资源        精确控制启动时机

进程守护          无                        支持崩溃自动重启

日志追溯          无                        journalctl 统一管理

权限控制          仅 root                   可指定任意用户运行



验证系统是否使用 systemd：



&#x20;   ps -p 1 -o comm=

&#x20;   # 输出为 systemd 即正确



================================================================

二、操作流程

================================================================



────────────────────────────────────────

步骤 1：准备 APP 可执行文件

────────────────────────────────────────



&#x20;   # 赋予执行权限

&#x20;   chmod +x /home/orangepi/fyz\_test/APP\_orangepi



&#x20;   # （可选）复制到系统标准路径

&#x20;   sudo cp /home/orangepi/fyz\_test/APP\_orangepi /usr/local/bin/



────────────────────────────────────────

步骤 2：创建 systemd 服务文件  //systemd 翻译为系统守护进程

────────────────────────────────────────



&#x20;   sudo nano /etc/systemd/system/app-orangepi.service



服务文件完整内容：



\[Unit]

\# ── 服务基本描述，显示在 systemctl status 输出中 ──

Description=APP\_orangepi Service



\# ── 文档链接，方便维护人员查阅 ──

Documentation=https://github.com/your-org/app-orangepi



\# ── 启动顺序：在 network-online.target 之后启动 ──

\# network-online.target 表示网络已完全就绪（有 IP、可通信）

\# 确保 APP 启动时网络已可用

After=network-online.target



\# ── 弱依赖：尝试激活 network-online.target，但不强制 ──

\# 与 After 配合使用，即使网络未就绪也不会阻止 APP 启动

Wants=network-online.target



\# ── 重启限制时间窗口（秒） ──

\# 在 60 秒内统计重启次数，超过 StartLimitBurst 次则停止重启

StartLimitIntervalSec=60



\# ── 时间窗口内允许的最大重启次数 ──

\# 60 秒内重启超过 3 次，systemd 放弃重启，防止死循环崩溃

StartLimitBurst=3



\[Service]

\# ── 进程类型 ──

\# simple：ExecStart 启动的进程就是主进程，最常用

\# forking：程序会 fork 子进程；oneshot：一次性任务

Type=simple



\# ── 运行用户：以 root 身份运行 APP ──

\# 生产环境建议改为普通用户（如 orangepi）提升安全性

User=root

\# ── 运行用户组 ──

Group=root



\# ── 工作目录：APP 启动后的当前目录 ──

\# APP 内部使用相对路径时，基于此目录解析

WorkingDirectory=/home/orangepi/fyz\_test



\# ── 启动命令：必须使用完整绝对路径 ──

\# systemd 不会解析 PATH，所以不能写 ./APP\_orangepi

ExecStart=/home/orangepi/fyz\_test/APP\_orangepi



\# ── 环境变量：注入 PATH，确保 APP 内部调用外部命令时能找到 ──

Environment="PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"



\# ── 重启策略：任何原因退出都自动重启 ──

\# always：无论正常退出还是崩溃，都重启

\# on-failure：仅崩溃时重启（退出码非 0）

Restart=always



\# ── 重启前等待时间（秒） ──

\# 避免 APP 崩溃后立即重启形成高频循环，等 5 秒再拉起

RestartSec=5





\[Install]

\# ── 安装目标：multi-user.target 对应传统的运行级别 3 ──

\# 即多用户、无图形界面模式，系统正常启动后此 target 激活

\# systemctl enable 时会在此 target 下创建符号链接

WantedBy=multi-user.target

────────────────────────────────────────

步骤 3：启用并启动服务

────────────────────────────────────────



&#x20;   # 重新加载 systemd 配置（修改服务文件后必须执行）

&#x20;   sudo systemctl daemon-reload



&#x20;   # 设置开机自启

&#x20;   sudo systemctl enable app-orangepi.service



&#x20;   # 立即启动（无需重启即可测试）

&#x20;   sudo systemctl start app-orangepi.service



&#x20;   # 查看服务状态

&#x20;   sudo systemctl status app-orangepi.service



预期输出（正常状态）：



&#x20;   ● app-orangepi.service - APP\_orangepi Service

&#x20;        Loaded: loaded (/etc/systemd/system/app-orangepi.service; enabled)

&#x20;        Active: active (running) since ...



────────────────────────────────────────

步骤 4：验证开机自启

────────────────────────────────────────



&#x20;   # 查看是否已启用（输出应为 enabled）

&#x20;   sudo systemctl is-enabled app-orangepi.service



&#x20;   # 列出所有已启用服务，确认 APP 在列

&#x20;   systemctl list-unit-files --type=service --state=enabled | grep app



&#x20;   # 重启验证

&#x20;   sudo reboot



&#x20;   # 重启后检查

&#x20;   sudo systemctl status app-orangepi.service



================================================================

三、服务配置文件详解

================================================================



────────────────────────────────────────

\[Unit] 段落 — 服务描述与依赖

────────────────────────────────────────



字段                    说明                              示例值

\----------------------------------------------------------------

Description             服务描述                          APP\_orangepi Service

Documentation           文档链接                          https://github.com/...

After                   在指定服务之后启动                network-online.target

Wants                   弱依赖，尝试激活但不强求          network-online.target

StartLimitIntervalSec   重启统计时间窗口（秒）            60

StartLimitBurst         时间窗口内最大重启次数            3



────────────────────────────────────────

\[Service] 段落 — 运行配置

────────────────────────────────────────



字段                说明                                  示例值

\----------------------------------------------------------------

Type                进程类型                              simple

User / Group        运行用户及用户组                      root

WorkingDirectory    工作目录                              /home/orangepi/fyz\_test

ExecStart           启动命令（必须完整路径）              /home/orangepi/fyz\_test/APP\_orangepi

Environment         环境变量                              PATH=/usr/local/bin:...

Restart             重启策略                              always

RestartSec          重启前等待时间（秒）                  5



────────────────────────────────────────

\[Install] 段落 — 安装配置

────────────────────────────────────────



字段          说明                        示例值

\----------------------------------------------------------------

WantedBy      服务安装的目标运行级别      multi-user.target



────────────────────────────────────────

常用 Type 类型

────────────────────────────────────────



Type 值     说明                              适用场景

\----------------------------------------------------------------

simple      ExecStart 进程就是主进程          大多数 APP

forking     程序 fork 子进程后父进程退出      传统守护进程（如 httpd）

oneshot     一次性任务，执行完即退出          初始化脚本

dbus        等待 D-Bus 名称获取后启动         D-Bus 服务

notify      APP 发送就绪通知后视为启动完成    需等待初始化的服务



────────────────────────────────────────

Restart 重启策略

────────────────────────────────────────



Restart 值      行为

\----------------------------------------------------------------

no              不自动重启（默认）

on-success      正常退出时重启（退出码 0）

on-failure      异常退出时重启（非 0 退出码）

on-abnormal     信号终止 / 超时 / 看门狗时重启

always          任何原因退出都重启（推荐生产环境）



================================================================

四、常用管理命令

================================================================



────────────────────────────────────────

服务管理

────────────────────────────────────────



操作            命令

\----------------------------------------------------------------

启动 APP        sudo systemctl start   app-orangepi.service

停止 APP        sudo systemctl stop    app-orangepi.service

重启 APP        sudo systemctl restart app-orangepi.service

重载配置        sudo systemctl reload  app-orangepi.service

查看状态        sudo systemctl status  app-orangepi.service



────────────────────────────────────────

开机自启管理

────────────────────────────────────────



操作            命令

\----------------------------------------------------------------

启用开机自启    sudo systemctl enable     app-orangepi.service

禁用开机自启    sudo systemctl disable    app-orangepi.service

查看是否启用    sudo systemctl is-enabled app-orangepi.service



────────────────────────────────────────

日志查看

────────────────────────────────────────



操作                命令

\----------------------------------------------------------------

实时查看日志        sudo journalctl -u app-orangepi.service -f

查看最近 50 行      sudo journalctl -u app-orangepi.service -n 50

查看全部日志        sudo journalctl -u app-orangepi.service

查看今天日志        sudo journalctl -u app-orangepi.service --since today



────────────────────────────────────────

调试命令

────────────────────────────────────────



操作                命令

\----------------------------------------------------------------

查看服务依赖        systemctl list-dependencies app-orangepi.service

查看所有启用服务    systemctl list-unit-files --type=service --state=enabled

重新加载 systemd    sudo systemctl daemon-reload



================================================================

五、快速操作清单

================================================================



首次部署按此顺序执行：



序号  操作              命令                                              备注

\----------------------------------------------------------------------

1     赋予执行权限      chmod +x APP\_orangepi

2     创建服务文件      sudo nano /etc/systemd/system/app-orangepi.service  参见第二节内容

3     重载 systemd      sudo systemctl daemon-reload                      修改服务文件后必须执行

4     启用开机自启      sudo systemctl enable app-orangepi.service

5     立即启动服务      sudo systemctl start  app-orangepi.service

6     验证运行状态      sudo systemctl status app-orangepi.service        确认 active (running)

7     重启验证          sudo reboot                                       重启后再次查看状态



================================================================

&#x20;                   — 内部技术文档 —

================================================================







