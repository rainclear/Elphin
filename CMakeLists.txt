cmake_minimum_required(VERSION 3.20)

# 项目名称、版本及语言
project(Elphin 
    VERSION 1.0.0
    DESCRIPTION "High-performance In-Memory Key-Value Server in C++20"
    LANGUAGES CXX
)

# 强制要求 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 将全局二进制输出路径设置为项目根目录下的 bin/ 目录（必须在 add_executable 之前！）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 编译选项设置
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# 自动收集 src/ 目录下所有的 .cpp 源文件
file(GLOB_RECURSE ELPHIN_SOURCES "src/*.cpp")

# 1. 构建核心静态库 (elphin_lib)
add_library(elphin_lib STATIC ${ELPHIN_SOURCES})

# 移除 main.cpp，避免静态库与可执行文件重定义
list(FILTER ELPHIN_SOURCES EXCLUDE REGEX "src/main.cpp")
set_target_properties(elphin_lib PROPERTIES SOURCES "${ELPHIN_SOURCES}")

# 设置头文件搜索路径：把 src/ 目录加入 include 搜索路径
target_include_directories(elphin_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 链接必要的系统库
find_package(Threads REQUIRED)
target_link_libraries(elphin_lib PRIVATE Threads::Threads)

# 2. 构建可执行文件 (elphin_server 或 elphind)
add_executable(elphin_server src/main.cpp)

# 链接核心静态库与包含路径
target_link_libraries(elphin_server PRIVATE elphin_lib)
target_include_directories(elphin_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)