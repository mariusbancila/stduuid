cmake_minimum_required(VERSION 3.16)

set(UUID_MAIN_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(UUID_MAIN_PROJECT ON)
endif()

project(stduuid CXX)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

option(UUID_BUILD_TESTS "Build the unit tests" ${UUID_MAIN_PROJECT})
option(UUID_BUILD_BENCH "Build the benchmarks" ${UUID_MAIN_PROJECT})
option(UUID_SYSTEM_GENERATOR "Enable operating system uuid generator" OFF)
option(UUID_TIME_GENERATOR "Enable experimental time-based uuid generator" OFF)
option(UUID_USING_CXX20_SPAN "Using span from std instead of gsl" OFF)
option(UUID_ENABLE_INSTALL "Create an install target" ${UUID_MAIN_PROJECT})

# The C++ standard the tests are compiled with. The library itself picks
# std::span over gsl::span on its own whenever the standard library provides it.
if(NOT DEFINED CXX_STD_VER)
  if(UUID_USING_CXX20_SPAN)
    set(CXX_STD_VER cxx_std_20)
  else()
    set(CXX_STD_VER cxx_std_17)
  endif()
endif()

# Library target
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# Using system uuid generator
if (UUID_SYSTEM_GENERATOR)
    target_compile_definitions(${PROJECT_NAME} INTERFACE UUID_SYSTEM_GENERATOR)

    if (WIN32)
    elseif (APPLE)
        find_library(CFLIB CoreFoundation REQUIRED)
        target_link_libraries(${PROJECT_NAME} INTERFACE ${CFLIB})
    else ()
        find_package(Libuuid REQUIRED)
        if (Libuuid_FOUND)
            target_include_directories(${PROJECT_NAME} INTERFACE ${Libuuid_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} INTERFACE ${Libuuid_LIBRARIES})
        endif ()
    endif ()
endif ()

# Using time-based generator
if (UUID_TIME_GENERATOR)
    target_compile_definitions(${PROJECT_NAME} INTERFACE UUID_TIME_GENERATOR)
endif()

# Using span from std
if (NOT UUID_USING_CXX20_SPAN AND NOT TARGET GSL)
    target_include_directories(${PROJECT_NAME} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>)
    install(DIRECTORY gsl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()

if(UUID_ENABLE_INSTALL)
    # Install step and imported target
    install(FILES include/uuid.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-targets)
    install(EXPORT ${PROJECT_NAME}-targets
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

    # Config files for find_package()
    include(CMakePackageConfigHelpers)
    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
            VERSION "1.2.3"
            COMPATIBILITY AnyNewerVersion)
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLibuuid.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
    export(EXPORT ${PROJECT_NAME}-targets
            FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}-targets.cmake")
endif()

# Tests
if (UUID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif ()

# Benchmarks
if (UUID_BUILD_BENCH)
    add_subdirectory(bench)
endif ()
