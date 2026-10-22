# OpenGL 画面提交修复与实机复查

最终状态：**0.1.3-dev 已安装，DX11 蓝色 / DX9 绿色场景出图与正常退出通过。** 下文15:00时的设备未连接记录保留为过程；最终证据见 `device-graphics-2026-10-21.md`。旧安装包与容器均保留。

## 已知证据与定位

上午的 Windows 探针由 XInput 收到完整虚拟手柄输入；DXVK1.10.3 在真实 Mali-G615 MC5 上成功创建 D3D11 FL11_0、D3D9 设备并成功 Present。屏幕截图仍为白色，不能用 API 成功代替画面成功。已有证据在 `artifacts/device/smoke-*`。

代码显示 OpenGL 的 Present 分支对 GPU 生产的 AHardwareBuffer 使用 CPU copyArea；导入缓冲也被强制以 CPU_WRITE_OFTEN 长期映射。PresentSelectInput 则会另分配一块 GPUImage 并替换窗口的普通纹理。这些路径不应把 GPU 生产的像素当作普通 CPU 写入内存处理。相关 API 约束参见 [Android NDK AHardwareBuffer](https://developer.android.com/ndk/reference/group/a-hardware-buffer)。这是基于代码的修复方向，尚无本轮修复后实机结论。

## 变更

- OpenGL GPU pixmap 在现有 GL 线程内通过 EGLImage/FBO 拷贝到窗口自己的纹理，保留偏移裁剪。只复用一个拷贝 FBO，不逐帧创建。
- GPU 拷贝完成后才发送 PresentComplete / 排程 PresentIdle，防止生产端提前复用源缓冲；使用 glFinish 作为保守同步点。普通 CPU pixmap 的 copyArea 路径保留。
- 订阅 Present 事件不再替换窗口纹理，GDI 窗口继续使用自己的数据。
- 导入的 GPU 缓冲仅读取 stride/format 并交给 EGL，不强制 CPU 映射；只对实际映射过的缓冲执行 unlock。移除创建 EGLImage 时多余而未配平的显式 AHB acquire。
- 无 CPU 映射的 GPU pixmap 读写请求返回 BadMatch，避免空指针；未实现 GPU 读回。失败时只记录一次 DimensityPresent 诊断，避免逐帧刷日志。
- 探针新增 `--graphics`（约7秒）、`--d3d9` 或 `--d3d11`（各8秒）模式，跳过已验证的35秒手柄阶段。

## 15:00 初次构建时的验证与待办

- 完成 Windows 探针编译；Android Java/JNI/APK 构建通过。未添加重复实现的单元测试，也未开展压力测试。
- 15:00 后 ADB 列表为空，因此未安装新 APK、未采集新画面、未进行 DisplayX 对比。SuperDisplay 仍为 Stopped / Disabled。
- 接回平板后：普通 install -r 更新候选版，先以现有 OpenGL 容器分别短跑一个 DX11 / DX9 色块阶段，各留一张实际截图；如仍失败，查看 DimensityPresent 诊断，再用仅测试快捷方式的 DisplayX 配置比较。只修改本应用数据。
- 还需观察颜色、方向和退出资源；glFinish 的性能代价尚未测量，不以通过构建推断性能提升或无 GPU 泄漏。没有真实游戏结论。

构建入口：`scripts/build.ps1 -Offline`；候选 APK：`artifacts/DimensityGaming-0.1.2-dev.apk`。

## 连接平板后的结果

0.1.2实际出图但红蓝互换；0.1.3按AHB格式处理交换并完成DX11/DX9各8秒的针对性复查，RGB与程序设定一致。最终安装包为 artifacts/DimensityGaming-0.1.3-dev.apk，具体结果见 device-graphics-2026-10-21.md。没有追加已通过的手柄或压力测试。
