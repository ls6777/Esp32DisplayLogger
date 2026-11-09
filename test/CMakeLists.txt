# Host (PC) unit tests for the target independent parts of the printf_task, task_monitor and display components.
#
# Uses the Unity test framework that ships with ESP-IDF, so IDF_PATH must be set (or pass -DUNITY_DIR=<path
# to Unity's src directory>). Requires a host C and C++ compiler (e.g. gcc/g++ from MinGW).
#
# Build and run:
#   cmake -S test -B test/build -G Ninja
#   cmake --build test/build
#   ctest --test-dir test/build --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(Esp32DisplayLoggerTests C CXX)

# Unity is C, the project code and tests are C++
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Lets VS Code (C/C++ extension) resolve includes in the test files
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(UNITY_DIR "$ENV{IDF_PATH}/components/unity/unity/src" CACHE PATH "Path to the Unity test framework src directory")
if(NOT EXISTS "${UNITY_DIR}/unity.c")
    message(FATAL_ERROR "Unity not found at '${UNITY_DIR}'. Set IDF_PATH or pass -DUNITY_DIR=<path to Unity src>.")
endif()

add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

set(PRINTF_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/printf_task)
set(MONITOR_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/task_monitor)
set(DISPLAY_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/display)
set(COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/common)
set(MAIN_DIR ${CMAKE_CURRENT_LIST_DIR}/../main)

# Host implementation of the OS port layer (TargetPort) and test helpers. Code that uses TargetPort must list this
# directory before ${COMMON_DIR}/Inc, so the fake TargetPort.hpp and Utils.hpp are found instead of the real ones.
set(FAKES_DIR ${CMAKE_CURRENT_LIST_DIR}/fakes)

# PrintBuffer is header only and has no target dependencies
add_executable(test_PrintBuffer
    test_PrintBuffer.cpp
)
target_include_directories(test_PrintBuffer PRIVATE ${PRINTF_DIR}/Inc)

# PrintUtils (and the PrintBuffer it owns) with a fake StdOutPort (defined in the test), so no target code is needed
add_executable(test_PrintUtils
    test_PrintUtils.cpp
    ${PRINTF_DIR}/Src/PrintUtils.cpp
)
target_include_directories(test_PrintUtils PRIVATE ${PRINTF_DIR}/Inc)

# CheckinTracker is header only and has no target dependencies
add_executable(test_CheckinTracker
    test_CheckinTracker.cpp
)
target_include_directories(test_CheckinTracker PRIVATE ${MONITOR_DIR}/Inc)

# TextRenderer and the font have no target dependencies
add_executable(test_TextRenderer
    test_TextRenderer.cpp
    ${DISPLAY_DIR}/Src/TextRenderer.cpp
    ${DISPLAY_DIR}/Src/Font5x7.cpp
)
target_include_directories(test_TextRenderer PRIVATE ${DISPLAY_DIR}/Inc)

# Display with a fake DisplayPort (defined in the test)
add_executable(test_Display
    test_Display.cpp
    ${DISPLAY_DIR}/Src/Display.cpp
    ${DISPLAY_DIR}/Src/TextRenderer.cpp
    ${DISPLAY_DIR}/Src/Font5x7.cpp
)
target_include_directories(test_Display PRIVATE ${DISPLAY_DIR}/Inc)

# The printf task with the real PrintUtils, a fake StdOutPort (defined in the test) and the fake OS
add_executable(test_PrintfTask
    test_PrintfTask.cpp
    ${PRINTF_DIR}/Src/PrintfTask.cpp
    ${PRINTF_DIR}/Src/PrintUtils.cpp
    ${FAKES_DIR}/TargetPort.cpp
)
target_include_directories(test_PrintfTask PRIVATE ${FAKES_DIR} ${PRINTF_DIR}/Inc ${COMMON_DIR}/Inc)

# The task monitor task with the fake OS
add_executable(test_TaskMonitor
    test_TaskMonitor.cpp
    ${MONITOR_DIR}/Src/TaskMonitor.cpp
    ${FAKES_DIR}/TargetPort.cpp
)
target_include_directories(test_TaskMonitor PRIVATE ${FAKES_DIR} ${MONITOR_DIR}/Inc ${COMMON_DIR}/Inc)

# HelloTask with the real TaskMonitor, PrintUtils and Display. Only the port layers are fakes: the OS, and
# StdOutPort and DisplayPort (defined in the test).
add_executable(test_HelloTask
    test_HelloTask.cpp
    ${MAIN_DIR}/Src/HelloTask.cpp
    ${MONITOR_DIR}/Src/TaskMonitor.cpp
    ${PRINTF_DIR}/Src/PrintUtils.cpp
    ${DISPLAY_DIR}/Src/Display.cpp
    ${DISPLAY_DIR}/Src/TextRenderer.cpp
    ${DISPLAY_DIR}/Src/Font5x7.cpp
    ${FAKES_DIR}/TargetPort.cpp
)
target_include_directories(test_HelloTask PRIVATE
    ${FAKES_DIR} ${MAIN_DIR}/Inc ${COMMON_DIR}/Inc ${PRINTF_DIR}/Inc ${MONITOR_DIR}/Inc ${DISPLAY_DIR}/Inc)

enable_testing()
foreach(name PrintBuffer PrintUtils CheckinTracker TextRenderer Display PrintfTask TaskMonitor HelloTask)
    target_link_libraries(test_${name} PRIVATE unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
