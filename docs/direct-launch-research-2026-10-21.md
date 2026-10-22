# 本地 EXE 直启与自动启动

用户明确要求游戏通过自己的 EXE 直接启动，后续以完整游戏目录作为输入。研究及实现均未将 Steam 客户端界面作为统一入口。

## 读取的现成实现

- [Winlator app：getWineStartCommand](https://github.com/brunodev85/winlator-app/blob/a030f552f452158a2db64fdb32b490fa19c0b48d/app/src/main/java/com/winlator/XServerDisplayActivity.java#L947)：快捷方式或 exec_path 提供目标；从目标推导工作目录；winhandler /dir 调用程序。不是所有游戏统一交给 Steam。此处仅参考结构，当前工程已有同源的 WinlatorMali 实现。
- [GameNative：Custom Game 启动分支](https://github.com/utkarshdalal/GameNative/blob/f3f954f54b98d75b79feb7b8cdff41c7d4d935a7/app/src/main/java/app/gamenative/ui/screen/xserver/XServerScreen.kt#L5001)：使用用户选定 EXE，缺省仅在主程序唯一时自动检测，映射游戏目录至 A:，设置 workingDir 后直接运行。本工程沿用私有 C: 导入，避免另复制已经导入的 55 GB 游戏。
- [GameNative：IntentLaunchManager](https://github.com/utkarshdalal/GameNative/blob/f3f954f54b98d75b79feb7b8cdff41c7d4d935a7/app/src/main/java/app/gamenative/utils/IntentLaunchManager.kt)：外部启动请求独立于库界面。本工程以已保存的容器/快捷方式为入口，校验文件处于该容器 Desktop 内，不接受外部传入任意命令。
- [GameNative：Bionic Steam 路径](https://github.com/utkarshdalal/GameNative/blob/f3f954f54b98d75b79feb7b8cdff41c7d4d935a7/app/src/main/java/app/gamenative/ui/screen/xserver/XServerScreen.kt#L5058)：游戏 EXE 直接运行，但 Android 进程中已运行原生客户端接口；免 GUI 不等于没有接口依赖。该项目还有替换客户端 DLL/其他不同授权处理分支；没有将它们照搬到此工程。其明示专有辅助库也没有复制。研究通用流程，新增 Java 实现独立编写。
- [同一提交的 THIRD_PARTY_NOTICES](https://github.com/utkarshdalal/GameNative/blob/f3f954f54b98d75b79feb7b8cdff41c7d4d935a7/THIRD_PARTY_NOTICES#L107) 明确：登录引导 `libsteambootstrap.so` 的源码不在仓库中，二进制为专有；运行时用独立登录流程取得的 refresh token 引导用户自己的客户端。因此不能把这条路径描述为复制开源 Java/Kotlin 就能完成，也没有从 PC 提取登录凭据。

## 0.1.11 的实际改动

- `WindowsLaunchCommand` 分离 EXE、工作目录和参数；兼容旧快捷方式附带参数，支持引号和空格。传递完整目标路径，工作目录可与 EXE 所在目录不同；不再仅凭 basename 在工作目录中寻找 EXE。
- `DirectLaunchDialog` 提供「直接启动设置」；可添加容器 C: 或已有映射驱动中的完整游戏，无需再次导入。保存前检查文件、工作目录和单行参数。
- 新建导入快捷方式明确保存 `executablePath` 和正确的 `[Extra Data] container_id=`。
- 游戏库、Android 桌面入口、打开应用自动启动共用同一本地启动链。桌面入口经过 MainActivity，再进入非导出的运行界面。
- 自动启动默认关闭，逐游戏选择；打开应用后 3 秒倒计时，可立即启动、本次取消或关闭。退出/重启返回时携带 skip_auto_start；前后台切换和重建不会重复触发。
- 未增加开机广播、自行后台拉起、扩大系统权限或更换游戏授权文件。
- 运行阶段读取自己的版本号改用 BuildConfig，移除又一处 PackageManager 查询；退出默认回游戏库。

## 艾尔登法环的边界

60 文件/55099171926 bytes 已在此容器中完成逐文件校验。正常直启目标为原版 `eldenring.exe`，工作目录为其 Game 目录。`steam_appid.txt` 仅避免 SteamAPI_RestartAppIfNecessary 拉起 GUI；原版 SteamAPI_Init 实测返回 false，游戏在约 1.3 秒退出，未进入 DX12。新启动器设置并不补齐这个接口，尚未得到菜单、场景或可玩帧率；不能将直启设置保存成功写成游戏兼容通过。

当前先交付可用的本地启动入口，后续继续原版客户端接口兼容与实际图形执行开发，避免重复客户端 GUI 的安装更新排查。保留原始游戏文件与已有日志。

## 23:05进一步核对宿主实现

同一固定提交的[SteamBootstrap.kt](https://github.com/utkarshdalal/GameNative/blob/f3f954f54b98d75b79feb7b8cdff41c7d4d935a7/app/src/main/java/app/gamenative/SteamBootstrap.kt#L120)实际通过ProcessBuilder启动libsteambootstrap.so独立进程，以状态文件INIT/READY/FAILED/CLOSED等待准备完成，并向宿主提供独立登录的账号/refresh token。故上文沿启动分支注释称「Android进程中」应精确为**Android侧独立宿主进程**；不能只调用游戏EXE就提供该能力。本工程未复制专有宿主或PC凭据，登录状态缺失仍是具体依赖，非GPU错误证据。
