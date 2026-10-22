# X11 窗口与 Warzone 实机定位

## 已确认的修复

Warzone 4.7.0 的 `Entering main loop` 日志位于 `osSpecificPostInit_Win()` 之前。后者在 Wine 下显示原生提示；因此仅看到该日志不能推断主循环或 GPU 挂死。[上游 main.cpp](https://github.com/Warzone2100/warzone2100/blob/4.7.0/src/main.cpp) 是这次定位依据。

0.1.13 的窗口快照显示弹窗 drawable 46137348 排在主窗口 46137345 和客户区 41943148 之前。纹理已分配、已更新，但后画的主窗口覆盖弹窗。0.1.14 根据 `WM_TRANSIENT_FOR` 在事件发生时修正实际兄弟窗口顺序，同时用于绘制与指针命中；忽略自引用、无效所属节点和环。不在每帧排序。符合 [Windows owned window 顺序](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#owned-windows) 所需的基本行为。

修复后快照顺序为主窗口 → 客户区 → 弹窗，弹窗 transient_for 指向主窗口。`artifacts/mali-compat/v014-visible-wine-dialog.png` 实际拍到完整的 Wine 提示。这不是仅以 JSON 推测显示成功。

另修复 `ReparentWindow` 丢弃坐标、不通知/重映射，以及 `DestroySubwindows` 误删父窗口、缺少 switch break 导致下一请求解析错误。依据 [XReparentWindow 规范](https://xorg.freedesktop.org/archive/X11R7.5/doc/man/man3/XReparentWindow.3.html)。一次实际线协议检查通过坐标、事件、拒绝环及保留父节点，证据 `artifacts/mali-compat/x11-window-protocol.txt`。未宣称完整 X11 实现。

## 游戏结果和边界

修正后普通启动已绘制主菜单，Wine 提示确认后自动打开的 IE 页面会遮挡游戏。诊断 helper 只正常确认已知提示，并关闭本应用 Wine 会话内该提示打开的 IE 窗口；没有修改 Warzone EXE 或游戏逻辑。

随后使用游戏官方参数 `--game=CAM_1A` 进入第一关。参数必须含等号，源码自带解析器不接受 `--game CAM_1A`；后者会立即退出 1，已修正诊断程序。`warzone-campaign-run.log` 明确加载 `wrf/cam1/cam1a.gam`，`warzone-campaign-current.png` 实际显示地形、车辆、HUD 与游戏指引，helper 最终正常退出 0。

尚未完成单位移动/战斗与持续稳定性检查。截图中暂停/过场时 HUD 的 FPS 不能当作可玩性能。此次进入关卡不等于手柄适配或 AAA 验收。

仍有偶发 `PHYSFS_openRead` 资源错误：不同启动可能报不同 PNG，下一次又成功。原始数据档与主机摘要一致，档内存在文件，FD 未接近上限；没有通过反复复制数据或提高系统 FD 限制掩盖问题。该稳定性问题仍待定位。

## 诊断本身的修复

0.1.13 暴露了调试关闭时 DebugDialog 未创建的空对象崩溃；第一个 0.1.14 候选又暴露旧文件名为空。最终 0.1.14 懒创建并采用应用私有日志，实机复查存活。0.1.15 继续将 UI 日志限定为 600 行、每行 4096 字符和单个覆盖写入的约 1MiB 文件，退出关闭文件/解除回调；避免每条日志扫描全文算宽度。子进程输出和等待改为结束即退出的线程，避免每次创建不关闭的单线程池。取消重复输出全部日志到 logcat。

ZUI 的应用列表权限提示和 Settings ANR 仍出现过；只选择等待或拒绝多余权限，没有停止 Settings、修改其数据或扩大权限。不能称 Glide 调整已经消除此设备问题。
