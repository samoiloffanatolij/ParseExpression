add_library(parse_exp STATIC parse_exp.cpp)
target_include_directories(parse_exp PUBLIC include)

target_link_libraries(parse_exp PUBLIC trims)
