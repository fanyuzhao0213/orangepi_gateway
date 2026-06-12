# ============================================================
# OrangePi Zero 2W LRADC 按键驱动交叉编译 Makefile
# 内核: Linux 6.1 Sun50iw9 (Rockchip H618)
# 架构: ARM64 (aarch64)
# ============================================================

# 工具链路径
TOOLCHAIN_PATH = /home/fyz/orangepi_2w_linux/orangepi-build-next/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin

# 交叉编译前缀
CROSS_COMPILE = $(TOOLCHAIN_PATH)/aarch64-none-linux-gnu-

# 目标架构
ARCH = arm64

# 内核源码目录
KDIR = /home/fyz/orangepi_2w_linux/orangepi-build-next/kernel/orange-pi-6.1-sun50iw9

# 当前目录
PWD = $(shell pwd)

# OrangePi 配置
ORANGEPI_IP = 192.168.1.125
ORANGEPI_USER = orangepi
ORANGEPI_PATH = /home/orangepi/orangepi_driver

# 驱动目标文件
obj-m := fyz_key.o

# ============================================================
# 编译命令
# ============================================================
all:
	@echo "=========================================="
	@echo "交叉编译 OrangePi Zero 2W LRADC 按键驱动"
	@echo "=========================================="
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

# 编译 APP (应用层程序)
app:
	@echo "=========================================="
	@echo "交叉编译 LRADC 按键测试APP"
	@echo "=========================================="
	$(CROSS_COMPILE)gcc -o fyz_key_test app.c

# 编译并上传驱动+APP
upload-all: upload app
	@echo "=========================================="
	@echo "上传驱动和APP到 OrangePi"
	@echo "=========================================="
	scp fyz_key.ko fyz_key_test $(ORANGEPI_USER)@$(ORANGEPI_IP):$(ORANGEPI_PATH)/
	@echo "上传完成!"

# 编译并上传到 OrangePi
upload:
	@echo "=========================================="
	@echo "编译并上传驱动到 OrangePi"
	@echo "=========================================="
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules
	scp fyz_key.ko $(ORANGEPI_USER)@$(ORANGEPI_IP):$(ORANGEPI_PATH)/
	@echo "上传完成!"

# 上传驱动文件到 OrangePi
push:
	scp fyz_key.ko $(ORANGEPI_USER)@$(ORANGEPI_IP):$(ORANGEPI_PATH)/
	@echo "上传完成!"

# 在 OrangePi 上加载驱动
load:
	ssh $(ORANGEPI_USER)@$(ORANGEPI_IP) "sudo insmod $(ORANGEPI_PATH)/fyz_key.ko"

# 在 OrangePi 上卸载驱动
unload:
	ssh $(ORANGEPI_USER)@$(ORANGEPI_IP) "sudo rmmod fyz_key"

# 运行 APP
run-app:
	ssh $(ORANGEPI_USER)@$(ORANGEPI_IP) "sudo $(ORANGEPI_PATH)/fyz_key_test"

# 查看 OrangePi 上的内核日志
dmesg:
	ssh $(ORANGEPI_USER)@$(ORANGEPI_IP) "dmesg | grep -E 'fyz|lradc'"

# 完整流程：编译+上传+加载+APP
full:
	@echo "=========================================="
	@echo "完整流程：编译+上传+加载+APP"
	@echo "=========================================="
	$(MAKE) upload-all
	$(MAKE) load
	$(MAKE) run-app
	@echo "=========================================="
	@echo "完成!"
	@echo "=========================================="

# 清理编译产物
clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
	rm -f fyz_key_test

.PHONY: all app upload-all upload push load unload run-app dmesg full clean