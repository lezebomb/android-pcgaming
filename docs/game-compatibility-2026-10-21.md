# 2026-10-21 游戏兼容性与大文件导入短测

设备为联想TB375FC、Android16、8GB、Mali-G615 MC5。游戏运行结果来自0.1.3-dev；0.1.4-dev随后保留数据安装，完成一次SAF目录导入。每项只验证必要流程，不代表完整通关或性能验收。

| 样本 | 实际路径与结果 | 结论 |
| --- | --- | --- |
| Crispy Doom7.2.0 win64 + Freedoom0.13.0 | SDL经D3D11/DXVK1.10.3出图；进入关卡，键盘移动/开火、虚拟手柄转向/横移/开火，弹药50降至43；正常退出 | 关卡与基本操控短测通过 |
| C-Dogs SDL2.5.0 win32 | SDL2经D3D9/DXVK1.10.3出图；进入Sand战役任务1，虚拟手柄A开火、HAT右移动；正常退出 | 32位游戏关卡与基本操控短测通过 |
| Warzone2100 4.7.0 win64 | ANGLE/D3D11设备初始化成功，之后停在黑屏；原生Vulkan也停在相似terrain初始化位置，未进入主菜单/关卡；由运行器Exit结束 | 未通过，根因待定位 |
| DX12 FL12_0设备创建 | DXVK2.3.1 + VKD3D2.8、未强制功能等级；native d3d12.dll已加载，返回0x80070057。独立日志明确报Feature level 0xc000 is not supported | 当前组合不能创建所需等级，尚未测试游戏 |
| 艾尔登法环Steam版 | Steam于19:08完成正版下载，build25827938，安装目录D:/SteamLibrary/steamapps/common/ELDEN RING；未导入平板或启动 | 已下载，不作可玩性结论 |

Freedoom首次由SDL选择WGI/RawInput输入路径时不响应虚拟手柄。游戏专用快捷方式改为下列环境变量后，Windows XInput操控奏效，不改全局设备设置：

```text
SDL_JOYSTICK_WGI=0 SDL_JOYSTICK_RAWINPUT=0 SDL_JOYSTICK_GAMEINPUT=0 SDL_XINPUT_ENABLED=1
```

Freedoom使用`-iwad freedoom1.wad -config game.cfg -extraconfig extra.cfg -nomusic -warp 1 1 -skill 1`。两游戏的配置及源码下载清单位于`scripts/smoke/game-profiles/`；游戏资源不捆入APK。临时UHID手柄已移除，无需用户拿实体手柄操作。

Warzone对照时还降低了游戏纹理/阴影并关闭纹理压缩，因此不能把两次黑屏当成严格单变量实验，也不能据此认定是Present、BCn或某个后端的共同缺陷。没有以API初始化成功代替画面成功。

## 0.1.4导入改进与实际验证

新增复制量、当前文件、已完成文件数及取消按钮；使用long统计大文件，逐文件检查空间，复制中每8MiB检查余量。大小与提供方声明不符不提交，同名不覆盖。取消或失败会保留已复制内容及`.part`，提示实际目录；尚不支持断点续传。

通过系统文件选择器选择自有`Download/DimensityTest/Crispy`目录，38个文件共50,423,436字节导入到`C:/Games/import-def5ea51/Crispy`，没有残留.part，创建主EXE快捷方式。检查两份关键文件内容：

| 文件 | 电脑原件与SAF副本共同SHA256 |
| --- | --- |
| freedoom1.wad | 7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d |
| crispy-doom.exe | 3e8d50b10c5594e4326cf21043b414864dea546c491f742a5f75f6745777685c |

这次覆盖实际目录读取、复制提交和快捷方式创建；未额外制造多GB测试文件，取消、空间不足和超过4GB文件仍未实机验证。之前直接通过run-as放入容器的游戏测试不计为SAF验收。

## 证据与边界

证据目录`artifacts/device/game-compat/`：Freedoom的keyboard、xinput-controlled、gamepad-strafe截图；C-Dogs的gameplay-before、gamepad-action、gamepad-move截图；各自final-session.json、内存CSV、Wine/DXVK日志；Warzone两个后端日志；DX12的`dx12-trace-wine.log`及`dx12-vkd3d-diagnostic.log`；导入的`import-v014-hashes.txt`、`import-result-ui.xml`和完成截图。

DXVK2.3.1本轮版本应以`dx12-trace-wine.log`的实际运行记录为准；原来的`DG-D3D12Capability_dxgi.log`没有被新版覆盖，属于旧DXVK1.10.3日志，不能用它判断本次加载版本。

Freedoom与C-Dogs的同UID采样RSS峰值分别约1.48GiB、1.47GiB，含共享页重复计数，5秒采样会漏过瞬时峰值，不是独占物理内存。Freedoom的HUD暖机后接近30FPS上限仅作观察；没有帧时间基准、持续性能、音频质量或GPU释放证明。C-Dogs日志实际使用IMMEDIATE，不能把容器请求FIFO当成所有游戏实际FIFO。

免费样本来源：[Crispy Doom7.2](https://github.com/fabiangreffrath/crispy-doom/releases/tag/crispy-doom-7.2)、[Freedoom0.13.0](https://github.com/freedoom/freedoom/releases/tag/v0.13.0)、[C-Dogs2.5.0](https://github.com/cxong/cdogs-sdl/releases/tag/2.5.0)、[Warzone4.7.0](https://github.com/Warzone2100/warzone2100/releases/tag/4.7.0)。引擎与资源许可证随原包保留。归档大小/SHA256已保存，GitHub提供digest的资产均核对一致；Freedoom仅保存下载后SHA256及官方大小，没有声称有官方digest。

## 离线后续调整

0.1.5候选版将新容器/设备预设的VKD3D功能等级设为自动检测，设置页增加自动选项，不再由默认设置强制报告12_1。已有手动等级与显式自定义环境变量保留。这不能补齐驱动缺少的功能，也没有据此宣称艾尔登法环已能运行。用户带走平板后停止实机操作，候选版须待重连再安装。

20:05离线续办：平板未连接；0.1.5候选APK（versionCode6，625969467字节）编译25秒成功、签名v2通过，SHA256 de63b4ad6c8bac4fdb4e135b3e3468433ad1453fe36661dee2e02c9fa1c1820a。DX12探针增加失败后尝试FL11_0并查询实际功能等级、资源绑定/分块资源等级、Typed UAV等信息，低等级创建成功不计FL12通过。新探针仅编译，尚未实机运行。构建/安装脚本现从build.gradle读取版本，避免输出文件名遗漏更新。

## 21日21:23最新进度指引

后续续办以 docs/resume-2026-10-22.md 为准。平板已安装0.1.8-dev（versionCode9），新增默认关闭的应用内Mali-G615 DX12实验兼容层。VKD3D2.14.1+该层已在未强制功能级别时完成FL12_0设备创建，60帧清屏/GPU拷贝/显存回读/Present及应用内集成短复查通过，正常退出。功能声明包含伪装，未验证复杂着色器和3A游戏，不把基础探针当作可玩验收。Warzone仍黑屏；艾尔登法环未导入、未启动。北京时间22日01:10和06:15单次续办已设置。详细来源、代码、测试边界见 docs/mali-compatibility-2026-10-21.md；不要重复已通过项。