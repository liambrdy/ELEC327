add_library(vmlib STATIC vm.c disass.c)
target_include_directories(vmlib PUBLIC
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(vmlib PUBLIC common)

add_executable(vm main.c)
target_link_libraries(vm PRIVATE vmlib)
