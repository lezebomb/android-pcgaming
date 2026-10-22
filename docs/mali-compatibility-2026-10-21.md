# Mali-G615 适配研究与 DX12 实验路径

## 结果

目标设备 TB375FC / Mali-G615 MC5 / 原生驱动44.1.0。旧路径 DXVK2.3.1 + VKD3D2.8、未强制功能等级时，FL12_0创建失败0x80070057；FL11_0诊断成功。通过可选Mali兼容层和VKD3D2.14.1后，FL12_0创建成功。一次实机基础命令检查完成60帧清屏、GPU拷贝、Fence同步、CPU显存回读及Present，四色像素逐帧匹配，截图实际显示正确，探针返回0。

随后已补一次 SM6.0 顶点/像素绘制与 BC1/3/5/7 采样实测，见后文；仍未完成复杂游戏着色器、纹理流送压力或3A游戏。早期清屏探针每帧主动Sleep(100)，截图HUD约9FPS不能用于估计游戏性能。兼容层会伪装部分功能声明，FL12_0、ShaderModel6.0、LogicOp=1等查询值不能独立证明其所有语义可用。

证据：`artifacts/mali-compat/{d3d12-capability.txt,d3d12-present.txt,dx12-present.png,wine.log,present-wine.log,vkd3d.log}`。0.1.8应用内集成复查另存`v018-*`，以交接最后追加的实际结果为准。

## 为什么采用此方案

- 开发者主仓库 https://github.com/leegao/compat_layer ：针对Mali原生Vulkan补充push descriptor、nullDescriptor模拟；可选dense sparseBinding模拟。列表含G615及44.1.0，与本机匹配。README所谓无问题属于兼容配置表，并非艾尔登法环通过声明。
- 代码区分了实际模拟与功能声明伪装。`src/spoof_profile.cpp`把若干功能报为true、创建设备时再掩去底层不支持的项；例如logicOp没有因此获得完整硬件实现。不能把查询值包装成实际支持度。
- sparseBinding通过dense后备资源与数据拷贝实现，会增加内存与带宽开销；上游默认预算8GiB不适合本机8GB总内存。本工程实验预设默认限制该部分为1GiB，超额返回分配失败。此预算不是整个游戏进程的内存上限，不保证免OOM。
- https://github.com/FristOneRR/FristOneRR-Panvk-Driver 的README明确CSF G610/G710/G615/Immortalis不支持，排除此G57/JM驱动。没有安装到系统或更改内核驱动。
- 现有资产`graphics_driver/wrapper.tzst`版本记录为GameNative Unified Vulkan Wrapper1.3.303，已存在push descriptor开关和Vulkan layer搜索路径。本次没有盲目用leegao旧Wrapper二进制整体替换它。

## 可复现构建、使用和回退

`scripts/prepare-mali-compat.mjs`固定上游Git源码树`22960fb33a91226a9ab615288b0846ef7eac456b`，仅获取Android源代码和必要SPIRV-Tools静态依赖，按Git blob SHA与文件大小逐项校验，不执行下载来的脚本。`scripts/build-mali-compat.ps1`使用NDK28.2.13676358/CMake3.22.1，静态C++运行库，产出ARM64层；锁文件与MIT通知随资产保存。

构建命令（工程根目录）：

```powershell
D:/nodejs/node.exe scripts/prepare-mali-compat.mjs
./scripts/build-mali-compat.ps1
```

实测二进制16813448bytes，SHA256 `cb9c80d3229f1703d2854147c4f05eba950761c54d71b5f8db93abf3fbf9d61d`。0.1.8将相同文件打包到`assets/mali_compat/`；应用仅在DXVK配置显式`maliCompat=1`时检查并安装自身私有层，使用explicit manifest，不会全局自动启用。启动时校验SHA256，按需原子替换库文件，合并已有layer列表，不改变系统驱动。普通游戏默认关闭。

在游戏快捷方式的DXVK设置勾选“Mali-G615 DX12 实验兼容”，保存时选择DXVK2.x（必要时2.3.1）、VKD3D2.14.1和自动功能等级。取消勾选即可停止由该配置启用；手工环境变量仍属于用户显式覆盖，若手动设置过VK_INSTANCE_LAYERS，应同时自行移除相应层。`COMPAT_EMULATE_SPARSE_BINDING=1`与`COMPAT_SPARSE_COMMIT_BUDGET=1073741824`为默认值，保留显式环境覆盖。

