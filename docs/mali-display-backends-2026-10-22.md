# Mali 显示后端调研（2026-10-22）

平板已有可调用的系统 Mali Vulkan 驱动；现有实机绘制证明不是完全缺驱动。当前需要改进应用内适配、资源转换及窗口提交，不能把“安装一个驱动”当作所有游戏可玩的保证。

| 路线 | 开发者实现 | 本项目状态 |
|---|---|---|
| System Mali + wrapper | Android系统libvulkan/厂商Mali，外加Mesa的X11/WSI及格式处理 | 当前默认。固定原库有明确SPIR-V字节/字数越界，副本单样本回归已过；复杂场景仍未通过 |
| Mali兼容层 | leegao为DXVK/VKD3D补部分缺失功能，同时有功能声明覆盖 | 已默认关闭、显式启用，预算/SM6/BC基础执行已证实；不当作AAA验收 |
| Vortek | Winlator客户端通过共享内存/Unix socket连Android服务端；服务端用系统Vulkan和AHardwareBuffer更新窗口 | 新的可选路线。已固定配套源码、开始Bionic客户端移植；还没接入/实机，不替换现有后端 |
| Turnip | Mesa的Adreno Vulkan驱动 | 不用于Mali-G615 |

来源：[Vortek主仓库](https://github.com/brunodev85/vortek)、[Winlator明确加入Mali支持的更新](https://github.com/brunodev85/winlator/releases/tag/v10.0.0)、[GameNative Mali/DX12提交](https://github.com/utkarshdalal/GameNative/commit/17da0b3)、[Mali兼容层](https://github.com/leegao/compat_layer)、[Mesa Turnip适用GPU](https://docs.mesa3d.org/drivers/freedreno.html)。

本地源码对照：upstream/winlator根记录的客户端commit b1730c5def9b575672e671aee11d79ae7adc63d1，app子模块a030f552f452158a2db64fdb32b490fa19c0b48d。原始客户端178930bytes，SHA256eef892505c77fb6f3cc7bc78a151662ffb77bab934e5eeff8ead7270a6734c82。客户端/服务端serializer正规化换行后完全一致（2761a1cad4c6812004e57c210b57507800b64b28ba33824b936d9daa37461253）。GameNative旧Java接口只有(I)J，但该Winlator服务端要求(IZ)J，不能混接。

已经完成的离线代码：客户端允许明确选择Android客体角色，保留Bionic ABI；socket从会话环境变量读取、拒绝空/过长/非绝对路径，避免访问原Winlator应用目录；固定配套Vulkan 1.3.302头及Khronos Xlib表面声明。ARM64 NDK链接成功，无未解析符号，依赖仅libc/libdl/libm/liblog，导出ICD入口，2195272bytes。当前Xlib版本产物a1f9f29357c35a656f508ea7112b92ac72b0932195cb90a3e11784695766a8b3，只算客户端构建，不算服务端/显示通过。

后续必须一起完成：Android服务端JNI/缓存路径移植；专用GPU输出AHB（不要永久CPU锁住GPU生产者缓冲）；Xlib/XCB surface协商与现有Wine匹配；在启动前开启服务端、退出释放；每游戏可回退选择。不能只复制旧glibc客户库到目前Bionic/FEX环境，也不能把官方声明支持Mali推成已在本平板稳定跑3A。

Qt黑屏最新等待栈显示Java GLThread正常wait、XConnector在epoll；原生WSI queue没有待提交图片、event线程在condition等待，故暂没有证据证明“Present事件丢失”是该Qt会话原因。不要仅因服务端缺NotifyMSC就冒称定位成功；原库WSI源码没调用它。独立的引擎shader越界证据仍然成立。
