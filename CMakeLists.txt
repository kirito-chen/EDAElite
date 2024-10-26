cmake_minimum_required(VERSION 3.1) 
project(eda)          

SET(CMAKE_BUILD_TYPE "debug")    
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(app)

add_executable(eda240819 main.cpp) 

target_include_directories(eda240819 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app/include) 

target_link_libraries(eda240819 PUBLIC engine) 





