# DX9 / DX11 实机图形短测 · 2026-10-21

结论：**0.1.3-dev 已保留数据安装到联想 TB375FC，DX11 蓝色和 DX9 绿色场景显示正确、正常结束。** 本次只做三个有明确目的的短会话：0.1.2 的 DX11 定位颜色问题，以及修复后各一次 DX11 / DX9；没有重跑上午已通过的手柄序列。

## 修复与结果

0.1.2 的 GPU 拷贝使原白屏恢复出图，但蓝色显示成橙色。截图点 (700,600) 的 RGB 为 204,76,15，证实红蓝通道互换。实机新增诊断随后确认导入 AHB 的实际 format=1（RGBA），而 DRI3 传递的是 X11 BGRA 数据。

0.1.3 根据 AHB 格式决定是否交换红蓝，逻辑与已有原生 DisplayX 转换一致：BGRA 格式5不再重复交换，其他承载 BGRA 字节的格式按需转换。标记随纹理保存，普通 CPU 窗口上传时清除，后处理合成阶段不重复转换。只增加一次成功诊断，不逐帧刷日志。

| 项目 | DX11 | DX9 |
| --- | --- | --- |
| 场景时间 | 8秒 | 8秒 |
| 创建 / Present HRESULT | 均为 S_OK | 均为 S_OK |
| 特性级别 | FL11_0 | 不适用 |
| 成功 Present 数 | 239 | 241 |
| 截图 RGB，点 (700,600) | **15,76,204**，正确蓝色 | **20,160,70**，正确绿色 |
| 探针完整结束 | complete=1 | complete=1 |
| 内存样本 | 3个，5秒间隔 | 3个，5秒间隔 |
| 采样应用 PSS 最大值 | 435819 KiB | 469280 KiB |
| lowMemory=true | 0 | 0 |

DXVK 日志确认 v1.10.3、Apex(Mali-G615 MC5)；交换链先初始化 IMMEDIATE，随后切到 FIFO，不能描述成从未请求过 IMMEDIATE。退出后未发现本应用的 EmulationService；单次 meminfo 的应用 PSS 为167750 KiB。这些数据不证明 GPU 零泄漏或性能提升。

## 证据与安装包

- `artifacts/device/recheck-v012/`：首次出图但通道错误的 DX11 截图、RGB、日志和结果。
- `artifacts/device/recheck-v013/`：最终两个 API 的截图、RGB、结果、DXVK/Android 日志、内存与退出记录、summary.json、final-state.txt。
- `artifacts/DimensityGaming-0.1.3-dev.apk`，包名 org.dimensity.gaming.dev，versionCode=4；626013689字节；APK签名验证通过。
- SHA256：`fd99834366765961c9c0e960608afebfea2705c70c91ec243a96981751aa1046`。

平板最终保留0.1.3-dev，回到应用主页；旧APK和原容器保留。测试文件仅写入本应用容器；电脑 SuperDisplay 仍为 Stopped / Disabled，临时 ADB 文件已清理。

## 结论边界

这是清屏/Present/HUD 的基础显示场景，尚未验证实际游戏、复杂着色器、纹理、声音、长时稳定性或性能。当前日志仍有 Present 扩展的 BadImplementation 请求，未阻断这两个短场景；未以它们的通过推断高级 Present 请求兼容性完整。

虚拟手柄的 Windows XInput 按键、摇杆和扳机此前已经通过；本轮没有将无操控的连接采样当成再次验证。DisplayX 对比因现有 OpenGL 路径已通过对应复查而未再开展。
