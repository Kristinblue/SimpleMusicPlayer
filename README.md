# SimpleMusicPlayer

1. 安装 Qt **6.12.0 → MinGW 13.1.0 64-bit**、Build Tools 中的 **MinGW 13.1.0 64-bit**，以及 CMake 3.25 或以上。
2. 默认 Qt 路径是 `D:/MySoftware/QT`。路径不同时，修改 `CMakePresets.json` 的 `SMP_QT_ROOT`，以及 `.vscode/settings.json` 的 `cmake.debugConfig.miDebuggerPath`。
3. VS Code 安装 **C/C++** 和 **CMake Tools**，打开整个工程，选择 **Qt MinGW Debug**。
4. **Ctrl+Shift+B** 编译，**F5** 调试，**Ctrl+F5** 运行；启动配置选择 **Debug SimpleMusicPlayer (GDB)**。


生成文件在 `bin/Debug`；Release 使用任务 **CMake: build Release**。构建会自动复制运行需要的 Qt DLL 和插件；发送可运行版本时要发送整个输出文件夹。

界面在 `src/main.cpp`，功能模块写在 `src/data.c`、`src/player.c`，新增 `src` 下的 `.c` / `.cpp` 自动参与编译。接口与分工见 [模块接口](docs/模块接口.md)。队友只写 C 模块时不用安装 Qt。

目前是界面预览，示例歌曲没有音频，收藏退出不保存。共享源码时不用发 `build` 和 `bin`。
