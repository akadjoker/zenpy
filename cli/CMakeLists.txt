cmake_minimum_required(VERSION 3.16)
project(zen_cli LANGUAGES CXX)

add_executable(zen main.cpp)
set_target_properties(zen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})

# Multi-config generators (Visual Studio) append the config name to the output
# directory — pin each config so the binary always lands in ZEN_BIN_DIR.
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(zen PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} ${ZEN_BIN_DIR})
endforeach()
if(NOT MSVC)
    target_compile_options(zen PRIVATE $<$<CONFIG:Release>:-O3>)
endif()
target_compile_definitions(zen PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_link_libraries(zen zen_static)
if(NOT WIN32 AND NOT EMSCRIPTEN AND NOT ANDROID)
    target_link_options(zen PRIVATE -rdynamic)
endif()

if(EMSCRIPTEN)
    # A CLI on the web means node: NODERAWFS gives it the real filesystem so
    # `node zen.js script.py` behaves like the native binary. The default 64KB
    # wasm stack overflows in the recursive-descent compiler, which surfaces as
    # an unrelated "divide by zero" trap — hence the explicit STACK_SIZE.
    set_target_properties(zen PROPERTIES SUFFIX ".js")
    target_link_options(zen PRIVATE
        -sNODERAWFS=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXIT_RUNTIME=1
        -sSTACK_SIZE=8388608)
endif()
