<!--
SPDX-License-Identifier: MIT
Origin: Generated for this repository from local project structure and build files.
Created-By: gpt-5.5
Signed-off-by: czstara12
-->

# Repository Guidelines

## Project Structure & Module Organization

This repository contains the LCKFB DAPLink Debug Tool hardware and firmware project. The Git root is two levels above this directory and includes `1_Code/`, `2_TF_card_file/`, `3_3D model/`, and `4_docs/`. Firmware projects live in `1_Code/`, with separate RT-Thread BSPs for `GD32F407/` and `STM32F407/`. In this STM32 BSP, application code lives in `applications/`, board ports and linker scripts in `board/`, middleware and vendor libraries in `libraries/`, RT-Thread sources in `rt-thread/`, and third-party packages in `packages/`.

## Build, Test, and Development Commands

Run firmware commands from this directory, `1_Code/STM32F407/`.

- `scons`: builds the STM32 BSP and emits artifacts such as `rt-thread.elf`, `rtthread.bin`, and `rtthread.hex`.
- `scons --target=mdk5`: regenerates Keil MDK5 project files after configuration changes.
- `scons --target=iar`: regenerates IAR project files.
- `menuconfig`: opens RT-Thread configuration in the RT-Thread ENV shell.
- `pkgs --update`: updates RT-Thread package dependencies after changing package options.

For GCC builds, set `RTT_EXEC_PATH` to the directory containing `arm-none-eabi-*` tools when they are not on `PATH`.

## Coding Style & Naming Conventions

Use C99-compatible C for firmware. Follow the existing style: 4-space indentation, lower-case file names, and module prefixes such as `bsp_`, `dap_`, `usb2uart_`, and `ui_`. Keep generated vendor and package code isolated unless a targeted patch is required. Add Doxygen comments for public APIs and module interfaces. New files must start with a standard SPDX header plus `Origin` and `Created-By` metadata.

## Testing Guidelines

There is no standalone unit-test suite. Validate firmware changes by building with `scons` and checking that binary artifacts are produced. For hardware-facing work, flash the STM32F407 board and verify serial output at `115200-8-1-N`. Manually exercise affected DAPLink, USB-UART, LVGL UI, power monitor, PWM, DAC, or offline-download flows.

## Commit & Pull Request Guidelines

Use Conventional Commits in Chinese where possible, for example `fix(build): 修复 GCC 构建兼容性`. Keep each commit focused and buildable. Pull requests should state the affected board or module, list build and hardware verification results, link related issues, and include screenshots or photos for UI, hardware, or enclosure changes.
