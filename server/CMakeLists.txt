cmake_minimum_required(VERSION 3.8)
project(server)

set(CMAKE_CXX_STANDARD 20)

add_executable(server main.cpp)

target_include_directories(server PUBLIC ${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/include)

target_link_libraries(server PRIVATE
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Debug/GameNetworkingSockets.lib>
    $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Release/GameNetworkingSockets.lib>
)

add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Debug/GameNetworkingSockets.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Release/GameNetworkingSockets.dll>
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Debug/libprotobufd.dll>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/third_party/GameNetworkingSockets/bin/Windows/Release/libprotobufd.dll>
        $<TARGET_FILE_DIR:server>
)
