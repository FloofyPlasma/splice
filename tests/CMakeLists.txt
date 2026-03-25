cmake_minimum_required(VERSION 3.30)

find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(splice_tests
        test_registry.cpp
        test_priority.cpp
        test_wire.cpp
        test_flux.cpp
)

target_link_libraries(splice_tests
        PRIVATE
        splice::splice
        Catch2::Catch2WithMain
)

target_compile_options(splice_tests PRIVATE -freflection)

catch_discover_tests(splice_tests)