# SimpleMusicPlayer

1. 安装 Qt **6.12.0 → MinGW 13.1.0 64-bit**、Build Tools 中的 **MinGW 13.1.0 64-bit**，以及 CMake 3.25 或以上。
2. 首次获取工程后，将 `CMakePresets.example.json` 复制为 `CMakePresets.json`，把 `SMP_QT_ROOT`、`SMP_QT_VERSION`、`SMP_MINGW_FOLDER` 改成自己安装的路径和版本。已有本机配置时继续使用，不覆盖。`CMakePresets.json` 和 `.vscode/settings.json` 按团队约定只保存在本机，不上传。
3. VS Code 安装 **C/C++** 和 **CMake Tools**，打开整个工程，选择 **Qt MinGW Debug**。
4. **Ctrl+Shift+B** 编译，**F5** 调试，**Ctrl+F5** 运行；启动配置选择 **Debug SimpleMusicPlayer (GDB)**。

使用 Qt Creator 时，打开根目录 `CMakeLists.txt`，选择 **Qt MinGW Debug** 并配置项目。使用 VS Code 调试时，还需在本机 `.vscode/settings.json` 中配置 `cmake.debugConfig.miDebuggerPath` 为 Qt 配套的 `gdb.exe` 路径。


生成文件在 `bin/Debug`；Release 使用任务 **CMake: build Release**。构建会自动复制运行需要的 Qt DLL 和插件；发送可运行版本时要发送整个输出文件夹。

界面在 `src/main.cpp`，
功能模块写在 `src/data.c`、`src/player.c`，
新增 `src` 下的 `.c` / `.cpp` 自动参与编译。
接口与分工见 [模块接口](docs/模块接口.md)。队友只写 C 模块时不用安装 Qt。

收藏已接入：点击空心爱心添加收藏，点击红心取消收藏，两侧列表同步更新，立即保存并在重启时恢复。音频模块尚未接入，示例歌曲没有音频。

歌单存放在 `data/songs.txt`，每行是 `编号|歌名|歌手|专辑|WAV路径`；收藏自动保存在 `data/favorites.txt`，首次使用时可以没有该文件。Qt Creator 启动和双击 `bin/Debug` 中的程序均使用工程根目录的数据。发送可运行版本时，构建会将歌单复制到输出文件夹的 `data` 目录；要携带现有收藏，另将 `data/favorites.txt` 复制到输出的 `data` 目录，构建不会覆盖收藏。接收者将整个输出文件夹放在工程以外的位置运行。

独立数据测试见 `tests/data_test.c`，界面集成测试见 `tests/favorites_ui_test.cpp`，运行方法见模块接口文档。共享源码时不用发 `build` 和 `bin`。原题的收藏夹“修改”功能仍需补充，当前实现的是查询、添加、取消和保存。
