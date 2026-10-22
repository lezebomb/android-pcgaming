# Dimensity Gaming

面向天玑 8300 / Mali-G615 / 8GB 平板的本地 Windows 游戏运行器开发版。以实际审计过的 WinlatorMali 为基础，独立包名 `org.dimensity.gaming.dev`，无需 Root。

已在联想 TB375FC / Android 16 上完成基础 DX9/11 显示、Freedoom 与 C-Dogs 关卡及手柄短测。当前已安装0.1.51-dev。Warzone在原wrapper + 游戏DirectX/ANGLE路线已实际显示第一关地形、车辆与HUD；0.1.48的虚拟手柄检查确认7个单位由初始位置移动到目标点。战斗、长时间稳定性与经校准的帧时间仍待验收。Superposition已显示完整GUI，但引擎仍主动退出；艾尔登法环原版EXE停在客户端接口初始化，尚未达到3A流畅稳定游玩的目标。

FF14官方免费测试程序在0.1.50的对应配置下已实际显示角色编辑器、户外地形建筑和人物材质，菜单点击有效；采样约9–20FPS，不能视作流畅稳定或正式游戏可玩。0.1.51将避开Wine旧式显卡查询崩溃的配置做成可选开关，详见[大型DX11样本记录](docs/ffxiv-benchmark-compatibility.md)。

最近修复了Mali层退出清理、X11焦点请求/通知、鼠标状态字段与松开投递。代码构建、一次新增协议短查、真实游戏结果分别记录于[验证记录](docs/validation.md)。实验Vortek缺少游戏要求的coherent上传类型，当前不能替代wrapper，早期3D结果不视作新版回归通过。

## 使用

1. 本地构建产物位于 `artifacts/DimensityGaming-<版本>.apk`。源码仓库不上传APK或游戏文件。首次解包需预留数 GB 存储空间。
2. 手动安装并打开后等待运行环境初始化，到「容器与 Wine 桌面」创建独立容器。
3. 回主页应用「均衡」，运行 Wine 基础程序 `winecfg.exe` 检查窗口。
4. 使用主页「导入游戏目录」复制合法游戏及 DLL/资源到本应用容器；选择主 EXE 加入游戏库。单文件程序可选「导入 EXE」。
5. 从游戏库直接启动 EXE；长按游戏 →「直接启动设置」可修改 EXE、工作目录、参数及打开应用后自动启动。已有容器内的游戏可在主页添加，无需重新复制；桌面快捷方式可一键进入。游戏内手动选低纹理、低阴影。
6. 游戏退出后在主页检测真实 Vulkan/内存并导出诊断 ZIP。

开发包与原运行器共存。存档在本应用自己的容器中；卸载本开发包前请导出需要保留的内容。不会 Root、刷机、改系统分区、zRAM、Swap、LMKD 或强杀其他应用。

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

首次取得源码而未准备运行包时加 `-FetchRuntime`。需要 JDK、SDK 36、NDK 28.2.13676358、CMake 3.22.1；详情见 [构建与安装](docs/build-plan.md)。脚本只编译；不会自动操作平板。`device.ps1 -Install/-Launch` 必须由操作者主动指定。

公开仓库只保存源码与依赖来源记录。首次构建还需运行 `node scripts/fetch-bundled-assets.mjs` 恢复基线资产；三个大运行包用 `node scripts/fetch-runtime.mjs`。可选Mali层与Vortek客户端分别用 `scripts/build-mali-compat.ps1`、`scripts/build-vortek-client.ps1` 从固定源码构建。资产路径、摘要及来源见 [运行资产清单](docs/runtime-assets.json)。

## 已实现

- 横屏主页、游戏容器、系统文件选择器 EXE/目录导入与诊断 ZIP 导出。
- 三种显式预设，默认 1280×800 / DXVK 1.10.3 / 30 FPS 上限；保持同步与转译正确性。
- 5 秒内存采样、同 UID 进程 RSS/PSS、采样峰值、低内存启动保护和持续预警。
- 原生 Vulkan 设备、版本、扩展、压缩格式和可用时的 Memory Budget 探测。
- 有大小上限的 Wine 日志与 Android 历史退出原因读取。
- 删除人为内存压力做法；缩小封面、减少并发位图分配；保留上游触控、手柄与键鼠功能。

## 文档

- [实际源码审计](docs/research.md)
- [架构与实现边界](docs/architecture.md)
- [许可证与预编译依赖](docs/licensing.md)
- [设备预设](docs/presets.md)
- [实机短测报告](docs/device-smoke-2026-10-21.md)
- [白屏修复过程](docs/graphics-fix-2026-10-21.md)
- [DX9 / DX11 实机复查](docs/device-graphics-2026-10-21.md)
- [游戏兼容性与导入短测](docs/game-compatibility-2026-10-21.md)
- [Mali适配研究与DX12实验](docs/mali-compatibility-2026-10-21.md)
- [Mali显示后端与驱动调研](docs/mali-display-backends-2026-10-22.md)
- [窗口兼容修复与Warzone第一关证据](docs/window-compatibility-2026-10-22.md)
- [本地直启源码研究与实现](docs/direct-launch-research-2026-10-21.md)
- [必要验证记录](docs/validation.md)
- [已知问题](docs/known-issues.md)

未宣称 Mali 游戏帧率、内存节省或经实机确认的 Wrapper 优化结果。所有尚不可读的设备指标保留 unavailable。公开再分发前仍需补全第三方对应源码、通知与许可材料。
