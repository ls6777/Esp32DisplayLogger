# Esp32DisplayLogger
ESP32 application built on [PrintfTask](https://github.com/ls6777/PrintfTask) and [TaskMonitor](https://github.com/ls6777/TaskMonitor)

## Overview
This project combines two reusable embedded components in one ESP-IDF application:

- **PrintfTask** buffers all `printf` output in RAM and sends it to the UART from a low priority task, so `printf` returns quickly and doesn't block time critical tasks.
- **TaskMonitor** makes sure every registered task keeps checking in. If a task stops responding, the monitor logs the task's name and stops kicking the watchdog, so the device resets.

A third component, **display**, draws text on the ESP-WROVER-KIT's 320x240 LCD.

The current application is a hello world: a single `HelloTask` shows "Hello World!" on the LCD once at start up, then prints a message once a second through the printf buffer and checks in with the task monitor when asked.

The example application runs on an ESP32 (ESP-WROVER-KIT) with ESP-IDF and FreeRTOS. All components are RTOS and target agnostic: all OS and target calls go through small port layers.

## Project layout
```
CMakeLists.txt                ESP-IDF project
sdkconfig.defaults            Settings the project relies on (tick rate, task watchdog, run-time stats)
components/
  common/                     Shared by all tasks
    Inc/TargetPort.hpp        Port layer: queues, task IDs, time, watchdog, task statistics
    Src/TargetPort.cpp        Port layer implementation for FreeRTOS on ESP-IDF
    Inc/Message.hpp           Message posted to a task's queue
    Inc/Utils.hpp             Small shared helpers
  printf_task/                From PrintfTask
    Inc/PrintfTask.hpp        The printf task: sends buffered output to the real stdout
    Inc/PrintUtils.hpp        Generic stdout API that owns the printf buffer
    Inc/PrintBuffer.hpp       Generic circular buffer (no RTOS/target code)
    Inc/StdOutPort.hpp        Port layer: stdout redirect and real stdout write
    Src/StdOutPort.cpp        Port layer implementation for ESP-IDF (Picolibc and Newlib)
  task_monitor/               From TaskMonitor
    Inc/TaskMonitor.hpp       The task monitor task (message handling, watchdog, violation action)
    Inc/CheckinTracker.hpp    Generic monitor logic: checkin timing, requests, violations (no RTOS/target code)
  display/                    Text on the LCD
    Inc/Display.hpp           Generic display API: clear the screen, draw text
    Inc/TextRenderer.hpp      Generic text to RGB565 pixel rendering, and colors (no RTOS/target code)
    Inc/Font5x7.hpp           Generic 5x7 bitmap font for printable ASCII
    Inc/DisplayPort.hpp       Port layer: panel init, backlight, fill and draw pixel rectangles
    Src/DisplayPort.cpp       Port layer implementation for the ESP-WROVER-KIT LCD (ILI9341 or ST7789V over SPI)
main/
  main.cpp                    Sets up stdout buffering, the display and the task monitor, then creates the tasks
  Inc|Src/HelloTask.*         Hello world: LCD text once, prints every second, 5 s task monitor timeout
test/                         Host unit tests (Unity)
  fakes/                      Host OS port layer (TargetPort) and test helpers
```

## Tasks
| Task | Priority | Purpose |
|---|---|---|
| TaskMonitor | Highest | Checks every registered task and kicks the watchdog |
| HelloTask | 5 | Shows "Hello World!" on the LCD once. Prints "Hello world!" every second, and the printf buffer high water mark every 10 seconds |
| PrintfTask | 1 (lowest) | Sends buffered printf output to the UART every 10 ms |

`app_main()` redirects stdout into the printf buffer before creating any task, so all task output is buffered. It also initializes the display and creates the task monitor's queue first, so tasks can draw and register however they get scheduled.

## Display
The ESP-WROVER-KIT has a 320x240 SPI LCD. Depending on the kit version its controller is an ILI9341 or an ST7789V. `DisplayPort` reads the controller ID at start up and uses the matching initialization, so both work without configuration. The serial output shows which one was found:

```
LCD ID: 0xa9c242, ST7789V detected
```

Draw text with `Display::DrawString()`:

```cpp
Display::DrawString(x, y, "Hello World!", Color::WHITE, Color::BLACK, 4);
```

The last argument scales the 6x8 pixel character cell (1 to `Display::MAX_SCALE`). `Display` is not thread safe: call it from one task, or protect it with a mutex.

## Adding a task
Follow `HelloTask`:

1. Create a message queue in `Initialize()` and post `Message::INITIALIZE` to it.
2. On `INITIALIZE`, register with the task monitor: `TaskMonitor::Register(CHECKIN_TIMEOUT, &TaskCheckin);`
3. `TaskCheckin()` posts `Message::TASK_CHECKIN` to the task's own queue. On that message, call `TaskMonitor::TaskCheckin()`.
4. Use `printf` as usual. The output goes through the printf buffer.

Use `PrintUtils::GetHighWaterMark()` to size the printf buffer (`PrintUtils::BUFFER_SIZE`, 15000 bytes by default).

See the [PrintfTask](https://github.com/ls6777/PrintfTask) and [TaskMonitor](https://github.com/ls6777/TaskMonitor) READMEs for details on each component, including timing measurements and how to port them.

## Building
Tested with ESP-IDF v6.1. From an ESP-IDF terminal:

```
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Expected output (and "Hello World!" in the middle of the LCD):
```
LCD ID: 0xa9c242, ST7789V detected
Task Monitor Initialized
HelloTask Initialized
Hello world! 1
Hello world! 2
...
Hello world! 10
Printf buffer high water mark: 67 of 14999 bytes
```

`sdkconfig.defaults` holds the settings the project relies on, so they survive `idf.py set-target` and deleting `sdkconfig`:

- `CONFIG_FREERTOS_HZ=1000`: 1 ms tick, for 10 ms task monitor evaluation and clean wrap-around of `GetTimeMs()`
- `CONFIG_ESP_TASK_WDT_PANIC=y`: the task watchdog resets the device. Without it, a task monitor violation only prints a warning.
- `CONFIG_FREERTOS_USE_TRACE_FACILITY`, `..._USE_STATS_FORMATTING_FUNCTIONS`, `..._GENERATE_RUN_TIME_STATS`: task statistics (`TargetPort::GetTaskList()`, `TargetPort::GetRunTimeStats()`)

The project includes a shared VS Code setup (`.vscode/c_cpp_properties.json` and `extensions.json`) for the ESP-IDF and C/C++ extensions. Code navigation uses `build/compile_commands.json`, so build once first.

## Unit tests
All the target independent code has host unit tests in `test/`, using the Unity framework that ships with ESP-IDF. They need `IDF_PATH` set and a host gcc, CMake and Ninja (e.g. MinGW from WinLibs on Windows):

```
cmake -S test -B test/build -G Ninja
cmake --build test/build
ctest --test-dir test/build --output-on-failure
```

| Test | Code under test | Fakes |
|---|---|---|
| `test_PrintBuffer` | `PrintBuffer` | none |
| `test_PrintUtils` | `PrintUtils` | `StdOutPort` |
| `test_PrintfTask` | `PrintfTask`, with the real `PrintUtils` | `StdOutPort`, OS |
| `test_CheckinTracker` | `CheckinTracker` | none |
| `test_TaskMonitor` | `TaskMonitor` task: requests, violations, watchdog, statistics | OS |
| `test_TextRenderer` | `TextRenderer`, `Font5x7` (including glyph shapes as ASCII art) | none |
| `test_Display` | `Display`: positions, scaling, clipping | `DisplayPort` |
| `test_HelloTask` | `HelloTask`, with the real `TaskMonitor`, `PrintUtils` and `Display` | `StdOutPort`, `DisplayPort`, OS |

The tasks run their real `Run()` loops on a host implementation of the OS port layer ([test/fakes/TargetPort.hpp](test/fakes/TargetPort.hpp)): in-memory queues and a clock that advances when a task waits. Since `Run()` never returns, the fake ends it by throwing when a call budget runs out or when the task would block forever.

The port layer implementations (`TargetPort.cpp`, `StdOutPort.cpp`, `DisplayPort.cpp`) and `main.cpp` call ESP-IDF and FreeRTOS directly, so they are verified on the board instead.

## Targets
The application runs on an ESP32 (ESP-WROVER-KIT) with ESP-IDF and FreeRTOS. Porting to another target or RTOS only means changing the port layers: `TargetPort.hpp/.cpp`, `StdOutPort.cpp` and `DisplayPort.cpp`.
