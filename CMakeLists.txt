cmake_minimum_required(VERSION 3.7.0)
project(stduuid CXX)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

option(UUID_BUILD_TESTS "Build the unit tests" ON)
option(UUID_SYSTEM_GENERATOR "Enable operating system uuid generator" ON)

# Library target
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_SOURCE_DIR}/include)

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

# Tests
if (UUID_BUILD_TESTS)
    add_subdirectory(test)
endif ()