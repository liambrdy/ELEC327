add_executable(bmp2c bmp2c.c)
target_include_directories(bmp2c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# stb_image uses math functions on some platforms
target_link_libraries(bmp2c PRIVATE m)