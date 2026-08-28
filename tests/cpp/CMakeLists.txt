cmake_minimum_required(VERSION 3.16)
project(zen_tests LANGUAGES CXX)

# --- Embedding test ---
add_executable(test_embedding test_embedding.cpp)
set_target_properties(test_embedding PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})

# Multi-config generators (Visual Studio) append the config name to the output
# directory — pin each config so the binary always lands in ZEN_BIN_DIR.
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(test_embedding PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} ${ZEN_BIN_DIR})
endforeach()
target_link_libraries(test_embedding zen_static)

if(EMSCRIPTEN)
    set_target_properties(test_embedding PROPERTIES SUFFIX ".js")
    target_link_options(test_embedding PRIVATE
        -sNODERAWFS=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXIT_RUNTIME=1
        -sSTACK_SIZE=8388608)
endif()

# --- Host output hook (only meaningful with the option on) ---
if(ZEN_HOST_OUTPUT)
    add_executable(test_host_output test_host_output.cpp)
    set_target_properties(test_host_output PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(test_host_output PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} ${ZEN_BIN_DIR})
    endforeach()
    target_link_libraries(test_host_output zen_static)
endif()
