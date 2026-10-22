# DXVK-Sarek 1.14.1 可选接入

使用作者 [1.14.1发布包](https://github.com/pythonlover02/dxvk-sarek/releases/tag/v1.14.1)，源码标签对应 `0c0e8f6f62c7cb37dbf5d38413517efa00c08fef`。打包的DLL未修改；压缩包9821269bytes，SHA256 `07a6d5d889e9810ce856511e853cd72d2f547ec12c0dc5d7049b32e4bfa05c25`。来源记录在 `app/src/main/assets/dxwrapper/sarek-source-lock.json`，许可证保留于 [DXVK-Sarek-LICENSE](licenses/DXVK-Sarek-LICENSE) 及原压缩包。公开仓库不上传该运行包，`node scripts/fetch-bundled-assets.mjs` 按运行清单恢复。

选用理由：作者的 [DXBC选项实现](https://github.com/pythonlover02/DXVK-Sarek/blob/0c0e8f6f62c7cb37dbf5d38413517efa00c08fef/src/dxbc/dxbc_options.cpp) 会在真实ARM vendor ID上关闭未绑定纹理优化；新版还包含内存分配与D3D10修复。项目的 [PR36](https://github.com/pythonlover02/DXVK-Sarek/pull/36) 根据实际Clip/CullDistance能力生成shader。本机已保存日志中这两项均为1，所以不能把该PR直接当作本机Superposition异常的根因或现成解法。原项目也说明缺失功能可能导致图形问题；接入不等于设备通过。

在DXVK版本列表选择 `1.14.1-sarek`，针对DX8/9/10/11游戏使用；若旧配置启用了VKD3D，先选None以显示1.x版本。默认版本保持现有设置。切回1.10.3等版本使用原有解包路径恢复。Sarek自己的D3D8/D3D9组件保持配套，避免被旧D8VK覆盖；D3D10上层桥接恢复Wine原组件。应用保留物理ARM厂商标识以触发Mali规则，游戏的可选DXGI显卡识别兼容仍单独生效。

首轮比较默认同步完整shader编译（`DXVK_SHADER_COMPILATION_METHOD=none`）及sliced限帧等待。用户显式环境仍可覆盖。固定压缩包先做整包摘要验证，只解出十个已知的普通DLL文件，全部暂存成功才替换；不提取源码、符号链接或任意压缩路径。失败中止启动，不将失败版本记为已安装。

0.1.39仅完成离线构建/v2验签和产品解包器短查：Win64十个DLL，Win32五个DLL。没有平板安装或GPU渲染结果。Superposition仍须复查；此选项不实现DX12，也不改变艾尔登法环当前接口初始化的阻塞。
