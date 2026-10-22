# Mali 显示后端调研（2026-10-22）

平板已有可调用的系统 Mali Vulkan 驱动；现有实机绘制证明不是完全缺驱动。当前需要改进应用内适配、资源转换及窗口提交，不能把“安装一个驱动”当作所有游戏可玩的保证。

| 路线 | 开发者实现 | 本项目状态 |
|---|---|---|
| System Mali + wrapper | Android系统libvulkan/厂商Mali，外加Mesa的X11/WSI及格式处理 | 当前默认。固定原库有明确SPIR-V字节/字数越界，副本单样本回归已过；复杂场景仍未通过 |
| Mali兼容层 | leegao为DXVK/VKD3D补部分缺失功能，同时有功能声明覆盖 | 已默认关闭、显式启用，预算/SM6/BC基础执行已证实；不当作AAA验收 |
| Vortek | Winlator客户端通过共享内存/Unix socket连Android服务端；服务端用系统Vulkan和AHardwareBuffer更新窗口 | 默认关闭的可选路线。客户端/服务端与每游戏选择已集成，0.1.34已在实机Warzone第一关显示3D；尚未完整验收 |
| Turnip | Mesa的Adreno Vulkan驱动 | 不用于Mali-G615 |

来源：[Vortek主仓库](https://github.com/brunodev85/vortek)、[Winlator明确加入Mali支持的更新](https://github.com/brunodev85/winlator/releases/tag/v10.0.0)、[GameNative Mali/DX12提交](https://github.com/utkarshdalal/GameNative/commit/17da0b3)、[Mali兼容层](https://github.com/leegao/compat_layer)、[Mesa Turnip适用GPU](https://docs.mesa3d.org/drivers/freedreno.html)。

本地源码对照：upstream/winlator根记录的客户端commit b1730c5def9b575672e671aee11d79ae7adc63d1，app子模块a030f552f452158a2db64fdb32b490fa19c0b48d。原始客户端178930bytes，SHA256eef892505c77fb6f3cc7bc78a151662ffb77bab934e5eeff8ead7270a6734c82。客户端/服务端serializer正规化换行后完全一致（2761a1cad4c6812004e57c210b57507800b64b28ba33824b936d9daa37461253）。GameNative旧Java接口只有(I)J，但该Winlator服务端要求(IZ)J，不能混接。

目前已集成配套Bionic客户端、Android服务端、Xlib/XCB surface、每游戏回退选择与GPU专用AHB呈现，缓存及socket均在本应用目录；不依赖其他应用的私有目录。当前客户端SHA256为0a472ab04badb7989f422dab8d304be245effaad857bdefe9138c240b0c44a0f。构建来源与配套确认规则见源码准备脚本和source-lock。离线构建不等于实机显示通过。

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

## 0.1.31：Android缓冲格式与驱动用途

旧实现枚举sRGB图像却将其直接绑定UNORM AHB，且固定AHB用途。现只枚举驱动确认可导入的RGBA8_UNORM，颜色空间仍为SRGB_NONLINEAR；先查询外部图像能力与AndroidHardwareBufferUsage，传递驱动返回的用途（含厂商位），导入时严格核对格式、尺寸、层和用途。保留原生错误码便于定位。规范依据：[AHB导入格式要求](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryAllocateInfo.html)。

表面格式/模式/图像枚举限制到调用方容量，零容量输出在客户侧返回INCOMPLETE；计数查询不再泄漏分配。单缓冲后端只声明FIFO与实际不透明合成。EGL导入使用当前display，并排除之前遗留的GL错误。仅ARM64/Java构建和签名通过，未在平板验证。

## 0.1.32–34：实机驱动反馈

先在Vortek上完成Win64 Vulkan的64次present与窗口resize重建，再发现Warzone要求>=2图像；0.1.33为每图像分配独立AHB/所有权命令并轮转2–3缓冲，断言消失但场景仍黑。

上游将普通Fence等待转成SYNC_FD导出，导出会影响payload，已完成时还可能返回-1，原传FD路径没有正确处理。0.1.34改为原生GetFenceStatus轮询，等待期间不持全局IPC锁，用单调纳秒截止；取消强制export，ResetFences返回真实结果。依据：[vkGetFenceFdKHR](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetFenceFdKHR.html)。

该次实机已从相同初始化黑屏进入Warzone第一关，地形/车辆/HUD可见，虚拟手柄关闭指南并选取7单位。移动与战斗尚未证明；HUD显示的0 FPS未校准，不能据此报告实际帧率。短会话由有3分钟上限的helper关闭，未作长稳验收。Vortek仍缺完整BC6/7和DX12能力，不继承另一条Mali层的通过结论。

## 0.1.35：显存结构传输修正

Superposition/DXVK实机日志出现第二堆随机flags。上游数组把native sizeof16的VkMemoryHeap按wire12字节整段memcpy，导致后续堆错位；预算数组又将元素数量当字节数量。现逐项传输堆、完整传输VkDeviceSize预算数组，客户/服务端统一reply v4。直接编译产品serializer在平板自己UID检查二堆与最后预算项通过；新DXVK日志flags已有效，Qt仍0xc0000005，无benchmark场景。新2.3.1另因缺VK_EXT_robustness2而失败，不伪装扩展或渲染成功。

### 离线候选 0.1.36：共享内存路径

修正AHB BLOB缺GPU_DATA_BUFFER、未查询memoryTypeBits/allocationSize、静默改内存类型和覆盖pNext的问题；保留DEVICE_ADDRESS和游戏实际资源的dedicated信息，移除绑定到虚拟占位buffer的错误dedication。按最终原生大小复查预算，返回实际错误。DMA-BUF查询导入类型；fd=0作为有效描述符。客户端映射大小改64位，修正VK_WHOLE_SIZE偏移、非页对齐offset、placed pointer/保留地址空间、失败fd与free时映射回收。

规则依据：[Vulkan内存分配](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryAllocateInfo.html)、[专用分配](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryDedicatedAllocateInfo.html)、[fd属性查询](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetMemoryFdPropertiesKHR.html)。此次离线修改不代表已解决Superposition异常或完成AAA验收；资源创建的external handle声明及非coherent缓存维护仍需继续核查。

### 离线候选 0.1.37：shader复用与管线错误

上游首次管线会修改ShaderModule共享源码并在结束时释放，之后不同顶点布局沿用首次生成的模块；并发管线也会写同一份源码。现保留模块拥有的原始SPIR-V，每次管线使用私有转换副本，按顶点/片元依赖处理而不依赖stage排列，临时原生shader在管线创建完成后释放。编译失败阻止继续调用原生管线创建，避免传入未完成的模块。

SPIR-V增加零长度/越界指令及所读取操作数的界限检查，OpName复制带容量与结束符检查，Location数字缓冲支持完整uint32。它不是完整SPIR-V验证器。管线回复改准确读写循环及小块传输，处理fd=0/句柄0、断开连接与SIGPIPE，不再用大batch栈数组；消息的字节结构保持不变。私有转换可能增加模块创建开销，实际性能与图形结果待实机。

### 离线候选 0.1.38：原生映射生命周期

此前只有guest的fd mmap，没有调用驱动vkMapMemory，却直接转发Flush/Invalidate。现首次guest Map建立原生整段映射并保留到allocation释放前，再unmap；native Map失败通过不含fd的错误回复返回，避免用无效fd发送而卡住等待。依据：[VkMappedMemoryRange 00684](https://docs.vulkan.org/refpages/latest/refpages/source/VkMappedMemoryRange.html)。这是原生调用有效性修正；跨进程noncoherent别名缓存的实际行为仍需Mali实机检查，不伪造HOST_COHERENT、不声称黑屏全部解决。

### 离线0.1.39：开发者Sarek补丁参考

已从作者源码确认ARM未绑定纹理优化开关、Clip/Cull能力门控与新版分配器，并把固定1.14.1原发布包作为可选图形版本接入，保留物理ARM标识。同步编译用于初次画面对比，默认旧版本不变。详见 [来源、许可证与边界](dxvk-sarek-1.14.1.md)。当前只是build/v2与解包短查，不是实机显示通过。

### 离线0.1.40：补齐表面查询与Acquire2

确认AcquireNextImage2仍使用上游把0到10状态当index的旧解码，TIMEOUT=2可能被当作第3张图；现在两入口共用校验回复路径，错误/超时不写index，成功回复校验4字节。host两入口均独立返回状态与index（成对协议v5）。

系统可能枚举VK_KHR_get_surface_capabilities2，但原client缺两个对应入口。现用本后端已实现的表面能力/格式查询补齐，保持调用方sType/pNext，正确处理容量和INCOMPLETE；不把X11 window id传给Android driver当surface。统一屏蔽未实现的present-wait/id、display-timing、HDR/维护/可变格式与原生平台surface、debug callback等扩展，防止枚举有功能但调用无入口。Swapchain销毁前等待本后端内部ownership命令结束。

依据：[Acquire2状态](https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImage2KHR.html)、[格式查询2容量语义](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceSurfaceFormats2KHR.html)。一次离线产品适配器短查vortek_wsi_result=0，覆盖容量/结构头、超时不改index与屏蔽范围。ARM64两端编译通过；实机效果仍未知。

### 0.1.41离线显示恢复

GPUImage原有100ms导入退避，却被Vortek首次失败直接转为DEVICE_LOST，无法利用重试。现在GL线程返回明确结果，只有有效AHB且尚无可用纹理时才重试，保持FOREIGN所有权到实际拷贝完成；750ms内仍不可用则终止会话。排队超过1秒可取消，已经运行的拷贝不取消、不允许生产者复用，5秒上限仍保留。

窗口输出使用原目标drawable，避免预检查与入队间resize导致旧AHB拷进新窗口；目标身份变化返回OUT_OF_DATE，不冒充GPU丢失。窗口content用volatile保证GL线程看到变更。失败拷贝不再算成功帧，glFinish后的错误也纳入结果。只完成离线构建，未确认实机恢复或稳定性。

### 0.1.42离线呈现队列

原实现未传入vkQueuePresentKHR的queue，一直使用后端固定queue0。现在使用完整queue/present-info请求（成对协议v6），一次提交同批已准备图像的FOREIGN释放命令，只消费一次等待信号；一次GPU完成等待后才逐窗口GL拷贝，减少多窗口重复等待。即使所有目标过期，等待信号也会被消费；提交前内存失败保留图像/信号状态，提交后不能保证状态的失败终止设备会话。依据：[vkQueuePresentKHR队列和错误语义](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)。

一次直接编译产品present_queue.c的CPU记录接口短查覆盖调用队列、两窗口单次等待、部分/全部目标失败、提交前内存错误和提交后错误，无真实GPU。已编译ARM64两端与APK，未安装实机。
