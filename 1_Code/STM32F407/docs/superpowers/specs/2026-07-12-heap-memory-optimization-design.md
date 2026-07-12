<!--
SPDX-License-Identifier: MIT
Origin: Generated for this repository from ELF/map memory analysis and user-approved constraints.
Created-By: gpt-5.5
Signed-off-by: czstara12
-->

# STM32F407 主堆内存保守优化设计

## 目标

在不删除现有功能、不改造 RT-Thread 全局内存分配器、不处理 Flash 占用的前提下，通过保守收缩模块资源和迁移 CPU-only 静态对象，将 SRAM1 主堆从当前 34,208 字节提高至少 18 KiB。

## 当前基线

- SRAM1：128 KiB，静态占用截至 `0x20017A60`，主堆为 34,208 字节。
- CCM：64 KiB，静态占用 53,584 字节，剩余 11,952 字节。
- DMA 外设不能访问 STM32F407 CCM，因此所有 DMA 源、目的和描述缓冲必须保留在 SRAM1。
- CCM 仅用于静态分配，不注册为动态堆。

## 资源保守收缩

UFFS 配置调整为：

- `MAX_PAGE_BUFFERS`：10 调整为 8；
- `MAX_CACHED_BLOCK_INFO`：10 调整为 8；
- `MAX_OBJECT_HANDLE`：50 调整为 32；
- `MAX_DIR_HANDLE`：10 调整为 6。

LVGL 单绘图缓冲由 30 行调整为 20 行。绘图缓冲仍留在 SRAM1，因为 SPI DMA 会读取该缓冲。

不缩减 USB、UART、SPI、SDIO 和 NAND 页传输缓冲，不改变现有线程栈大小。

## 线程栈迁移

所有已经静态分配、且仅由 CPU 使用的线程栈迁入 `.ccm.cpu`。包括现有 DAP、离线下载、LVGL 栈，以及待迁移的 idle、系统回收和 MMCSD 检测线程栈。

项目中长期存活、当前通过 `rt_thread_create()` 从主堆分配的线程，应在 CCM 容量允许时改用静态线程控制块和静态栈，并通过 `rt_thread_init()` 创建。候选包括：

- main 线程；
- FinSH 线程；
- SD 卡监测线程；
- 运行负载指示线程；
- 当前配置实际启用的 SDIO IRQ 或 USB OSAL 常驻线程。

迁移顺序按“长期常驻、栈较大、无 DMA 指针”排序。若全部候选无法装入 CCM，则以 CCM 保留 1～2 KiB 安全余量为上限停止迁移。第三方组件可能动态创建的短生命周期线程继续使用主堆，不修改 `rt_thread_create()` 或 RT-Thread 全局分配器。

## 其他静态对象迁移

完成资源收缩和线程栈迁移后，如 CCM 仍有超过 2 KiB 空间，再迁移经过调用链确认不参与 DMA 的 CPU-only 静态对象。优先级为：

1. DFS 管理表和纯软件状态表；
2. 路径、解析和 UI 临时数组；
3. 不会传给 HAL、USB、SPI、UART、SDIO 或 NAND 传输接口的工作缓冲。

迁移使用源文件上的显式 `.ccm.cpu` 属性或链接脚本中的精确输入段匹配。禁止使用可能吞入 DMA 对象的宽泛 `.bss.*` 规则。

## 必须保留在 SRAM1 的对象

- LVGL 绘图缓冲；
- UART3 DMA 接收缓冲；
- SPI DMA dummy 缓冲；
- SDIO cache；
- USB 端点及发送缓冲；
- W25N01GV 页缓冲；
- 任何传入 HAL DMA、CherryUSB 控制器或外设 DMA 接口的内存。

## CCM 容量策略

CCM 仅静态分配。每次迁移后重新构建并读取 ELF/map，最终目标为：

- `.TCM + .ccm.cpu` 不超过 62～63 KiB；
- 保留 1～2 KiB，吸收对齐填充和后续小幅增长；
- 不通过缩小线程栈来强行填满 CCM；
- 链接脚本必须让 CCM 溢出在链接阶段失败。

## 验证

每个独立调整都以 map/ELF 检查作为回归测试：

1. 调整前先用检查命令证明目标资源值或目标符号位置不满足设计；
2. 修改后执行 `scons -j4`；
3. 使用 `arm-none-eabi-size -A rt-thread.elf` 核对 SRAM1 与 CCM；
4. 使用 `arm-none-eabi-nm -S rt-thread.elf` 核对线程栈和迁移对象地址；
5. 核对所有已知 DMA 缓冲仍位于 `0x20000000`～`0x2001FFFF`；
6. 主堆必须比基线增加至少 18 KiB；
7. CCM 必须保留 1～2 KiB，构建无区域溢出。

硬件验证应覆盖启动、FinSH、LVGL 刷屏、SD 卡插拔、NAND/UFFS 文件操作、DAPLink、USB-UART 和离线下载流程。