`scripts/install-mali-compat.ps1`只用于旧开发包的手动实验安装；`-Remove`仅删除本开发包私有的两个层文件，不删除游戏。0.1.8正常使用不需要此脚本。

## 下一步

2026-10-22凌晨追加：已在同一平板完成一次真正的SM6.0顶点/像素绘制（12帧，GPU回读梯度匹配）；随后单次四格式纹理检查完成BC1/BC3/BC5/BC7上传、SRV描述符绑定、采样、回读及显示，12帧result=0。像素分别为255,0,0,255 / 0,255,0,255 / 64,192,0,255 / 0,0,255,255（BC7端点预期254，容差2）。BC7常量块按Microsoft官方mode6布局生成。证据artifacts/mali-compat/{d3d12-shader.txt,d3d12-bc-textures.txt,vkd3d-bc.log}。这是基本着色器/单块纹理链路，不包含纹理流送压力、大体量内存、复杂游戏着色器或性能结论。不再重复这些检查。

用户最终验收为真实3A游戏稳定、流畅游玩和完整手柄适配，当前尚未达到。Steam正版艾尔登法环55.1GB已完整导入并匹配摘要，原版接口初始化后退出，尚未进入DX12。不能以探针结果宣称艾尔登法环可玩。Warzone黑屏已确认包含被覆盖的Wine模态提示，0.1.14修正堆叠后已实际进入第一关3D场景；尚未完成操控和稳定性，见window-compatibility-2026-10-22.md。

## 22日稀疏分配预算修复

0.1.17集成同一固定上游树的两文件本地修复，`scripts/patch-mali-compat.mjs`先还原本补丁、验证原Git blob，再可重复应用；构建锁包含脚本和修改文件SHA256。修复后的ARM64库16816400bytes，SHA256 `171d6d533c91ae9789fb6b144018b331589594aa50fe95835126c013c1df27e6`；原版备份保留在artifacts/mali-compat/libdxvk_mali_compat_layer-upstream.so。仍默认关闭，仅快捷方式显式选用，关闭开关即可回退到未启用层的路径。

缓冲区预算原先按内存需求大小扣除、按请求大小返还，对有差额的分配会累积预算占用；现在另存committedSize并按相同数额返还。缓冲区构建未注册到全局表前调用底层绑定，避免自身查找错误；缓冲区/图像的底层绑定失败现清理内存、资源、预算并返回真实错误。

仅一次针对新路径的实际检查通过：64次创建/释放，4096字节预算填满1024个4字节存活分配后返回VK_ERROR_OUT_OF_DEVICE_MEMORY，全部释放后再分配成功，超额8192字节也被拒绝。`artifacts/mali-compat/vulkan-sparse-budget.txt`与sparse-wine.log。本机该缓冲区req.size=4、alignment=64，没有padding差额，明确记录padding_regression_case_available=0；未声称实机重现或A/B验证了padding泄漏，也未故意制造底层BindMemory失败。预算仅该模拟层的分配记账，不是全部GPU实际驻留或整个游戏内存上限。

## 23日设备退出生命周期修正

原wrapper+Sarek1.14.1已经实际显示Superposition GUI。一次原生abort断点定位到兼容层`device::~device()`，其调用方是进程退出的`__cxa_finalize`。引擎自身日志在此前已记录Benchmark停止及Shutdown；因此这次abort是退出清理的第二个问题，不能将它当作加载失败或Mali驱动崩溃的根因。

0.1.46令设备注册表与线程等待锁保持到原生进程结束，避免游戏遗留的Vulkan设备在静态析构时销毁仍joinable的线程。正常`vkDestroyDevice`仍释放设备资源；现在在驱动释放句柄前保存dispatch key，以持有的shared_ptr完成清理，并在等待线程使用的同一把锁下改变停止条件，避免丢失唤醒。这个改动不增加或伪造GPU功能。

固定上游blob仍由补丁脚本核对，构建锁记录五个本地修改文件。新ARM64库SHA256为`f023a7820abcde1af21e7fc8de1e3ca0d77682e66d93af048a136873a6e9c79d`。已完成ARM64/APK构建与v2验签，真实场景另行记录；退出安全修正不能代替3D与稳定性验收。
