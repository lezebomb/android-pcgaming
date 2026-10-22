# Phase 0：源码审计与基线

审计日期：2026-10-21。候选仓库均已浅克隆至 `upstream/`；研究副本保留原始提交。参考项目只检出必要文件，WinlatorMali 完整检出并作为根目录工程的来源。

| 项目 | 固定提交 | 实际源码结论 |
| --- | --- | --- |
| Playport | `412de5f8c0bdd26d84e0127eb5776ba992e3b152` | iOS/Mach 单进程 Madeira、Wine ARM64EC、FEX、DXMT；不适合作为 Android APK 基础 |
| GameNative | `929cfcdccce9b398c5a996c0d6693d9410631c77` | Android 游戏启动与采样成熟，但有明确的专有运行辅助组件；作为设计参考 |
| Winlator | `b3d21498076dea3b2306ecbb03d168d77d26cc8f` | 当前顶层将 app、vortek、gladio 拆成子模块；app 已实际检出审计 |
| WinlatorMali | `fbf42d26411444249a301086da0dcb652b861b17` | 完整 Java/XServer/JNI、Mali Wrapper、FEX/Box64、触控手柄链；选为开发基础 |
| Bionic Vulkan Wrapper | `3abf48704a4d9303967a0811c0dc8ff8567d613b` | Mesa 分支，需专用 Termux/Meson 构建；本轮读取底层资源处理，不贸然改动 |

## 实际读取与发现

- [Playport ARCHITECTURE](https://github.com/playportdev/playport/blob/412de5f8c0bdd26d84e0127eb5776ba992e3b152/docs/ARCHITECTURE.md)：Wine 的“进程”在 Mach task 中变成线程，JIT pool 依赖 iOS debugger；Metal/Mach/JIT 机制不可直接移植。借鉴按阶段记录、版本固定与资源所有权。
- GameNative 的 `PerfSampler.kt` 读取可访问 procfs、PSS、热状态，`PerfVerdicts.kt` 将多窗口数据用于诊断。借鉴缺失指标留空、持续状态判断；没有复制其 GPL Kotlin 实现。
- `MainActivity.kt`、`DXVKHelper.java`、`VortekRendererComponent.java`、`FEXCoreManager.java`、`FEXCorePresetManager.java` 均已读取。Vortek 涉及独立原生上下文与图像缓存；FEX 预设会改变 TSO/x87，不能把最激进项作为默认。
- [THIRD_PARTY_NOTICES](https://github.com/utkarshdalal/GameNative/blob/929cfcdccce9b398c5a996c0d6693d9410631c77/THIRD_PARTY_NOTICES)：明确列出专有 `libredirect.so`、`libsteambootstrap.so`、`libkgslshim.so` 等；其说明称去掉 redirect 后 Android 15+ 的游戏兼容性会降低。更改包名不能只改 Gradle applicationId。
- 原版 Winlator 的 `app` 子模块固定 `a030f552f452158a2db64fdb32b490fa19c0b48d`，已读取 `app/build.gradle`：API 35/target 28、NDK 24、Wine/Box64 运行架构。它不直接提供本项目所需的 Mali 保守配置。
- WinlatorMali `app/build.gradle` 原来自动下载 imagefs，但没有固定摘要；SDK/NDK 版本与本机不符。已迁移构建配置、取消构建时隐式大文件下载，改用显式校验脚本。
- `Container.java` 原默认 ASTC 转码、mailbox 和多个未经目标机验证的环境设置；`DXVKConfigDialog.java` 默认忽略图形屏障；已改成保守起点。
- `RamBoosterComponent.java` + `rambooster.cpp` 会分波 mmap 并写入大量匿名内存，意图触发 Android 回收其他进程。这与日用平板目标冲突：组件不再注册，JNI 压力代码不编入 APK，Java 入口固定无操作。
- `GPUImage.java`、`gpu_image.c` 的 AHardwareBuffer/EGL 生命周期存在值得进一步核对的引用计数路径。未连接设备，本轮不把静态疑点作为已实测 Mali 泄漏，也未据此改动图形核心。
- Wrapper 已逐项读取 `wrapper_device_memory.c`、`wrapper_image.c`、`wrapper_device.c`、`wrapper_physical_device.c`、`wsi_common_ahardware_buffer.c`。DMA heap/ION、memory type 选择、BCn 格式替换、AHB swapchain 的资源和同步需一起验证；某些失败分支的 FD 所有权是后续排查候选，尚无目标设备复现。

## 选择依据与边界

选用 WinlatorMali 是因为实际存在可构建 Android 前端、原生 XServer/输入/显示代码与可获取的运行包。本轮根工程已实际构建 APK；没有分别构建或运行所有候选。Wine、FEX、Box64、Mesa 等大量依赖仍使用上游预编译资产，不能将此次构建称为整个运行栈全部从源码重建。

原始副本及未修改 Wrapper 作为回退参考。本轮不声称已修复某项经实机确认的 G615 性能瓶颈；Phase 3 仍需设备数据。
