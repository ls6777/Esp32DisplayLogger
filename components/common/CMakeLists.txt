# Shared by every task: the OS/target port layer (TargetPort), the queue message base class and small helpers
idf_component_register(SRCS "Src/TargetPort.cpp"
                       INCLUDE_DIRS "Inc"
                       REQUIRES freertos log
                       PRIV_REQUIRES esp_system)
