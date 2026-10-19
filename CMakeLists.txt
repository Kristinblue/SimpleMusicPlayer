cmake_minimum_required(VERSION 3.25)

# 优先使用 GCC 自己的 DLL，避免与其他软件的同名 DLL 冲突。
# 路径来自 CMakePresets.json，使用 Qt 配套的 MinGW。
get_filename_component(SMP_GCC_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
set(ENV{PATH} "${SMP_GCC_BIN};$ENV{PATH}")
set(SMP_TOOL_LAUNCHER
    "${CMAKE_COMMAND}" -E env
    --modify "PATH=path_list_prepend:${SMP_GCC_BIN}" --
)
set(CMAKE_C_COMPILER_LAUNCHER ${SMP_TOOL_LAUNCHER})
set(CMAKE_CXX_COMPILER_LAUNCHER ${SMP_TOOL_LAUNCHER})
set(CMAKE_C_LINKER_LAUNCHER ${SMP_TOOL_LAUNCHER})
set(CMAKE_CXX_LINKER_LAUNCHER ${SMP_TOOL_LAUNCHER})

project(SimpleMusicPlayer LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(NOT WIN32 OR NOT MINGW)
    message(FATAL_ERROR "Use Windows with MinGW-w64 GCC.")
endif()

# 自动收集 src 及子目录中的 C/C++ 源文件。
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.c"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
# 收集公共头文件并列入目标，让 Qt Creator 可以显示和打开这些接口文件。
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h"
)
add_executable(SimpleMusicPlayer WIN32 ${SOURCES} ${HEADERS})
target_include_directories(SimpleMusicPlayer PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties(SimpleMusicPlayer PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
)
target_compile_options(SimpleMusicPlayer PRIVATE
    -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8
)
# 安装器提供动态 Qt，所以移除原来的全局 -static。
target_link_libraries(SimpleMusicPlayer PRIVATE Qt6::Widgets winmm)

# 可选界面集成测试使用隔离的临时数据，不依赖 Qt Test，也不进入主程序源文件列表。
option(SMP_BUILD_UI_TESTS "Build UI integration tests" OFF)
if(SMP_BUILD_UI_TESTS)
    enable_testing()
    add_executable(FavoritesUiTest tests/favorites_ui_test.cpp src/data.c src/player.c)
    target_include_directories(FavoritesUiTest PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(FavoritesUiTest PRIVATE Qt6::Widgets winmm)
    set_target_properties(FavoritesUiTest PROPERTIES C_STANDARD 17 CXX_STANDARD 17)
    target_compile_options(FavoritesUiTest PRIVATE
        -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    add_test(NAME favorites_ui COMMAND FavoritesUiTest)
    set_tests_properties(favorites_ui PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(PlaybackUiTest tests/playback_ui_test.cpp src/data.c src/player.c)
    target_include_directories(PlaybackUiTest PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(PlaybackUiTest PRIVATE Qt6::Widgets winmm)
    set_target_properties(PlaybackUiTest PROPERTIES C_STANDARD 17 CXX_STANDARD 17)
    target_compile_options(PlaybackUiTest PRIVATE
        -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    add_test(NAME playback_ui COMMAND PlaybackUiTest)
    set_tests_properties(playback_ui PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# 只复制程序运行需要的 DLL / 插件，不复制整个 Qt 或编译器。
get_target_property(SMP_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(SMP_QT_BIN "${SMP_QMAKE}" DIRECTORY)
find_program(SMP_WINDEPLOYQT windeployqt HINTS "${SMP_QT_BIN}" NO_DEFAULT_PATH REQUIRED)
add_custom_command(TARGET SimpleMusicPlayer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:SimpleMusicPlayer>/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/data/songs.txt" "$<TARGET_FILE_DIR:SimpleMusicPlayer>/data/songs.txt"
    COMMAND "${CMAKE_COMMAND}" -E env
        --modify "PATH=path_list_prepend:${SMP_GCC_BIN}" --
        "${SMP_WINDEPLOYQT}" --no-translations --no-opengl-sw
        --no-system-d3d-compiler "$<TARGET_FILE:SimpleMusicPlayer>"
    COMMENT "Copying Qt runtime files"
    VERBATIM
)
