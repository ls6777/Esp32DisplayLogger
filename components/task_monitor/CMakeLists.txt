# TaskMonitor: makes sure every registered task keeps checking in, and owns the watchdog
idf_component_register(SRCS "Src/TaskMonitor.cpp"
                       INCLUDE_DIRS "Inc"
                       REQUIRES common)
