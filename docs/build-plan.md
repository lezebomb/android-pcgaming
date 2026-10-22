# 构建与设备验证

## 当前工具链

- Windows、JDK 21（代码目标 Java 17）
- Gradle 9.3.1、Android Gradle Plugin 9.1.0
- Android SDK Platform 36、Build Tools 36.0.0
- NDK 28.2.13676358、CMake 3.22.1、arm64-v8a
- Node.js 20+ 用于下载与 SHA-256 校验运行包

该组合已在当前机器成功生成 debug APK。两个兼容配置保留旧 Java switch 的 R 常量：`android.nonFinalResIds=false` 与 `android.enableAppCompileTimeRClass=false`，AGP 提示将在 10.0 移除，后续升级需迁移对应 switch。

## 一条命令构建

在项目根目录运行：

```powershell
$env:JAVA_HOME = '你的 JDK 目录'
$env:ANDROID_HOME = '你的 Android SDK 目录'
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -FetchRuntime
```

运行包已在本机下载，增量构建可不带 `-FetchRuntime`。缓存完备时可加 `-Offline`。脚本默认只构建 APK，**不会安装、启动应用或改变设备**。使用项目自己的 `.gradle-home` 和开发签名；不要把开发私钥作为公开源码分发。

产物：`artifacts/DimensityGaming-0.1.1-dev.apk`、同名 `.sha256`；日志：`artifacts/build.log`。首次下载约 411 MiB，APK 因包含完整运行栈较大。`gradlew.bat` 也可直接运行 `:app:assembleDebug`，前提是已经准备资产、SDK 路径与签名。

依赖子模块已复制到根工程实际源文件中，根工程不依赖 `upstream/` 研究副本。重新取得基线时，libadrenotools 为 `8483dfdaa2abf97ee89ad0e5f337e7b508550c6b`，其 liblinkernsbypass 为 `b10d48548d608dfca38ffc449d0350335b2f3737`。

## 最少的实机步骤

1. 连接并在平板上授权 USB 调试后，仅识别设备：`scripts/device.ps1`。
2. 用户确认安装影响后运行 `scripts/device.ps1 -Install`。安装独立包，可与原运行器共存；不会卸载其他应用或清除数据。
3. 用户准备好使用平板时手动打开，或执行 `scripts/device.ps1 -Launch`。首次会解包运行环境。
4. 主页选择「容器与 Wine 桌面」创建容器；主页应用「均衡」；运行 `winecfg.exe` 看是否出现窗口。
5. 主页检测真实 GPU/Vulkan；经确认后导入一个合法小型程序或游戏，验证显示、触控与手柄。
6. 退出游戏后从主页导出诊断 ZIP；需要 Android 日志时 `scripts/device.ps1 -CollectLogs`。

脚本不执行 root、刷机、全局设置更改、卸载、清理数据或清空日志。安装/启动开关需要操作者主动指定。没有任何自动 30 分钟压力测试。

首次构建时未连接设备。2026-10-21 后续已完成安装、主页、winecfg、GPU 检测与内存采样短测，见 device-smoke-2026-10-21.md；D3D9/11 游戏及持续性能仍未验证。安装更新使用 -r 保留现有应用数据。电脑 SuperDisplay 服务曾与新版 ADB 冲突，本次已依用户明确要求停止并禁用；构建和设备脚本不会自行更改任何电脑服务。
