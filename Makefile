# ============================================================
# Orange Pi Zero 2W 交叉编译 Makefile
# Ubuntu PC: 192.168.1.143
# Orange Pi: 192.168.1.125
# ============================================================

# ============================================================
# 交叉编译工具链
# ============================================================
CROSS_COMPILE = aarch64-none-linux-gnu-
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
CFLAGS = -Wall -O2 -I. -I./app -I./hal -I./service -I./business -I./network
LDFLAGS = -lm

# ============================================================
# 项目目录结构
# ============================================================
# 定义所有模块目录
MODULE_DIRS = app hal service business network
# 以后添加新模块只需添加到这个列表，例如：
# MODULE_DIRS = app hal gpio uart i2c spi

# ============================================================
# 源文件自动查找
# ============================================================
# 自动查找所有模块的源文件
SOURCES = $(foreach dir, $(MODULE_DIRS), $(wildcard $(dir)/*.c))

# 自动生成目标文件名列表
OBJECTS = $(SOURCES:.c=.o)

# 最终目标文件名
TARGET = Gate_orangepi

# ============================================================
# 编译规则
# ============================================================

# 默认目标：编译所有
all: $(TARGET)

# 链接所有目标文件
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "✅ 编译完成: $(TARGET)"
	@echo "📦 目标文件大小:"
	@$(CROSS_COMPILE)size $(TARGET)

# 通用编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# 清理规则
# ============================================================

# 清理
clean:
	rm -f $(TARGET) $(OBJECTS)
	@echo "✅ 清理完成"

# 深度清理
distclean: clean
	rm -f *.o *.d *.bak *~
	@echo "✅ 深度清理完成"

# ============================================================
# Orange Pi 传输和运行
# ============================================================

PI_USER = orangepi
PI_IP = 192.168.1.125
PI_PATH = /home/$(PI_USER)/fyz_test/

# 传输程序到 Orange Pi
deploy: $(TARGET)
	@echo "📤 传输 $(TARGET) 到 $(PI_USER)@$(PI_IP):$(PI_PATH)"
	scp $(TARGET) $(PI_USER)@$(PI_IP):$(PI_PATH)

# 传输并运行程序
run: deploy
	@echo "🚀 在 Orange Pi 上运行 $(TARGET)"
	ssh $(PI_USER)@$(PI_IP) "cd $(PI_PATH) && ./$(TARGET)"

# 带参数运行
run-args: deploy
	@echo "🚀 在 Orange Pi 上运行 $(TARGET) 参数: $(ARGS)"
	ssh $(PI_USER)@$(PI_IP) "cd $(PI_PATH) && ./$(TARGET) $(ARGS)"

# 持续运行
run-forever: deploy
	@echo "🔄 在 Orange Pi 上持续运行 $(TARGET)"
	ssh $(PI_USER)@$(PI_IP) "cd $(PI_PATH) && while true; do ./$(TARGET); sleep 1; done"

# ============================================================
# 调试和开发辅助
# ============================================================

# 查看工具链版本
version:
	$(CC) --version

# 查看生成的文件信息
info: $(TARGET)
	@echo "📄 文件信息:"
	file $(TARGET)
	@echo ""
	@echo "📊 段大小:"
	$(CROSS_COMPILE)size $(TARGET)

# 查看依赖关系
deps:
	@echo "📂 源文件列表:"
	@echo "$(SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo "📂 目标文件列表:"
	@echo "$(OBJECTS)" | tr ' ' '\n'

# 在 Orange Pi 上执行任意命令
cmd:
	@echo "🔧 在 Orange Pi 上执行: $(CMD)"
	ssh $(PI_USER)@$(PI_IP) "$(CMD)"

# 查看 Orange Pi 系统信息
pi-info:
	@echo "🔍 Orange Pi 系统信息:"
	ssh $(PI_USER)@$(PI_IP) "uname -a && cat /etc/os-release"

# 清理 Orange Pi 上的旧文件
pi-clean:
	@echo "🧹 清理 Orange Pi 上的旧文件"
	ssh $(PI_USER)@$(PI_IP) "cd $(PI_PATH) && rm -f $(TARGET)"

# ============================================================
# systemd 服务部署 (推荐生产用法)
# ============================================================

# 完整部署: 编译 → 传输 → 安装并启动服务
deploy-service: $(TARGET) deploy
	@echo "📦 安装 systemd 服务单元"
	ssh $(PI_USER)@$(PI_IP) "sudo cp /etc/systemd/system/gate-orangepi.service /tmp/gate-orangepi.service.bak 2>/dev/null || true"
	ssh $(PI_USER)@$(PI_IP) "sudo tee /etc/systemd/system/gate-orangepi.service > /dev/null" < service/gate-orangepi.service
	ssh $(PI_USER)@$(PI_IP) "sudo chmod 644 /etc/systemd/system/gate-orangepi.service"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl daemon-reload"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl enable gate-orangepi.service"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl restart gate-orangepi.service"
	@sleep 2
	@echo ""
	@echo "✅ 部署完成"
	@echo ""
	@echo "查看状态:  ssh $(PI_USER)@$(PI_IP) 'sudo systemctl status gate-orangepi.service'"
	@echo "查看日志:  ssh $(PI_USER)@$(PI_IP) 'sudo journalctl -u gate-orangepi.service -f'"
	@echo "停止服务:  ssh $(PI_USER)@$(PI_IP) 'sudo systemctl stop gate-orangepi.service'"
	@echo "取消自启:  ssh $(PI_USER)@$(PI_IP) 'sudo systemctl disable gate-orangepi.service'"

# 仅安装服务单元(不编译、不重启)
install-service:
	@echo "📦 安装 systemd 服务单元到 Orange Pi"
	ssh $(PI_USER)@$(PI_IP) "sudo tee /etc/systemd/system/gate-orangepi.service > /dev/null" < service/gate-orangepi.service
	ssh $(PI_USER)@$(PI_IP) "sudo chmod 644 /etc/systemd/system/gate-orangepi.service"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl daemon-reload"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl enable gate-orangepi.service"
	@echo "✅ 服务单元已安装,运行 'make deploy-service' 部署并启动"

# 重启服务
service-restart:
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl restart gate-orangepi.service"

# 查看服务状态
service-status:
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl status gate-orangepi.service --no-pager -l"

# 查看服务日志(最近 50 行)
service-logs:
	ssh $(PI_USER)@$(PI_IP) "sudo journalctl -u gate-orangepi.service -n 50 --no-pager"

# 实时跟踪服务日志
service-logs-f:
	ssh -t $(PI_USER)@$(PI_IP) "sudo journalctl -u gate-orangepi.service -f"

# 卸载服务
service-uninstall:
	@echo "🗑️  卸载 systemd 服务"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl stop gate-orangepi.service || true"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl disable gate-orangepi.service || true"
	ssh $(PI_USER)@$(PI_IP) "sudo rm -f /etc/systemd/system/gate-orangepi.service"
	ssh $(PI_USER)@$(PI_IP) "sudo systemctl daemon-reload"
	@echo "✅ 服务已卸载"

# ============================================================
# 帮助信息
# ============================================================

help:
	@echo "=========================================="
	@echo "Orange Pi Zero 2W 交叉编译框架"
	@echo "=========================================="
	@echo "项目结构:"
	@echo "  app/         - 应用程序 (main.c)"
	@echo "  service/     - 服务层 + systemd 单元模板"
	@echo "  scripts/     - 部署/运维脚本"
	@echo "  (添加新模块只需在 MODULE_DIRS 中添加)"
	@echo "=========================================="
	@echo "编译命令:"
	@echo "  make                  - 编译程序"
	@echo "  make clean            - 清理"
	@echo "  make distclean        - 深度清理"
	@echo "=========================================="
	@echo "部署与运行(临时):"
	@echo "  make deploy           - 传输到 Orange Pi"
	@echo "  make run              - 传输并运行(前台)"
	@echo "  make run-args ARGS='-d' - 带参数运行"
	@echo "  make pi-clean         - 清理 Orange Pi 上的旧文件"
	@echo "=========================================="
	@echo "systemd 服务部署(推荐生产用法):"
	@echo "  make deploy-service   - 编译 + 传输 + 安装 + 启动 + 自启"
	@echo "  make install-service  - 仅安装服务单元"
	@echo "  make service-status   - 查看服务状态"
	@echo "  make service-logs     - 查看最近 50 行日志"
	@echo "  make service-logs-f   - 实时跟踪日志"
	@echo "  make service-restart  - 重启服务"
	@echo "  make service-uninstall- 卸载服务"
	@echo "=========================================="
	@echo "其他:"
	@echo "  make version          - 查看工具链版本"
	@echo "  make pi-info          - 查看 Orange Pi 系统信息"
	@echo "  make cmd CMD='命令'   - 在 Orange Pi 上执行命令"
	@echo "=========================================="


