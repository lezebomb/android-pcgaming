# 联想 TB375FC 实机短测 · 2026-10-21

结果：APK 0.1.0-dev 首次安装及 Wine 基础启动通过；修复两个界面问题后，0.1.1-dev 已保留数据更新安装，并完成对应界面复查。

## 真实设备能力

| 项目 | 读到的值 |
| --- | --- |
| 型号 / 系统 | LENOVO TB375FC / Android 16，SDK 36 |
| Board / SoC 原始属性 | mt6897 / MT8792 |
| 物理内存 | 8,059,478,016 字节，8GB 级设备 |
| GPU 驱动名称 | **Mali-G615 MC5**，不按最初 MC6 的描述推断 |
| Vulkan loader / GPU API | 1.4.0 / **1.3.247**，二者不同 |
| BC1 / BC3 / BC5 / BC7 采样纹理 | 不原生支持 |
| ASTC 4×4 / ETC2 RGBA8 | 支持 |
| Android Hardware Buffer 扩展 | 存在；具体图像组合兼容性仍未验证 |
| Memory Budget | unavailable，不能伪造预算值 |
| 内存页 | 4096 字节 |
| 安装前 /data 空间 | 约 145GB 可用 |

原生探测结果：`artifacts/device/vulkan.json`。没有修改系统 Vulkan 驱动、分区、zRAM、Swap、LMKD、温控或调度设置。

## 执行内容与证据

1. 普通 `adb install -r` 安装独立包 `org.dimensity.gaming.dev`，结果 Success；未覆盖其他运行器。
2. 主页正常显示；bundled imagefs 初始化完成，内部版本 21，Proton 10 ARM64EC 的 Wine 文件存在。
3. 通过界面创建一个 Container-1，并应用均衡预设。配置记录在 `artifacts/device/container-config.json`。
4. 从主页启动 `winecfg.exe`，实机出现 Wine configuration 窗口；日志显示 `onMapWindow: winecfg.exe`，并能采样到 wineserver、services、explorer、winhandler 和 winecfg 等同 UID 进程。
5. 对 Android 主页按钮、容器设置与退出菜单进行了触控操作；发送了一次 Enter 键，再通过本应用 Exit 退出会话；未把这一次按键当作完整键盘输入验证。没有连接手柄，不能宣称蓝牙手柄已实测。
6. 正常退出后诊断写出 session.json，主页重新打开；Android `dumpsys meminfo` 观察到退出后的应用 TOTAL PSS 180,825 KiB。这个单点不构成无 GPU 泄漏或性能提升的证明。
7. 修复后以 `adb install -r` 更新 0.1.1-dev，versionCode=2，保留原容器。新建容器界面默认 DXVK / 1280×800；从容器页返回正确回到 Dimensity 主页。只查看并取消新建界面，没有制造额外测试容器。

截图：`artifacts/device/homepage.png`、`artifacts/device/winecfg.png`。安装、日志、配置及界面证据均保存于 `artifacts/device/`。设备解锁密码没有写入源码、脚本或这些记录。

## 一次基础程序会话的内存样本

- 16 个样本，最后一个时间点 75,769 ms，采样间隔约 5 秒。
- Android Debug 应用 PSS 采样最大值：431,992 KiB。
- 可读同 UID 进程 RSS 累加采样最大值：640,304 KiB（**包含共享页重复计数，不能当作物理内存占用**）。
- Android lowMemory=true 样本：0。
- 热状态样本为 Android status 2；没有开展温控前后对比或持续性能测试。
- CSV 在 `artifacts/device/memory.csv`，退出概要在 `artifacts/device/session.json`。

以上仅是单次 winecfg 基础会话，不是游戏基准，也不是修改前后的性能比较。未声称 1% Low、P95/P99、FPS 提升或节省多少 RAM。HUD 的 FPS 0 在静态 GDI 窗口中不能解释为游戏性能。

## 实机发现并修复

| 问题 | 原因 / 修改 | 复查 |
| --- | --- | --- |
| 新建容器落到 WineD3D | DX Wrapper 选项数组只有 WineD3D 和 DXVK+VKD3D，找不到新的独立 dxvk 标识；加入 DXVK 选项 | 新建界面实际显示 DXVK |
| 新建容器转译预设未采用保守默认 | 界面 fallback 仍硬编码 COMPATIBILITY / INTERMEDIATE；改为 STABILITY | 代码编译通过；均衡预设保存路径此前已实机验证 |
| 从容器页返回退出应用 | MainActivity 沿用了以容器页为根的旧返回逻辑；改为先处理回退栈，再返回 Dimensity 主页 | ADB UI 证据确认返回主页 |

这些属于已复现的默认配置和界面修复，不包装成 GPU 性能优化。

## 电脑环境处理

SuperDisplay 的 MirrorService 曾反复拉起旧 ADB，导致连接重置。先经用户允许临时暂停，后依用户进一步明确要求将 Windows 服务 SuperDisplay 设为 **Stopped / Disabled**，已确认最终状态；未改平板服务。此前的自动恢复安排已取消。

## 本轮未覆盖

DirectX 9/11 图形场景、具体游戏、EXE/目录导入的完整文件选择流程、手柄、音频输出质量、纹理转码性能、GPU 资源泄漏及 30 分钟持续性能均没有本轮结论。后续优先选用户合法拥有的一个轻量游戏做短测。
