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
add_executable(SimpleMusicPlayer WIN32 ${SOURCES})
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

# 只复制程序运行需要的 DLL / 插件，不复制整个 Qt 或编译器。
get_target_property(SMP_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(SMP_QT_BIN "${SMP_QMAKE}" DIRECTORY)
find_program(SMP_WINDEPLOYQT windeployqt HINTS "${SMP_QT_BIN}" NO_DEFAULT_PATH REQUIRED)
add_custom_command(TARGET SimpleMusicPlayer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E env
        --modify "PATH=path_list_prepend:${SMP_GCC_BIN}" --
        "${SMP_WINDEPLOYQT}" --no-translations --no-opengl-sw
        --no-system-d3d-compiler "$<TARGET_FILE:SimpleMusicPlayer>"
    COMMENT "Copying Qt runtime files"
    VERBATIM
)
