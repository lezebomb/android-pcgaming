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

### 离线0.1.29构建

GL上下文恢复与EGL导入失败恢复改动完成APK构建，v2签名验证通过。产物仅在本地产物目录：631674958bytes，SHA256dcc308abe572114bec2ba95dd75c1f2fbca00886fdb9e5d211feef16803cd25a。平板离线，未安装、未将构建通过作为实机画面或可玩结论；未重复既有探针或输入测试。

### 离线0.1.30纹理更新

单次本机原生检查直接编译产品上传布局与BC解码实现，验证全内存标志选择、带padding的5×5双层BC区域打包及边界、四线程BC4颜色块偏移，vortek_upload_result=0。ARM64客户/服务端及APK构建成功，v2验签通过。APK631678294bytes，SHA256753638b713681f67be39146783cb042315faf83d1b0d1a1032a807dfd48cfaed。没有运行旧的色块、手柄或游戏套件；平板离线，未安装，实际黑屏与性能结果仍待设备复查。

### 离线0.1.31输出互操作更新

ARM64客户/服务端与Java APK构建通过，v2验签通过；631679342bytes，SHA256e8e91da13eab245a8249baaffadbe8e79cfcafd9f114bd7c157e281a38bf7738。此次只做必要构建/验签，没有重复纹理fixture或实机测试。输出格式和厂商AHB用途查询仍需设备确认。

### 0.1.32首次Vortek实机路径

APK631681066bytes，SHA256752e46fa3557ecfe93cd777450445f575ebb3c23c958ccd5228e14261fae9937，构建/v2/保留数据安装通过。自编Win64 Vulkan程序在实际Vortek(Mali-G615 MC5)上64次呈现、一次窗口resize返回OUT_OF_DATE并重建成功，result=0；主机首次截屏超出默认缓冲，取到的后续截图已回库页，因此不以该截图证明色块可见。宿主确认AHB拷贝完成。真实Warzone Vortek短查仍黑：日志明确swapchainDesiredImageCount=1触发>=2断言，已停止该会话，下一步补多缓冲。这些均非AAA验收。

### 0.1.33实机多缓冲

APK631681386bytes，SHA2562edfdef6071d8203fcc7fc49bbabe8eaa19ed80037ea20a83dba7a6fd15ebfc6，构建/v2/更新安装通过。Warzone日志不再出现单缓冲断言，宿主实际分配3个独立输出；仍停在初始化黑屏，未通过3D或操控。约150秒后出现waitForFences ErrorDeviceLost，不能将断言消失称为黑屏已修。下一步修正上游sync_fd等待语义；没有重跑基础输入/旧图形套件。

### 0.1.34实机Fence修复

APK631678438bytes，SHA256941d3b8de3761810967a7bac2619743b65707c7775d3ae929ee9db6431ead2e2；构建/v2/安装通过。相同Warzone Vortek会话已越过黑屏，日志stageTwoInitialise/gameLoad CAM_1A/Entering main loop；实际截图有地形、车辆、HUD、手柄选7单位。摇杆已移动鼠标，A/B尝试未证明单位位移，helper达到时限退出，不冒称移动/战斗/稳定性通过。未重复旧色块或全套XInput。

### 0.1.35显存传输实机修复

产品serializer头直接NDK编译在本app UID执行：memory_wire_result=0 native_heap_size=16；覆盖二堆数据/flags、预算最后项及260字节线长。APK631679338bytes，SHA256070574d84cf1a870fbd7040c199e5b94fbbb044bbfaff114ddee0c210aa7643e，构建/v2/安装通过。对应一次Superposition legacy1.10.3 launcher复查中Heap[1].flags从随机值修正为0x1，但仍0xc0000005，未到benchmark场景。

### 离线0.1.36共享内存修正

单次产品range helper短查vortek_mapping_result=0，覆盖3GiB、4K/16K页、非零offset的WHOLE_SIZE及越界；ARM64两端和APK构建、v2验签通过。APK631680282bytes，SHA256fd2fe875a53e1655319e5e1deaa5f93a47e6757dce6be466bff797c496a2a13c。按用户要求本轮离线，没有安装或运行游戏，已安装仍0.1.35。

### 离线0.1.37着色器与管线

一次host检查直接编译产品shader_inspector.c与arrays实现，以记录型Vulkan模块接口验证同一shader两种顶点格式的独立转换、原始code不变、编译失败返回及边界；vortek_shader_result=0，不包含GPU渲染。ARM64两端、APK构建与v2通过。APK631682418bytes，SHA2568e1de1aea7a151499310108f9808b6e513106ec417f7e91d67422b780a08edb3。未安装/未游戏复查。

### 离线0.1.38原生映射

APK构建及v2通过，631682630bytes，SHA2560fd5ae2ed82066c4ad958a61f5676dbecefe927c9c36da541835f5bfa35c57a2。只补原生映射调用有效性，没有实机缓存一致性或画面结论。

### 离线0.1.39可选Sarek

固定作者发布包SHA256核对通过；一次host短查直接运行产品DxvkSarek.java（仅Android资产入口使用stub，tar库与产品相同）：sarek_install_result=0，Win64十DLL/Win32五DLL、没有解出源码目录。APK构建/v2通过，642834879bytes，SHA2566beee625f4fc532c743229b18c9231a8a62a4995de95a91921da4c6553a204a6。首次编译发现OnExtractFileListener并非嵌套类型，已改为产品接口并重新构建成功。未安装，已安装仍0.1.35；没有重复游戏/输入/旧探针。


### 离线0.1.40呈现接口修正

一次host检查直接编译产品表面查询实现和扩展策略，覆盖零/有限/大容量查询、输出结构头保留、Acquire2超时不改图像编号及未实现扩展过滤：vortek_wsi_result=0。ARM64客户/服务端、APK构建和v2验签通过。APK642835899bytes，SHA25665fcda402804852ee25f7156c65888bd461d8e606e22f6a0befeb720dc66afb8。此检查验证接口语义，不含GPU渲染；未安装，设备仍0.1.35。

### 离线0.1.41显示恢复

完成Java/ARM64与APK构建、v2验签；APK642837639bytes，SHA256cacc4ce78d5a683f3cec1afdc6681a147158cb7bcee19f91c12433a98ce97f6c。只做必要构建和拷贝/取消/重试的源码时序检查，未重复旧GPU或手柄套件。未安装，设备仍0.1.35，EGL恢复效果待平板。
