find_package(SDL2 QUIET)

if (NOT SDL2_FOUND)
    message(STATUS "SDL2 not found — skipping simulator target")
    return()
endif()

add_executable(simulator
    main.c
    display.c
    buttons.c
    syscall.c
)

target_include_directories(simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(simulator PRIVATE vmlib SDL2::SDL2)
