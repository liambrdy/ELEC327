add_library(common STATIC darray.c hashtable.c arena.c)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})