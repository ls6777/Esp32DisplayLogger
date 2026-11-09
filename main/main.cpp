/// @file main.cpp
/// @brief Hello world application: creates the PrintfTask, the TaskMonitor, the HelloTask and the DisplayTask.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Display.hpp"
#include "DisplayTask.hpp"
#include "HelloTask.hpp"
#include "PrintfTask.hpp"
#include "PrintUtils.hpp"
#include "TaskMonitor.hpp"

extern "C" void app_main(void); ///< ESP-IDF application entry point (C linkage)

constexpr uint32_t TASK_MONITOR_STACK_SIZE = 2048;                      ///< TaskMonitor stack size, in bytes
constexpr UBaseType_t TASK_MONITOR_PRIORITY = configMAX_PRIORITIES - 1; ///< TaskMonitor priority (highest)

constexpr uint32_t PRINTF_TASK_STACK_SIZE = 2048;                       ///< PrintfTask stack size, in bytes
constexpr UBaseType_t PRINTF_TASK_PRIORITY = 1;                         ///< PrintfTask priority (lowest)

constexpr uint32_t HELLO_TASK_STACK_SIZE = 2048;                        ///< HelloTask stack size, in bytes
constexpr UBaseType_t HELLO_TASK_PRIORITY = 5;                          ///< HelloTask priority

constexpr uint32_t DISPLAY_TASK_STACK_SIZE = 3072;                      ///< DisplayTask stack size, in bytes (snprintf)
constexpr UBaseType_t DISPLAY_TASK_PRIORITY = 3;                        ///< DisplayTask priority

//------------------------------------------------------------------
// StartTaskMonitor
//------------------------------------------------------------------
/// @brief Entry point for the TaskMonitor task. Never returns.
/// @param[in] argument - task argument (unused)
static void StartTaskMonitor(void* argument)
{
    TaskMonitor taskMon;
    taskMon.Run();
}

//------------------------------------------------------------------
// StartPrintfTask
//------------------------------------------------------------------
/// @brief Entry point for the PrintfTask task. Never returns.
/// @param[in] argument - task argument (unused)
static void StartPrintfTask(void* argument)
{
    PrintfTask task;
    task.Run();
}

//------------------------------------------------------------------
// StartHelloTask
//------------------------------------------------------------------
/// @brief Entry point for the HelloTask task. Never returns.
/// @param[in] argument - task argument (unused)
static void StartHelloTask(void* argument)
{
    HelloTask task;
    task.Initialize();
    task.Run();
}

//------------------------------------------------------------------
// StartDisplayTask
//------------------------------------------------------------------
/// @brief Entry point for the DisplayTask task. Never returns.
/// @param[in] argument - task argument (unused)
static void StartDisplayTask(void* argument)
{
    DisplayTask task;
    task.Initialize();
    task.Run();
}

//------------------------------------------------------------------
// Main Task
//------------------------------------------------------------------
/// @brief Application entry point: sets up stdout buffering, the display and the task monitor, then creates the tasks
/// @details Returns once the tasks are created. ESP-IDF then deletes the main task.
void app_main(void)
{
    // Route printf into the printf buffer before any task prints, so all task output is buffered
    PrintfTask::Initialize();

    // Show everything sent to the UART on the LCD terminal too (set before the PrintfTask starts)
    PrintUtils::SetMirror(&DisplayTask::MirrorOutput);

    // Set up the LCD before any task draws on it (blocks for a few hundred ms during the panel reset)
    Display::Initialize();

    // Create the task monitor's queue before any task can register with it
    TaskMonitor::Initialize();

    // Task monitor
    xTaskCreate(&StartTaskMonitor, "TaskMonitor", TASK_MONITOR_STACK_SIZE, nullptr, TASK_MONITOR_PRIORITY, nullptr);

    // Printf task
    xTaskCreate(&StartPrintfTask, "PrintfTask", PRINTF_TASK_STACK_SIZE, nullptr, PRINTF_TASK_PRIORITY, nullptr);

    // Hello task
    xTaskCreate(&StartHelloTask, "HelloTask", HELLO_TASK_STACK_SIZE, nullptr, HELLO_TASK_PRIORITY, nullptr);

    // Display task
    xTaskCreate(&StartDisplayTask, "DisplayTask", DISPLAY_TASK_STACK_SIZE, nullptr, DISPLAY_TASK_PRIORITY, nullptr);
}
