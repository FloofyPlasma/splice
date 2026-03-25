cmake_minimum_required(VERSION 3.30)

project(
        splice
        VERSION 0.2.0
        DESCRIPTION "A header-only C++26 reflection-based hook and mixin library"
        HOMEPAGE_URL "https://github.com/FloofyPlasma/splice"
        LANGUAGES CXX
)

option(SPLICE_BUILD_TESTS "Build splice tests" OFF)
option(SPLICE_BUILD_EXAMPLES "Build splice examples" OFF)
option(SPLICE_BUILD_MODULE "Build splice as a C++ module" OFF)

if (SPLICE_BUILD_MODULE)
        add_library(splice)
        add_library(splice::splice ALIAS splice)

        target_sources(splice
                PUBLIC
                FILE_SET CXX_MODULES FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/include/splice/splice.cppm
        )
else()
        add_library(splice INTERFACE)
        add_library(splice::splice ALIAS splice)
endif()

target_compile_features(splice INTERFACE cxx_std_26)

target_compile_options(splice INTERFACE -freflection)

target_include_directories(splice INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if (SPLICE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (SPLICE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
        TARGETS splice
        EXPORT spliceTargets
)

install(
        DIRECTORY include/splice
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
        EXPORT spliceTargets
        FILE spliceTargets.cmake
        NAMESPACE splice::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/splice
)

write_basic_package_version_file(
        spliceConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spliceConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/spliceConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/splice
)

install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/spliceConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/spliceConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/splice
)


find_package(nlohmann_json REQUIRED)
target_link_libraries(splice INTERFACE nlohmann_json::nlohmann_json)