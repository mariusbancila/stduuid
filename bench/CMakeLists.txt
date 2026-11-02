# Benchmark target
#
# Compares the SIMD accelerated header with a verbatim copy of the original
# scalar implementation (reference/uuid_original.h).  Both are compiled into
# the same translation unit, so the comparison is not affected by different
# compiler flags or by the two runs landing on different CPU frequencies.
add_executable(bench_${PROJECT_NAME} bench.cpp)

target_link_libraries(bench_${PROJECT_NAME} PRIVATE ${PROJECT_NAME})
target_include_directories(bench_${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/reference)
target_compile_features(bench_${PROJECT_NAME} PRIVATE ${CXX_STD_VER})

if (MSVC)
    target_compile_options(bench_${PROJECT_NAME} PRIVATE /O2)
    # /arch:AVX2 enables the SSSE3 code path on MSVC.  It is meaningless on
    # ARM, where the NEON path is selected by the compiler instead.
    if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|arm")
        target_compile_options(bench_${PROJECT_NAME} PRIVATE /arch:AVX2)
    endif ()
    target_compile_definitions(bench_${PROJECT_NAME} PRIVATE NDEBUG)
else ()
    target_compile_options(bench_${PROJECT_NAME} PRIVATE -O3 -Wall -Wextra)
    target_compile_definitions(bench_${PROJECT_NAME} PRIVATE NDEBUG)
endif ()

# The benchmark deliberately does not use -march=native: the library selects
# SSE2/SSSE3/SHA at run time, and that is the configuration most users get.
option(UUID_BENCH_NATIVE "Compile the benchmark with -march=native" OFF)
if (UUID_BENCH_NATIVE AND NOT MSVC)
    target_compile_options(bench_${PROJECT_NAME} PRIVATE -march=native)
endif ()
