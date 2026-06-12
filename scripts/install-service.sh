#!/bin/bash
# ==============================================================================
# Gate_orangepi systemd 服务一键安装脚本
# ==============================================================================
# 用法:
#   chmod +x scripts/install-service.sh
#   sudo ./scripts/install-service.sh                    # 完整安装
#   sudo ./scripts/install-service.sh --uninstall        # 卸载
#   sudo ./scripts/install-service.sh --restart          # 重启服务
#   sudo ./scripts/install-service.sh --status           # 查看状态
#
# 适用系统: Armbian / Ubuntu / Debian(带 systemd)
# ==============================================================================

set -e

# ============== 配置 ==============
SERVICE_NAME="gate-orangepi"
SERVICE_SRC="$(cd "$(dirname "$0")/.." && pwd)/service/${SERVICE_NAME}.service"
SERVICE_DST="/etc/systemd/system/${SERVICE_NAME}.service"
RUN_USER="orangepi"
APP_DIR="/home/${RUN_USER}/fyz_test"
APP_BIN="${APP_DIR}/Gate_orangepi"

# ============== 颜色输出 ==============
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ============== 前置检查 ==============
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "请使用 root 权限运行: sudo $0 $*"
    fi
}

check_systemd() {
    if ! command -v systemctl &> /dev/null; then
        error "未检测到 systemctl,本脚本仅支持 systemd 系统"
    fi
    if ! pidof systemd &> /dev/null; then
        error "当前 PID 1 不是 systemd,本脚本仅支持 systemd 系统"
    fi
}

check_files() {
    if [[ ! -f "$SERVICE_SRC" ]]; then
        error "找不到 service 文件: $SERVICE_SRC"
    fi
    if [[ ! -x "$APP_BIN" ]]; then
        warn "可执行文件不存在或无执行权限: $APP_BIN"
        warn "  请先编译并部署: make && make deploy"
    fi
}

check_user() {
    if ! id "$RUN_USER" &> /dev/null; then
        warn "用户 ${RUN_USER} 不存在,创建中..."
        useradd -m -s /bin/bash "$RUN_USER"
        info "已创建用户: ${RUN_USER}"
    fi
}

# ============== 安装 ==============
do_install() {
    info "=== 安装 Gate_orangepi 服务 ==="
    check_root "$@"
    check_systemd
    check_files
    check_user

    # 1. 复制 service 文件
    info "复制 service 文件到 ${SERVICE_DST}"
    cp "$SERVICE_SRC" "$SERVICE_DST"
    chmod 644 "$SERVICE_DST"

    # 2. 重新加载 systemd
    info "重新加载 systemd 配置"
    systemctl daemon-reload

    # 3. 启用开机自启
    info "启用开机自启"
    systemctl enable "${SERVICE_NAME}.service"

    # 4. 立即启动
    info "启动服务"
    systemctl restart "${SERVICE_NAME}.service"

    # 5. 等待 2 秒后查看状态
    sleep 2

    if systemctl is-active --quiet "${SERVICE_NAME}.service"; then
        info "✅ 服务已成功启动"
        echo ""
        info "服务状态:"
        systemctl status "${SERVICE_NAME}.service" --no-pager -l || true
        echo ""
        info "常用命令:"
        echo "  查看状态: sudo systemctl status ${SERVICE_NAME}.service"
        echo "  查看日志: sudo journalctl -u ${SERVICE_NAME}.service -f"
        echo "  停止服务: sudo systemctl stop ${SERVICE_NAME}.service"
        echo "  重启服务: sudo systemctl restart ${SERVICE_NAME}.service"
        echo "  取消自启: sudo systemctl disable ${SERVICE_NAME}.service"
    else
        warn "❌ 服务启动失败,查看日志:"
        journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager
        return 1
    fi
}

# ============== 卸载 ==============
do_uninstall() {
    info "=== 卸载 Gate_orangepi 服务 ==="
    check_root "$@"

    if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
        info "停止服务"
        systemctl stop "${SERVICE_NAME}.service"
    fi

    if systemctl is-enabled --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
        info "禁用开机自启"
        systemctl disable "${SERVICE_NAME}.service"
    fi

    if [[ -f "$SERVICE_DST" ]]; then
        info "删除 service 文件"
        rm -f "$SERVICE_DST"
        systemctl daemon-reload
    fi

    info "✅ 卸载完成(应用文件保留在 ${APP_DIR})"
}

# ============== 重启 ==============
do_restart() {
    info "=== 重启服务 ==="
    check_root "$@"
    systemctl restart "${SERVICE_NAME}.service"
    sleep 2
    if systemctl is-active --quiet "${SERVICE_NAME}.service"; then
        info "✅ 服务已重启"
    else
        warn "❌ 重启后服务未运行,查看日志:"
        journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager
        return 1
    fi
}

# ============== 状态 ==============
do_status() {
    echo "=== 服务状态 ==="
    systemctl status "${SERVICE_NAME}.service" --no-pager -l || true
    echo ""
    echo "=== 最近 20 行日志 ==="
    journalctl -u "${SERVICE_NAME}.service" -n 20 --no-pager || true
}

# ============== 入口 ==============
case "${1:-install}" in
    install|"")
        do_install "$@"
        ;;
    uninstall|remove)
        do_uninstall "$@"
        ;;
    restart)
        do_restart "$@"
        ;;
    status)
        do_status
        ;;
    *)
        echo "用法: $0 {install|uninstall|restart|status}"
        echo ""
        echo "  install    安装并启用服务(默认)"
        echo "  uninstall  停止并删除服务"
        echo "  restart    重启服务"
        echo "  status     查看状态与日志"
        exit 1
        ;;
esac
