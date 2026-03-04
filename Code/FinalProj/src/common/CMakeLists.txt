add_library(common STATIC darray.c hashtable.c arena.c file.c str.c)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(common PUBLIC m)