# 许可证与第三方二进制

本文件记录源码与分发风险，不把仓库可下载等同于全部组件可无条件重新分发。

## 代码来源

- 根工程来自 WinlatorMali `fbf42d2`，根 `LICENSE` 为 MIT（BrunoSX 2023），版权与许可原文保留。其历史继承的各个组件仍受各自许可证约束。
- 原版 Winlator 当前顶层为 LGPL-2.1；不能仅根据 Mali 分支根 MIT 文件推断所有新旧代码均可改为 MIT。未来同步新代码需逐项核查。
- GameNative 当前为 GPL-3.0，另外明确列出闭源组件。本工程仅研究其设计，没有复制其 GPL 性能采样实现，也没有打包其 `libredirect.so`、`libsteambootstrap.so`、`libkgslshim.so`。
- Playport 为 GPL-3.0-or-later 并有 Apple 库例外；本工程没有导入其 iOS 实现。
- Mesa/Wrapper 由多个许可证组成，主要核心为 MIT，按文件版权与许可证判断。
- OpenXR-SDK、libadrenotools、liblinkernsbypass、patchelf、Gladio renderer 等源码随本工程保留其许可文件；不能用应用根许可证覆盖它们。

## 已带入的预编译资产

Wine/Proton、Box64、FEX、DXVK、Wrapper/Mesa、音频库、Prefix 模板、Winetricks、Windows 辅助程序和多个图形兼容资源来自上游 WinlatorMali 仓库及 Winlator-Extras 的 `Sd` 发布。Wine 通常为 LGPL，FEX/Box64/DXVK 的上游主要为 MIT，patchelf 为 GPL；实际二进制的补丁和嵌入组件需根据对应版本核对。

`app/src/main/jniLibs` 中存在两组 PulseAudio 等音频预编译库，包括 `libgn_*` 名称。名称来自 GameNative 不代表它就是其专有 redirect：这些是上游音频库组，默认已选普通 PulseAudio。仍需保留 LGPL 等通知和取得确切对应源码。

imagefs 中发现 `usr/lib/libfile_redirect_hook.so`；libadrenotools 子模块具有该 hook 的构建源码。这与 GameNative 的专有 `libredirect.so` 不是同名组件。文件名对照尚不构成二进制逐字节可复现证明。

以下三份下载包已校验发布方提供的 SHA-256，实际记录见 `artifacts/runtime-downloads.json`：

| 文件 | SHA-256 |
| --- | --- |
| imagefs.tzst | `0c8c6c1c7653766323378f3294be6618392075bd354d50b2416a534a6e231620` |
| proton-10-arm64ec.tzst | `d00ff472fb2ee9fe1bfe1a6e7b28d012e7f854b1ed10596aaa702295e8410548` |
| proton-10-arm64ec_container_pattern.tzst | `514fb9e0301d49194d904ea62d9d065649c014da1601a0a133dfaff756e1d79c` |

摘要用于来源记录与完整性检查，不代替授权，也不是第三方发布的独立安全审计。

## 当前交付范围

提供本地个人开发 APK 和可构建前端/JNI 源码，不宣称已完成所有预编译包的对应源码归档、全组件许可证审计或完整从源码重建。没有发布到商店或公共下载渠道。公开再分发前，必须补全全部随包组件的通知、补丁来源、GPL/LGPL 对应源码及必要的重链接材料；不具备这些条件的资产需替换或从发行包中移除。

未加入任何需购买授权的商业兼容库；未复制 GameNative 已明示专有的辅助组件。若上游压缩包的进一步审计发现不明确或受限资产，应停止分发该资产并替换，不能以“个人修改”推导重新分发许可。

## 0.1.8实验Mali兼容层

新增leegao/compat_layer（MIT），固定源码树22960fb33a91226a9ab615288b0846ef7eac456b。Android层由NDK本地编译，不执行下载脚本；源码/依赖文件锁在assets/mali_compat/dimensity-source-lock.json，MIT通知在同目录LICENSE。该层使用上游仓库附带的Android ARM64 SPIRV-Tools静态库，后者Apache-2.0及其依赖通知/对应源码仍需在公开分发前补全；当前个人开发包不冒充完整许可证审计或完全从源码构建。详见mali-compatibility-2026-10-21.md。

## Vortek及源码仓库

Vortek客户端固定b1730c5def9b575672e671aee11d79ae7adc63d1，Android服务端来自Winlator app a030f552f452158a2db64fdb32b490fa19c0b48d，LGPL-2.1，通知与许可证保留于app/src/main/cpp/vortekrenderer及assets/vortek。新增Java桥接类同按LGPL-2.1提供；Khronos头保留自身Apache-2.0通知。客户端移植由prepare-vortek-client.mjs复现，服务端改动随源码公开。

公开源码仓库排除所有预编译运行包、游戏、APK、工具缓存和私有交接记录。运行资产仅记录原始固定来源、摘要及构建入口，不以本仓库代码许可证覆盖第三方资产。
