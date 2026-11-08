# PrintfTask: buffers printf output and sends it to the real stdout from a low priority task
idf_component_register(SRCS "Src/PrintfTask.cpp"
                            "Src/PrintUtils.cpp"
                            "Src/StdOutPort.cpp"
                       INCLUDE_DIRS "Inc"
                       REQUIRES common)
