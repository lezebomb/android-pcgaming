# Superposition 1.1：启动器适配与场景加载

## 当前结论

原版免费 GUI 已显示，可选择 720p Low 并启动原引擎；引擎显示启动画面后崩溃，尚无 3D 场景或性能通过结论。不使用付费 CLI，不修改原 EXE 或授权逻辑。

旧 `launcher+0x238c2` 访问异常位于显卡枚举/活动适配器选择函数。LLVM 反汇编与字符串包含 `Determine active adapters`、`Active adapters not found`、回退选择显卡及指针写入。空适配器集合触发不安全回退是据此作出的推断，未还原原始源码。

REA 仅识别原文件 SHA256 `5c820fe7d9a2cc45087d628436ca2cd271999734b79988260fadbf6e95f3734b`；反编译引擎未配置，分析会话已关闭。反汇编证据来自已有 LLVM 工具，不标成 REA 反编译结果。

## 最小有效修复

仅对该游戏应用 DXVK 配置：

```ini
dxgi.customVendorId = 10de
dxgi.hideNvidiaGpu = False
```

先前组合检查含临时开源 DXVK-NVAPI v0.9.2 DLL，GUI 存活 30 秒、正常退出。随后移除 DLL 的消融仍能显示 GUI，证明新增 DLL 并非必要；不将 20MB DLL 加入 APK。实际 GPU 名称及功能支持保持运行时报告，不宣称获得 NVIDIA 的硬件能力。

0.1.24 源码提供默认关闭的每游戏「兼容旧启动器的显卡识别」选项，运行时仅向 `DXVK_CONFIG` 追加这两个选项，保留用户的配置文件和其他环境选项。

参考：[DXVK 2.3.1 配置说明](https://github.com/doitsujin/dxvk/blob/v2.3.1/dxvk.conf)、[配置文件及环境变量的合并实现](https://github.com/doitsujin/dxvk/blob/v2.3.1/src/util/config/config.cpp)、[DXVK-NVAPI 主仓库](https://github.com/jp7677/dxvk-nvapi)。

## 证据与未完成项

`artifacts/superposition/`：`launcher-crash-disassembly.txt`、`launcher-crash-function-annotated.txt`、`launch-nvapi-result.txt`、`identity-gui-visible.png`、`selected-low-key.png`、`engine-720p-first.png`、`engine-returned-gui.png`、`engine-720p-log.html`。

第一轮引擎日志止于创建 meshes/shaders/textures，DXVK 日志未见明确失败，未查到对应 PID 的 lmkd 记录；不能推断为内存不足。目前在同一免费 GUI 路径启用 SEH 记录定位具体异常，不重装、不重传资源。

## 11:45：已复现并修复的 wrapper 越界

单次故障引擎 PID21368 的 PC0x71bf03ab50，与同进程映射对应 `libvulkan_wrapper.so` 文件偏移0x110b50 / ELF VA0x111b50。DWARF定位 `patch_OpConstantComposite_to_OpSpecConstantComposite` / spirv_patcher.cpp:87。w19=104736字节，w22=26184字，正好读过输入末尾。输入指令边界可完整走查；这不等于完整SPIR-V语义验证。

公开源码也有相同单位错误：[patcher](https://github.com/GunaCharanTeja/gamenative-wrapper/blob/b8663654f7eacad900d2a23958422c64878fdf79/src/vulkan/wrapper/spirv_patcher.cpp)、[传入codeSize的调用方](https://github.com/GunaCharanTeja/gamenative-wrapper/blob/b8663654f7eacad900d2a23958422c64878fdf79/src/vulkan/wrapper/wrapper_device.c)。调用传字节，patcher直接按uint32索引。源码层的对应修正是进入该函数后将codeSize除以sizeof(uint32_t)。

当前保留精确原库，仅生成独立副本。原SHA256 `3b4c285550847a0caa6d110614298d6c0c57feefd9c96460713ad707a3c8304e`，副本 `6b229b929ee543440eb992c7163cbc4fcbe6765c252af10a97bc47e835be638b`，19072872bytes。两个AArch64指令经NDK汇编核对：文件0x110b04的最小长度改为24字节；0x110b24边界寄存器改为字节数右移2。原BuildID保留，不冒充完整源码重构建。`scripts/prepare-wrapper-bounds-fix.mjs`拒绝其他原始摘要/指令。

**实际平板单样本A/B已通过**：同一104736字节着色器末尾紧邻不可读保护页，直接调用固定库中的原函数。原库SIGSEGV；副本正常返回，全部输入字节未变。`artifacts/wrapper-bounds/guard-result.txt`，`bounds_result=0`。这是解析越界回归检查，不创建Vulkan设备，不作为场景/FPS通过。

0.1.26候选仅在G615实验性Mali兼容已开启时安装并选择独立ICD；原驱动完整保留，未知驱动构建不打补丁，关闭实验选项恢复原ICD。下一步只做一次必要场景短复查。
