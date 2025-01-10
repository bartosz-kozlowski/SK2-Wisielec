cmake_minimum_required(VERSION 3.10)

add_executable(server
    Wisielec.cpp
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
target_compile_options(server PRIVATE -Wall -Wextra)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/server/topics)

file(COPY ${CMAKE_SOURCE_DIR}/server/topics/
     DESTINATION ${CMAKE_BINARY_DIR}/server/topics)
