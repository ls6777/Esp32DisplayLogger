idf_component_register(SRCS "main.cpp"
                            "Src/HelloTask.cpp"
                       INCLUDE_DIRS "Inc"
                       PRIV_REQUIRES common display printf_task task_monitor)
