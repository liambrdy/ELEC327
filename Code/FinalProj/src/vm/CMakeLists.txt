add_executable(vm main.c vm.c)

target_include_directories(vm PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/compiler
)

target_link_libraries(vm PRIVATE common)
