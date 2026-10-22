# 可运行最小版本架构

`org.dimensity.gaming.dev` 是独立 Android 包；Java/JNI 命名空间保留 `com.winlator.cmod`，避免无必要的 JNI 符号重命名。与原版 Winlator/WinlatorMali/GameNative 的应用私有数据隔离。

启动链：平板主页 → 选择独立游戏容器 → Wine Prefix → XServer/音频/输入 → Wine ARM64EC + FEX → DXVK → 现有 Mali Wrapper → Android 系统 Vulkan 驱动。x86_64 Wine + Box64 保留上游可选入口，默认内置的是 Proton 10 ARM64EC。仅选择 Box64 名称不会把 ARM64EC Wine 变成 x86_64 Wine，需要另外安装匹配运行包。

APK 打包 `imagefs.tzst`、Proton 10 ARM64EC、对应 Prefix 模板、FEX/Box64、DXVK、Wrapper 等资产。首次启动解包到自己的 `files/imagefs`，随后通过原有容器管理创建 Prefix。系统文件初始化可能持续数分钟，建议预留数 GB 空间；具体占用尚未在目标机测量。

## 新增模块

| 模块 | 作用 |
| --- | --- |
| `dimensity/DimensityFragment.java` | 横屏入口、SAF 单文件/目录导入、预设、Wine 基础程序、检测与 ZIP 导出 |
| `dimensity/DeviceProfiles.java` | 三种显式设置；保存到单游戏容器，可恢复均衡 |
| `dimensity/MemoryMonitor.java` | 5 秒采样系统内存、应用 PSS、可读同 UID 进程 RSS/PSS、热状态与采样峰值 |
| `dimensity/RuntimeLog.java` | 有上限的 Wine 输出文件；退出移除回调并关闭文件 |
| `dimensity/VulkanProbe.java` + `cpp/dimensity/probe.cpp` | 原生 Vulkan 实例/物理设备/扩展/格式能力/支持时的内存预算查询 |

导入经系统文件选择器授权，按流复制到容器 `C:/Games/import-<随机值>`，原文件不改动。目录遍历设深度/数量限制，保留 256 MiB 存储余量，不通过绝对路径猜测 SAF 文件位置。一次导入最多列出 256 个 EXE 供选择。复制失败保留私有目录中的部分文件供用户处理。

## 内存和进程生命周期

启动前检查 Android lowMemory，系统已报告低内存时取消本次 Wine 启动。可用内存低于 1.5 GiB 或低于该容器历史采样 RSS 加余量时提示风险；这只是保守启发式，不是启动保证。

每 5 秒采样，仅访问可读的同 UID 进程，不为采样申请危险权限。首 60 秒和全会话分别记录采样峰值。RSS 含共享页，不能解释为物理占用总和。不可读取的 PSS/GPU 占用明确为 `unavailable`，5 秒采样可能漏掉瞬时峰值。API 30+ 在主动诊断时读取 Android 历史进程退出原因。

游戏退出沿用已有环境关闭链，并将主启动进程的立即 SIGKILL 改为 SIGTERM；等待后仍存在的本应用 Wine 进程由原有退出流程处理。进程清单现在明确验证 UID。清理不删除游戏、Prefix、存档或磁盘 Shader 缓存。退出采样不等于已证实 GPU 缓冲全部释放。

封面生成降为 300×450；用户封面有解码采样限制；取消将图标无条件放大至 512，并让封面初始化单线程执行。只声称减少分配规模，不声称实测节省了多少系统 RAM。

## Android 版本与权限

minSdk 26、targetSdk 28、compileSdk 36。保留 target 28 是因为普通 APK 的可写目录可执行文件限制会影响 Wine/Box64；未引入 GameNative 的专有 exec 重定向。Android/ZUI 具体兼容性、16 KiB 页设备及包名变化对上游预编译组件的影响均待实机确认。不能为了提高 targetSdk 复制未知权限的闭源库。

已移除 WRITE_SECURE_SETTINGS、MANAGE_EXTERNAL_STORAGE 和通用读写存储权限，导入/诊断导出走 SAF。原上游文件管理器对任意共享目录的直接访问及其部分旧导出入口不属于当前推荐路径。只更改本应用窗口设置，不写全局刷新率、温控、调度或系统内存配置。

## 图形边界

默认 DXVK 1.10.3 提供已在其上游 `dxvk.conf` 确认存在的 `dxgi.maxFrameRate` 与 `d3d9.maxFrameRate`。GPU 功能不伪装，屏障不默认跳过，ASTC/ETC2 转码默认关闭，BCn 保留上游自动/CPU 路径与磁盘缓存。60 FPS 是上限设置，不是设备性能承诺。

探测器不创建游戏 surface，因此 present modes、swapchain formats、AHB 实际用法兼容性会报告 unavailable。设备扩展存在不等于某个图像组合可用。没有实现或宣称动态后端自动回退、完整帧时间统计、DX12 可用性或已修复 Wrapper 性能瓶颈。
