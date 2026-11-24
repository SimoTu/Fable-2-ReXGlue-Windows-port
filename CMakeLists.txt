# pablo - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(pablo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(PABLO_SOURCES
    src/main.cpp
)

if(WIN32)
    enable_language(RC)
    add_executable(pablo WIN32 ${PABLO_SOURCES} src/pablo.rc)
    target_link_libraries(pablo PRIVATE comdlg32)
else()
    add_executable(pablo ${PABLO_SOURCES})
endif()

rexglue_setup_target(pablo GPU_PLUGINS xenos)

# Keep the original boot baseline available for comparison.
set_target_properties(pablo PROPERTIES OUTPUT_NAME pablo-native)
