cmake_minimum_required(VERSION 3.16)
project(cairn CXX)	# 项目名 cairn，语言c++

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 生成 build/compile_commands.json，让 IDE 知道真实编译参数

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)	# 编译器不支持 C++20 就报错！不允许降级

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)	# 默认 Release（自带优化，FORTIFY 才生效）
endif()

add_compile_options(-Wall -Wextra -Werror=return-type)	# 警告全开
add_compile_definitions(_FORTIFY_SOURCE=2)

add_executable(cairn src/main.cpp src/net/Buffer.cpp src/net/Connection.cpp src/proto/RespParser.cpp src/store/MemStore.cpp src/proto/RespWriter.cpp src/cmd/commands.cpp src/net/Channel.cpp src/net/EpollPoller.cpp src/net/EventLoop.cpp src/net/Acceptor.cpp src/net/FdGuard.cpp src/store/RocksDBStore.cpp src/net/TimerQueue.cpp src/store/ExpireDict.cpp) 
target_include_directories(cairn PRIVATE src)
target_link_libraries(cairn PRIVATE rocksdb)
# ---- 单测 ----
add_executable(buffer_test tests/buffer_test.cpp src/net/Buffer.cpp)
target_include_directories(buffer_test PRIVATE src)   # 让 #include "net/Buffer.h" 能找到
enable_testing()
add_test(NAME buffer_test COMMAND buffer_test)

add_executable(parse_test tests/parse_test.cpp src/proto/RespParser.cpp)
target_include_directories(parse_test PRIVATE src)
add_test(NAME parse_test COMMAND parse_test)

add_executable(expire_test tests/expire_dict.cpp src/store/ExpireDict.cpp)
target_include_directories(expire_test PRIVATE src)   # 让 #include "net/Buffer.h" 能找到
add_test(NAME expire_test COMMAND expire_test)
