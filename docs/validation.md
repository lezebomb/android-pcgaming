> 最新进展：已连接目标平板完成基础短测，并安装修复版 0.1.1-dev。见 [实机短测报告](device-smoke-2026-10-21.md)。以下原记录是首次构建阶段，不能再将其中“未连接设备”作为当前状态。
# 本轮验证记录

日期：2026-10-21。按用户要求，验证集中在必要编译和产物检查，不增加大规模测试套件或压力测试。

| 检查 | 状态 / 限制 |
| --- | --- |
| 候选项目真实源码审计 | 已完成；提交与具体文件见 research.md |
| Android Java + JNI/C++ 编译 | 已成功生成 APK；最终增量构建记录见 artifacts/build.log |
| 三个运行包 SHA-256 | 已与 GitHub release digest 一致 |
| ADB 设备识别 | 设备列表为空 |
| APK 安装、主页交互 | 未验证 |
| winecfg / Windows 程序 | 已实现入口并捆绑运行环境；未验证启动 |
| D3D9 / D3D11、Mali Wrapper | 未实测 |
| 帧率、P95/P99、1% Low、功耗 | 未测量，不提供数值 |
| 退出后 GPU 资源释放 | 未验证；不能用 Java RSS 下降替代 GPU 生命周期证明 |
| 长时间温控 / 30 分钟测试 | 未执行 |

## 修改与证据边界

| 修改 | 目标问题 | 源码位置 | 前后数据 / 决策 |
| --- | --- | --- | --- |
| 删除内存压力组件注册与 native 压力实现 | 上游主动分配匿名页触发 LMK，可能干扰日用应用 | XServerDisplayActivity、CMakeLists、RamBooster | 静态行为可确认；没有实测性能数据；保留 |
| 缩小封面、取消图标放大、串行初始化 | 启动器重复/并发位图分配 | container/Shortcut.java | 分配像素规模减少；无实测 RSS 对比；保留 |
| 5 秒同 UID 采样、低内存启动保护 | 8GB 设备缺少诊断与预警 | dimensity/MemoryMonitor.java、GuestProgramLauncherComponent | 已编译；采样开销及设备权限待验证 |
| 去掉跳过 barrier、强制 Vulkan 版本、默认转码 | 缺少硬件证据的兼容性风险 | DXVKConfigDialog、Container、XServerDisplayActivity | 不声称更高 FPS；保留保守默认 |
| SAF 导入、独立包、同 UID 退出 | 数据隔离与最小权限 | DimensityFragment、Manifest、ProcessHelper | 构建可确认；设备行为待验证 |
| 真实 Vulkan 查询和预算探测 | 不应根据 Mali 型号猜测特性 | cpp/dimensity/probe.cpp | 已编译；具体 GPU 结果须在设备生成 |

没有优化前游戏场景、目标设备帧时间或三次重复数据，不能标记 Phase 3 已达成。后续只在用户批准使用设备后做一次简短基础验证；有必要再增加针对性检查。

## 最终产物检查

- 最终构建：`BUILD SUCCESSFUL in 18s`；39 个任务中 30 个复用缓存。
- APK：`artifacts/DimensityGaming-0.1.0-dev.apk`，626,248,181 字节（约 597.2 MiB）。
- SHA-256：`933baf9d461c4d866658bf00503b2843f7bce3f8e860e714402cd28a9ab686b1`。
- `apksigner verify` 通过，APK Signature Scheme v2；记录在 `artifacts/signature.txt`。
- `aapt2 dump badging` 确认包名 `org.dimensity.gaming.dev`、版本 0.1.0-dev、arm64-v8a、minSdk 26 / targetSdk 28；记录在 `artifacts/apk-metadata.txt`。
- 最终 Manifest 不含 WRITE_SECURE_SETTINGS、MANAGE_EXTERNAL_STORAGE、READ/WRITE_EXTERNAL_STORAGE。
- 已检查 Wine/imagefs/Prefix 模板、原生显示库、探测库、输入库及第三方说明确实在 APK 内；记录在 `artifacts/package-check.json`。

这些检查证明构建、签名和必要打包内容，不替代设备安装与游戏运行结果。
## 0.1.1-dev 更新

修复后构建成功（16 秒），APK 625,963,511 字节。
SHA-256：`5911c7ae156117dcddad3c1bffc6249af72f0dd8342b08a2935282473a896fef`。
已实际以保留数据方式更新安装；目标机报告 versionCode=2 / versionName=0.1.1-dev。

## 0.1.11-dev 直启更新（2026-10-21）

最终APK631103786bytes，SHA256 e5acb631a24f82ab9e4d62b92b0d33e5421cadab08b8d404a4723a4f2d88f342；构建、v2签名验证及保留数据安装成功。只做两次针对启动链的必要短测：自动启动与MainActivity外部快捷方式入口。自写程序实际捕获带空格的完整EXE路径、不同工作目录、含空格单参数，均匹配，正常退出且没有循环。原始结果artifacts/launcher/direct-launch-result.txt；未重复图形清屏、XInput、小型游戏或55GB导入。

ZUI应用列表提示仍曾出现，没有授权。随后改用BuildConfig读取自己版本；第二次入口运行与最终安装结束存在时间重叠，不把该轮当作提示已彻底修复的证据。自动启动测试偏好/临时快捷方式已清理，正式默认关闭。实际艾尔登仍在原版接口初始化阶段退出；详见本地直启研究报告及凌晨交接。
