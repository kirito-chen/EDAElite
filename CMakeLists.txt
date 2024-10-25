cmake_minimum_required(VERSION 3.1)
project(eda)

set(CMAKE_BUILD_TYPE "debug")
set(CMAKE_CXX_STANDARD 11)

# Find Threads package for pthread
find_package(Threads REQUIRED)

add_subdirectory(app)

add_executable(checker main.cpp)

target_include_directories(checker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app/include)

# Link with engine and pthread libraries
target_link_libraries(checker PUBLIC engine Threads::Threads)
