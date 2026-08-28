cmake_minimum_required(VERSION 3.16)
project(libzen LANGUAGES CXX)
 

# --- Vendor libraries (conditional) ---
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ZEN_VENDOR_LIBS "")
set(ZEN_VENDOR_INCLUDES "")
set(ZEN_DEFINITIONS "")
 
 
# Route print() and runtime errors through a host-supplied writer instead of
# stdout — see include/zen/zen_host_output.h. Off by default: no indirection,
# no behaviour change for the CLI.
option(ZEN_HOST_OUTPUT "Route script output through a host writer hook" OFF)
if(ZEN_HOST_OUTPUT)
    list(APPEND ZEN_DEFINITIONS ZEN_HOST_OUTPUT)
endif()

# Core library sources
file(GLOB CORE_SRCS CONFIGURE_DEPENDS src/*.cpp)

# --- Optional modules ---
# Each of these is one self-contained builtin_<name>.cpp exporting one
# zen_lib_<name>. Nothing in the VM refers to them — imports resolve through the
# runtime registry, so a module that was never compiled is simply one the host
# cannot register. Turning one off is how a host trims what it does not ship,
# without maintaining its own copy of this source list:
#
#   cmake -DZEN_MODULE_NUMPY=OFF ...
#
# `base` is not here: it is opened as globals, not imported.
set(ZEN_OPTIONAL_MODULES math time struct numpy io os path json net http)
foreach(mod IN LISTS ZEN_OPTIONAL_MODULES)
    string(TOUPPER ${mod} MOD)
    option(ZEN_MODULE_${MOD} "Build the ${mod} module" ON)
    if(NOT ZEN_MODULE_${MOD})
        list(REMOVE_ITEM CORE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin_${mod}.cpp)
        list(APPEND ZEN_DEFINITIONS ZEN_NO_MODULE_${MOD})
    endif()
endforeach()

# --- libzen (static library) ---
add_library(zen_static STATIC ${CORE_SRCS})
target_include_directories(zen_static PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_link_libraries(zen_static PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(zen_static PUBLIC ws2_32)
endif()
if(ANDROID)
    # zenconf.h routes output through __android_log_write
    target_link_libraries(zen_static PUBLIC log)
endif()
target_compile_definitions(zen_static PUBLIC ${ZEN_DEFINITIONS})
if(NOT MSVC)
    target_compile_options(zen_static PRIVATE $<$<CONFIG:Release>:-O3>)
endif()
target_compile_definitions(zen_static PRIVATE $<$<CONFIG:Release>:NDEBUG>)
set_target_properties(zen_static PROPERTIES OUTPUT_NAME zen POSITION_INDEPENDENT_CODE ON)

# --- libzen (shared library) ---
# add_library(zen_shared SHARED ${CORE_SRCS})
# target_include_directories(zen_shared PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
# target_link_libraries(zen_shared PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
# target_compile_definitions(zen_shared PUBLIC ${ZEN_DEFINITIONS})
# target_compile_options(zen_shared PRIVATE $<$<CONFIG:Release>:-O3>)
# target_compile_definitions(zen_shared PRIVATE $<$<CONFIG:Release>:NDEBUG>)
# set_target_properties(zen_shared PROPERTIES OUTPUT_NAME zen)
