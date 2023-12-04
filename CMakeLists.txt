# CMakeList.txt: mtr_malloc 的 CMake 项目，在此处包括源代码并定义
cmake_minimum_required (VERSION 3.8)

project ("mtr_malloc")

# 查找目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)

add_subdirectory("internal")


# 将源代码添加到此项目的可执行文件。
add_executable(mtr_malloc ${DIR_SRCS})
# TODO: 如有需要，请添加测试并安装目标。

#target_link_libraries(mtr_malloc COMMON)