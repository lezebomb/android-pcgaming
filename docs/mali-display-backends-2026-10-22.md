# Mali 显示后端调研（2026-10-22）

平板已有可调用的系统 Mali Vulkan 驱动；现有实机绘制证明不是完全缺驱动。当前需要改进应用内适配、资源转换及窗口提交，不能把“安装一个驱动”当作所有游戏可玩的保证。

| 路线 | 开发者实现 | 本项目状态 |
|---|---|---|
| System Mali + wrapper | Android系统libvulkan/厂商Mali，外加Mesa的X11/WSI及格式处理 | 当前默认。固定原库有明确SPIR-V字节/字数越界，副本单样本回归已过；复杂场景仍未通过 |
| Mali兼容层 | leegao为DXVK/VKD3D补部分缺失功能，同时有功能声明覆盖 | 已默认关闭、显式启用，预算/SM6/BC基础执行已证实；不当作AAA验收 |
| Vortek | Winlator客户端通过共享内存/Unix socket连Android服务端；服务端用系统Vulkan和AHardwareBuffer更新窗口 | 默认关闭的可选路线。客户端/服务端与每游戏选择已集成，0.1.30构建通过，尚未实机 |
| Turnip | Mesa的Adreno Vulkan驱动 | 不用于Mali-G615 |

来源：[Vortek主仓库](https://github.com/brunodev85/vortek)、[Winlator明确加入Mali支持的更新](https://github.com/brunodev85/winlator/releases/tag/v10.0.0)、[GameNative Mali/DX12提交](https://github.com/utkarshdalal/GameNative/commit/17da0b3)、[Mali兼容层](https://github.com/leegao/compat_layer)、[Mesa Turnip适用GPU](https://docs.mesa3d.org/drivers/freedreno.html)。

本地源码对照：upstream/winlator根记录的客户端commit b1730c5def9b575672e671aee11d79ae7adc63d1，app子模块a030f552f452158a2db64fdb32b490fa19c0b48d。原始客户端178930bytes，SHA256eef892505c77fb6f3cc7bc78a151662ffb77bab934e5eeff8ead7270a6734c82。客户端/服务端serializer正规化换行后完全一致（2761a1cad4c6812004e57c210b57507800b64b28ba33824b936d9daa37461253）。GameNative旧Java接口只有(I)J，但该Winlator服务端要求(IZ)J，不能混接。

目前已集成配套Bionic客户端、Android服务端、Xlib/XCB surface、每游戏回退选择与GPU专用AHB呈现，缓存及socket均在本应用目录；不依赖其他应用的私有目录。当前客户端SHA256为fed0760809cccd752e7f466890307c7eb48d9e3c4f6e92b3ca3a3650d9f6a1a6。构建来源与配套确认规则见源码准备脚本和source-lock。离线构建不等于实机显示通过。

Qt黑屏最新等待栈显示Java GLThread正常wait、XConnector在epoll；原生WSI queue没有待提交图片、event线程在condition等待，故暂没有证据证明“Present事件丢失”是该Qt会话原因。不要仅因服务端缺NotifyMSC就冒称定位成功；原库WSI源码没调用它。独立的引擎shader越界证据仍然成立。

## 社区线索与本轮落地

检索Reddit、LINUX DO及酷安公开索引。Reddit有相近的Mali黑屏但音频/输入继续工作反馈，未找到可复现的统一配置修复：[原始讨论](https://www.reddit.com/r/winlator/comments/1p635of/how_do_i_fix_this_black_screen_issue_on_winlator/)。同型号族的[wrapper issue143](https://github.com/leegao/bionic-vulkan-wrapper/issues/143)也报告G615黑屏，关闭状态本身不能当作本项目已有修复。酷安暂未取得可核对的公开源码方案。

LINUX DO作者的[原生移植记录](https://linux.do/t/topic/2985510)说明Mali缺少相应BC格式时，需要真正解码纹理，并处理Mip及绑定状态；该记录不是Winlator/G615上的3A实测。我们据此继续核对解码与上传路径，不复制其游戏文件或原生引擎改包。另对照[wrapper开发者的Vortek分析](https://leegao.github.io/winlator-internals/2025/06/02/Vortek2.html)与当前固定源码，不能将旧分析的推断替代本版本代码。

新发现[G615 PanVK开发记录](https://github.com/zenithblue-oss/panvk-kbase-android/blob/main/docs/RUN-PC-GAMES-ON-MALI.md)：其验证为DXVK Native/FL11_0，Wine、普通app加载、sync_fd权限与DX12仍列有限制。另有[MineGraph实验分支](https://github.com/MineGraph-bilibili/panvk-Android)面向G610/G615等CSF设备。当前不以“安装PanVK即可3A”作结论，不修改本机系统驱动。

0.1.28已完成独立Vortek选择/私有ICD/socket、GPU输出AHB、Xlib/XCB、呈现等待及GPU拷贝；单缓冲、QueueWaitIdle、glFinish保守同步，只有离线构建和签名通过，没有实机画面与性能结论。取消每游戏开关回到原后端。源代码已上传指定仓库，二进制不上传。

0.1.29已完成另一条明确的源码修复：Android重建GL上下文后旧资源编号失效，纹理、材质/Uniform、VBO、后处理FBO须重新创建；失败的EGL导入保留producer AHB以允许重试。Android官方说明资源会随上下文丢失而失效：[GLSurfaceView.Renderer](https://developer.android.com/reference/android/opengl/GLSurfaceView.Renderer)。这是对该恢复路径的代码修复，不能冒充此前Qt黑屏已在平板消失。


## 0.1.30：纹理上传与能力声明

按现有平板记录，系统GPU为Mali-G615 MC5，Vulkan 1.3.247，driver_version_raw=184553472，原生BC为false，ASTC/ETC2为true。因此只为缺失且已实现的BC1–5提供实验解码，保留原生ASTC/ETC2及其他真实能力；不再直接把全部核心功能改成true。该驱动标识来自既有探测，没有再次连接平板。

修复内存类型仅满足部分标志的问题；BC4分块偏移使用8字节；Mip、数组层、行距与缓冲范围分别校验。缓存按当前上传尺寸、格式与内容区分，分配好暂存缓冲后才读取，要求文件精确大小。上传拷贝放在原命令缓冲内，保留游戏的布局转换和屏障；CPU解码发生在提交前，暂存直到命令重置或释放，独立上限256MiB。相同数据重提交避免改写在用暂存。

客户端和服务端增加成对的QueueSubmit/2确认，序列化请求不变，但回复策略是本项目扩展，不能混用原版服务端。确认不等待GPU完成，避免拦住后续时间线信号或其他队列提交。呈现仍采用单缓冲、QueueWaitIdle与glFinish，性能未验。

BC6/7、GPU生成压缩数据、BC编码读回和块视图重解释尚未实现；不伪称完整BC。对RGBA解码图像直接读回较小BC缓冲的请求会明确终止，避免越界。查询的能力与支持范围仍需实机核对。上传规则参照[Khronos VkBufferImageCopy](https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferImageCopy.html)。
