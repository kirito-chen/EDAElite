cmake_minimum_required(VERSION 3.1) 
project(eda)          

SET(CMAKE_BUILD_TYPE "debug")    
set(CMAKE_CXX_STANDARD 11)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")  # 或者 -O3
endif()


add_subdirectory(app)

add_executable(eda240819 main.cpp) 

target_include_directories(eda240819 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app/include) 

target_link_libraries(eda240819 PUBLIC engine)

find_package(Threads REQUIRED)

target_link_libraries(eda240819 PRIVATE ${CMAKE_THREAD_LIBS_INIT})





