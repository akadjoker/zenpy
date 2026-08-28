cmake_minimum_required(VERSION 3.10)

# Fuzzer requires clang with libFuzzer support
if(ZEN_BUILD_FUZZ AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_zen fuzz_compile_run.cpp)
    target_link_libraries(fuzz_zen zen_static)
    target_compile_options(fuzz_zen PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(fuzz_zen PRIVATE -fsanitize=fuzzer,address,undefined)
    target_compile_definitions(fuzz_zen PRIVATE ZEN_DEBUG_STRESS_GC)
    set_target_properties(fuzz_zen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})
    message(STATUS "Fuzzer target 'fuzz_zen' enabled")
else()
    if(ZEN_BUILD_FUZZ)
        message(WARNING "Fuzzer requires Clang — skipping fuzz_zen target")
    endif()
endif()
