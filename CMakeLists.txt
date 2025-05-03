cmake_minimum_required(VERSION 3.15)
project(QuickOK-BlitzDigits VERSION 0.0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BINARY_DIR ${CMAKE_BINARY_DIR}/dummy)

if(NOT DEFINED QOZERO_SOURCE_ROOT)
    if(EXISTS ${CMAKE_SOURCE_DIR}/../QuickOK-Zero)
        set(QOZERO_SOURCE_ROOT ${CMAKE_SOURCE_DIR}/../QuickOK-Zero)
        message(STATUS "QOZERO_SOURCE_ROOT not set but QuickOK-Zero found in the parent directory")
    else()
        message(FATAL_ERROR "QOZERO_SOURCE_ROOT not set and QuickOK-Zero not found in the parent directory")
    endif()
endif()

file(WRITE ${CMAKE_BINARY_DIR}/dummy.c "int main() { return 0; }")
add_executable(dummy ${CMAKE_BINARY_DIR}/dummy.c)
message(STATUS ${CMAKE_BINARY_DIR})

if(WIN32)
    set(DUMMY_PROGRAM_EXT ".exe")
else()
    set(DUMMY_PROGRAM_EXT "")
endif()

target_include_directories(dummy PRIVATE ${QOZERO_SOURCE_ROOT}/include)
add_custom_command(
    TARGET dummy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/dummy
    COMMENT "Cleaning up dummy build directory"
)
