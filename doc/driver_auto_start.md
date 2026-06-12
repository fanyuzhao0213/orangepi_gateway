================================================================
        嵌入式 Linux 驱动开机自动加载配置文档
================================================================

文档版本 : v1.0
编写日期 : 2026-06-12
适用系统 : Orange Pi Linux (Armbian/Debian)
驱动列表 : fyz_key.ko / fyz_led.ko / fyz_gpio.ko

================================================================
目录
================================================================
一、概述
二、目录结构说明
三、配置步骤
四、验证方法
五、常用命令速查
六、注意事项

================================================================
一、概述
================================================================

目标：
    实现自定义编写的内核驱动模块（.ko 文件）在系统启动时自动
    加载，无需每次开机后手动执行 insmod。

原理：
    systemd 在启动阶段会读取 /etc/modules-load.d/ 目录下的
    所有 .conf 文件，按文件中列出的模块名依次调用 modprobe
    完成加载。modprobe 与 insmod 的区别在于 modprobe 会自动
    处理模块间的依赖关系。

================================================================
二、目录结构说明
================================================================

驱动模块安装路径：

    /lib/modules/$(uname -r)/kernel/drivers/custom/
    ├── fyz_key.ko          # LRADC 按键驱动
    ├── fyz_led.ko          # LED 驱动
    ├── fyz_gpio.ko         # GPIO 驱动
    └── ...                 # 其他自定义驱动

模块加载配置文件路径：

    /etc/modules-load.d/
    └── fyz-drivers.conf    # 开机自动加载配置

================================================================
三、配置步骤
================================================================

────────────────────────────────────────
步骤 1：将驱动模块安装到系统目录
────────────────────────────────────────

    # 创建自定义驱动目录
    sudo mkdir -p /lib/modules/$(uname -r)/kernel/drivers/custom

    # 将编译好的 .ko 文件复制到该目录
    sudo cp ~/orangepi_driver/*.ko /lib/modules/$(uname -r)/kernel/drivers/custom/

    # 确认文件已复制
    ls /lib/modules/$(uname -r)/kernel/drivers/custom/

────────────────────────────────────────
步骤 2：更新模块依赖关系
────────────────────────────────────────

    sudo depmod -a

    # depmod 会扫描所有 .ko 文件，生成模块依赖数据库
    # 每次新增或删除 .ko 文件后都必须重新执行此命令
    # 依赖信息保存在 /lib/modules/$(uname -r)/modules.dep

────────────────────────────────────────
步骤 3：创建模块加载配置文件
────────────────────────────────────────

    sudo nano /etc/modules-load.d/fyz-drivers.conf

文件内容（模块名不加 .ko 后缀，按依赖顺序从上到下排列）：

    # FYZ Custom Drivers - 开机自动加载
    # 格式：每行一个模块名，# 开头为注释

    fyz_gpio        # 基础 GPIO 驱动，其他驱动依赖它，必须最先加载
    fyz_key         # LRADC 按键驱动
    fyz_led         # LED 驱动

────────────────────────────────────────
步骤 4：手动触发加载（无需重启验证）
────────────────────────────────────────

    # 方式一：重启加载服务（等效于开机加载，推荐）
    sudo systemctl restart systemd-modules-load

    # 方式二：直接用 modprobe 手动加载单个模块（临时测试）
    sudo modprobe fyz_gpio
    sudo modprobe fyz_key
    sudo modprobe fyz_led

────────────────────────────────────────
步骤 5：重启验证
────────────────────────────────────────

    sudo reboot

    # 重启后检查加载状态（见第四节）

================================================================
四、验证方法
================================================================

    # 1. 查看模块是否已加载（有输出即表示加载成功）
    lsmod | grep fyz

    # 2. 查看内核日志，确认 probe 成功
    dmesg | grep -i "fyz"

    # 3. 查看 systemd 模块加载服务日志
    journalctl -u systemd-modules-load --no-pager | tail -20

    # 4. 查看某个模块的详细信息
    modinfo fyz_key
    modinfo fyz_led
    modinfo fyz_gpio

预期输出示例（lsmod）：

    Module                  Size  Used by
    fyz_led                20480  0
    fyz_key                20480  0
    fyz_gpio               20480  0

================================================================
五、常用命令速查
================================================================

操作                        命令
----------------------------------------------------------------
加载单个模块                sudo modprobe fyz_key
卸载单个模块                sudo modprobe -r fyz_key
查看已加载模块              lsmod | grep fyz
查看模块详细信息            modinfo fyz_key
更新模块依赖数据库          sudo depmod -a
查看模块依赖关系            modprobe --show-depends fyz_key
重启加载服务                sudo systemctl restart systemd-modules-load
查看加载服务状态            sudo systemctl status systemd-modules-load
查看加载日志                journalctl -u systemd-modules-load -f
查看内核驱动日志            dmesg | grep -i fyz
列出自定义驱动目录          ls /lib/modules/$(uname -r)/kernel/drivers/custom/

================================================================
六、注意事项
================================================================

1. 加载顺序
   fyz-drivers.conf 中模块按从上到下的顺序加载。
   若 fyz_key 依赖 fyz_gpio，则 fyz_gpio 必须写在前面，
   否则 modprobe 会报 "Unknown symbol" 错误。

2. depmod 必须重新运行
   每次修改（新增/删除/替换）.ko 文件后，都必须执行：
       sudo depmod -a
   否则系统找不到新模块。

3. 内核版本对应
   .ko 文件与内核版本严格绑定，升级内核后需重新编译驱动，
   并重新复制到新版本对应的目录：
       /lib/modules/新版本号/kernel/drivers/custom/

4. 系统自带驱动冲突
   若系统自带驱动（如 sun4i_lradc_keys）与自定义驱动冲突，
   需在 /etc/modprobe.d/ 下创建黑名单文件屏蔽系统驱动：

       sudo nano /etc/modprobe.d/fyz-blacklist.conf
       # 文件内容：
       blacklist sun4i_lradc_keys

5. 模块签名警告
   自编译模块加载时会出现以下警告，属于正常现象，不影响使用：
       loading out-of-tree module taints kernel
       module verification failed: signature and/or required key missing

================================================================
                    — 内部技术文档 —
================================================================