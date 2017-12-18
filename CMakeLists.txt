cmake_minimum_required(VERSION 3.7.0)
project(test_uuid)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB headers ${CMAKE_SOURCE_DIR}/include/*.h)
file(GLOB SOURCES "test/*.cpp" "include/*.cpp")

add_executable(test_uuid ${SOURCES} ${headers})

if(WIN32)
	message(status "Setting MSVC flags")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc /std:c++latest")
elseif(APPLE)
	find_library(CFLIB CoreFoundation)
	target_link_libraries(test_uuid ${CFLIB})
	
	message(status "Setting Clang flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -fexceptions -g -Wall")
else()
	find_package(Libuuid REQUIRED)
	if (NOT LIBUUID_FOUND)
		message(FATAL_ERROR
			"You might need to run 'sudo apt-get install uuid-dev' or similar")
	endif()
	include_directories(${LIBUUID_INCLUDE_DIR})
	target_link_libraries(test_uuid ${LIBUUID_LIBRARY})
	
    message(status "Setting GCC flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -fexceptions -g -Wall")
endif()

message(status "** CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
