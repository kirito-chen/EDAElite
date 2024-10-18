add_library(engine STATIC "")

set_target_properties(engine PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(engine PUBLIC ./include/)

file(GLOB srcs ./src/*.cpp)

target_sources(engine PRIVATE ${srcs})