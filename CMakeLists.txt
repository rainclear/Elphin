cmake_minimum_required(VERSION 3.16)

project(Elphin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# executable will be put to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# header files directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find pthread for std::thread / ThreadPool support
find_package(Threads REQUIRED)

# source code directory
file(GLOB_RECURSE SOURCES "src/*.cpp")

# server executable elphind
add_executable(elphind ${SOURCES})

# Link pthread library
target_link_libraries(elphind PRIVATE Threads::Threads)

# print config information
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")