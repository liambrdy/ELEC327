add_library(common STATIC darray.c)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})