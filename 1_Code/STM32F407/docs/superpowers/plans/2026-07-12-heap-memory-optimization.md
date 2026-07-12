<!--
SPDX-License-Identifier: MIT
Origin: Generated from the approved STM32F407 heap memory optimization design.
Created-By: gpt-5.5
Signed-off-by: czstara12
-->

# STM32F407 主堆内存保守优化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 保守收缩模块资源并将长期常驻的 CPU-only 线程栈静态迁入 CCM，使 SRAM1 主堆至少增加 18 KiB。

**Architecture:** 保留所有 DMA 可见缓冲于 SRAM1；通过 UFFS/LVGL 配置收缩释放 SRAM1，再使用显式 `.ccm.cpu` 属性承载静态线程栈。每轮以 ELF/map 地址和区域容量断言验证。

**Tech Stack:** C99、RT-Thread 5.1、GNU Arm Embedded Toolchain、SCons、GNU ELF/map 工具。

## Global Constraints

- 回复、注释和提交说明使用中文。
- 不安装任何工具，不处理 Flash 优化，不修改 DMA 缓冲位置。
- 新文件包含 SPDX、Origin、Created-By 和签名元数据。
- CCM 仅静态分配，最终保留 1～2 KiB。
- 保留现有线程栈大小和业务功能。

---

### Task 1: 建立内存布局回归断言

**Files:**
- Test: `rt-thread.elf`
- Test: `rt-thread.map`

**Interfaces:**
- Consumes: 当前 ELF/map 基线。
- Produces: 可重复执行的主堆、CCM 和 DMA 地址验证命令。

- [ ] **Step 1: 运行目标状态断言并确认失败**

运行一段只读 shell 检查，要求当前主堆至少为 `52640` 字节、CCM 剩余介于 `1024` 和 `2048` 字节，并要求已知 DMA 符号地址位于 SRAM1。预期当前因主堆仅 `34208` 字节而失败。

- [ ] **Step 2: 保存基线数值**

运行 `arm-none-eabi-size -A rt-thread.elf` 和 `arm-none-eabi-nm -S --size-sort rt-thread.elf`，记录区域结束地址和目标符号。

### Task 2: 保守收缩 UFFS 和 LVGL 资源

**Files:**
- Modify: `packages/uffs-latest/uffs_config.h`
- Modify: `applications/lvgl/lv_port_disp.c`

**Interfaces:**
- Consumes: UFFS 编译期资源宏和 LVGL 单缓冲配置。
- Produces: 8 个页面缓冲、8 个块缓存、32 个文件对象、6 个目录对象和 20 行 LVGL 缓冲。

- [ ] **Step 1: 运行配置断言并确认失败**

使用 `rg` 检查目标宏值和 `PKG_ST_7789_HEIGHT*20`；预期因当前值仍为 10、50、10 和 30 而失败。

- [ ] **Step 2: 修改最小配置**

仅修改四个 UFFS 宏和 LVGL 行数，不改变算法、接口或 DMA 属性。

- [ ] **Step 3: 构建并核对收益**

运行 `scons -j4`，然后读取 ELF/map，确认 SRAM1 静态占用下降且 DMA 缓冲仍在 SRAM1。

### Task 3: 将内核和组件静态线程栈迁入 CCM

**Files:**
- Modify: `rt-thread/src/idle.c`
- Modify: `rt-thread/src/components.c`
- Modify: `rt-thread/components/finsh/shell.c`
- Modify: `rt-thread/components/drivers/sdio/mmcsd_core.c`

**Interfaces:**
- Consumes: `rt_thread_init()` 和固定大小线程栈。
- Produces: `.ccm.cpu` 中的 idle、system、main、FinSH 和 MMCSD 静态栈。

- [ ] **Step 1: 运行符号地址断言并确认失败**

用 `arm-none-eabi-nm` 要求 `idle_thread_stack`、`main_thread_stack`、`finsh_thread_stack` 和 `mmcsd_stack` 位于 `0x10000000`～`0x1000FFFF`；预期当前至少 idle、MMCSD 不满足且 main、FinSH 不存在。

- [ ] **Step 2: 使用静态线程路径**

为 GCC 构建显式声明 `.ccm.cpu` 静态栈；main 和 FinSH 在启用系统堆时仍改用 `rt_thread_init()`，但保留 FinSH shell 结构的既有动态分配行为。

- [ ] **Step 3: 构建并核对 CCM 容量**

运行 `scons -j4`，确认目标栈进入 CCM、链接未溢出，并记录 CCM 剩余空间。

### Task 4: 将应用常驻线程改为 CCM 静态线程

**Files:**
- Modify: `applications/sd_card_thread.c`
- Modify: `applications/runtime_load_indicator.c`

**Interfaces:**
- Consumes: 原线程入口、优先级、栈大小和时间片。
- Produces: 通过 `rt_thread_init()` 启动的 SD 卡监测和负载指示静态线程。

- [ ] **Step 1: 运行符号断言并确认失败**

要求两个新静态栈符号存在且位于 CCM；预期因符号尚不存在而失败。

- [ ] **Step 2: 最小化改为静态线程**

增加静态控制块和 `.ccm.cpu` 栈，保留原错误返回、入口函数、优先级、时间片和栈大小。

- [ ] **Step 3: 构建并检查容量**

运行 `scons -j4`；若 CCM 剩余不足 1 KiB，则按优先级撤回最小收益的应用线程迁移；若超过 2 KiB，则进入 Task 5。

### Task 5: 精确填充剩余 CCM 并完成验证

**Files:**
- Modify: 仅限已确认 CPU-only 静态对象所在的现有源文件。
- Test: `rt-thread.elf`
- Test: `rt-thread.map`

**Interfaces:**
- Consumes: Task 4 后 CCM 实际余量。
- Produces: CCM 剩余 1～2 KiB、主堆增量至少 18 KiB的最终布局。

- [ ] **Step 1: 按符号调用链选择对象**

只选择未传入 HAL DMA、CherryUSB、SPI、UART、SDIO 或 NAND 传输接口的静态对象；若 CCM 已在目标余量内则不修改。

- [ ] **Step 2: 运行目标符号地址断言并确认失败**

对选中的精确符号要求其地址进入 CCM，确认修改前失败。

- [ ] **Step 3: 添加显式 `.ccm.cpu` 属性**

一次只迁移一个对象并重新构建，禁止添加宽泛链接匹配规则。

- [ ] **Step 4: 运行完整验证**

执行干净重构建、ELF 分段检查、符号地址检查和 `git diff --check`。确认主堆不少于 `52640` 字节、CCM 剩余 1～2 KiB、所有已知 DMA 缓冲仍位于 SRAM1。

- [ ] **Step 5: 提交实现**

仅暂存本次内存优化涉及的文件，使用中文 Conventional Commit，并包含 `Signed-off-by: czstara12`。
